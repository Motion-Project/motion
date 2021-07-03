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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 *
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "alg_sec.hpp" /* For sec detect in format output */
#include "dbse.hpp" /*For dbse ID in format output */


/** Non case sensitive equality check for strings*/
int mystrceq(const char* var1, const char* var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return 0;
    }
    return (strcasecmp(var1,var2) ? 0 : 1);
}

/** Non case sensitive inequality check for strings*/
int mystrcne(const char* var1, const char* var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return 0;
    }
    return (strcasecmp(var1,var2) ? 1: 0);
}

/** Case sensitive equality check for strings*/
int mystreq(const char* var1, const char* var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return 0;
    }
    return (strcmp(var1,var2) ? 0 : 1);
}

/** Case sensitive inequality check for strings*/
int mystrne(const char* var1, const char* var2)
{
    if ((var1 == NULL) ||(var2 == NULL)) {
        return 0;
    }
    return (strcmp(var1,var2) ? 1: 0);
}

/* Trim whitespace from left side */
void myltrim(std::string &parm)
{
    if (parm.length() == 0 ) {
        return;
    }

    while (parm.substr(0, 1) == " ") {
        if (parm.length() == 1) {
            parm="";
            return;
        } else {
            parm = parm.substr(1);
        }
    }
}

/* Trim whitespace from right side */
void myrtrim(std::string &parm)
{
    if (parm.length() == 0 ) {
        return;
    }

    while (parm.substr(parm.length()-1,1) == " ") {
        if (parm.length() == 1) {
            parm="";
            return;
        } else {
            parm = parm.substr(0,parm.length()-1);
        }
    }
}

/* Trim left and right whitespace */
void mytrim(std::string &parm)
{
    myrtrim(parm);
    myltrim(parm);
}

/** mymalloc */
void *mymalloc(size_t nbytes)
{
    void *dummy = calloc(nbytes, 1);

    if (!dummy) {
        MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, _("Could not allocate %llu bytes of memory!")
            ,(unsigned long long)nbytes);
        exit(1);
    }

    return dummy;
}

/** myrealloc */
void *myrealloc(void *ptr, size_t size, const char *desc)
{
    void *dummy = NULL;

    if (size == 0) {
        free(ptr);
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Warning! Function %s tries to resize memoryblock at %p to 0 bytes!")
            ,desc, ptr);
    } else {
        dummy = realloc(ptr, size);
        if (!dummy) {
            MOTION_LOG(EMG, TYPE_ALL, NO_ERRNO
                ,_("Could not resize memory-block at offset %p to %llu bytes (function %s)!")
                ,ptr, (unsigned long long)size, desc);
            exit(1);
        }
    }

    return dummy;
}


/**
 * mycreate_path
 *
 *   This function creates a whole path, like mkdir -p. Example paths:
 *      this/is/an/example/
 *      /this/is/an/example/
 *   Warning: a path *must* end with a slash!
 *
 * Parameters:
 *
 *   cam  - current thread's context structure (for logging)
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 */
int mycreate_path(const char *path)
{
    char *start;
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    if (path[0] == '/') {
        start = (char*)strchr(path + 1, '/');
    } else {
        start = (char*)strchr(path, '/');
    }

    while (start) {
        char *buffer = mystrdup(path);
        buffer[start-path] = 0x00;

        if (mkdir(buffer, mode) == -1 && errno != EEXIST) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Problem creating directory %s"), buffer);
            free(buffer);
            return -1;
        }

        start = strchr(start + 1, '/');

        if (!start) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("creating directory %s"), buffer);
        }

        free(buffer);
    }

    return 0;
}

/** myfopen */
FILE * myfopen(const char *path, const char *mode)
{
    /* first, just try to open the file */
    FILE *dummy = fopen(path, mode);
    if (dummy) {
        return dummy;
    }

    /* could not open file... */
    /* path did not exist? */
    if (errno == ENOENT) {

        /* create path for file... */
        if (mycreate_path(path) == -1) {
            return NULL;
        }

        /* and retry opening the file */
        dummy = fopen(path, mode);
    }
    if (!dummy) {
        /*
         * Two possibilities
         * 1: there was an other error while trying to open the file for the
         * first time
         * 2: could still not open the file after the path was created
         */
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Error opening file %s with mode %s"), path, mode);
        return NULL;
    }

    return dummy;
}

