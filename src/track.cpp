/*    track.cpp
 *
 *    Experimental motion tracking.
 *
 *    Copyright 2000, Jeroen Vreeken
 *    This program is published under the GNU Public license
 */
#include <math.h>
#include "motion.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "track.hpp"

#ifdef HAVE_V4L2
    /*UVC*/
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
    #include <linux/videodev2.h>
#endif

#define TRACK_TYPE_GENERIC      1
#define TRACK_TYPE_UVC          2


/******************************************************************************
    Logitech QuickCam Sphere camera tracking code by oBi
    Modify by Dirk Wesenberg(Munich) 30.03.07
    - for new API in uvcvideo
    - add Trace-steps for investigation
******************************************************************************/
static int uvc_center(struct ctx_cam *cam, int dev, int x_angle, int y_angle) {

    #ifdef HAVE_V4L2
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

        if (cam->track->dev == -1) {

            int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt
            struct v4l2_control control_s;

            control_s.id = V4L2_CID_PAN_RESET;
            control_s.value = (unsigned char) reset;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to reset UVC camera to starting position! Reason"));
                return 0;
            }

            control_s.id = V4L2_CID_TILT_RESET;
            control_s.value = (unsigned char) reset;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to reset UVC camera to starting position! Reason"));
                return 0;
            }

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO
                ,_("Reseting UVC camera to starting position"));

            SLEEP(8, 0);

            /* Get camera range */
            struct v4l2_queryctrl queryctrl;
            queryctrl.id = V4L2_CID_PAN_RELATIVE;

            if (ioctl(dev, VIDIOC_QUERYCTRL, &queryctrl) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO,_("ioctl querycontrol"));
                return 0;
            }

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO,_("Getting camera range"));

            /* DWe 30.03.07 The orig request failed :
            * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual
            * Range X = -70 to +70 degrees
            * Y = -30 to +30 degrees
            */

            //get mininum
            //pan.value = queryctrl.minimum;

            cam->track->minx = -4480 / INCPANTILT;
            cam->track->miny = -1920 / INCPANTILT;
            //get maximum
            cam->track->maxx = 4480 / INCPANTILT;
            cam->track->maxy = 1920 / INCPANTILT;
            //pan.value = queryctrl.maximum;

            cam->track->dev = dev;
            cam->track->pan_angle = 0;
            cam->track->tilt_angle = 0;
            cam->track->minmaxfound = 1;

        }

        struct v4l2_control control_s;

        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("INPUT_PARAM_ABS pan_min %d,pan_max %d,tilt_min %d,tilt_max %d ")
            ,cam->track->minx, cam->track->maxx, cam->track->miny, cam->track->maxy);
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("INPUT_PARAM_ABS X_Angel %d, Y_Angel %d ")
            ,x_angle, y_angle);

        if (x_angle <= cam->track->maxx && x_angle >= cam->track->minx){
            move_x_degrees = x_angle - (cam->track->pan_angle);
        }

        if (y_angle <= cam->track->maxy && y_angle >= cam->track->miny){
            move_y_degrees = y_angle - (cam->track->tilt_angle);
        }

        /*
        * tilt up: - value
        * tilt down: + value
        * pan left: - value
        * pan right: + value
        */
        pan.s16.pan = -move_x_degrees * INCPANTILT;
        pan.s16.tilt = -move_y_degrees * INCPANTILT;

        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("For_SET_ABS move_X %d,move_Y %d")
            ,move_x_degrees, move_y_degrees);

        /* DWe 30.03.07 Must be broken in diff calls, because
        * one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
        * The Webcam or uvcvideo does not like a call with a zero-move
        */

        if (move_x_degrees != 0) {
            control_s.id = V4L2_CID_PAN_RELATIVE;
            //control_s.value = pan.value;
            control_s.value = pan.s16.pan;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO,_("Failed to move UVC camera!"));
                return 0;
            }
        }

        /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */
        if ((move_x_degrees != 0) && (move_y_degrees != 0)){
            SLEEP(1, 0);
        }

        if (move_y_degrees != 0) {
            control_s.id = V4L2_CID_TILT_RELATIVE;
            //control_s.value = pan.value;
            control_s.value = pan.s16.tilt;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO,_("Failed to move UVC camera!"));
                return 0;
            }
        }

        MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO
            ,_("Found MINMAX = %d"),cam->track->minmaxfound);

        if (cam->track->dev != -1) {
            MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
                ,_("Before_ABS_Y_Angel : x= %d , Y= %d, ")
                ,cam->track->pan_angle, cam->track->tilt_angle);

            if (move_x_degrees != -1) {
                cam->track->pan_angle += move_x_degrees;
            }

            if (move_x_degrees != -1) {
                cam->track->tilt_angle += move_y_degrees;
            }

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO
                ,_("After_ABS_Y_Angel : x= %d , Y= %d")
                ,cam->track->pan_angle, cam->track->tilt_angle);
        }

        return cam->conf->track_move_wait;
    #else
        return 0;
    #endif

}

