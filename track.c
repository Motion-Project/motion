/*    track.c
 *
 *    Experimental motion tracking.
 *
 *    Copyright 2000, Jeroen Vreeken
 *    This program is published under the GNU Public license
 */

#include <math.h>
#include "motion.h"

#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))
#include "pwc-ioctl.h"
#endif


struct trackoptions track_template = {
    dev:            -1,             /* dev open */
    port:           NULL,           /* char *port */
    motorx:         0,              /* int motorx */
    motory:         0,              /* int motory */
    maxx:           0,              /* int maxx; */
    maxy:           0,              /* int maxy; */
    minx:           0,              /* int minx; */
    miny:           0,              /* int miny; */
    homex:          128,            /* int homex; */
    homey:          128,            /* int homey; */
    motorx_reverse: 0,              /* int reversed x servo; */
    motory_reverse: 0,              /* int reversed y servo; */
    speed:          TRACK_SPEED,    /* speed */
    stepsize:       TRACK_STEPSIZE, /* stepsize */
    active:         0,              /* auto tracking active */
    minmaxfound:    0,              /* flag for minmax values stored for pwc based camera */
    step_angle_x:   10,             /* UVC step angle in degrees X-axis that camera moves during auto tracking */
    step_angle_y:   10,             /* UVC step angle in degrees Y-axis that camera moves during auto tracking */
    move_wait:      10              /* number of frames to disable motion detection after camera moving */
};




/* Add your own center and move functions here: */

static unsigned int servo_position(struct context *cnt, unsigned int motor);

static unsigned int servo_center(struct context *cnt, int xoff, int yoff ATTRIBUTE_UNUSED);
static unsigned int stepper_center(struct context *cnt, int xoff, int yoff ATTRIBUTE_UNUSED);
static unsigned int iomojo_center(struct context *cnt, int xoff, int yoff);

static unsigned int stepper_move(struct context *cnt, struct coord *cent, struct images *imgs);
static unsigned int servo_move(struct context *cnt, struct coord *cent,
                                     struct images *imgs, unsigned int manual);
static unsigned int iomojo_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs);

#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))
static unsigned int lqos_center(struct context *cnt, int dev, int xoff, int yoff);
static unsigned int lqos_move(struct context *cnt, int dev, struct coord *cent,
                                    struct images *imgs, unsigned int manual);
#ifdef MOTION_V4L2
static unsigned int uvc_center(struct context *cnt, int dev, int xoff, int yoff);
static unsigned int uvc_move(struct context *cnt, int dev, struct coord *cent,
                                   struct images *imgs, unsigned int manual);
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */

/* Add a call to your functions here: */
unsigned int track_center(struct context *cnt, int dev ATTRIBUTE_UNUSED,
                                unsigned int manual, int xoff, int yoff)
{
    if (!manual && !cnt->track.active)
        return 0;

    if (cnt->track.type == TRACK_TYPE_STEPPER) {
        unsigned int ret;
        ret = stepper_center(cnt, xoff, yoff);
        if (!ret) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: internal error");
            return 0;
        }
        else return ret;
    } else if (cnt->track.type == TRACK_TYPE_SERVO) {
        return servo_center(cnt, xoff, yoff);
    }
#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))
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

    MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: internal error, %hu is not a known track-type",
               cnt->track.type);

    return 0;
}

/* Add a call to your functions here: */
unsigned int track_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs,
                              unsigned int manual)
{

    if (!manual && !cnt->track.active)
        return 0;

    if (cnt->track.type == TRACK_TYPE_STEPPER)
        return stepper_move(cnt, cent, imgs);
    else if (cnt->track.type == TRACK_TYPE_SERVO)
        return servo_move(cnt, cent, imgs, manual);
#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))
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

    MOTION_LOG(WRN, TYPE_TRACK, SHOW_ERRNO, "%s: internal error, %hu is not a known track-type",
               cnt->track.type);

    return 0;
}

/******************************************************************************
    Stepper motor on serial port
    http://www.lavrsen.dk/twiki/bin/view/Motion/MotionTracking
    http://www.lavrsen.dk/twiki/bin/view/Motion/MotionTrackerAPI
******************************************************************************/

