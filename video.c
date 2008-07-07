/*	video.c
 *
 *	Video stream functions for motion.
 *	Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *	This software is distributed under the GNU public license version 2
 *	See also the file 'COPYING'.
 *
 */
#ifndef WITHOUT_V4L

/* Common stuff: */
#include "rotate.h"     /* already includes motion.h */
#include "video.h"


/* for the v4l stuff: */
#include <sys/mman.h>
#include <math.h>
#include <sys/utsname.h>
#include <dirent.h>


static void v4l_picture_controls(struct context *cnt, struct video_dev *viddev)
{
	int dev = viddev->fd;
	struct video_picture vid_pic;
	int make_change = 0;

	if (cnt->conf.contrast && cnt->conf.contrast != viddev->contrast) {

		if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
			motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);

		make_change = 1;
		vid_pic.contrast = cnt->conf.contrast * 256;
		viddev->contrast = cnt->conf.contrast;
	}

	if (cnt->conf.saturation && cnt->conf.saturation != viddev->saturation) {

		if (!make_change) {
			if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
		}

		make_change = 1;
		vid_pic.colour = cnt->conf.saturation * 256;
		viddev->saturation = cnt->conf.saturation;
	}

	if (cnt->conf.hue && cnt->conf.hue != viddev->hue) {

		if (!make_change) {
			if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
		}

		make_change = 1;
		vid_pic.hue = cnt->conf.hue * 256;
		viddev->hue = cnt->conf.hue;
	}
	
	if (cnt->conf.autobright) {
		
		if (vid_do_autobright(cnt, viddev)) {
			/* If we already read the VIDIOGPICT - we should not do it again */
			if (!make_change) {
				if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
					motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
			}
					
			vid_pic.brightness = viddev->brightness * 256;
			make_change = 1;
		}
	
	} else {
		if (cnt->conf.brightness && cnt->conf.brightness != viddev->brightness) {
			if (!make_change) {
				if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1)
					motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
			}
	
			make_change = 1;
			vid_pic.brightness = cnt->conf.brightness * 256;
			viddev->brightness = cnt->conf.brightness;
		}
	}

	if (make_change) {
		if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1)
			motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSPICT)", __FUNCTION__);
	}
}



