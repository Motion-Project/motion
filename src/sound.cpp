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
#include "conf.hpp"
#include "logger.hpp"
#include "sound.hpp"

static void *sound_handler(void *arg)
{
    ((cls_sound *)arg)->handler();

    return nullptr;
}

void cls_sound::init_values()
{
    #ifdef HAVE_FFTW3
        snd_info->snd_fftw->bin_max = 0;
        snd_info->snd_fftw->bin_min = 0;
        snd_info->snd_fftw->bin_size = 0;
        snd_info->snd_fftw->ff_in = NULL;
        snd_info->snd_fftw->ff_out = NULL;
        snd_info->snd_fftw->ff_plan = NULL;
    #endif
    snd_info->sample_rate = 0;
    snd_info->channels = 0;
    snd_info->vol_count = 0;
    snd_info->vol_max = 0;
    snd_info->vol_min = 9999;
    snd_info->buffer = NULL;
    snd_info->buffer_size = 0;
    snd_info->pulse_server = "";
}

void cls_sound::init_alerts(ctx_snd_alert  *tmp_alert)
{
    tmp_alert->alert_id = 0;
    tmp_alert->alert_nm = "";
    tmp_alert->freq_high = 10000;
    tmp_alert->freq_low = 0;
    tmp_alert->volume_count = 0;
    tmp_alert->volume_level = 0;
    tmp_alert->trigger_count = 0;
    tmp_alert->trigger_threshold = 10;
    tmp_alert->trigger_duration = 10;
    clock_gettime(CLOCK_MONOTONIC, &tmp_alert->trigger_time);

}

void cls_sound::edit_alerts()
{
    std::list<ctx_snd_alert>::iterator it_a0;
    std::list<ctx_snd_alert>::iterator it_a1;
    int     indx, id_chk, id_cnt;
    bool    validids;

    validids = true;
    for (it_a0=snd_info->alerts.begin(); it_a0!=snd_info->alerts.end(); it_a0++) {
        id_chk = it_a0->alert_id;
        id_cnt = 0;
        for (it_a1=snd_info->alerts.begin(); it_a1!=snd_info->alerts.end(); it_a1++) {
            if (id_chk == it_a1->alert_id) {
                id_cnt++;
            }
        }
        if (id_cnt > 1) {
            validids = false;
        }
    }

    if (validids == false) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Sound alert ids must be unique.");
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Creating new sound alert ids.");
        indx = 0;
        for (it_a0=snd_info->alerts.begin(); it_a0!=snd_info->alerts.end(); it_a0++) {
            it_a0->alert_id = indx;
            indx++;
        }
    }

    for (it_a0=snd_info->alerts.begin(); it_a0!=snd_info->alerts.end(); it_a0++) {
        if (it_a0->alert_nm == "") {
            it_a0->alert_nm = "sound_alert" + std::to_string(it_a0->alert_id);
        }
        if (it_a0->volume_level < snd_info->vol_min) {
            snd_info->vol_min = it_a0->volume_level;
        }
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Sound Alert Parameters:");
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  alert_id:            %d",it_a0->alert_id);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  alert_nm             %s",it_a0->alert_nm.c_str());
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  freq_low:            %.4f",it_a0->freq_low);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  freq_high:           %.4f",it_a0->freq_high);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  volume_count:        %d",it_a0->volume_count);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  volume_level:        %d",it_a0->volume_level);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  trigger_threshold:   %d",it_a0->trigger_threshold);
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "  trigger_duration:    %d",it_a0->trigger_duration);
    }

}

