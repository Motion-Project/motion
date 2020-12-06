/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *      util.c
 *
 *      Module of utility and "my" functions used across Motion application.
 *
 */

#include "translate.h"
#include "motion.h"
#include "logger.h"
#include "util.h"

#ifdef HAVE_FFMPEG

/*********************************************/
AVFrame *my_frame_alloc(void)
{
    AVFrame *pic;
    #if ( MYFFVER >= 55000)
        pic = av_frame_alloc();
    #else
        pic = avcodec_alloc_frame();
    #endif
    return pic;
}
/*********************************************/
void my_frame_free(AVFrame *frame)
{
    #if ( MYFFVER >= 55000)
        av_frame_free(&frame);
    #else
        av_freep(&frame);
    #endif
}
/*********************************************/
int my_image_get_buffer_size(enum MyPixelFormat pix_fmt, int width, int height)
{
    int retcd = 0;
    #if ( MYFFVER >= 57000)
        int align = 1;
        retcd = av_image_get_buffer_size(pix_fmt, width, height, align);
    #else
        retcd = avpicture_get_size(pix_fmt, width, height);
    #endif
    return retcd;
}
/*********************************************/
int my_image_copy_to_buffer(AVFrame *frame, uint8_t *buffer_ptr, enum MyPixelFormat pix_fmt,int width, int height,int dest_size)
{
    int retcd = 0;
    #if ( MYFFVER >= 57000)
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
int my_image_fill_arrays(AVFrame *frame,uint8_t *buffer_ptr,enum MyPixelFormat pix_fmt,int width,int height)
{
    int retcd = 0;
    #if ( MYFFVER >= 57000)
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
void my_packet_unref(AVPacket pkt)
{
    #if ( MYFFVER >= 57000)
        av_packet_unref(&pkt);
    #else
        av_free_packet(&pkt);
    #endif
}
/*********************************************/
void my_avcodec_close(AVCodecContext *codec_context)
{
    #if ( MYFFVER >= 57041)
        avcodec_free_context(&codec_context);
    #else
        avcodec_close(codec_context);
    #endif
}
/*********************************************/
int my_copy_packet(AVPacket *dest_pkt, AVPacket *src_pkt)
{
    #if ( MYFFVER >= 55000)
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

#endif

/*********************************************/

/**
 * mymalloc
 *
 *   Allocates some memory and checks if that succeeded or not. If it failed,
 *   do some errorlogging and bail out.
 *
 *   NOTE: Kenneth Lavrsen changed printing of size_t types so instead of using
 *   conversion specifier %zd I changed it to %llu and casted the size_t
 *   variable to unsigned long long. The reason for this nonsense is that older
 *   versions of gcc like 2.95 uses %Zd and does not understand %zd. So to avoid
 *   this mess I used a more generic way. Long long should have enough bits for
 *   64-bit machines with large memory areas.
 *
 * Parameters:
 *
 *   nbytes - no. of bytes to allocate
 *
 * Returns: a pointer to the allocated memory
 */
void *mymalloc(size_t nbytes)
{
    void *dummy = calloc(nbytes, 1);

    if (!dummy) {
        MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, _("Could not allocate %llu bytes of memory!")
            ,(unsigned long long)nbytes);
        motion_remove_pid();
        exit(1);
    }

    return dummy;
}

/**
 * myrealloc
 *
 *   Re-allocate (i.e., resize) some memory and check if that succeeded or not.
 *   If it failed, do some errorlogging and bail out. If the new memory size
 *   is 0, the memory is freed.
 *
 * Parameters:
 *
 *   ptr  - pointer to the memory to resize/reallocate
 *   size - new memory size
 *   desc - name of the calling function
 *
 * Returns: a pointer to the reallocated memory, or NULL if the memory was
 *          freed
 */
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
            motion_remove_pid();
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
 *   cnt  - current thread's context structure (for logging)
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 */
int mycreate_path(const char *path)
{
    char *start;
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    if (path[0] == '/') {
        start = strchr(path + 1, '/');
    } else {
        start = strchr(path, '/');
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

/**
 * myfopen
 *
 *   This function opens a file, if that failed because of an ENOENT error
 *   (which is: path does not exist), the path is created and then things are
 *   tried again. This is faster then trying to create that path over and over
 *   again. If someone removes the path after it was created, myfopen will
 *   recreate the path automatically.
 *
 * Parameters:
 *
 *   path - path to the file to open
 *   mode - open mode
 *
 * Returns: the file stream object
 */
FILE *myfopen(const char *path, const char *mode)
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

/**
 * myfclose
 *
 *  Motion-specific variant of fclose()
 *
 * Returns: fclose() return value
 */
int myfclose(FILE *fh)
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
 *   cnt        - current thread's context structure.
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
static void mystrftime_long (const struct context *cnt, int width, const char *word, int l, char *out)
{
    #define SPECIFIERWORD(k) ((strlen(k)==l) && (!strncmp (k, word, l)))

    if (SPECIFIERWORD("host")) {
        snprintf (out, PATH_MAX, "%*s", width, cnt->hostname);
        return;
    }
    if (SPECIFIERWORD("fps")) {
        sprintf(out, "%*d", width, cnt->movie_fps);
        return;
    }
    if (SPECIFIERWORD("dbeventid")) {
        sprintf(out, "%*llu", width, cnt->database_event_id);
        return;
    }
    if (SPECIFIERWORD("ver")) {
        sprintf(out, "%*s", width, VERSION);
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
 *   cnt        - current thread's context structure
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
size_t mystrftime(const struct context *cnt, char *s, size_t max, const char *userformat
            , const struct timeval *tv1, const char *filename, int sqltype)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;
    int width;
    struct tm timestamp_tm;

    localtime_r(&tv1->tv_sec, &timestamp_tm);

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
                sprintf(tempstr, "%0*d", width ? width : 2, cnt->event_nr);
                break;

            case 'q': // shots
                sprintf(tempstr, "%0*d", width ? width : 2,
                    cnt->current_image->shot);
                break;

            case 'D': // diffs
                sprintf(tempstr, "%*d", width, cnt->current_image->diffs);
                break;

            case 'N': // noise
                sprintf(tempstr, "%*d", width, cnt->noise);
                break;

            case 'i': // motion width
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->location.width);
                break;

            case 'J': // motion height
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->location.height);
                break;

            case 'K': // motion center x
                sprintf(tempstr, "%*d", width, cnt->current_image->location.x);
                break;

            case 'L': // motion center y
                sprintf(tempstr, "%*d", width, cnt->current_image->location.y);
                break;

            case 'o': // threshold
                sprintf(tempstr, "%*d", width, cnt->threshold);
                break;

            case 'Q': // number of labels
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->total_labels);
                break;

            case 't': // camera id
                sprintf(tempstr, "%*d", width, cnt->camera_id);
                break;

            case 'C': // text_event
                if (cnt->text_event_string[0]) {
                    snprintf(tempstr, PATH_MAX, "%*s", width,
                        cnt->text_event_string);
                } else {
                    ++pos_userformat;
                }
                break;

            case 'w': // picture width
                sprintf(tempstr, "%*d", width, cnt->imgs.width);
                break;

            case 'h': // picture height
                sprintf(tempstr, "%*d", width, cnt->imgs.height);
                break;

            case 'f': // filename -- or %fps
                if ((*(pos_userformat+1) == 'p') && (*(pos_userformat+2) == 's')) {
                    sprintf(tempstr, "%*d", width, cnt->movie_fps);
                    pos_userformat += 2;
                    break;
                }

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
                    while ((*pos_userformat != '}') && (*pos_userformat != 0)) {
                        ++pos_userformat;
                    }
                    mystrftime_long (cnt, width, word, (int)(pos_userformat-word), tempstr);
                    if (*pos_userformat == '\0') {
                        --pos_userformat;
                    }
                }
                break;

            case '$': // thread name
                if (cnt->conf.camera_name && cnt->conf.camera_name[0]) {
                    snprintf(tempstr, PATH_MAX, "%s", cnt->conf.camera_name);
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

/* This is a temporary location for these util functions.  All the generic utility
 * functions will be collected here and ultimately moved into a new common "util" module
 */
void util_threadname_set(const char *abbr, int threadnbr, const char *threadname)
{
    /* When the abbreviation is sent in as null, that means we are being
     * provided a fully filled out thread name (usually obtained from a
     * previously called get_threadname so we set it without additional
     *  formatting.
     */

    char tname[16];
    if (abbr != NULL) {
        snprintf(tname, sizeof(tname), "%s%d%s%s",abbr,threadnbr,
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

void util_threadname_get(char *threadname)
{

    #if ((!defined(BSD) && HAVE_PTHREAD_GETNAME_NP) || defined(__APPLE__))
        char currname[16];
        pthread_getname_np(pthread_self(), currname, sizeof(currname));
        snprintf(threadname, sizeof(currname), "%s",currname);
    #else
        snprintf(threadname, 8, "%s","Unknown");
    #endif

}

int util_check_passthrough(struct context *cnt)
{
    #if ( MYFFVER < 55000)
        if (cnt->movie_passthrough) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("FFMPEG version too old. Disabling pass-through processing."));
        }
        return 0;
    #else
        if (cnt->movie_passthrough) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("pass-through enabled."));
            return 1;
        } else {
            return 0;
        }
    #endif
}

/* util_trim
 * Trim away any leading or trailing whitespace in the string
*/
void util_trim(char *parm)
{
    int indx, indx_st, indx_en;

    if (parm == NULL) {
        return;
    }

    indx_en = strlen(parm) - 1;
    if (indx_en == -1) {
        return;
    }

    indx_st = 0;

    while (isspace(parm[indx_st]) && (indx_st <= indx_en)) {
        indx_st++;
    }
    if (indx_st > indx_en) {
        parm[0]= '\0';
        return;
    }

    while (isspace(parm[indx_en]) && (indx_en > indx_st)) {
        indx_en--;
    }

    for (indx = indx_st; indx<=indx_en; indx++) {
        parm[indx-indx_st] = parm[indx];
    }
    parm[indx_en-indx_st+1] = '\0';

}

/* util_parms_add
 * Add the parsed out parameter and value to the control array.
*/
static void util_parms_add(struct params_context *parameters
            , const char *parm_nm, const char *parm_vl)
{
    int indx, retcd;

    indx=parameters->params_count;
    parameters->params_count++;

    if (indx == 0) {
        parameters->params_array =(struct params_item_ctx *) mymalloc(sizeof(struct params_item_ctx));
    } else {
        parameters->params_array =
            (struct params_item_ctx *)realloc(parameters->params_array
                , sizeof(struct params_item_ctx)*(indx+1));
    }

    if (parm_nm != NULL) {
        parameters->params_array[indx].param_name = (char*)mymalloc(strlen(parm_nm)+1);
        retcd = sprintf(parameters->params_array[indx].param_name,"%s",parm_nm);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error setting parm >%s<"),parm_nm);
            free(parameters->params_array[indx].param_name);
            parameters->params_array[indx].param_name = NULL;
        }
    } else {
        parameters->params_array[indx].param_name = NULL;
    }

    if (parm_vl != NULL) {
        parameters->params_array[indx].param_value = (char*)mymalloc(strlen(parm_vl)+1);
        retcd = sprintf(parameters->params_array[indx].param_value,"%s",parm_vl);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error setting parm >%s<"),parm_vl);
            free(parameters->params_array[indx].param_value);
            parameters->params_array[indx].param_value = NULL;
        }
    } else {
        parameters->params_array[indx].param_value = NULL;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Parsed: >%s< >%s<")
        ,parameters->params_array[indx].param_name
        ,parameters->params_array[indx].param_value);

}

