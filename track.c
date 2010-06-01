/*    track.c
 *
 *    Experimental motion tracking.
 *
 *    Copyright 2000, Jeroen Vreeken
 *    This program is published under the GNU Public license
 */

#include <math.h>
#include <termios.h>
#include "motion.h"

#ifndef WITHOUT_V4L
#include "pwc-ioctl.h"
#endif


struct trackoptions track_template = {
    dev:            -1,             /* dev open */
    port:           NULL,           /* char *port */
    motorx:         0,              /* int motorx */
    motory:         0,              /* int motory */
    maxx:           0,              /* int maxx; */
    maxy:           0,              /* int maxy; */
    speed:          TRACK_SPEED,    /* speed */
    stepsize:       TRACK_STEPSIZE, /* stepsize */
    active:         0,              /* auto tracking active */
    minmaxfound:    0,              /* flag for minmax values stored for pwc based camera */
    step_angle_x:   10,             /* step angle in degrees X-axis that camera moves during auto tracking */
    step_angle_y:   10,             /* step angle in degrees Y-axis that camera moves during auto tracking */
    move_wait:      10              /* number of frames to disable motion detection after camera moving */
};


/* Add your own center and move functions here: */
static unsigned short int stepper_center(struct context *, int xoff, int yoff ATTRIBUTE_UNUSED);
static unsigned short int stepper_move(struct context *, struct coord *, struct images *);
static unsigned short int iomojo_center(struct context *, int xoff, int yoff);
static unsigned short int iomojo_move(struct context *, int dev, struct coord *, struct images *);
#ifndef WITHOUT_V4L
static unsigned short int lqos_center(struct context *, int dev, int xoff, int yoff);
static unsigned short int lqos_move(struct context *, int dev, struct coord *, struct images *, 
                                    unsigned short int);
#ifdef MOTION_V4L2
static unsigned short int uvc_center(struct context *, int dev, int xoff, int yoff);
static unsigned short int uvc_move(struct context *, int dev, struct coord *, struct images *, 
                                   unsigned short int);
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */

/* Add a call to your functions here: */
unsigned short int track_center(struct context *cnt, int dev ATTRIBUTE_UNUSED, 
                                unsigned short int manual, int xoff, int yoff)
{
    if (!manual && !cnt->track.active)
        return 0;

    if (cnt->track.type == TRACK_TYPE_STEPPER) {
        unsigned short int ret;
        ret = stepper_center(cnt, xoff, yoff);
        if (!ret) {
                motion_log(LOG_ERR, 1, "track_center: internal error (stepper_center)");
                return 0;        
        } else {
            return ret;    
        }    
    }
#ifndef WITHOUT_V4L    
    else if (cnt->track.type == TRACK_TYPE_PWC)
        return lqos_center(cnt, dev, xoff, yoff);
#ifdef MOTION_V4L2
    else if (cnt->track.type == TRACK_TYPE_UVC)
        return uvc_center(cnt, dev, xoff, yoff);
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */
    else if (cnt->track.type == TRACK_TYPE_IOMOJO)
        return iomojo_center(cnt, xoff, yoff);
    else if (cnt->track.type == TRACK_TYPE_GENERIC)
        return 10; // FIX ME. I chose to return something reasonable.

    motion_log(LOG_ERR, 1, "track_center: internal error, %hu is not a known track-type", 
               cnt->track.type);

    return 0;
}

/* Add a call to your functions here: */
unsigned short int track_move(struct context *cnt, int dev, struct coord *cent, 
                              struct images *imgs, unsigned short int manual)
{
    if (!manual && !cnt->track.active)
        return 0;

    if (cnt->track.type == TRACK_TYPE_STEPPER)
        return stepper_move(cnt, cent, imgs);
#ifndef WITHOUT_V4L
    else if (cnt->track.type == TRACK_TYPE_PWC)
        return lqos_move(cnt, dev, cent, imgs, manual);
#ifdef MOTION_V4L2
    else if (cnt->track.type == TRACK_TYPE_UVC)
        return uvc_move(cnt, dev, cent, imgs, manual);
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */
    else if (cnt->track.type == TRACK_TYPE_IOMOJO)
        return iomojo_move(cnt, dev, cent, imgs);
    else if (cnt->track.type == TRACK_TYPE_GENERIC)
        return cnt->track.move_wait; // FIX ME. I chose to return something reasonable.

