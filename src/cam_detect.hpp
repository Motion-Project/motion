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
 * cam_detect.hpp - Camera Auto-Detection System
 *
 * Provides intelligent camera detection for Pi cameras (CSI/libcamera),
 * USB/V4L2 cameras, and network cameras with device info extraction,
 * blacklist filtering, and sensor-aware default configuration.
 *
 */

#ifndef _INCLUDE_CAM_DETECT_HPP_
#define _INCLUDE_CAM_DETECT_HPP_

#include <string>
#include <vector>
#include <utility>

/* Camera type detected */
enum CAM_DETECT_TYPE {
    CAM_DETECT_LIBCAM,  /* Pi camera via libcamera (CSI) */
    CAM_DETECT_V4L2,    /* USB/V4L2 camera */
    CAM_DETECT_NETCAM   /* Network camera (manually configured) */
};

/* Detected camera information */
struct ctx_detected_cam {
    CAM_DETECT_TYPE type;
    std::string device_id;      /* Persistent identifier (e.g., /dev/v4l/by-id/...) */
    std::string device_path;    /* /dev/video0 or libcam id */
    std::string device_name;    /* Human-readable name */
    std::string sensor_model;   /* imx708, imx219, etc. */
    int default_width;
    int default_height;
    int default_fps;
    bool already_configured;    /* True if this device is already in config */
    std::vector<std::pair<int,int>> resolutions;
};

/* Platform information */
struct ctx_platform_info {
    bool is_raspberry_pi;
    std::string pi_model;       /* "Pi 4", "Pi 5", etc. */
    bool has_libcamera;
    bool has_v4l2;
};

class cls_motapp;

/* Camera detection and platform identification */
class cls_cam_detect {
    public:
        cls_cam_detect(cls_motapp *p_app);
        ~cls_cam_detect();

        /* Get platform information (CPU, capabilities) */
        ctx_platform_info get_platform_info();

        /* Detect all cameras (libcam + V4L2) */
        std::vector<ctx_detected_cam> detect_cameras();

        /* Test network camera connection */
        bool test_netcam(const std::string &url, const std::string &user,
                        const std::string &pass, int timeout_sec);

    private:
        cls_motapp *app;

        /* Platform detection */
        bool is_raspberry_pi();
        std::string get_pi_model();
        bool has_libcamera_support();
        bool has_v4l2_support();

        /* Pi camera detection (CSI via libcamera) */
        std::vector<ctx_detected_cam> detect_libcam();

        /* USB/V4L2 camera detection */
        std::vector<ctx_detected_cam> detect_v4l2();

        /* Check if device is already configured */
        bool is_device_configured(const std::string &device_id,
                                 const std::string &device_path);

        /* Apply sensor-aware defaults */
        void apply_sensor_defaults(ctx_detected_cam &cam);

        /* Get available resolutions for device */
        std::vector<std::pair<int,int>> get_device_resolutions(const std::string &device_path);

        /* V4L2 helpers */
        std::string get_v4l2_device_name(const std::string &device_path);
        std::string get_v4l2_persistent_id(const std::string &device_path);
        bool is_v4l2_blacklisted(const std::string &device_name);

        /* V4L2 blacklist (ISP devices, not actual cameras) */
        const std::vector<std::string> v4l2_blacklist = {
            "bcm2835-codec", "pispbe", "bcm2835-isp",
            "rpivid", "unicam", "rp1-cfe"
        };
};

#endif /* _INCLUDE_CAM_DETECT_HPP_ */