static unsigned int stepper_command(struct context *cnt, unsigned int motor,
                                    unsigned int command, unsigned int data)
{
    char buffer[3];
    time_t timeout = time(NULL);

    buffer[0] = motor;
    buffer[1] = command;
    buffer[2] = data;

    if (write(cnt->track.dev, buffer, 3) != 3) {
        MOTION_LOG(NTC, TYPE_TRACK, SHOW_ERRNO, "%s: port %s dev fd %i, motor %hu command %hu data %hu",
                   cnt->track.port, cnt->track.dev, motor, command, data);
        return 0;
    }

    while (read(cnt->track.dev, buffer, 1) != 1 && time(NULL) < timeout + 1);

    if (time(NULL) >= timeout + 2) {
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Status byte timeout!");
        return 0;
    }

    return buffer[0];
}


static unsigned int stepper_status(struct context *cnt, unsigned int motor)
{
    return stepper_command(cnt, motor, STEPPER_COMMAND_STATUS, 0);
}


static unsigned int stepper_center(struct context *cnt, int x_offset, int y_offset)
{
    struct termios adtio;

    if (cnt->track.dev < 0) {
        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Try to open serial device %s", cnt->track.port);

        if ((cnt->track.dev = open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to open serial device %s",
                       cnt->track.port);
            return 0;
        }

        bzero (&adtio, sizeof(adtio));
        adtio.c_cflag= STEPPER_BAUDRATE | CS8 | CLOCAL | CREAD;
        adtio.c_iflag= IGNPAR;
        adtio.c_oflag= 0;
        adtio.c_lflag= 0;    /* non-canon, no echo */
        adtio.c_cc[VTIME] = 0;    /* timer unused */
        adtio.c_cc[VMIN] = 0;    /* blocking read until 1 char */
        tcflush (cnt->track.dev, TCIFLUSH);

        if (tcsetattr(cnt->track.dev, TCSANOW, &adtio) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to initialize serial device %s",
                       cnt->track.port);
            cnt->track.dev = -1;
            return 0;
        }
        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Opened serial device %s and initialize, fd %i",
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

static unsigned int stepper_move(struct context *cnt,
                                       struct coord *cent, struct images *imgs)
{
    unsigned int command = 0, data = 0;

    if (cnt->track.dev < 0) {
        MOTION_LOG(WRN, TYPE_TRACK, NO_ERRNO, "%s: No device %s started yet , trying stepper_center()",
                   cnt->track.port);

        if (!stepper_center(cnt, 0, 0)) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: failed to initialize stepper device on %s , fd [%i].",
                       cnt->track.port, cnt->track.dev);
            return 0;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: succeed , device started %s , fd [%i]",
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

    if (data) stepper_command(cnt, cnt->track.motorx, command, data);

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
 *   Servo motor on serial port
 *   http://www.lavrsen.dk/twiki/bin/view/Motion/MotionTracking
 *   http://www.lavrsen.dk/twiki/bin/view/Motion/MotionTrackerServoAPI
 ******************************************************************************/

static int servo_open(struct context *cnt)
{
    struct termios adtio;

    if ((cnt->track.dev = open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to open serial device %s",
                   cnt->track.port);
        return 0;
    }

    bzero (&adtio, sizeof(adtio));
    adtio.c_cflag= SERVO_BAUDRATE | CS8 | CLOCAL | CREAD;
    adtio.c_iflag= IGNPAR;
    adtio.c_oflag= 0;
    adtio.c_lflag= 0;       /* non-canon, no echo */
    adtio.c_cc[VTIME] = 0;  /* timer unused */
    adtio.c_cc[VMIN] = 0;   /* blocking read until 1 char */
    tcflush (cnt->track.dev, TCIFLUSH);

    if (tcsetattr(cnt->track.dev, TCSANOW, &adtio) < 0) {
        MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: Unable to initialize serial device %s",
                   cnt->track.port);
        cnt->track.dev = -1;
        return 0;
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Opened serial device %s and initialize, fd %i",
               cnt->track.port, cnt->track.dev);

    return 1;
}


static unsigned int servo_command(struct context *cnt, unsigned int motor,
                                  unsigned int command, unsigned int data)
{
    unsigned char buffer[3];
    time_t timeout = time(NULL);

    buffer[0] = motor;
    buffer[1] = command;
    buffer[2] = data;


    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: SENDS port %s dev fd %i, motor %hu command %hu data %hu",
               cnt->track.port, cnt->track.dev, buffer[0], buffer[1], buffer[2]);

    if (write(cnt->track.dev, buffer, 3) != 3) {
        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: port %s dev fd %i, motor %hu command %hu data %hu",
                   cnt->track.port, cnt->track.dev, motor, command, data);
        return 0;
    }

    while (read(cnt->track.dev, buffer, 1) != 1 && time(NULL) < timeout + 1);

    if (time(NULL) >= timeout + 2) {
        MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: Status byte timeout!");
        return 0;
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Command return %d", buffer[0]);


    return buffer[0];
}


