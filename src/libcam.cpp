/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "rotate.hpp"
#include "video_convert.hpp"
#include "libcam.hpp"

#ifdef HAVE_LIBCAM

using namespace libcamera;

int cls_libcam::start_mgr()
{
    int retcd, indx;
    std::string camid;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Starting."));

    retcd = cam_mgr->start();
    if (retcd != 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Error starting camera manager.  Return code: %d"),retcd);
        return retcd;
    }
    started_mgr = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("cam_mgr started."));
    if (cam_mgr->cameras().size() == 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("No camera devices found"));
        return -1;
    }

    for (indx=0;indx<cam_mgr->cameras().size();indx++) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Available libcam_device '%s'")
            , cam_mgr->cameras()[indx]->id().c_str());
    }
    if (cam->cfg->libcam_device == "camera0"){
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("camera0 specified as device.  Using the first device found. %s")
            , cam_mgr->cameras()[0]->id().c_str());
        camid = cam_mgr->cameras()[0]->id();
    } else {
        camid = "";
        for (indx=0;indx<cam_mgr->cameras().size();indx++) {
            if (cam->cfg->libcam_device ==
                cam_mgr->cameras()[indx]->id()) {
                camid = cam_mgr->cameras()[0]->id();
            }
        }
        if (camid == "") {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Invalid libcam_device '%s'.  See log message for available devices")
                ,cam->cfg->libcam_device.c_str());
            return -1;
        }
    }
    camera = cam_mgr->get(camid);
    camera->acquire();
    started_aqr = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Finished."));

    return 0;
}

void cls_libcam::config_assign_float(int pctrl, std::string pvalue)
{
    float f_var, f_min, f_max, f_def;

    auto fdctrl = camera->controls().find(pctrl);
    if (fdctrl == camera->controls().end()) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Control not found"));
        return;
    }
    f_min = fdctrl->second.min().get<float>();
    f_max = fdctrl->second.max().get<float>();
    f_def = fdctrl->second.def().get<float>();

    if (pvalue == "default") {
        camctrls->set(pctrl, f_def);
    } else {
        f_var = mtof(pvalue);
        if ((f_var < f_min) || (f_var >f_max)) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Invalid %s value %6.4f")
                ,fdctrl->first->name().c_str(), f_var);
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("%s Default %6.4f and range %6.4f to %6.4f.")
                ,fdctrl->first->name().c_str(), f_def, f_min, f_max);
        } else {
            camctrls->set(pctrl, f_var);
        }
    }
}

void cls_libcam::config_assign_int(int pctrl, std::string pvalue)
{
    int i_var, i_min, i_max, i_def;

    auto fdctrl = camera->controls().find(pctrl);
    if (fdctrl == camera->controls().end()) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Control id %d not found. value=%s"), pctrl,pvalue.c_str());
        return;
    }

    i_min = fdctrl->second.min().get<int>();
    i_max = fdctrl->second.max().get<int>();
    i_def = fdctrl->second.def().get<int>();

    if (pvalue == "default") {
        camctrls->set(pctrl, i_def);
    } else {
        i_var = mtoi(pvalue);
        if ((i_var < i_min) || (i_var > i_max)) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Invalid %s value %6.4f")
                ,fdctrl->first->name().c_str(), i_var);
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("%s Default %6.4f and range %6.4f to %6.4f.")
                ,fdctrl->first->name().c_str(), i_def, i_min, i_max);
        } else {
            camctrls->set(pctrl, i_var);
        }
    }
}