/* util_parms_extract
 * Extract out of the configuration string the name and values at the location specified.
*/
static void util_parms_extract(struct params_context *parameters
            , char *parmlne, int indxnm_st, int indxnm_en, int indxvl_st, int indxvl_en)
{
    char *parm_nm, *parm_vl;
    int retcd, chksz;

    if ((indxnm_en != 0) &&
        (indxvl_st != 0) &&
        ((indxnm_en - indxnm_st) > 0) &&
        ((indxvl_en - indxvl_st) > 0)) {
        parm_nm = mymalloc(PATH_MAX);
        parm_vl = mymalloc(PATH_MAX);

        chksz = indxnm_en - indxnm_st + 1;
        retcd = snprintf(parm_nm, chksz, "%s", parmlne + indxnm_st);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error parsing parm_nm controls: %s"), parmlne);
            free(parm_nm);
            parm_nm = NULL;
        }

        chksz = indxvl_en - indxvl_st + 1;
        retcd = snprintf(parm_vl, chksz, "%s", parmlne + indxvl_st);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error parsing parm_vl controls: %s"), parmlne);
            free(parm_vl);
            parm_vl = NULL;
        }

        util_trim(parm_nm);
        util_trim(parm_vl);

        util_parms_add(parameters, parm_nm, parm_vl);

        if (parm_nm != NULL) {
            free(parm_nm);
        }
        if (parm_vl != NULL) {
            free(parm_vl);
        }
    }

}