static unsigned int servo_position(struct context *cnt, unsigned int motor)
{
    unsigned int ret = 0;

    ret = servo_command(cnt, motor, SERVO_COMMAND_POSITION, 0);

    return ret;
}


/**
 * servo_move
 *      Does relative movements to current position.
 *
 */
static unsigned int servo_move(struct context *cnt, struct coord *cent,
                                     struct images *imgs, unsigned int manual)
{
    unsigned int command = 0;
    unsigned int data = 0;
    unsigned int position;

    /* If device is not open yet , open and center */
    if (cnt->track.dev < 0) {
        if (!servo_center(cnt, 0, 0)) {
            MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: Problem opening servo!");
            return 0;
        }
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: cent->x %d, cent->y %d, reversex %d,"
               "reversey %d manual %d", cent->x , cent->y,
               cnt->track.motorx_reverse, cnt->track.motory_reverse, manual);

    if (manual) {
        int offset;

        if (cent->x) {
            position = servo_position(cnt, cnt->track.motorx);
            offset = cent->x * cnt->track.stepsize;


            if ((cnt->track.motorx_reverse && (offset > 0)) ||
                (!cnt->track.motorx_reverse && (offset < 0)))
                command = SERVO_COMMAND_LEFT_N;
            else
                command = SERVO_COMMAND_RIGHT_N;

            data = abs(offset);

            if ((data + position > (unsigned)cnt->track.maxx) ||
                (position - offset < (unsigned)cnt->track.minx)) {
                MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: x %d value out of range! (%d - %d)",
                           data, cnt->track.minx, cnt->track.maxx);
                return 0;
            }

            /* Set Speed , TODO : it should be done only when speed changes */
            servo_command(cnt, cnt->track.motorx, SERVO_COMMAND_SPEED, cnt->track.speed);
            servo_command(cnt, cnt->track.motorx, command, data);
        }


        if (cent->y) {
            position = servo_position(cnt, cnt->track.motory);
            offset = cent->y * cnt->track.stepsize;

            if ((cnt->track.motory_reverse && (offset > 0)) ||
                (!cnt->track.motory_reverse && (offset < 0)))
                command = SERVO_COMMAND_UP_N;
            else
                command = SERVO_COMMAND_DOWN_N;

            data = abs(offset);

            if ((data + position > (unsigned)cnt->track.maxy) ||
                (position - offset < (unsigned)cnt->track.miny)) {
                MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: y %d value out of range! (%d - %d)",
                           data, cnt->track.miny, cnt->track.maxy);
                return 0;
            }