void cls_sound::load_alerts()
{
    ctx_snd_alert  tmp_alert;
    std::list<std::string> parm_val;
    std::list<std::string>::iterator  it_a;
    ctx_params  *tmp_params;
    ctx_params_item *itm;
    int indx;

    cfg->edit_get("snd_alerts", parm_val, PARM_CAT_18);

    tmp_params = new ctx_params;
    for (it_a=parm_val.begin(); it_a!=parm_val.end(); it_a++) {
        util_parms_parse(tmp_params,"snd_alerts", it_a->c_str());
        init_alerts(&tmp_alert);
        for (indx=0;indx<tmp_params->params_cnt;indx++) {
            itm = &tmp_params->params_array[indx];
            if (itm->param_name == "alert_id") {
                tmp_alert.alert_id = mtoi(itm->param_value);
            }
            if (itm->param_name == "alert_nm") {
                tmp_alert.alert_nm = itm->param_value;
            }
            if (itm->param_name == "freq_low") {
                tmp_alert.freq_low = mtof(itm->param_value);
            }
            if (itm->param_name == "freq_high") {
                tmp_alert.freq_high = mtof(itm->param_value);
            }
            if (itm->param_name == "volume_count") {
                tmp_alert.volume_count = mtoi(itm->param_value);
            }
            if (itm->param_name == "volume_level") {
                tmp_alert.volume_level = mtoi(itm->param_value);
            }
            if (itm->param_name == "trigger_threshold") {
                tmp_alert.trigger_threshold = mtoi(itm->param_value);
            }
            if (itm->param_name == "trigger_duration") {
                tmp_alert.trigger_duration = mtoi(itm->param_value);
            }
        }
        snd_info->alerts.push_back(tmp_alert);
    }

    mydelete(tmp_params);

    edit_alerts();
}

void cls_sound::load_params()
{
    int indx;
    ctx_params_item *itm;

    util_parms_parse(snd_info->params,"snd_params", cfg->snd_params);

    util_parms_add_default(snd_info->params,"source","alsa");
    util_parms_add_default(snd_info->params,"channels","1");
    util_parms_add_default(snd_info->params,"frames","2048");
    util_parms_add_default(snd_info->params,"sample_rate","44100");

    for (indx=0;indx<snd_info->params->params_cnt;indx++) {
        itm = &snd_info->params->params_array[indx];
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s : %s"
            ,itm->param_name.c_str(),itm->param_value.c_str());
    }

    for (indx=0;indx<snd_info->params->params_cnt;indx++) {
        itm = &snd_info->params->params_array[indx];
        if (itm->param_name == "source") {
            snd_info->source = itm->param_value;
        }
        if (itm->param_name == "channels") {
            snd_info->channels = mtoi(itm->param_value);
        }
        if (itm->param_name == "frames") {
            snd_info->frames = mtoi(itm->param_value);
        }
        if (itm->param_name == "sample_rate") {
            snd_info->sample_rate = mtoi(itm->param_value);
        }
        if (itm->param_name == "pulse_server") {
            snd_info->pulse_server = itm->param_value;
        }
    }
}

#ifdef HAVE_ALSA  /************ Start ALSA *******************/

void cls_sound::alsa_list_subdev()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;
    int indx, retcd, cnt;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Card %i(%s): %s [%s]")
        , alsa->card_id, alsa->device_nm.c_str()
        , snd_ctl_card_info_get_id(alsa->card_info)
        , snd_ctl_card_info_get_name(alsa->card_info));
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("  Device %i (%s,%d): %s [%s]")
        , alsa->device_id, alsa->device_nm.c_str()
        , alsa->device_id
        , snd_pcm_info_get_id(alsa->pcm_info)
        , snd_pcm_info_get_name(alsa->pcm_info));

    cnt = (int)snd_pcm_info_get_subdevices_count(alsa->pcm_info);
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("  Subdevices: %i/%i")
        , snd_pcm_info_get_subdevices_avail(alsa->pcm_info),cnt);

    for (indx=0; indx<cnt; indx++) {
        snd_pcm_info_set_subdevice(alsa->pcm_info, (uint)indx);
        retcd = snd_ctl_pcm_info(alsa->ctl_hdl, alsa->pcm_info);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("control digital audio playback info (%i): %s")
                , alsa->card_id, snd_strerror(retcd));
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("  Subdevice #%i: %s"), indx
                , snd_pcm_info_get_subdevice_name(alsa->pcm_info));
        }
    }

}

