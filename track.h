/*    track.h
 *
 *    Experimental motion tracking.
 *
 *    Copyright 2000, Jeroen Vreeken
 *    This program is published under the GNU Public license
 */

#ifndef _INCLUDE_TRACK_H
#define _INCLUDE_TRACK_H

#include "alg.h"

struct trackoptions {
    int dev;
    /* Config options: */
    unsigned short int type;
    char *port;
    unsigned short int motorx;
    unsigned short int motory;
    unsigned short int maxx;
    unsigned short int maxy;
    unsigned short int stepsize;
    unsigned short int speed;
    unsigned short int iomojo_id;
    unsigned short int active;
    int panmin;
    int panmax;
    int tiltmin;
    int tiltmax;
    unsigned short int minmaxfound;
    unsigned short int step_angle_x;
    unsigned short int step_angle_y;
    unsigned short int move_wait;
    // UVC
    int pan_angle; // degrees
    int tilt_angle; // degrees
};

extern struct trackoptions track_template;

unsigned short int track_center(struct context *, int, unsigned short int, int, int);
unsigned short int track_move(struct context *, int, struct coord *, struct images *, unsigned short int);

/*
    Some default values:
 */
#define TRACK_SPEED             255
#define TRACK_STEPSIZE          40

#define TRACK_TYPE_STEPPER      1
#define TRACK_TYPE_IOMOJO       2
#define TRACK_TYPE_PWC          3
#define TRACK_TYPE_GENERIC      4
#define TRACK_TYPE_UVC          5

/*
    Some defines for the Serial stepper motor:
 */

#define STEPPER_BAUDRATE        B9600

#define STEPPER_STATUS_LEFT     1
#define STEPPER_STATUS_RIGHT    2
#define STEPPER_STATUS_SAFETYL  4
#define STEPPER_STATUS_SAFETYR  8

#define STEPPER_STATUS_UP       1
#define STEPPER_STATUS_DOWN     2
#define STEPPER_STATUS_SAFETYU  4
#define STEPPER_STATUS_SAFETYD  8



#define STEPPER_COMMAND_STATUS  0
#define STEPPER_COMMAND_LEFT_N  1
#define STEPPER_COMMAND_RIGHT_N 2
#define STEPPER_COMMAND_LEFT    3
#define STEPPER_COMMAND_RIGHT   4
#define STEPPER_COMMAND_SWEEP   5
#define STEPPER_COMMAND_STOP    6
#define STEPPER_COMMAND_SPEED   7

#define STEPPER_COMMAND_UP_N    1
#define STEPPER_COMMAND_DOWN_N  2
#define STEPPER_COMMAND_UP      3
#define STEPPER_COMMAND_DOWN    4


/*
    Some defines for the Iomojo Smilecam:
 */

#define IOMOJO_BAUDRATE    B19200

#define IOMOJO_CHECKPOWER_CMD   0xff
#define IOMOJO_CHECKPOWER_RET   'Q'
#define IOMOJO_MOVEOFFSET_CMD   0xfe
#define IOMOJO_SETSPEED_CMD     0xfd
#define IOMOJO_SETSPEED_RET     'P'
#define IOMOJO_MOVEHOME         0xf9
#define IOMOJO_RESTART          0xf7

#define IOMOJO_DIRECTION_RIGHT  0x01
#define IOMOJO_DIRECTION_LEFT   0x02
#define IOMOJO_DIRECTION_DOWN   0x04
#define IOMOJO_DIRECTION_UP     0x08

#ifndef WITHOUT_V4L

/*
    Defines for the Logitech QuickCam Orbit/Sphere USB webcam
*/

#define LQOS_VERTICAL_DEGREES   180
#define LQOS_HORIZONAL_DEGREES  120

/*
 * UVC
 */

#ifdef MOTION_V4L2

#ifndef V4L2_CID_PAN_RELATIVE
#define V4L2_CID_PAN_RELATIVE   (V4L2_CID_PRIVATE_BASE+7)
#endif

#ifndef V4L2_CID_TILT_RELATIVE
#define V4L2_CID_TILT_RELATIVE  (V4L2_CID_PRIVATE_BASE+8)
#endif

#ifndef V4L2_CID_PANTILT_RESET
#define V4L2_CID_PANTILT_RESET  (V4L2_CID_PRIVATE_BASE+9)
#endif

#define INCPANTILT 64 // 1 degree
#endif /* MOTION_V4L2 */


#endif /* WITHOUT_V4L */

#endif /* _INCLUDE_TRACK_H */
