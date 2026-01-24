/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * thumbnail.cpp - Video Thumbnail Generation
 *
 * Background worker that generates JPEG thumbnails for video files.
 * Extracts a frame at 2 seconds into the video to show actual motion
 * content rather than pre-capture buffer frames.
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "jpegutils.hpp"
#include "thumbnail.hpp"

/* Target timestamp: 2 seconds into video */
static const int64_t THUMB_TARGET_SEC = 2;

/* Thumbnail dimensions */
static const int THUMB_WIDTH = 320;
static const int THUMB_QUALITY = 70;

cls_thumbnail::cls_thumbnail(cls_motapp *p_app)
{
    app = p_app;
    shutdown_flag = false;

    /* Start background worker thread */
    worker_thread = std::thread(&cls_thumbnail::worker_loop, this);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Thumbnail worker started"));
}

cls_thumbnail::~cls_thumbnail()
{
    /* Signal shutdown and wake worker */
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        shutdown_flag = true;
    }
    queue_cv.notify_one();

    /* Wait for worker to finish */
    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Thumbnail worker stopped"));
}

void cls_thumbnail::queue(const std::string &video_path)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    pending.push(video_path);
    queue_cv.notify_one();
}

bool cls_thumbnail::exists(const std::string &video_path)
{
    std::string thumb_path = path_for(video_path);
    struct stat st;
    return (stat(thumb_path.c_str(), &st) == 0);
}

std::string cls_thumbnail::path_for(const std::string &video_path)
{
    return video_path + ".thumb.jpg";
}

void cls_thumbnail::worker_loop()
{
    /* Set thread name for debugging */
    #ifdef __linux__
        pthread_setname_np(pthread_self(), "motion-thumb");
    #endif

    while (true) {
        std::string video_path;

        /* Wait for work or shutdown */
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return shutdown_flag || !pending.empty();
            });

            if (shutdown_flag && pending.empty()) {
                break;
            }

            if (!pending.empty()) {
                video_path = pending.front();
                pending.pop();
            }
        }

        if (!video_path.empty()) {
            /* Generate thumbnail (errors logged internally) */
            generate(video_path);

            /* Small delay to avoid CPU saturation */
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void cls_thumbnail::generate(const std::string &video_path)
{
    AVFrame *frame = nullptr;
    std::string thumb_path = path_for(video_path);
    int retcd;

    /* Check if thumbnail already exists */
    if (exists(video_path)) {
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
            , _("Thumbnail already exists: %s"), thumb_path.c_str());
        return;
    }

    /* Extract frame from video */
    retcd = extract_frame(video_path, &frame);
    if (retcd < 0 || frame == nullptr) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            , _("Failed to extract frame from: %s"), video_path.c_str());
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
        return;
    }

    /* Encode thumbnail */
    retcd = encode_thumbnail(frame, thumb_path);
    av_frame_free(&frame);

    if (retcd < 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            , _("Failed to encode thumbnail: %s"), thumb_path.c_str());
        return;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        , _("Generated thumbnail: %s"), thumb_path.c_str());
}