static int uvc_move(struct ctx_cam *cam, int dev, struct ctx_coord *cent
            , struct ctx_images *imgs, int manual) {

    #ifdef HAVE_V4L2
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
        if ((cam->track->minmaxfound != 1) || (cent->x == 7777)) {
            int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt
            struct v4l2_control control_s;

            control_s.id = V4L2_CID_PAN_RESET;
            control_s.value = (unsigned char) reset;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to reset UVC camera to starting position! Reason"));
                return 0;
            }

            control_s.id = V4L2_CID_TILT_RESET;
            control_s.value = (unsigned char) reset;

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to reset UVC camera to starting position! Reason"));
                return 0;
            }

            MOTION_LOG(NTC, TYPE_TRACK, NO_ERRNO
                ,_("Reseting UVC camera to starting position"));

            /* set the "helpvalue" back to null because after reset CAM should be in x=0 and not 70 */
            cent->x = 0;
            SLEEP(8, 0);

            /*
            * DWe 30.03.07 The orig request failed :
            * must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual
            * Range X = -70 to +70 degrees
            *       Y = -30 to +30 degrees
            */

            cam->track->minx = -4480 / INCPANTILT;
            cam->track->miny = -1920 / INCPANTILT;
            cam->track->maxx = 4480 / INCPANTILT;
            cam->track->maxy = 1920 / INCPANTILT;
            cam->track->dev = dev;
            cam->track->pan_angle = 0;
            cam->track->tilt_angle = 0;
            cam->track->minmaxfound = 1;
        }


        /* If we are on auto track we calculate delta, otherwise we use user input in degrees */
        if (!manual) {
            if (delta_x > imgs->width * 3/8 && delta_x < imgs->width * 5/8){
                return 0;
            }
            if (delta_y > imgs->height * 3/8 && delta_y < imgs->height * 5/8){
                return 0;
            }

            move_x_degrees = delta_x * cam->conf->track_step_angle_x / (imgs->width / 2);
            move_y_degrees = -delta_y * cam->conf->track_step_angle_y / (imgs->height / 2);
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

        if (cam->track->minmaxfound == 1) {
            /*
            * Check current position of camera and see if we need to adjust
            * values down to what is left to move
            */
            if (move_x_degrees < 0 && (cam->track->minx - cam->track->pan_angle) > move_x_degrees){
                move_x_degrees = cam->track->minx - cam->track->pan_angle;
            }

            if (move_x_degrees > 0 && (cam->track->maxx - cam->track->pan_angle) < move_x_degrees){
                move_x_degrees = cam->track->maxx - cam->track->pan_angle;
            }

            if (move_y_degrees < 0 && (cam->track->miny - cam->track->tilt_angle) > move_y_degrees){
                move_y_degrees = cam->track->miny - cam->track->tilt_angle;
            }

            if (move_y_degrees > 0 && (cam->track->maxy - cam->track->tilt_angle) < move_y_degrees){
                move_y_degrees = cam->track->maxy - cam->track->tilt_angle;
            }
        }

        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("For_SET_REL pan_min %d,pan_max %d,tilt_min %d,tilt_max %d")
            ,cam->track->minx, cam->track->maxx, cam->track->miny, cam->track->maxy);
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("For_SET_REL track_pan_Angel %d, track_tilt_Angel %d")
            ,cam->track->pan_angle, cam->track->tilt_angle);
        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("For_SET_REL move_X %d,move_Y %d"), move_x_degrees, move_y_degrees);

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
            MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
                ,_(" dev %d, addr= %d, control_S= %d, Wert= %d")
                ,dev, VIDIOC_S_CTRL, &control_s, pan.s16.pan);

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to move UVC camera!"));
                return 0;
            }
        }

        /* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */
        if ((move_x_degrees != 0) && (move_y_degrees != 0)){
            SLEEP (1, 0);
        }

        if (move_y_degrees != 0) {

            control_s.id = V4L2_CID_TILT_RELATIVE;

            control_s.value = pan.s16.tilt;
            MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
                ,_(" dev %d,addr= %d, control_S= %d, Wert= %d")
                ,dev, VIDIOC_S_CTRL, &control_s, pan.s16.tilt);

            if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
                MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
                    ,_("Failed to move UVC camera!"));
                return 0;
            }
        }

        MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
            ,_("Found MINMAX = %d"), cam->track->minmaxfound);

        if (cam->track->minmaxfound == 1) {
            MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
                ,_("Before_REL_Y_Angel : x= %d , Y= %d")
                ,cam->track->pan_angle, cam->track->tilt_angle);

            if (move_x_degrees != 0){
                cam->track->pan_angle += -pan.s16.pan / INCPANTILT;
            }

            if (move_y_degrees != 0){
                cam->track->tilt_angle += -pan.s16.tilt / INCPANTILT;
            }

            MOTION_LOG(DBG, TYPE_TRACK, NO_ERRNO
                ,_("After_REL_Y_Angel : x= %d , Y= %d")
                ,cam->track->pan_angle, cam->track->tilt_angle);
        }

        return cam->conf->track_move_wait;
    #else
        return 0;
    #endif /* HAVE_V4L2 */
}


