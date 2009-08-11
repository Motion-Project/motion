/*    video_freebsd.c
 *
 *    BSD Video stream functions for motion.
 *    Copyright 2004 by Angel Carpintero (ack@telefonica.net)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

/* for rotation */
#include "rotate.h"     /* already includes motion.h */
#include "video_freebsd.h"

#ifndef WITHOUT_V4L

/* for the v4l stuff: */
#include <sys/mman.h>

/* Hack from xawtv 4.x */

#define VIDEO_NONE           0
#define VIDEO_RGB08          1  /* bt848 dithered */
#define VIDEO_GRAY           2
#define VIDEO_RGB15_LE       3  /* 15 bpp little endian */
#define VIDEO_RGB16_LE       4  /* 16 bpp little endian */
#define VIDEO_RGB15_BE       5  /* 15 bpp big endian */
#define VIDEO_RGB16_BE       6  /* 16 bpp big endian */
#define VIDEO_BGR24          7  /* bgrbgrbgrbgr (LE) */
#define VIDEO_BGR32          8  /* bgr-bgr-bgr- (LE) */
#define VIDEO_RGB24          9  /* rgbrgbrgbrgb (BE) */
#define VIDEO_RGB32         10  /* -rgb-rgb-rgb (BE) */
#define VIDEO_LUT2          11  /* lookup-table 2 byte depth */
#define VIDEO_LUT4          12  /* lookup-table 4 byte depth */
#define VIDEO_YUYV          13  /* 4:2:2 */
#define VIDEO_YUV422P       14  /* YUV 4:2:2 (planar) */
#define VIDEO_YUV420P       15  /* YUV 4:2:0 (planar) */
#define VIDEO_MJPEG         16  /* MJPEG (AVI) */
#define VIDEO_JPEG          17  /* JPEG (JFIF) */
#define VIDEO_UYVY          18  /* 4:2:2 */
#define VIDEO_MPEG          19  /* MPEG1/2 */
#define VIDEO_FMT_COUNT     20

#define array_elem(x) (sizeof(x) / sizeof( (x)[0] ))

static const struct camparam_st {
  int min, max, range, drv_min, drv_range, def;
} CamParams[] = {
  {
    BT848_BRIGHTMIN, BT848_BRIGHTMIN + BT848_BRIGHTRANGE,
    BT848_BRIGHTRANGE, BT848_BRIGHTREGMIN,
    BT848_BRIGHTREGMAX - BT848_BRIGHTREGMIN + 1, BT848_BRIGHTCENTER, },
  {
    BT848_CONTRASTMIN, (BT848_CONTRASTMIN + BT848_CONTRASTRANGE),
    BT848_CONTRASTRANGE, BT848_CONTRASTREGMIN,
    (BT848_CONTRASTREGMAX - BT848_CONTRASTREGMIN + 1),
    BT848_CONTRASTCENTER, },
  {
    BT848_CHROMAMIN, (BT848_CHROMAMIN + BT848_CHROMARANGE), BT848_CHROMARANGE,
    BT848_CHROMAREGMIN, (BT848_CHROMAREGMAX - BT848_CHROMAREGMIN + 1 ),
    BT848_CHROMACENTER, },
};

#define BRIGHT 0
#define CONTR  1
#define CHROMA 2

volatile sig_atomic_t bktr_frame_waiting;

//sigset_t sa_mask;


static void catchsignal(int sig)
{
    bktr_frame_waiting++;
}


/* Not tested yet */
static void yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    unsigned char *src, *dest, *src2, *dest2;
    int i, j;

    /* Create the Y plane */
    src = cap_map;
    dest = map;
    for (i = width * height; i; i--) {
        *dest++ = *src;
        src += 2;
    }
    /* Create U and V planes */
    src = cap_map + 1;
    src2 = cap_map + width * 2 + 1;
    dest = map + width* height;
    dest2 = dest + (width * height) / 4;
    for (i = height / 2; i; i--) {
        for (j = width / 2; j; j--) {
            *dest = ((int)*src + (int)*src2) / 2;
            src += 2;
            src2 += 2;
            dest++;
            *dest2 = ((int)*src + (int)*src2) / 2;
            src += 2;
            src2 += 2;
            dest2++;
        }
        src += width * 2;
        src2 += width * 2;
    }

}

/* FIXME seems no work with METEOR_GEO_RGB24 , check BPP as well ? */

static void rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    unsigned char *y, *u, *v;
    unsigned char *r, *g, *b;
    int i, loop;

    b = cap_map;
    g = b + 1;
    r = g + 1;
    y = map;
    u = y + width * height;
    v = u + (width * height) / 4;
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);

    for (loop = 0; loop < height; loop++) {
        for (i = 0; i < width; i += 2) {
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941**g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            u++;
            v++;
        }

        if ((loop & 1) == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }

}


/*********************************************************************************************
                CAPTURE CARD STUFF
**********************************************************************************************/
/* NOT TESTED YET FIXME */