    motion_log(LOG_ERR, 1, "track_move: internal error, %hu is not a known track-type", 
               cnt->track.type);

    return 0;
}


/******************************************************************************
    Stepper motor on serial port
    http://www.lavrsen.dk/foswiki/bin/view/Motion/MotionTracking
    http://www.lavrsen.dk/foswiki/bin/view/Motion/MotionTrackerAPI
******************************************************************************/


static unsigned short int stepper_command(struct context *cnt, unsigned short int motor, 
                                          unsigned short int command, unsigned short int data)
{
    char buffer[3];
    time_t timeout = time(NULL);

    buffer[0] = motor;
    buffer[1] = command;
    buffer[2] = data;

    if (write(cnt->track.dev, buffer, 3) != 3) {
        motion_log(LOG_ERR, 1, "stepper_command port %s dev fd %i, motor %hu command %hu data %hu",
                               cnt->track.port, cnt->track.dev, motor, command, data);
        return 0;
    }

    while (read(cnt->track.dev, buffer, 1) != 1 && time(NULL) < timeout + 1);

    if (time(NULL) >= timeout + 2) {
        motion_log(LOG_ERR, 1, "Status byte timeout!");
        return 0;
    }

    return buffer[0];
}


static unsigned short int stepper_status(struct context *cnt,  unsigned short int motor)
{
    return stepper_command(cnt, motor, STEPPER_COMMAND_STATUS, 0);
}


static unsigned short int stepper_center(struct context *cnt, int x_offset, int y_offset)
{
    struct termios adtio;

    if (cnt->track.dev < 0) {
        motion_log(LOG_INFO, 0, "Try to open serial device %s", cnt->track.port);
        
        if ((cnt->track.dev=open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
            motion_log(LOG_ERR, 1, "Unable to open serial device %s", cnt->track.port);
            return 0;
        }

        bzero (&adtio, sizeof(adtio));
        adtio.c_cflag = STEPPER_BAUDRATE | CS8 | CLOCAL | CREAD;
        adtio.c_iflag = IGNPAR;
        adtio.c_oflag = 0;
        adtio.c_lflag = 0;    /* non-canon, no echo */
        adtio.c_cc[VTIME] = 0;    /* timer unused */
        adtio.c_cc[VMIN] = 0;    /* blocking read until 1 char */
        tcflush (cnt->track.dev, TCIFLUSH);

        if (tcsetattr(cnt->track.dev, TCSANOW, &adtio) < 0) {
            motion_log(LOG_ERR, 1, "Unable to initialize serial device %s", cnt->track.port);
            return 0;
        }
        motion_log(LOG_INFO, 0, "Opened serial device %s and initialize, fd %i", 
                   cnt->track.port, cnt->track.dev);
    }

    /* x-axis */
    
    stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_SPEED, cnt->track.speed);
    stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_LEFT_N, cnt->track.maxx);

    while (stepper_status(cnt, cnt->track.motorx) & STEPPER_STATUS_LEFT);

    stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_RIGHT_N,
                    cnt->track.maxx / 2 + x_offset * cnt->track.stepsize);

    while (stepper_status(cnt, cnt->track.motorx) & STEPPER_STATUS_RIGHT);

    /* y-axis */

    stepper_command(cnt, cnt->track.motory, STEPPER_COMMAND_SPEED, cnt->track.speed);
    stepper_command(cnt, cnt->track.motory, STEPPER_COMMAND_UP_N, cnt->track.maxy);

    while (stepper_status(cnt, cnt->track.motory) & STEPPER_STATUS_UP)
    
    stepper_command(cnt, cnt->track.motory, STEPPER_COMMAND_DOWN_N,
                    cnt->track.maxy / 2 + y_offset * cnt->track.stepsize);
        
    while (stepper_status(cnt, cnt->track.motory) & STEPPER_STATUS_DOWN);
    
    return cnt->track.move_wait;
}

