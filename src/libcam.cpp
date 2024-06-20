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
#include "libcam.hpp"

#ifdef HAVE_LIBCAM

using namespace libcamera;

void cls_libcam::cam_log_orientation()
{
    #if (LIBCAMVER >= 2000)
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "Libcamera Orientation Options:");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate0");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate0Mirror");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate180");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate180Mirror");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate90");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate90Mirror");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate270");
        motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Rotate270Mirror");
    #else
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Orientation Not available");
    #endif

}

void cls_libcam::cam_log_controls()
{

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "Libcamera Controls:");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeEnable(bool)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeLocked(bool)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeMeteringMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    MeteringCentreWeighted = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    MeteringSpot = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    MeteringMatrix = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    MeteringCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeConstraintMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ConstraintNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ConstraintHighlight = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ConstraintShadows = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ConstraintCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeExposureMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ExposureNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ExposureShort = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ExposureLong = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ExposureCustom = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ExposureValue(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ExposureTime(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AnalogueGain(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Brightness(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Contrast(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Lux(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AwbEnable(bool)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AwbMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbAuto = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbIncandescent = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbTungsten = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbFluorescent = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbIndoor = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbDaylight = 5");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbCloudy = 6");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbCustom = 7");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AwbLocked(bool)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ColourGains(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     Red | Blue");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ColourTemperature(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Saturation(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  SensorBlackLevels(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     var1|var2|var3|var4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  Sharpness(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  FocusFoM(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ColourCorrectionMatrix(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     var1|var2|...|var8|var9");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ScalerCrop(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     x | y | h | w");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  DigitalGain(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  FrameDuration(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  FrameDurationLimits(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     min | max");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  SensorTemperature(float)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  SensorTimestamp(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfModeManual = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfModeAuto = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfModeContinuous = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfRange(0-2)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfRangeNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfRangeMacro = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfRangeFull = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfSpeed(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfSpeedNormal = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfSpeedFast = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfMetering(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfMeteringAuto = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfMeteringWindows = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfWindows(Pipe delimited)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "     x | y | h | w");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfTrigger(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfTriggerStart = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfTriggerCancel = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfPause(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseImmediate = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseDeferred = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseResume = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  LensPosition(float)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfStateIdle = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfStateScanning = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfStateFocused = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfStateFailed = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AfPauseState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseStateRunning = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseStatePausing = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AfPauseStatePaused = 2");

}

void cls_libcam:: cam_log_draft()
{
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "Libcamera Controls Draft:");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AePrecaptureTrigger(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AePrecaptureTriggerIdle = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AePrecaptureTriggerStart = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AePrecaptureTriggerCancel = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  NoiseReductionMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    NoiseReductionModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    NoiseReductionModeFast = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    NoiseReductionModeHighQuality = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    NoiseReductionModeMinimal = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    NoiseReductionModeZSL = 4");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  ColorCorrectionAberrationMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ColorCorrectionAberrationOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ColorCorrectionAberrationFast = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    ColorCorrectionAberrationHighQuality = 2");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AeState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AeStateSearching = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AeStateConverged = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AeStateLocked = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AeStateFlashRequired = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AeStatePrecapture = 5");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  AwbState(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbStateInactive = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbStateSearching = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbConverged = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    AwbLocked = 3");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  SensorRollingShutterSkew(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  LensShadingMapMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    LensShadingMapModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    LensShadingMapModeOn = 1");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  PipelineDepth(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  MaxLatency(int)");

    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "  TestPatternMode(int)");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModeOff = 0");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModeSolidColor = 1");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModeColorBars = 2");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModeColorBarsFadeToGray = 3");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModePn9 = 4");
    motpls_log(DBG, TYPE_VIDEO, NO_ERRNO,0,NULL, "    TestPatternModeCustom1 = 256");

}

void cls_libcam::cam_start_params(ctx_dev *ptr)
{
    camctx = ptr;
    p_lst *lst;
    p_it it;

    camctx->libcam->params = new ctx_params;
    camctx->libcam->params->update_params = true;
    util_parms_parse(camctx->libcam->params
        ,"libcam_params", camctx->conf->libcam_params);

    lst = &camctx->libcam->params->params_array;

    for (it = lst->begin(); it != lst->end(); it++) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s : %s"
            ,it->param_name.c_str(), it->param_value.c_str());
    }
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
        if (cam_mgr->cameras().size() == 0) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                , "No camera devices found");
            return -1;
        };
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
/* Set named control to configuration value */
void cls_libcam::cam_config_control_item(std::string pname, std::string pvalue)
{
    if (pname == "AeEnable") {
        controls.set(controls::AeEnable, mtob(pvalue));
    }
    if (pname == "AeLocked") {
        controls.set(controls::AeLocked, mtob(pvalue));
    }
    if (pname == "AeMeteringMode") {
       controls.set(controls::AeMeteringMode, mtoi(pvalue));
    }
    if (pname == "AeConstraintMode") {
        controls.set(controls::AeConstraintMode, mtoi(pvalue));
    }
    if (pname == "AeExposureMode") {
        controls.set(controls::AeExposureMode, mtoi(pvalue));
    }
    if (pname == "ExposureValue") {
        controls.set(controls::ExposureValue, mtof(pvalue));
    }
    if (pname == "ExposureTime") {
        controls.set(controls::ExposureTime, mtoi(pvalue));
    }
    if (pname == "AnalogueGain") {
        controls.set(controls::AnalogueGain, mtof(pvalue));
    }
    if (pname == "Brightness") {
        controls.set(controls::Brightness, mtof(pvalue));
    }
    if (pname == "Contrast") {
        controls.set(controls::Contrast, mtof(pvalue));
    }
    if (pname == "Lux") {
        controls.set(controls::Lux, mtof(pvalue));
    }
    if (pname == "AwbEnable") {
        controls.set(controls::AwbEnable, mtob(pvalue));
    }
    if (pname == "AwbMode") {
        controls.set(controls::AwbMode, mtoi(pvalue));
    }
    if (pname == "AwbLocked") {
        controls.set(controls::AwbLocked, mtob(pvalue));
    }
    if (pname == "ColourGains") {
        float cg[2];
        cg[0] = mtof(mtok(pvalue,"|"));
        cg[1] = mtof(mtok(pvalue,"|"));
        controls.set(controls::ColourGains, cg);
    }
    if (pname == "ColourTemperature") {
        controls.set(controls::ColourTemperature, mtoi(pvalue));
    }
    if (pname == "Saturation") {
        controls.set(controls::Saturation, mtof(pvalue));
    }
    if (pname == "SensorBlackLevels") {
        int32_t sbl[4];
        sbl[0] = mtoi(mtok(pvalue,"|"));
        sbl[1] = mtoi(mtok(pvalue,"|"));
        sbl[2] = mtoi(mtok(pvalue,"|"));
        sbl[3] = mtoi(mtok(pvalue,"|"));
        controls.set(controls::SensorBlackLevels, sbl);
    }
    if (pname == "Sharpness") {
        controls.set(controls::Sharpness, mtof(pvalue));
    }
    if (pname == "FocusFoM") {
        controls.set(controls::FocusFoM, mtoi(pvalue));
    }
    if (pname == "ColourCorrectionMatrix") {
        float ccm[9];
        ccm[0] = mtof(mtok(pvalue,"|"));
        ccm[1] = mtof(mtok(pvalue,"|"));
        ccm[2] = mtof(mtok(pvalue,"|"));
        ccm[3] = mtof(mtok(pvalue,"|"));
        ccm[4] = mtof(mtok(pvalue,"|"));
        ccm[5] = mtof(mtok(pvalue,"|"));
        ccm[6] = mtof(mtok(pvalue,"|"));
        ccm[7] = mtof(mtok(pvalue,"|"));
        ccm[8] = mtof(mtok(pvalue,"|"));
        controls.set(controls::ColourCorrectionMatrix, ccm);
    }
    if (pname == "ScalerCrop") {
        Rectangle crop;
        crop.x = mtoi(mtok(pvalue,"|"));
        crop.y = mtoi(mtok(pvalue,"|"));
        crop.width = mtoi(mtok(pvalue,"|"));
        crop.height = mtoi(mtok(pvalue,"|"));
        controls.set(controls::ScalerCrop, crop);
    }
    if (pname == "DigitalGain") {
        controls.set(controls::DigitalGain, mtof(pvalue));
    }
    if (pname == "FrameDuration") {
        controls.set(controls::FrameDuration, mtoi(pvalue));
    }
    if (pname == "FrameDurationLimits") {
        int64_t fdl[2];
        fdl[0] = mtol(mtok(pvalue,"|"));
        fdl[1] = mtol(mtok(pvalue,"|"));
        controls.set(controls::FrameDurationLimits, fdl);
    }
    if (pname == "SensorTemperature") {
        controls.set(controls::SensorTemperature, mtof(pvalue));
    }
    if (pname == "SensorTimestamp") {
        controls.set(controls::SensorTimestamp, mtoi(pvalue));
    }
    if (pname == "AfMode") {
        controls.set(controls::AfMode, mtoi(pvalue));
    }
    if (pname == "AfRange") {
        controls.set(controls::AfRange, mtoi(pvalue));
    }
    if (pname == "AfSpeed") {
        controls.set(controls::AfSpeed, mtoi(pvalue));
    }
    if (pname == "AfMetering") {
        controls.set(controls::AfMetering, mtoi(pvalue));
    }
    if (pname == "AfWindows") {
        Rectangle afwin[1];
        afwin[0].x = mtoi(mtok(pvalue,"|"));
        afwin[0].y = mtoi(mtok(pvalue,"|"));
        afwin[0].width = mtoi(mtok(pvalue,"|"));
        afwin[0].height = mtoi(mtok(pvalue,"|"));
        controls.set(controls::AfWindows, afwin);
    }
    if (pname == "AfTrigger") {
        controls.set(controls::AfTrigger, mtoi(pvalue));
    }
    if (pname == "AfPause") {
        controls.set(controls::AfPause, mtoi(pvalue));
    }
    if (pname == "LensPosition") {
        controls.set(controls::LensPosition, mtof(pvalue));
    }
    if (pname == "AfState") {
        controls.set(controls::AfState, mtoi(pvalue));
    }
    if (pname == "AfPauseState") {
        controls.set(controls::AfPauseState, mtoi(pvalue));
    }

    /* DRAFT*/
    if (pname == "AePrecaptureTrigger") {
        controls.set(controls::draft::AePrecaptureTrigger, mtoi(pvalue));
    }
    if (pname == "NoiseReductionMode") {
        controls.set(controls::draft::NoiseReductionMode, mtoi(pvalue));
    }
    if (pname == "ColorCorrectionAberrationMode") {
        controls.set(controls::draft::ColorCorrectionAberrationMode, mtoi(pvalue));
    }
    if (pname == "AeState") {
        controls.set(controls::draft::AeState, mtoi(pvalue));
    }
    if (pname == "AwbState") {
        controls.set(controls::draft::AwbState, mtoi(pvalue));
    }
    if (pname == "SensorRollingShutterSkew") {
        controls.set(controls::draft::SensorRollingShutterSkew, mtoi(pvalue));
    }
    if (pname == "LensShadingMapMode") {
        controls.set(controls::draft::LensShadingMapMode, mtoi(pvalue));
    }
    if (pname == "PipelineDepth") {
        controls.set(controls::draft::PipelineDepth, mtoi(pvalue));
    }
    if (pname == "MaxLatency") {
        controls.set(controls::draft::MaxLatency, mtoi(pvalue));
    }
    if (pname == "TestPatternMode") {
        controls.set(controls::draft::TestPatternMode, mtoi(pvalue));
    }

}

void cls_libcam:: cam_config_controls()
{
    int retcd;
    p_lst *lst = &camctx->libcam->params->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        cam_config_control_item(it->param_name, it->param_value);
    }

    retcd = config->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
            , "Configuration controls adjusted.");
    } else if (retcd == CameraConfiguration::Valid) {
         MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Configuration controls valid");
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , "Configuration controls error");
    }

}

