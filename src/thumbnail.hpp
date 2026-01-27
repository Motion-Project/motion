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
 * thumbnail.hpp - Video Thumbnail Generation
 *
 * Background worker that generates JPEG thumbnails for video files.
 * Extracts a frame at 2 seconds into the video (or first frame if shorter)
 * and creates a scaled-down JPEG thumbnail for display in the web UI.
 *
 */

#ifndef _INCLUDE_THUMBNAIL_HPP_
#define _INCLUDE_THUMBNAIL_HPP_

#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <string>

class cls_motapp;

class cls_thumbnail {
public:
    cls_thumbnail(cls_motapp *p_app);
    ~cls_thumbnail();

    void queue(const std::string &video_path);
    bool exists(const std::string &video_path);
    std::string path_for(const std::string &video_path);

private:
    cls_motapp          *app;
    std::queue<std::string> pending;
    std::mutex          queue_mutex;
    std::thread         worker_thread;
    std::atomic<bool>   shutdown_flag;
    std::condition_variable queue_cv;

    void worker_loop();
    void generate(const std::string &video_path);
    int extract_frame(const std::string &video_path, AVFrame **frame);
    int encode_thumbnail(AVFrame *frame, const std::string &thumb_path);
};

#endif /* _INCLUDE_THUMBNAIL_HPP_ */
