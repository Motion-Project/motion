/*
 *    video.c
 *
 *    Video stream functions for motion.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
/* Common stuff: */
#include "rotate.h"     /* already includes motion.h */
#include "video.h"

#if defined(HAVE_LINUX_VIDEODEV_H) && !defined(WITHOUT_V4L)

/**
 * v4l_picture_controls
 */
static void v4l_picture_controls(struct context *cnt, struct video_dev *viddev)
{
    int dev = viddev->fd;
    struct video_picture vid_pic;
    int make_change = 0;

    if (cnt->conf.contrast && cnt->conf.contrast != viddev->contrast) {

        if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGPICT)");

        make_change = 1;
        vid_pic.contrast = cnt->conf.contrast * 256;
        viddev->contrast = cnt->conf.contrast;
    }

    if (cnt->conf.saturation && cnt->conf.saturation != viddev->saturation) {

        if (!make_change) {
            if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGPICT)");
        }

        make_change = 1;
        vid_pic.colour = cnt->conf.saturation * 256;
        viddev->saturation = cnt->conf.saturation;
    }

    if (cnt->conf.hue && cnt->conf.hue != viddev->hue) {

        if (!make_change) {
            if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGPICT)");
        }

        make_change = 1;
        vid_pic.hue = cnt->conf.hue * 256;
        viddev->hue = cnt->conf.hue;
    }

/* Only tested with PWCBSD in FreeBSD */    
#if defined(PWCBSD)   
    if (cnt->conf.frame_limit != viddev->fps) {
        struct video_window vw;
        int fps;

        if (ioctl(dev, VIDIOCGWIN, &vw) == -1) { 
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl VIDIOCGWIN");
        } else {
            fps = vw.flags  >> PWC_FPS_SHIFT;
            MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s: Get Current framerate %d .. trying %d", 
                       fps, cnt->conf.frame_limit);
        }

        fps = cnt->conf.frame_limit;
        vw.flags = fps << PWC_FPS_SHIFT;
    
        if (ioctl(dev, VIDIOCSWIN, &vw) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl VIDIOCSWIN");                
        } else if (ioctl(dev, VIDIOCGWIN, &vw) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl VIDIOCGWIN");
        } else {
            fps = vw.flags  >> PWC_FPS_SHIFT;
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set new framerate %d", fps);
        }  

        viddev->fps = fps;        
    }    
#endif

    if (cnt->conf.autobright) {
        
        if (vid_do_autobright(cnt, viddev)) {
            /* If we already read the VIDIOGPICT - we should not do it again. */
            if (!make_change) {
                if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
                    MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGPICT)");
            }
                    
            vid_pic.brightness = viddev->brightness * 256;
            make_change = 1;
        }
    
    } else if (cnt->conf.brightness && cnt->conf.brightness != viddev->brightness) {
        
        if ((!make_change) && (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1))
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGPICT)");
        
        make_change = 1;
        vid_pic.brightness = cnt->conf.brightness * 256;
        viddev->brightness = cnt->conf.brightness;
    }

    if (make_change) {
        if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1)
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCSPICT)");
    }
}

/*******************************************************************************
    Video4linux capture routines
********************************************************************************/

/**
 * v4l_start
 *      Initialize video device to start capturing and allocates memory map 
 *      for video device.
 *      
 * Returns mmapped buffer for video device or NULL if any error happens.
 *
 */ 
