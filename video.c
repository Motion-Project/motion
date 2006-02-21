/*	video.c
 *
 *	Video stream functions for motion.
 *	Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *	This software is distributed under the GNU public license version 2
 *	See also the file 'COPYING'.
 *
 */

/* Common stuff: */
#include "motion.h"
#include "video.h"
/* for rotation */
#include "rotate.h"

#ifndef WITHOUT_V4L

/* for the v4l stuff: */
#include "pwc-ioctl.h"
#include <sys/mman.h>
//#include "sys/ioctl.h"
#include <math.h>
#include <sys/utsname.h>
#include <dirent.h>

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MIN2(x, y) ((x) < (y) ? (x) : (y))

/* Constants used by auto brightness feature
 * Defined as constant to make it easier for people to tweak code for a
 * difficult camera.
 * The experience gained from people could help improving the feature without
 * adding too many new options.
 * AUTOBRIGHT_HYSTERESIS sets the minimum the light intensity must change before
 * we adjust brigtness.
 * AUTOBRIGHTS_DAMPER damps the speed with which we adjust the brightness
 * When the brightness changes a lot we step in large steps and as we approach the
 * target value we slow down to avoid overshoot and oscillations. If the camera
 * adjusts too slowly decrease the DAMPER value. If the camera oscillates try
 * increasing the DAMPER value. DAMPER must be minimum 1.
 * MAX and MIN are the max and min values of brightness setting we will send to
 * the camera device.
 */
#define AUTOBRIGHT_HYSTERESIS 10
#define AUTOBRIGHT_DAMPER 5
#define AUTOBRIGHT_MAX 255
#define AUTOBRIGHT_MIN 0

static void v4l_picture_controls(struct context *cnt, struct video_dev *viddev)
{
	int dev = viddev->fd;
	unsigned char *image = cnt->imgs.image_ring_buffer;
	struct video_picture vid_pic;
	int make_change = 0;

	if (cnt->conf.contrast && cnt->conf.contrast != viddev->contrast) {

		if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
			motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");

		make_change = 1;
		vid_pic.contrast = cnt->conf.contrast * 256;
		viddev->contrast = cnt->conf.contrast;
	}

	if (cnt->conf.saturation && cnt->conf.saturation != viddev->saturation) {

		if (!make_change) {
			if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");
		}

		make_change = 1;
		vid_pic.colour = cnt->conf.saturation * 256;
		viddev->saturation = cnt->conf.saturation;
	}

	if (cnt->conf.hue && cnt->conf.hue != viddev->hue) {

		if (!make_change) {
			if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");
		}

		make_change = 1;
		vid_pic.hue = cnt->conf.hue * 256;
		viddev->hue = cnt->conf.hue;
	}
	
	if (cnt->conf.autobright) {
		
		int brightness_window_high;
		int brightness_window_low;
		int brightness_target;
		int i, j = 0, avg = 0, step = 0;
		
		if (cnt->conf.brightness)
			brightness_target = cnt->conf.brightness;
		else
			brightness_target = 128;
		
		brightness_window_high = MIN2(brightness_target + AUTOBRIGHT_HYSTERESIS, 255);
		brightness_window_low = MAX2(brightness_target - AUTOBRIGHT_HYSTERESIS, 1);
		
		for (i = 0; i < cnt->imgs.motionsize; i += 101) {
			avg += image[i];
			j++;
		}
		avg = avg / j;
		
		if (avg > brightness_window_high || avg < brightness_window_low) {
			/* If we already read the VIDIOGPICT - we should not do it again */
			if (!make_change) {
				if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
					motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");
			}
			/* average is above window - turn down brightness - go for the target */
			if (avg > brightness_window_high) {
				step = MIN2((avg - brightness_target)/AUTOBRIGHT_DAMPER + 1,
				             viddev->brightness - AUTOBRIGHT_MIN);
				if (viddev->brightness > step + 1 - AUTOBRIGHT_MIN) {
					viddev->brightness -= step;
					vid_pic.brightness = viddev->brightness * 256;
					make_change = 1;
				}
			}
			/* average is below window - turn up brightness - go for the target */
			if (avg < brightness_window_low) {
				step = MIN2((brightness_target - avg)/AUTOBRIGHT_DAMPER + 1,
				             AUTOBRIGHT_MAX - viddev->brightness);
				if (viddev->brightness < AUTOBRIGHT_MAX - step ) {
					viddev->brightness += step;
					vid_pic.brightness = viddev->brightness * 256;
					make_change = 1;
				}
			}
		}
	} else {
		if (cnt->conf.brightness && cnt->conf.brightness != viddev->brightness) {
			if (!make_change) {
				if (ioctl(dev, VIDIOCGPICT, &vid_pic)==-1)
					motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");
			}
	
			make_change = 1;
			vid_pic.brightness = cnt->conf.brightness * 256;
			viddev->brightness = cnt->conf.brightness;
		}
	}

	if (make_change) {
		if (ioctl(dev, VIDIOCSPICT, &vid_pic)==-1)
			motion_log(LOG_ERR, 1, "ioctl (VIDIOCSPICT)");
	}
}
		