int cls_thumbnail::extract_frame(const std::string &video_path, AVFrame **frame)
{
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    const AVCodec *decoder = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *best_frame = nullptr;
    int video_stream = -1;
    int64_t target_pts = 0;
    int64_t duration_sec = 0;
    bool got_frame = false;
    int retcd = -1;

    /* Open video file */
    retcd = avformat_open_input(&fmt_ctx, video_path.c_str(), nullptr, nullptr);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Failed to open video file: %s"), video_path.c_str());
        goto cleanup;
    }

    /* Find stream information */
    retcd = avformat_find_stream_info(fmt_ctx, nullptr);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Failed to find stream info: %s"), video_path.c_str());
        goto cleanup;
    }

    /* Find video stream */
    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (video_stream < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("No video stream found in: %s"), video_path.c_str());
        goto cleanup;
    }

    /* Allocate codec context */
    dec_ctx = avcodec_alloc_context3(decoder);
    if (dec_ctx == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate codec context"));
        goto cleanup;
    }

    /* Copy codec parameters to context */
    retcd = avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream]->codecpar);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to copy codec parameters"));
        goto cleanup;
    }

    /* Open codec */
    retcd = avcodec_open2(dec_ctx, decoder, nullptr);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to open codec"));
        goto cleanup;
    }

    /* Calculate target timestamp in stream time_base */
    target_pts = THUMB_TARGET_SEC * fmt_ctx->streams[video_stream]->time_base.den
                 / fmt_ctx->streams[video_stream]->time_base.num;

    /* Check if video is long enough for 2-second seek */
    if (fmt_ctx->streams[video_stream]->duration != AV_NOPTS_VALUE) {
        duration_sec = fmt_ctx->streams[video_stream]->duration
                       * fmt_ctx->streams[video_stream]->time_base.num
                       / fmt_ctx->streams[video_stream]->time_base.den;
    } else if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        duration_sec = fmt_ctx->duration / AV_TIME_BASE;
    }

    /* Seek to target (or stay at beginning if video too short) */
    if (duration_sec >= THUMB_TARGET_SEC) {
        retcd = av_seek_frame(fmt_ctx, video_stream, target_pts, AVSEEK_FLAG_BACKWARD);
        if (retcd >= 0) {
            avcodec_flush_buffers(dec_ctx);
        }
    } else {
        target_pts = 0;  /* Fallback: use first frame */
    }

    /* Allocate packet and frames */
    pkt = av_packet_alloc();
    *frame = av_frame_alloc();
    best_frame = av_frame_alloc();

    if (pkt == nullptr || *frame == nullptr || best_frame == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate packet/frame"));
        goto cleanup;
    }

    /* Decode frames until we reach or pass target timestamp */
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream) {
            retcd = avcodec_send_packet(dec_ctx, pkt);
            if (retcd < 0 && retcd != AVERROR(EAGAIN)) {
                av_packet_unref(pkt);
                continue;
            }

            while (avcodec_receive_frame(dec_ctx, *frame) == 0) {
                /* Keep the frame closest to (but not before) target */
                if ((*frame)->pts >= target_pts || !got_frame) {
                    av_frame_unref(best_frame);
                    av_frame_move_ref(best_frame, *frame);
                    got_frame = true;

                    /* Found our target frame */
                    if (best_frame->pts >= target_pts) {
                        av_packet_unref(pkt);
                        goto found_frame;
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

found_frame:
    if (got_frame) {
        av_frame_move_ref(*frame, best_frame);
        retcd = 0;
    } else {
        retcd = -1;
    }

cleanup:
    if (best_frame != nullptr) {
        av_frame_free(&best_frame);
    }
    if (pkt != nullptr) {
        av_packet_free(&pkt);
    }
    if (dec_ctx != nullptr) {
        avcodec_free_context(&dec_ctx);
    }
    if (fmt_ctx != nullptr) {
        avformat_close_input(&fmt_ctx);
    }

    return retcd;
}

int cls_thumbnail::encode_thumbnail(AVFrame *frame, const std::string &thumb_path)
{
    uint8_t *src_buffer = nullptr;
    uint8_t *scaled_buffer = nullptr;
    unsigned char *jpg_buffer = nullptr;
    struct SwsContext *swsctx = nullptr;
    AVFrame *yuv_frame = nullptr;
    int jpg_size = 0;
    int thumb_h;
    int jpg_buffer_size;
    int src_buffer_size;
    int retcd = -1;
    FILE *f = nullptr;

    /* Calculate proportional height maintaining aspect ratio */
    thumb_h = (frame->height * THUMB_WIDTH) / frame->width;

    /* Ensure even dimensions for YUV420 */
    thumb_h = (thumb_h / 2) * 2;

    /* Calculate buffer size for source YUV420P data */
    src_buffer_size = (frame->width * frame->height * 3) / 2;

    /* Allocate buffer for source YUV420P conversion */
    src_buffer = (uint8_t*)mymalloc((size_t)src_buffer_size);
    if (src_buffer == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate source buffer"));
        goto cleanup;
    }

    /* Allocate buffer for scaled YUV image */
    scaled_buffer = (uint8_t*)mymalloc(THUMB_WIDTH * thumb_h * 3 / 2);
    if (scaled_buffer == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate scaled buffer"));
        goto cleanup;
    }

    /* Create SwsContext to convert decoded frame to YUV420P contiguous buffer */
    swsctx = sws_getContext(
        frame->width, frame->height, (enum AVPixelFormat)frame->format,
        frame->width, frame->height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to create sws context"));
        goto cleanup;
    }

    /* Allocate output frame for YUV420P conversion */
    yuv_frame = av_frame_alloc();
    if (yuv_frame == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate YUV frame"));
        goto cleanup;
    }

    /* Fill the output frame arrays pointing to our contiguous buffer */
    retcd = av_image_fill_arrays(
        yuv_frame->data, yuv_frame->linesize,
        src_buffer, AV_PIX_FMT_YUV420P,
        frame->width, frame->height, 1);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to fill YUV frame arrays"));
        goto cleanup;
    }

    /* Convert frame to YUV420P in contiguous buffer */
    retcd = sws_scale(
        swsctx,
        (const uint8_t* const*)frame->data, frame->linesize,
        0, frame->height,
        yuv_frame->data, yuv_frame->linesize);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to convert frame to YUV420P"));
        goto cleanup;
    }

    /* Now src_buffer contains contiguous YUV420P data - scale it */
    util_resize(src_buffer, frame->width, frame->height,
                scaled_buffer, THUMB_WIDTH, thumb_h);

    /* Allocate buffer for JPEG output (estimate max size) */
    jpg_buffer_size = THUMB_WIDTH * thumb_h * 3 / 2;  /* Conservative estimate */
    jpg_buffer = (unsigned char*)mymalloc((uint)jpg_buffer_size);
    if (jpg_buffer == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to allocate JPEG buffer"));
        goto cleanup;
    }

    /* Encode JPEG using existing JPEG encoder */
    jpg_size = jpgutl_put_yuv420p(jpg_buffer, jpg_buffer_size,
                                   scaled_buffer, THUMB_WIDTH, thumb_h,
                                   THUMB_QUALITY, nullptr, nullptr, nullptr);

    if (jpg_size <= 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed to encode JPEG"));
        goto cleanup;
    }

    /* Write to file */
    f = fopen(thumb_path.c_str(), "wb");
    if (f == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            , _("Failed to open thumbnail file: %s"), thumb_path.c_str());
        goto cleanup;
    }

    if (fwrite(jpg_buffer, 1, (size_t)jpg_size, f) != (size_t)jpg_size) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            , _("Failed to write thumbnail: %s"), thumb_path.c_str());
        goto cleanup;
    }

    retcd = 0;

cleanup:
    if (f != nullptr) {
        fclose(f);
    }
    if (jpg_buffer != nullptr) {
        free(jpg_buffer);
    }
    if (scaled_buffer != nullptr) {
        free(scaled_buffer);
    }
    if (src_buffer != nullptr) {
        free(src_buffer);
    }
    if (yuv_frame != nullptr) {
        av_frame_free(&yuv_frame);
    }
    if (swsctx != nullptr) {
        sws_freeContext(swsctx);
    }

    return retcd;
}