            /* Set Speed , TODO : it should be done only when speed changes */
            servo_command(cnt, cnt->track.motory, SERVO_COMMAND_SPEED, cnt->track.speed);
            servo_command(cnt, cnt->track.motory, command, data);
        }

    } else {
        /***** x-axis *****/

        /* Move left */
        if (cent->x < imgs->width / 2) {
            if (cnt->track.motorx_reverse)
                command = SERVO_COMMAND_RIGHT_N;
            else
                command = SERVO_COMMAND_LEFT_N;
            data = imgs->width / 2 - cent->x;
        }

        /* Move right */
        if (cent->x > imgs->width / 2) {
            if (cnt->track.motorx_reverse)
                command = SERVO_COMMAND_LEFT_N;
            else
                command = SERVO_COMMAND_RIGHT_N;
            data = cent->x - imgs->width / 2;
        }


        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: X offset %d", data);

        data = data * cnt->track.stepsize / imgs->width;

        if (data && command) {

            // TODO: need to get position to avoid overflow limits
            position = servo_position(cnt, cnt->track.motorx);

            if ((position + data > (unsigned)cnt->track.maxx) ||
                (position - data < (unsigned)cnt->track.minx)) {
                MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: x %d value out of range! (%d - %d)",
                           data, cnt->track.minx, cnt->track.maxx);
                return 0;
            }

            /* Set Speed , TODO : it should be done only when speed changes */

            servo_command(cnt, cnt->track.motorx, SERVO_COMMAND_SPEED, cnt->track.speed);
            servo_command(cnt, cnt->track.motorx, command, data);

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: X cent->x %d, cent->y %d, reversex %d,"
                       "reversey %d motorx %d data %d command %d",
                       cent->x, cent->y, cnt->track.motorx_reverse,
                       cnt->track.motory_reverse, cnt->track.motorx, data, command);
        }

        /***** y-axis *****/

        /* Move down */
        if (cent->y < imgs->height / 2) {
            if (cnt->track.motory_reverse)
                command = SERVO_COMMAND_UP_N;
            else
                command = SERVO_COMMAND_DOWN_N;
            data = imgs->height / 2 - cent->y;
        }

        /* Move up */
        if (cent->y > imgs->height / 2) {
            if (cnt->track.motory_reverse)
                command = SERVO_COMMAND_DOWN_N;
            else
                command = SERVO_COMMAND_UP_N;
            data = cent->y - imgs->height / 2;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Y offset %d", data);

        data = data * cnt->track.stepsize / imgs->height;

        if (data && command) {

            // TODO: need to get position to avoid overflow limits
            position = servo_position(cnt, cnt->track.motory);

            if ((position + data > (unsigned)cnt->track.maxy) ||
                (position - data < (unsigned)cnt->track.miny)) {
                MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: y %d value out of range! (%d - %d)",
                           data, cnt->track.miny, cnt->track.maxy);
                return 0;
            }

            /* Set Speed , TODO : it should be done only when speed changes */
            servo_command(cnt, cnt->track.motory, SERVO_COMMAND_SPEED, cnt->track.speed);
            servo_command(cnt, cnt->track.motory, command, data);

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Y cent->x %d, cent->y %d, reversex %d,"
                       "reversey %d motory %d data %d command %d",
                        cent->x, cent->y, cnt->track.motorx_reverse,
                        cnt->track.motory_reverse, cnt->track.motory, command);
        }
    }

    return cnt->track.move_wait;
}

#if 0
static unsigned int servo_status(struct context *cnt, unsigned int motor)
{
    return servo_command(cnt, motor, SERVO_COMMAND_STATUS, 0);
}
#endif

/**
 * servo_center
 *      Moves servo to home position.
 *      Does absolute movements ( offsets relative to home position ).
 *
 *      Note : Using Clockwise as a convention for right , left , up , down
 *              so left minx , right maxx , down miny , up maxy
 *
 */