static void yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
	unsigned char *src, *dest, *src2, *dest2;
	int i, j;

	/* Create the Y plane */
	src=cap_map;
	dest=map;
	for (i=width*height; i; i--) {
		*dest++=*src;
		src+=2;
	}
	/* Create U and V planes */
	src=cap_map+1;
	src2=cap_map+width*2+1;
	dest=map+width*height;
	dest2=dest+(width*height)/4;
	for (i=height/2; i; i--) {
		for (j=width/2; j; j--) {
			*dest=((int)*src+(int)*src2)/2;
			src+=2;
			src2+=2;
			dest++;
			*dest2=((int)*src+(int)*src2)/2;
			src+=2;
			src2+=2;
			dest2++;
		}
		src+=width*2;
		src2+=width*2;
	}
}

static void rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
	unsigned char *y, *u, *v;
	unsigned char *r, *g, *b;
	int i, loop;

	b=cap_map;
	g=b+1;
	r=g+1;
	y=map;
	u=y+width*height;
	v=u+(width*height)/4;
	memset(u, 0, width*height/4);
	memset(v, 0, width*height/4);

	for(loop=0; loop<height; loop++) {
		for(i=0; i<width; i+=2) {
			*y++=(9796**r+19235**g+3736**b)>>15;
			*u+=((-4784**r-9437**g+14221**b)>>17)+32;
			*v+=((20218**r-16941**g-3277**b)>>17)+32;
			r+=3;
			g+=3;
			b+=3;
			*y++=(9796**r+19235**g+3736**b)>>15;
			*u+=((-4784**r-9437**g+14221**b)>>17)+32;
			*v+=((20218**r-16941**g-3277**b)>>17)+32;
			r+=3;
			g+=3;
			b+=3;
			u++;
			v++;
		}

		if ((loop & 1) == 0)
		{
			u-=width/2;
			v-=width/2;
		}
	}
}

/*******************************************************************************************
	Video4linux capture routines
*/