static unsigned short int stepper_move(struct context *cnt, struct coord *cent, 
                                       struct images *imgs)
{
    unsigned short int command = 0, data = 0;

    if (cnt->track.dev < 0) {
        motion_log(LOG_INFO, 0, "No device %s started yet , trying stepper_center()", cnt->track.port);    
        if (!stepper_center(cnt, 0, 0)){
            motion_log(LOG_ERR, 1, "Stepper_center() failed to initialize stepper device on %s , fd [%i].", 
                                    cnt->track.port, cnt->track.dev);    
            return 0;
        }
        motion_log(LOG_INFO, 0, "stepper_center() succeed , device started %s , fd [%i]", 
                   cnt->track.port, cnt->track.dev);    
    }

    /* x-axis */
    
    if (cent->x < imgs->width / 2) {
        command = STEPPER_COMMAND_LEFT_N;
        data = imgs->width / 2 - cent->x;
    }

    if (cent->x > imgs->width / 2) {
        command = STEPPER_COMMAND_RIGHT_N;
        data = cent->x - imgs->width / 2;
    }

    data = data * cnt->track.stepsize / imgs->width;

    if (data) 
        stepper_command(cnt, cnt->track.motorx, command, data);

    /* y-axis */

    if (cent->y < imgs->height / 2) {
        command = STEPPER_COMMAND_UP_N;
        data = imgs->height / 2 - cent->y;
    }

    if (cent->y > imgs->height / 2) {
        command = STEPPER_COMMAND_DOWN_N;
        data = cent->y - imgs->height / 2;
    }
    
    data = data * cnt->track.stepsize / imgs->height;

    if (data) 
        stepper_command(cnt, cnt->track.motory, command, data);    
    
    
    return cnt->track.move_wait;
}

/******************************************************************************

    Iomojo Smilecam on serial port

******************************************************************************/

static char iomojo_command(struct context *cnt, char *command, 
                           unsigned short int len, unsigned short int ret)
{
    char buffer[1];
    time_t timeout = time(NULL);

    if (write(cnt->track.dev, command, len) != len)
        return 0;

    if (ret) {
        while (read(cnt->track.dev, buffer, 1) != 1 && time(NULL) < timeout + 2);
        
        if (time(NULL) >= timeout + 2) {
            motion_log(LOG_ERR, 1, "Return byte timeout!");
            return 0;
        }
    }
    /* range values ? */
    return buffer[0];
}

static void iomojo_setspeed(struct context *cnt, unsigned short int speed)
{
    char command[3];
    
    command[0] = IOMOJO_SETSPEED_CMD;
    command[1] = cnt->track.iomojo_id;
    command[2] = speed;
    
    if (iomojo_command(cnt, command, 3, 1) != IOMOJO_SETSPEED_RET)
        motion_log(LOG_ERR, 1, "Unable to set camera speed");
}

static void iomojo_movehome(struct context *cnt)
{
    char command[2];
    
    command[0] = IOMOJO_MOVEHOME;
    command[1] = cnt->track.iomojo_id;

    iomojo_command(cnt, command, 2, 0);
}