/*
static int camparam_normalize(int param, int cfg_value, int *ioctl_val) 
{
    int val;

    cfg_value = MIN(CamParams[ param ].max, MAX( CamParams[ param ].min, cfg_value)); 
    val = (cfg_value - CamParams[ param ].min ) /
          (CamParams[ param ].range + 0.01) * CamParams[param].drv_range + CamParams[param].drv_min;
    val = MAX(CamParams[param].min,
          MIN(CamParams[param].drv_min + CamParams[ param ].drv_range-1, val));
    *ioctl_val = val;
    return cfg_value;
}
*/

static int set_hue(int viddev, int new_hue)
{
    signed char ioctlval = new_hue;

    if (ioctl(viddev, METEORSHUE, &ioctlval) < 0) {
                motion_log(LOG_ERR, 1, "%s: METEORSHUE Error setting hue [%d]", __FUNCTION__, new_hue);
                return -1;
        }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);

    return ioctlval;
}

static int get_hue(int viddev , int *hue)
{
    signed char ioctlval;

    if (ioctl(viddev, METEORGHUE, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORGHUE Error getting hue", __FUNCTION__);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);
    
    *hue = ioctlval; 
    return ioctlval;
}

static int set_saturation(int viddev, int new_saturation) 
{
    unsigned char ioctlval= new_saturation;

    if (ioctl(viddev, METEORSCSAT, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORSCSAT Error setting saturation [%d]", 
                   __FUNCTION__, new_saturation);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);

    return ioctlval;
}

static int get_saturation(int viddev , int *saturation)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGCSAT, &ioctlval) < 0) {

        motion_log(LOG_ERR, 1, "%s: METEORGCSAT Error getting saturation", __FUNCTION__);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);
    
    *saturation = ioctlval;
    return ioctlval;
}

static int set_contrast(int viddev, int new_contrast) 
{
    unsigned char ioctlval = new_contrast;

    if (ioctl(viddev, METEORSCONT, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORSCONT Error setting contrast [%d]", 
                   __FUNCTION__, new_contrast);
        return 0;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);

    return ioctlval;
}

static int get_contrast(int viddev, int *contrast)
{
    unsigned char ioctlval;

    if (ioctl (viddev, METEORGCONT, &ioctlval ) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORGCONT Error getting contrast", __FUNCTION__);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
         motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);
    
    *contrast = ioctlval; 
    return ioctlval;
}


static int set_brightness(int viddev, int new_bright)
{
    unsigned char ioctlval = new_bright;

    if (ioctl(viddev, METEORSBRIG, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORSBRIG  brightness [%d]", __FUNCTION__, new_bright);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);
    
    return ioctlval;
}


static int get_brightness(int viddev, int *brightness)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGBRIG, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "%s: METEORGBRIG  getting brightness", __FUNCTION__);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, ioctlval);
    
    *brightness = ioctlval;
    return ioctlval;
}

// Set channel needed ? FIXME 
/*
static int set_channel(struct video_dev *viddev, int new_channel) 
{
    int ioctlval;

    ioctlval = new_channel;
    if (ioctl(viddev->fd_tuner, TVTUNER_SETCHNL, &ioctlval) < 0) {
        motion_log(LOG_ERR, 1, "Error channel %d", ioctlval);
        return -1;
    } else {
        motion_log(LOG_DEBUG, 0, "channel set to %d", ioctlval);
    }

    viddev->channel = new_channel;
 
    return 0;
}
*/

/* set frequency to tuner */

static int set_freq(struct video_dev *viddev, unsigned long freq)
{
    int tuner_fd = viddev->fd_tuner;
    int old_audio;

    motion_log(LOG_DEBUG, 0, "%s: Not implemented", __FUNCTION__);
    
    return 0; 
    
    /* HACK maybe not need it , but seems that is needed to mute before changing frequency */

    if (ioctl(tuner_fd, BT848_GAUDIO, &old_audio) < 0) {
        motion_log(LOG_ERR, 1, "%s: BT848_GAUDIO", __FUNCTION__);
        return -1;
    }
    
    if (ioctl(tuner_fd, TVTUNER_SETFREQ, &freq) < 0) {
        motion_log(LOG_ERR, 1, "%s: Tuning (TVTUNER_SETFREQ) failed , freq [%lu]", __FUNCTION__, freq);
        return -1;
    }

    old_audio &= AUDIO_MUTE;
    if (old_audio) {
        old_audio = AUDIO_MUTE;
        if (ioctl(tuner_fd , BT848_SAUDIO, &old_audio) < 0) {
            motion_log(LOG_ERR, 1, "%s: BT848_SAUDIO %i", __FUNCTION__, old_audio);
            return -1;
        }
    }
    
    return 0;
}

/*
  set the input to capture images , could be tuner (METEOR_INPUT_DEV1)
  or any of others input :  
    RCA/COMPOSITE1 (METEOR_INPUT_DEV0) 
    COMPOSITE2/S-VIDEO (METEOR_INPUT_DEV2)
    S-VIDEO (METEOR_INPUT_DEV3)
    VBI ?! (METEOR_INPUT_DEV_SVIDEO)
*/