static unsigned int servo_center(struct context *cnt, int x_offset, int y_offset)
{
    int x_offset_abs;
    int y_offset_abs;

    /* If device is not open yet */
    if (cnt->track.dev < 0) {
        if (!servo_open(cnt)) {
            MOTION_LOG(ERR, TYPE_TRACK, NO_ERRNO, "%s: Problem opening servo!");
            return 0;
        }
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: X-offset %d, Y-offset %d, x-position %d. y-position %d,"
               "reversex %d, reversey %d , stepsize %d", x_offset, y_offset,
               cnt->track.homex + (x_offset * cnt->track.stepsize),
               cnt->track.homey + (y_offset * cnt->track.stepsize),
               cnt->track.motorx_reverse, cnt->track.motory_reverse,
               cnt->track.stepsize);

    /* x-axis */
    if (cnt->track.motorx_reverse)
        x_offset_abs = (128 - cnt->track.homex) - (x_offset * cnt->track.stepsize) + 128;
    else
        x_offset_abs = cnt->track.homex + (x_offset * cnt->track.stepsize);

    if (x_offset_abs <= cnt->track.maxx  && x_offset_abs >= cnt->track.minx) {
        /* Set Speed , TODO : it should be done only when speed changes */
        servo_command(cnt, cnt->track.motorx, SERVO_COMMAND_SPEED, cnt->track.speed);
        servo_command(cnt, cnt->track.motorx, SERVO_COMMAND_ABSOLUTE, x_offset_abs);
    }

    /* y-axis */
    if (cnt->track.motory_reverse)
        y_offset_abs = (128 - cnt->track.homey) - (y_offset * cnt->track.stepsize) + 128;
    else
        y_offset_abs = cnt->track.homey + (y_offset * cnt->track.stepsize);

    if (y_offset_abs <= cnt->track.maxy && y_offset_abs >= cnt->track.minx) {
        /* Set Speed , TODO : it should be done only when speed changes */
        servo_command(cnt, cnt->track.motory, SERVO_COMMAND_SPEED, cnt->track.speed);
        servo_command(cnt, cnt->track.motory, SERVO_COMMAND_ABSOLUTE, y_offset_abs);
    }

    return cnt->track.move_wait;
}


/******************************************************************************

    Iomojo Smilecam on serial port

******************************************************************************/

static char iomojo_command(struct context *cnt, char *command, int len, unsigned int ret)
{
    char buffer[1];
    time_t timeout = time(NULL);

    if (write(cnt->track.dev, command, len) != len)
        return 0;

    if (ret) {
        while (read(cnt->track.dev, buffer, 1) != 1 && time(NULL) < timeout + 2);

        if (time(NULL) >= timeout + 2) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Return byte timeout!");
            return 0;
        }
    }
    /* range values ? */
    return buffer[0];
}

static void iomojo_setspeed(struct context *cnt, unsigned int speed)
{
    char command[3];

    command[0] = IOMOJO_SETSPEED_CMD;
    command[1] = cnt->track.iomojo_id;
    command[2] = speed;

    if (iomojo_command(cnt, command, 3, 1) != IOMOJO_SETSPEED_RET)
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to set camera speed");
}

static void iomojo_movehome(struct context *cnt)
{
    char command[2];

    command[0] = IOMOJO_MOVEHOME;
    command[1] = cnt->track.iomojo_id;

    iomojo_command(cnt, command, 2, 0);
}

static unsigned int iomojo_center(struct context *cnt, int x_offset, int y_offset)
{
    struct termios adtio;
    char command[5], direction = 0;

    if (cnt->track.dev < 0) {
        if ((cnt->track.dev = open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to open serial device %s",
                       cnt->track.port);
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
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Unable to initialize serial device %s",
                       cnt->track.port);
            return 0;
        }
    }

    iomojo_setspeed(cnt, 40);
    iomojo_movehome(cnt);

    if (x_offset || y_offset) {
        if (x_offset > 0) {
            direction |= IOMOJO_DIRECTION_RIGHT;
        } else {
            direction |= IOMOJO_DIRECTION_LEFT;
            x_offset *= -1;
        }

        if (y_offset > 0) {
            direction |= IOMOJO_DIRECTION_UP;
        } else {
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

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: succeed");

    return cnt->track.move_wait;
}

static unsigned int iomojo_move(struct context *cnt, int dev, struct coord *cent,
                                      struct images *imgs)
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
#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))
static unsigned int lqos_center(struct context *cnt, int dev, int x_angle, int y_angle)
{
    int reset = 3;
    struct pwc_mpt_angles pma;
    struct pwc_mpt_range pmr;

    if (cnt->track.dev == -1) {

        if (ioctl(dev, VIDIOCPWCMPTRESET, &reset) == -1) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to reset pwc camera to starting position! Reason");
            return 0;
        }

        SLEEP(6, 0);

        if (ioctl(dev, VIDIOCPWCMPTGRANGE, &pmr) == -1) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: failed VIDIOCPWCMPTGRANGE");
            return 0;
        }

        cnt->track.dev = dev;
        cnt->track.minmaxfound = 1;
        cnt->track.minx = pmr.pan_min;
        cnt->track.maxx = pmr.pan_max;
        cnt->track.miny = pmr.tilt_min;
        cnt->track.maxy = pmr.tilt_max;
    }

    if (ioctl(dev, VIDIOCPWCMPTGANGLE, &pma) == -1)
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: ioctl VIDIOCPWCMPTGANGLE");

    pma.absolute = 1;

    if (x_angle * 100 < cnt->track.maxx && x_angle * 100 > cnt->track.minx)
        pma.pan = x_angle * 100;

    if (y_angle * 100 < cnt->track.maxy && y_angle * 100 > cnt->track.miny)
        pma.tilt = y_angle * 100;

    if (ioctl(dev, VIDIOCPWCMPTSANGLE, &pma) == -1) {
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to pan/tilt pwc camera! Reason");
        return 0;
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: succeed");

    return cnt->track.move_wait;
}

