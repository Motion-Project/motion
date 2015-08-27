/*
 *      webhttpd.c
 *
 *      HTTP Control interface for motion.
 *
 *      Specs : http://www.lavrsen.dk/twiki/bin/view/Motion/MotionHttpAPI
 *
 *      Copyright 2004-2005 by Angel Carpintero  (motiondevelop@gmail.com)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#include "webhttpd.h"    /* already includes motion.h */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>

/* Timeout in seconds, used for read and write */
const int NONBLOCK_TIMEOUT = 1;

pthread_mutex_t httpd_mutex;

// This is a dummy variable use to kill warnings when not checking sscanf and similar functions
int warningkill;

static const char *ini_template =
    "<html><head><title>Motion "VERSION"</title></head>\n"
    "<body>\n";

static const char *set_template =
    "<html><head><script language='javascript'>"
    "function show() {top.location.href="
    "'set?'+document.n.onames.options[document.n.onames.selectedIndex].value"
    "+'='+document.s.valor.value;"
    "}</script>\n<title>Motion "VERSION"</title>\n"
    "</head><body>\n";

static const char *end_template =
    "</body>\n"
    "</html>\n";

static const char *ok_response =
    "HTTP/1.1 200 OK\r\n"
    "Server: Motion-httpd/"VERSION"\r\n"
    "Connection: close\r\n"
    "Max-Age: 0\r\n"
    "Expires: 0\r\n"
    "Cache-Control: no-cache\r\n"
    "Cache-Control: private\r\n"
    "Pragma: no-cache\r\n"
    "Content-type: text/html\r\n\r\n";

static const char *ok_response_raw =
    "HTTP/1.1 200 OK\r\n"
    "Server: Motion-httpd/"VERSION"\r\n"
    "Connection: close\r\n"
    "Max-Age: 0\r\n"
    "Expires: 0\r\n"
    "Cache-Control: no-cache\r\n"
    "Cache-Control: private\r\n"
    "Pragma: no-cache\r\n"
    "Content-type: text/plain\r\n\r\n";

static const char *bad_request_response =
    "HTTP/1.0 400 Bad Request\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Bad Request</h1>\n"
    "<p>The server did not understand your request.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *bad_request_response_raw =
    "HTTP/1.0 400 Bad Request\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Bad Request";

static const char *not_found_response_template =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Not Found</h1>\n"
    "<p>The requested URL was not found on the server.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *not_found_response_template_raw =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Not Found";

static const char *not_found_response_valid =
    "HTTP/1.0 404 Not Valid\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Not Valid</h1>\n"
    "<p>The requested URL is not valid.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *not_found_response_valid_raw =
    "HTTP/1.0 404 Not Valid\r\n"
    "Content-type: text/plain\r\n\r\n"
    "The requested URL is not valid.";

static const char *not_valid_syntax =
    "HTTP/1.0 404 Not Valid Syntax\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Not Valid Syntax</h1>\n"
    "</body>\n"
    "</html>\n";

static const char *not_valid_syntax_raw =
    "HTTP/1.0 404 Not Valid Syntax\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Not Valid Syntax\n";

static const char *not_track =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Tracking Not Enabled</h1>\n";

static const char *not_track_raw =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Tracking Not Enabled";

static const char *track_error =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Track Error</h1>\n";

static const char *track_error_raw =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Track Error";

static const char *error_value =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Value Error</h1>\n";

static const char *error_value_raw =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Value Error";

static const char *not_found_response_valid_command =
    "HTTP/1.0 404 Not Valid Command\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Not Valid Command</h1>\n"
    "<p>The requested URL is not valid Command.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *not_found_response_valid_command_raw =
    "HTTP/1.0 404 Not Valid Command\r\n"
    "Content-type: text/plain\n\n"
    "Not Valid Command\n";

static const char *bad_method_response_template =
    "HTTP/1.0 501 Method Not Implemented\r\n"
    "Content-type: text/html\r\n\r\n"
    "<html>\n"
    "<body>\n"
    "<h1>Method Not Implemented</h1>\n"
    "<p>The method is not implemented by this server.</p>\n"
    "</body>\n"
    "</html>\n";

static const char *bad_method_response_template_raw =
    "HTTP/1.0 501 Method Not Implemented\r\n"
    "Content-type: text/plain\r\n\r\n"
    "Method Not Implemented\n";

static const char *request_auth_response_template=
    "HTTP/1.0 401 Authorization Required\r\n"
    "WWW-Authenticate: Basic realm=\"Motion Security Access\"\r\n";

/**
 * write_nonblock
 */
static ssize_t write_nonblock(int fd, const void *buf, size_t size)
{
    ssize_t nwrite = -1;
    struct timeval tm;
    fd_set fds;

    tm.tv_sec = NONBLOCK_TIMEOUT;
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (select(fd + 1, NULL, &fds, NULL, &tm) > 0) {
        if (FD_ISSET(fd, &fds)) {
            if ((nwrite = write(fd , buf, size)) < 0) {
                if (errno != EWOULDBLOCK)
                    return -1;
            }
        }
    }

    return nwrite;
}

/**
 * read_nonblock
 */
static ssize_t read_nonblock(int fd ,void *buf, ssize_t size)
{
    ssize_t nread = -1;
    struct timeval tm;
    fd_set fds;

    tm.tv_sec = NONBLOCK_TIMEOUT; /* Timeout in seconds */
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (select(fd + 1, &fds, NULL, NULL, &tm) > 0) {
        if (FD_ISSET(fd, &fds)) {
            if ((nread = read(fd , buf, size)) < 0) {
                if (errno != EWOULDBLOCK)
                    return -1;
            }
        }
    }

    return nread;
}

/**
 * send_template_ini_client
 */
static void send_template_ini_client(int client_socket, const char *template)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, ok_response, strlen(ok_response));
    nwrite += write_nonblock(client_socket, template, strlen(template));
    if (nwrite != (ssize_t)(strlen(ok_response) + strlen(template)))
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: failure write");
}

/**
 * send_template_ini_client_raw
 */
static void send_template_ini_client_raw(int client_socket)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, ok_response_raw, strlen(ok_response_raw));
    if (nwrite != (ssize_t)strlen(ok_response_raw))
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: failure write");
}

/**
 * send_template
 */
static void send_template(int client_socket, char *res)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, res, strlen(res));
    if (nwrite != (ssize_t)strlen(res))
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: failure write");
}

/**
 * send_template_raw
 */
static void send_template_raw(int client_socket, char *res)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, res, strlen(res));
    if (nwrite < 0)
        MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO, "%s: write_nonblock returned value less than zero.");
}

/**
 * send_template_end_client
 */
static void send_template_end_client(int client_socket)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, end_template, strlen(end_template));
    if (nwrite < 0)
        MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO, "%s: write_nonblock returned value less than zero.");

}

/**
 * response_client
 */
static void response_client(int client_socket, const char *template, char *back)
{
    ssize_t nwrite = 0;
    nwrite = write_nonblock(client_socket, template, strlen(template));
    if (back != NULL) {
        send_template(client_socket, back);
        send_template_end_client(client_socket);
    }
    if (nwrite < 0)
        MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO, "%s: write_nonblock returned value less than zero.");

}

/**
 * replace
 */
static char *replace(const char *str, const char *old, const char *new)
{
    char *ret, *r;
    const char *p, *q;
    size_t oldlen = strlen(old);
    size_t count, retlen, newlen = strlen(new);

    if (oldlen != newlen) {
        for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
            count++;
        /* this is undefined if p - str > PTRDIFF_MAX */
        retlen = p - str + strlen(p) + count * (newlen - oldlen);
    } else
        retlen = strlen(str);

    ret = mymalloc(retlen + 1);

    for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
        /* this is undefined if q - p > PTRDIFF_MAX */
        ptrdiff_t l = q - p;
        memcpy(r, p, l);
        r += l;
        memcpy(r, new, newlen);
        r += newlen;
    }
    strcpy(r, p);

    return ret;
}

/**
 * url_decode
 *      This function decode the values from GET request following the http RFC.
 *
 * Returns nothing.
 */
static void url_decode(char *urlencoded, size_t length)
{
    char *data = urlencoded;
    char *urldecoded = urlencoded;

    while (length > 0) {
        if (*data == '%') {
            char c[3];
            int i;
            data++;
            length--;
            c[0] = *data++;
            length--;
            c[1] = *data;
            c[2] = 0;

            warningkill = sscanf(c, "%x", &i);

            if (i < 128) {
                *urldecoded++ = (char)i;
            } else {
                *urldecoded++ = '%';
                *urldecoded++ = c[0];
                *urldecoded++ = c[1];
            }

	} else if (*data == '<' || *data == '+' || *data == '>') {
	  *urldecoded++ = ' ';
        } else {
            *urldecoded++ = *data;
        }

        data++;
        length--;
    }
    *urldecoded = '\0';
}


/**
 * config
 *      Manages/parses the config action for motion ( set , get , write , list ).
 *
 *   Returns 1 to exit from function.
 */