void cls_sound::alsa_list_card()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;
    int retcd;

    retcd = snd_ctl_card_info(alsa->ctl_hdl, alsa->card_info);
    if (retcd < 0) {
        snd_ctl_close(alsa->ctl_hdl);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("control hardware info (%i): %s")
            , alsa->card_id, snd_strerror(retcd));
        return;
    }

    alsa->device_id = -1;
    retcd = snd_ctl_pcm_next_device(alsa->ctl_hdl, &alsa->device_id);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("snd_ctl_pcm_next_device"));
        return;
    }

    while (alsa->device_id >= 0) {
        snd_pcm_info_set_device(alsa->pcm_info, (uint)alsa->device_id);
        snd_pcm_info_set_subdevice(alsa->pcm_info, 0);
        snd_pcm_info_set_stream(alsa->pcm_info, SND_PCM_STREAM_CAPTURE);
        retcd = snd_ctl_pcm_info(alsa->ctl_hdl, alsa->pcm_info);
        if (retcd == 0) {
            alsa_list_subdev();
        } else if (retcd != -ENOENT){
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("control digital audio info (%i): %s")
                , alsa->card_id, snd_strerror(retcd));
        }
        retcd = snd_ctl_pcm_next_device(alsa->ctl_hdl, &alsa->device_id);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("snd_ctl_pcm_next_device"));
        }
    }

}

void cls_sound::alsa_list()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;
    int retcd;

    if (device_status == STATUS_CLOSED) {
        return;
    }

    snd_ctl_card_info_alloca(&alsa->card_info);
    snd_pcm_info_alloca(&alsa->pcm_info);

    alsa->card_id = -1;
    retcd = snd_card_next(&alsa->card_id);
    if ((retcd < 0) || (alsa->card_id == -1)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("no soundcards found..."));
        device_status = STATUS_CLOSED;
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Devices"));

    while (alsa->card_id >= 0) {
        alsa->device_nm="hw:"+std::to_string(alsa->card_id);
        retcd = snd_ctl_open(&alsa->ctl_hdl, alsa->device_nm.c_str(), 0);
        if (retcd == 0) {
            alsa_list_card();
            snd_ctl_close(alsa->ctl_hdl);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("control open (%i): %s")
                , alsa->card_id, snd_strerror(retcd));
        }
        snd_card_next(&alsa->card_id);
    }

}