static unsigned int lqos_move(struct context *cnt, int dev, struct coord *cent,
                                    struct images *imgs, unsigned int manual)
{
    int delta_x = cent->x - (imgs->width / 2);
    int delta_y = cent->y - (imgs->height / 2);
    int move_x_degrees, move_y_degrees;
    struct pwc_mpt_angles pma;
    struct pwc_mpt_range pmr;

    /* If we are on auto track we calculate delta, otherwise we use user input in degrees times 100 */
    if (!manual) {
        if (delta_x > imgs->width * 3/8 && delta_x < imgs->width * 5/8)
            return 0;
        if (delta_y > imgs->height * 3/8 && delta_y < imgs->height * 5/8)
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
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: failed VIDIOCPWCMPTGRANGE");
            return 0;
        }
        cnt->track.minmaxfound = 1;
        cnt->track.minx = pmr.pan_min;
        cnt->track.maxx = pmr.pan_max;
        cnt->track.miny = pmr.tilt_min;
        cnt->track.maxy = pmr.tilt_max;
    }

    /* Get current camera position */
    if (ioctl(dev, VIDIOCPWCMPTGANGLE, &pma) == -1)
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: ioctl VIDIOCPWCMPTGANGLE");


    /*
     * Check current position of camera and see if we need to adjust
     * values down to what is left to move
     */
    if (move_x_degrees < 0 && (cnt->track.minx - pma.pan) > move_x_degrees)
        move_x_degrees = (cnt->track.minx - pma.pan);

    if (move_x_degrees > 0 && (cnt->track.maxx - pma.pan) < move_x_degrees)
        move_x_degrees = (cnt->track.maxx - pma.pan);

    if (move_y_degrees < 0 && (cnt->track.miny - pma.tilt) > move_y_degrees)
        move_y_degrees = (cnt->track.miny - pma.tilt);

    if (move_y_degrees > 0 && (cnt->track.maxy - pma.tilt) < move_y_degrees)
        move_y_degrees = (cnt->track.maxy - pma.tilt);

    /* Move camera relative to current position */
    pma.absolute = 0;
    pma.pan = move_x_degrees;
    pma.tilt = move_y_degrees;

    if (ioctl(dev, VIDIOCPWCMPTSANGLE, &pma) == -1) {
        MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to pan/tilt pwc camera! Reason");
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

static unsigned int uvc_center(struct context *cnt, int dev, int x_angle, int y_angle)
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
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to reset UVC camera to starting position! Reason");
            return 0;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Reseting UVC camera to starting position");

        SLEEP(8, 0);

        /* Get camera range */
        struct v4l2_queryctrl queryctrl;
        queryctrl.id = V4L2_CID_PAN_RELATIVE;

        if (ioctl(dev, VIDIOC_QUERYCTRL, &queryctrl) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: ioctl querycontrol");
            return 0;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Getting camera range");

       /* DWe 30.03.07 The orig request failed :
        * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual
        * Range X = -70 to +70 degrees
        * Y = -30 to +30 degrees
        */

        //get mininum
        //pan.value = queryctrl.minimum;

        cnt->track.minx = -4480 / INCPANTILT;
        cnt->track.miny = -1920 / INCPANTILT;
        //get maximum
        cnt->track.maxx = 4480 / INCPANTILT;
        cnt->track.maxy = 1920 / INCPANTILT;
        //pan.value = queryctrl.maximum;

        cnt->track.dev = dev;
        cnt->track.pan_angle = 0;
        cnt->track.tilt_angle = 0;
        cnt->track.minmaxfound = 1;

    }

    struct v4l2_control control_s;

    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "%s: INPUT_PARAM_ABS pan_min %d,pan_max %d,tilt_min %d,tilt_max %d ",
               cnt->track.minx, cnt->track.maxx, cnt->track.miny, cnt->track.maxy);
    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "%s: INPUT_PARAM_ABS X_Angel %d, Y_Angel %d ",
               x_angle, y_angle);

    if (x_angle <= cnt->track.maxx && x_angle >= cnt->track.minx)
        move_x_degrees = x_angle - (cnt->track.pan_angle);

    if (y_angle <= cnt->track.maxy && y_angle >= cnt->track.miny)
        move_y_degrees = y_angle - (cnt->track.tilt_angle);


    /*
     * tilt up: - value
     * tilt down: + value
     * pan left: - value
     * pan right: + value
     */
    pan.s16.pan = -move_x_degrees * INCPANTILT;
    pan.s16.tilt = -move_y_degrees * INCPANTILT;

    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "%s: For_SET_ABS move_X %d,move_Y %d",
               move_x_degrees, move_y_degrees);

    /* DWe 30.03.07 Must be broken in diff calls, because
     * one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
     * The Webcam or uvcvideo does not like a call with a zero-move
     */

    if (move_x_degrees != 0) {
        control_s.id = V4L2_CID_PAN_RELATIVE;
        //control_s.value = pan.value;
        control_s.value = pan.s16.pan;

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to move UVC camera!");
            return 0;
        }
    }

    /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */
    if ((move_x_degrees != 0) && (move_y_degrees != 0))
        SLEEP(1, 0);

    if (move_y_degrees != 0) {
        control_s.id = V4L2_CID_TILT_RELATIVE;
        //control_s.value = pan.value;
        control_s.value = pan.s16.tilt;

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to move UVC camera!");
            return 0;
        }
    }

    MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Found MINMAX = %d",
               cnt->track.minmaxfound);

    if (cnt->track.dev != -1) {
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "%s: Before_ABS_Y_Angel : x= %d , Y= %d, ",
                   cnt->track.pan_angle, cnt->track.tilt_angle);

        if (move_x_degrees != -1) {
            cnt->track.pan_angle += move_x_degrees;
        }

        if (move_x_degrees != -1) {
            cnt->track.tilt_angle += move_y_degrees;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: After_ABS_Y_Angel : x= %d , Y= %d",
                   cnt->track.pan_angle, cnt->track.tilt_angle);
    }

    return cnt->track.move_wait;
}