/** myfclose */
int myfclose(FILE* fh)
{
    int rval = fclose(fh);

    if (rval != 0) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error closing file"));
    }

    return rval;
}

/**
 * mystrftime_long
 *
 *   Motion-specific long form of format specifiers.
 *
 * Parameters:
 *
 *   cam        - current thread's context structure.
 *   width      - width associated with the format specifier.
 *   word       - beginning of the format specifier's word.
 *   l          - length of the format specifier's word.
 *   out        - output buffer where to store the result. Size: PATH_MAX.
 *
 * This is called if a format specifier with the format below was found:
 *
 *   % { word }
 *
 * As a special edge case, an incomplete format at the end of the string
 * is processed as well:
 *
 *   % { word \0
 *
 * Any valid format specified width is supported, e.g. "%12{host}".
 *
 * The following specifier keywords are currently supported:
 *
 * host    Replaced with the name of the local machine (see gethostname(2)).
 * fps     Equivalent to %fps.
 */
static void mystrftime_long (const struct ctx_cam *cam,
        int width, const char *word, int l, char *out)
{

    #define SPECIFIERWORD(k) ((strlen(k)==l) && (!strncmp (k, word, l)))

    if (SPECIFIERWORD("host")) {
        snprintf (out, PATH_MAX, "%*s", width, cam->hostname);
        return;
    }
    if (SPECIFIERWORD("fps")) {
        sprintf(out, "%*d", width, cam->movie_fps);
        return;
    }
    if (SPECIFIERWORD("dbeventid")) {
        sprintf(out, "%*llu", width, cam->database_event_id);
        return;
    }
    if (SPECIFIERWORD("ver")) {
        sprintf(out, "%*s", width, VERSION);
        return;
    }
    if (SPECIFIERWORD("sdevx")) {
        sprintf(out, "%*d", width,  cam->current_image->location.stddev_x);
        return;
    }
    if (SPECIFIERWORD("sdevy")) {
        sprintf(out, "%*d", width,  cam->current_image->location.stddev_y);
        return;
    }
    if (SPECIFIERWORD("sdevxy")) {
        sprintf(out, "%*d", width,  cam->current_image->location.stddev_xy);
        return;
    }
    if (SPECIFIERWORD("ratio")) {
        sprintf(out, "%*d", width,  cam->current_image->diffs_ratio);
        return;
    }
    if (SPECIFIERWORD("secdetect")) {
        if (cam->algsec_inuse) {
            if (cam->algsec->isdetected) {
                sprintf(out, "%*s", width, "Y");
            } else {
                sprintf(out, "%*s", width, "N");
            }
        } else {
            sprintf(out, "%*s", width, "N");
        }
        return;
    }


    // Not a valid modifier keyword. Log the error and ignore.
    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
        _("invalid format specifier keyword %*.*s"), l, l, word);

    // Do not let the output buffer empty, or else where to restart the
    // interpretation of the user string will become dependent to far too
    // many conditions. Maybe change loop to "if (*pos_userformat == '%') {
    // ...} __else__ ..."?
    out[0] = '~'; out[1] = 0;
}

/**
 * mystrftime
 *
 *   Motion-specific variant of strftime(3) that supports additional format
 *   specifiers in the format string.
 *
 * Parameters:
 *
 *   cam        - current thread's context structure
 *   s          - destination string
 *   max        - max number of bytes to write
 *   userformat - format string
 *   tm         - time information
 *   filename   - string containing full path of filename
 *                set this to NULL if not relevant
 *   sqltype    - Filetype as used in SQL feature, set to 0 if not relevant
 *
 * Returns: number of bytes written to the string s
 */
size_t mystrftime(const struct ctx_cam *cam, char *s, size_t max, const char *userformat,
        const struct timespec *ts1, const char *filename, int sqltype)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;
    int width;
    struct tm timestamp_tm;

    localtime_r(&ts1->tv_sec, &timestamp_tm);

    format = formatstring;

    /* if mystrftime is called with userformat = NULL we return a zero length string */
    if (userformat == NULL) {
        *s = '\0';
        return 0;
    }

    for (pos_userformat = userformat; *pos_userformat; ++pos_userformat) {

        if (*pos_userformat == '%') {
            /*
             * Reset 'tempstr' to point to the beginning of 'tempstring',
             * otherwise we will eat up tempstring if there are many
             * format specifiers.
             */
            tempstr = tempstring;
            tempstr[0] = '\0';
            width = 0;
            while ('0' <= pos_userformat[1] && pos_userformat[1] <= '9') {
                width *= 10;
                width += pos_userformat[1] - '0';
                ++pos_userformat;
            }

            switch (*++pos_userformat) {
            case '\0': // end of string
                --pos_userformat;
                break;

            case 'v': // event
                sprintf(tempstr, "%0*d", width ? width : 2, cam->event_nr);
                break;

            case 'q': // shots
                sprintf(tempstr, "%0*d", width ? width : 2,
                    cam->current_image->shot);
                break;

            case 'D': // diffs
                sprintf(tempstr, "%*d", width, cam->current_image->diffs);
                break;

            case 'N': // noise
                sprintf(tempstr, "%*d", width, cam->noise);
                break;

            case 'i': // motion width
                sprintf(tempstr, "%*d", width,
                    cam->current_image->location.width);
                break;

            case 'J': // motion height
                sprintf(tempstr, "%*d", width,
                    cam->current_image->location.height);
                break;

            case 'K': // motion center x
                sprintf(tempstr, "%*d", width, cam->current_image->location.x);
                break;

            case 'L': // motion center y
                sprintf(tempstr, "%*d", width, cam->current_image->location.y);
                break;

            case 'o': // threshold
                sprintf(tempstr, "%*d", width, cam->threshold);
                break;

            case 'Q': // number of labels
                sprintf(tempstr, "%*d", width,
                    cam->current_image->total_labels);
                break;

            case 't': // camera id
                sprintf(tempstr, "%*d", width, cam->camera_id);
                break;

            case 'C': // text_event
                if (cam->text_event_string[0]) {
                    snprintf(tempstr, PATH_MAX, "%*s", width,
                        cam->text_event_string);
                } else {
                    ++pos_userformat;
                }
                break;

            case 'w': // picture width
                sprintf(tempstr, "%*d", width, cam->imgs.width);
                break;

            case 'h': // picture height
                sprintf(tempstr, "%*d", width, cam->imgs.height);
                break;

            case 'f': // filename
                if (filename) {
                    snprintf(tempstr, PATH_MAX, "%*s", width, filename);
                } else {
                    ++pos_userformat;
                }
                break;

            case 'n': // sqltype
                if (sqltype) {
                    sprintf(tempstr, "%*d", width, sqltype);
                } else {
                    ++pos_userformat;
                }
                break;

            case '{': // long format specifier word.
                {
                    const char *word = ++pos_userformat;
                    while ((*pos_userformat != '}') && (*pos_userformat != 0))
                        ++pos_userformat;
                    mystrftime_long (cam, width, word, (int)(pos_userformat-word), tempstr);
                    if (*pos_userformat == '\0') {
                        --pos_userformat;
                    }
                }
                break;

            case '$': // thread name
                if (cam->conf->camera_name != "") {
                    cam->conf->camera_name.copy(tempstr, PATH_MAX);
                } else {
                    ++pos_userformat;
                }
                break;

            default: // Any other code is copied with the %-sign
                *format++ = '%';
                *format++ = *pos_userformat;
                continue;
            }

            /*
             * If a format specifier was found and used, copy the result from
             * 'tempstr' to 'format'.
             */
            if (tempstr[0]) {
                while ((*format = *tempstr++) != '\0') {
                    ++format;
                }
                continue;
            }
        }

        /* For any other character than % we just simply copy the character */
        *format++ = *pos_userformat;
    }

    *format = '\0';
    format = formatstring;

    return strftime(s, max, format, &timestamp_tm);
}

void mythreadname_set(const char *abbr, int threadnbr, const char *threadname)
{
    /* When the abbreviation is sent in as null, that means we are being
     * provided a fully filled out thread name (usually obtained from a
     * previously called get_threadname so we set it without additional
     *  formatting.
     */

    char tname[32];
    if (abbr != NULL) {
        snprintf(tname, sizeof(tname), "%s%02d%s%s",abbr,threadnbr,
             threadname ? ":" : "",
             threadname ? threadname : "");
    } else {
        snprintf(tname, sizeof(tname), "%s",threadname);
    }

    #ifdef __APPLE__
        pthread_setname_np(tname);
    #elif defined(BSD)
        pthread_set_name_np(pthread_self(), tname);
    #elif HAVE_PTHREAD_SETNAME_NP
        pthread_setname_np(pthread_self(), tname);
    #else
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Unable to set thread name %s"), tname);
    #endif

}

void mythreadname_get(char *threadname)
{
    #if ((!defined(BSD) && HAVE_PTHREAD_GETNAME_NP) || defined(__APPLE__))
        char currname[16];
        pthread_getname_np(pthread_self(), currname, sizeof(currname));
        snprintf(threadname, sizeof(currname), "%s",currname);
    #else
        snprintf(threadname, 8, "%s","Unknown");
    #endif
}

bool mycheck_passthrough(struct ctx_cam *cam)
{
    #if (MYFFVER >= 57041)
        if (cam->movie_passthrough) {
            return true;
        } else {
            return false;
        }
    #else
        if (cam->movie_passthrough) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("FFMPEG version too old. Disabling pass-through processing."));
        }
        return false;
    #endif

}

static void mytranslate_locale_chg(const char *langcd)
{
    #ifdef HAVE_GETTEXT
        /* This routine is for development testing only.  It is not used for
        * regular users because once this locale is change, it changes the
        * whole computer over to the new locale.  Therefore, we just return
        */
        return;

        setenv ("LANGUAGE", langcd, 1);
        /* Invoke external function to change locale*/
        ++_nl_msg_cat_cntr;
    #else
        if (langcd != NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
        }
    #endif
}

void mytranslate_init(void)
{
    #ifdef HAVE_GETTEXT
        mytranslate_text("", 1);
        setlocale (LC_ALL, "");

        //translate_locale_chg("li");
        mytranslate_locale_chg("es");

        bindtextdomain ("motion", LOCALEDIR);
        bind_textdomain_codeset ("motion", "UTF-8");
        textdomain ("motion");

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Language: English"));

    #else
        /* Disable native language support */
        mytranslate_text("", 0);

        /* This avoids a unused function warning */
        mytranslate_locale_chg("en");
    #endif
}

char* mytranslate_text(const char *msgid, int setnls)
{
    static bool nls_enabled = true;

    if (setnls == 0) {
        if (nls_enabled) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Disabling native language support"));
        }
        nls_enabled = false;
        return NULL;

    } else if (setnls == 1) {
        if (!nls_enabled) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Enabling native language support"));
        }
        nls_enabled = true;
        return NULL;

    } else {
        #ifdef HAVE_GETTEXT
            if (nls_enabled) {
                return (char*)gettext(msgid);
            } else {
                return (char*)msgid;
            }
        #else
            return (char*)msgid;
        #endif
    }
}

/**
 * mystrcpy
 *      Is used to assign string type fields (e.g. config options)
 *      In a way so that we the memory is malloc'ed to fit the string.
 *      If a field is already pointing to a string (not NULL) the memory of the
 *      old string is free'd and new memory is malloc'ed and filled with the
 *      new string is copied into the the memory and with the char pointer
 *      pointing to the new string.
 *
 *      from - pointer to the new string we want to copy
 *      to   - the pointer to the current string (or pointing to NULL)
 *              If not NULL the memory it points to is free'd.
 *
 * Returns pointer to the new string which is in malloc'ed memory
 * FIXME The strings that are malloc'ed with this function should be freed
 * when the motion program is terminated normally instead of relying on the
 * OS to clean up.
 */
char *mystrcpy(char *to, const char *from)
{
    /*
     * Free the memory used by the to string, if such memory exists,
     * and return a pointer to a freshly malloc()'d string with the
     * same value as from.
     */

    if (to != NULL) {
        free(to);
    }

    return mystrdup(from);
}

/**
 * mystrdup
 *      Truncates the string to the length given by the environment
 *      variable PATH_MAX to ensure that config options can always contain
 *      a really long path but no more than that.
 *
 * Returns a pointer to a freshly malloc()'d string with the same
 *      value as the string that the input parameter 'from' points to,
 *      or NULL if the from string is 0 characters.
 */
char *mystrdup(const char *from)
{
    char *tmp;
    size_t stringlength;

    if (from == NULL || !strlen(from)) {
        tmp = NULL;
    } else {
        stringlength = strlen(from);
        stringlength = (stringlength < PATH_MAX ? stringlength : PATH_MAX);
        tmp = (char*)mymalloc(stringlength + 1);
        strncpy(tmp, from, stringlength);

        /*
         * We must ensure the string always has a NULL terminator.
         * This necessary because strncpy will not append a NULL terminator
         * if the original string is greater than string length.
         */
        tmp += stringlength;
        *tmp = '\0';
        tmp -= stringlength;
    }

    return tmp;
}

/****************************************************************************
 *  The section below is the "my" section of functions.
 *  These are designed to be extremely simple version specific
 *  variants of the libav functions.
 ****************************************************************************/

/*********************************************/
AVFrame *myframe_alloc(void)
{
    AVFrame *pic;
    #if (MYFFVER >= 55000)
        pic = av_frame_alloc();
    #else
        pic = avcodec_alloc_frame();
    #endif
    return pic;
}
/*********************************************/
void myframe_free(AVFrame *frame)
{
    #if (MYFFVER >= 55000)
        av_frame_free(&frame);
    #else
        av_freep(&frame);
    #endif
}
/*********************************************/
int myimage_get_buffer_size(enum MyPixelFormat pix_fmt, int width, int height)
{
    int retcd = 0;
    #if (MYFFVER >= 57000)
        int align = 1;
        retcd = av_image_get_buffer_size(pix_fmt, width, height, align);
    #else
        retcd = avpicture_get_size(pix_fmt, width, height);
    #endif
    return retcd;
}
/*********************************************/
int myimage_copy_to_buffer(AVFrame *frame, uint8_t *buffer_ptr, enum MyPixelFormat pix_fmt
        , int width, int height, int dest_size)
{
    int retcd = 0;
    #if (MYFFVER >= 57000)
        int align = 1;
        retcd = av_image_copy_to_buffer((uint8_t *)buffer_ptr,dest_size
            ,(const uint8_t * const*)frame,frame->linesize,pix_fmt,width,height,align);
    #else
        retcd = avpicture_layout((const AVPicture*)frame,pix_fmt,width,height
            ,(unsigned char *)buffer_ptr,dest_size);
    #endif
    return retcd;
}
/*********************************************/
int myimage_fill_arrays(AVFrame *frame,uint8_t *buffer_ptr,enum MyPixelFormat pix_fmt
        , int width,int height)
{
    int retcd = 0;
    #if (MYFFVER >= 57000)
        int align = 1;
        retcd = av_image_fill_arrays(
            frame->data
            ,frame->linesize
            ,buffer_ptr
            ,pix_fmt
            ,width
            ,height
            ,align
        );
    #else
        retcd = avpicture_fill(
            (AVPicture *)frame
            ,buffer_ptr
            ,pix_fmt
            ,width
            ,height);
    #endif
    return retcd;
}
/*********************************************/
void mypacket_free(AVPacket *pkt)
{
    #if (MYFFVER >= 57041)
        av_packet_free(&pkt);
    #else
        av_free_packet(pkt);
    #endif

}
/*********************************************/
void myavcodec_close(AVCodecContext *codec_context)
{
    #if (MYFFVER >= 57041)
        avcodec_free_context(&codec_context);
    #else
        avcodec_close(codec_context);
    #endif
}
/*********************************************/
int mycopy_packet(AVPacket *dest_pkt, AVPacket *src_pkt)
{
    #if (MYFFVER >= 55000)
        return av_packet_ref(dest_pkt, src_pkt);
    #else
        /* Old versions of libav do not support copying packet
        * We therefore disable the pass through recording and
        * for this function, simply do not do anything
        */
        if (dest_pkt == src_pkt ) {
            return 0;
        } else {
            return 0;
        }
    #endif
}
/*********************************************/
AVPacket *mypacket_alloc(AVPacket *pkt)
{
    if (pkt != NULL) {
        mypacket_free(pkt);
    };
    pkt = av_packet_alloc();
    #if (MYFFVER < 58076)
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;
        return pkt;
    #endif
}

