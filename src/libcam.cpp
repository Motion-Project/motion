/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 *
 */

/* TODO:
 * Determine if we need to have multiple requests or buffers.
 *     (The current logic is just a single request and buffer but
 *      this may need to change to allow for multiple requests or buffers
 *      so as to reduce latency.  As of now, it is kept simple with
 *      a single request and buffer.)
 * Need to determine flags for designating start up, shutdown
 *     etc. and possibly add mutex locking.  Startup currently has
 *     a SLEEP to allow for initialization but this should change
 */
#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "video_common.hpp"
#include "libcam.hpp"

#ifdef HAVE_LIBCAM

using namespace libcamera;

bool cls_libcam::cam_parm_bool(char *parm)
{
    if (mystrceq(parm,"1") || mystrceq(parm,"yes") || mystrceq(parm,"on") || mystrceq(parm,"true") ) {
        return true;
    } else {
        return false;
    }
}

float cls_libcam::cam_parm_single(char *parm)
{
    return (float)atof(parm);
}

void cls_libcam:: cam_log_transform()
{
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "Libcamera Transform Options:");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Identity");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Rot0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  HFlip");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  VFlip");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  HVFlip");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Rot180");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Transpose");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Rot270");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Rot90");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Rot180Transpose");

}

void cls_libcam:: cam_log_controls()
{

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "Libcamera Controls:");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeEnable(bool)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeLocked(bool)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeMeteringMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    MeteringCentreWeighted = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    MeteringSpot = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    MeteringMatrix = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    MeteringCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeConstraintMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ConstraintNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ConstraintHighlight = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ConstraintShadows = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ConstraintCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeExposureMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ExposureNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ExposureShort = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ExposureLong = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ExposureCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  ExposureValue(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  ExposureTime(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AnalogueGain(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Brightness(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Contrast(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Lux(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AwbEnable(bool)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AwbMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbAuto = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbIncandescent = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbTungsten = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbFluorescent = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbIndoor = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbDaylight = 5");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbCloudy = 6");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbCustom = 7");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AwbLocked(bool)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  ColourTemperature(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Saturation(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  Sharpness(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  FocusFoM(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  ScalerCrop(Rect x-y-h-w)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  DigitalGain(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  FrameDuration(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  SensorTemperature(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  SensorTimestamp(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfModeManual = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfModeAuto = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfModeContinuous = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfRange(0-2)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfRangeNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfRangeMacro = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfRangeFull = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfSpeed(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfSpeedNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfSpeedFast = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfMetering(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfMeteringAuto = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfMeteringWindows = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfWindows(rect x-y-h-w)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfTrigger(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfTriggerStart = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfTriggerCancel = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfPause(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseImmediate = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseDeferred = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseResume = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  LensPosition(float)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfStateIdle = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfStateScanning = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfStateFocused = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfStateFailed = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AfPauseState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseStateRunning = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseStatePausing = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AfPauseStatePaused = 2");

}

void cls_libcam:: cam_log_draft()
{
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "Libcamera Controls Draft:");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AePrecaptureTrigger(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AePrecaptureTriggerIdle = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AePrecaptureTriggerStart = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AePrecaptureTriggerCancel = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  NoiseReductionMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    NoiseReductionModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    NoiseReductionModeFast = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    NoiseReductionModeHighQuality = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    NoiseReductionModeMinimal = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    NoiseReductionModeZSL = 4");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  ColorCorrectionAberrationMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ColorCorrectionAberrationOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ColorCorrectionAberrationFast = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    ColorCorrectionAberrationHighQuality = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AeState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AeStateSearching = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AeStateConverged = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AeStateLocked = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AeStateFlashRequired = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AeStatePrecapture = 5");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  AwbState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbStateInactive = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbStateSearching = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbConverged = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    AwbLocked = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  SensorRollingShutterSkew(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  LensShadingMapMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    LensShadingMapModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    LensShadingMapModeOn = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  SceneFlicker(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    SceneFickerOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    SceneFicker50Hz = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    SceneFicker60Hz = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  PipelineDepth(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  MaxLatency(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "  TestPatternMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModeSolidColor = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModeColorBars = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModeColorBarsFadeToGray = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModePn9 = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0, "    TestPatternModeCustom1 = 256");

}

void cls_libcam::cam_start_params(ctx_dev *ptr)
{
    int indx;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    camctx = ptr;

    camctx->libcam->params = (ctx_params*)mymalloc(sizeof(ctx_params));
    camctx->libcam->params->update_params = true;
    util_parms_parse(camctx->libcam->params, camctx->conf->libcam_params);

    for (indx = 0; indx < camctx->libcam->params->params_count; indx++) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s : %s"
            ,camctx->libcam->params->params_array[indx].param_name
            ,camctx->libcam->params->params_array[indx].param_value
            );
    }
    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

}

int cls_libcam::cam_start_mgr()
{
    int retcd;
    std::string camid;
    libcamera::Size picsz;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    cam_mgr = std::make_unique<CameraManager>();

    retcd = cam_mgr->start();
    if (retcd != 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Error starting camera manager.  Return code: %d",retcd);
        return retcd;
    }
    started_mgr = true;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "cam_mgr started.");

    if (camctx->conf->libcam_device == "camera0"){
        camid = cam_mgr->cameras()[0]->id();
    } else {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Invalid libcam_device '%s'.  The only name supported is 'camera0' "
            ,camctx->conf->libcam_device.c_str());
        return -1;
    }
    camera = cam_mgr->get(camid);
    camera->acquire();
    started_aqr = true;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

void cls_libcam::cam_config_control_item(char *pname, char *pvalue)
{
    char *x, *y, *w, *h;

    if (mystrceq(pname, "AeEnable")) {
        controls.set(controls::AeEnable, cam_parm_bool(pvalue));
    }
    if (mystrceq(pname, "AeLocked")) {
        controls.set(controls::AeLocked, cam_parm_bool(pvalue));
    }
    if (mystrceq(pname,"AeMeteringMode")) {
       controls.set(controls::AeMeteringMode, atoi(pvalue));
    }
    if (mystrceq(pname,"AeConstraintMode")) {
        controls.set(controls::AeConstraintMode, atoi(pvalue));
    }
    if (mystrceq(pname,"AeExposureMode")) {
        controls.set(controls::AeExposureMode, atoi(pvalue));
    }
    if (mystrceq(pname,"ExposureValue")) {
        controls.set(controls::ExposureValue, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"ExposureTime")) {
        controls.set(controls::ExposureTime, atoi(pvalue));
    }
    if (mystrceq(pname,"AnalogueGain")) {
        controls.set(controls::AnalogueGain, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"Brightness")) {
        controls.set(controls::Brightness, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"Contrast")) {
        controls.set(controls::Contrast, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"Lux")) {
        controls.set(controls::Lux, cam_parm_single(pvalue));
    }
    if (mystrceq(pname, "AwbEnable")) {
        controls.set(controls::AwbEnable, cam_parm_bool(pvalue));
    }
    if (mystrceq(pname,"AwbMode")) {
        controls.set(controls::AwbMode, atoi(pvalue));
    }
    if (mystrceq(pname, "AwbLocked")) {
        controls.set(controls::AwbLocked, cam_parm_bool(pvalue));
    }
    if (mystrceq(pname,"ColourGains")) {
        //extern const Control<Span<const float, 2>> ColourGains;
        /* Not functional (can not set a float to the Span)
        controls.set(controls::ColourGains, cam_parm_single(pvalue));
        */
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "ColourGains Not Implemented.");
    }
    if (mystrceq(pname,"ColourTemperature")) {
        controls.set(controls::ColourTemperature, atoi(pvalue));
    }
    if (mystrceq(pname,"Saturation")) {
        controls.set(controls::Saturation, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"SensorBlackLevels")) {
        //extern const Control<Span<const int32_t, 4>> SensorBlackLevels;
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "SensorBlackLevels Not Implemented.");
    }
    if (mystrceq(pname,"Sharpness")) {
        controls.set(controls::Sharpness, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"FocusFoM")) {
        controls.set(controls::FocusFoM, atoi(pvalue));
    }
    if (mystrceq(pname,"ColourCorrectionMatrix")) {
        //extern const Control<Span<const float, 9>> ColourCorrectionMatrix;
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "SensorBlackLevels Not Implemented.");
    }
    if (mystrceq(pname,"ScalerCrop")) {
        x = strtok(pvalue, "-");
        y = strtok(0, "-");
        w = strtok(0, "-");
        h = strtok(0, "-");
        if (h != NULL) {
            controls.set(controls::ScalerCrop, Rectangle(atoi(x),atoi(y),atoi(w),atoi(h)));
        }
    }
    if (mystrceq(pname,"DigitalGain")) {
        controls.set(controls::DigitalGain, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"FrameDuration")) {
        controls.set(controls::FrameDuration, atoi(pvalue));
    }
    if (mystrceq(pname,"FrameDurationLimits")) {
        //extern const Control<Span<const int64_t, 2>> FrameDurationLimits;
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "FrameDurationLimits Not Implemented.");
    }
    if (mystrceq(pname,"SensorTemperature")) {
        controls.set(controls::SensorTemperature, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"SensorTimestamp")) {
        controls.set(controls::SensorTimestamp, atoi(pvalue));
    }
    if (mystrceq(pname,"AfMode")) {
        controls.set(controls::AfMode, atoi(pvalue));
    }
    if (mystrceq(pname,"AfRange")) {
        controls.set(controls::AfRange, atoi(pvalue));
    }
    if (mystrceq(pname,"AfSpeed")) {
        controls.set(controls::AfSpeed, atoi(pvalue));
    }
    if (mystrceq(pname,"AfMetering")) {
        controls.set(controls::AfMetering, atoi(pvalue));
    }
    if (mystrceq(pname,"AfWindows")) {
        //extern const Control<Span<const Rectangle>> AfWindows;
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "AfWindows Not Implemented.");
    }
    if (mystrceq(pname,"AfTrigger")) {
        controls.set(controls::AfTrigger, atoi(pvalue));
    }
    if (mystrceq(pname,"AfPause")) {
        controls.set(controls::AfPause, atoi(pvalue));
    }
    if (mystrceq(pname,"LensPosition")) {
        controls.set(controls::LensPosition, cam_parm_single(pvalue));
    }
    if (mystrceq(pname,"AfState")) {
        controls.set(controls::AfState, atoi(pvalue));
    }
    if (mystrceq(pname,"AfPauseState")) {
        controls.set(controls::AfPauseState, atoi(pvalue));
    }

    /* DRAFT*/
    if (mystrceq(pname,"AePrecaptureTrigger")) {
        controls.set(controls::draft::AePrecaptureTrigger, atoi(pvalue));
    }
    if (mystrceq(pname,"NoiseReductionMode")) {
        controls.set(controls::draft::NoiseReductionMode, atoi(pvalue));
    }
    if (mystrceq(pname,"ColorCorrectionAberrationMode")) {
        controls.set(controls::draft::ColorCorrectionAberrationMode, atoi(pvalue));
    }
    if (mystrceq(pname,"AeState")) {
        controls.set(controls::draft::AeState, atoi(pvalue));
    }
    if (mystrceq(pname,"AwbState")) {
        controls.set(controls::draft::AwbState, atoi(pvalue));
    }
    if (mystrceq(pname,"SensorRollingShutterSkew")) {
        controls.set(controls::draft::SensorRollingShutterSkew, atoi(pvalue));
    }
    if (mystrceq(pname,"LensShadingMapMode")) {
        controls.set(controls::draft::LensShadingMapMode, atoi(pvalue));
    }
    if (mystrceq(pname,"PipelineDepth")) {
        controls.set(controls::draft::PipelineDepth, atoi(pvalue));
    }
    if (mystrceq(pname,"MaxLatency")) {
        controls.set(controls::draft::MaxLatency, atoi(pvalue));
    }
    if (mystrceq(pname,"TestPatternMode")) {
        controls.set(controls::draft::TestPatternMode, atoi(pvalue));
    }

}