void cls_libcam::config_control_item(std::string pnm, std::string pvalue)
{
    std::string pname;
    pname = pnm;
    mylower(pname);

    if (pname == "aemeteringmode") {
       config_assign_int(libcamera::controls::AeMeteringMode.id(), pvalue);
    } else if (pname == "aeconstraintmode") {
        config_assign_int(libcamera::controls::AeConstraintMode.id(), pvalue);
    } else if (pname == "aeexposuremode") {
        config_assign_int(libcamera::controls::AeExposureMode.id(), pvalue);
    } else if (pname == "exposurevalue") {
        config_assign_float(libcamera::controls::ExposureValue.id(), pvalue);
    } else if (pname == "exposuretime") {
        config_assign_int(libcamera::controls::ExposureTime.id(), pvalue);
    } else if (pname == "analoguegain") {
        config_assign_float(libcamera::controls::AnalogueGain.id(), pvalue);
    } else if (pname == "brightness") {
        config_assign_float(libcamera::controls::Brightness.id(), pvalue);
    } else if (pname == "contrast") {
        config_assign_float(libcamera::controls::Contrast.id(), pvalue);
    } else if (pname == "lux") {
        config_assign_float(libcamera::controls::Lux.id(), pvalue);
    } else if (pname == "awbenable") {
        camctrls->set(controls::AwbEnable, mtob(pvalue));
    } else if (pname == "awbmode") {
        config_assign_int(libcamera::controls::AwbMode.id(), pvalue);
    } else if (pname == "awblocked") {
        camctrls->set(controls::AwbLocked, mtob(pvalue));
    } else if (pname == "colourgains") {
        float cg[2];
        cg[0] = mtof(mtok(pvalue,"|"));
        cg[1] = mtof(mtok(pvalue,"|"));
        camctrls->set(controls::ColourGains, cg);
    } else if (pname == "colourtemperature") {
        camctrls->set(controls::ColourTemperature, mtoi(pvalue));
    } else if (pname == "saturation") {
        config_assign_float(libcamera::controls::Saturation.id(), pvalue);
    } else if (pname == "sensorblacklevels") {
        int32_t sbl[4];
        sbl[0] = mtoi(mtok(pvalue,"|"));
        sbl[1] = mtoi(mtok(pvalue,"|"));
        sbl[2] = mtoi(mtok(pvalue,"|"));
        sbl[3] = mtoi(mtok(pvalue,"|"));
        camctrls->set(controls::SensorBlackLevels, sbl);
    } else if (pname == "sharpness") {
        config_assign_float(libcamera::controls::Sharpness.id(), pvalue);
    } else if (pname == "focusfom") {
        config_assign_int(libcamera::controls::FocusFoM.id(), pvalue);
    } else if (pname == "colourcorrectionmatrix") {
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
        camctrls->set(controls::ColourCorrectionMatrix, ccm);
    } else if (pname == "scalercrop") {
        Rectangle crop;
        crop.x = mtoi(mtok(pvalue,"|"));
        crop.y = mtoi(mtok(pvalue,"|"));
        crop.width =(uint)mtoi(mtok(pvalue,"|"));
        crop.height =(uint)mtoi(mtok(pvalue,"|"));
        camctrls->set(controls::ScalerCrop, crop);
    } else if (pname == "digitalgain") {
        config_assign_float(libcamera::controls::DigitalGain.id(), pvalue);
    } else if (pname == "frameduration") {
        config_assign_int(libcamera::controls::FrameDuration.id(), pvalue);
    } else if (pname == "framedurationlimits") {
        int64_t fdl[2];
        fdl[0] = mtol(mtok(pvalue,"|"));
        fdl[1] = mtol(mtok(pvalue,"|"));
        camctrls->set(controls::FrameDurationLimits, fdl);
    } else if (pname == "sensortemperature") {
        config_assign_float(libcamera::controls::SensorTemperature.id(), pvalue);
    } else if (pname == "sensortimestamp") {
        config_assign_int(libcamera::controls::SensorTimestamp.id(), pvalue);
    } else if (pname == "afmode") {
        config_assign_int(libcamera::controls::AfMode.id(), pvalue);
    } else if (pname == "afrange") {
        config_assign_int(libcamera::controls::AfRange.id(), pvalue);
    } else if (pname == "afspeed") {
        config_assign_int(libcamera::controls::AfSpeed.id(), pvalue);
    } else if (pname == "afmetering") {
        config_assign_int(libcamera::controls::AfMetering.id(), pvalue);
    } else if (pname == "afwindows") {
        Rectangle afwin[1];
        afwin[0].x = mtoi(mtok(pvalue,"|"));
        afwin[0].y = mtoi(mtok(pvalue,"|"));
        afwin[0].width = (uint)mtoi(mtok(pvalue,"|"));
        afwin[0].height = (uint)mtoi(mtok(pvalue,"|"));
        camctrls->set(controls::AfWindows, afwin);
    } else if (pname == "aftrigger") {
        config_assign_int(libcamera::controls::AfTrigger.id(), pvalue);
    } else if (pname == "afpause") {
        config_assign_int(libcamera::controls::AfPause.id(), pvalue);
    } else if (pname == "lensposition") {
        config_assign_float(libcamera::controls::LensPosition.id(), pvalue);
    } else if (pname == "afstate") {
        config_assign_int(libcamera::controls::AfState.id(), pvalue);
    } else if (pname == "afpausestate") {
        config_assign_int(libcamera::controls::AfPauseState.id(), pvalue);

    /* DRAFT*/
    } else if (pname == "aeprecapturetrigger") {
        config_assign_int(libcamera::controls::draft::AePrecaptureTrigger.id(), pvalue);
    } else if (pname == "noisereductionmode") {
        config_assign_int(libcamera::controls::draft::NoiseReductionMode.id(), pvalue);
    } else if (pname == "colorcorrectionaberrationmode") {
        config_assign_int(libcamera::controls::draft::ColorCorrectionAberrationMode.id(), pvalue);
    } else if (pname == "awbstate") {
        config_assign_int(libcamera::controls::draft::AwbState.id(), pvalue);
    } else if (pname == "sensorrollingshutterskew") {
        config_assign_int(libcamera::controls::draft::SensorRollingShutterSkew.id(), pvalue);
    } else if (pname == "lensshadingmapmode") {
        config_assign_int(libcamera::controls::draft::LensShadingMapMode.id(), pvalue);
    } else if (pname == "pipelinedepth") {
        config_assign_int(libcamera::controls::draft::PipelineDepth.id(), pvalue);
    } else if (pname == "maxlatency") {
        config_assign_int(libcamera::controls::draft::MaxLatency.id(), pvalue);
    } else if (pname == "testpatternmode") {
        config_assign_int(libcamera::controls::draft::TestPatternMode.id(), pvalue);
    } else {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Unknown libcamera control %s"), pnm.c_str());
    }

}

