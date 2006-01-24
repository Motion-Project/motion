/*	track.c
 *
 *	Experimental motion tracking.
 *
 *	Copyright 2000, Jeroen Vreeken
 *	This program is published under the GNU Public license
 */

#include <math.h>
#include <termios.h>
#include "motion.h"
//#include "conf.h"
//#include "track.h"
//#include "alg.h"

#include "pwc-ioctl.h"


struct trackoptions track_template = {
	dev:            -1,             /* dev open */
	port:           NULL,           /* char *port */
	motorx:         0,              /* int motorx */
	maxx:           0,              /* int maxx; */
	speed:          TRACK_SPEED,    /* speed */
	stepsize:       TRACK_STEPSIZE, /* stepsize */
	active:         0,              /* auto tracking active */
	minmaxfound:    0,  /* flag for minmax values stored for pwc based camera */
	step_angle_x:   10, /* step angle in degrees X-axis that camera moves during auto tracking */
	step_angle_y:   10, /* step angle in degrees Y-axis that camera moves during auto tracking */
	move_wait:      10   /* number of frames to disable motion detection after camera moving */
};


/* Add your own center and move functions here: */
static int stepper_center(struct context *cnt, int xoff, int yoff ATTRIBUTE_UNUSED);
static int stepper_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs);
static int iomojo_center(struct context *cnt, int xoff, int yoff);
static int iomojo_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs);
static int lqos_center(struct context *cnt, int dev, int xoff, int yoff);
static int lqos_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs, int manual);

/* Add a call to your functions here: */
int track_center(struct context *cnt, int dev, int manual, int xoff, int yoff)
{
	if (!manual && !cnt->track.active)
		return 0;
	if (cnt->track.type == TRACK_TYPE_STEPPER)
		return stepper_center(cnt, xoff, yoff);
	else if (cnt->track.type == TRACK_TYPE_PWC)
		return lqos_center(cnt, dev, xoff, yoff);
	else if (cnt->track.type == TRACK_TYPE_IOMOJO)
		return iomojo_center(cnt, xoff, yoff);
	else if (cnt->track.type == TRACK_TYPE_GENERIC)
		return 10; // FIX ME. I chose to return something reasonable.

	motion_log(LOG_ERR, 1, "track_move: internal error, %d is not a known track-type", cnt->track.type);

	return 0;
}

/* Add a call to your functions here: */
int track_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs, int manual)
{
	if (!manual && !cnt->track.active)
		return 0;
	if (cnt->track.type == TRACK_TYPE_STEPPER)
		return stepper_move(cnt, dev, cent, imgs);
	else if (cnt->track.type == TRACK_TYPE_PWC)
		return lqos_move(cnt, dev, cent, imgs, manual);
	else if (cnt->track.type == TRACK_TYPE_IOMOJO)
		return iomojo_move(cnt, dev, cent, imgs);
	else if (cnt->track.type == TRACK_TYPE_GENERIC)
		return cnt->track.move_wait; // FIX ME. I chose to return something reasonable.

	motion_log(LOG_ERR, 1, "track_move: internal error, %d is not a known track-type", cnt->track.type);

	return 0;
}


/******************************************************************************

	Stepper motor on serial port

******************************************************************************/


static int stepper_command(struct context *cnt, int motor, int command, int n)
{
	char buffer[3];
	time_t timeout=time(NULL);

	buffer[0]=motor;
	buffer[1]=command;
	buffer[2]=n;
	if (write(cnt->track.dev, buffer, 3)!=3)
		return -1;

	while (read(cnt->track.dev, buffer, 1)!=1 && time(NULL) < timeout+1);
	if (time(NULL) >= timeout+2) {
		motion_log(LOG_ERR, 1, "Status byte timeout!");
		return 0;
	}
	return buffer[0];
}


static int stepper_status(struct context *cnt, int motor)
{
	return stepper_command(cnt, motor, STEPPER_COMMAND_STATUS, 0);
}