/* util_parms_next
 * Remove the parameter parsed out in previous steps from the parms string
 * and set up the string for parsing out the next parameter.
*/
static void util_parms_next(char *parmlne, int indxnm_st, int indxvl_en)
{
    char *parm_tmp;
    int retcd;

    parm_tmp = mymalloc(PATH_MAX);
    retcd = snprintf(parm_tmp, PATH_MAX, "%s", parmlne);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error setting temp: %s"), parmlne);
        free(parm_tmp);
        return;
    }

    if (indxnm_st == 0) {
        if ((size_t)(indxvl_en + 1) >strlen(parmlne)) {
            parmlne[0]='\0';
        } else {
            retcd = snprintf(parmlne, strlen(parm_tmp) - indxvl_en + 1
                , "%s", parm_tmp+indxvl_en+1);
        }
    } else {
        if ((size_t)(indxvl_en + 1) > strlen(parmlne) ) {
            retcd = snprintf(parmlne, indxnm_st - 1, "%s", parm_tmp);
        } else {
            retcd = snprintf(parmlne, PATH_MAX, "%.*s%.*s"
                , indxnm_st - 1, parm_tmp
                , (int)(strlen(parm_tmp) - indxvl_en)
                , parm_tmp + indxvl_en);
        }
    }
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error reparsing controls: %s"), parmlne);
    }

    free(parm_tmp);

}

