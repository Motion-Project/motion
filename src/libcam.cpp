/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020-2022 MotionMrDave@gmail.com
 *
 */

/* TODO:
 * 1.  Identify parameters for camera and print them out to the user.
 * 2.  Apply user provided parameters to the camera.
 * 3.  Determine if we need to have multiple requests or buffers.
 *     (The current logic is just a single request and buffer but
 *      this may need to change to allow for multiple requests or buffers
 *      so as to reduce latency.  As of now, it is kept simple with
 *      a single request and buffer.)
 * 4.  Need to determine flags for designating start up, shutdown
 *     etc. and possibly add mutex locking.  Startup currently has
 *     a SLEEP to allow for initialization but this should change
 * 5.  The shutdown is not completely cleaning up something with libcamera.
 *     Upon a stop of a camera and starting it again libcamera reports
 *     that the v4l2 devices are busy and can not be opened.  Then it
 *     seg aborts upon us.
 */
#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "video_common.hpp"
#include "libcam.hpp"

#ifdef HAVE_LIBCAM

using namespace libcamera;

void cls_libcam::cam_start_params(ctx_cam *ptr)
{
    int indx;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    camctx = ptr;

    camctx->libcam->params = (ctx_params*)mymalloc(sizeof(struct ctx_params));
    camctx->libcam->params->update_params = true;
    util_parms_parse(camctx->libcam->params, camctx->conf->libcam_params);

    for (indx = 0; indx < camctx->libcam->params->params_count; indx++) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s : %s"
            ,camctx->libcam->params->params_array[indx].param_name
            ,camctx->libcam->params->params_array[indx].param_value
            );
    }
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

}

int cls_libcam::cam_start_mgr()
{
    int retcd;
    std::string camid;
    libcamera::Size picsz;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    cam_mgr = std::make_unique<CameraManager>();
    retcd = cam_mgr->start();
    if (retcd != 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Error starting camera manager.  Return code: %d",retcd);
        return retcd;
    }
    started_mgr = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "cam_mgr started.");

    if (camctx->conf->libcam_name == "camera0"){
        camid = cam_mgr->cameras()[0]->id();
    } else {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Invalid libcam_name '%s'.  The only name supported is 'camera0' "
            ,camctx->conf->libcam_name.c_str());
        return -1;
    }
    camera = cam_mgr->get(camid);
    camera->acquire();
    started_aqr = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

int cls_libcam::cam_start_config()
{
    int retcd;
    libcamera::Size picsz;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    config = camera->generateConfiguration({ StreamRole::Viewfinder });
    config->at(0).pixelFormat = PixelFormat::fromString("YUV420");

    config->at(0).size.width = camctx->conf->width;
    config->at(0).size.height = camctx->conf->height;
    config->at(0).bufferCount = 1;

    retcd = config->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        if (config->at(0).pixelFormat != PixelFormat::fromString("YUV420")) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                , "Pixel format was adjusted to %s."
                , config->at(0).pixelFormat.toString().c_str());
            return -1;
        }
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Error setting configuration");
        return -1;
    }

    if ((config->at(0).size.width != (unsigned int)camctx->conf->width) ||
        (config->at(0).size.height != (unsigned int)camctx->conf->height)) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            , "Image size adjusted from %d x %d to %d x %d"
            , camctx->conf->width
            , camctx->conf->height
            , config->at(0).size.width
            , config->at(0).size.height);
    }

    camctx->imgs.width = config->at(0).size.width;
    camctx->imgs.height = config->at(0).size.height;
    camctx->imgs.size_norm = (camctx->imgs.width * camctx->imgs.height * 3) / 2;
    camctx->imgs.motionsize = camctx->imgs.width * camctx->imgs.height;

    camera->configure(config.get());

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

int cls_libcam::req_add(Request *request)
{
    int retcd;
    retcd = camera->queueRequest(request);
    return retcd;
}