static unsigned short int iomojo_center(struct context *cnt, int x_offset, int y_offset)
{
    struct termios adtio;
    char command[5], direction = 0;

    if (cnt->track.dev<0) {
        if ((cnt->track.dev=open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
            motion_log(LOG_ERR, 1, "Unable to open serial device %s", cnt->track.port);
            return 0;
        }

        bzero (&adtio, sizeof(adtio));
        adtio.c_cflag = IOMOJO_BAUDRATE | CS8 | CLOCAL | CREAD;
        adtio.c_iflag = IGNPAR;
        adtio.c_oflag = 0;
        adtio.c_lflag = 0;      /* non-canon, no echo */
        adtio.c_cc[VTIME] = 0;  /* timer unused */
        adtio.c_cc[VMIN] = 0;   /* blocking read until 1 char */
        tcflush(cnt->track.dev, TCIFLUSH);
        if (tcsetattr(cnt->track.dev, TCSANOW, &adtio) < 0) {
            motion_log(LOG_ERR, 1, "Unable to initialize serial device %s", cnt->track.port);
            return 0;
        }
    }

    iomojo_setspeed(cnt, 40);
    iomojo_movehome(cnt);

    if (x_offset || y_offset) {
        if (x_offset > 0)
            direction |= IOMOJO_DIRECTION_RIGHT;
        else {
            direction |= IOMOJO_DIRECTION_LEFT;
            x_offset *= -1;
        }

        if (y_offset > 0)
            direction |= IOMOJO_DIRECTION_UP;
        else {
            direction |= IOMOJO_DIRECTION_DOWN;
            y_offset *= -1;
        }

        if (x_offset > 180)
            x_offset = 180;

        if (y_offset > 60)
            y_offset = 60;

        command[0] = IOMOJO_MOVEOFFSET_CMD;
        command[1] = cnt->track.iomojo_id;
        command[2] = direction;
        command[3] = x_offset;
        command[4] = y_offset;
        iomojo_command(cnt, command, 5, 0);
    }

    motion_log(LOG_INFO, 0, "iomojo_center() succeed");

    return cnt->track.move_wait;
}

static unsigned short int iomojo_move(struct context *cnt, int dev, 
                                      struct coord *cent, struct images *imgs)
{
    char command[5];
    int direction = 0;
    int nx = 0, ny = 0;
    int i;
    
    if (dev < 0)
        if (!iomojo_center(cnt, 0, 0))
            return 0;

    if (cent->x < imgs->width / 2) {
        direction |= IOMOJO_DIRECTION_LEFT;
        nx = imgs->width / 2 - cent->x;
    }

    if (cent->x > imgs->width / 2) {
        direction |= IOMOJO_DIRECTION_RIGHT;
        nx = cent->x - imgs->width / 2;
    }

    if (cent->y < imgs->height / 2) {
        direction |= IOMOJO_DIRECTION_DOWN;
        ny = imgs->height / 2 - cent->y;
    }

    if (cent->y > imgs->height / 2) {
        direction |= IOMOJO_DIRECTION_UP;
        ny = cent->y - imgs->height / 2;
    }

    nx = nx * 72 / imgs->width;
    ny = ny * 72 / imgs->height;

    if (nx || ny) {
        if (nx > 180)
            nx = 180;

        if (ny > 60)
            ny = 60;

        command[0] = IOMOJO_MOVEOFFSET_CMD;
        command[1] = cnt->track.iomojo_id;
        command[2] = direction;
        command[3] = nx;
        command[4] = ny;
        iomojo_command(cnt, command, 5, 0);

        /* Number of frames to skip while moving */
        if (ny >= nx)
            i = 25 * ny / 90;
        else
            i = 25 * nx / 90;

        return i;
    }

    return 0;
}

/******************************************************************************

    Logitech QuickCam Orbit camera tracking code by folkert@vanheusden.com

******************************************************************************/
#ifndef WITHOUT_V4L
static unsigned short int lqos_center(struct context *cnt, int dev, int x_angle, int y_angle)
{
    int reset = 3;
    struct pwc_mpt_angles pma;
    struct pwc_mpt_range pmr;

    if (cnt->track.dev == -1) {

        if (ioctl(dev, VIDIOCPWCMPTRESET, &reset) == -1) {
            motion_log(LOG_ERR, 1, "Failed to reset pwc camera to starting position! Reason");
            return 0;
        }

        SLEEP(6,0)

        if (ioctl(dev, VIDIOCPWCMPTGRANGE, &pmr) == -1) {
            motion_log(LOG_ERR, 1, "failed VIDIOCPWCMPTGRANGE");
            return 0;
        }

        cnt->track.dev = dev;
        cnt->track.minmaxfound = 1;
        cnt->track.panmin = pmr.pan_min;
        cnt->track.panmax = pmr.pan_max;
        cnt->track.tiltmin = pmr.tilt_min;
        cnt->track.tiltmax = pmr.tilt_max;
    }

    if (ioctl(dev, VIDIOCPWCMPTGANGLE, &pma) == -1)
        motion_log(LOG_ERR, 1, "ioctl VIDIOCPWCMPTGANGLE");
    
    pma.absolute = 1;

    if (x_angle * 100 < cnt->track.panmax && x_angle * 100 > cnt->track.panmin)
        pma.pan = x_angle * 100;

    if (y_angle * 100 < cnt->track.tiltmax && y_angle * 100 > cnt->track.tiltmin)
        pma.tilt = y_angle * 100;

    if (ioctl(dev, VIDIOCPWCMPTSANGLE, &pma) == -1) {
        motion_log(LOG_ERR, 1, "Failed to pan/tilt pwc camera! Reason");
        return 0;
    }

    motion_log(LOG_INFO, 0, "lqos_center succeed");

    return cnt->track.move_wait;
}

static unsigned short int lqos_move(struct context *cnt, int dev, struct coord *cent, 
                                    struct images *imgs, unsigned short int manual)
{
    int delta_x = cent->x - (imgs->width / 2);
    int delta_y = cent->y - (imgs->height / 2);
    int move_x_degrees, move_y_degrees;
    struct pwc_mpt_angles pma;
    struct pwc_mpt_range pmr;

    /* If we are on auto track we calculate delta, otherwise we use user input in degrees times 100 */
    if (!manual) {
        if (delta_x > imgs->width * 3 / 8 && delta_x < imgs->width * 5 / 8)
            return 0;
        if (delta_y > imgs->height * 3 / 8 && delta_y < imgs->height * 5 / 8)
            return 0;

        move_x_degrees = delta_x * cnt->track.step_angle_x * 100 / (imgs->width / 2);
        move_y_degrees = -delta_y * cnt->track.step_angle_y * 100 / (imgs->height / 2);
    } else {
        move_x_degrees = cent->x * 100;
        move_y_degrees = cent->y * 100;
    }
    
    /* If we never checked for the min/max values for pan/tilt we do it now */
    if (cnt->track.minmaxfound == 0) {
        if (ioctl(dev, VIDIOCPWCMPTGRANGE, &pmr) == -1) {
            motion_log(LOG_ERR, 1, "failed VIDIOCPWCMPTGRANGE");
            return 0;
        }
        cnt->track.minmaxfound = 1;
        cnt->track.panmin = pmr.pan_min;
        cnt->track.panmax = pmr.pan_max;
        cnt->track.tiltmin = pmr.tilt_min;
        cnt->track.tiltmax = pmr.tilt_max;
    }

    /* Get current camera position */
    if (ioctl(dev, VIDIOCPWCMPTGANGLE, &pma) == -1)
        motion_log(LOG_ERR, 1, "ioctl VIDIOCPWCMPTGANGLE");


    /* Check current position of camera and see if we need to adjust
       values down to what is left to move */
    if (move_x_degrees<0 && (cnt->track.panmin - pma.pan) > move_x_degrees)
        move_x_degrees = (cnt->track.panmin - pma.pan);

    if (move_x_degrees>0 && (cnt->track.panmax - pma.pan) < move_x_degrees)
        move_x_degrees = (cnt->track.panmax - pma.pan);

    if (move_y_degrees<0 && (cnt->track.tiltmin - pma.tilt) > move_y_degrees)
        move_y_degrees = (cnt->track.tiltmin - pma.tilt);

    if (move_y_degrees>0 && (cnt->track.tiltmax - pma.tilt) < move_y_degrees)
        move_y_degrees = (cnt->track.tiltmax - pma.tilt);
        
    /* Move camera relative to current position */
    pma.absolute = 0;
    pma.pan = move_x_degrees;
    pma.tilt = move_y_degrees;

    if (ioctl(dev, VIDIOCPWCMPTSANGLE, &pma) == -1) {
        motion_log(LOG_ERR, 1, "Failed to pan/tilt pwc camera! Reason");
        return 0;
    }

    return cnt->track.move_wait;
}
/******************************************************************************

    Logitech QuickCam Sphere camera tracking code by oBi
    
    Modify by Dirk Wesenberg(Munich) 30.03.07 
    - for new API in uvcvideo 
    - add Trace-steps for investigation
******************************************************************************/
#ifdef MOTION_V4L2

static unsigned short int uvc_center(struct context *cnt, int dev, int x_angle, int y_angle)
{
    /* CALC ABSOLUTE MOVING : Act.Position +/- delta to request X and Y */
    int move_x_degrees = 0, move_y_degrees = 0;

    union pantilt {
        struct {
            short pan;
            short tilt;
        } s16;
        int value;
    };
    union pantilt pan;

    if (cnt->track.dev == -1) {

        int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt
        struct v4l2_control control_s;

        control_s.id = V4L2_CID_PANTILT_RESET;
        control_s.value = (unsigned char) reset;

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            motion_log(LOG_ERR, 1, "Failed to reset UVC camera to starting position! Reason");
            return 0;
        }
        motion_log(LOG_DEBUG, 0, "Reseting UVC camera to starting position");

        SLEEP(8, 0)

        /* Get camera range */
        struct v4l2_queryctrl queryctrl;

        queryctrl.id = V4L2_CID_PAN_RELATIVE;

        if (ioctl(dev, VIDIOC_QUERYCTRL, &queryctrl) < 0) {
            motion_log(LOG_ERR, 1, "ioctl querycontrol");
            return 0;
        }

        motion_log(LOG_DEBUG, 0, "Getting camera range");
        

        /* DWe 30.03.07 The orig request failed : 
        * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual 
        * Range X = -70 to +70 degrees              
        * Y = -30 to +30 degrees  
        */    
        
//        //get mininum
//        pan.value = queryctrl.minimum;

        cnt->track.panmin = -4480 / INCPANTILT;
        cnt->track.tiltmin = -1920 / INCPANTILT;
//        //get maximum
        cnt->track.panmax = 4480 / INCPANTILT; 
        cnt->track.tiltmax = 1920 / INCPANTILT;
//        pan.value = queryctrl.maximum;

        cnt->track.dev = dev;
        cnt->track.pan_angle = 0;
        cnt->track.tilt_angle = 0;
        cnt->track.minmaxfound = 1;

    }

    struct v4l2_control control_s;

    motion_log(LOG_DEBUG, 0, "INPUT_PARAM_ABS pan_min %d,pan_max %d,tilt_min %d,tilt_max %d ", 
               cnt->track.panmin, cnt->track.panmax, cnt->track.tiltmin, cnt->track.tiltmax );
    motion_log(LOG_DEBUG, 0, "INPUT_PARAM_ABS X_Angel %d, Y_Angel %d ", x_angle, y_angle);

    if (x_angle <= cnt->track.panmax && x_angle >= cnt->track.panmin)
        move_x_degrees = x_angle - (cnt->track.pan_angle);

    if (y_angle <= cnt->track.tiltmax && y_angle >= cnt->track.tiltmin)
        move_y_degrees = y_angle - (cnt->track.tilt_angle);
            

    /*
    tilt up: - value
    tilt down: + value
    pan left: - value
    pan right: + value
    */
    pan.s16.pan = -move_x_degrees * INCPANTILT;
    pan.s16.tilt = -move_y_degrees * INCPANTILT;
    
    motion_log(LOG_DEBUG, 0, "For_SET_ABS move_X %d,move_Y %d", move_x_degrees, move_y_degrees);
        
    /* DWe 30.03.07 Must be broken in diff calls, because 
        - one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
        - The Webcam or uvcvideo does not like a call with a zero-move 
    */
    
    if (move_x_degrees != 0) {
        control_s.id = V4L2_CID_PAN_RELATIVE;
    //    control_s.value = pan.value;
        control_s.value = pan.s16.pan;
        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            motion_log(LOG_ERR, 1, "Failed to move UVC camera!");
            return 0;
        }
    }

    /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */     
    if ((move_x_degrees != 0) && (move_y_degrees != 0)) 
        SLEEP (1,0);
       
    
    if (move_y_degrees != 0) {
        control_s.id = V4L2_CID_TILT_RELATIVE;
    //    control_s.value = pan.value;
        control_s.value = pan.s16.tilt;
        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            motion_log(LOG_ERR, 1, "Failed to move UVC camera!");
            return 0;
        }    
    
    }

    motion_log(LOG_DEBUG, 0,"Found MINMAX = %d", cnt->track.minmaxfound); 

    if (cnt->track.dev != -1) {
        motion_log(LOG_DEBUG, 0," Before_ABS_Y_Angel : x= %d , Y= %d , ", 
                   cnt->track.pan_angle, cnt->track.tilt_angle );
        if (move_x_degrees != -1)  
            cnt->track.pan_angle += move_x_degrees;
        
        if (move_x_degrees != -1)  
            cnt->track.tilt_angle += move_y_degrees;
        
        motion_log(LOG_DEBUG, 0," After_ABS_Y_Angel : x= %d , Y= %d , ", 
                   cnt->track.pan_angle, cnt->track.tilt_angle );    
    }

    return cnt->track.move_wait;
}