static unsigned int config(char *pointer, char *res, unsigned int length_uri,
                                 unsigned int thread, int client_socket, void *userdata)
{
    char question='\0';
    char command[256] = {'\0'};
    unsigned int i;
    struct context **cnt = userdata;

    warningkill = sscanf(pointer, "%255[a-z]%c", command , &question);
    if (!strcmp(command, "list")) {
        pointer = pointer + 4;
        length_uri = length_uri - 4;
        if (length_uri == 0) {
            const char *value = NULL;
            char *retval = NULL;
            /*call list*/
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/config>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>\n<ul>",
                        thread, thread);
                send_template(client_socket, res);

                for (i=0; config_params[i].param_name != NULL; i++) {

                    if ((thread != 0) && (config_params[i].main_thread))
                        continue;

                    value = config_params[i].print(cnt, NULL, i, thread);

                    if (value == NULL) {
                        retval = NULL;

                        /* Only get the thread value for main thread */
                        if (thread == 0)
                            config_params[i].print(cnt, &retval, i, thread);

                        /* thread value*/

                        if (retval) {

                            if (!strcmp(retval,"")) {
                                free(retval);
                                retval = mystrdup("No threads");
                            } else {
                                char *temp = retval;
                                size_t retval_miss = 0;
                                size_t retval_len = strlen(retval);
                                unsigned int ind = 0;
                                char thread_strings[1024] = {'\0'};

                                while (retval_miss != retval_len) {
                                    while (*temp != '\n') {
                                        thread_strings[ind++] = *temp;
                                        retval_miss++;
                                        temp++;
                                    }
                                    temp++;
                                    thread_strings[ind++] = '<';
                                    thread_strings[ind++] = 'b';
                                    thread_strings[ind++] = 'r';
                                    thread_strings[ind++] = '>';
                                    retval_miss++;
                                }
                                free(retval);
                                retval = NULL;
                                retval = mystrdup(thread_strings);
                            }

                            sprintf(res, "<li><a href=/%hu/config/set?%s>%s</a> = %s</li>\n", thread,
                                    config_params[i].param_name, config_params[i].param_name, retval);
                            free(retval);
                        } else if (thread != 0) {
                            /* get the value from main thread for the rest of threads */
                            value = config_params[i].print(cnt, NULL, i, 0);

                            sprintf(res, "<li><a href=/%hu/config/set?%s>%s</a> = %s</li>\n", thread,
                                    config_params[i].param_name, config_params[i].param_name,
                                    value ? value : "(not defined)");
                        } else {
                            sprintf(res, "<li><a href=/%hu/config/set?%s>%s</a> = %s</li>\n", thread,
                                    config_params[i].param_name, config_params[i].param_name,
                                    "(not defined)");
                        }

                    } else {
                        sprintf(res, "<li><a href=/%hu/config/set?%s>%s</a> = %s</li>\n", thread,
                                config_params[i].param_name, config_params[i].param_name, value);
                    }
                    send_template(client_socket, res);
                }

                sprintf(res, "</ul><a href=/%hu/config>&lt;&ndash; back</a>", thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                for (i=0; config_params[i].param_name != NULL; i++) {
                    value = config_params[i].print(cnt, NULL, i, thread);
                    if (value == NULL)
                        value = config_params[i].print(cnt, NULL, i, 0);
                    sprintf(res, "%s = %s\n", config_params[i].param_name, value);
                    send_template_raw(client_socket, res);
                }
            }
        } else {
            /*error*/
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "set")) {
        /* set?param_name=value */
        pointer = pointer + 3;
        length_uri = length_uri - 3;
        if ((length_uri != 0) && (question == '?')) {
            pointer++;
            length_uri--;
            warningkill = sscanf(pointer,"%255[-0-9a-z_]%c", command, &question);
            /*check command , question == '='  length_uri too*/
            if ((question == '=') && (command[0] != '\0')) {
                length_uri = length_uri - strlen(command) - 1;
                pointer = pointer + strlen(command) + 1;
                /* check if command exists and type of command and not end of URI */
                i=0;
                while (config_params[i].param_name != NULL) {
                    if ((thread != 0) && (config_params[i].main_thread)) {
                        i++;
                        continue;
                    }

                    if (!strcasecmp(command, config_params[i].param_name))
                        break;
                    i++;
                }

                if (config_params[i].param_name) {
                    if (length_uri > 0) {
                        char Value[1024] = {'\0'};
                        warningkill = sscanf(pointer,"%1023s", Value);
                        length_uri = length_uri - strlen(Value);
                        if ((length_uri == 0) && (strlen(Value) > 0)) {
                            /* FIXME need to assure that is a valid value */
                            url_decode(Value, strlen(Value));
                            conf_cmdparse(cnt + thread, config_params[i].param_name, Value);
                            if (cnt[0]->conf.webcontrol_html_output) {
                                sprintf(res,
                                    "<a href=/%hu/config/list>&lt;&ndash; back</a>"
                                    "<br><br>\n<b>Thread %hu</b>\n"
                                    "<ul><li><a href=/%hu/config/set?%s>%s</a> = %s"
                                    "</li></ul><b>Done</b>",
                                        thread, thread, thread, config_params[i].param_name,
                                        config_params[i].param_name, Value);

                                send_template_ini_client(client_socket, ini_template);
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "%s = %s\nDone\n", config_params[i].param_name, Value);
                                send_template_raw(client_socket, res);
                            }
                        } else {
                            /*error*/
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_valid_syntax, NULL);
                            else
                                response_client(client_socket, not_valid_syntax_raw, NULL);
                        }
                    } else {
                        char *type = NULL;
                        type = mystrdup(config_type(&config_params[i]));

                        if (!strcmp(type, "string")) {
                            char *value = NULL;
                            conf_cmdparse(cnt+thread, config_params[i].param_name, value);
                            free(type);
                            type = mystrdup("(null)");
                        } else if (!strcmp(type, "int")) {
                            free(type);
                            type = mystrdup("0");
                            conf_cmdparse(cnt+thread, config_params[i].param_name, type);
                        } else if (!strcmp(type, "bool")) {
                            free(type);
                            type = mystrdup("off");
                            conf_cmdparse(cnt+thread, config_params[i].param_name, type);
                        } else {
                            free(type);
                            type = mystrdup("unknown");
                        }

                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res,
                                "<a href=/%hu/config/list>&lt;&ndash; back</a><br><br>\n"
                                    "<b>Thread %hu</b>\n<ul><li><a href=/%hu/config/set?%s>%s</a>"
                                    "= %s</li></ul><br><b>Done</b>", thread, thread, thread,
                                    config_params[i].param_name, config_params[i].param_name, type);

                            send_template_ini_client(client_socket, ini_template);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "%s = %s\nDone\n", config_params[i].param_name, type);
                            send_template_raw(client_socket, res);
                        }
                        free(type);

                    }
                } else {
                    /*error*/
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_found_response_valid_command, NULL);
                    else
                        response_client(client_socket, not_found_response_valid_command_raw, NULL);
                }
            } else {
                /* Show param_name dialogue only for html output */
                if ((cnt[0]->conf.webcontrol_html_output) && (command[0] != '\0') &&
                    (((length_uri = length_uri - strlen(command)) == 0))) {
                    i=0;
                    while (config_params[i].param_name != NULL) {
                        if ((thread != 0) && (config_params[i].main_thread)) {
                            i++;
                            continue;
                        }

                        if (!strcasecmp(command, config_params[i].param_name))
                            break;
                        i++;
                    }
                    /* param_name exists */
                    if (config_params[i].param_name) {
                        const char *value = NULL;
                        char *text_help = NULL;
                        char *sharp = NULL;

                        value = config_params[i].print(cnt, NULL, i, thread);

                        sharp = strstr(config_params[i].param_help, "#\n\n#");
                        if (sharp == NULL)
                            sharp = strstr(config_params[i].param_help, "#");
                        sharp++;

                        text_help = replace(sharp, "\n#", "<br>");

                        send_template_ini_client(client_socket, ini_template);
                        if (!strcmp ("bool", config_type(&config_params[i]))) {
                            char option[80] = {'\0'};

                            if ((value == NULL) && (thread != 0))
                                value = config_params[i].print(cnt, NULL, i, 0);

                            if (!strcmp ("on", value))
                                sprintf(option, "<option value='on' selected>on</option>\n"
                                                "<option value='off'>off</option>\n");
                            else
                                sprintf(option, "<option value='on'>on</option>\n"
                                                "<option value='off' selected>off</option>\n");

                            sprintf(res, "<a href=/%hu/config/list>&lt;&ndash; back</a><br><br>\n"
                                         "<b>Thread %hu</b>\n"
                                         "<form action=set?>\n"
                                         "<b>%s</b>&nbsp;<select name='%s'>\n"
                                         "%s"
                                         "</select><input type='submit' value='set'>\n"
                                         "&nbsp;&nbsp;&nbsp;&nbsp;"
                                         "<a href='%s#%s' target=_blank>[help]</a>"
                                         "</form>\n<hr><i>%s</i>", thread, thread,
                                         config_params[i].param_name, config_params[i].param_name,
                                         option, TWIKI_URL, config_params[i].param_name, text_help);
                        } else {

                            if (value == NULL) {
                                if (thread != 0)
                                    /* get the value from main thread for the rest of threads */
                                                                value = config_params[i].print(cnt, NULL, i, 0);
                                if (value == NULL) value = "";
                            }
                            sprintf(res, "<a href=/%hu/config/list>&lt;&ndash; back</a><br><br>\n"
                                         "<b>Thread %hu</b>\n<form action=set?>\n"
                                         "<b>%s</b>&nbsp;<input type=text name='%s' value='%s' size=80>\n"
                                         "<input type='submit' value='set'>\n"
                                         "&nbsp;&nbsp;&nbsp;&nbsp;"
                                         "<a href='%s#%s' target=_blank>[help]</a>"
                                         "</form>\n<hr><i>%s</i>", thread, thread,
                                         config_params[i].param_name, config_params[i].param_name,
                                         value, TWIKI_URL, config_params[i].param_name, text_help);
                        }

                        send_template(client_socket, res);
                        send_template_end_client(client_socket);
                        free(text_help);
                    } else {
                        if (cnt[0]->conf.webcontrol_html_output)
                            response_client(client_socket, not_found_response_valid_command, NULL);
                        else
                            response_client(client_socket, not_found_response_valid_command_raw, NULL);
                    }
                } else {
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_found_response_valid_command, NULL);
                    else
                        response_client(client_socket, not_found_response_valid_command_raw, NULL);
                }
            }
        } else if (length_uri == 0) {
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, set_template);
                sprintf(res, "<a href=/%hu/config>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>\n"
                             "<form name='n'>\n<select name='onames'>\n", thread, thread);

                send_template(client_socket, res);
                for (i=0; config_params[i].param_name != NULL; i++) {
                    if ((thread != 0) && (config_params[i].main_thread))
                        continue;
                    sprintf(res, "<option value='%s'>%s</option>\n",
                            config_params[i].param_name, config_params[i].param_name);
                    send_template(client_socket, res);
                }
                sprintf(res, "</select>\n</form>\n"
                             "<form action=set name='s'"
                             "ONSUBMIT='if (!this.submitted) return false; else return true;'>\n"
                             "<input type=text name='valor' value=''>\n"
                             "<input type='button' value='set' onclick='javascript:show()'>\n"
                             "</form>\n");
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "set needs param_name=value\n");
                send_template_raw(client_socket, res);
            }
        } else {
            /*error*/
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "get")) {
        /* get?query=param_name */
        pointer = pointer + 3;
        length_uri = length_uri - 3;

        if ((length_uri > 7) && (question == '?')) {
            /* 8 -> query=param_name FIXME minimum length param_name */
            pointer++;
            length_uri--;
            warningkill = sscanf(pointer,"%255[-0-9a-z]%c", command, &question);

            if ((question == '=') && (!strcmp(command, "query"))) {
                pointer = pointer + 6;
                length_uri = length_uri - 6;
                warningkill = sscanf(pointer, "%255[-0-9a-z_]", command);
                /*check if command exist, length_uri too*/
                length_uri = length_uri - strlen(command);

                if (length_uri == 0) {
                    const char *value = NULL;
                    i = 0;
                    while (config_params[i].param_name != NULL) {
                        if ((thread != 0) && (config_params[i].main_thread)) {
                            i++;
                            continue;
                        }

                        if (!strcasecmp(command, config_params[i].param_name))
                            break;
                        i++;
                    }
                    /*
                     * FIXME bool values or commented values maybe that should be
                     * solved with config_type
                     */
                    if (config_params[i].param_name) {
                        const char *type = NULL;
                        type = config_type(&config_params[i]);
                        if (!strcmp(type, "unknown")) {
                            /* error doesn't exists this param_name */
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_found_response_valid_command, NULL);
                            else
                                response_client(client_socket, not_found_response_valid_command_raw, NULL);
                            return 1;
                        } else {
                            char *text_help = NULL;
                            char *sharp = NULL;

                            value = config_params[i].print(cnt, NULL, i, thread);

                            sharp = strstr(config_params[i].param_help, "#\n\n#");
                            if (sharp == NULL)
                                sharp = strstr(config_params[i].param_help, "#");
                            sharp++;

                            text_help = replace(sharp, "\n#", "<br>");

                            if (value == NULL)
                                value = config_params[i].print(cnt, NULL, i, 0);
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hu/config/get>&lt;&ndash; back</a><br><br>\n"
                                             "<b>Thread %hu</b><br>\n<ul><li>%s = %s &nbsp;&nbsp;"
                                             "&nbsp;&nbsp;<a href='%s#%s' target=_blank>"
                                             "[help]</a></li></ul><hr><i>%s</i>",
                                             thread, thread, config_params[i].param_name, value,
                                             TWIKI_URL, config_params[i].param_name, text_help);

                                send_template(client_socket, res);
                                send_template_end_client(client_socket);

                                free(text_help);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "%s = %s\nDone\n", config_params[i].param_name, value);
                                send_template_raw(client_socket, res);
                            }
                        }
                    } else {
                        /* error */
                        if (cnt[0]->conf.webcontrol_html_output)
                            response_client(client_socket, not_found_response_valid_command, NULL);
                        else
                            response_client(client_socket, not_found_response_valid_command_raw, NULL);
                    }
                } else {
                    /* error */
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_found_response_valid_command, NULL);
                    else
                        response_client(client_socket, not_found_response_valid_command_raw, NULL);
                }
            }
        } else if (length_uri == 0) {
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/config>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b><br>\n"
                             "<form action=get>\n"
                             "<select name='query'>\n", thread, thread);
                send_template(client_socket, res);
                for (i=0; config_params[i].param_name != NULL; i++) {
                    if ((thread != 0) && (config_params[i].main_thread))
                        continue;
                    sprintf(res, "<option value='%s'>%s</option>\n",
                            config_params[i].param_name, config_params[i].param_name);
                    send_template(client_socket, res);
                }
                sprintf(res, "</select>\n"
                             "<input type='submit' value='get'>\n"
                             "</form>\n");
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "get needs param_name\n");
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_valid_syntax, NULL);
            else
                response_client(client_socket, not_valid_syntax_raw, NULL);
        }
    } else if (!strcmp(command, "write")) {
            pointer = pointer + 5;
            length_uri = length_uri - 5;
            if (length_uri == 0) {
                if (cnt[0]->conf.webcontrol_html_output) {
                    send_template_ini_client(client_socket, ini_template);
                    sprintf(res, "<a href=/%hu/config>&lt;&ndash; back</a><br><br>"
                                 "Are you sure? <a href=/%hu/config/writeyes>Yes</a>\n", thread, thread);
                    send_template(client_socket, res);
                    send_template_end_client(client_socket);
                } else {
                    conf_print(cnt);
                    send_template_ini_client_raw(client_socket);
                    sprintf(res, "Thread %hu write\nDone\n", thread);
                    send_template_raw(client_socket, res);
                }
            } else {
                /* error */
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_found_response_valid_command, NULL);
                else
                    response_client(client_socket, not_found_response_valid_command_raw, NULL);
            }

    } else if (!strcmp(command, "writeyes")) {
        pointer = pointer + 8;
        length_uri = length_uri - 8;
        if (length_uri == 0) {
            conf_print(cnt);
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/config>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>  write done !\n",
                             thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "Thread %hu write\nDone\n", thread);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output) {
                response_client(client_socket, not_found_response_valid_command, NULL);
            }
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else {
        /* error */
        if (cnt[0]->conf.webcontrol_html_output)
            response_client(client_socket, not_found_response_valid_command, NULL);
        else
            response_client(client_socket, not_found_response_valid_command_raw, NULL);
    }

    return 1;
}