void cls_libcam:: config_orientation()
{
    #if (LIBCAMVER >= 2000)
        int retcd, indx;
        std::string adjdesc;
        ctx_params_item *itm;

        for (indx=0;indx<params->params_cnt;indx++) {
            itm = &params->params_array[indx];
            if ((itm->param_name == "Orientation") &&
                (itm->param_source == PARM_SRC_USER)) {
                if (itm->param_value == "Rotate0") {
                    libcam_cfg->orientation = Orientation::Rotate0;
                } else if (itm->param_value == "Rotate0Mirror") {
                    libcam_cfg->orientation = Orientation::Rotate0Mirror;
                } else if (itm->param_value == "Rotate180") {
                    libcam_cfg->orientation = Orientation::Rotate180;
                } else if (itm->param_value == "Rotate180Mirror") {
                    libcam_cfg->orientation = Orientation::Rotate180Mirror;
                } else if (itm->param_value == "Rotate90") {
                    libcam_cfg->orientation = Orientation::Rotate90;
                } else if (itm->param_value == "Rotate90Mirror") {
                    libcam_cfg->orientation = Orientation::Rotate90Mirror;
                } else if (itm->param_value == "Rotate270") {
                    libcam_cfg->orientation = Orientation::Rotate270;
                } else if (itm->param_value == "Rotate270Mirror") {
                    libcam_cfg->orientation = Orientation::Rotate270Mirror;
                } else {
                    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                        ,_("Invalid Orientation option: %s.")
                        , itm->param_value.c_str());
                }
            }
        }

        retcd = libcam_cfg->validate();
        if (retcd == CameraConfiguration::Adjusted) {
            if (libcam_cfg->orientation == Orientation::Rotate0) {
                adjdesc = "Rotate0";
            } else if (libcam_cfg->orientation == Orientation::Rotate0Mirror) {
                adjdesc = "Rotate0Mirror";
            } else if (libcam_cfg->orientation == Orientation::Rotate90) {
                adjdesc = "Rotate90";
            } else if (libcam_cfg->orientation == Orientation::Rotate90Mirror) {
            adjdesc = "Rotate90Mirror";
            } else if (libcam_cfg->orientation == Orientation::Rotate180) {
                adjdesc = "Rotate180";
            } else if (libcam_cfg->orientation == Orientation::Rotate180Mirror) {
                adjdesc = "Rotate180Mirror";
            } else if (libcam_cfg->orientation == Orientation::Rotate270) {
                adjdesc = "Rotate270";
            } else if (libcam_cfg->orientation == Orientation::Rotate270Mirror) {
                adjdesc = "Rotate270Mirror";
            } else {
                adjdesc = "unknown";
            }
            MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
                ,_("Configuration orientation adjusted to %s.")
                , adjdesc.c_str());
        } else if (retcd == CameraConfiguration::Valid) {
            MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                ,_("Configuration orientation valid"));
        } else if (retcd == CameraConfiguration::Invalid) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Configuration orientation error"));
        }
    #else
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Orientation Not available"));
    #endif

}