static unsigned short int uvc_move(struct context *cnt, int dev, struct coord *cent, 
                                   struct images *imgs, unsigned short int manual)
{
    /* RELATIVE MOVING : Act.Position +/- X and Y */
    
    int delta_x = cent->x - (imgs->width / 2);
    int delta_y = cent->y - (imgs->height / 2);
    int move_x_degrees, move_y_degrees;
    
    /* DWe 30.03.07 Does the request of act.position from WebCam work ? luvcview shows at every position 180 :( */
    /*        Now we init the Web by call Reset, so we can sure, that we are at x/y = 0,0                 */
    /*         Don't worry, if the WebCam make a sound - over End at PAN  - hmmm, should it be normal ...? */
    /*         PAN Value 7777 in relative will init also a want reset for CAM - it will be "0" after that  */  
    if ((cnt->track.minmaxfound != 1) || (cent->x == 7777)) {
        unsigned short int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt
        struct v4l2_control control_s;

        control_s.id = V4L2_CID_PANTILT_RESET;
        control_s.value = (unsigned char) reset;

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            motion_log(LOG_ERR, 1, "Failed to reset UVC camera to starting position! Reason");
            return 0;
        }

        motion_log(LOG_DEBUG, 0, "Reseting UVC camera to starting position");
        
        /* set the "helpvalue" back to null because after reset CAM should be in x=0 and not 70 */
        cent->x = 0;
        SLEEP(8,0);
        