static int set_input(struct video_dev *viddev, unsigned short input)
{
    int actport;
    int portdata[] = { METEOR_INPUT_DEV0, METEOR_INPUT_DEV1,
                       METEOR_INPUT_DEV2, METEOR_INPUT_DEV3,
                       METEOR_INPUT_DEV_SVIDEO};

    if (input >= array_elem(portdata)) {
        motion_log(LOG_INFO, 0, "%s: Channel Port %d out of range (0-4)", __FUNCTION__, input);
        return -1;
    }

    actport = portdata[input];
    if (ioctl(viddev->fd_bktr, METEORSINPUT, &actport) < 0) {
        if (input != IN_DEFAULT) {
            motion_log(LOG_INFO, 1, "%s: METEORSINPUT %d invalid - Trying default %d", 
                       __FUNCTION__, input, IN_DEFAULT);
            input = IN_DEFAULT;
            actport = portdata[input];
            if (ioctl(viddev->fd_bktr, METEORSINPUT, &actport) < 0) {
                motion_log(LOG_ERR, 1, "%s: METEORSINPUT %d init", __FUNCTION__, input);
                return -1;
            }
        } else {
            motion_log(LOG_ERR, 1, "%s: METEORSINPUT %d init", __FUNCTION__, input);
            return -1;
        }
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d]", __FUNCTION__, input);
    
    return input;
}