int cls_libcam::start_config()
{
    int retcd, indx, bufcnt;
    char tmp4cc[5];
    std::string pfmt, tmpfmt;
    StreamFormats allfmts;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Starting."));

    libcam_cfg = camera->generateConfiguration({ StreamRole::Viewfinder });

    allfmts = libcam_cfg->at(0).formats();
    for (indx=0;indx<allfmts.pixelformats().size();indx++){
        tmpfmt = allfmts.pixelformats()[indx].toString();
        sprintf(tmp4cc,"%c%c%c%c"
            , allfmts.pixelformats()[indx].fourcc() >> 0
            , allfmts.pixelformats()[indx].fourcc() >> 8
            , allfmts.pixelformats()[indx].fourcc() >> 16
            , allfmts.pixelformats()[indx].fourcc() >> 24);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Available pixel format: %s (FourCC %s)")
                , tmpfmt.c_str(), tmp4cc);
    }

    pfmt = "YUV420";
    bufcnt = 4;
    for (indx=0;indx<params->params_cnt;indx++) {
        if ((params->params_array[indx].param_name == "pixelformat") &&
            (params->params_array[indx].param_source == PARM_SRC_USER)) {
            pfmt = params->params_array[indx].param_value;
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("User specified pixelformat %s.")
                , pfmt.c_str());
        }
        if (params->params_array[indx].param_name == "buffer_count") {
            bufcnt = mtoi(params->params_array[indx].param_value);
        }
    }

    for (indx=0;indx<allfmts.pixelformats().size();indx++){
        tmpfmt = allfmts.pixelformats()[indx].toString();
        if (pfmt == tmpfmt) {
            break;
        }
    }
    if (pfmt != tmpfmt) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Did not find pixelformat %s"), pfmt.c_str());
        pfmt="";
        for (indx=0;indx<allfmts.pixelformats().size();indx++) {
            sprintf(tmp4cc,"%c%c%c%c"
                , allfmts.pixelformats()[indx].fourcc() >> 0
                , allfmts.pixelformats()[indx].fourcc() >> 8
                , allfmts.pixelformats()[indx].fourcc() >> 16
                , allfmts.pixelformats()[indx].fourcc() >> 24);
            tmpfmt = tmp4cc;
            if ((tmpfmt == "I420")||(tmpfmt == "YU12")||(tmpfmt == "IYUV")) {
                pfmt=allfmts.pixelformats()[indx].toString();
                break;
            }
        }
        if (pfmt == "") {
            /*mjpeg doesn't seem to work very well so we pick the first alternative*/
            for (indx=0;indx<allfmts.pixelformats().size();indx++) {
                tmpfmt = allfmts.pixelformats()[indx].toString();
                if (tmpfmt != "MJPEG") {
                    pfmt = tmpfmt;
                    break;
                }
            }
        }
        if (pfmt == "") {
            pfmt = "MJPEG";
        }
    }

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
        ,_("Trying pixel format %s"), pfmt.c_str());

    libcam_cfg->at(0).pixelFormat = PixelFormat::fromString(pfmt);
    libcam_cfg->at(0).size.width = (uint)cam->cfg->width;
    libcam_cfg->at(0).size.height = (uint)cam->cfg->height;
    libcam_cfg->at(0).bufferCount = bufcnt;
    libcam_cfg->at(0).stride = 0;

    retcd = libcam_cfg->validate();
    if (retcd == CameraConfiguration::Adjusted) {
        if (libcam_cfg->at(0).pixelFormat.toString() != pfmt) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Pixel format was adjusted to %s.")
                , libcam_cfg->at(0).pixelFormat.toString().c_str());
        } else {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Configuration adjusted: %s")
                ,libcam_cfg->at(0).toString().c_str());
        }
    } else if (retcd == CameraConfiguration::Valid) {
         MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Configuration is valid"));
    } else if (retcd == CameraConfiguration::Invalid) {
         MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Error setting configuration"));
        return -1;
    }

    if (bufcnt != (int)libcam_cfg->at(0).bufferCount) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("Configuration adjusted: buffer count from %d to %d")
            ,bufcnt, libcam_cfg->at(0).bufferCount);
    }

    sprintf(tmp4cc,"%c%c%c%c"
        , libcam_cfg->at(0).pixelFormat.fourcc() >> 0
        , libcam_cfg->at(0).pixelFormat.fourcc() >> 8
        , libcam_cfg->at(0).pixelFormat.fourcc() >> 16
        , libcam_cfg->at(0).pixelFormat.fourcc() >> 24);
    pixfmt = tmp4cc;

    if ((libcam_cfg->at(0).size.width != (uint)cam->cfg->width) ||
        (libcam_cfg->at(0).size.height != (uint)cam->cfg->height)) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Image size adjusted from %dx%d to %dx%d")
            , cam->cfg->width, cam->cfg->height
            , libcam_cfg->at(0).size.width, libcam_cfg->at(0).size.height);
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Pixelformat %s Image size %dx%d")
        , pixfmt.c_str()
        , libcam_cfg->at(0).size.width
        , libcam_cfg->at(0).size.height);

    cam->imgs.width = (int)libcam_cfg->at(0).size.width;
    cam->imgs.height = (int)libcam_cfg->at(0).size.height;
    cam->imgs.size_norm = (cam->imgs.width * cam->imgs.height * 3) / 2;
    cam->imgs.motionsize = cam->imgs.width * cam->imgs.height;

    config_orientation();
    camera->configure(libcam_cfg.get());

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Finished."));

    return 0;
}