int cls_libcam::cam_start_req()
{
    int retcd, bytes;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    camera->requestCompleted.connect(this, &cls_libcam::req_complete);
    frmbuf = std::make_unique<FrameBufferAllocator>(camera);

    retcd = frmbuf->allocate(config->at(0).stream());
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Buffer allocation error.");
        return -1;
    }

    std::unique_ptr<Request> request = camera->createRequest();
    if (!request) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Create request error.");
        return -1;
    }

    Stream *stream = config->at(0).stream();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers =
        frmbuf->buffers(stream);
    const std::unique_ptr<FrameBuffer> &buffer = buffers[0];

    retcd = request->addBuffer(stream, buffer.get());
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Add buffer for request error.");
        return -1;
    }

    const FrameBuffer::Plane &plane0 = buffer->planes()[0];
    bytes = plane0.length;
    if (buffer->planes().size() > 1) {
        const FrameBuffer::Plane &plane1 = buffer->planes()[1];
        bytes += plane1.length;
    }
    if (buffer->planes().size() > 2) {
        const FrameBuffer::Plane &plane2 = buffer->planes()[2];
        bytes += plane2.length;
    }
    if (bytes > camctx->imgs.size_norm){
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Image size error.  Usual size is %d but planes size is %d"
            ,camctx->imgs.size_norm, bytes);
        return -1;
    }

    membuf.buf = (uint8_t *)mmap(NULL,bytes, PROT_READ
        , MAP_SHARED, plane0.fd.get(), 0);
    membuf.bufsz = bytes;

    requests.push_back(std::move(request));
    started_req = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

int cls_libcam::cam_start_capture()
{
    int retcd;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    retcd = camera->start(&this->controls);
    if (retcd) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Failed to start capture.");
        return -1;
    }
    controls.clear();

    for (std::unique_ptr<Request> &request : requests) {
        retcd = req_add(request.get());
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                , "Failed to queue request.");
            if (started_cam) {
                camera->stop();
            }
            return -1;
        }
    }
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

void cls_libcam::req_complete(Request *request)
{
    if (request->status() == Request::RequestCancelled) {
        return;
    }
    req_queue.push(request);
}

int cls_libcam::cam_start(ctx_cam *cam)
{
    int retcd;

    started_cam = false;
    started_mgr = false;
    started_aqr = false;
    started_req = false;

    cam_start_params(cam);

    retcd = cam_start_mgr();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_config();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_req();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_capture();
    if (retcd != 0) {
        return -1;
    }

    SLEEP(2,0);

    started_cam = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Started all.");

    return 0;
}

void cls_libcam::cam_stop()
{
    util_parms_free(camctx->libcam->params);
    myfree(&camctx->libcam->params);
    camctx->libcam->params = NULL;

    if (started_aqr) {
        camera->stop();
    }
    if (started_req) {
        camera->requestCompleted.disconnect(this, &cls_libcam::req_complete);
        while (req_queue.empty() == false) {
            req_queue.pop();
        }
        requests.clear();

        frmbuf->free(config->at(0).stream());
        frmbuf.reset();
    }

    controls.clear();

    if (started_aqr){
        camera->release();
        camera.reset();
    }
    if (started_mgr) {
        cam_mgr->stop();
        cam_mgr.reset();
    }
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Stopped.");
}

/* get the image from libcam */
int cls_libcam::cam_next(ctx_image_data *img_data)
{
    int indx;

    if (started_cam == false) {
        return -1;
    }

    /* Allow time for request to finish.*/
    indx=0;
    while ((req_queue.empty() == true) && (indx < 50)) {
        SLEEP(0,2000)
        indx++;
    }

    if (req_queue.empty() == false) {
        Request *request = this->req_queue.front();

        memcpy(img_data->image_norm, membuf.buf, membuf.bufsz);

        this->req_queue.pop();

        request->reuse(Request::ReuseBuffers);
        req_add(request);
        return 0;

    } else {
        return -1;
    }
}

#endif

/** initialize and start libcam */
int libcam_start(ctx_cam *cam)
{
    #ifdef HAVE_LIBCAM
        int retcd;
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting experimental libcamera .");
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "EXPECT crashes and hung processes!!!");
        cam->libcam = new cls_libcam;
        retcd = cam->libcam->cam_start(cam);
        return retcd;
    #else
        (void)cam;
        return -1;
    #endif
}

/** close and stop libcam */
void libcam_cleanup(struct ctx_cam *cam)
{
    #ifdef HAVE_LIBCAM
        cam->libcam->cam_stop();
        delete cam->libcam;
        cam->libcam = nullptr;
   #else
        (void)cam;
    #endif
}

/** get next image from libcam */
int libcam_next(struct ctx_cam *cam,  struct ctx_image_data *img_data)
{
    #ifdef HAVE_LIBCAM
        int retcd;

        if (cam->libcam == nullptr){
            return -1;
        }
        retcd = cam->libcam->cam_next(img_data);
        if (retcd == 0) {
            rotate_map(cam, img_data);
        }
        return retcd;
    #else
        (void)cam;
        (void)img_data;
        return -1;
    #endif
}