static int set_geometry(struct video_dev *viddev, int width, int height)
{
    struct meteor_geomet geom;
    int h_max;

    geom.columns = width;
    geom.rows = height;

    geom.oformat = METEOR_GEO_YUV_422 | METEOR_GEO_YUV_12;


    switch (viddev->norm) {
    case PAL:   
        h_max = PAL_HEIGHT;  
        break;
    case NTSC:  
        h_max = NTSC_HEIGHT; 
        break;
    case SECAM: 
        h_max = SECAM_HEIGHT;
        break;
    default:    
        h_max = PAL_HEIGHT;
    }

    if (height <= h_max / 2) 
        geom.oformat |= METEOR_GEO_EVEN_ONLY;

    geom.frames = 1;

    if (ioctl(viddev->fd_bktr, METEORSETGEO, &geom) < 0) {
        motion_log(LOG_ERR, 1, "%s: Couldn't set the geometry", __FUNCTION__);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to [%d/%d] Norm %d", __FUNCTION__, width, height, viddev->norm);        
    
    return 0;
}

/*
   set input format ( PAL, NTSC, SECAM, etc ... )
*/

static int set_input_format(struct video_dev *viddev, unsigned short newformat) 
{
    int input_format[] = { NORM_PAL_NEW, NORM_NTSC_NEW, NORM_SECAM_NEW, NORM_DEFAULT_NEW};
    int format;
 
    if (newformat >= array_elem( input_format )) {
        motion_log(LOG_WARNING, 0, "%s: Input format %d out of range (0-2)", __FUNCTION__, newformat);
        return -1;
    } 

    format = input_format[newformat]; 

    if (ioctl( viddev->fd_bktr, BT848SFMT, &format) < 0) {
        motion_log(LOG_ERR, 1, "%s: BT848SFMT, Couldn't set the input format , try again with default",
                   __FUNCTION__);
        format = NORM_DEFAULT_NEW;
        newformat = 3;
        
        if (ioctl(viddev->fd_bktr, BT848SFMT, &format) < 0) {
            motion_log(LOG_ERR, 1, "%s: BT848SFMT, Couldn't set the input format either default", 
                       __FUNCTION__);
            return -1;
        }
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(-1, 0, "%s: to %d", __FUNCTION__, newformat);
        
    return newformat;
}

/*
statict int setup_pixelformat(int bktr)
{
    int i;
    struct meteor_pixfmt p;
    int format=-1;

    for(i = 0; ; i++){
        p.index = i;
        if (ioctl(bktr, METEORGSUPPIXFMT, &p ) < 0) {
            if (errno == EINVAL)
                break;
            motion_log(LOG_ERR, 1, "METEORGSUPPIXFMT getting pixformat %d", i);    
            return -1;
        }


    // Hack from xawtv 4.x 

    switch ( p.type ) {
        case METEOR_PIXTYPE_RGB:
            motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB");
            switch(p.masks[0]) {
            case 31744: // 15 bpp 
                format = p.swap_bytes ? VIDEO_RGB15_LE : VIDEO_RGB15_BE;
                motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB15");
                break;
            case 63488: // 16 bpp 
                format = p.swap_bytes ? VIDEO_RGB16_LE : VIDEO_RGB16_BE;
                motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB16");
                break;
            case 16711680: // 24/32 bpp 
                if (p.Bpp == 3 && p.swap_bytes == 1) {
                    format = VIDEO_BGR24;
                    motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_BGR24");
                } else if (p.Bpp == 4 && p.swap_bytes == 1 && p.swap_shorts == 1) {
                    format = VIDEO_BGR32;
                    motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_BGR32");
                } else if (p.Bpp == 4 && p.swap_bytes == 0 && p.swap_shorts == 0) {
                    format = VIDEO_RGB32;
                    motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB32");
                }
                break;
            case METEOR_PIXTYPE_YUV:
                format = VIDEO_YUV422P;
                motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV");
                break;
            case METEOR_PIXTYPE_YUV_12:
                format = VIDEO_YUV422P;
                motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV_12");
                break;
            case METEOR_PIXTYPE_YUV_PACKED:
                format = VIDEO_YUV422P;
                motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV_PACKED");
                break;
            }

            if (p.type == METEOR_PIXTYPE_RGB && p.Bpp == 3){
                // Found a good pixeltype -- set it up 
                if (ioctl(bktr, METEORSACTPIXFMT, &i) < 0){
                    motion_log(LOG_WARNING, 1, "METEORSACTPIXFMT etting pixformat METEOR_PIXTYPE_RGB Bpp == 3");
                // Not immediately fatal 
                }
                motion_log(LOG_DEBUG, 0, "input format METEOR_PIXTYPE_RGB %i", i);
                format = i;
            }

            if (p.type == METEOR_PIXTYPE_YUV_PACKED) {
                // Found a good pixeltype -- set it up
                if (ioctl(bktr, METEORSACTPIXFMT, &i ) < 0){
                    motion_log(LOG_WARNING, 1, "METEORSACTPIXFMT setting pixformat METEOR_PIXTYPE_YUV_PACKED");
                // Not immediately fatal
                } 
                motion_log(LOG_DEBUG, 0, "input format METEOR_PIXTYPE_YUV_PACKED %i", i);
                format = i;
            }

        }

    return format;
}

*/

static void v4l_picture_controls(struct context *cnt, struct video_dev *viddev)
{
    int dev = viddev->fd_bktr;

    if ((cnt->conf.contrast) && (cnt->conf.contrast != viddev->contrast)) { 
        set_contrast(dev, cnt->conf.contrast);
        viddev->contrast = cnt->conf.contrast;    
    }

    if ((cnt->conf.hue) && (cnt->conf.hue != viddev->hue)) {
        set_hue(dev, cnt->conf.hue);
        viddev->hue = cnt->conf.hue;    
    }

    if ((cnt->conf.brightness) && 
        (cnt->conf.brightness != viddev->brightness)) {
        set_brightness(dev, cnt->conf.brightness);
        viddev->brightness = cnt->conf.brightness; 
    }

    if ((cnt->conf.saturation) && 
        (cnt->conf.saturation != viddev->saturation)) {
        set_saturation(dev, cnt->conf.saturation);
        viddev->saturation = cnt->conf.saturation;
    }
}


/*******************************************************************************************
    Video capture routines

 - set input
 - setup_pixelformat
 - set_geometry

 - set_brightness 
 - set_chroma
 - set_contrast
 - set_channelset
 - set_channel
 - set_capture_mode

*/
static unsigned char *v4l_start(struct video_dev *viddev, int width, int height, 
                                unsigned short input, unsigned short norm, unsigned long freq)
{
    int dev_bktr = viddev->fd_bktr;
    struct sigaction act, old;
    //int dev_tunner = viddev->fd_tuner;
    /* to ensure that all device will be support the capture mode 
      _TODO_ : Autodected the best capture mode .
    */
    int dummy = 1;
//    int pixelformat = BSD_VIDFMT_I420;

    void *map;

    /* if we have choose the tuner is needed to setup the frequency */
    if ((viddev->tuner_device != NULL) && (input == IN_TV)) {
        if (!freq) {
            motion_log(LOG_ERR, 0, "%s: Not valid Frequency [%lu] for Source input [%i]",
                       __FUNCTION__, freq, input);
            return NULL;
        } else if (set_freq(viddev, freq) == -1) {
            motion_log(LOG_ERR, 0, "%s: Frequency [%lu] Source input [%i]", 
                       __FUNCTION__, freq, input);
            return NULL;
        }
    }
    
    /* FIXME if we set as input tuner , we need to set option for tuner not for bktr */

    if ((dummy = set_input(viddev, input)) == -1) {
        motion_log(LOG_ERR, 0, "%s: set input [%d]", __FUNCTION__, input);
        return NULL;
    }

    viddev->input = dummy;

    if ((dummy = set_input_format(viddev, norm)) == -1) {
        motion_log(LOG_ERR, 0, "%s: set input format [%d]", __FUNCTION__, norm);
        return NULL;
    }

    viddev->norm = dummy;

    if (set_geometry(viddev, width, height) == -1) {
        motion_log(LOG_ERR, 0, "%s: set geometry [%d]x[%d]", __FUNCTION__, width, height);
        return NULL;
    }
/*
    if (ioctl(dev_bktr, METEORSACTPIXFMT, &pixelformat) < 0) {
        motion_log(LOG_ERR, 1, "set encoding method BSD_VIDFMT_I420"); 
        return NULL;
    }


    NEEDED !? FIXME 

    if (setup_pixelformat(viddev) == -1)
        return NULL;
*/

    if (freq) {
        if (debug_level >= CAMERA_DEBUG)    
            motion_log(-1, 0, "%s: Frequency set (no implemented yet", __FUNCTION__);
    /*
     TODO missing implementation
        set_channelset(viddev);
        set_channel(viddev);
        if (set_freq (viddev, freq) == -1) {
            return NULL;
        }
    */
    }


    /* set capture mode and capture buffers */

    /* That is the buffer size for capture images ,
     so is dependent of color space of input format / FIXME */

    viddev->v4l_bufsize = (((width * height * 3 / 2)) * sizeof(unsigned char));
    viddev->v4l_fmt = VIDEO_PALETTE_YUV420P;
    

    map = mmap((caddr_t)0, viddev->v4l_bufsize, PROT_READ|PROT_WRITE, MAP_SHARED, dev_bktr, (off_t)0);

    if (map == MAP_FAILED){
        motion_log(LOG_ERR, 1, "%s: mmap failed", __FUNCTION__);
        return NULL;
    }

    /* FIXME double buffer */ 
    if (0) {
        viddev->v4l_maxbuffer = 2;
        viddev->v4l_buffers[0] = map;
        viddev->v4l_buffers[1] = (unsigned char *)map + 0; /* 0 is not valid just a test */
        //viddev->v4l_buffers[1] = map+vid_buf.offsets[1];
    } else {
        viddev->v4l_buffers[0] = map;
        viddev->v4l_maxbuffer = 1;
    }

    viddev->v4l_curbuffer = 0;

    /* Clear the buffer */

    if (ioctl(dev_bktr, BT848SCBUF, &dummy) < 0) {
        motion_log(LOG_ERR, 1, "%s: BT848SCBUF", __FUNCTION__);
        return NULL;
    }

    /* signal handler to know when data is ready to be read() */
         
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = catchsignal;
    sigaction(SIGUSR2, &act, &old);
     
    dummy = SIGUSR2;

    //viddev->capture_method = METEOR_CAP_CONTINOUS;
    //viddev->capture_method = METEOR_CAP_SINGLE;
    
    if ((viddev->capture_method == METEOR_CAP_CONTINOUS) && (ioctl(dev_bktr, METEORSSIGNAL, &dummy) < 0)) {
        motion_log(LOG_ERR, 1, "%s: METEORSSIGNAL", __FUNCTION__);
        motion_log(LOG_INFO, 0 , "%s: METEORSSIGNAL", __FUNCTION__);
        viddev->capture_method = METEOR_CAP_SINGLE;
        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
            motion_log(LOG_ERR, 1, "%s: METEORCAPTUR using single method "
                       "Error capturing", __FUNCTION__);
            motion_log(LOG_INFO, 0, "%s: METEORCAPTUR using single method "
                       "Error capturing", __FUNCTION__);
        }    
    } else {
        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
            viddev->capture_method = METEOR_CAP_SINGLE;
            if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
                motion_log(LOG_ERR, 1, "%s: METEORCAPTUR using single method "
                           "Error capturing", __FUNCTION__);
                motion_log(LOG_INFO, 0, "%s: METEORCAPTUR using single method "
                           "Error capturing", __FUNCTION__);
            }    
        }    
    }            
        
    if (viddev->capture_method == METEOR_CAP_CONTINOUS)
        motion_log(LOG_INFO, 0, "%s: METEORCAPTUR METEOR_CAP_CONTINOUS", __FUNCTION__);    
    else        
        motion_log(LOG_INFO, 0, "%s: METEORCAPTUR METEOR_CAP_SINGLE", __FUNCTION__);
    
    // settle , sleep(1) replaced
    SLEEP(1, 0);

    /* FIXME*/
    switch (viddev->v4l_fmt) {
    case VIDEO_PALETTE_YUV420P:
        viddev->v4l_bufsize = (width * height * 3) / 2;
        break;
    case VIDEO_PALETTE_YUV422:
        viddev->v4l_bufsize = (width * height * 2);
        break;
    case VIDEO_PALETTE_RGB24:
        viddev->v4l_bufsize = (width * height * 3);
        break;
    case VIDEO_PALETTE_GREY:
        viddev->v4l_bufsize = width * height;
        break;
    }
    
    motion_log(LOG_INFO, 0, "HUE [%d]", get_hue(dev_bktr, &dummy));
    motion_log(LOG_INFO, 0, "SATURATION [%d]", get_saturation(dev_bktr, &dummy));
    motion_log(LOG_INFO, 0, "BRIGHTNESS [%d]", get_brightness(dev_bktr, &dummy));
    motion_log(LOG_INFO, 0, "CONTRAST [%d]", get_contrast(dev_bktr, &dummy));
    
    return map;
}