static int generic_move(struct ctx_cam *cam, enum track_action action, int manual, int xoff
        , int yoff, struct ctx_coord *cent, struct ctx_images *imgs) {

    char fmtcmd[PATH_MAX];
    cam->track->posx += cent->x;
    cam->track->posy += cent->y;

    mystrftime(cam, fmtcmd, sizeof(fmtcmd), cam->conf->track_generic_move.c_str()
        , &cam->current_image->imgts, NULL, 0);

    if (!fork()) {
        int i;
        char buf[12];

        /* Detach from parent */
        setsid();

        /* Provides data as environment variables */
        if (manual){
          setenv("TRACK_MANUAL", "manual", 1);
        }
        switch (action) {
          case TRACK_CENTER:
            setenv("TRACK_ACTION", "center", 1);
            sprintf(buf, "%d", xoff);    setenv("TRACK_XOFF", buf, 1);
            sprintf(buf, "%d", yoff);    setenv("TRACK_YOFF", buf, 1);
            break;
          case TRACK_MOVE:
            setenv("TRACK_ACTION", "move", 1);
            if (cent) {
              sprintf(buf, "%d", cent->x);          setenv("TRACK_CENT_X", buf, 1);
              sprintf(buf, "%d", cent->y);          setenv("TRACK_CENT_Y", buf, 1);
              sprintf(buf, "%d", cent->width);      setenv("TRACK_CENT_WIDTH", buf, 1);
              sprintf(buf, "%d", cent->height);     setenv("TRACK_CENT_HEIGHT", buf, 1);
              sprintf(buf, "%d", cent->minx);       setenv("TRACK_CENT_MINX", buf, 1);
              sprintf(buf, "%d", cent->maxx);       setenv("TRACK_CENT_MAXX", buf, 1);
              sprintf(buf, "%d", cent->miny);       setenv("TRACK_CENT_MINY", buf, 1);
              sprintf(buf, "%d", cent->maxy);       setenv("TRACK_CENT_MAXY", buf, 1);
            }
            if (imgs) {
              sprintf(buf, "%d", imgs->width);      setenv("TRACK_IMGS_WIDTH", buf, 1);
              sprintf(buf, "%d", imgs->height);     setenv("TRACK_IMGS_HEIGHT", buf, 1);
              sprintf(buf, "%d", imgs->motionsize); setenv("TRACK_IMGS_MOTIONSIZE", buf, 1);
            }
        }

        /*
         * Close any file descriptor except console because we will
         * like to see error messages
         */
        for (i = getdtablesize() - 1; i > 2; i--){
            close(i);
        }

        execl("/bin/sh", "sh", "-c", fmtcmd, " &", (char *)NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO
            ,_("Unable to start external command '%s'")
            ,cam->conf->track_generic_move.c_str());

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO
        ,_("Executing external command '%s'")
        , fmtcmd);

    return cam->conf->track_move_wait;
}

void track_init(struct ctx_cam *cam){

    cam->track = new ctx_track;
    memset(cam->track,0,sizeof(ctx_track));

    cam->track->dev = -1;             /* dev open */

    if (cam->conf->track_type)
        cam->frame_skip = track_center(cam, cam->video_dev, 0, 0, 0);

}
void track_deinit(struct ctx_cam *cam){

    delete cam->track;

}

/* Add a call to your functions here: */
int track_center(struct ctx_cam *cam, int dev,
        int manual, int xoff, int yoff)
{
    struct ctx_coord cent;
    (void)dev;

    if (!manual && !cam->conf->track_auto) return 0;

    if (cam->conf->track_type == TRACK_TYPE_UVC){
        return uvc_center(cam, dev, xoff, yoff);
    } else if (cam->conf->track_type == TRACK_TYPE_GENERIC) {
        if (cam->conf->track_generic_move != ""){
            cent.x = -cam->track->posx;
            cent.y = -cam->track->posy;
            return generic_move(cam, TRACK_CENTER, manual,0 ,0 ,&cent , NULL);
        } else {
            return 10;
        }
    }

    MOTION_LOG(ERR, TYPE_TRACK, SHOW_ERRNO
        ,_("internal error, %hu is not a known track-type"), cam->conf->track_type);

    return 0;
}

int track_move(struct ctx_cam *cam, int dev, struct ctx_coord *cent
        , struct ctx_images *imgs, int manual)
{

    if (!manual && !cam->conf->track_auto) return 0;

    if (cam->conf->track_type == TRACK_TYPE_UVC){
        return uvc_move(cam, dev, cent, imgs, manual);
    } else if (cam->conf->track_type == TRACK_TYPE_GENERIC) {
        if (cam->conf->track_generic_move != "") {
            return generic_move(cam, TRACK_MOVE, manual, 0, 0, cent, imgs);
        } else {
            return cam->conf->track_move_wait;
        }
    }

    MOTION_LOG(WRN, TYPE_TRACK, SHOW_ERRNO
        ,_("internal error, %hu is not a known track-type"), cam->conf->track_type);

    return 0;
}