/* util_parms_parse_qte
 * Split out the parameters that have quotes around the name.
*/
static void util_parms_parse_qte(struct params_context *parameters, char *parmlne)
{
    int indxnm_st, indxnm_en, indxvl_st, indxvl_en;

    while (strstr(parmlne,"\"") != NULL) {
        indxnm_st = 0;
        indxnm_en = 0;
        indxvl_st = 0;
        indxvl_en = strlen(parmlne);

        indxnm_st = strstr(parmlne,"\"") - parmlne + 1;
        if (strstr(parmlne + indxnm_st,"\"") != NULL) {
            indxnm_en = strstr(parmlne + indxnm_st,"\"") - parmlne;
            if (strstr(parmlne + indxnm_en + 1,"=") != NULL) {
                indxvl_st = strstr(parmlne + indxnm_en + 1,"=") - parmlne + 1;
            }
            if (strstr(parmlne + indxvl_st + 1,",") != NULL) {
                indxvl_en = strstr(parmlne + indxvl_st + 1,",") - parmlne;
            }
        }

        util_parms_extract(parameters, parmlne, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        util_parms_next(parmlne, indxnm_st, indxvl_en);
    }
}

/* util_parms_parse_comma
 * Split out the parameters between the commas.
*/
static void util_parms_parse_comma(struct params_context *parameters, char *parmlne)
{
    int indxnm_st, indxnm_en, indxvl_st, indxvl_en;

    while (strstr(parmlne,",") != NULL) {
        indxnm_st = 0;
        indxnm_en = 0;
        indxvl_st = 0;
        indxvl_en = strstr(parmlne, ",") - parmlne;

        if (strstr(parmlne, "=") != NULL) {
            indxnm_en = strstr(parmlne,"=") - parmlne;
            if ((size_t)indxnm_en < strlen(parmlne)) {
                indxvl_st = indxnm_en + 1;
            }
        }
        util_parms_extract(parameters, parmlne, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        util_parms_next(parmlne, indxnm_st, indxvl_en);
    }

}

/* util_parms_free
 * Free all the memory associated with the parameter control array.
*/
void util_parms_free(struct params_context *parameters)
{
    int indx;

    if (parameters == NULL) {
        return;
    }

    for (indx = 0; indx < parameters->params_count; indx++) {
        if (parameters->params_array[indx].param_name != NULL) {
            free(parameters->params_array[indx].param_name);
        }
        parameters->params_array[indx].param_name = NULL;

        if (parameters->params_array[indx].param_value != NULL) {
            free(parameters->params_array[indx].param_value);
        }
        parameters->params_array[indx].param_value = NULL;
    }

    if (parameters->params_array != NULL) {
        free(parameters->params_array);
    }
    parameters->params_array = NULL;

    parameters->params_count = 0;

}

/* util_parms_parse
 * Parse the user provided string of parameters into a array.
*/
void util_parms_parse(struct params_context *parameters, char *confparm)
{
    /* Parse through the configuration option to get values
     * The values are separated by commas but may also have
     * double quotes around the names which include a comma.
     * Examples:
     * vid_control_params ID01234= 1, ID23456=2
     * vid_control_params "Brightness, auto" = 1, ID23456=2
     * vid_control_params ID23456=2, "Brightness, auto" = 1,ID2222=5
     */

    int retcd, indxnm_st, indxnm_en, indxvl_st, indxvl_en;
    char *parmlne;

    util_parms_free(parameters);
    parmlne = NULL;

    if (confparm != NULL) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            ,_("Parsing controls: %s"), confparm);

        parmlne = mymalloc(PATH_MAX);

        retcd = snprintf(parmlne, PATH_MAX, "%s", confparm);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Error parsing controls: %s"), confparm);
            free(parmlne);
            return;
        }

        util_parms_parse_qte(parameters, parmlne);

        util_parms_parse_comma(parameters, parmlne);

        if (strlen(parmlne) != 0) {
            indxnm_st = 0;
            indxnm_en = 0;
            indxvl_st = 0;
            indxvl_en = strlen(parmlne);
            if (strstr(parmlne + 1,"=") != NULL) {
                indxnm_en = strstr(parmlne + 1,"=") - parmlne;
                if ((size_t)indxnm_en < strlen(parmlne)) {
                    indxvl_st = indxnm_en + 1;
                }
            }

            util_parms_extract(parameters, parmlne, indxnm_st, indxnm_en, indxvl_st, indxvl_en);
        }
        free(parmlne);
    }

    return;

}