/**
 * action
 *      manages/parses the actions for motion ( makemovie , snapshot , restart , quit ).
 *
 * Returns
 *      0 for restart & quit
 *      1 for makemovie & snaphost
 */
static unsigned int action(char *pointer, char *res, unsigned int length_uri,
                                 unsigned int thread, int client_socket, void *userdata)
{
    /* parse action commands */
    char command[256] = {'\0'};
    struct context **cnt = userdata;
    unsigned int i = 0;

    warningkill = sscanf(pointer, "%255[a-z]" , command);
    if (!strcmp(command, "makemovie")) {
        pointer = pointer + 9;
        length_uri = length_uri - 9;
        if (length_uri == 0) {
            /* call makemovie */

            if (thread == 0) {
                while (cnt[++i])
                    cnt[i]->makemovie = 1;
            } else {
                cnt[thread]->makemovie = 1;
            }

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/action>&lt;&ndash; back</a><br><br>\n"
                             "makemovie for thread %hu done<br>\n", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "makemovie for thread %hu\nDone\n", thread);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "snapshot")) {
        pointer = pointer + 8;
        length_uri = length_uri - 8;
        if (length_uri == 0) {
            /* call snapshot */

            if (thread == 0) {
                while (cnt[++i])
                    cnt[i]->snapshot = 1;
            } else {
                cnt[thread]->snapshot = 1;
            }

            cnt[thread]->snapshot = 1;
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/action>&lt;&ndash; back</a><br><br>\n"
                             "snapshot for thread %hu done<br>\n", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "snapshot for thread %hu\nDone\n", thread);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "restart")) {
        pointer = pointer + 7;
        length_uri = length_uri - 7;
        if (length_uri == 0) {
            /* call restart */

            if (thread == 0) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: httpd is going to restart");
                kill(getpid(),SIGHUP);
                if (cnt[0]->conf.webcontrol_html_output) {
                    send_template_ini_client(client_socket, ini_template);
                    sprintf(res, "restart in progress ... bye<br>\n<a href='/'>Home</a>");
                    send_template(client_socket, res);
                    send_template_end_client(client_socket);
                } else {
                    send_template_ini_client_raw(client_socket);
                    sprintf(res, "restart in progress ...\nDone\n");
                    send_template_raw(client_socket, res);
                }
                return 0; // to restart
            } else {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: httpd is going to restart thread %d",
                           thread);
                if (cnt[thread]->running) {
                    cnt[thread]->makemovie = 1;
                    cnt[thread]->finish = 1;
                }
                cnt[thread]->restart = 1;
                if (cnt[0]->conf.webcontrol_html_output) {
                    send_template_ini_client(client_socket, ini_template);
                    sprintf(res, "<a href=/%hu/action>&lt;&ndash; back</a><br><br>\n"
                                 "restart for thread %hu done<br>\n", thread, thread);
                    send_template(client_socket, res);
                    send_template_end_client(client_socket);
                } else {
                    send_template_ini_client_raw(client_socket);
                    sprintf(res, "restart for thread %hu\nDone\n", thread);
                    send_template_raw(client_socket, res);
                }
            }
        } else {
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "quit")) {
        pointer = pointer + 4;
        length_uri = length_uri - 4;
        if (length_uri == 0) {
            /* call quit */

            if (thread == 0) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: httpd quits");
                kill(getpid(),SIGQUIT);
                if (cnt[0]->conf.webcontrol_html_output) {
                    send_template_ini_client(client_socket, ini_template);
                    sprintf(res, "quit in progress ... bye");
                    send_template(client_socket, res);
                    send_template_end_client(client_socket);
                } else {
                    send_template_ini_client_raw(client_socket);
                    sprintf(res,"quit in progress ... bye\nDone\n");
                    send_template_raw(client_socket, res);
                }
                return 0; // to quit
            } else {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: httpd quits thread %d",
                           thread);
                cnt[thread]->restart = 0;
                cnt[thread]->makemovie = 1;
                cnt[thread]->finish = 1;
                cnt[thread]->watchdog = WATCHDOG_OFF;
                if (cnt[0]->conf.webcontrol_html_output) {
                    send_template_ini_client(client_socket, ini_template);
                    sprintf(res, "<a href=/%hu/action>&lt;&ndash; back</a><br><br>\n"
                                 "quit for thread %hu done<br>\n", thread, thread);
                    send_template(client_socket, res);
                    send_template_end_client(client_socket);
                } else {
                    send_template_ini_client_raw(client_socket);
                    sprintf(res, "quit for thread %hu\nDone\n", thread);
                    send_template_raw(client_socket, res);
                }
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else {
        if (cnt[0]->conf.webcontrol_html_output)
            response_client(client_socket, not_found_response_valid_command, NULL);
        else
            response_client(client_socket, not_found_response_valid_command_raw, NULL);
    }

    return 1;
}

