/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*    track.h
 *
 *    Experimental motion tracking.
 *
 *    Copyright 2000, Jeroen Vreeken
 */

#ifndef _INCLUDE_TRACK_H
#define _INCLUDE_TRACK_H

#include "alg.h"
#include <termios.h>

struct trackoptions {
    int dev;
    /* Config options: */
    unsigned int type;
    char *port;
    unsigned int motorx;
    unsigned int motory;
    int maxx;
    int maxy;
    int minx;
    int miny;
    unsigned int stepsize;
    unsigned int speed;
    unsigned int homex;
    unsigned int homey;
    unsigned int iomojo_id;
    unsigned int active;            /* This is the track_auto but 'auto' is defined word so use active*/
    unsigned int motorx_reverse;
    unsigned int motory_reverse;
    unsigned int minmaxfound;
    unsigned int step_angle_x;
    unsigned int step_angle_y;
    unsigned int move_wait;
    /* UVC */
    int pan_angle; // degrees
    int tilt_angle; // degrees
    char *generic_move;
};

extern struct trackoptions track_template;

unsigned int track_center(struct context *cnt, int dev
            , unsigned int manual, int xoff, int yoff);
unsigned int track_move(struct context *cnt, int dev, struct coord *cent
            , struct images *imgs, unsigned int manual);

enum track_action { TRACK_CENTER, TRACK_MOVE };

/*
* Some default values:
*/
#define TRACK_SPEED             255
#define TRACK_STEPSIZE          40

#define TRACK_TYPE_STEPPER      1
#define TRACK_TYPE_IOMOJO       2
#define TRACK_TYPE_PWC          3
#define TRACK_TYPE_GENERIC      4
#define TRACK_TYPE_UVC          5
#define TRACK_TYPE_SERVO        6

/*
* Some defines for the Serial stepper motor:
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
* Some defines for the Serial servo motor:
*/

/*
* Controlling:
* Three bytes are sent to the servo - BYTE1=SERVO_COMMAND BYTE2=COMMAND BYTE3=DATA
* eg, sending the command    01 02 08    would Command SERVO_COMMAND1 to move LEFT a total of 8 STEPS
*
* An extra command 0x08 has been added but here is the basic command set.
*
* 0x00 STATUS   - Current status byte will be returned, data byte ignored
* 0x01 LEFT_N   - Servo will take N Steps to the Left until it reaches the Servos safety limit
* 0x02 RIGHT_N  - Servo will take N Steps to the Right until it reaches the Servos safety limit
* 0x03 LEFT     - Servo will move to Left most position, data byte ignored.
* 0x04 RIGHT    - Servo will move to Right most position, data byte ignored.
* 0x05 SWEEP    - Servo will sweep between its extremes, data byte ignored.
* 0x06 STOP     -  Servo will Stop, data byte ignored
* 0x07 SPEED    - Set servos speed between 0 and 255.
* 0x08 ABSOLUTE - Set servo to absolute position between 0 and 255
* 0x09 POSITION - Get servo to absolute position between 0 and 255
* */

#define SERVO_BAUDRATE        B9600

#define SERVO_COMMAND_STATUS   0
#define SERVO_COMMAND_LEFT_N   1
#define SERVO_COMMAND_RIGHT_N  2
#define SERVO_COMMAND_LEFT     3
#define SERVO_COMMAND_RIGHT    4
#define SERVO_COMMAND_SWEEP    5
#define SERVO_COMMAND_STOP     6
#define SERVO_COMMAND_SPEED    7
#define SERVO_COMMAND_ABSOLUTE 8
#define SERVO_COMMAND_POSITION 9


#define SERVO_COMMAND_UP_N     1
#define SERVO_COMMAND_DOWN_N   2
#define SERVO_COMMAND_UP       3
#define SERVO_COMMAND_DOWN     4



/*
* Some defines for the Iomojo Smilecam:
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

#ifdef HAVE_V4L2

    /*
    * Defines for the Logitech QuickCam Orbit/Sphere USB webcam
    */

    #define LQOS_VERTICAL_DEGREES   180
    #define LQOS_HORIZONAL_DEGREES  120

    /*
    * UVC
    */

    #ifndef V4L2_CID_PAN_RELATIVE
        #define V4L2_CID_PAN_RELATIVE                   (V4L2_CID_CAMERA_CLASS_BASE+4)
    #endif

    #ifndef V4L2_CID_TILT_RELATIVE
        #define V4L2_CID_TILT_RELATIVE                  (V4L2_CID_CAMERA_CLASS_BASE+5)
    #endif

    #ifndef V4L2_CID_PAN_RESET
        #define V4L2_CID_PAN_RESET                      (V4L2_CID_CAMERA_CLASS_BASE+6)
    #endif

    #ifndef V4L2_CID_TILT_RESET
        #define V4L2_CID_TILT_RESET                     (V4L2_CID_CAMERA_CLASS_BASE+7)
    #endif

    #define INCPANTILT 64 // 1 degree


#endif /* HAVE_V4L2 */

#endif /* _INCLUDE_TRACK_H */