static unsigned char *v4l_start(struct context *cnt, struct video_dev *viddev, int width, int height,
                                int input, int norm, unsigned long freq, int tuner_number)
{
	int dev = viddev->fd;
	struct video_capability vid_caps;
	struct video_channel vid_chnl;
	struct video_tuner vid_tuner;
	struct video_mbuf vid_buf;
	struct video_mmap vid_mmap;
	void *map;

	if (ioctl (dev, VIDIOCGCAP, &vid_caps) == -1) {
		motion_log(LOG_ERR, 1, "ioctl (VIDIOCGCAP)");
		return (NULL);
	}

	if (vid_caps.type & VID_TYPE_MONOCHROME)
		viddev->v4l_fmt=VIDEO_PALETTE_GREY;

	if (input != IN_DEFAULT) {
		memset(&vid_chnl, 0, sizeof(struct video_channel));
		vid_chnl.channel = input;

		if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
			motion_log(LOG_ERR, 1, "ioctl (VIDIOCGCHAN)");
		} else {
			vid_chnl.channel = input;
			vid_chnl.norm    = norm;
			if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCSCHAN)");
				return (NULL);
			}
		}
	}

	if (freq) {
		vid_tuner.tuner = tuner_number;
		if (ioctl (dev, VIDIOCGTUNER, &vid_tuner)==-1) {
			motion_log(LOG_ERR, 1, "ioctl (VIDIOCGTUNER)");
		} else {
			if (vid_tuner.flags & VIDEO_TUNER_LOW) {
				freq=freq*16; /* steps of 1/16 KHz */
			} else {
				freq=(freq*10)/625;
			}
			if (ioctl(dev, VIDIOCSFREQ, &freq)==-1) {
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCSFREQ)");
				return (NULL);
			}
			if (cnt->conf.setup_mode)
				motion_log(-1, 0, "Frequency set");
		}
	}

	if (ioctl (dev, VIDIOCGMBUF, &vid_buf) == -1) {
		motion_log(LOG_ERR, 0, "ioctl(VIDIOCGMBUF) - Error device does not support memory map");
		motion_log(LOG_ERR, 0, "V4L capturing using read is deprecated!");
		motion_log(LOG_ERR, 0, "Motion only supports mmap.");
		return NULL;
	} else {
		map=mmap(0, vid_buf.size, PROT_READ|PROT_WRITE, MAP_SHARED, dev, 0);
		viddev->size_map=vid_buf.size;
		if (vid_buf.frames>1) {
			viddev->v4l_maxbuffer=2;
			viddev->v4l_buffers[0]=map;
			viddev->v4l_buffers[1]=(unsigned char *)map + vid_buf.offsets[1];
		} else {
			viddev->v4l_buffers[0]=map;
			viddev->v4l_maxbuffer=1;
		}

		if (MAP_FAILED == map) {
			return (NULL);
		}
		viddev->v4l_curbuffer=0;
		vid_mmap.format=viddev->v4l_fmt;
		vid_mmap.frame=viddev->v4l_curbuffer;
		vid_mmap.width=width;
		vid_mmap.height=height;
		if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
			motion_log(LOG_DEBUG, 1, "Failed with YUV420P, trying YUV422 palette");
			viddev->v4l_fmt=VIDEO_PALETTE_YUV422;
			vid_mmap.format=viddev->v4l_fmt;
			/* Try again... */
			if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
				motion_log(LOG_DEBUG, 1, "Failed with YUV422, trying RGB24 palette");
				viddev->v4l_fmt=VIDEO_PALETTE_RGB24;
				vid_mmap.format=viddev->v4l_fmt;
				/* Try again... */
				if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
					motion_log(LOG_DEBUG, 1, "Failed with RGB24, trying GREYSCALE palette");
					viddev->v4l_fmt=VIDEO_PALETTE_GREY;
					vid_mmap.format=viddev->v4l_fmt;
					/* Try one last time... */
					if (ioctl(dev, VIDIOCMCAPTURE, &vid_mmap) == -1) {
						motion_log(LOG_ERR, 1, "Failed with all supported palettes "
						                            "- giving up");
						return (NULL);
					}
				}
			}
		}
	}

	switch (viddev->v4l_fmt) {
		case VIDEO_PALETTE_YUV420P:
			viddev->v4l_bufsize=(width*height*3)/2;
			break;
		case VIDEO_PALETTE_YUV422:
			viddev->v4l_bufsize=(width*height*2);
			break;
		case VIDEO_PALETTE_RGB24:
			viddev->v4l_bufsize=(width*height*3);
			break;
		case VIDEO_PALETTE_GREY:
			viddev->v4l_bufsize=width*height;
			break;
	}
	return map;
}


/**
 * v4l_next
 *                v4l_next fetches a video frame from a v4l device
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
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
static int v4l_next(struct video_dev *viddev, unsigned char *map, int width, int height)
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
		motion_log(LOG_ERR, 1, "mcapture error in proc %d", getpid());
		sigprocmask (SIG_UNBLOCK, &old, NULL);
		return V4L_FATAL_ERROR;
	}

	vid_mmap.frame=frame;

	if (ioctl(dev, VIDIOCSYNC, &vid_mmap.frame) == -1) {
		motion_log(LOG_ERR, 1, "sync error in proc %d", getpid());
		sigprocmask (SIG_UNBLOCK, &old, NULL);
	}

	pthread_sigmask (SIG_UNBLOCK, &old, NULL);        /*undo the signal blocking*/

	switch (viddev->v4l_fmt) {
		case VIDEO_PALETTE_RGB24:
			rgb24toyuv420p(map, cap_map, width, height);
			break;
		case VIDEO_PALETTE_YUV422:
			yuv422to420p(map, cap_map, width, height);
			break;
		default:
			memcpy(map, cap_map, viddev->v4l_bufsize);
	}


	return 0;
}