        /* DWe 30.03.07 The orig request failed : 
        * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual 
        * Range X = -70 to +70 degrees              
        *    Y = -30 to +30 degrees  
        */    

        cnt->track.panmin = -4480 / INCPANTILT;
        cnt->track.tiltmin = -1920 / INCPANTILT;
        cnt->track.panmax = 4480 / INCPANTILT; 
        cnt->track.tiltmax = 1920 / INCPANTILT;
        cnt->track.dev = dev;
        cnt->track.pan_angle = 0;
        cnt->track.tilt_angle = 0;
        cnt->track.minmaxfound = 1;
    }

    
    /* If we are on auto track we calculate delta, otherwise we use user input in degrees */
    if (!manual) {
        if (delta_x > imgs->width * 3 / 8 && delta_x < imgs->width * 5 / 8)
            return 0;
        if (delta_y > imgs->height * 3 / 8 && delta_y < imgs->height * 5 / 8)
            return 0;

        move_x_degrees = delta_x * cnt->track.step_angle_x / (imgs->width / 2);
        move_y_degrees = -delta_y * cnt->track.step_angle_y / (imgs->height / 2);
    } else {
        move_x_degrees = cent->x;
        move_y_degrees = cent->y;
    }

    union pantilt {
        struct {
            short pan;
            short tilt;
        } s16;
        int value;
    };

    struct v4l2_control control_s;
    union pantilt pan;

    if (cnt->track.minmaxfound == 1) {
    /* Check current position of camera and see if we need to adjust
    values down to what is left to move */
        if (move_x_degrees<0 && (cnt->track.panmin - cnt->track.pan_angle) > move_x_degrees)
            move_x_degrees = (cnt->track.panmin - cnt->track.pan_angle);

        if (move_x_degrees>0 && (cnt->track.panmax - cnt->track.pan_angle) < move_x_degrees)
            move_x_degrees = (cnt->track.panmax - cnt->track.pan_angle);

        if (move_y_degrees<0 && (cnt->track.tiltmin - cnt->track.tilt_angle) > move_y_degrees)
            move_y_degrees = (cnt->track.tiltmin - cnt->track.tilt_angle);

        if (move_y_degrees>0 && (cnt->track.tiltmax - cnt->track.tilt_angle) < move_y_degrees)
            move_y_degrees = (cnt->track.tiltmax - cnt->track.tilt_angle);
    }


    motion_log(LOG_DEBUG, 0, "For_SET_REL pan_min %d,pan_max %d,tilt_min %d,tilt_max %d ", 
               cnt->track.panmin, cnt->track.panmax, cnt->track.tiltmin, cnt->track.tiltmax );
    motion_log(LOG_DEBUG, 0, "For_SET_REL track_pan_Angel %d, track_tilt_Angel %d ", 
               cnt->track.pan_angle, cnt->track.tilt_angle);    
    motion_log(LOG_DEBUG, 0, "For_SET_REL move_X %d,move_Y %d", 
               move_x_degrees, move_y_degrees);
    /*
    tilt up: - value
    tilt down: + value
    pan left: - value
    pan right: + value
    */

    pan.s16.pan = -move_x_degrees * INCPANTILT;
    pan.s16.tilt = -move_y_degrees * INCPANTILT;
    
    /* DWe 30.03.07 Must be broken in diff calls, because 
           - one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
           - The Webcam or uvcvideo does not like a call with a zero-move 
        */

    if (move_x_degrees != 0) {

        control_s.id = V4L2_CID_PAN_RELATIVE;

        control_s.value = pan.s16.pan;
        motion_log(LOG_DEBUG, 0," dev %d,addr= %d, control_S= %d,Wert= %d,", 
                   dev,VIDIOC_S_CTRL, &control_s, pan.s16.pan ); 

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) { 
             motion_log(LOG_ERR, 1, "Failed to move UVC camera!");
            return 0;
        }
    }
    
    /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */     
    if ((move_x_degrees != 0) && (move_y_degrees != 0)) 
        SLEEP (1,0);
       


    if (move_y_degrees != 0) {

        control_s.id = V4L2_CID_TILT_RELATIVE;
        control_s.value = pan.s16.tilt;
        motion_log(LOG_DEBUG, 0," dev %d,addr= %d, control_S= %d, Wert= %d, ", 
                   dev,VIDIOC_S_CTRL, &control_s, pan.s16.tilt); 

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            motion_log(LOG_ERR, 1, "Failed to move UVC camera!");
            return 0;
        }
    }
    
  
    motion_log(LOG_DEBUG, 0,"Found MINMAX = %d", cnt->track.minmaxfound); 
    
    if (cnt->track.minmaxfound == 1) {
        motion_log(LOG_DEBUG, 0," Before_REL_Y_Angel : x= %d , Y= %d", 
                   cnt->track.pan_angle, cnt->track.tilt_angle);
        
        if (move_x_degrees != 0) 
            cnt->track.pan_angle += -pan.s16.pan / INCPANTILT;
            
        if (move_y_degrees != 0)
            cnt->track.tilt_angle += -pan.s16.tilt / INCPANTILT;
            
         motion_log(LOG_DEBUG, 0," After_REL_Y_Angel : x= %d , Y= %d", 
                    cnt->track.pan_angle, cnt->track.tilt_angle);
    }

    return cnt->track.move_wait;
}
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */
