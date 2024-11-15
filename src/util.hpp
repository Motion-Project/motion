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
#else
    typedef const uint8_t myuint;   /* Version independent uint */
#endif


#ifdef HAVE_GETTEXT
    #include <libintl.h>
    extern int  _nl_msg_cat_cntr;    /* Required for changing the locale dynamically */
#endif

#define _(STRING) mytranslate_text(STRING, 2)

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
    std::string     param_name;       /* The name or description of the ID as requested by user*/
    std::string     param_value;      /* The value that the user wants the control set to*/
};
typedef std::vector<ctx_params_item> vec_params;
struct ctx_params {
    vec_params  params_array;
    int         params_cnt;
    std::string params_desc;
};

    void *mymalloc(size_t nbytes);

    void *myrealloc(void *ptr, size_t size, const char *desc);
    int mycreate_path(const char *path);
    FILE *myfopen(const char *path, const char *mode);
    int myfclose(FILE *fh);
    void mystrftime(cls_camera *cam, char *s, size_t mx_sz
        , const char *usrfmt, const char *fname);
    void mystrftime(cls_camera *cam, std::string &rslt
        , std::string usrfmt, std::string fname);
    void mystrftime(cls_sound *snd, std::string &dst, std::string fmt);
    void util_exec_command(cls_camera *cam, const char *command, const char *filename);
    void util_exec_command(cls_sound *snd, std::string cmd);
    void util_exec_command(cls_camera *cam, std::string cmd);

    void mythreadname_set(const char *abbr, int threadnbr, const char *threadname);
    void mythreadname_get(char *threadname);
    void mythreadname_get(std::string &threadname);

    char* mytranslate_text(const char *msgid, int setnls);
    void mytranslate_init(void);

    int mystrceq(const char* var1, const char* var2);
    int mystrcne(const char* var1, const char* var2);
    int mystreq(const char* var1, const char* var2);
    int mystrne(const char* var1, const char* var2);
    void myltrim(std::string &parm);
    void myrtrim(std::string &parm);
    void mytrim(std::string &parm);
    void myunquote(std::string &parm);

    void myframe_key(AVFrame *frame);
    void myframe_interlaced(AVFrame *frame);
    AVPacket *mypacket_alloc(AVPacket *pkt);

    void util_parms_parse(ctx_params *params, std::string parm_desc, std::string confline);
    void util_parms_add_default(ctx_params *params, std::string parm_nm, std::string parm_vl);
    void util_parms_add_default(ctx_params *params, std::string parm_nm, int parm_vl);
    void util_parms_add(ctx_params *params, std::string parm_nm, std::string parm_val);
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

    void util_resize(uint8_t *src, int src_w, int src_h
        , uint8_t *dst, int dst_w, int dst_h);

#endif /* _INCLUDE_UTIL_HPP_ */