int cls_libcam::req_add(Request *request)
{
    int retcd, indx;

    if (set_controls) {
        camctrls = &request->controls();
        for (indx=0;indx<params->params_cnt;indx++) {
            if ((params->params_array[indx].param_name == "params_file") ||
                (params->params_array[indx].param_name == "pixelformat") ||
                (params->params_array[indx].param_name == "buffer_count") ||
                (params->params_array[indx].param_name == "Orientation")) {
                continue;
            } else if (params->params_array[indx].param_source == PARM_SRC_USER) {
                config_control_item(
                    params->params_array[indx].param_name
                    ,params->params_array[indx].param_value);
            }
        }
        set_controls = false;
    }

    retcd = camera->queueRequest(request);
    return retcd;
}

int cls_libcam::start_req()
{
    int retcd, bytes, indx, indx2;
    ctx_imgmap  mbuf;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Starting."));

    camera->requestCompleted.connect(this, &cls_libcam::req_complete);

    libcamera::Stream *stream = libcam_cfg->at(0).stream();
    frmbuf = std::make_unique<FrameBufferAllocator>(camera);

    retcd = frmbuf->allocate(stream);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Buffer allocation error."));
        return -1;
    }
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = frmbuf->buffers(stream);
    for (indx=0;indx<buffers.size();indx++) {
        std::unique_ptr<Request> request = camera->createRequest(indx);
        if (request == nullptr) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Create request error."));
            return -1;
        }
        const libcamera::Stream *strm = libcam_cfg->at(0).stream();
        retcd = request->addBuffer(strm, buffers[indx].get());
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Add buffer for request error."));
            return -1;
        }
        const FrameBuffer::Plane &plane0 = buffers[indx]->planes()[0];
        bytes = 0;
        for (indx2=0; indx2<(int)buffers[indx]->planes().size(); indx2++) {
            bytes += buffers[indx]->planes()[(uint)indx2].length;
            MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                ,_("Buffer %d plane %d of %d length %d")
                , indx, indx2, buffers[indx]->planes().size()
                , buffers[indx]->planes()[(uint)indx2].length);

        }
        mbuf.buf = (uint8_t *)mmap(NULL, (uint)bytes, PROT_READ
            , MAP_SHARED, plane0.fd.get(), 0);
        mbuf.bufsz = bytes;
        mbuf.id = indx;
        membuf.push_back(mbuf);
        requests.push_back(std::move(request));

    }

    convert = new cls_convert(cam, pixfmt, cam->imgs.width, cam->imgs.height);

    started_req = true;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Finished."));

    return 0;
}