void cls_sound::alsa_start()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t frames_per;
    snd_pcm_format_t actl_sndfmt;
    unsigned int actl_rate, smpl_rate;
    int retcd;

    frames_per = (uint)snd_info->frames;
    smpl_rate = (unsigned int)snd_info->sample_rate;

    retcd = snd_pcm_open(&alsa->pcm_dev
        , cfg->snd_device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_open device %s (%s)")
            , cfg->snd_device.c_str(), snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_malloc(&hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_malloc(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_any(alsa->pcm_dev, hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_any(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_access(alsa->pcm_dev
        , hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_access(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_format(alsa->pcm_dev
        , hw_params, SND_PCM_FORMAT_S16_LE);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_format(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_rate_near(alsa->pcm_dev
        , hw_params, &smpl_rate, 0);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_rate_near(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_channels(alsa->pcm_dev
        , hw_params, (uint)snd_info->channels);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_channels(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_period_size_near(alsa->pcm_dev
        , hw_params, &frames_per, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_period_size_near(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params(alsa->pcm_dev, hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_prepare(alsa->pcm_dev);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_prepare(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    /* get actual parms selected */
	retcd = snd_pcm_hw_params_get_format(hw_params, &actl_sndfmt);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_format(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_get_rate(hw_params, &actl_rate, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_rate(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_get_period_size(hw_params, &frames_per, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_period_size(%s)")
            , snd_strerror (retcd));
        device_status = STATUS_CLOSED;
        return;
    }

    snd_pcm_hw_params_free(hw_params);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Actual rate %hu"), actl_rate);
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Actual frames per %lu"), frames_per);
    if (actl_sndfmt <= 5) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Sound format 16"));
    } else if (actl_sndfmt <= 9 ) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Sound format 24"));
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Sound format 32"));
    }

    /*************************************************************/
    /** allocate and initialize the sound buffers                */
    /*************************************************************/
    snd_info->frames = (int)frames_per;
    snd_info->buffer_size = snd_info->frames * 2;
    snd_info->buffer = (int16_t*)mymalloc(
        (uint)snd_info->buffer_size * sizeof(int16_t));
    memset(snd_info->buffer, 0x00
        , (uint)snd_info->buffer_size * sizeof(int16_t));

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Started.");
    device_status =STATUS_OPENED;

}

void cls_sound::alsa_init()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;

    if (snd_info->source != "alsa") {
        return;
    }

    alsa->pcm_dev = NULL;
    alsa->pcm_info = NULL;
    alsa->card_id = -1;
    alsa->card_info = NULL;
    alsa->pcm_dev = NULL;
    alsa->ctl_hdl = NULL;
    alsa->card_info = NULL;
    alsa->card_id = 0;
    alsa->pcm_info = NULL;
    alsa->device_nm = "";
    alsa->device_id = 0;

    alsa_list();
    alsa_start();
}

void cls_sound::alsa_capture()
{
    ctx_snd_alsa *alsa = snd_info->snd_alsa;
    long int retcd;

    if (snd_info->source != "alsa") {
        return;
    }
    retcd = snd_pcm_readi(alsa->pcm_dev
        , snd_info->buffer, (uint)snd_info->frames);
    if (retcd != snd_info->frames) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: read from audio interface failed (%s)")
            , snd_strerror((int)retcd));
        device_status = STATUS_CLOSED;
    }
}

void cls_sound::alsa_cleanup()
{
    if (snd_info->source != "alsa") {
        return;
    }
    if (snd_info->snd_alsa->pcm_dev != NULL) {
        snd_pcm_close(snd_info->snd_alsa->pcm_dev);
        snd_config_update_free_global();
    }
}

#endif  /************ End ALSA *******************/

#ifdef HAVE_PULSE /************ Start PULSE *******************/

void cls_sound::pulse_init()
{
    pa_sample_spec specs;
    int errcd;

    if (snd_info->source != "pulse") {
        return;
    }

    specs.format = PA_SAMPLE_S16LE;
    specs.rate = (uint32_t)snd_info->sample_rate;
    specs.channels = (uint8_t)snd_info->channels;

    snd_info->snd_pulse->dev = NULL;
    snd_info->snd_pulse->dev = pa_simple_new(
        (snd_info->pulse_server=="" ? NULL : snd_info->pulse_server.c_str())
        , "motion", PA_STREAM_RECORD
        , (cfg->snd_device=="" ? NULL : cfg->snd_device.c_str())
        , "motion", &specs, NULL, NULL, &errcd);
    if (snd_info->snd_pulse->dev == NULL) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Error opening pulse (%s)")
            , pa_strerror(errcd));
        device_status = STATUS_CLOSED;
        return;
    }
    snd_info->buffer_size = snd_info->frames * 2;
    snd_info->buffer = (int16_t*)mymalloc(
        (uint)snd_info->buffer_size * sizeof(int16_t));
    memset(snd_info->buffer, 0x00
        , (uint)snd_info->buffer_size * sizeof(int16_t));

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Started.");
    device_status =STATUS_OPENED;

}

void cls_sound::pulse_capture()
{
    ctx_snd_pulse *pulse = snd_info->snd_pulse;
    int errcd, retcd;

    if (snd_info->source != "pulse") {
        return;
    }

    retcd = pa_simple_read(pulse->dev, snd_info->buffer
        , (uint)snd_info->buffer_size, &errcd);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Error capturing PulseAudio (%s)")
            , pa_strerror(errcd));
        device_status = STATUS_CLOSED;
    }
}

void cls_sound::pulse_cleanup()
{
    if (snd_info->source != "pulse") {
        return;
    }

    if (snd_info->snd_pulse->dev != NULL) {
        pa_simple_free(snd_info->snd_pulse->dev);
    }
}

#endif  /************ End PULSE *******************/

#ifdef HAVE_FFTW3 /************ Start FFTW3 *******************/

void cls_sound::fftw_open()
{
    ctx_snd_fftw *fftw = snd_info->snd_fftw;
    int indx;

    if (device_status == STATUS_CLOSED) {
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Opening FFTW plan"));

    fftw->ff_in   = (double*) fftw_malloc(
        sizeof(fftw_complex) * (uint)snd_info->frames);
    fftw->ff_out  = (fftw_complex*) fftw_malloc(
        sizeof(fftw_complex) * (uint)snd_info->frames);
    fftw->ff_plan = fftw_plan_dft_r2c_1d(
        snd_info->frames, fftw->ff_in, fftw->ff_out, FFTW_MEASURE);

    for (indx=0; indx<snd_info->frames; indx++) {
        fftw->ff_in[indx] = 0;
    }
    fftw->bin_min = 1;
    fftw->bin_max = (snd_info->frames / 2);
    fftw->bin_size = ((float)snd_info->sample_rate / (float)snd_info->frames);

}

float cls_sound::HammingWindow(int n1, int N2){
    return 0.54F - 0.46F * (float)(cos((2 * M_PI * n1)) / (N2 - 1));
}

float cls_sound::HannWindow(int n1, int N2){
    return 0.5F * (float)(1 - (cos(2 * M_PI * n1 * N2)));
}

void cls_sound::check_alerts()
{
    double freq_value;
    int    indx, chkval, chkcnt;
    double pMaxIntensity;
    int    pMaxBinIndex;
    double pRealNbr;
    double pImaginaryNbr;
    double pIntensity;
    bool   trigger;
    std::list<ctx_snd_alert>::iterator it;
    struct timespec trig_ts;

    for (indx=0;indx <snd_info->frames;indx++){
        if (cfg->snd_window == "hamming") {
            snd_info->snd_fftw->ff_in[indx] =
                snd_info->buffer[indx] * HammingWindow(indx, snd_info->frames);
        } else if (cfg->snd_window == "hann") {
            snd_info->snd_fftw->ff_in[indx] =
                snd_info->buffer[indx] * HannWindow(indx, snd_info->frames);
        } else {
            snd_info->snd_fftw->ff_in[indx] = snd_info->buffer[indx];
        }
    }

    fftw_execute(snd_info->snd_fftw->ff_plan);

    pMaxIntensity = 0;
    pMaxBinIndex = 0;

    for (indx = snd_info->snd_fftw->bin_min;
        indx <= snd_info->snd_fftw->bin_max; indx++) {
        pRealNbr = snd_info->snd_fftw->ff_out[indx][0];
        pImaginaryNbr = snd_info->snd_fftw->ff_out[indx][1];
        pIntensity = pRealNbr * pRealNbr + pImaginaryNbr * pImaginaryNbr;
        if (pIntensity > pMaxIntensity){
            pMaxIntensity = pIntensity;
            pMaxBinIndex = indx;
        }
    }

    freq_value = (snd_info->snd_fftw->bin_size * pMaxBinIndex * snd_info->channels);

    if (cfg->snd_show) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            , _("Freq: %.4f threshold: %d count: %d maximum: %d")
            , freq_value, snd_info->vol_min
            , snd_info->vol_count, snd_info->vol_max);
    }

    for (it=snd_info->alerts.begin(); it!=snd_info->alerts.end(); it++) {
        trigger = false;
        if ((freq_value >= it->freq_low) && (freq_value <= it->freq_high)) {
            chkcnt = 0;
            for(indx=0; indx < snd_info->frames; indx++) {
                chkval = abs((int)snd_info->buffer[indx] / 256);
                if (chkval >= it->volume_level) {
                    chkcnt++;
                }
            }
            if (chkcnt >= it->volume_count) {
                trigger = true;
            }
        }
        if (trigger) {
            clock_gettime(CLOCK_MONOTONIC, &trig_ts);
            if ((trig_ts.tv_sec - it->trigger_time.tv_sec) > it->trigger_duration) {
                it->trigger_count = 1;
            } else {
                it->trigger_count++;
            }
            clock_gettime(CLOCK_MONOTONIC, &it->trigger_time);

            if (it->trigger_count == it->trigger_threshold) {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                    , _("Sound Alert %d-%s : level %d count %d max vol %d")
                    , it->alert_id ,it->alert_nm.c_str()
                    , it->volume_level, chkcnt
                    , snd_info->vol_max);
                if (cfg->on_sound_alert != "") {
                    snd_info->trig_freq =std::to_string(freq_value);
                    snd_info->trig_nbr = std::to_string(it->alert_id);
                    snd_info->trig_nm = it->alert_nm;
                    util_exec_command(this, cfg->on_sound_alert);
                }
            }
        }
    }

}

#endif  /************ End FFTW3 *******************/

void cls_sound::capture()
{
    if (device_status == STATUS_CLOSED) {
        return;
    }
    #ifdef HAVE_ALSA
        alsa_capture();
    #endif
    #ifdef HAVE_PULSE
        pulse_capture();
    #endif
}

void cls_sound::cleanup()
{
    #ifdef HAVE_ALSA
        alsa_cleanup();
    #endif
    #ifdef HAVE_PULSE
        pulse_cleanup();
    #endif
    #ifdef HAVE_FFTW3
        fftw_destroy_plan(snd_info->snd_fftw->ff_plan);
        fftw_free(snd_info->snd_fftw->ff_in);
        fftw_free(snd_info->snd_fftw->ff_out);
    #endif
    if (snd_info->buffer != NULL) {
        free(snd_info->buffer);
        snd_info->buffer = NULL;
    }

    mydelete(snd_info->snd_alsa);
    mydelete(snd_info->snd_fftw);
    mydelete(snd_info->snd_pulse);
    mydelete(snd_info->params);
    mydelete(snd_info);

    device_status = STATUS_CLOSED;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Stopped.");

}

void cls_sound::init()
{
    if ((device_status != STATUS_INIT) && (restart == false)) {
        return;
    }

    if (restart == true) {
        cleanup();
        restart = false;
    }

    cfg->parms_copy(conf_src);

    mythreadname_set("sl",cfg->device_id, cfg->device_name.c_str());

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Initialize sound frequency"));

    snd_info = new ctx_snd_info;
    snd_info->params = new ctx_params;
    snd_info->snd_fftw = new ctx_snd_fftw;
    snd_info->snd_alsa = new ctx_snd_alsa;
    snd_info->snd_pulse = new ctx_snd_pulse;

    init_values();
    load_params();
    load_alerts();

    if ((snd_info->source != "alsa") &&
        (snd_info->source != "pulse")) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Invalid sound source."));
        device_status = STATUS_CLOSED;
        return;
    }

    #ifdef HAVE_ALSA
        alsa_init();
    #endif
    #ifdef HAVE_PULSE
        pulse_init();
    #endif
    #ifdef HAVE_FFTW3
        fftw_open();
    #endif

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Detecting"));

}

void cls_sound::check_levels()
{
    #ifdef HAVE_FFTW3
        int indx, chkval;

        if (device_status == STATUS_CLOSED) {
            return;
        }

        snd_info->vol_max = 0;
        snd_info->vol_count = 0;

        for(indx=0; indx < snd_info->frames; indx++) {
            chkval = abs((int)snd_info->buffer[indx] / 256);
            if (chkval > snd_info->vol_max) snd_info->vol_max = chkval ;
            if (chkval > snd_info->vol_min) snd_info->vol_count++;
        }

        if (snd_info->vol_count > 0) {
            check_alerts();
        }
    #endif
 }

void cls_sound::handler()
{
    device_status = STATUS_INIT;

    while (handler_stop == false) {
        watchdog = cfg->watchdog_tmo;
        init();
        capture();
        check_levels();
        if (device_status == STATUS_CLOSED) {
            handler_stop = true;
        }
    }

    cleanup();

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Sound exiting"));

    handler_running = false;
    pthread_exit(nullptr);
}

void cls_sound::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    #if  !defined(HAVE_FFTW3) || (!defined(HAVE_ALSA) && !defined(HAVE_PULSE))
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Required packages not installed"));
        device_status = STATUS_CLOSED;
        return;
    #endif

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &sound_handler, this);
        if (retcd != 0) {
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start sound frequency detection loop."));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_sound::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == cfg->watchdog_tmo) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of sound frequency detection failed"));
            if (cfg->watchdog_kill > 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == cfg->watchdog_kill) {
                    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
        watchdog = cfg->watchdog_tmo;
    }

}

cls_sound::cls_sound(cls_motapp *p_app)
{
    app = p_app;
    handler_running = false;
    handler_stop = true;
    restart = false;
    watchdog = 30;
}

cls_sound::~cls_sound()
{
    mydelete(conf_src);
    mydelete(cfg);
}