/*******************************************************************************************
	Video4linux capture routines
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
		motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGCAP)", __FUNCTION__);
		return (NULL);
	}

	if (vid_caps.type & VID_TYPE_MONOCHROME)
		viddev->v4l_fmt = VIDEO_PALETTE_GREY;

	if (input != IN_DEFAULT) {
		memset(&vid_chnl, 0, sizeof(struct video_channel));
		vid_chnl.channel = input;

		if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
			motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGCHAN)", __FUNCTION__);
		} else {
			vid_chnl.channel = input;
			vid_chnl.norm    = norm;
			if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSCHAN)", __FUNCTION__);
				return (NULL);
			}
		}
	}

	if (freq) {
		memset(&vid_tuner, 0, sizeof(struct video_tuner));
		vid_tuner.tuner = tuner_number;
		if (ioctl (dev, VIDIOCGTUNER, &vid_tuner) == -1) {
			motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGTUNER)", __FUNCTION__);
		} else {
			if (vid_tuner.flags & VIDEO_TUNER_LOW) {
				freq = freq*16; /* steps of 1/16 KHz */
			} else {
				freq = (freq*10)/625;
			}
			if (ioctl(dev, VIDIOCSFREQ, &freq) == -1) {
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSFREQ)", __FUNCTION__);
				return (NULL);
			}
			if (debug_level >= CAMERA_VERBOSE)
				motion_log(-1, 0, "%s: Frequency set", __FUNCTION__);
		}
	}

	if (ioctl (dev, VIDIOCGMBUF, &vid_buf) == -1) {
		motion_log(LOG_ERR, 0, "%s: ioctl(VIDIOCGMBUF) - Error device does not support memory map\n", 
		           "V4L capturing using read is deprecated!\nMotion only supports mmap.", __FUNCTION__);
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
			motion_log(LOG_ERR, 1, "%s: MAP_FAILED", __FUNCTION__);
			return (NULL);
		}
		viddev->v4l_curbuffer = 0;
		vid_mmap.format = viddev->v4l_fmt;
		vid_mmap.frame = viddev->v4l_curbuffer;
		vid_mmap.width = width;
		vid_mmap.height = height;
		if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
			motion_log(LOG_DEBUG, 1, "%s: Failed with YUV420P, trying YUV422 palette", 
			           __FUNCTION__);
			viddev->v4l_fmt = VIDEO_PALETTE_YUV422;
			vid_mmap.format = viddev->v4l_fmt;
			/* Try again... */
			if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
				motion_log(LOG_DEBUG, 1, "%s: Failed with YUV422, trying YUYV palette", 
				           __FUNCTION__);
				viddev->v4l_fmt = VIDEO_PALETTE_YUYV;
				vid_mmap.format = viddev->v4l_fmt;
				
				if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
					motion_log(LOG_DEBUG, 1, "%s: Failed with YUYV, trying RGB24 palette", 
					           __FUNCTION__);
					viddev->v4l_fmt = VIDEO_PALETTE_RGB24;
					vid_mmap.format = viddev->v4l_fmt;
					/* Try again... */
				
					if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
						motion_log(LOG_DEBUG, 1, "%s: Failed with RGB24, trying"
						           "GREYSCALE palette", __FUNCTION__);
						viddev->v4l_fmt = VIDEO_PALETTE_GREY;
						vid_mmap.format = viddev->v4l_fmt;
						/* Try one last time... */
						if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
							motion_log(LOG_ERR, 1, "%s: Failed with all supported palettes "
						                   "- giving up", __FUNCTION__);
							return (NULL);
						}
					}
				}
			}
		}
	}

	switch (viddev->v4l_fmt) {
		case VIDEO_PALETTE_YUV420P:
			viddev->v4l_bufsize = (width * height * 3) / 2;
			motion_log(LOG_DEBUG, 0, "%s: Using VIDEO_PALETTE_YUV420P palette", __FUNCTION__);
			break;
		case VIDEO_PALETTE_YUV422:
			viddev->v4l_bufsize = (width * height * 2);
			motion_log(LOG_DEBUG, 0, "%s: Using VIDEO_PALETTE_YUV422 palette", __FUNCTION__);
			break;
		case VIDEO_PALETTE_YUYV:
			viddev->v4l_bufsize = (width * height * 2);
			motion_log(LOG_DEBUG, 0, "%s: Using VIDEO_PALETTE_YUYV palette", __FUNCTION__);
			break;
		case VIDEO_PALETTE_RGB24:
			viddev->v4l_bufsize = (width * height * 3);
			motion_log(LOG_DEBUG, 0, "%s: Using VIDEO_PALETTE_RGB24 palette", __FUNCTION__);
			break;
		case VIDEO_PALETTE_GREY:
			viddev->v4l_bufsize = width * height;
			motion_log(LOG_DEBUG, 0, "%s: Using VIDEO_PALETTE_GREY palette", __FUNCTION__);
			break;
	}
	return map;
}


/**
 * v4l_next
 *                v4l_next fetches a video frame from a v4l device
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
		motion_log(LOG_ERR, 1, "%s: mcapture error in proc %d", __FUNCTION__, getpid());
		sigprocmask (SIG_UNBLOCK, &old, NULL);
		return V4L_FATAL_ERROR;
	}

	vid_mmap.frame = frame;

	if (ioctl(dev, VIDIOCSYNC, &vid_mmap.frame) == -1) {
		motion_log(LOG_ERR, 1, "%s: sync error in proc %d", __FUNCTION__, getpid());
		sigprocmask (SIG_UNBLOCK, &old, NULL);
	}

	pthread_sigmask (SIG_UNBLOCK, &old, NULL);        /*undo the signal blocking*/

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