void cls_libcam:: cam_config_controls()
{
    int indx, retcd;

    for (indx = 0; indx < camctx->libcam->params->params_count; indx++) {
        cam_config_control_item(
            camctx->libcam->params->params_array[indx].param_name
            ,camctx->libcam->params->params_array[indx].param_value);
    }

    retcd = config->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "Configuration adjusted.");
    } else if (retcd == CameraConfiguration::Valid) {
         MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Configuration valid");
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Configuration error");
    }

}

void cls_libcam:: cam_config_transform()
{
    int indx, retcd;
    ctx_params_item itm;

    config->transform = Transform::Identity;

    for (indx = 0; indx < camctx->libcam->params->params_count; indx++) {
        itm = camctx->libcam->params->params_array[indx];
        if (mystreq(itm.param_name,"transform")) {
            if (mystrceq(itm.param_value,"identity")) {
                config->transform = Transform::Identity;
            } else if (mystrceq(itm.param_value,"rot0")) {
                config->transform = Transform::Rot0;
            } else if (mystrceq(itm.param_value,"hflip")) {
                config->transform = Transform::HFlip;
            } else if (mystrceq(itm.param_value,"vflip")) {
                config->transform = Transform::VFlip;
            } else if (mystrceq(itm.param_value,"hvflip")) {
                config->transform = Transform::HVFlip;
            } else if (mystrceq(itm.param_value,"rot180")) {
                config->transform = Transform::Rot180;
            } else if (mystrceq(itm.param_value,"transpose")) {
                config->transform = Transform::Transpose;
            } else if (mystrceq(itm.param_value,"rot270")) {
                config->transform = Transform::Rot270;
            } else if (mystrceq(itm.param_value,"rot90")) {
                config->transform = Transform::Rot90;
            } else if (mystrceq(itm.param_value,"rot180transpose")) {
                config->transform = Transform::Rot180Transpose;
            } else {
                MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                    , "Invalid transform option: %s."
                    , itm.param_value);
            }
        }
    }

    retcd = config->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "Configuration adjusted.");
    } else if (retcd == CameraConfiguration::Valid) {
         MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Configuration valid");
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Configuration error");
    }

}