/*********************************************/

void util_parms_free(struct ctx_params *params)
{
    int indx_parm;

    if (params == NULL ) {
        return;
    }

    for (indx_parm=0; indx_parm<params->params_count; indx_parm++) {
        if (params->params_array[indx_parm].param_name != NULL) {
            free(params->params_array[indx_parm].param_name);
            params->params_array[indx_parm].param_name = NULL;
        }
        if (params->params_array[indx_parm].param_value != NULL) {
            free(params->params_array[indx_parm].param_value);
            params->params_array[indx_parm].param_value = NULL;
        }
    }

    if (params->params_array != NULL) {
      free(params->params_array);
      params->params_array = NULL;
    }

    params->params_count = 0;

}

static void util_parms_add(struct ctx_params *params, const char *parm_nm, const char *parm_val)
{
    params->params_count++;

    if (params->params_count == 1) {
        params->params_array =(struct ctx_params_item *) mymalloc(sizeof(struct ctx_params_item));
    } else {
        params->params_array =(struct ctx_params_item *)realloc(params->params_array
            , sizeof(struct ctx_params_item)*params->params_count);
    }

    if (parm_nm != NULL) {
        params->params_array[params->params_count-1].param_name =(char*)mymalloc(strlen(parm_nm)+1);
        sprintf(params->params_array[params->params_count-1].param_name,"%s",parm_nm);
    } else {
        params->params_array[params->params_count-1].param_name = NULL;
    }

    if (parm_val != NULL) {
        params->params_array[params->params_count-1].param_value =(char*)mymalloc(strlen(parm_val)+1);
        sprintf(params->params_array[params->params_count-1].param_value,"%s",parm_val);
    } else {
        params->params_array[params->params_count-1].param_value = NULL;
    }

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Parsed: >%s< >%s<")
        ,params->params_array[params->params_count-1].param_name
        ,params->params_array[params->params_count-1].param_value);

}

static void util_parms_strip_qte(std::string &parm)
{

    if (parm.length() == 0) {
        return;
    }

    if (parm.substr(0, 1)=="\"") {
        if (parm.length() == 1) {
            parm = "";
            return;
        } else {
            parm = parm.substr(1);
        }
    }

    if (parm.substr(parm.length() -1, 1)== "\"") {
        if (parm.length() == 1) {
            parm = "";
            return;
        } else {
            parm = parm.substr(0, parm.length() - 1);
        }
    }

}

static void util_parms_extract(struct ctx_params *params, std::string &parmline
        ,size_t indxnm_st,size_t indxnm_en,size_t indxvl_st,size_t indxvl_en)
{
    std::string parm_nm, parm_vl;

    if ((indxnm_st != std::string::npos) &&
        (indxnm_en != std::string::npos) &&
        (indxvl_st != std::string::npos) &&
        (indxvl_en != std::string::npos)) {

        parm_nm = parmline.substr(indxnm_st, indxnm_en - indxnm_st + 1);
        parm_vl = parmline.substr(indxvl_st, indxvl_en - indxvl_st + 1);

        mytrim(parm_nm);
        mytrim(parm_vl);

        util_parms_strip_qte(parm_nm);
        util_parms_strip_qte(parm_vl);

        util_parms_add(params, parm_nm.c_str(), parm_vl.c_str());

    }

}

/* Cut out the parameter that was just extracted from the parmline string */
static void util_parms_next(std::string &parmline, size_t indxnm_st, size_t indxvl_en)
{
    size_t indxcm;

    indxcm = parmline.find(",", indxvl_en);

    if (indxnm_st == 0) {
        if ((indxcm + 1) > parmline.length() ) {
            parmline = "";
        } else {
            parmline = parmline.substr(indxcm + 1);
        }
    } else {
        if ((indxcm + 1) > parmline.length() ) {
            parmline = parmline.substr(0, indxnm_st - 1);
        } else {
            parmline = parmline.substr(0, indxnm_st - 1) + parmline.substr(indxcm + 1);
        }
    }
    mytrim(parmline);

}