int cls_libcam::start_capture()
{
    int retcd, indx;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Starting."));

    camctrls = new libcamera::ControlList();
    for (indx=0;indx<params->params_cnt;indx++) {
        if ((params->params_array[indx].param_name == "params_file") ||
            (params->params_array[indx].param_name == "pixelformat") ||
            (params->params_array[indx].param_name == "buffer_count") ||
            (params->params_array[indx].param_name == "Orientation")) {
            continue;
        } else if (params->params_array[indx].param_source == PARM_SRC_USER) {
            config_control_item(
                params->params_array[indx].param_name
                ,params->params_array[indx].param_value);
        }
    }
    retcd = camera->start(camctrls);
    if (retcd) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Failed to start capture."));
        mydelete(camctrls);
        return -1;
    }
    mydelete(camctrls);
    started_cam = true;

    for (std::unique_ptr<Request> &request : requests) {
        retcd = req_add(request.get());
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Failed to queue request."));
            if (started_cam) {
                camera->stop();
                started_cam = false;
            }
            return -1;
        }
    }
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Finished."));

    return 0;
}

void cls_libcam::req_complete(Request *request)
{
    if (request->status() == Request::RequestCancelled) {
        return;
    }
    req_queue.push(request);
}

int cls_libcam::libcam_start()
{
    started_cam = false;
    started_mgr = false;
    started_aqr = false;
    started_req = false;
    set_controls = true;

    if (start_mgr() != 0) {
        return -1;
    }
    if (start_config() != 0) {
        return -1;
    }
    if (start_req() != 0) {
        return -1;
    }
    if (start_capture() != 0) {
        return -1;
    }

    cam->watchdog = cam->cfg->watchdog_tmo;
    SLEEP(1,0);

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Camera started"));

    return 0;
}

void cls_libcam::libcam_stop()
{
    int indx;
    mydelete(convert);

    if (started_cam) {
        camera->requestCompleted.disconnect(this, &cls_libcam::req_complete);
        while (req_queue.empty() == false) {
            req_queue.pop();
        }
        camera->stop();
    }

    if (started_req) {
        for (indx=0;indx<membuf.size();indx++){
            munmap(membuf[indx].buf, membuf[indx].bufsz);
        }
        membuf.clear();

        for (indx=0;indx<requests.size();indx++) {
            requests[indx].reset();
        }
        requests.clear();
        frmbuf->free(libcam_cfg->at(0).stream());
        frmbuf.reset();
        started_req = false;
    }

    if (started_cam) {
        camera->release();
        camera.reset();
        started_cam = false;
    }

    if (started_mgr) {
        cam_mgr->stop();
        started_mgr = false;
    }

    cam->device_status = STATUS_CLOSED;
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Camera stopped."));
}

#endif

