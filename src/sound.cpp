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

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "sound.hpp"

#if defined(HAVE_ALSA) && defined(HAVE_FFTW3)

static float HammingWindow(int n1, int N2){
    return 0.54F - 0.46F * (float)(cos((2 * M_PI * n1)) / (N2 - 1));
}
static float HannWindow(int n1, int N2){
    return 0.5F * (float)(1 - (cos(2 * M_PI * n1 * N2)));
}

static void snd_init_values(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    ctx_snd_info *vars = snd->snd_info;

    alsa->pcm_dev = NULL;
    alsa->pcm_info = NULL;
    alsa->card_id = -1;
    alsa->card_info = NULL;
    alsa->pcm_dev = NULL;
    alsa->sample_rate = 0;
    alsa->channels = 0;
    alsa->buf_frames = 0;
    alsa->buf_size = 0;
    alsa->buf_items = 0;
    alsa->input_buffer = 0;
    alsa->ctl_hdl = NULL;
    alsa->card_info = NULL;
    alsa->card_id = 0;
    alsa->pcm_info = NULL;
    alsa->device_nm = "";
    alsa->device_id = 0;

    snd->snd_fftw->bin_max = 0;
    snd->snd_fftw->bin_min = 0;
    snd->snd_fftw->bin_size = 0;
    snd->snd_fftw->ff_in = NULL;
    snd->snd_fftw->ff_out = NULL;
    snd->snd_fftw->ff_plan = NULL;

    vars->snd_vol_count = 0;
    vars->snd_vol_max = 0;
    vars->snd_vol_min = 9999;

}

static void snd_init_alerts(ctx_snd_alert  *tmp_alert)
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