static unsigned int uvc_move(struct context *cnt, int dev, struct coord *cent,
                                   struct images *imgs, unsigned int manual)
{
    /* RELATIVE MOVING : Act.Position +/- X and Y */

    int delta_x = cent->x - (imgs->width / 2);
    int delta_y = cent->y - (imgs->height / 2);
    int move_x_degrees, move_y_degrees;

    /*
     *  DWe 30.03.07 Does the request of act.position from WebCam work ? luvcview shows at every position 180 :(
     *        Now we init the Web by call Reset, so we can sure, that we are at x/y = 0,0
     *        Don't worry, if the WebCam make a sound - over End at PAN  - hmmm, should it be normal ...?
     *        PAN Value 7777 in relative will init also a want reset for CAM - it will be "0" after that
     */
    if ((cnt->track.minmaxfound != 1) || (cent->x == 7777)) {
        unsigned int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt
        struct v4l2_control control_s;

        control_s.id = V4L2_CID_PANTILT_RESET;
        control_s.value = (unsigned char) reset;

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to reset UVC camera to starting position! Reason");
            return 0;
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO, "%s: Reseting UVC camera to starting position");

        /* set the "helpvalue" back to null because after reset CAM should be in x=0 and not 70 */
        cent->x = 0;
        SLEEP(8, 0);

        /*
         * DWe 30.03.07 The orig request failed :
         * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual
         * Range X = -70 to +70 degrees
         *       Y = -30 to +30 degrees
         */

