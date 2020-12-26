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

/*
 *    webu_status.c
 *
 *    Status reports in JSON format via stream HTTP endpoint.
 *
 */

#include <ctype.h>
#include <inttypes.h>

#include "motion.h"
#include "webu.h"
#include "webu_status.h"

/* Conservatively encode characters in an array as a JSON string */
static void webu_json_write_string(struct webui_ctx *webui, const char *str)
{
    const char *ptr;
    char cur, buf[8];

    webu_write(webui, "\"");

    for (ptr = str; *ptr != '\0'; ++ptr) {
        cur = *ptr;

        switch (cur) {
        case '\\':
            /*FALLTHROUGH*/
        case '"':
            snprintf(buf, sizeof(buf), "\\%c", cur);
            webu_write(webui, buf);
            continue;
        }

        /* TODO: Encode non-ASCII characters correctly as UTF-8 */
        if (cur & ~0x7F) {
            cur = '?';
        }

        if (isalnum(cur) || ispunct(cur) || cur == ' ') {
            /* Output character literally */
            buf[0] = cur;
            buf[1] = '\0';
        } else {
            snprintf(buf, sizeof(buf), "\\u%04x", (uint16_t)cur);
        }

        webu_write(webui, buf);
    }

    webu_write(webui, "\"");
}

/* Write time_t as seconds since Unix epoch */
static void webu_json_write_timestamp(struct webui_ctx *webui, time_t ts)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%" PRId64, (int64_t)ts);

    webu_write(webui, buf);
}

/* Write duration of time between two time_t */
static void webu_json_write_timestamp_elapsed(struct webui_ctx *webui, time_t ts, time_t since)
{
    char buf[32];
    double elapsed;

    elapsed = difftime(since, ts);

    snprintf(buf, sizeof(buf), "%" PRId64, (int64_t)elapsed);

    webu_write(webui, buf);
}

/* Write time_t as ISO-8601-formatted string */
static void webu_json_write_timestamp_iso8601(struct webui_ctx *webui, time_t ts)
{
    char buf[32];
    struct tm timestamp_tm;

    localtime_r(&ts, &timestamp_tm);

    strftime(buf, sizeof(buf), "%FT%T%z", &timestamp_tm);

    webu_json_write_string(webui, buf);
}

static void webu_status_write_list(struct webui_ctx *webui, const char *toplevel_key
        , void (*callback)(struct webui_ctx *, struct context *))
{
    int indx, indx_st;

    if (webui->thread_nbr == 0) {
        if (webui->cam_threads == 1) {
            indx_st = 0;
        } else {
            indx_st = 1;
        }

        webu_write(webui, "{\"");
        webu_write(webui, toplevel_key);
        webu_write(webui, "\": [");

        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            if (indx > indx_st) {
                webu_write(webui, ", ");
            }

            callback(webui, webui->cntlst[indx]);
        }

        webu_write(webui, "]}\n");
    } else {
        callback(webui, webui->cnt);
    }
}

static void webu_json_cam_list_single(struct webui_ctx *webui, struct context *cnt)
{
    char buf[WEBUI_LEN_RESP];

    snprintf(buf, sizeof(buf), "{\"id\": %d, \"name\": ", cnt->camera_id);

    webu_write(webui, buf);

    if (cnt->conf.camera_name == NULL) {
        webu_write(webui, "null");
    } else {
        webu_json_write_string(webui, cnt->conf.camera_name);
    }

    webu_write(webui, "}");
}

static void webu_status_list(struct webui_ctx *webui)
{
    webu_status_write_list(webui, "cameras", webu_json_cam_list_single);
}

/* Describe a single camera status */
static void webu_json_cam_status_single(struct webui_ctx *webui, struct context *cnt)
{
    char buf[WEBUI_LEN_RESP];
    const struct {
        const char *name;
        time_t value;
    } timestamps[] = {
        { "lasttime", cnt->lasttime },
        { "eventtime", cnt->eventtime },
        { "connectionlosttime", cnt->connectionlosttime },
        { NULL, 0 },
    }, *cur_timestamp;

    snprintf(buf, sizeof(buf), "{\"id\": %d, \"name\": ", cnt->camera_id);

    webu_write(webui, buf);

    if (cnt->conf.camera_name == NULL) {
        webu_write(webui, "null");
    } else {
        webu_json_write_string(webui, cnt->conf.camera_name);
    }

    snprintf(buf, sizeof(buf),
             ", \"image_width\": %d"
             ", \"image_height\": %d"
             ", \"fps\": %u"
             ", \"missing_frame_counter\": %u"
             ", \"running\": %u"
             ", \"lost_connection\": %u"
             , cnt->imgs.width
             , cnt->imgs.height
             , cnt->lastrate
             , cnt->missing_frame_counter
             , cnt->running
             , cnt->lost_connection);

    webu_write(webui, buf);

    webu_write(webui, ", \"currenttime\": ");
    webu_json_write_timestamp(webui, cnt->currenttime);
    webu_write(webui, ", \"currenttime_iso8601\": ");
    webu_json_write_timestamp_iso8601(webui, cnt->currenttime);

    for (cur_timestamp = timestamps; cur_timestamp && cur_timestamp->name;
         ++cur_timestamp) {
        snprintf(buf, sizeof(buf), ", \"%s\": ", cur_timestamp->name);
        webu_write(webui, buf);
        webu_json_write_timestamp(webui, cur_timestamp->value);

        snprintf(buf, sizeof(buf), ", \"%s_iso8601\": ", cur_timestamp->name);
        webu_write(webui, buf);
        if (cur_timestamp->value == (time_t)0) {
            webu_write(webui, "null");
        } else {
            webu_json_write_timestamp_iso8601(webui, cur_timestamp->value);
        }

        snprintf(buf, sizeof(buf), ", \"%s_elapsed\": ", cur_timestamp->name);
        webu_write(webui, buf);
        if (cur_timestamp->value == (time_t)0) {
            webu_write(webui, "null");
        } else {
            webu_json_write_timestamp_elapsed(webui, cur_timestamp->value,
                                              cnt->currenttime);
        }
    }

    webu_write(webui, "}\n");
}

static void webu_status_one(struct webui_ctx *webui)
{
    webu_status_write_list(webui, "camera_status", webu_json_cam_status_single);
}

static void webu_status_badreq(struct webui_ctx *webui)
{
    webu_write(webui, "{ \"error\": \"Server did not understand the request\" }");
}

void webu_status_main(struct webui_ctx *webui)
{
    switch (webui->cnct_type) {
    case WEBUI_CNCT_STATUS_LIST:
        webu_status_list(webui);
        break;

    case WEBUI_CNCT_STATUS_ONE:
        webu_status_one(webui);
        break;

    default:
        webu_status_badreq(webui);
        break;
    }
}