void cls_libcam:: cam_config_orientation()
{
    #if (LIBCAMVER >= 2000)
        int retcd;
        std::string adjdesc;
        p_lst *lst = &camctx->libcam->params->params_array;
        p_it it;

        for (it = lst->begin(); it != lst->end(); it++) {
            if (it->param_name == "orientation") {
                if (it->param_value == "Rotate0") {
                    config->orientation = Orientation::Rotate0;
                } else if (it->param_value == "Rotate0Mirror") {
                    config->orientation = Orientation::Rotate0Mirror;
                } else if (it->param_value == "Rotate180") {
                    config->orientation = Orientation::Rotate180;
                } else if (it->param_value == "Rotate180Mirror") {
                    config->orientation = Orientation::Rotate180Mirror;
                } else if (it->param_value == "Rotate90") {
                    config->orientation = Orientation::Rotate90;
                } else if (it->param_value == "Rotate90Mirror") {
                    config->orientation = Orientation::Rotate90Mirror;
                } else if (it->param_value == "Rotate270") {
                    config->orientation = Orientation::Rotate270;
                } else if (it->param_value == "Rotate270Mirror") {
                    config->orientation = Orientation::Rotate270Mirror;
                } else {
                    MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                        , "Invalid Orientation option: %s."
                        , it->param_value.c_str());
                }
            }
        }

        retcd = config->validate();
        if (retcd == CameraConfiguration::Adjusted) {
            if (config->orientation == Orientation::Rotate0) {
                adjdesc = "Rotate0";
            } else if (config->orientation == Orientation::Rotate0Mirror) {
                adjdesc = "Rotate0Mirror";
            } else if (config->orientation == Orientation::Rotate90) {
                adjdesc = "Rotate90";
            } else if (config->orientation == Orientation::Rotate90Mirror) {
            adjdesc = "Rotate90Mirror";
            } else if (config->orientation == Orientation::Rotate180) {
                adjdesc = "Rotate180";
            } else if (config->orientation == Orientation::Rotate180Mirror) {
                adjdesc = "Rotate180Mirror";
            } else if (config->orientation == Orientation::Rotate270) {
                adjdesc = "Rotate270";
            } else if (config->orientation == Orientation::Rotate270Mirror) {
                adjdesc = "Rotate270Mirror";
            } else {
                adjdesc = "unknown";
            }
            MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
                , "Configuration orientation adjusted to %s."
                , adjdesc.c_str());
        } else if (retcd == CameraConfiguration::Valid) {
            MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                , "Configuration orientation valid");
        } else if (retcd == CameraConfiguration::Invalid) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                , "Configuration orientation error");
        }
    #else
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Orientation Not available");
    #endif

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
    config->at(0).stride = 0;

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

    cam_log_orientation();
    cam_log_controls();
    cam_log_draft();

    cam_config_orientation();
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
    delete camctx->libcam->params;

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
            cam->rotate->process(img_data);
        }

        return retcd;
    #else
        (void)cam;
        (void)img_data;
        return CAPTURE_FAILURE;
    #endif
}

