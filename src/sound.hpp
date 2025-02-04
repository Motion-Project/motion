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
 */

#ifndef _INCLUDE_SOUND_HPP_
#define _INCLUDE_SOUND_HPP_

#ifdef HAVE_ALSA
    extern "C" {
        #include <alsa/asoundlib.h>
    }
#endif

#ifdef HAVE_PULSE
    extern "C" {
        #include <pulse/simple.h>
        #include <pulse/error.h>
    }
#endif


#ifdef HAVE_FFTW3
    extern "C" {
        #include <fftw3.h>
    }
#endif

struct ctx_snd_alert {
    int             alert_id;           /* Id number for the alert*/
    std::string     alert_nm;           /* Name of the alert*/
    int             volume_level;       /* Volume level required to consider the sample*/
    int             volume_count;       /* For each sample, number of times required to exceed volumne level*/
    double          freq_low;           /* Lowest frequency for detecting this alert*/
    double          freq_high;          /* Highest frequency for detecting this alert*/
    int             trigger_count;      /* Count of how many times it has been triggered so far*/
    int             trigger_threshold;  /* How many times does it need to be triggered before an event*/
    timespec        trigger_time;       /* The last time the trigger was invoked */
    int             trigger_duration;   /* Min. duration to trigger a new /event */
};

struct ctx_snd_alsa {
   #ifdef HAVE_ALSA
        int                     device_id;
        std::string             device_nm;
        snd_pcm_t               *pcm_dev;
        snd_pcm_info_t          *pcm_info;
        int                     card_id;
        snd_ctl_card_info_t     *card_info;
        snd_ctl_t               *ctl_hdl;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_pulse {
   #ifdef HAVE_PULSE
        pa_simple       *dev;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_fftw {
    #ifdef HAVE_FFTW3
        fftw_plan       ff_plan;
        double          *ff_in;
        fftw_complex    *ff_out;
        int             bin_max;
        int             bin_min;
        double          bin_size;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_info {
    std::string                 source;         /* Source string in ALSA format e.g. hw:1,0*/
    int                         sample_rate;    /* Sample rate of sound source*/
    int                         channels;       /* Number of audio channels */
    std::list<ctx_snd_alert>    alerts;         /* list of sound alert criteria */
    int                         vol_min;        /* The minimum volume from alerts*/
    int                         vol_max;        /* Maximum volume of sample*/
    int                         vol_count;      /* Number of times volumne exceeded user specified volume level */
    int16_t                     *buffer;
    int                         buffer_size;
    int                         frames;
    std::string                 pulse_server;
    std::string                 trig_freq;
    std::string                 trig_nbr;
    std::string                 trig_nm;
    ctx_params                  *params;        /* Device parameters*/
    ctx_snd_fftw                *snd_fftw;      /* fftw for sound*/
    ctx_snd_alsa                *snd_alsa;      /* Alsa device for sound*/
    ctx_snd_pulse               *snd_pulse;     /* PulseAudio for sound*/
};

class cls_sound {
    public:
        cls_sound(cls_motapp *p_app);
        ~cls_sound();

        enum DEVICE_STATUS      device_status;

        cls_config      *conf_src;
        cls_config      *cfg;
        std::string     device_name;
        ctx_snd_info    *snd_info;
        int             threadnr;
        bool            restart;
        bool            finish;

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();
        void            handler_startup();
        void            handler_shutdown();

    private:
        cls_motapp      *app;
        int             watchdog;

        void cleanup();
        void init_values();
        void init();
        void init_alerts(ctx_snd_alert  *tmp_alert);
        void edit_alerts();
        void load_alerts();
        void load_params();
        void capture();
        void check_levels();

        #ifdef HAVE_ALSA
            void alsa_list_subdev();
            void alsa_list_card();
            void alsa_list();
            void alsa_start();
            void alsa_init();
            void alsa_capture();
            void alsa_cleanup();
        #endif
        #ifdef HAVE_PULSE
            void pulse_init();
            void pulse_capture();
            void pulse_cleanup();
        #endif
        #ifdef HAVE_FFTW3
            void fftw_open();
            float HammingWindow(int n1, int N2);
            float HannWindow(int n1, int N2);
            void check_alerts();
        #endif

};

#endif /* _INCLUDE_SOUND_HPP_ */
