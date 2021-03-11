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
 *    Copyright 2020 MotionMrDave@gmail.com
*/

#ifndef _INCLUDE_WEBU_H_
#define _INCLUDE_WEBU_H_

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
        WEBUI_CNCT_CONTROL     = 0,
        WEBUI_CNCT_FULL        = 1,
        WEBUI_CNCT_SUB         = 2,
        WEBUI_CNCT_MOTION      = 3,
        WEBUI_CNCT_SOURCE      = 4,
        WEBUI_CNCT_SECONDARY   = 5,
        WEBUI_CNCT_UNKNOWN     = 99
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

    struct webui_ctx {
        std::string                 url;            /* The URL sent from the client */
        std::string                 uri_camid;      /* Parsed camera number from the url eg /camid/cmd1/cmd2 */
        std::string                 uri_cmd1;       /* Parsed command1 from the url eg /camid/cmd1/cmd2 */
        std::string                 uri_cmd2;       /* Parsed command2 from the url eg /camid/cmd1/cmd2 */

        std::string                 clientip;       /* IP of the connecting client */
        std::string                 hostfull;       /* Full http name for host with port number */

        char                        *auth_opaque;   /* Opaque string for digest authentication*/
        char                        *auth_realm;    /* Realm string for digest authentication*/
        char                        *auth_user;     /* Parsed user from config authentication string*/
        char                        *auth_pass;     /* Parsed password from config authentication string*/
        int                         authenticated;  /* Boolean for whether authentication has been passed */

        std::string                 resp_page;      /* The response that will be sent */
        char                        *resp_image;    /* Response image to provide to user */
        int                         resp_type;      /* indicator for the type of response to provide. */
        size_t                      resp_size;      /* The allocated size of the response */
        size_t                      resp_used;      /* The amount of the response page used */

        int                         cam_count;      /* Count of the number of cameras*/
        int                         cam_threads;    /* Count of the number of camera threads running*/
        std::string                 lang;           /* Two character abbreviation for locale language*/
        int                         threadnbr;      /* Thread number provided from the uri */
        enum WEBUI_CNCT             cnct_type;      /* Type of connection we are processing */

        int                         post_sz;        /* The number of entries in the post info */
        std::string                 post_cmd;       /* The command sent with the post */
        struct ctx_key              *post_info;     /* Structure of the entries provided from the post data */
        struct MHD_PostProcessor    *post_processor; /* Processor for handling Post method connections */

        enum WEBUI_METHOD           cnct_method;    /* Connection method.  Get or Post */

        uint64_t                    stream_pos;     /* Stream position of sent image */
        int                         stream_fps;     /* Stream rate per second */
        struct timespec             time_last;      /* Keep track of processing time for stream thread*/
        int                         mhd_first;      /* Boolean for whether it is the first connection*/
        struct MHD_Connection       *connection;    /* The MHD connection value from the client */
        struct ctx_motapp           *motapp;        /* The motionplus context pointer */
        struct ctx_cam              *cam;           /* The ctx_cam information for the camera requested */

    };

    void webu_init(struct ctx_motapp *motapp);
    void webu_deinit(struct ctx_motapp *motapp);

#endif