static int stepper_center(struct context *cnt, int x_offset, int y_offset ATTRIBUTE_UNUSED)
{
	struct termios adtio;

	if (cnt->track.dev<0) {
		if ((cnt->track.dev=open(cnt->track.port, O_RDWR | O_NOCTTY)) < 0) {
			motion_log(LOG_ERR, 1, "Unable to open serial device %s", cnt->track.port);
			exit(1);
		}

		bzero (&adtio, sizeof(adtio));
		adtio.c_cflag= STEPPER_BAUDRATE | CS8 | CLOCAL | CREAD;
		adtio.c_iflag= IGNPAR;
		adtio.c_oflag= 0;
		adtio.c_lflag= 0;	/* non-canon, no echo */
		adtio.c_cc[VTIME]=0;	/* timer unused */
		adtio.c_cc[VMIN]=0;	/* blocking read until 1 char */
		tcflush (cnt->track.dev, TCIFLUSH);

		if (tcsetattr(cnt->track.dev, TCSANOW, &adtio) < 0) {
			motion_log(LOG_ERR, 1, "Unable to initialize serial device %s", cnt->track.port);
			exit(1);
		}
	}

	stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_SPEED, cnt->track.speed);
	stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_LEFT_N, cnt->track.maxx);

	while (stepper_status(cnt, cnt->track.motorx) & STEPPER_STATUS_LEFT);

	stepper_command(cnt, cnt->track.motorx, STEPPER_COMMAND_RIGHT_N,
	                cnt->track.maxx / 2 + x_offset * cnt->track.stepsize);

	while (stepper_status(cnt, cnt->track.motorx) & STEPPER_STATUS_RIGHT);

	return cnt->track.move_wait;
}

static int stepper_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs)
{
	int command = 0;
	int n = 0;

	if (dev < 0)
		if (stepper_center(cnt, 0, 0) < 0)
			return 0;

	if (cent->x < imgs->width / 2) {
		command = STEPPER_COMMAND_LEFT_N;
		n = imgs->width / 2 - cent->x;
	}

	if (cent->x > imgs->width / 2) {
		command = STEPPER_COMMAND_RIGHT_N;
		n = cent->x - imgs->width / 2;
	}

	n = n * cnt->track.stepsize / imgs->width;

	if (n) {
		stepper_command(cnt, cnt->track.motorx, command, n);
		return n / 5;
	}

	return 0;
}

/******************************************************************************

	Iomojo Smilecam on serial port

******************************************************************************/

static char iomojo_command(struct context *cnt, char *command, int len, int ret)
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
	return buffer[0];
}

static void iomojo_setspeed(struct context *cnt, int speed)
{
	char command[3];
	
	command[0] = IOMOJO_SETSPEED_CMD;
	command[1] = cnt->track.iomojo_id;
	command[2] = speed;
	
	if (iomojo_command(cnt, command, 3, 1)!=IOMOJO_SETSPEED_RET)
		motion_log(LOG_ERR, 1, "Unable to set camera speed");
}

static void iomojo_movehome(struct context *cnt)
{
	char command[2];
	
	command[0] = IOMOJO_MOVEHOME;
	command[1] = cnt->track.iomojo_id;

	iomojo_command(cnt, command, 2, 0);
}

static int iomojo_center(struct context *cnt, int x_offset, int y_offset)
{
	struct termios adtio;
	char command[5], direction=0;

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

	return cnt->track.move_wait;
}

static int iomojo_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs)
{
	char command[5];
	int direction = 0;
	int nx = 0, ny = 0;
	int i;
	
	if (dev < 0)
		if (iomojo_center(cnt, 0, 0) < 0)
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

static int lqos_center(struct context *cnt, int dev, int x_angle, int y_angle)
{
	int reset = 3;
	struct pwc_mpt_angles pma;
	struct pwc_mpt_range pmr;

	if (cnt->track.dev==-1) {

		if (ioctl(dev, VIDIOCPWCMPTRESET, &reset) == -1) {
			motion_log(LOG_ERR, 1, "Failed to reset camera to starting position! Reason");
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
		motion_log(LOG_ERR, 1, "Failed to pan/tilt camera! Reason");
		return 0;
	}

	return cnt->track.move_wait;
}

static int lqos_move(struct context *cnt, int dev, struct coord *cent, struct images *imgs, int manual)
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
		motion_log(LOG_ERR, 1, "Failed to pan/tilt camera! Reason");
		return 0;
	}

	return cnt->track.move_wait;
}