/**
 * detection
 *      manages/parses the detection actions for motion ( status , start , pause ).
 *
 * Returns
 *      1 to exit from function.
 */

static unsigned int detection(char *pointer, char *res, unsigned int length_uri,
                                    unsigned int thread, int client_socket, void *userdata)
{
    char command[256] = {'\0'};
    struct context **cnt = userdata;
    unsigned int i = 0;

    warningkill = sscanf(pointer, "%255[a-z]" , command);
    if (!strcmp(command, "status")) {
        pointer = pointer + 6;
        length_uri = length_uri - 6;
        if (length_uri == 0) {
            /* call status */

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/detection>&lt;&ndash; back</a><br><br><b>Thread %hu</b>"
                             " Detection status %s\n", thread, thread,
                             (!cnt[thread]->running)? "NOT RUNNING": (cnt[thread]->pause)? "PAUSE":"ACTIVE");
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                sprintf(res, "Thread %hu Detection status %s\n", thread,
                             (!cnt[thread]->running)? "NOT RUNNING": (cnt[thread]->pause)? "PAUSE":"ACTIVE");
                send_template_ini_client_raw(client_socket);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "start")) {
        pointer = pointer + 5;
        length_uri = length_uri - 5;
        if (length_uri == 0) {
            /* call start */

            if (thread == 0) {
                do {
                    cnt[i]->pause = 0;
                } while (cnt[++i]);
            } else {
                cnt[thread]->pause = 0;
            }

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/detection>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>"
                             " Detection resumed\n", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "Thread %hu Detection resumed\nDone\n", thread);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "pause")) {
        pointer = pointer + 5;
        length_uri = length_uri - 5;
        if (length_uri == 0) {
            /* call pause */

            if (thread == 0) {
                do {
                    cnt[i]->pause = 1;
                } while (cnt[++i]);
            } else {
                cnt[thread]->pause = 1;
            }

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/detection>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>"
                             " Detection paused\n", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "<b>Thread %hu</b> Detection paused\nDone\n", thread);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "connection")) {
        pointer = pointer + 10;
        length_uri = length_uri - 10;

        if (length_uri == 0) {
            /* call connection */
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/detection>&lt;&ndash; back</a><br><br>\n", thread);
                send_template(client_socket, res);
                if (thread == 0) {
                    do{
                        sprintf(res, "<b>Thread %hu</b> %s<br>\n", i,
                                     (!cnt[i]->running)? "NOT RUNNING" :
                                     (cnt[i]->lost_connection)?CONNECTION_KO:CONNECTION_OK);
                        send_template(client_socket, res);
                    } while (cnt[++i]);
                } else {
                    sprintf(res, "<b>Thread %hu</b> %s\n", thread,
                                 (!cnt[thread]->running)? "NOT RUNNING" :
                                 (cnt[thread]->lost_connection)? CONNECTION_KO: CONNECTION_OK);
                    send_template(client_socket, res);
                }
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                if (thread == 0) {
                    do {
                        sprintf(res, "Thread %hu %s\n", i,
                                     (!cnt[i]->running)? "NOT RUNNING" :
                                                             (cnt[i]->lost_connection)? CONNECTION_KO: CONNECTION_OK);
                        send_template_raw(client_socket, res);
                    } while (cnt[++i]);
                } else {
                    sprintf(res, "Thread %hu %s\n", thread,
                                 (!cnt[thread]->running)? "NOT RUNNING" :
                                 (cnt[thread]->lost_connection)? CONNECTION_KO: CONNECTION_OK);
                    send_template_raw(client_socket, res);
                }

            }
        } else {
            /* error */
             if (cnt[0]->conf.webcontrol_html_output)
                 response_client(client_socket, not_found_response_valid_command, NULL);
             else
                 response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else {
        if (cnt[0]->conf.webcontrol_html_output)
            response_client(client_socket, not_found_response_valid_command, NULL);
        else
            response_client(client_socket, not_found_response_valid_command_raw, NULL);
    }

    return 1;
}


/**
 * track
 *      manages/parses the track action for motion ( set , pan , tilt , auto ).
 *
 * Returns
 *      1 to exit from function.
 */