void util_parms_parse_qte(struct ctx_params *params, std::string &parmline)
{
    size_t indxnm_st, indxnm_en;
    size_t indxvl_st, indxvl_en;
    size_t indxcm, indxeq;

    /* Parse out all the items within quotes first */
    while (parmline.find("\"", 0) != std::string::npos) {

        indxnm_st = parmline.find("\"", 0);

        indxnm_en = parmline.find("\"", indxnm_st + 1);
        if (indxnm_en == std::string::npos) {
            indxnm_en = parmline.length() - 1;
        }

        indxcm = parmline.find(",", indxnm_en + 1);
        if (indxcm == std::string::npos) {
            indxcm = parmline.length() - 1;
        }

        indxeq = parmline.find("=", indxnm_en + 1);
        if (indxeq == std::string::npos) {
            indxeq = parmline.length() - 1;
        }

        if (indxcm <= indxeq) {
            /* The quoted part of the parm was the value not the name */
            indxvl_st = indxnm_st;
            indxvl_en = indxnm_en;

            indxnm_st = parmline.find_last_of(",", indxvl_st);
            if (indxnm_st == std::string::npos) {
                indxnm_st = 0;
            } else {
                indxnm_st++; /* Move past the comma */
            }

            indxnm_en = parmline.find("=", indxnm_st);
            if ((indxnm_en == std::string::npos) ||
                (indxnm_en > indxvl_st)) {
                indxnm_en = indxvl_st + 1;
            }
            indxnm_en--; /* do not include the = */

        } else {
            /* The quoted part of the parm was the name */
            indxvl_st = parmline.find("\"",indxeq + 1);
            indxcm = parmline.find(",", indxeq + 1);
            if (indxcm == std::string::npos) {
                if (indxnm_st == std::string::npos) {
                    indxvl_st = indxeq + 1;
                    if (indxvl_st >= parmline.length()) {
                        indxvl_st = parmline.length() - 1;
                    }
                    indxvl_en = parmline.length() - 1;
                } else {
                    /* The value is also enclosed in quotes */
                    indxvl_en=parmline.find("\"", indxvl_st + 1);
                    if (indxvl_en == std::string::npos) {
                        indxvl_en = parmline.length() - 1;
                    }
                }
            } else if (indxvl_st == std::string::npos) {
                /* There are no more quotes in the line */
                indxvl_st = indxeq + 1;
                indxvl_en = parmline.find(",",indxvl_st) - 1;
            } else if (indxcm < indxvl_st) {
                /* The quotes belong to next item */
                indxvl_st = indxeq + 1;
                indxvl_en = parmline.find(",",indxvl_st) - 1;
            } else {
                /* The value is also enclosed in quotes */
                indxvl_en=parmline.find("\"", indxvl_st + 1);
                if (indxvl_en == std::string::npos) {
                    indxvl_en = parmline.length() - 1;
                }
            }
        }

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("Parsing: >%s< >%ld %ld %ld %ld<")
            ,parmline.c_str(), indxnm_st, indxnm_en, indxvl_st, indxvl_en);

        util_parms_extract(params, parmline, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        util_parms_next(parmline, indxnm_st, indxvl_en);

    }
}

void util_parms_parse_comma(struct ctx_params *params, std::string &parmline)
{
    size_t indxnm_st, indxnm_en;
    size_t indxvl_st, indxvl_en;

    while (parmline.find(",", 0) != std::string::npos) {
        indxnm_st = 0;
        indxnm_en = parmline.find("=", 1);
        if (indxnm_en == std::string::npos) {
            indxnm_en = 0;
            indxvl_st = 0;
        } else {
            indxvl_st = indxnm_en + 1;  /* Position past = */
            indxnm_en--;                /* Position before = */
        }

        if (parmline.find(",", indxvl_st) == std::string::npos) {
            indxvl_en = parmline.length() - 1;
        } else {
            indxvl_en = parmline.find(",",indxvl_st) - 1;
        }

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("Parsing: >%s< >%ld %ld %ld %ld<")
            ,parmline.c_str(), indxnm_st, indxnm_en, indxvl_st, indxvl_en);

        util_parms_extract(params, parmline, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        util_parms_next(parmline, indxnm_st, indxvl_en);
    }

    /* Take care of last parameter */
    if (parmline != "") {
        indxnm_st = 0;
        indxnm_en = parmline.find("=", 1);
        if (indxnm_en == std::string::npos) {
            /* If no value then we are done */
            return;
        } else {
            indxvl_st = indxnm_en + 1;  /* Position past = */
            indxnm_en--;                /* Position before = */
        }
        indxvl_en = parmline.length() - 1;

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("Parsing: >%s< >%ld %ld %ld %ld<")
            ,parmline.c_str(), indxnm_st, indxnm_en, indxvl_st, indxvl_en);

        util_parms_extract(params, parmline, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        util_parms_next(parmline, indxnm_st, indxvl_en);
    }

}