        cnt->track.minx = -4480 / INCPANTILT;
        cnt->track.miny = -1920 / INCPANTILT;
        cnt->track.maxx = 4480 / INCPANTILT;
        cnt->track.maxy = 1920 / INCPANTILT;
        cnt->track.dev = dev;
        cnt->track.pan_angle = 0;
        cnt->track.tilt_angle = 0;
        cnt->track.minmaxfound = 1;
    }


    /* If we are on auto track we calculate delta, otherwise we use user input in degrees */
    if (!manual) {
        if (delta_x > imgs->width * 3/8 && delta_x < imgs->width * 5/8)
            return 0;
        if (delta_y > imgs->height * 3/8 && delta_y < imgs->height * 5/8)
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
    /*
     * Check current position of camera and see if we need to adjust
     * values down to what is left to move
     */
        if (move_x_degrees < 0 && (cnt->track.minx - cnt->track.pan_angle) > move_x_degrees)
            move_x_degrees = cnt->track.minx - cnt->track.pan_angle;

        if (move_x_degrees > 0 && (cnt->track.maxx - cnt->track.pan_angle) < move_x_degrees)
            move_x_degrees = cnt->track.maxx - cnt->track.pan_angle;

        if (move_y_degrees < 0 && (cnt->track.miny - cnt->track.tilt_angle) > move_y_degrees)
            move_y_degrees = cnt->track.miny - cnt->track.tilt_angle;

        if (move_y_degrees > 0 && (cnt->track.maxy - cnt->track.tilt_angle) < move_y_degrees)
            move_y_degrees = cnt->track.maxy - cnt->track.tilt_angle;
    }

    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "For_SET_REL pan_min %d,pan_max %d,tilt_min %d,tilt_max %d",
               cnt->track.minx, cnt->track.maxx, cnt->track.miny, cnt->track.maxy);
    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "For_SET_REL track_pan_Angel %d, track_tilt_Angel %d",
               cnt->track.pan_angle, cnt->track.tilt_angle);
    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "For_SET_REL move_X %d,move_Y %d", move_x_degrees, move_y_degrees);

    /*
     * tilt up: - value
     * tilt down: + value
     * pan left: - value
     * pan right: + value
     */

    pan.s16.pan = -move_x_degrees * INCPANTILT;
    pan.s16.tilt = -move_y_degrees * INCPANTILT;

    /* DWe 30.03.07 Must be broken in diff calls, because
     * one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
     * The Webcam or uvcvideo does not like a call with a zero-move
     */

    if (move_x_degrees != 0) {

        control_s.id = V4L2_CID_PAN_RELATIVE;

        control_s.value = pan.s16.pan;
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, " dev %d, addr= %d, control_S= %d, Wert= %d",
                   dev, VIDIOC_S_CTRL, &control_s, pan.s16.pan);

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to move UVC camera!");
            return 0;
        }
    }

    /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */
    if ((move_x_degrees != 0) && (move_y_degrees != 0))
        SLEEP (1, 0);


    if (move_y_degrees != 0) {

        control_s.id = V4L2_CID_TILT_RELATIVE;

        control_s.value = pan.s16.tilt;
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, " dev %d,addr= %d, control_S= %d, Wert= %d",
                   dev, VIDIOC_S_CTRL, &control_s, pan.s16.tilt);

        if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
            MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO, "%s: Failed to move UVC camera!");
            return 0;
        }
    }

    MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "%s: Found MINMAX = %d",
                cnt->track.minmaxfound);

    if (cnt->track.minmaxfound == 1) {
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "Before_REL_Y_Angel : x= %d , Y= %d",
                   cnt->track.pan_angle, cnt->track.tilt_angle);

        if (move_x_degrees != 0)
            cnt->track.pan_angle += -pan.s16.pan / INCPANTILT;

        if (move_y_degrees != 0)
            cnt->track.tilt_angle += -pan.s16.tilt / INCPANTILT;

        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO, "After_REL_Y_Angel : x= %d , Y= %d",
                   cnt->track.pan_angle, cnt->track.tilt_angle);
    }

    return cnt->track.move_wait;
}
#endif /* MOTION_V4L2 */
#endif /* WITHOUT_V4L */