static void snd_edit_alerts(ctx_dev *snd)
{
    ctx_snd_info *vars = snd->snd_info;
    std::list<ctx_snd_alert>::iterator it_a0;
    std::list<ctx_snd_alert>::iterator it_a1;
    int     indx, id_chk, id_cnt;
    bool    validids;

    validids = true;
    for (it_a0=vars->snd_alerts.begin(); it_a0!=vars->snd_alerts.end(); it_a0++) {
        id_chk = it_a0->alert_id;
        id_cnt = 0;
        for (it_a1=vars->snd_alerts.begin(); it_a1!=vars->snd_alerts.end(); it_a1++) {
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
        for (it_a0=vars->snd_alerts.begin(); it_a0!=vars->snd_alerts.end(); it_a0++) {
            it_a0->alert_id = indx;
            indx++;
        }
    }

    for (it_a0=vars->snd_alerts.begin(); it_a0!=vars->snd_alerts.end(); it_a0++) {
        if (it_a0->alert_nm == "") {
            it_a0->alert_nm = "sound_alert" + std::to_string(it_a0->alert_id);
        }
        if (it_a0->volume_level < vars->snd_vol_min) {
            vars->snd_vol_min = it_a0->volume_level;
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

static void snd_load_alerts(ctx_dev *snd)
{
    ctx_snd_info *vars = snd->snd_info;
    ctx_snd_alert  tmp_alert;
    std::list<std::string> parm_val;
    std::list<std::string>::iterator  it_p;
    ctx_params      *tmp_params;
    int     indx;

    conf_edit_get(snd->conf, "snd_alerts", parm_val, PARM_CAT_18);

    for (it_p=parm_val.begin(); it_p!=parm_val.end(); it_p++) {
        tmp_params = (ctx_params*)mymalloc(sizeof(ctx_params));
        tmp_params->update_params = true;
        util_parms_parse(tmp_params, it_p->c_str());
        snd_init_alerts(&tmp_alert);
        for (indx=0; indx<tmp_params->params_count; indx++) {
            if (mystreq(tmp_params->params_array[indx].param_name,"alert_id")) {
                tmp_alert.alert_id = atoi(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"alert_nm")) {
                tmp_alert.alert_nm = (tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"freq_low")) {
                tmp_alert.freq_low = atof(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"freq_high")) {
                tmp_alert.freq_high = atof(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"volume_count")) {
                tmp_alert.volume_count = atoi(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"volume_level")) {
                tmp_alert.volume_level = atoi(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"trigger_threshold")) {
                tmp_alert.trigger_threshold = atoi(tmp_params->params_array[indx].param_value);
            }
            if (mystreq(tmp_params->params_array[indx].param_name,"trigger_duration")) {
                tmp_alert.trigger_duration = atoi(tmp_params->params_array[indx].param_value);
            }
        }
        vars->snd_alerts.push_back(tmp_alert);
        util_parms_free(tmp_params);
        myfree(&tmp_params);
        tmp_params = NULL;
    }

    snd_edit_alerts(snd);
}

static void snd_load_params(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    int indx;

    snd->params = (ctx_params*)mymalloc(sizeof(ctx_params));
    snd->params->update_params = true;
    util_parms_parse(snd->params, snd->conf->snd_params);

    util_parms_add_default(snd->params,"source","alsa");
    util_parms_add_default(snd->params,"channels","1");
    util_parms_add_default(snd->params,"frames","2048");
    util_parms_add_default(snd->params,"sample","44100");

    for (indx = 0; indx < snd->params->params_count; indx++) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s : %s"
            , snd->params->params_array[indx].param_name
            , snd->params->params_array[indx].param_value
            );
    }

    for (indx = 0; indx < snd->params->params_count; indx++) {
        if (mystreq(snd->params->params_array[indx].param_name,"source")) {
            alsa->source = snd->params->params_array[indx].param_value;
        }
        if (mystreq(snd->params->params_array[indx].param_name,"channels")) {
            alsa->channels =atoi(snd->params->params_array[indx].param_value);
        }
        if (mystreq(snd->params->params_array[indx].param_name,"frames")) {
            alsa->buf_frames =atoi(snd->params->params_array[indx].param_value);
        }
        if (mystreq(snd->params->params_array[indx].param_name,"sample")) {
            alsa->sample_rate =atoi(snd->params->params_array[indx].param_value);
        }
    }

}

static void snd_device_list_subdev(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
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
        snd_pcm_info_set_subdevice(alsa->pcm_info,indx);
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

static void snd_device_list_card(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
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
        snd_pcm_info_set_device(alsa->pcm_info, alsa->device_id);
        snd_pcm_info_set_subdevice(alsa->pcm_info, 0);
        snd_pcm_info_set_stream(alsa->pcm_info, SND_PCM_STREAM_CAPTURE);
        retcd = snd_ctl_pcm_info(alsa->ctl_hdl, alsa->pcm_info);
        if (retcd == 0) {
            snd_device_list_subdev(snd);
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

static void snd_device_list(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    int retcd;

    if (snd->device_status == STATUS_CLOSED) {
        return;
    }

    snd_ctl_card_info_alloca(&alsa->card_info);
    snd_pcm_info_alloca(&alsa->pcm_info);

    alsa->card_id = -1;
    retcd = snd_card_next(&alsa->card_id);
    if ((retcd < 0) || (alsa->card_id == -1)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("no soundcards found..."));
        snd->device_status = STATUS_CLOSED;
        snd->restart_dev = false;
        snd->finish_dev = true;
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Devices"));

    while (alsa->card_id >= 0) {
        alsa->device_nm="hw:"+std::to_string(alsa->card_id);
        retcd = snd_ctl_open(&alsa->ctl_hdl, alsa->device_nm.c_str(), 0);
        if (retcd == 0) {
            snd_device_list_card(snd);
            snd_ctl_close(alsa->ctl_hdl);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("control open (%i): %s")
                , alsa->card_id, snd_strerror(retcd));
        }
        snd_card_next(&alsa->card_id);
    }

}

static void snd_fftw_open(ctx_dev *snd)
{
    ctx_snd_fftw *fftw = snd->snd_fftw;
    ctx_snd_alsa *alsa = snd->snd_alsa;
    int indx;

    if (snd->device_status == STATUS_CLOSED) {
        snd->finish_dev = true;
        snd->restart_dev = false;
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Opening FFTW plan"));

    fftw->ff_in   = (double*) fftw_malloc(
        sizeof(fftw_complex) * alsa->buf_frames);
    fftw->ff_out  = (fftw_complex*) fftw_malloc(
        sizeof(fftw_complex) * alsa->buf_frames);
    fftw->ff_plan = fftw_plan_dft_r2c_1d(
        alsa->buf_frames, fftw->ff_in, fftw->ff_out, FFTW_MEASURE);

    for (indx=0; indx<alsa->buf_frames; indx++) {
        fftw->ff_in[indx] = 0;
    }
    fftw->bin_min = 1;
    fftw->bin_max = (alsa->buf_frames / 2);
    fftw->bin_size = ((float)alsa->sample_rate / (float)alsa->buf_frames);
    alsa->buf_items = alsa->sample_rate * alsa->channels;

}

static void snd_device_start(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t frames_per;
    snd_pcm_format_t actl_sndfmt;
    unsigned int actl_rate;
    int retcd;

    frames_per = alsa->buf_frames;

    retcd = snd_pcm_open(&alsa->pcm_dev
        , snd->conf->snd_device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_open device %s (%s)")
            , snd->conf->snd_device.c_str(), snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_malloc(&hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_malloc(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_any(alsa->pcm_dev, hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_any(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_access(alsa->pcm_dev
        , hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_access(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_format(alsa->pcm_dev
        , hw_params, SND_PCM_FORMAT_S16_LE);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_format(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_rate_near(alsa->pcm_dev
        , hw_params, &alsa->sample_rate, 0);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_rate_near(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_channels(alsa->pcm_dev
        , hw_params, alsa->channels);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_channels(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_set_period_size_near(alsa->pcm_dev
        , hw_params, &frames_per, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_set_period_size_near(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params(alsa->pcm_dev, hw_params);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    snd_pcm_hw_params_free(hw_params);

    retcd = snd_pcm_prepare(alsa->pcm_dev);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_prepare(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    /* get actual parms selected */
	retcd = snd_pcm_hw_params_get_format(hw_params, &actl_sndfmt);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_format(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_get_rate(hw_params, &actl_rate, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_rate(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

    retcd = snd_pcm_hw_params_get_period_size(hw_params, &frames_per, NULL);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: snd_pcm_hw_params_get_period_size(%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        return;
    }

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
    alsa->buf_size = alsa->buf_frames * 2;
    alsa->input_buffer = (int16_t*)mymalloc(alsa->buf_size * sizeof(int16_t));
    memset(alsa->input_buffer, 0x00,  alsa->buf_size * sizeof(int16_t));

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Started.");
    snd->device_status =STATUS_OPENED;

}

static void slp_capture(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    int retcd;

    if (snd->device_status == STATUS_CLOSED) {
        snd->finish_dev = true;
        snd->restart_dev = false;
        return;
    }

    retcd = snd_pcm_readi(alsa->pcm_dev, alsa->input_buffer
        , alsa->buf_frames);
    if (retcd != alsa->buf_frames) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("error: read from audio interface failed (%s)")
            , snd_strerror (retcd));
        snd->device_status = STATUS_CLOSED;
        snd->finish_dev = true;
        snd->restart_dev = false;
        return;
    }

}

void snd_cleanup(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;

    if (alsa->pcm_dev != NULL) {
        snd_pcm_close(alsa->pcm_dev);
        snd_config_update_free_global();
    }

    snd_config_update_free_global();

    if (alsa->input_buffer != NULL) {
        free(alsa->input_buffer);
        alsa->input_buffer = NULL;
    }

    util_parms_free(snd->params);
    myfree(&snd->params);
    snd->params = NULL;

    fftw_destroy_plan(snd->snd_fftw->ff_plan);
    fftw_free(snd->snd_fftw->ff_in);
    fftw_free(snd->snd_fftw->ff_out);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Stopped.");

}

static void slp_init(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;

    if ((snd->device_status != STATUS_INIT) &&
        (snd->device_status != STATUS_RESET)) {
        return;
    }

    if (snd->device_status == STATUS_RESET) {
        snd_cleanup(snd);
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Initialize"));

    snd_init_values(snd);

    snd_load_params(snd);

    snd_load_alerts(snd);

    if (alsa->source == "alsa") {
        snd_device_list(snd);
        snd_device_start(snd);
        snd_fftw_open(snd);
    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Invalid sound source."));
        snd->finish_dev = true;
        snd->device_status = STATUS_CLOSED;
    }
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Detecting"));

}

static void snd_check_alerts(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    ctx_snd_info *vars = snd->snd_info;
    float freq_value;
    int   indx, chkval, chkcnt;
    float pMaxIntensity;
    int   pMaxBinIndex;
    float pRealNbr;
    float pImaginaryNbr;
    float pIntensity;
    bool trigger;
    std::string pcmd;
    std::list<ctx_snd_alert>::iterator it;
    struct timespec trig_ts;

    for (indx=0;indx <alsa->buf_frames;indx++){
        if (snd->conf->snd_window == "hamming") {
            snd->snd_fftw->ff_in[indx] = alsa->input_buffer[indx] * HammingWindow(indx, alsa->buf_frames);
        } else if (snd->conf->snd_window == "hann") {
            snd->snd_fftw->ff_in[indx] = alsa->input_buffer[indx] * HannWindow(indx, alsa->buf_frames);
        } else {
            snd->snd_fftw->ff_in[indx] = alsa->input_buffer[indx];
        }
    }

    fftw_execute(snd->snd_fftw->ff_plan);

    pMaxIntensity = 0;
    pMaxBinIndex = 0;

    for (indx = snd->snd_fftw->bin_min; indx <= snd->snd_fftw->bin_max; indx++){
        pRealNbr = snd->snd_fftw->ff_out[indx][0];
        pImaginaryNbr = snd->snd_fftw->ff_out[indx][1];
        pIntensity = pRealNbr * pRealNbr + pImaginaryNbr * pImaginaryNbr;
        if (pIntensity > pMaxIntensity){
            pMaxIntensity = pIntensity;
            pMaxBinIndex = indx;
        }
    }

    freq_value = (snd->snd_fftw->bin_size * pMaxBinIndex * alsa->channels);

    if (snd->conf->snd_show) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            , _("Freq: %.4f threshold: %d count: %d maximum: %d")
            , freq_value, vars->snd_vol_min
            , vars->snd_vol_count, vars->snd_vol_max);
    }

    for (it=vars->snd_alerts.begin(); it!=vars->snd_alerts.end(); it++) {
        trigger = false;
        if ((freq_value >= it->freq_low) && (freq_value <= it->freq_high)) {
            chkcnt = 0;
            for(indx=0; indx < alsa->buf_frames ; indx++) {
                chkval = abs((int)alsa->input_buffer[indx] / 256);
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
                    , vars->snd_vol_max);
                if (snd->conf->on_sound_alert != "") {
                    pcmd = snd->conf->on_sound_alert;
                    pcmd = pcmd + " " + std::to_string(it->alert_id);
                    pcmd = pcmd + " " + std::to_string(freq_value);
                    util_exec_command(snd, pcmd.c_str(), NULL, 0);
                }
            }
        }
    }

}

static void snd_check_levels(ctx_dev *snd)
{
    ctx_snd_alsa *alsa = snd->snd_alsa;
    ctx_snd_info *vars = snd->snd_info;
    int indx, chkval;

    if (snd->device_status == STATUS_CLOSED) {
        snd->finish_dev = true;
        snd->restart_dev = false;
        return;
    }

    vars->snd_vol_max = 0;
    vars->snd_vol_count = 0;

    for(indx=0; indx < alsa->buf_frames; indx++) {
        chkval = abs((int)alsa->input_buffer[indx] / 256);
        if (chkval > vars->snd_vol_max) vars->snd_vol_max = chkval ;
        if (chkval > vars->snd_vol_min) vars->snd_vol_count++;
    }

    if (vars->snd_vol_count > 0) {
        snd_check_alerts(snd);
    }

 }

void *snd_loop(void *arg)
{
    ctx_dev *snd = (ctx_dev *)arg;

    pthread_mutex_lock(&snd->motapp->global_lock);
        snd->motapp->threads_running++;
    pthread_mutex_unlock(&snd->motapp->global_lock);

    mythreadname_set("sl", snd->threadnr, snd->conf->device_name.c_str());
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)snd->threadnr));

    snd->snd_fftw = new ctx_snd_fftw;
    snd->snd_alsa = new ctx_snd_alsa;
    snd->snd_info = new ctx_snd_info;
    snd->running_dev = true;
    snd->finish_dev = false;
    snd->restart_dev = false;
    snd->device_status = STATUS_INIT;

    while (snd->finish_dev == false) {
        slp_init(snd);
        slp_capture(snd);
        snd_check_levels(snd);
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Capture loop finished"));

    snd_cleanup(snd);

    pthread_mutex_lock(&snd->motapp->global_lock);
        snd->motapp->threads_running--;
    pthread_mutex_unlock(&snd->motapp->global_lock);

    snd->finish_dev = true;
    snd->running_dev = false;
    delete snd->snd_alsa;
    delete snd->snd_fftw;
    delete snd->snd_info;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Exiting"));

    pthread_exit(NULL);

}

#else

void *snd_loop(void *arg)
{
    ctx_dev *snd =(ctx_dev *) arg;

    snd->restart_dev = false;
    snd->finish_dev = true;
    snd->running_dev = false;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Sound detection functionality not supported"));

    pthread_exit(NULL);
}

#endif