/* Parse through the config line and put into the array */
void util_parms_parse(struct ctx_params *params, std::string confline)
{
    /* Parse through the configuration option to get values
     * The values are separated by commas but may also have
     * double quotes around the names which include a comma.
     * Examples:
     * v4l2_params ID01234= 1, ID23456=2
     * v4l2_params "Brightness, auto" = 1, ID23456=2
     * v4l2_params ID23456=2, "Brightness, auto" = 1,ID2222=5
     */

    std::string parmline;

    if ((params->update_params == false) ||
        (confline == "")) {
        return;
    }
    /* We make a copy because the parsing destroys the value passed */
    parmline = confline;

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Starting parsing parameters: %s"), parmline.c_str());

    util_parms_free(params);

    util_parms_parse_qte(params, parmline);

    util_parms_parse_comma(params, parmline);

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Finished parsing parameters: %s"), confline.c_str());

    params->update_params = false;

    return;

}

void util_parms_add_default(ctx_params *params, std::string parm_nm, std::string parm_vl)
{
    int indx;
    bool dflt;

    dflt = true;
    for (indx = 0; indx < params->params_count; indx++) {
        if (mystreq(params->params_array[indx].param_name, parm_nm.c_str()) ) {
            dflt = false;
        }
    }
    if (dflt == true) {
        util_parms_add(params, parm_nm.c_str(), parm_vl.c_str());
    }

}

/* Update config line with the values from the params array */
void util_parms_update(struct ctx_params *params, std::string &confline)
{
    int indx;
    char *tst;
    std::string parmline;

    for (indx = 0; indx < params->params_count; indx++) {
        if (indx == 0) {
            parmline = " ";
        } else {
            parmline += ",";
        }
        tst = strstr(params->params_array[indx].param_name," ");
        if (tst == NULL) {
            parmline += params->params_array[indx].param_name;
        } else {
            parmline += "\"";
            parmline += params->params_array[indx].param_name;
            parmline += "\"";
        }
        parmline += "=";

        tst = strstr(params->params_array[indx].param_value," ");
        if (tst == NULL) {
            parmline += params->params_array[indx].param_value;
        } else {
            parmline += "\"";
            parmline += params->params_array[indx].param_value;
            parmline += "\"";
        }

    }
    parmline += " ";

    confline = parmline;

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("New config: %s"), confline.c_str());

    return;

}

/**
 * util_exec_command
 *      Execute 'command' with 'arg' as its argument.
 *      if !arg command is started with no arguments
 *      Before we call execl we need to close all the file handles
 *      that the fork inherited from the parent in order not to pass
 *      the open handles on to the shell
 */
void util_exec_command(struct ctx_cam *cam, const char *command, char *filename, int filetype)
{
    char stamp[PATH_MAX];
    mystrftime(cam, stamp, sizeof(stamp), command, &cam->current_image->imgts, filename, filetype);

    if (!fork()) {
        int i;

        /* Detach from parent */
        setsid();

        /*
         * Close any file descriptor except console because we will
         * like to see error messages
         */
        for (i = getdtablesize() - 1; i > 2; i--)
            close(i);

        execl("/bin/sh", "sh", "-c", stamp, " &",(char*)NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO
            ,_("Unable to start external command '%s'"), stamp);

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO
        ,_("Executing external command '%s'"), stamp);
}