static unsigned int track(char *pointer, char *res, unsigned int length_uri,
                                unsigned int thread, int client_socket, void *userdata)
{
    char question='\0';
    char command[256] = {'\0'};
    struct context **cnt = userdata;

    warningkill = sscanf(pointer, "%255[a-z]%c", command, &question);
    if (!strcmp(command, "set")) {
        pointer = pointer+3;
        length_uri = length_uri-3;
        /*
         * FIXME need to check each value
         * Relative movement set?pan=0&tilt=0 | set?pan=0 | set?tilt=0
         * Absolute movement set?x=0&y=0 | set?x=0 | set?y=0
         */

        if ((question == '?') && (length_uri > 2)) {
            char panvalue[12] = {'\0'}, tiltvalue[12] = {'\0'};
            char x_value[12] = {'\0'}, y_value[12] = {'\0'};
            struct context *setcnt;
            int pan = 0, tilt = 0, X = 0 , Y = 0;

            pointer++;
            length_uri--;
            /*
             * set?pan=value&tilt=value
             * set?x=value&y=value
             * pan= or x= | tilt= or y=
             */

            warningkill = sscanf(pointer, "%255[a-z]%c" , command, &question);

            if ((question != '=') || (command[0] == '\0')) {
                /* no valid syntax */
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 1");
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_valid_syntax, NULL);
                else
                    response_client(client_socket, not_valid_syntax_raw, NULL);
                return 1;
            }

            pointer++;
            length_uri--;

            /* Check first parameter */

            if (!strcmp(command, "pan")) {
                pointer = pointer + 3;
                length_uri = length_uri - 3;
                pan = 1;
                if ((warningkill = sscanf(pointer, "%10[-0-9]", panvalue))) {
                    pointer = pointer + strlen(panvalue);
                    length_uri = length_uri - strlen(panvalue);
                }
            }
            else if (!strcmp(command, "tilt")) {
                pointer = pointer + 4;
                length_uri = length_uri - 4;
                tilt = 1;
                if ((warningkill = sscanf(pointer, "%10[-0-9]", tiltvalue))) {
                    pointer = pointer + strlen(tiltvalue);
                    length_uri = length_uri - strlen(tiltvalue);
                }
            }
            else if (!strcmp(command, "x")) {
                pointer++;
                length_uri--;
                X = 1;
                if ((warningkill = sscanf(pointer, "%10[-0-9]", x_value))) {
                    pointer = pointer + strlen(x_value);
                    length_uri = length_uri - strlen(x_value);
                }
            }
            else if (!strcmp(command, "y")) {
                pointer++;
                length_uri--;
                Y = 1;
                if ((warningkill = sscanf(pointer, "%10[-0-9]" , y_value))) {
                    pointer = pointer + strlen(y_value);
                    length_uri = length_uri - strlen(y_value);
                }
            } else {
                /* no valid syntax */
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 2");
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_valid_syntax, NULL);
                else
                    response_client(client_socket, not_valid_syntax_raw, NULL);
                return 1;
            }

            /* first value check for error */

            if (!warningkill) {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 3");
                /* error value */
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, error_value, NULL);
                else
                    response_client(client_socket, error_value_raw, NULL);
                return 1;
            }

            /* Only one parameter (pan= ,tilt= ,x= ,y= ) */
            if (length_uri == 0) {
                if (pan) {
                    struct coord cent;
                    struct context *pancnt;

                    /* move pan */

                    pancnt = cnt[thread];
                    cent.width = pancnt->imgs.width;
                    cent.height = pancnt->imgs.height;
                    cent.y = 0;
                    cent.x = atoi(panvalue);
                    // Add the number of frame to skip for motion detection
                    cnt[thread]->moved = track_move(pancnt, pancnt->video_dev, &cent, &pancnt->imgs, 1);
                    if (cnt[thread]->moved) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>\n"
                                         "track set relative pan=%s<br>\n",
                                         thread, thread, panvalue);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "track set relative pan=%s\nDone\n", panvalue);
                            send_template_raw(client_socket, res);
                        }
                    } else {
                    /* error in track action */
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b>\n", thread, thread);
                            response_client(client_socket, track_error, res);
                        } else {
                            response_client(client_socket, track_error_raw, NULL);
                        }
                    }
                } else if (tilt) {
                    struct coord cent;
                    struct context *tiltcnt;

                    /* move tilt */

                    tiltcnt = cnt[thread];
                    cent.width = tiltcnt->imgs.width;
                    cent.height = tiltcnt->imgs.height;
                    cent.x = 0;
                    cent.y = atoi(tiltvalue);
                    // Add the number of frame to skip for motion detection
                    cnt[thread]->moved = track_move(tiltcnt, tiltcnt->video_dev, &cent, &tiltcnt->imgs, 1);
                    if (cnt[thread]->moved) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>\n"
                                         "track set relative tilt=%s\n",
                                         thread, thread, tiltvalue);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "track set relative tilt=%s\nDone\n", tiltvalue);
                            send_template_raw(client_socket, res);
                        }
                    } else {
                        /* error in track action */
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b>\n", thread, thread);
                            response_client(client_socket, track_error, res);
                        } else {
                            response_client(client_socket, track_error_raw, NULL);
                        }
                    }
                } else if (X) {
                    /* X */
                    setcnt = cnt[thread];
                    // 1000 is out of range for pwc
                    cnt[thread]->moved = track_center(setcnt, setcnt->video_dev, 1, atoi(x_value), 1000);
                    if (cnt[thread]->moved) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>\n"
                                     "track set absolute x=%s\n",
                                         thread, thread, x_value);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "track set absolute x=%s\nDone\n", x_value);
                            send_template_raw(client_socket, res);
                        }
                    } else {
                        /* error in track action */
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b>\n", thread, thread);
                            response_client(client_socket, track_error, res);
                        } else {
                            response_client(client_socket, track_error_raw, NULL);
                        }
                    }

                } else {
                    /* Y */
                    setcnt = cnt[thread];
                    // 1000 is out of range for pwc
                    cnt[thread]->moved = track_center(setcnt, setcnt->video_dev, 1, 1000, atoi(y_value));
                    if (cnt[thread]->moved) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>\n"
                                         "track set absolute y=%s<br>\n",
                                         thread, thread, y_value);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "track set absolute y=%s\nDone\n", y_value);
                            send_template_raw(client_socket, res);
                        }
                    } else {
                        /* error in track action */
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b>\n", thread, thread);
                            response_client(client_socket, track_error, res);
                        } else {
                            response_client(client_socket, track_error_raw, NULL);
                        }
                    }
                }
                return 1;
            }

            /* Check Second parameter */

            warningkill = sscanf(pointer, "%c%255[a-z]" , &question, command);
            if ((question != '&') || (command[0] == '\0')) {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 4");
                if (strstr(pointer, "&")) {
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, error_value, NULL);
                    else
                        response_client(client_socket, error_value_raw, NULL);

                /* no valid syntax */
                } else {
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_valid_syntax, NULL);
                    else
                        response_client(client_socket, not_valid_syntax_raw, NULL);
                }
                return 1;
            }

            pointer++;
            length_uri--;

            if (!strcmp(command, "pan")) {
                pointer = pointer + 3;
                length_uri = length_uri - 3;
                if (pan || !tilt || X || Y) {
                    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 5");
                    /* no valid syntax */
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_valid_syntax, NULL);
                    else
                        response_client(client_socket, not_valid_syntax_raw, NULL);
                    return 1;
                }
                pan=2;
                warningkill = sscanf(pointer, "%c%10[-0-9]" , &question, panvalue);

            } else if (!strcmp(command, "tilt")) {
                pointer = pointer + 4;
                length_uri = length_uri - 4;
                if (tilt || !pan || X || Y) {
                    /* no valid syntax */
                    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 6");
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_valid_syntax, NULL);
                    else
                        response_client(client_socket, not_valid_syntax_raw, NULL);
                    return 1;
                }
                tilt = 2;
                warningkill = sscanf(pointer, "%c%10[-0-9]" , &question, tiltvalue);

            } else if (!strcmp(command, "x")) {
                pointer++;
                length_uri--;
                if (X || !Y || pan || tilt) {
                    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 7");

                    /* no valid syntax */
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_valid_syntax, NULL);
                    else
                        response_client(client_socket, not_valid_syntax_raw, NULL);
                    return 1;
                }
                X = 2;
                warningkill = sscanf(pointer, "%c%10[-0-9]" , &question, x_value);

            } else if (!strcmp(command, "y")) {
                pointer++;
                length_uri--;
                if (Y || !X || pan || tilt) {
                    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 8");
                    /* no valid syntax */
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_valid_syntax, NULL);
                    else
                        response_client(client_socket, not_valid_syntax_raw, NULL);
                    return 1;
                }
                Y = 2;
                warningkill = sscanf(pointer, "%c%10[-0-9]" , &question, y_value);

            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 9");
                /* no valid syntax */
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_valid_syntax, NULL);
                else
                    response_client(client_socket, not_valid_syntax_raw, NULL);
                return 1;
            }

            /* Second value check */

            if ((warningkill < 2) && (question != '=')) {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 10");
                /* no valid syntax */
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_valid_syntax, NULL);
                else
                    response_client(client_socket, not_valid_syntax_raw, NULL);
                return 1;
            } else if ((question == '=') && (warningkill == 1)) {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 11");
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, error_value, NULL);
                else
                    response_client(client_socket, error_value_raw, NULL);
                return 1;
            }


            if (pan == 2) {
                pointer = pointer + strlen(panvalue) + 1;
                length_uri = length_uri - strlen(panvalue) - 1;
            } else if (tilt == 2) {
                pointer = pointer + strlen(tiltvalue) + 1;
                length_uri = length_uri - strlen(tiltvalue) - 1;
            } else if (X == 2) {
                pointer = pointer + strlen(x_value) + 1;
                length_uri = length_uri - strlen(x_value) - 1;
            } else {
                pointer = pointer + strlen(y_value) + 1;
                length_uri = length_uri - strlen(y_value) - 1;
            }


            if (length_uri != 0) {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "%s: httpd debug race 12");
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, error_value, NULL);
                else
                    response_client(client_socket, error_value_raw, NULL);
                return 1;
            }

            /* track set absolute ( x , y )*/

            if (X && Y) {
                setcnt = cnt[thread];
                cnt[thread]->moved = track_center(setcnt, setcnt->video_dev, 1, atoi(x_value), atoi(y_value));
                if (cnt[thread]->moved) {
                    if (cnt[0]->conf.webcontrol_html_output) {
                        send_template_ini_client(client_socket, ini_template);
                        sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                     "<b>Thread %hu</b><br>\n"
                                     "track absolute set x=%s y=%s<br>\n",
                                     thread, thread, x_value, y_value);
                        send_template(client_socket, res);
                        send_template_end_client(client_socket);
                    } else {
                        send_template_ini_client_raw(client_socket);
                        sprintf(res, "track absolute set x=%s y=%s\nDone\n", x_value, y_value);
                        send_template_raw(client_socket, res);
                    }
                } else {
                    /* error in track action */
                    if (cnt[0]->conf.webcontrol_html_output) {
                        sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                     "<b>Thread %hu</b>\n", thread, thread);
                        response_client(client_socket, track_error, res);
                    } else {
                        response_client(client_socket, track_error_raw, NULL);
                    }
                }
                /* track set relative ( pan , tilt )*/
            } else {
                struct coord cent;
                struct context *relativecnt;

                /* move pan */

                relativecnt = cnt[thread];
                cent.width = relativecnt->imgs.width;
                cent.height = relativecnt->imgs.height;
                cent.y = 0;
                cent.x = atoi(panvalue);
                // Add the number of frame to skip for motion detection
                cnt[thread]->moved = track_move(relativecnt, relativecnt->video_dev,
                                                &cent, &relativecnt->imgs, 1);

                if (cnt[thread]->moved) {
                    /* move tilt */
                    relativecnt = cnt[thread];
                    cent.width = relativecnt->imgs.width;
                    cent.height = relativecnt->imgs.height;
                    cent.x = 0;
                    cent.y = atoi(tiltvalue);
                    // SLEEP(1,0);
                    cnt[thread]->moved = track_move(relativecnt, relativecnt->video_dev,
                                                    &cent, &relativecnt->imgs, 1);
                    if (cnt[thread]->moved) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>\n"
                                         "track relative pan=%s tilt=%s\n",
                                         thread, thread, panvalue, tiltvalue);
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            send_template_ini_client_raw(client_socket);
                            sprintf(res, "track relative pan=%s tilt=%s\nDone\n", panvalue, tiltvalue);
                            send_template_raw(client_socket, res);
                        }
                        return 1;

                    } else {
                        /* error in track tilt */
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b>\n", thread, thread);
                            response_client(client_socket, track_error, res);
                        } else {
                            response_client(client_socket, track_error_raw, NULL);
                        }
                    }
                }

                /* error in track pan */
                if (cnt[0]->conf.webcontrol_html_output) {
                    sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br><b>Thread %hu</b>\n",
                                 thread, thread);
                    response_client(client_socket, track_error, res);
                } else {
                    response_client(client_socket, track_error_raw, NULL);
                }
            }
        } else if (length_uri == 0) {
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b><br>\n"
                             "<form action='set'>\n"
                             "Pan<input type=text name='pan' value=''>\n"
                             "Tilt<input type=text name='tilt' value=''>\n"
                             "<input type=submit value='set relative'>\n"
                             "</form>\n"
                             "<form action='set'>\n"
                             "X<input type=text name='x' value=''>\n"
                             "Y<input type=text name='y' value=''>\n"
                             "<input type=submit value='set absolute'>\n"
                             "</form>\n", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "set needs a pan/tilt or x/y values\n");
                send_template_raw(client_socket, res);
            }
        } else {
            /* error not valid command */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "center")) {
        pointer = pointer+6;
        length_uri = length_uri-6;
        if (length_uri==0) {
            struct context *setcnt;
            setcnt = cnt[thread];
            // 1000 is out of range for pwc
            cnt[thread]->moved = track_center(setcnt, setcnt->video_dev, 1, 0, 0);

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>"
                              "<br>track set center", thread, thread);
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                sprintf(res, "Thread %hu\n track set center\nDone\n", thread);
                send_template_ini_client_raw(client_socket);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error not valid command */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "status")) {
        pointer = pointer+6;
        length_uri = length_uri-6;
        if (length_uri==0) {
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>"
                             "<br>track auto %s", thread, thread,
                             (cnt[thread]->track.active)? "enabled":"disabled");
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                sprintf(res, "Thread %hu\n track auto %s\nDone\n", thread,
                             (cnt[thread]->track.active)? "enabled":"disabled");
                send_template_ini_client_raw(client_socket);
                send_template_raw(client_socket, res);
            }
        } else {
            /* error not valid command */
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else if (!strcmp(command, "auto")) {
        pointer = pointer + 4;
        length_uri = length_uri - 4;
        if ((question == '?') && (length_uri > 0)) {
            char query[256] = {'\0'};
            pointer++;
            length_uri--;
            /* value= */

            warningkill = sscanf(pointer, "%255[a-z]%c", query, &question);
            if ((question == '=') && (!strcmp(query, "value"))) {
                pointer = pointer + 6;
                length_uri = length_uri - 6;
                warningkill = sscanf(pointer, "%255[-0-9a-z]" , command);
                if ((command != NULL) && (strlen(command) > 0)) {
                    struct context *autocnt;

                    /* auto value=0|1|status */

                    if (!strcmp(command, "status")) {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            send_template_ini_client(client_socket, ini_template);
                            sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                         "<b>Thread %hu</b><br>"
                                     "track auto %s", thread, thread,
                                    (cnt[thread]->track.active)? "enabled":"disabled");
                            send_template(client_socket, res);
                            send_template_end_client(client_socket);
                        } else {
                            sprintf(res, "Thread %hu\n track auto %s\nDone\n", thread,
                                     (cnt[thread]->track.active)? "enabled":"disabled");
                            send_template_ini_client_raw(client_socket);
                            send_template_raw(client_socket, res);
                        }
                    } else {
                        int active;
                        active = atoi(command);
                        /* CHECK */
                        if (active > -1 && active < 2) {
                            autocnt = cnt[thread];
                            autocnt->track.active = active;
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>"
                                             "<b>Thread %hu</b>"
                                             "<br>track auto %s<br>", thread, thread,
                                             active ? "enabled":"disabled");
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "track auto %s\nDone\n", active ? "enabled":"disabled");
                                send_template_raw(client_socket, res);
                            }
                        } else {
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_found_response_valid_command, NULL);
                            else
                                response_client(client_socket, not_found_response_valid_command_raw, NULL);
                        }
                    }
                } else {
                    if (cnt[0]->conf.webcontrol_html_output)
                        response_client(client_socket, not_found_response_valid_command, NULL);
                    else
                        response_client(client_socket, not_found_response_valid_command_raw, NULL);
                }
            } else {
                if (cnt[0]->conf.webcontrol_html_output)
                    response_client(client_socket, not_found_response_valid_command, NULL);
                else
                    response_client(client_socket, not_found_response_valid_command_raw, NULL);
            }
        } else if (length_uri == 0) {

            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<a href=/%hu/track>&lt;&ndash; back</a><br><br>\n<b>Thread %hu</b>\n"
                             "<form action='auto'><select name='value'>\n"
                             "<option value='0' %s>Disable</option><option value='1' %s>Enable</option>\n"
                             "<option value='status'>status</option>\n"
                             "</select><input type=submit value='set'>\n"
                             "</form>\n", thread, thread, (cnt[thread]->track.active) ? "selected":"",
                             (cnt[thread]->track.active) ? "selected":"");
                send_template(client_socket, res);
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "auto accepts only 0,1 or status as valid value\n");
                send_template_raw(client_socket, res);
            }
        } else {
            if (cnt[0]->conf.webcontrol_html_output)
                response_client(client_socket, not_found_response_valid_command, NULL);
            else
                response_client(client_socket, not_found_response_valid_command_raw, NULL);
        }
    } else {
        if (cnt[0]->conf.webcontrol_html_output)
            response_client(client_socket, not_found_response_valid_command, NULL);
        else
            response_client(client_socket, not_found_response_valid_command_raw, NULL);
    }

    return 1;
}