void cls_libcam::params_update()
{
    #ifdef HAVE_LIBCAM
        ctx_params  *prm_s;
        int indx, indx2;
        bool chgnew;
        ctx_params_item *itm_s;

        MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, _("Updating libcam config"));

        prm_s = nullptr;
        for (indx=0;indx<cam->conf_src->params.size();indx++) {
            if (cam->conf_src->params[indx]->params_pcat == PARM_PCAT_LIB) {
                prm_s = cam->conf_src->params[indx];
            }
        }
        util_parms_parse(prm_s, "libcam_params", cam->conf_src->libcam_params,PARM_PCAT_LIB);

        for (indx=0;indx<prm_s->params_cnt;indx++) {
            chgnew = true;
            itm_s = &prm_s->params_array[indx];
            for (indx2=0;indx2<params->params_cnt;indx2++) {
                if ((itm_s->param_name == params->params_array[indx2].param_name) &&
                    (itm_s->param_value == params->params_array[indx2].param_value)) {
                    chgnew = false;
                }
            }
            if (chgnew == true) {
                util_parms_add(params,itm_s->param_name,itm_s->param_value);
                if ((itm_s->param_name == "pixelformat") ||
                    (itm_s->param_name == "buffer_count") ||
                    (itm_s->param_name == "Orientation")) {
                    MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO
                        , _("Restarting camera. Parameter %s changed to %s")
                        , itm_s->param_name.c_str(),itm_s->param_value.c_str());
                    cam->restart = true;
                }
            }
        }

        set_controls = true;    /* Trigger update on next req_add*/

    #endif
}

void cls_libcam::noimage()
{
    #ifdef HAVE_LIBCAM
        int slp_dur;

        if (reconnect_count < 100) {
            reconnect_count++;
        } else {
            if (reconnect_count >= 500) {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 2 hours."));
                slp_dur = 7200;
            } else if (reconnect_count >= 200) {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 10 minutes."));
                reconnect_count++;
                slp_dur = 600;
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 30 seconds."));
                reconnect_count++;
                slp_dur = 30;
            }
            cam->watchdog = slp_dur + (cam->cfg->watchdog_tmo * 3);
            while (slp_dur > 0){
                SLEEP(10,0);
                slp_dur -= 10;
                if (cam->finish == true) {
                    libcam_stop();
                    cam->watchdog = cam->cfg->watchdog_tmo;
                    return;
                }
            }
            libcam_stop();
            if (libcam_start() < 0) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("libcam failed to open"));
                libcam_stop();
            } else {
                cam->device_status = STATUS_OPENED;
            }
        }
    #endif
}

int cls_libcam::next(ctx_image_data *img_data)
{
    #ifdef HAVE_LIBCAM
        int indx, retcd;
        static int chkcnt=0;

        if (started_cam == false) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Unable to get image.  Camera not started."));
            return CAPTURE_FAILURE;
        }

        cam->watchdog = cam->cfg->watchdog_tmo;
        if (req_queue.empty() == false) {
            Request *request = this->req_queue.front();
            indx=(int)request->cookie();
            if (indx == membuf[indx].id) {
                retcd = convert->process(img_data->image_norm
                    , membuf[indx].buf, membuf[indx].bufsz);
                if (retcd < 0) {
                    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Unable to process image."));
                    return CAPTURE_FAILURE;
                }
            } else {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("index, ID and cookie mismatch."));
            }
            this->req_queue.pop();
            request->reuse(Request::ReuseBuffers);
            req_add(request);

            cam->rotate->process(img_data);
            reconnect_count = 0;
            chkcnt=0;

            return CAPTURE_SUCCESS;

        } else {
            if (chkcnt > 1000) {
                MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                    ,_("Excessive empty queue requests: %d"), chkcnt);
                chkcnt=0;
            }
            chkcnt++;
            return CAPTURE_FAILURE;
        }
    #else
        (void)img_data;
        return CAPTURE_FAILURE;
    #endif
}

cls_libcam::cls_libcam(cls_camera *p_cam)
{
    cam = p_cam;
    #ifdef HAVE_LIBCAM
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening libcam"));
        int indx;
        convert = nullptr;
        params = nullptr;
        for (indx=0;indx<cam->cfg->params.size();indx++) {
            if (cam->cfg->params[indx]->params_pcat == PARM_PCAT_LIB) {
                params = cam->cfg->params[indx];
            }
        }
        reconnect_count = 0;
        cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbitrary multiplier to give startup more time*/
        cam_mgr = std::make_unique<CameraManager>();
        if (libcam_start() < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("libcam failed to open"));
            libcam_stop();
        } else {
            cam->device_status = STATUS_OPENED;
        }
    #else
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("libcam not available"));
        cam->device_status = STATUS_CLOSED;
    #endif
}

cls_libcam::~cls_libcam()
{
    #ifdef HAVE_LIBCAM
        libcam_stop();
    #endif

    cam->device_status = STATUS_CLOSED;
}

