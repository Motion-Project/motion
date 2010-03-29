/*
 *    vloopback_motion.c
 *
 *    Video loopback functions for motion.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Copyright 2008 by Angel Carpintero (ack@telefonica.net)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "vloopback_motion.h"
#ifndef BSD
#include <sys/utsname.h>
#include <dirent.h>

/**
 * v4l_open_vidpipe
 *
 */ 
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
        if (!fgets(buffer, sizeof(buffer), vloopbacks)) {
            motion_log(LOG_ERR, 1, "%s: Unable to read vloopback version", __FUNCTION__);
            return -1;
        }
        
        fprintf(stderr, "\t%s", buffer);
        
        /* Read explanation line */
        
        if (!fgets(buffer, sizeof(buffer), vloopbacks)) {
            motion_log(LOG_ERR, 1, "%s: Unable to read vloopback explanation line", 
                       __FUNCTION__);
            return -1;
        }
        
        while (fgets(buffer, sizeof(buffer), vloopbacks)) {
            if (strlen(buffer) > 1) {
                buffer[strlen(buffer)-1] = 0;
                loop = strtok(buffer, "\t");
                input = strtok(NULL, "\t");
                istatus = strtok(NULL, "\t");
                output = strtok(NULL, "\t");
                ostatus = strtok(NULL, "\t");

                if (istatus[0] == '-') {
                    snprintf(pipepath, sizeof(pipepath), "/dev/%s", input);
                    pipe_fd = open(pipepath, O_RDWR);

                    if (pipe_fd >= 0) {
                        motion_log(0, 0, "%s: \tInput:  /dev/%s \tOutput: /dev/%s",
                                   __FUNCTION__, input, output);
                        break;
                    }
                }
            }
        }

        myfclose(vloopbacks);
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
                strncpy(buffer, prefix, sizeof(buffer));
                strncat(buffer, dirp->d_name, sizeof(buffer) - strlen(buffer));
                strncat(buffer, "/name", sizeof(buffer) - strlen(buffer));

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
                        strncat(buffer, dirp->d_name, sizeof(buffer) - strlen(buffer));
                        if ((tfd = open(buffer, O_RDWR)) >= 0) {
                            strncpy(pipepath, buffer, sizeof(pipepath));

                            if (pipe_fd >= 0) 
                                close(pipe_fd);
                            
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
            motion_log(0, 0, "%s: Opened %s as input", __FUNCTION__, pipepath);
    }

    return pipe_fd;
}

/**
 * v4l_startpipe
 *
 */ 
static int v4l_startpipe(const char *dev_name, int width, int height, int type)
{
    int dev;
    struct video_picture vid_pic;
    struct video_window vid_win;

    if (!strcmp(dev_name, "-")) {
        dev = v4l_open_vidpipe();
    } else {
        dev = open(dev_name, O_RDWR);
        motion_log(0, 0, "%s: Opened %s as input", __FUNCTION__, dev_name);
    }

    if (dev < 0) {
        motion_log(LOG_ERR, 1, "%s: Opening %s as input failed", __FUNCTION__, dev_name);
        return -1;
    }

    if (ioctl(dev, VIDIOCGPICT, &vid_pic) == -1) {
        motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGPICT)", __FUNCTION__);
        return -1;
    }

    vid_pic.palette = type;

    if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1) {
        motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSPICT)", __FUNCTION__);
        return -1;
    }

    if (ioctl(dev, VIDIOCGWIN, &vid_win) == -1) {
        motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCGWIN)", __FUNCTION__);
        return -1;
    }

    vid_win.height = height;
    vid_win.width = width;

    if (ioctl(dev, VIDIOCSWIN, &vid_win) == -1) {
        motion_log(LOG_ERR, 1, "%s: ioctl (VIDIOCSWIN)", __FUNCTION__);
        return -1;
    }

    return dev;
}

/**
 * v4l_putpipe
 *
 */ 
static int v4l_putpipe(int dev, unsigned char *image, int size)
{
    return write(dev, image, size);
}

/**
 * vid_startpipe
 *
 */
int vid_startpipe(const char *dev_name, int width, int height, int type)
{
    return v4l_startpipe(dev_name, width, height, type);
}

/**
 * vid_putpipe
 *
 */
int vid_putpipe (int dev, unsigned char *image, int size)
{
    return v4l_putpipe(dev, image, size);
}
#endif /* !WITHOUT_V4L && !BSD */