int cls_libcam::cam_start_config()
{
    int retcd;
    libcamera::Size picsz;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    config = camera->generateConfiguration({ StreamRole::Viewfinder });

    config->at(0).pixelFormat = PixelFormat::fromString("YUV420");

    config->at(0).size.width = camctx->conf->width;
    config->at(0).size.height = camctx->conf->height;
    config->at(0).bufferCount = 1;

    retcd = config->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        if (config->at(0).pixelFormat != PixelFormat::fromString("YUV420")) {
            MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                , "Pixel format was adjusted to %s."
                , config->at(0).pixelFormat.toString().c_str());
            return -1;
        } else {
            MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                , "Configuration adjusted.");
        }
    } else if (retcd == CameraConfiguration::Valid) {
         MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            , "Configuration is valid");
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Error setting configuration");
        return -1;
    }

    if ((config->at(0).size.width != (unsigned int)camctx->conf->width) ||
        (config->at(0).size.height != (unsigned int)camctx->conf->height)) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            , "Image size adjusted from %d x %d to %d x %d"
            , camctx->conf->width
            , camctx->conf->height
            , config->at(0).size.width
            , config->at(0).size.height);
    }

    camctx->imgs.width = config->at(0).size.width;
    camctx->imgs.height = config->at(0).size.height;
    camctx->imgs.size_norm = (camctx->imgs.width * camctx->imgs.height * 3) / 2;
    camctx->imgs.motionsize = camctx->imgs.width * camctx->imgs.height;

    cam_log_transform();
    cam_log_controls();
    cam_log_draft();

    cam_config_transform();
    cam_config_controls();

    camera->configure(config.get());

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