void v4l_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height, int input,
                    int norm, int skip, unsigned long freq, int tuner_number)
{
	int dev = viddev->fd;
	int i;
	struct video_channel vid_chnl;
	struct video_tuner vid_tuner;
	unsigned long frequnits = freq;
	
	if (input != viddev->input || width != viddev->width || height != viddev->height ||
	    freq != viddev->freq || tuner_number != viddev->tuner_number) {
		if (freq) {

			memset(&vid_tuner, 0, sizeof(struct video_tuner));

			vid_tuner.tuner = tuner_number;
			if (ioctl (dev, VIDIOCGTUNER, &vid_tuner) == -1) {
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGTUNER)", __FUNCTION__);
			} else {
				if (vid_tuner.flags & VIDEO_TUNER_LOW) {
					frequnits = freq*16; /* steps of 1/16 KHz */
				} else {
					frequnits = (freq*10)/625;
				}
				if (ioctl(dev, VIDIOCSFREQ, &frequnits) == -1) {
					motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSFREQ)", __FUNCTION__);
					return;
				}
			}
		}

		memset(&vid_chnl, 0, sizeof(struct video_channel));
		vid_chnl.channel = input;
		
		if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
			motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGCHAN)", __FUNCTION__);
		} else {
			vid_chnl.channel = input;
			vid_chnl.norm = norm;
			if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
				motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSCHAN)", __FUNCTION__);
				return;
			}
		}
		v4l_picture_controls(cnt, viddev);
		viddev->input = input;
		viddev->width = width;
		viddev->height = height;
		viddev->freq = freq;
		viddev->tuner_number = tuner_number;
		/* skip a few frames if needed */
		for (i = 0; i < skip; i++)
			v4l_next(viddev, map, width, height);
	} else {
		/* No round robin - we only adjust picture controls */
		v4l_picture_controls(cnt, viddev);
	}
}