void util_parms_add_default(struct params_context *parameters, const char *parm_nm, const char *parm_vl)
{

    int indx, dflt;

    dflt = TRUE;
    for (indx = 0; indx < parameters->params_count; indx++) {
        if ( mystreq(parameters->params_array[indx].param_name, parm_nm) ) {
            dflt = FALSE;
        }
    }
    if (dflt == TRUE) {
        util_parms_add(parameters, parm_nm, parm_vl);
    }

}

/* Update config line with the values from the params array */
void util_parms_update(struct params_context *params, struct context *cnt, const char *cfgitm)
{
    int indx, retcd;
    char *tst;
    char newline[PATH_MAX];
    char hldline[PATH_MAX];

    for (indx = 0; indx < params->params_count; indx++) {
        if (indx == 0){
            retcd = snprintf(newline, PATH_MAX , "%s", " ");
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

            retcd = snprintf(hldline, PATH_MAX, "%s", " ");
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

        } else {
            retcd = snprintf(newline, PATH_MAX, "%s,", hldline);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

            retcd = snprintf(hldline, PATH_MAX, "%s", newline);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }
        }

        tst = strstr(params->params_array[indx].param_name," ");
        if (tst == NULL) {
            retcd = snprintf(newline, PATH_MAX, "%s%s"
                , hldline, params->params_array[indx].param_name);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

            retcd = snprintf(hldline, PATH_MAX, "%s", newline);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

        } else {
            retcd = snprintf(newline, PATH_MAX, "%s\"%s\""
                , hldline, params->params_array[indx].param_name);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }

            retcd = snprintf(hldline, PATH_MAX, "%s", newline);
            if ((retcd < 0) || (retcd > PATH_MAX)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
            }
        }

        retcd = snprintf(newline, PATH_MAX, "%s=%s"
            , hldline, params->params_array[indx].param_value);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
        }

        retcd = snprintf(hldline, PATH_MAX, "%s", newline);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
        }
    }

    if (mystrceq(cfgitm, "netcam_params")) {
        free(cnt->conf.netcam_params);
        cnt->conf.netcam_params = mymalloc(strlen(newline)+1);
        retcd = snprintf(cnt->conf.netcam_params, strlen(newline)+1, "%s", newline);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
        }
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("New netcam_params: %s"), newline);

    } else if (mystrceq(cfgitm, "netcam_high_params")) {
        free(cnt->conf.netcam_high_params);
        cnt->conf.netcam_high_params = mymalloc(strlen(newline)+1);
        retcd = snprintf(cnt->conf.netcam_high_params, strlen(newline)+1, "%s", newline);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
        }
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("New netcam_high_params: %s"), newline);

    } else if (mystrceq(cfgitm, "video_params")) {
        free(cnt->conf.video_params);
        cnt->conf.video_params = mymalloc(strlen(newline)+1);
        retcd = snprintf(cnt->conf.video_params, strlen(newline)+1, "%s", newline);
        if ((retcd < 0) || (retcd > PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Error: %s"), newline);
        }
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("New video_params: %s"), newline);

    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Programming error.  Unknown configuration item: %s"), cfgitm);
    }

    return;

}

/** Non case sensitive equality check for strings*/
int mystrceq(const char *var1, const char *var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return FALSE;
    }
    return (strcasecmp(var1,var2) ? FALSE : TRUE);
}

/** Non case sensitive inequality check for strings*/
int mystrcne(const char *var1, const char *var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return FALSE;
    }
    return (strcasecmp(var1,var2) ? TRUE : FALSE);
}

/** Case sensitive equality check for strings*/
int mystreq(const char *var1, const char *var2)
{
    if ((var1 == NULL) || (var2 == NULL)) {
        return FALSE;
    }
    return (strcmp(var1,var2) ? FALSE : TRUE);
}

/** Case sensitive inequality check for strings*/
int mystrne(const char *var1, const char *var2)
{
    if ((var1 == NULL) ||(var2 == NULL)) {
        return FALSE;
    }
    return (strcmp(var1,var2) ? TRUE : FALSE);
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
        tmp = mymalloc(stringlength + 1);
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