int cls_libcam::req_add(Request *request)
{
    int retcd;
    retcd = camera->queueRequest(request);
    return retcd;
}

int cls_libcam::cam_start_req()
{
    int retcd, bytes, indx, width;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    camera->requestCompleted.connect(this, &cls_libcam::req_complete);
    frmbuf = std::make_unique<FrameBufferAllocator>(camera);

    retcd = frmbuf->allocate(config->at(0).stream());
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Buffer allocation error.");
        return -1;
    }

    std::unique_ptr<Request> request = camera->createRequest();
    if (!request) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Create request error.");
        return -1;
    }

    Stream *stream = config->at(0).stream();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers =
        frmbuf->buffers(stream);
    const std::unique_ptr<FrameBuffer> &buffer = buffers[0];

    retcd = request->addBuffer(stream, buffer.get());
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Add buffer for request error.");
        return -1;
    }

    started_req = true;

    const FrameBuffer::Plane &plane0 = buffer->planes()[0];

    bytes = 0;
    for (indx=0; indx<(int)buffer->planes().size(); indx++){
        bytes += buffer->planes()[indx].length;
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "Plane %d of %d length %d"
            , indx, buffer->planes().size()
            , buffer->planes()[indx].length);
    }

    if (bytes > camctx->imgs.size_norm) {
        width = (buffer->planes()[0].length / camctx->imgs.height);
        if (((int)buffer->planes()[0].length != (width * camctx->imgs.height)) ||
            (bytes > ((width * camctx->imgs.height * 3)/2))) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                , "Error setting image size.  Plane 0 length %d, total bytes %d"
                , buffer->planes()[0].length, bytes);
        }
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            , "Image size adjusted from %d x %d to %d x %d"
            , camctx->imgs.width,camctx->imgs.height
            , width,camctx->imgs.height);
        camctx->imgs.width = width;
        camctx->imgs.size_norm = (camctx->imgs.width * camctx->imgs.height * 3) / 2;
        camctx->imgs.motionsize = camctx->imgs.width * camctx->imgs.height;
    }

    membuf.buf = (uint8_t *)mmap(NULL, bytes, PROT_READ
        , MAP_SHARED, plane0.fd.get(), 0);
    membuf.bufsz = bytes;

    requests.push_back(std::move(request));

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