static int v4l_open_vidpipe(void)
{
	int pipe_fd = -1;
	char pipepath[255];
	char buffer[255];
	char *major;
	char *minor;
	struct utsname uts;

	if (uname(&uts) < 0) {
		motion_log(LOG_ERR, 1, "%s: Unable to execute uname", __FUNCTION__);
		return -1;
	}
	major = strtok(uts.release, ".");
	minor = strtok(NULL, ".");
	if ((major == NULL) || (minor == NULL) || (strcmp(major, "2"))) {
		motion_log(LOG_ERR, 1, "%s: Unable to decipher OS version", __FUNCTION__);
		return -1;
	}
	if (strcmp(minor, "5") < 0) {
		FILE *vloopbacks;
		char *loop;
		char *input;
		char *istatus;
		char *output;
		char *ostatus;

		vloopbacks = fopen("/proc/video/vloopback/vloopbacks", "r");
		if (!vloopbacks) {
			motion_log(LOG_ERR, 1, "%s: Failed to open '/proc/video/vloopback/vloopbacks'", 
			           __FUNCTION__);
			return -1;
		}
		
		/* Read vloopback version*/
		if (!fgets(buffer, 255, vloopbacks)) {
			motion_log(LOG_ERR, 1, "%s: Unable to read vloopback version", __FUNCTION__);
			return -1;
		}
		
		fprintf(stderr, "\t%s", buffer);
		
		/* Read explanation line */
		
		if (!fgets(buffer, 255, vloopbacks)) {
			motion_log(LOG_ERR, 1, "%s: Unable to read vloopback explanation line", 
			           __FUNCTION__);
			return -1;
		}
		
		while (fgets(buffer, 255, vloopbacks)) {
			if (strlen(buffer) > 1) {
				buffer[strlen(buffer)-1] = 0;
				loop = strtok(buffer, "\t");
				input = strtok(NULL, "\t");
				istatus = strtok(NULL, "\t");
				output = strtok(NULL, "\t");
				ostatus = strtok(NULL, "\t");
				if (istatus[0] == '-') {
					snprintf(pipepath, 255, "/dev/%s", input);
					pipe_fd = open(pipepath, O_RDWR);
					if (pipe_fd >= 0) {
						motion_log(-1, 0, "%s: \tInput:  /dev/%s \tOutput: /dev/%s",
						           __FUNCTION__, input, output);
						break;
					}
				}
			}
		}
		fclose(vloopbacks);
	} else {
		DIR *dir;
		struct dirent *dirp;
		const char prefix[] = "/sys/class/video4linux/";
		char *ptr, *io;
		int fd;
		int low = 9999;
		int tfd;
		int tnum;

		if ((dir = opendir(prefix)) == NULL) {
			motion_log(LOG_ERR, 1, "%s: Failed to open '%s'", __FUNCTION__, prefix);
			return -1;
		}
		while ((dirp = readdir(dir)) != NULL) {
			if (!strncmp(dirp->d_name, "video", 5)) {
				strncpy(buffer, prefix, 255);
				strncat(buffer, dirp->d_name, 255);
				strncat(buffer, "/name", 255);
				if ((fd = open(buffer, O_RDONLY)) >= 0) {
					if ((read(fd, buffer, sizeof(buffer)-1)) < 0) {
						close(fd);
						continue;
					}
					ptr = strtok(buffer, " ");
					if (strcmp(ptr, "Video")) {
						close(fd);
						continue;
					}
					major = strtok(NULL, " ");
					minor = strtok(NULL, " ");
					io  = strtok(NULL, " \n");
					if (strcmp(major, "loopback") || strcmp(io, "input")) {
						close(fd);
						continue;
					}
					if ((ptr = strtok(buffer, " ")) == NULL) {
						close(fd);
						continue;
					}
					tnum = atoi(minor);
					if (tnum < low) {
						strcpy(buffer, "/dev/");
						strcat(buffer, dirp->d_name);
						if ((tfd = open(buffer, O_RDWR)) >= 0) {
							strcpy(pipepath, buffer);
							if (pipe_fd >= 0) {
								close(pipe_fd);
							}
							pipe_fd = tfd;
							low = tnum;
						}
					}
					close(fd);
				}
			}
		}
		closedir(dir);
		if (pipe_fd >= 0)
			motion_log(-1, 0, "%s: Opened input of %s", __FUNCTION__, pipepath);
	}
	return pipe_fd;
}

static int v4l_startpipe(const char *dev_name, int width, int height, int type)
{
	int dev;
	struct video_picture vid_pic;
	struct video_window vid_win;

	if (!strcmp(dev_name, "-")) {
		dev = v4l_open_vidpipe();
	} else {
		dev = open(dev_name, O_RDWR);
	}
	if (dev < 0)
		return(-1);

	if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1) {
		motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
		return(-1);
	}
	vid_pic.palette = type;
	if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1) {
		motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSPICT)", __FUNCTION__);
		return(-1);
	}
	if (ioctl(dev, VIDIOCGWIN, &vid_win) == -1) {
		motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGWIN)", __FUNCTION__);
		return(-1);
	}
	vid_win.height = height;
	vid_win.width = width;
	if (ioctl(dev, VIDIOCSWIN, &vid_win) == -1) {
		motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSWIN)", __FUNCTION__);
		return(-1);
	}
	return dev;
}

static int v4l_putpipe (int dev, unsigned char *image, int size)
{
	return write(dev, image, size);
}


int vid_startpipe(const char *dev_name, int width, int height, int type)
{
	return v4l_startpipe(dev_name, width, height, type);
}

int vid_putpipe (int dev, unsigned char *image, int size)
{
	return v4l_putpipe(dev, image, size);
}
#endif /*WITHOUT_V4L*/
