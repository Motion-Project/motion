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
*/

#ifndef _INCLUDE_WEBU_HPP_
#define _INCLUDE_WEBU_HPP_

    /* Some defines of lengths for our buffers */
    #define WEBUI_LEN_PARM 512          /* Parameters specified */
    #define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
    #define WEBUI_LEN_RESP 1024         /* Initial response size */
    #define WEBUI_MHD_OPTS 10           /* Maximum number of options permitted for MHD */

    #define WEBUI_POST_BFRSZ  512

    enum WEBUI_METHOD {
        WEBUI_METHOD_GET    = 0,
        WEBUI_METHOD_POST   = 1
    };

    enum WEBUI_CNCT {
        WEBUI_CNCT_CONTROL,
        WEBUI_CNCT_FILE,
        WEBUI_CNCT_JPG_FULL,
        WEBUI_CNCT_JPG_SUB,
        WEBUI_CNCT_JPG_MOTION,
        WEBUI_CNCT_JPG_SOURCE,
        WEBUI_CNCT_JPG_SECONDARY,
        WEBUI_CNCT_TS_FULL,
        WEBUI_CNCT_TS_SUB,
        WEBUI_CNCT_TS_MOTION,
        WEBUI_CNCT_TS_SOURCE,
        WEBUI_CNCT_TS_SECONDARY,
        WEBUI_CNCT_UNKNOWN
    };

    enum WEBUI_RESP {
        WEBUI_RESP_HTML     = 0,
        WEBUI_RESP_JSON     = 1,
        WEBUI_RESP_TEXT     = 2
    };

    struct ctx_key {
        char                        *key_nm;        /* Name of the key item */
        char                        *key_val;       /* Value of the key item */
        size_t                      key_sz;         /* The size of the value */
    };

    struct ctx_webui {
        std::string                 url;            /* The URL sent from the client */
        std::string                 uri_camid;      /* Parsed camera number from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd1;       /* Parsed command1 from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd2;       /* Parsed command2 from the url eg /camid/cmd1/cmd2/cmd3 */
        std::string                 uri_cmd3;       /* Parsed command3 from the url eg /camid/cmd1/cmd2/cmd3 */

        std::string                 clientip;       /* IP of the connecting client */
        std::string                 hostfull;       /* Full http name for host with port number */

        char                        *auth_opaque;   /* Opaque string for digest authentication*/
        char                        *auth_realm;    /* Realm string for digest authentication*/
        char                        *auth_user;     /* Parsed user from config authentication string*/
        char                        *auth_pass;     /* Parsed password from config authentication string*/
        int                         authenticated;  /* Boolean for whether authentication has been passed */

        std::string                 resp_page;      /* The response that will be sent */
        unsigned char               *resp_image;    /* Response image to provide to user */
        int                         resp_type;      /* indicator for the type of response to provide. */
        size_t                      resp_size;      /* The allocated size of the response */
        size_t                      resp_used;      /* The amount of the response page used */
        size_t                      aviobuf_sz;     /* The size of the mpegts avio buffer */
        struct timespec             start_time;     /* Start time of the mpegts stream*/

        AVFormatContext             *fmtctx;
        AVCodecContext              *ctx_codec;
        AVFrame                     *picture;       /* contains default image pointers */

        std::string                 lang;           /* Two character abbreviation for locale language*/
        int                         camindx;        /* Index number of the cam */
        int                         device_id;      /* Device id number requested */
        enum WEBUI_CNCT             cnct_type;      /* Type of connection we are processing */

        int                         post_sz;        /* The number of entries in the post info */
        std::string                 post_cmd;       /* The command sent with the post */
        ctx_key                     *post_info;     /* Structure of the entries provided from the post data */
        struct MHD_PostProcessor    *post_processor; /* Processor for handling Post method connections */

        FILE                        *req_file;      /* requested file*/

        enum WEBUI_METHOD           cnct_method;    /* Connection method.  Get or Post */

        uint64_t                    stream_pos;     /* Stream position of sent image */
        int                         stream_fps;     /* Stream rate per second */
        struct timespec             time_last;      /* Keep track of processing time for stream thread*/
        int                         mhd_first;      /* Boolean for whether it is the first connection*/
        struct MHD_Connection       *connection;    /* The MHD connection value from the client */
        ctx_motapp                  *motapp;        /* The motionplus context pointer */
        ctx_dev                     *cam;           /* The ctx_dev information for the camera requested */

    };

    void webu_init(ctx_motapp *motapp);
    void webu_deinit(ctx_motapp *motapp);

#endif /* _INCLUDE_WEBU_HPP_ */