static void v4l_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height, int input,
                    int norm, int skip, unsigned long freq, int tuner_number)
{
	int dev=viddev->fd;
	int i;
	struct video_channel vid_chnl;
	struct video_tuner vid_tuner;
	unsigned long frequnits = freq;
	
	if (input != viddev->input || width != viddev->width || height!=viddev->height ||
	    freq!=viddev->freq || tuner_number!=viddev->tuner_number) {
		if (freq) {
			vid_tuner.tuner = tuner_number;
			if (ioctl (dev, VIDIOCGTUNER, &vid_tuner)==-1) {
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCGTUNER)");
			} else {
				if (vid_tuner.flags & VIDEO_TUNER_LOW) {
					frequnits=freq*16; /* steps of 1/16 KHz */
				} else {
					frequnits=(freq*10)/625;
				}
				if (ioctl(dev, VIDIOCSFREQ, &frequnits)==-1) {
					motion_log(LOG_ERR, 1, "ioctl (VIDIOCSFREQ)");
					return;
				}
			}
		}

		vid_chnl.channel = input;
		
		if (ioctl (dev, VIDIOCGCHAN, &vid_chnl) == -1) {
			motion_log(LOG_ERR, 1, "ioctl (VIDIOCGCHAN)");
		} else {
			vid_chnl.channel = input;
			vid_chnl.norm = norm;
			if (ioctl (dev, VIDIOCSCHAN, &vid_chnl) == -1) {
				motion_log(LOG_ERR, 1, "ioctl (VIDIOCSCHAN)");
				return;
			}
		}
		v4l_picture_controls(cnt, viddev);
		viddev->input=input;
		viddev->width=width;
		viddev->height=height;
		viddev->freq=freq;
		viddev->tuner_number=tuner_number;
		/* skip a few frames if needed */
		for (i=0; i<skip; i++)
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
		motion_log(LOG_ERR, 1, "Unable to execute uname");
		return -1;
	}
	major = strtok(uts.release, ".");
	minor = strtok(NULL, ".");
	if ((major == NULL) || (minor == NULL) || (strcmp(major, "2"))) {
		motion_log(LOG_ERR, 1, "Unable to decipher OS version");
		return -1;
	}
	if (strcmp(minor, "5") < 0) {
		FILE *vloopbacks;
		char *loop;
		char *input;
		char *istatus;
		char *output;
		char *ostatus;

		vloopbacks=fopen("/proc/video/vloopback/vloopbacks", "r");
		if (!vloopbacks) {
			motion_log(LOG_ERR, 1, "Failed to open '/proc/video/vloopback/vloopbacks'");
			return -1;
		}
		
		/* Read vloopback version*/
		if (!fgets(buffer, 255, vloopbacks)) {
			motion_log(LOG_ERR, 1, "Unable to read vloopback version");
			return -1;
		}
		
		fprintf(stderr,"\t%s", buffer);
		
		/* Read explanation line */
		
		if (!fgets(buffer, 255, vloopbacks)) {
			motion_log(LOG_ERR, 1, "Unable to read vloopback explanation line");
			return -1;
		}
		
		while (fgets(buffer, 255, vloopbacks)) {
			if (strlen(buffer)>1) {
				buffer[strlen(buffer)-1]=0;
				loop=strtok(buffer, "\t");
				input=strtok(NULL, "\t");
				istatus=strtok(NULL, "\t");
				output=strtok(NULL, "\t");
				ostatus=strtok(NULL, "\t");
				if (istatus[0]=='-') {
					sprintf(pipepath, "/dev/%s", input);
					pipe_fd=open(pipepath, O_RDWR);
					if (pipe_fd>=0) {
						motion_log(-1, 0, "\tInput:  /dev/%s", input);
						motion_log(-1, 0, "\tOutput: /dev/%s", output);
						break;
					}
				}
			}
		}
		fclose(vloopbacks);
	} else {
		DIR *dir;
		struct dirent *dirp;
		const char prefix[]="/sys/class/video4linux/";
		char *ptr, *io;
		int fd;
		int low=9999;
		int tfd;
		int tnum;

		if ((dir=opendir(prefix))== NULL) {
			motion_log(LOG_ERR, 1, "Failed to open '%s'", prefix);
			return -1;
		}
		while ((dirp=readdir(dir)) != NULL) {
			if (!strncmp(dirp->d_name, "video", 5)) {
				strcpy(buffer, prefix);
				strcat(buffer, dirp->d_name);
				strcat(buffer, "/name");
				if ((fd=open(buffer, O_RDONLY)) >= 0) {
					if ((read(fd, buffer, sizeof(buffer)-1))<0) {
						close(fd);
						continue;
					}
					ptr = strtok(buffer, " ");
					if (strcmp(ptr,"Video")) {
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
					if ((ptr=strtok(buffer, " "))==NULL) {
						close(fd);
						continue;
					}
					tnum = atoi(minor);
					if (tnum < low) {
						strcpy(buffer, "/dev/");
						strcat(buffer, dirp->d_name);
						if ((tfd=open(buffer, O_RDWR))>=0) {
							strcpy(pipepath, buffer);
							if (pipe_fd>=0) {
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
			motion_log(-1, 0, "Opened input of %s", pipepath);
	}
	return pipe_fd;
}

static int v4l_startpipe(char *devname, int width, int height, int type)
{
	int dev;
	struct video_picture vid_pic;
	struct video_window vid_win;

	if (!strcmp(devname, "-")) {
		dev=v4l_open_vidpipe();
	} else {
		dev=open(devname, O_RDWR);
	}
	if (dev < 0)
		return(-1);

	if (ioctl(dev, VIDIOCGPICT, &vid_pic)== -1) {
		motion_log(LOG_ERR, 1, "ioctl (VIDIOCGPICT)");
		return(-1);
	}
	vid_pic.palette=type;
	if (ioctl(dev, VIDIOCSPICT, &vid_pic)== -1) {
		motion_log(LOG_ERR, 1, "ioctl (VIDIOCSPICT)");
		return(-1);
	}
	if (ioctl(dev, VIDIOCGWIN, &vid_win)== -1) {
		motion_log(LOG_ERR, 1, "ioctl (VIDIOCGWIN)");
		return(-1);
	}
	vid_win.height=height;
	vid_win.width=width;
	if (ioctl(dev, VIDIOCSWIN, &vid_win)== -1) {
		motion_log(LOG_ERR, 1, "ioctl (VIDIOCSWIN)");
		return(-1);
	}
	return dev;
}

static int v4l_putpipe (int dev, unsigned char *image, int size)
{
	return write(dev, image, size);
}
#endif /*WITHOUT_V4L*/

/*****************************************************************************
	Wrappers calling the actual capture routines
 *****************************************************************************/

/* big lock for vid_start to ensure exclusive access to viddevs while adding 
 * devices during initialization of each thread
 */
pthread_mutex_t vid_mutex;

#ifndef WITHOUT_V4L

/* Here we setup the viddevs structure which is used globally in the vid_*
 * functions.
 */
struct video_dev **viddevs=NULL;

/**
 * vid_init
 *
 * Called from motion.c at the very beginning before setting up the threads.
 * Function prepares the viddevs struct for the threads and the vid_mutex
 */
void vid_init(void)
{
	if (!viddevs) {
		viddevs = mymalloc(sizeof(struct video_dev *));
		viddevs[0] = NULL;
	}

	pthread_mutex_init(&vid_mutex, NULL);
}

/**
 * vid_close
 *
 * vid_close is called from motion.c when Motion is stopped or restarted
 * It gets rid of all open video devices. It is called BEFORE vid_cleanup.
 */
void vid_close(void)
{
	int i=-1;

	if (viddevs) {
		while(viddevs[++i]) {
			close(viddevs[i]->fd);
		}
	}
}

/**
 * vid_cleanup
 *
 * vid_cleanup is called from motion.c when Motion is stopped or restarted
 * It free all the memory held by the viddevs structs.
 */
void vid_cleanup(void)
{
	int i=-1;
	if (viddevs) {
		while(viddevs[++i]) {
			munmap(viddevs[i]->v4l_buffers[0],viddevs[i]->size_map);
			free(viddevs[i]);
		}
		free(viddevs);
		viddevs=NULL;
	}
}
#endif /*WITHOUT_V4L*/

/**
 * vid_start
 *
 * vid_start setup the capture device. This will be either a V4L device or a netcam.
 * The function does the following:
 * - If the camera is a netcam - netcam_start is called and function returns
 * - Width and height are checked for valid value (multiple of 16)
 * - Copy the config height and width to the imgs struct. Note that height and width are
 *   only copied to the from the conf struct to the imgs struct during program startup
 *   The width and height can no later be changed via http remote control as this would
 *   require major re-memory allocations of all image buffers.
 * - Setup basic V4L properties incl palette incl setting 
 * - Open the device
 * - Returns the device number.
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *
 * "Global" variable
 *     viddevs    The viddevs struct is "global" within the context of video.c
 *                and used in functions vid_*.
 *     vid_mutex  Mutex needed to handle exclusive access to the viddevs struct when
 *                each thread adds a new video device during startup calling vid_start
 *
 * Returns
 *     device number
 *     -1 if failed to open device.
 */
int vid_start(struct context *cnt)
{
	struct config *conf = &cnt->conf;
	int dev = -1;

	if (conf->netcam_url) {
		return netcam_start(cnt);
	}

#ifndef WITHOUT_V4L
	/* Start a new block so we can make declarations without breaking good old
	 * gcc 2.95 or older.
	 */
	{
		int i = -1;
		int width, height, input, norm, tuner_number;
		unsigned long frequency;

		/* We use width and height from conf in this function. They will be assigned
		 * to width and height in imgs here, and cap_width and cap_height in 
		 * rotate_data won't be set until in rotate_init.
		 * Motion requires that width and height is a multiple of 16 so we check
		 * for this first.
		 */
		if (conf->width % 16) {
			motion_log(LOG_ERR, 0, "config image width (%d) is not modulo 16", conf->width);
			return -1;
		}

		if (conf->height % 16) {
			motion_log(LOG_ERR, 0, "config image height (%d) is not modulo 16", conf->height);
			return -1;
		}

		width = conf->width;
		height = conf->height;
		input = conf->input;
		norm = conf->norm;
		frequency = conf->frequency;
		tuner_number = conf->tuner_number;
		
		pthread_mutex_lock(&vid_mutex);

		/* Transfer width and height from conf to imgs. The imgs values are the ones
		 * that is used internally in Motion. That way, setting width and height via
		 * http remote control won't screw things up.
		 */
		cnt->imgs.width=width;
		cnt->imgs.height=height;

		/* First we walk through the already discovered video devices to see
		 * if we have already setup the same device before. If this is the case
		 * the device is a Round Robin device and we set the basic settings
		 * and return the file descriptor
		 */
		while (viddevs[++i]) {
			if (!strcmp(conf->video_device, viddevs[i]->video_device)) {
				int fd;
				cnt->imgs.type = viddevs[i]->v4l_fmt;
				switch (cnt->imgs.type) {
					case VIDEO_PALETTE_GREY:
						cnt->imgs.motionsize = width * height;
						cnt->imgs.size = width * height;
						break;
					case VIDEO_PALETTE_RGB24:
					case VIDEO_PALETTE_YUV422:
						cnt->imgs.type = VIDEO_PALETTE_YUV420P;
					case VIDEO_PALETTE_YUV420P:
						cnt->imgs.motionsize = width * height;
						cnt->imgs.size = (width*height * 3) / 2;
						break;
				}
				fd=viddevs[i]->fd;
				pthread_mutex_unlock(&vid_mutex);
				return fd;
			}
		}

		viddevs = myrealloc(viddevs, sizeof(struct video_dev *)*(i+2), "vid_start");
		viddevs[i] = mymalloc(sizeof(struct video_dev));
		memset(viddevs[i], 0, sizeof(struct video_dev));
		viddevs[i+1]=NULL;

		pthread_mutexattr_init(&viddevs[i]->attr);
		pthread_mutex_init(&viddevs[i]->mutex, NULL);

		dev = open(conf->video_device, O_RDWR);

		if (dev <0) {
			motion_log(LOG_ERR, 1, "Failed to open video device %s", conf->video_device);
			return -1;
		}

		viddevs[i]->video_device = conf->video_device;
		viddevs[i]->fd = dev;
		viddevs[i]->input = input;
		viddevs[i]->height = height;
		viddevs[i]->width = width;
		viddevs[i]->freq = frequency;
		viddevs[i]->tuner_number = tuner_number;
		
		/* We set brightness, contrast, saturation and hue = 0 so that they only get
		 * set if the config is not zero.
		 */
		viddevs[i]->brightness = 0;
		viddevs[i]->contrast = 0;
		viddevs[i]->saturation = 0;
		viddevs[i]->hue = 0;
		viddevs[i]->owner = -1;
		viddevs[i]->v4l_fmt = VIDEO_PALETTE_YUV420P;

		if (!v4l_start(cnt, viddevs[i], width, height, input, norm, frequency, tuner_number)) {
			pthread_mutex_unlock(&vid_mutex);
			return -1;
		}

		cnt->imgs.type = viddevs[i]->v4l_fmt;

		switch (cnt->imgs.type) {
			case VIDEO_PALETTE_GREY:
				cnt->imgs.size = width*height;
				cnt->imgs.motionsize = width*height;
			break;
			case VIDEO_PALETTE_RGB24:
			case VIDEO_PALETTE_YUV422:
				cnt->imgs.type = VIDEO_PALETTE_YUV420P;
			case VIDEO_PALETTE_YUV420P:
				cnt->imgs.size = (width*height*3)/2;
				cnt->imgs.motionsize = width*height;
			break;
		}

		pthread_mutex_unlock(&vid_mutex);
	}
#endif /*WITHOUT_V4L*/	

	return dev;
}


/**
 * vid_next
 *
 * vid_next fetches a video frame from a either v4l device or netcam
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *     map        Pointer to the buffer in which the function puts the new image
 *
 * Global variable
 *     viddevs    The viddevs struct is "global" within the context of video.c
 *                and used in functions vid_*.
 * Returns
 *     0                        Success
 *    -1                        Fatal V4L error
 *    -2                        Fatal Netcam error
 *    Positive numbers...
 *    with bit 0 set            Non fatal V4L error (not implemented)
 *    with bit 1 set            Non fatal Netcam error
 */
int vid_next(struct context *cnt, unsigned char *map)
{
	struct config *conf=&cnt->conf;
	int ret = -1;		

	if (conf->netcam_url) {
		if (cnt->video_dev == -1)
			return NETCAM_GENERAL_ERROR;

		ret = netcam_next(cnt, map);
		return ret;
	}
#ifndef WITHOUT_V4L

	/* We start a new block so we can make declarations without breaking
	 * gcc 2.95 or older
	 */
	{
		int i = -1;
		int width, height;
		int dev = cnt->video_dev;

		/* NOTE: Since this is a capture, we need to use capture dimensions. */
		width = cnt->rotate_data.cap_width;
		height = cnt->rotate_data.cap_height;
		
		while (viddevs[++i])
			if (viddevs[i]->fd == dev)
				break;

		if (!viddevs[i])
			return V4L_FATAL_ERROR;

		if (viddevs[i]->owner != cnt->threadnr) {
			pthread_mutex_lock(&viddevs[i]->mutex);
			viddevs[i]->owner = cnt->threadnr;
			viddevs[i]->frames = conf->roundrobin_frames;
			cnt->switched = 1;
		}

		v4l_set_input(cnt, viddevs[i], map, width, height, conf->input, conf->norm,
		               conf->roundrobin_skip, conf->frequency, conf->tuner_number);

		ret = v4l_next(viddevs[i], map, width, height);

		if (--viddevs[i]->frames <= 0) {
			viddevs[i]->owner = -1;
			pthread_mutex_unlock(&viddevs[i]->mutex);
		}
	
		if(cnt->rotate_data.degrees > 0) {
			/* rotate the image as specified */
			rotate_map(cnt, map);
		}
	}
#endif /*WITHOUT_V4L*/
	return ret;
}

#ifndef WITHOUT_V4L
int vid_startpipe(const char *devname, int width, int height, int type)
{
	return v4l_startpipe( (char *)devname, width, height, type);
}

int vid_putpipe (int dev, unsigned char *image, int size)
{
	return v4l_putpipe(dev, image, size);
}
#endif /*WITHOUT_V4L*/