/**
 * handle_get
 *      parses the action requested for motion ( config , action , detection , track )
 *      and call to action function if needed.
 *
 * Returns
 *      0 on action restart or quit
 *      1 on success
 */
static unsigned int handle_get(int client_socket, const char *url, void *userdata)
{
    struct context **cnt = userdata;

    if (*url == '/') {
        int i = 0;
        char *res=NULL;
        res = mymalloc(2048);

        /* get the number of threads */
        while (cnt[++i]);
        /* ROOT_URI -> GET / */
        if (!strcmp(url, "/")) {
            int y;
            if (cnt[0]->conf.webcontrol_html_output) {
                send_template_ini_client(client_socket, ini_template);
                sprintf(res, "<b>Motion "VERSION" Running [%hu] Threads</b><br>\n"
                             "<a href='/0/'>All</a><br>\n", i);
                send_template(client_socket, res);
                for (y = 1; y < i; y++) {
                    sprintf(res, "<a href='/%hu/'>Thread %hu</a><br>\n", y, y);
                    send_template(client_socket, res);
                }
                send_template_end_client(client_socket);
            } else {
                send_template_ini_client_raw(client_socket);
                sprintf(res, "Motion "VERSION" Running [%hu] Threads\n0\n", i);
                send_template_raw(client_socket, res);
                for (y = 1; y < i; y++) {
                    sprintf(res, "%hu\n", y);
                    send_template_raw(client_socket, res);
                }
            }
        } else {
            char command[256] = {'\0'};
            char slash;
            int thread = -1;
            size_t length_uri = 0;
            char *pointer = (char *)url;

            length_uri = strlen(url);
            /* Check for Thread number first -> GET /2 */
            pointer++;
            length_uri--;
            warningkill = sscanf(pointer, "%d%c", &thread, &slash);

            if ((thread != -1) && (thread < i)) {
                /* thread_number found */
                if (thread > 9) {
                    pointer = pointer + 2;
                    length_uri = length_uri - 2;
                } else {
                    pointer++;
                    length_uri--;
                }

                if (slash == '/') {   /* slash found /2/ */
                    pointer++;
                    length_uri--;
                }

                if (length_uri != 0) {
                    warningkill = sscanf(pointer, "%255[a-z]%c" , command , &slash);

                    /* config */
                    if (!strcmp(command, "config")) {
                        pointer = pointer + 6;
                        length_uri = length_uri - 6;
                        if (length_uri == 0) {
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a><br><br>\n"
                                             "<b>Thread %hd</b><br>\n"
                                             "<a href=/%hd/config/list>list</a><br>\n"
                                             "<a href=/%hd/config/write>write</a><br>\n"
                                             "<a href=/%hd/config/set>set</a><br>\n"
                                             "<a href=/%hd/config/get>get</a><br>\n",
                                             thread, thread, thread, thread, thread, thread);
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "Thread %hd\nlist\nwrite\nset\nget\n", thread);
                                send_template_raw(client_socket, res);
                            }
                        } else if ((slash == '/') && (length_uri >= 4)) {
                            /* call config() */
                            pointer++;
                            length_uri--;
                            config(pointer, res, length_uri, thread, client_socket, cnt);
                        } else {
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_found_response_valid_command, NULL);
                            else
                                response_client(client_socket, not_found_response_valid_command_raw, NULL);
                        }

                    } else if (!strcmp(command, "action")) { /* action */
                        pointer = pointer + 6;
                        length_uri = length_uri - 6;
                        /* call action() */
                        if (length_uri == 0) {
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a><br><br>\n"
                                             "<b>Thread %hd</b><br>\n"
                                             "<a href=/%hd/action/makemovie>makemovie</a><br>\n"
                                             "<a href=/%hd/action/snapshot>snapshot</a><br>\n"
                                             "<a href=/%hd/action/restart>restart</a><br>\n"
                                             "<a href=/%hd/action/quit>quit</a><br>\n",
                                             thread, thread, thread, thread, thread, thread);
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "Thread %hd\nmakemovie\nsnapshot\nrestart\nquit\n", thread);
                                send_template_raw(client_socket, res);
                            }
                        } else if ((slash == '/') && (length_uri > 4)) {
                            unsigned int ret = 1;
                            pointer++;
                            length_uri--;
                            ret = action(pointer, res, length_uri, thread, client_socket, cnt);
                            free(res);
                            return ret;

                        } else {
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_found_response_valid_command,NULL);
                            else
                                response_client(client_socket, not_found_response_valid_command_raw,NULL);
                        }
                    } else if (!strcmp(command, "detection")) {  /* detection */
                        pointer = pointer + 9;
                        length_uri = length_uri - 9;
                        if (length_uri == 0) {
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a><br><br>\n"
                                             "<b>Thread %hd</b><br>\n"
                                             "<a href=/%hd/detection/status>status</a><br>\n"
                                             "<a href=/%hd/detection/start>start</a><br>\n"
                                             "<a href=/%hd/detection/pause>pause</a><br>\n"
                                             "<a href=/%hd/detection/connection>connection</a><br>\n",
                                             thread, thread, thread, thread, thread, thread);
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "Thread %hd\nstatus\nstart\npause\nconnection\n", thread);
                                send_template_raw(client_socket, res);
                            }
                        } else if ((slash == '/') && (length_uri > 5)) {
                            pointer++;
                            length_uri--;
                            /* call detection() */
                            detection(pointer, res, length_uri, thread, client_socket, cnt);
                        } else {
                            if (cnt[0]->conf.webcontrol_html_output)
                                response_client(client_socket, not_found_response_valid_command, NULL);
                            else
                                response_client(client_socket, not_found_response_valid_command_raw, NULL);
                        }
                    } else if (!strcmp(command,"track")) { /* track */
                        pointer = pointer + 5;
                        length_uri = length_uri - 5;
                        if (length_uri == 0) {
                            if (cnt[0]->conf.webcontrol_html_output) {
                                send_template_ini_client(client_socket, ini_template);
                                sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a><br><br>\n"
                                             "<b>Thread %hd</b><br>\n"
                                             "<a href=/%hd/track/set>track set pan/tilt</a><br>\n"
                                             "<a href=/%hd/track/center>track center</a><br>\n"
                                             "<a href=/%hd/track/auto>track auto</a><br>\n"
                                             "<a href=/%hd/track/status>track status</a><br>\n",
                                             thread, thread, thread, thread, thread, thread);
                                send_template(client_socket, res);
                                send_template_end_client(client_socket);
                            } else {
                                send_template_ini_client_raw(client_socket);
                                sprintf(res, "Thread %hd\nset pan/tilt\ncenter\nauto\nstatus\n", thread);
                                send_template_raw(client_socket, res);
                            }

                        } else if ((slash == '/') && (length_uri >= 4)) {
                            pointer++;
                            length_uri--;
                            /* call track() */
                            if (cnt[thread]->track.type) {
                                track(pointer, res, length_uri, thread, client_socket, cnt);
                            } else {
                                /* error track not enable */
                                if (cnt[0]->conf.webcontrol_html_output) {
                                    sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a>\n", thread);
                                    response_client(client_socket, not_track, res);
                                } else {
                                    response_client(client_socket, not_track_raw, NULL);
                                }
                            }
                        } else {
                            if (cnt[0]->conf.webcontrol_html_output) {
                                sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a>\n", thread);
                                response_client(client_socket, not_found_response_valid_command, res);
                            } else {
                                response_client(client_socket, not_found_response_valid_command_raw, NULL);
                            }
                        }
                    } else {
                        if (cnt[0]->conf.webcontrol_html_output) {
                            sprintf(res, "<a href=/%hd/>&lt;&ndash; back</a>\n", thread);
                            response_client(client_socket, not_found_response_valid_command, res);
                        } else {
                            response_client(client_socket, not_found_response_valid_command_raw, NULL);
                        }
                    }
                } else {
                    /* /thread_number/ requested */
                    if (cnt[0]->conf.webcontrol_html_output) {
                        send_template_ini_client(client_socket, ini_template);
                        sprintf(res, "<a href=/>&lt;&ndash; back</a><br><br>\n<b>Thread %hd</b><br>\n"
                                     "<a href='/%hd/config'>config</a><br>\n"
                                     "<a href='/%hd/action'>action</a><br>\n"
                                     "<a href='/%hd/detection'>detection</a><br>\n"
                                     "<a href='/%hd/track'>track</a><br>\n",
                                     thread, thread, thread, thread, thread);
                        send_template(client_socket, res);
                        send_template_end_client(client_socket);
                    } else {
                        send_template_ini_client_raw(client_socket);
                        sprintf(res, "Thread %hd\nconfig\naction\ndetection\ntrack\n", thread);
                        send_template_raw(client_socket, res);
                    }
                }
            } else {
                if (cnt[0]->conf.webcontrol_html_output) {
                    sprintf(res, "<a href=/>&lt;&ndash; back</a>\n");
                    response_client(client_socket, not_found_response_valid, res);
                } else {
                    response_client(client_socket, not_found_response_valid_raw, NULL);
                }
            }
        }
        free(res);
    } else {
        if (cnt[0]->conf.webcontrol_html_output)
            response_client(client_socket, not_found_response_template,NULL);
        else
            response_client(client_socket, not_found_response_template_raw,NULL);
    }

    return 1;
}