unsigned char *v4l_start(struct video_dev *viddev, int width, int height,int input, 
                         int norm, unsigned long freq, int tuner_number)
{
    int dev = viddev->fd;
    struct video_capability vid_caps;
    struct video_channel vid_chnl;
    struct video_tuner vid_tuner;
    struct video_mbuf vid_buf;
    struct video_mmap vid_mmap;
    void *map;

    if (ioctl (dev, VIDIOCGCAP, &vid_caps) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGCAP)");
        return NULL;
    }

    if (vid_caps.type & VID_TYPE_MONOCHROME)
        viddev->v4l_fmt = VIDEO_PALETTE_GREY;

    if (input != IN_DEFAULT) {
        memset(&vid_chnl, 0, sizeof(struct video_channel));
        vid_chnl.channel = input;

        if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGCHAN) Input %d", 
                        input);
        } else {
            vid_chnl.channel = input;
            vid_chnl.norm    = norm;
            if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCSCHAN) Input %d"
                           " Standard method %d", input, norm);
                return NULL;
            }
        }
    }

    if (freq) {
        memset(&vid_tuner, 0, sizeof(struct video_tuner));
        vid_tuner.tuner = tuner_number;
        if (ioctl (dev, VIDIOCGTUNER, &vid_tuner) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGTUNER) tuner %d", 
                       tuner_number);
        } else {
            if (vid_tuner.flags & VIDEO_TUNER_LOW) 
                freq = freq * 16; /* steps of 1/16 KHz */
            else 
                freq = freq * 10 / 625;
            
            if (ioctl(dev, VIDIOCSFREQ, &freq) == -1) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCSFREQ)"
                           " Frequency %ul", freq);
                return NULL;
            }

            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set Tuner to %d Frequency set to %ul", 
                       tuner_number, freq);
        }
    }

    if (ioctl (dev, VIDIOCGMBUF, &vid_buf) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: ioctl(VIDIOCGMBUF) - Error device"
                   " does not support memory map\n V4L capturing using read is deprecated!\n"
                   "Motion only supports mmap.");
        return NULL;
    } else {
        map = mmap(0, vid_buf.size, PROT_READ|PROT_WRITE, MAP_SHARED, dev, 0);
        viddev->size_map = vid_buf.size;

        if (vid_buf.frames > 1) {
            viddev->v4l_maxbuffer = 2;
            viddev->v4l_buffers[0] = map;
            viddev->v4l_buffers[1] = (unsigned char *)map + vid_buf.offsets[1];
        } else {
            viddev->v4l_buffers[0] = map;
            viddev->v4l_maxbuffer = 1;
        }

        if (MAP_FAILED == map) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: MAP_FAILED");
            return NULL;
        }

        viddev->v4l_curbuffer = 0;
        vid_mmap.format = viddev->v4l_fmt;
        vid_mmap.frame = viddev->v4l_curbuffer;
        vid_mmap.width = width;
        vid_mmap.height = height;

        if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
            MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed with YUV420P, "
                       "trying YUV422 palette");
            viddev->v4l_fmt = VIDEO_PALETTE_YUV422;
            vid_mmap.format = viddev->v4l_fmt;
            /* Try again... */
            if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
                MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed with YUV422,"
                           " trying YUYV palette");
                viddev->v4l_fmt = VIDEO_PALETTE_YUYV;
                vid_mmap.format = viddev->v4l_fmt;
                
                if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
                    MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed with YUYV, trying RGB24 palette"); 
                    viddev->v4l_fmt = VIDEO_PALETTE_RGB24;
                    vid_mmap.format = viddev->v4l_fmt;
                    /* Try again... */
                
                    if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
                        MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed with RGB24, trying"
                                   "GREYSCALE palette");
                        viddev->v4l_fmt = VIDEO_PALETTE_GREY;
                        vid_mmap.format = viddev->v4l_fmt;

                        /* Try one last time... */
                        if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
                            MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed with all supported palettes "
                                       "- giving up");
                            return NULL;
                        }
                    }
                }
            }
        }
    }

    switch (viddev->v4l_fmt) {
    case VIDEO_PALETTE_YUV420P:
        viddev->v4l_bufsize = (width * height * 3) / 2;
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using VIDEO_PALETTE_YUV420P palette");
        break;
    case VIDEO_PALETTE_YUV422:
        viddev->v4l_bufsize = (width * height * 2);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using VIDEO_PALETTE_YUV422 palette");
        break;
    case VIDEO_PALETTE_YUYV:
        viddev->v4l_bufsize = (width * height * 2);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using VIDEO_PALETTE_YUYV palette");
        break;
    case VIDEO_PALETTE_RGB24:
        viddev->v4l_bufsize = (width * height * 3);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using VIDEO_PALETTE_RGB24 palette");
        break;
    case VIDEO_PALETTE_GREY:
        viddev->v4l_bufsize = width * height;
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using VIDEO_PALETTE_GREY palette");
        break;
    }

    return map;
}


/**
 * v4l_next
 *                Fetches a video frame from a v4l device
 *
 * Parameters:
 *     viddev     Pointer to struct containing video device handle amd device parameters
 *     map        Pointer to the buffer in which the function puts the new image
 *     width      Width of image in pixels
 *     height     Height of image in pixels
 *
 * Returns
 *     0               Success
 *    V4L_FATAL_ERROR  Fatal error
 *    Positive with bit 0 set and bit 1 unset
 *                     Non fatal error (not implemented)
 */