/**
 * v4l_next fetches a video frame from a v4l device
 * Parameters:
 *     viddev     Pointer to struct containing video device handle
 *     map        Pointer to the buffer in which the function puts the new image
 *     width      Width of image in pixels
 *     height     Height of image in pixels
 *
 * Returns
 *     0          Success
 *    -1          Fatal error
 *     1          Non fatal error (not implemented)
 */
static int v4l_next(struct video_dev *viddev, unsigned char *map, int width, int height)
{
    int dev_bktr = viddev->fd_bktr;
    unsigned char *cap_map = NULL;
    int single = METEOR_CAP_SINGLE;
    sigset_t set, old;


    /* ONLY MMAP method is used to Capture */

    /* Allocate a new mmap buffer */
    /* Block signals during IOCTL */
    sigemptyset (&set);
    sigaddset (&set, SIGCHLD);
    sigaddset (&set, SIGALRM);
    sigaddset (&set, SIGUSR1);
    sigaddset (&set, SIGTERM);
    sigaddset (&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, &old);
    cap_map = viddev->v4l_buffers[viddev->v4l_curbuffer];

    viddev->v4l_curbuffer++;
    if (viddev->v4l_curbuffer >= viddev->v4l_maxbuffer)
        viddev->v4l_curbuffer = 0;

    /* capture */
    
    if (viddev->capture_method == METEOR_CAP_CONTINOUS) {
        if (bktr_frame_waiting) 
            bktr_frame_waiting = 0;    
            
    } else if (ioctl(dev_bktr, METEORCAPTUR, &single) < 0) {
        motion_log(LOG_ERR, 1, "%s: Error capturing using single method", __FUNCTION__);
        sigprocmask(SIG_UNBLOCK, &old, NULL);
        return -1;
    }

    /*undo the signal blocking*/
    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
    
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


/* set input & freq if needed FIXME not allowed use Tuner yet */

static void v4l_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height,
                          unsigned short input, unsigned short norm, int skip, unsigned long freq)
{

    if (input != viddev->input || norm != viddev->norm || freq != viddev->freq) {
        int dummy;
        unsigned long frequnits = freq;

        
        if ((dummy = set_input(viddev, input)) == -1)
            return;

        viddev->input = dummy;
        
        if ((dummy = set_input_format(viddev, norm)) == -1)
            return;
        
        viddev->norm = dummy;
        
        if ((viddev->tuner_device != NULL) && (viddev->input == IN_TV) && (frequnits > 0)) {
            if (set_freq(viddev, freq) == -1)
                return;
        }

        // FIXME
        /*
        if (setup_pixelformat(viddev) == -1) {
            motion_log(LOG_ERR, 1, "ioctl (VIDIOCSFREQ)");
            return
        }
        */

        /*
        if (set_geometry(viddev, width, height) == -1)
            return;
        */     
            
        v4l_picture_controls(cnt, viddev);

        viddev->freq = freq;

        /* skip a few frames if needed */
        for (dummy = 0; dummy < skip; dummy++)
            v4l_next(viddev, map, width, height);

    } else {
        /* No round robin - we only adjust picture controls */
        v4l_picture_controls(cnt, viddev);
    }
}