/**
 * read_client
 *      As usually web clients uses nonblocking connect/read
 *      read_client should handle nonblocking sockets.
 *
 * Returns
 *      0 to quit or restart
 *      1 on success
 */
static unsigned int read_client(int client_socket, void *userdata, char *auth)
{
    unsigned int alive = 1;
    unsigned int ret = 1;
    char buffer[1024] = {'\0'};
    ssize_t length = 1023;
    struct context **cnt = userdata;

    /* lock the mutex */
    pthread_mutex_lock(&httpd_mutex);

    while (alive) {
        ssize_t nread = 0, readb = -1;

        nread = read_nonblock(client_socket, buffer, length);

        if (nread <= 0) {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd First Read Error");
            pthread_mutex_unlock(&httpd_mutex);
            return 1;
        } else {
            char method[10]={'\0'};
            char url[512]={'\0'};
            char protocol[10]={'\0'};
            char *authentication=NULL;

            buffer[nread] = '\0';

            warningkill = sscanf(buffer, "%9s %511s %9s", method, url, protocol);

            if (warningkill != 3) {
                if (cnt[0]->conf.webcontrol_html_output)
                    warningkill = write_nonblock(client_socket, bad_request_response,
                                  sizeof (bad_request_response));
                else
                    warningkill = write_nonblock(client_socket, bad_request_response_raw,
                                  sizeof (bad_request_response_raw));
                pthread_mutex_unlock(&httpd_mutex);
                return 1;
            }

            while ((strstr(buffer, "\r\n\r\n") == NULL) && (readb != 0) && (nread < length)) {
                readb = read_nonblock(client_socket, buffer+nread, sizeof (buffer) - nread);

                if (readb == -1) {
                    nread = -1;
                    break;
                }

                nread += readb;

                if (nread > length) {
                    MOTION_LOG(WRN, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd End buffer"
                               " reached waiting for buffer ending");
                    break;
                }
                buffer[nread] = '\0';
            }

            /*
             * Make sure the last read didn't fail.  If it did, there's a
             * problem with the connection, so give up.
             */
            if (nread == -1) {
                MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd READ give up!");
                pthread_mutex_unlock(&httpd_mutex);
                return 1;
            }
            alive = 0;

            /* Check Protocol */
            if (strcmp(protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                /* We don't understand this protocol.  Report a bad response.  */
                if (cnt[0]->conf.webcontrol_html_output)
                    warningkill = write_nonblock(client_socket, bad_request_response,
                                  sizeof (bad_request_response));
                else
                    warningkill = write_nonblock(client_socket, bad_request_response_raw,
                                  sizeof (bad_request_response_raw));

                pthread_mutex_unlock(&httpd_mutex);
                return 1;
            }

            if (strcmp (method, "GET")) {
                /*
                 * This server only implements the GET method.  If client
                 * uses other method, report the failure.
                 */
                char response[1024];
                if (cnt[0]->conf.webcontrol_html_output)
                    snprintf(response, sizeof (response), bad_method_response_template, method);
                else
                    snprintf(response, sizeof (response), bad_method_response_template_raw, method);
                warningkill = write_nonblock(client_socket, response, strlen(response));
                pthread_mutex_unlock(&httpd_mutex);
                return 1;
            }

            if (auth != NULL) {
                if ((authentication = strstr(buffer,"Basic"))) {
                    char *end_auth = NULL;
                    authentication = authentication + 6;

                    if ((end_auth  = strstr(authentication,"\r\n"))) {
                        authentication[end_auth - authentication] = '\0';
                    } else {
                        char response[1024];
                        snprintf(response, sizeof (response), request_auth_response_template, method);
                        warningkill = write_nonblock(client_socket, response, strlen(response));
                        pthread_mutex_unlock(&httpd_mutex);
                        return 1;
                    }

                    if (strcmp(auth, authentication)) {
                        char response[1024] = {'\0'};
                        snprintf(response, sizeof (response), request_auth_response_template, method);
                        warningkill = write_nonblock(client_socket, response, strlen(response));
                        pthread_mutex_unlock(&httpd_mutex);
                        return 1;
                    } else {
                        ret = handle_get(client_socket, url, cnt);
                        /* A valid auth request.  Process it.  */
                    }
                } else {
                    // Request Authorization
                    char response[1024] = {'\0'};
                    snprintf(response, sizeof (response), request_auth_response_template, method);
                    warningkill = write_nonblock(client_socket, response, strlen(response));
                    pthread_mutex_unlock(&httpd_mutex);
                    return 1;
                }
            } else {
                ret = handle_get(client_socket, url, cnt);
                /* A valid request.  Process it.  */
            }
        }
    }
    pthread_mutex_unlock(&httpd_mutex);

    return ret;
}


/**
 * acceptnonblocking
 *      waits timeout seconds for listen socket.
 *
 * Returns
 *      -1 if the timeout expires or on accept error.
 *      curfd (client socket) on accept success.
 */
static int acceptnonblocking(int serverfd, int timeout)
{
    int curfd;
    struct sockaddr_storage client;
    socklen_t namelen = sizeof(client);

    struct timeval tm;
    fd_set fds;

    tm.tv_sec = timeout; /* Timeout in seconds */
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(serverfd, &fds);

    if (select(serverfd + 1, &fds, NULL, NULL, &tm) > 0) {
        if (FD_ISSET(serverfd, &fds)) {
            if ((curfd = accept(serverfd, (struct sockaddr*)&client, &namelen)) > 0)
                return curfd;
        }
    }

    return -1;
}


/**
 * httpd_run
 *      Creates the listening socket and waits client requests.
 */
void httpd_run(struct context **cnt)
{
    int sd = -1, client_socket_fd, val;
    unsigned int client_sent_quit_message = 1, closehttpd = 0;
    struct addrinfo hints, *res = NULL, *ressave = NULL;
    struct sigaction act;
    char *authentication = NULL;
    char portnumber[10], hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    /* Initialize the mutex */
    pthread_mutex_init(&httpd_mutex, NULL);

    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    memset(&hints, 0, sizeof(struct addrinfo));
    /* AI_PASSIVE as we are going to listen */
    hints.ai_flags = AI_PASSIVE;
#if defined(BSD)
    hints.ai_family = AF_INET;
#else
    if (!cnt[0]->conf.ipv6_enabled)
        hints.ai_family = AF_INET;
    else
        hints.ai_family = AF_UNSPEC;
#endif
    hints.ai_socktype = SOCK_STREAM;

    snprintf(portnumber, sizeof(portnumber), "%u", cnt[0]->conf.webcontrol_port);

    val = getaddrinfo(cnt[0]->conf.webcontrol_localhost ? "localhost" : NULL, portnumber, &hints, &res);

    /* check != 0 to allow FreeBSD compatibility */
    if (val != 0) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO, "%s: getaddrinfo() for httpd socket failed: %s",
                   gai_strerror(val));
        if (res)
            freeaddrinfo(res);
        pthread_mutex_destroy(&httpd_mutex);
        return;
    }

    ressave = res;

    while (res) {
        /* create socket */
        sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
                    sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd testing : %s addr: %s port: %s",
                   res->ai_family == AF_INET ? "IPV4":"IPV6", hbuf, sbuf);

        if (sd >= 0) {
            val = 1;
            /* Reuse Address */
            setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));

            if (bind(sd, res->ai_addr, res->ai_addrlen) == 0) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd Bound : %s addr: %s"
                           " port: %s", res->ai_family == AF_INET ? "IPV4":"IPV6",
                           hbuf, sbuf);
                break;
            }

            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd failed bind() interface %s"
                       " / port %s, retrying", hbuf, sbuf);
            close(sd);
            sd = -1;
        }
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd socket failed interface %s"
                   " / port %s, retrying", hbuf, sbuf);
        res = res->ai_next;
    }

    freeaddrinfo(ressave);

    if (sd < 0) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd ERROR bind()"
                   " [interface %s port %s]", hbuf, sbuf);
        pthread_mutex_destroy(&httpd_mutex);
        return;
    }

    if (listen(sd, DEF_MAXWEBQUEUE) == -1) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO, "%s: motion-httpd ERROR listen()"
                   " [interface %s port %s]", hbuf, sbuf);
        close(sd);
        pthread_mutex_destroy(&httpd_mutex);
        return;
    }

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd/"VERSION" running,"
               " accepting connections");
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd: waiting for data"
               " on %s port TCP %s", hbuf, sbuf);

    if (cnt[0]->conf.webcontrol_authentication != NULL) {
        char *userpass = NULL;
        size_t auth_size = strlen(cnt[0]->conf.webcontrol_authentication);

        authentication = mymalloc(BASE64_LENGTH(auth_size) + 1);
        userpass = mymalloc(auth_size + 4);
        /* base64_encode can read 3 bytes after the end of the string, initialize it */
        memset(userpass, 0, auth_size + 4);
        strcpy(userpass, cnt[0]->conf.webcontrol_authentication);
        base64_encode(userpass, authentication, auth_size);
        free(userpass);
    }

    while ((client_sent_quit_message) && (!closehttpd)) {

        client_socket_fd = acceptnonblocking(sd, NONBLOCK_TIMEOUT);

        if (client_socket_fd < 0) {
            if ((!cnt[0]) || (cnt[0]->webcontrol_finish)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd - Finishing");
                closehttpd = 1;
            }
        } else {
            /* Get the Client request */
            client_sent_quit_message = read_client(client_socket_fd, cnt, authentication);
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd - Read from client");

            /* Close Connection */
            if (client_socket_fd)
                close(client_socket_fd);
        }

    }

    free(authentication);
    close(sd);
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd Closing");
    pthread_mutex_destroy(&httpd_mutex);
}

/**
 * motion_web_control
 *      Calls main function httpd_run
 */
void *motion_web_control(void *arg)
{
    struct context **cnt = arg;
    httpd_run(cnt);

    /* 
     * Update how many threads we have running. This is done within a
     * mutex lock to prevent multiple simultaneous updates to
     * 'threads_running'.
     */
    pthread_mutex_lock(&global_lock);
    threads_running--;
    cnt[0]->webcontrol_running = 0;
    pthread_mutex_unlock(&global_lock);

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "%s: motion-httpd thread exit");
    pthread_exit(NULL);
}