int v4l_next(struct video_dev *viddev, unsigned char *map, int width, int height)
{
    int dev = viddev->fd;
    int frame = viddev->v4l_curbuffer;
    struct video_mmap vid_mmap;
    unsigned char *cap_map;

    sigset_t  set, old;

    /* MMAP method is used */
    vid_mmap.format = viddev->v4l_fmt;
    vid_mmap.width = width;
    vid_mmap.height = height;

    /* Block signals during IOCTL */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    pthread_sigmask (SIG_BLOCK, &set, &old);

    cap_map = viddev->v4l_buffers[viddev->v4l_curbuffer];
    viddev->v4l_curbuffer++;

    if (viddev->v4l_curbuffer >= viddev->v4l_maxbuffer)
        viddev->v4l_curbuffer = 0;

    vid_mmap.frame = viddev->v4l_curbuffer;

    if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
        MOTION_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO, "%s: mcapture error in proc %d", 
                   getpid());
        sigprocmask (SIG_UNBLOCK, &old, NULL);
        return V4L_FATAL_ERROR;
    }

    vid_mmap.frame = frame;

    if (ioctl(dev, VIDIOCSYNC, &vid_mmap.frame) == -1) {
        MOTION_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO, "%s: sync error in proc %d", 
                   getpid());
        sigprocmask (SIG_UNBLOCK, &old, NULL);
    }

    pthread_sigmask (SIG_UNBLOCK, &old, NULL);   /*undo the signal blocking*/

    switch (viddev->v4l_fmt) {
    case VIDEO_PALETTE_RGB24:
        conv_rgb24toyuv420p(map, cap_map, width, height);
        break;
    case VIDEO_PALETTE_YUYV:
    case VIDEO_PALETTE_YUV422:
        conv_yuv422to420p(map, cap_map, width, height);
        break;
    default:
        memcpy(map, cap_map, viddev->v4l_bufsize);
    }

    return 0;
}

/**
 * v4l_set_input
 *          Sets input for video device, adjust picture controls. 
 *          If needed skip frames for round robin.
 *
 * Parameters:
 *      cnt     Pointer to context struct
 *      viddev  Pointer to struct containing video device handle amd device parameters
 *      map     Pointer to the buffer in which the function puts the new image
 *      width   Width of image in pixels
 *      height  Height of image in pixels
 *      conf    Pointer to config struct
 *
 * Returns nothing
 */ 
void v4l_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, 
                   int width, int height, struct config *conf)
{
    int dev = viddev->fd;
    struct video_channel vid_chnl;
    struct video_tuner vid_tuner;
    unsigned long frequnits , freq;
    int input = conf->input;
    int norm = conf->norm;
    int tuner_number = conf->tuner_number;
    
    frequnits = freq = conf->frequency;

    if (input != viddev->input || width != viddev->width || height != viddev->height ||
        freq != viddev->freq || tuner_number != viddev->tuner_number || norm != viddev->norm) {
        unsigned int skip = conf->roundrobin_skip, i;      
        
        if (freq) {
            memset(&vid_tuner, 0, sizeof(struct video_tuner));
            vid_tuner.tuner = tuner_number;

            if (ioctl (dev, VIDIOCGTUNER, &vid_tuner) == -1) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGTUNER) tuner number %d", 
                           tuner_number);
            } else {
                if (vid_tuner.flags & VIDEO_TUNER_LOW) 
                    frequnits = freq * 16; /* steps of 1/16 KHz */
                else 
                    frequnits = (freq * 10) / 625;
                
                if (ioctl(dev, VIDIOCSFREQ, &frequnits) == -1) {
                    MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCSFREQ) Frequency %ul", 
                               frequnits);
                    return;
                }

                 MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set Tuner to %d Frequency to %ul",
                            tuner_number, frequnits);
            }
        }

        memset(&vid_chnl, 0, sizeof(struct video_channel));
        vid_chnl.channel = input;
        
        if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCGCHAN) Input %d", 
                       input);
        } else {
            vid_chnl.channel = input;
            vid_chnl.norm = norm;
            
            if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOCSCHAN) Input %d"
                           " Standard method %d", input, norm);
                return;
            } 

            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set Input to %d Standard method to %d", 
                       input, norm);
        }

        v4l_picture_controls(cnt, viddev);
        conf->input = viddev->input = input;
        conf->width = viddev->width = width;
        conf->height = viddev->height = height;
        conf->frequency = viddev->freq = freq;
        conf->tuner_number = viddev->tuner_number = tuner_number;
        conf->norm = viddev->norm = norm;
        /* skip a few frames if needed */
        for (i = 0; i < skip; i++)
            v4l_next(viddev, map, width, height);
    } else {
        /* No round robin - we only adjust picture controls */
        v4l_picture_controls(cnt, viddev);
    }
}
#endif /* !WITHOUT_V4L */
