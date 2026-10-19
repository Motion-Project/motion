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

#ifndef _INCLUDE_UTIL_HPP_
#define _INCLUDE_UTIL_HPP_

#define MYFFVER (LIBAVFORMAT_VERSION_MAJOR * 1000)+LIBAVFORMAT_VERSION_MINOR

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
    typedef const AVCodec myAVCodec; /* Version independent definition for AVCodec*/
#else
    typedef AVCodec myAVCodec; /* Version independent definition for AVCodec*/
#endif

#if (MYFFVER <= 60016)
    typedef uint8_t myuint;         /* Version independent uint */
    #define MY_PROFILE_H264_HIGH        FF_PROFILE_H264_HIGH
    #define MY_PROFILE_H264_MAIN        FF_PROFILE_H264_MAIN
    #define MY_PROFILE_H264_BASELINE    FF_PROFILE_H264_BASELINE
#else
    typedef const uint8_t myuint;   /* Version independent uint */
    #define MY_PROFILE_H264_HIGH        AV_PROFILE_H264_HIGH
    #define MY_PROFILE_H264_MAIN        AV_PROFILE_H264_MAIN
    #define MY_PROFILE_H264_BASELINE    AV_PROFILE_H264_BASELINE
#endif


#define SLEEP(seconds, nanoseconds) {              \
                struct timespec ts1;                \
                ts1.tv_sec = seconds;             \
                ts1.tv_nsec = (long)nanoseconds;        \
                while (nanosleep(&ts1, &ts1) == -1); \
        }
#define myfree(x)   {if(x!=nullptr) {free(x);  x=nullptr;}}
#define mydelete(x) {if(x!=nullptr) {delete x; x=nullptr;}}

#if MHD_VERSION >= 0x00097002
    typedef enum MHD_Result mhdrslt; /* Version independent return result from MHD */
#else
    typedef int             mhdrslt; /* Version independent return result from MHD */
#endif

struct ctx_params_item {
    std::string     param_name;     /* The name or description of the ID as requested by user*/
    std::string     param_value;    /* The value that the user wants the control set to*/
    enum PARM_SRC   param_source;   /* enum of parm_src for user,system. */
    enum PARM_TYP   param_type;     /* enum of parm_typ for bool,int or string. */
    int             param_lvl;      /* Enum of parm_lvl for webui*/
    enum PARM_CHG   param_chg;      /* Enum for whether a restart is needed to implement changes*/
    std::vector<std::string>   param_lst;      /* list of acceptable values*/
    std::string     param_desc;     /* Short description*/
};
typedef std::vector<ctx_params_item> vec_params;
struct ctx_params {
    cls_motapp      *app;
    vec_params      params_array;   /* Vector of parameter items*/
    /*TODO  Remove this params_cnt and use vector size.  This is a holdover from when this was C code*/
    int             params_cnt;     /* Count of items in vector*/
    bool            params_changed; /* Flag to determine if they have changed since applied*/
    std::string     params_desc;    /* Description of the parameters*/
    enum PARM_PCAT  params_pcat;    /* enum of parm_cat for grouping. */
};

    void *mymalloc(size_t nbytes);

    void *myrealloc(void *ptr, size_t size, const char *desc);
    int mycreate_path(const char *path);
    FILE *myfopen(const char *path, const char *mode);
    int myfclose(FILE *fh);
    void mystrftime(cls_camera *cam, char *s, size_t mx_sz
        , const char *usrcmd, const char *fname);
    void mystrftime(cls_camera *cam, std::string &rslt
        , std::string usrcmd, std::string fname);
    void mystrftime(cls_sound *snd, std::string &dst, std::string fmt);
    void util_exec_base(cls_motapp *p_app, std::string rslt);
    void util_exec_command(cls_camera *cam, std::string usrcmd, std::string fname);
    void util_exec_command(cls_camera *cam, const char *c_usrcmd, const char *c_fname);
    void util_exec_command(cls_sound *snd, std::string usrcmd);

    void mythreadname_set(const char *abbr, int threadnbr, const char *threadname);
    void mythreadname_get(char *threadname);
    void mythreadname_get(std::string &threadname);

    int mystrceq(const char* var1, const char* var2);
    int mystrcne(const char* var1, const char* var2);
    int mystreq(const char* var1, const char* var2);
    int mystrne(const char* var1, const char* var2);
    void mylower(std::string &parm);
    void myltrim(std::string &parm);
    void myrtrim(std::string &parm);
    void mytrim(std::string &parm);
    void myunquote(std::string &parm);

    void myframe_key(AVFrame *frame);
    void myframe_interlaced(AVFrame *frame);
    AVPacket *mypacket_alloc(AVPacket *pkt);

    void util_parms_parse(ctx_params *params, std::string pdesc, std::string confline, enum PARM_PCAT pcat);
    void util_parms_add_default(ctx_params *params, std::string parm_nm, std::string parm_vl, bool showmsg=true);
    void util_parms_add(ctx_params *params, std::string parm_nm, std::string parm_val, bool showmsg=true);
    void util_parms_update(ctx_params *params, std::string &confline);


    int mtoi(std::string parm);
    int mtoi(char *parm);
    float mtof(char *parm);
    float mtof(std::string parm);
    bool mtob(std::string parm);
    bool mtob(char *parm);
    long mtol(std::string parm);
    long mtol(char *parm);
    std::string mtok(std::string &parm, std::string tok);

    int util_resize(uint8_t *src, int src_w, int src_h, std::string src_4cc
        , uint8_t *dst, int dst_w, int dst_h);

#endif /* _INCLUDE_UTIL_HPP_ */