/*****************************************************************************
    Wrappers calling the current capture routines
 *****************************************************************************/

/*

vid_init - Initi vid_mutex.
vid_start - Setup Device parameters ( device , channel , freq , contrast , hue , saturation , brightness ) and open it.
vid_next - Capture a frame and set input , contrast , hue , saturation and brightness if necessary. 
vid_close - close devices. 
vid_cleanup - Destroy vid_mutex.

*/

/* big lock for vid_start to ensure exclusive access to viddevs while adding
 * devices during initialization of each thread
 */
static pthread_mutex_t vid_mutex;


/* Here we setup the viddevs structure which is used globally in the vid_*
 * functions.
 */      
static struct video_dev *viddevs = NULL;


/*
 * vid_init
 *
 * Called from motion.c at the very beginning before setting up the threads.
 * Function prepares the vid_mutex
 */
void vid_init(void)
{
    pthread_mutex_init(&vid_mutex, NULL);
}
                
/**
 * vid_cleanup
 *
 * vid_cleanup is called from motion.c when Motion is stopped or restarted
 */
void vid_cleanup(void)
{
    pthread_mutex_destroy(&vid_mutex);
}

#endif /*WITHOUT_V4L*/


/**
 * vid_close
 *
 * vid_close is called from motion.c when a Motion thread is stopped or restarted
 */
void vid_close(struct context *cnt)   
{
#ifndef WITHOUT_V4L
    struct video_dev *dev = viddevs;
    struct video_dev *prev = NULL;
#endif

    /* Cleanup the netcam part */
    if (cnt->netcam) {
        netcam_cleanup(cnt->netcam, 0);
        cnt->netcam = NULL;
        return;
    } 

#ifndef WITHOUT_V4L

    /* Cleanup the v4l part */
    pthread_mutex_lock(&vid_mutex);
    while (dev) {
        if (dev->fd_bktr == cnt->video_dev)
            break;
            prev = dev;
            dev = dev->next;
        }
        pthread_mutex_unlock(&vid_mutex);
 
        /* Set it as closed in thread context */
        cnt->video_dev = -1;
 
        if (dev == NULL) {
            motion_log(LOG_ERR, 0, "%s: Unable to find video device", __FUNCTION__);   
            return;
        }

        if (--dev->usage_count == 0) {
            motion_log(LOG_INFO, 0, "%s: Closing video device %s", 
                       __FUNCTION__, dev->video_device);

            if (dev->fd_tuner > 0)
                close(dev->fd_tuner);
    
            if (dev->fd_bktr > 0) { 
                if (dev->capture_method == METEOR_CAP_CONTINOUS) {
                    dev->fd_tuner = METEOR_CAP_STOP_CONT;
                    ioctl(dev->fd_bktr, METEORCAPTUR, &dev->fd_tuner);
                }
                close(dev->fd_bktr);
                dev->fd_tuner = -1;
            }


            munmap(viddevs->v4l_buffers[0], viddevs->v4l_bufsize);
            viddevs->v4l_buffers[0] = MAP_FAILED;

            dev->fd_bktr = -1;
            pthread_mutex_lock(&vid_mutex);
            
            /* Remove from list */
            if (prev == NULL)
                viddevs = dev->next;
            else
                prev->next = dev->next;
             
            pthread_mutex_unlock(&vid_mutex);

            pthread_mutexattr_destroy(&dev->attr);
            pthread_mutex_destroy(&dev->mutex);
            free(dev);
        } else {
            motion_log(LOG_INFO, 0, "%s: Still %d users of video device %s, so we don't close it now", 
                       __FUNCTION__, dev->usage_count, dev->video_device);
            /* There is still at least one thread using this device
             * If we own it, release it
             */
            if (dev->owner == cnt->threadnr) {
                dev->frames = 0;
                dev->owner = -1;
                pthread_mutex_unlock(&dev->mutex);
            }
        }
#endif /* WITHOUT_V4L */
}