int cls_libcam::cam_start_capture()
{
    int retcd;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Starting.");

    retcd = camera->start(&this->controls);
    if (retcd) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Failed to start capture.");
        return -1;
    }
    controls.clear();

    for (std::unique_ptr<Request> &request : requests) {
        retcd = req_add(request.get());
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                , "Failed to queue request.");
            if (started_cam) {
                camera->stop();
            }
            return -1;
        }
    }
    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Finished.");

    return 0;
}

void cls_libcam::req_complete(Request *request)
{
    if (request->status() == Request::RequestCancelled) {
        return;
    }
    req_queue.push(request);
}

int cls_libcam::cam_start(ctx_dev *cam)
{
    int retcd;

    started_cam = false;
    started_mgr = false;
    started_aqr = false;
    started_req = false;

    cam_start_params(cam);

    retcd = cam_start_mgr();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_config();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_req();
    if (retcd != 0) {
        return -1;
    }

    retcd = cam_start_capture();
    if (retcd != 0) {
        return -1;
    }

    SLEEP(2,0);

    started_cam = true;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Started all.");

    return 0;
}

void cls_libcam::cam_stop()
{
    util_parms_free(camctx->libcam->params);
    myfree(&camctx->libcam->params);
    camctx->libcam->params = NULL;

    if (started_aqr) {
        camera->stop();
    }

    if (started_req) {
        camera->requestCompleted.disconnect(this, &cls_libcam::req_complete);
        while (req_queue.empty() == false) {
            req_queue.pop();
        }
        requests.clear();

        frmbuf->free(config->at(0).stream());
        frmbuf.reset();
    }

    controls.clear();

    if (started_aqr){
        camera->release();
        camera.reset();
    }
    if (started_mgr) {
        cam_mgr->stop();
        cam_mgr.reset();
    }
    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Stopped.");
}

/* get the image from libcam */
int cls_libcam::cam_next(ctx_image_data *img_data)
{
    int indx;

    if (started_cam == false) {
        return CAPTURE_FAILURE;
    }

    /* Allow time for request to finish.*/
    indx=0;
    while ((req_queue.empty() == true) && (indx < 50)) {
        SLEEP(0,2000)
        indx++;
    }

    if (req_queue.empty() == false) {
        Request *request = this->req_queue.front();

        memcpy(img_data->image_norm, membuf.buf, membuf.bufsz);

        this->req_queue.pop();

        request->reuse(Request::ReuseBuffers);
        req_add(request);
        return CAPTURE_SUCCESS;

    } else {
        return CAPTURE_FAILURE;
    }
}

#endif

/** close and stop libcam */
void libcam_cleanup(ctx_dev *cam)
{
    #ifdef HAVE_LIBCAM
        cam->libcam->cam_stop();
        delete cam->libcam;
        cam->libcam = nullptr;
    #endif
    cam->device_status = STATUS_CLOSED;
}

/** initialize and start libcam */
void libcam_start(ctx_dev *cam)
{
    #ifdef HAVE_LIBCAM
        int retcd;
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening libcam"));
        cam->libcam = new cls_libcam;
        retcd = cam->libcam->cam_start(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("libcam failed to open"));
            libcam_cleanup(cam);
        } else {
            cam->device_status = STATUS_OPENED;
        }
    #else
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("libcam not available"));
        cam->device_status = STATUS_CLOSED;
    #endif
}


/** get next image from libcam */
int libcam_next(ctx_dev *cam,  ctx_image_data *img_data)
{
    #ifdef HAVE_LIBCAM
        int retcd;

        if (cam->libcam == nullptr){
            return CAPTURE_FAILURE;
        }

        retcd = cam->libcam->cam_next(img_data);
        if (retcd == CAPTURE_SUCCESS) {
            rotate_map(cam, img_data);
        }

        return retcd;
    #else
        (void)cam;
        (void)img_data;
        return CAPTURE_FAILURE;
    #endif
}