int vid_start(struct context *cnt)
{
    struct config *conf = &cnt->conf;
    int fd_bktr = -1;

    if (conf->netcam_url) {
        fd_bktr = netcam_start(cnt);
        if (fd_bktr < 0) {
            netcam_cleanup(cnt->netcam, 1);
            cnt->netcam = NULL;
        }
    }    
#ifdef WITHOUT_V4L
    else 
        motion_log(LOG_ERR, 0, "%s: You must setup netcam_url", __FUNCTION__);    
#else
    else {
        struct video_dev *dev;
        int fd_tuner = -1;
        int width, height, capture_method;
        unsigned short input, norm;
        unsigned long frequency;


        motion_log(-1, 0, "%s: [%s]", __FUNCTION__, conf->video_device);

        /* We use width and height from conf in this function. They will be assigned
         * to width and height in imgs here, and cap_width and cap_height in 
         * rotate_data won't be set until in rotate_init.
         * Motion requires that width and height are multiples of 16 so we check for this
         */
        if (conf->width % 16) {
            motion_log(LOG_ERR, 0,
                       "%s: config image width (%d) is not modulo 16",
                       __FUNCTION__, conf->width);
            return -1;
        }
        if (conf->height % 16) {
            motion_log(LOG_ERR, 0,
                       "%s: config image height (%d) is not modulo 16",
                       __FUNCTION__, conf->height);
            return -1;
        }

        width = conf->width;
        height = conf->height;
        input = conf->input;
        norm = conf->norm;
        frequency = conf->frequency;
        capture_method = METEOR_CAP_CONTINOUS;
        
        pthread_mutex_lock(&vid_mutex);

        /* Transfer width and height from conf to imgs. The imgs values are the ones
         * that is used internally in Motion. That way, setting width and height via
         * http remote control won't screw things up.
         */
        cnt->imgs.width = width;
        cnt->imgs.height = height;

        /* First we walk through the already discovered video devices to see
         * if we have already setup the same device before. If this is the case
         * the device is a Round Robin device and we set the basic settings
         * and return the file descriptor
         */
        dev = viddevs;
        while (dev) { 
            if (!strcmp(conf->video_device, dev->video_device)) {
                int dummy = METEOR_CAP_STOP_CONT;
                dev->usage_count++;
                cnt->imgs.type = dev->v4l_fmt;

                if (ioctl(dev->fd_bktr, METEORCAPTUR, &dummy) < 0) {
                    motion_log(LOG_ERR, 1, "%s Stopping capture", __FUNCTION__);
                    return -1;    
                }    
                
                motion_log(-1, 0, "%s Reusing [%s] inputs [%d,%d] Change capture method "
                           "METEOR_CAP_SINGLE",  __FUNCTION__, dev->video_device, 
                           dev->input, conf->input);

                dev->capture_method = METEOR_CAP_SINGLE;
                
                switch (cnt->imgs.type) {
                case VIDEO_PALETTE_GREY:
                    cnt->imgs.motionsize = width * height;
                    cnt->imgs.size = width * height;
                    break;
                case VIDEO_PALETTE_RGB24:
                case VIDEO_PALETTE_YUV422:
                    cnt->imgs.type = VIDEO_PALETTE_YUV420P;
                case VIDEO_PALETTE_YUV420P:
                    motion_log(-1, 0,
                               "%s VIDEO_PALETTE_YUV420P setting imgs.size "
                               "and imgs.motionsize", __FUNCTION__);
                    cnt->imgs.motionsize = width * height;
                    cnt->imgs.size = (width * height * 3) / 2;
                    break;
                }
                pthread_mutex_unlock(&vid_mutex);
                return dev->fd_bktr; // FIXME return fd_tuner ?!
            }
            dev = dev->next;
        }


        dev = mymalloc(sizeof(struct video_dev));
        memset(dev, 0, sizeof(struct video_dev));

        fd_bktr = open(conf->video_device, O_RDWR);

        if (fd_bktr < 0) { 
            motion_log(LOG_ERR, 1, "%s: open video device %s", __FUNCTION__, conf->video_device);
            free(dev);
            pthread_mutex_unlock(&vid_mutex);
            return -1;
        }


        /* Only open tuner if conf->tuner_device has set , freq and input is 1 */
        if ((conf->tuner_device != NULL) && (frequency > 0) && (input == IN_TV)) {
            fd_tuner = open(conf->tuner_device, O_RDWR);
            if (fd_tuner < 0) { 
                motion_log(LOG_ERR, 1, "%s: open tuner device %s", 
                           __FUNCTION__, conf->tuner_device);
                free(dev);
                pthread_mutex_unlock(&vid_mutex);
                return -1;
            }
        }

        pthread_mutexattr_init(&dev->attr);
        pthread_mutex_init(&dev->mutex, &dev->attr);

        dev->usage_count = 1;
        dev->video_device = conf->video_device;
        dev->tuner_device = conf->tuner_device;
        dev->fd_bktr = fd_bktr;
        dev->fd_tuner = fd_tuner;
        dev->input = input;
        dev->height = height;
        dev->width = width;
        dev->freq = frequency;
        dev->owner = -1;
        dev->capture_method = capture_method;
        
        /* We set brightness, contrast, saturation and hue = 0 so that they only get
         * set if the config is not zero.
         */
        
        dev->brightness = 0;
        dev->contrast = 0;
        dev->saturation = 0;
        dev->hue = 0;
        dev->owner = -1;

        /* default palette */ 
        dev->v4l_fmt = VIDEO_PALETTE_YUV420P;
        dev->v4l_curbuffer = 0;
        dev->v4l_maxbuffer = 1;

        if (!v4l_start(dev, width, height, input, norm, frequency)) { 
            close(dev->fd_bktr);
            pthread_mutexattr_destroy(&dev->attr);
            pthread_mutex_destroy(&dev->mutex);
            free(dev);

            pthread_mutex_unlock(&vid_mutex);
            return -1;
        }
    
        cnt->imgs.type = dev->v4l_fmt;
    
        switch (cnt->imgs.type) { 
        case VIDEO_PALETTE_GREY:
            cnt->imgs.size = width * height;
            cnt->imgs.motionsize = width * height;
            break;
        case VIDEO_PALETTE_RGB24:
        case VIDEO_PALETTE_YUV422:
            cnt->imgs.type = VIDEO_PALETTE_YUV420P;
        case VIDEO_PALETTE_YUV420P:
            motion_log(-1, 0, "%s: VIDEO_PALETTE_YUV420P imgs.type", __FUNCTION__);
            cnt->imgs.size = (width * height * 3) / 2;
            cnt->imgs.motionsize = width * height;
        break;
        }

        /* Insert into linked list */
        dev->next = viddevs;
        viddevs = dev;
    
        pthread_mutex_unlock(&vid_mutex);
    }
#endif /* WITHOUT_V4L */

    /* FIXME needed tuner device ?! */
    return fd_bktr;
}


/**
 * vid_next fetches a video frame from a either v4l device or netcam
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *     map        Pointer to the buffer in which the function puts the new image
 *
 * Returns
 *     0          Success
 *    -1          Fatal V4L error
 *    -2          Fatal Netcam error
 *     1          Non fatal V4L error (not implemented)
 *     2          Non fatal Netcam error
 */
int vid_next(struct context *cnt, unsigned char *map)
{
    struct config *conf = &cnt->conf;
    int ret = -1;

    if (conf->netcam_url) {
        if (cnt->video_dev == -1)
            return NETCAM_GENERAL_ERROR;

        ret = netcam_next(cnt, map);
        return ret;
    }

#ifndef WITHOUT_V4L

    struct video_dev *dev;    
    int width, height;
    int dev_bktr = cnt->video_dev;

    /* NOTE: Since this is a capture, we need to use capture dimensions. */
    width = cnt->rotate_data.cap_width;
    height = cnt->rotate_data.cap_height;
    
    pthread_mutex_lock(&vid_mutex);
    dev = viddevs;    
    while (dev) {
        if (dev->fd_bktr == dev_bktr)
            break;
        dev = dev->next;
    }
    
    pthread_mutex_unlock(&vid_mutex);

    if (dev == NULL)
        return V4L_FATAL_ERROR;
        //return -1;

    if (dev->owner != cnt->threadnr) { 
        pthread_mutex_lock(&dev->mutex);
        dev->owner = cnt->threadnr;
        dev->frames = conf->roundrobin_frames;
    }


    v4l_set_input(cnt, dev, map, width, height, conf->input, conf->norm,
                  conf->roundrobin_skip, conf->frequency);
    
    ret = v4l_next(dev, map, width, height);

    if (--dev->frames <= 0) { 
        dev->owner = -1;
        dev->frames = 0;
        pthread_mutex_unlock(&dev->mutex);
    }
    
    /* rotate the image as specified */
    if (cnt->rotate_data.degrees > 0)  
        rotate_map(cnt, map);
         
#endif /*WITHOUT_V4L*/
    return ret;
}
