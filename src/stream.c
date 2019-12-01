/*
 *                 NOTICE
 *   This entire module (stream.c) is dead code and will be removed in its entirety very shortly
 *
 *   The only reason that it has not been removed yet is to allow for an emergency option to use
 *   the old stream method if for some reason the webu MHD option does not work on a particular
 *   platform.  It is only avaialable for activation using the undocumented option of
 *   cnt->conf.stream_preview_method == 99
 *
 *   NO REVISIONS SHOULD BE MADE TO THIS MODULE.  IT IS DEAD
 */


/*
 *    stream.c (based in webcam.c)
 *    Streaming using jpeg images over a multipart/x-mixed-replace stream
 *    Copyright (C) 2002 Jeroen Vreeken (pe1rxq@amsat.org)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "translate.h"
#include "md5.h"
#include "picture.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>

#define STREAM_REALM       "Motion Stream Security Access"
#define KEEP_ALIVE_TIMEOUT 100

typedef void* (*auth_handler)(void*);
struct auth_param {
    struct context *cnt;
    struct stream *stm;
    int *stream_count;
    int sock;
    int sock_flags;
    int* thread_count;
    struct config *conf;
};

/**
 * get_host
 *      Gets the host (IP) of a client from the socket file descriptor
 * Returns nothing
 */
static void get_host(char *buf, int fd)
{
    struct sockaddr_storage client;
    int retcd;

    socklen_t client_len = sizeof(client);
    int res = getpeername(fd, (struct sockaddr *)&client, &client_len);
    if (res != 0)
        return;

    char host[NI_MAXHOST];
    res = getnameinfo((struct sockaddr *)&client, client_len, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    if (res != 0)
        return;

    retcd = snprintf(buf,NI_MAXHOST - 1,"%s",host);
    if ((retcd < 0) || (retcd >= (int)NI_MAXHOST-1)){
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error specifying host"));
    }

}

pthread_mutex_t stream_auth_mutex;

/**
 * set_sock_timeout
 *
 * Returns : 0 or 1 on timeout
 */
static int set_sock_timeout(int sock, int sec)
{
    struct timeval tv;

    tv.tv_sec = sec;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv))) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("set socket timeout failed"));
        return 1;
    }
    return 0;
}

/**
 * read_http_request
 *
 *
 * Returns : 1 on success or 0 if any error happens
 */
static int read_http_request(int sock, char* buffer, int buflen, char* uri, int uri_len)
{
    int nread = 0;
    int ret,readb = 1;
    char method[10] = {'\0'};
    char url[512] = {'\0'};
    char protocol[10] = {'\0'};

#define bad_request_response_raw \
        "HTTP/1.0 400 Bad Request\r\n" \
        "Content-type: text/plain\r\n\r\n" \
        "Bad Request\n"

#define bad_method_response_template_raw \
        "HTTP/1.0 501 Method Not Implemented\r\n" \
        "Content-type: text/plain\r\n\r\n" \
        "Method Not Implemented\n"

#define timeout_response_template_raw \
        "HTTP/1.0 408 Request Timeout\r\n" \
        "Content-type: text/plain\r\n\r\n" \
        "Request Timeout\n"

    buffer[0] = '\0';

    while ((strstr(buffer, "\r\n\r\n") == NULL) && (readb != 0) && (nread < buflen)) {

        readb = read(sock, buffer+nread, buflen - nread);

        if (readb == -1) {
            nread = -1;
            break;
        }

        nread += readb;

        if (nread > buflen) {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
                ,_("motion-stream End buffer reached waiting for buffer ending"));
            break;
        }

        buffer[nread] = '\0';
    }

    /*
     * Make sure the last read didn't fail. If it did, there's a
     * problem with the connection, so give up.
     */
    if (nread == -1) {
        if(errno == EAGAIN) { // Timeout
            ret = write(sock, timeout_response_template_raw, strlen(timeout_response_template_raw));
            return 0;
        }

        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("motion-stream READ give up!"));
        return 0;
    }

    ret = sscanf(buffer, "%9s %511s %9s", method, url, protocol);

    if (ret != 3) {
        ret = write(sock, bad_request_response_raw, sizeof(bad_request_response_raw));
        return 0;
    }

    /* Check Protocol */
    if (strcmp(protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
        /* We don't understand this protocol. Report a bad response. */
        ret = write(sock, bad_request_response_raw, sizeof(bad_request_response_raw));
        return 0;
    }

    if (strcmp(method, "GET")) {
        /*
         * This server only implements the GET method. If client
         * uses other method, report the failure.
         */
        char response[1024];
        snprintf(response, sizeof(response), bad_method_response_template_raw);
        ret = write(sock, response, strlen (response));

        return 0;
    }

    if(uri){
        ret = snprintf(uri, uri_len,"%s",url);
        if ((ret < 0) || (ret >= uri_len)){
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
                ,_("Unable to set uri"));
        }
    }

    return 1;
}

static void stream_add_client(struct stream *list, int sc);

/**
 * handle_basic_auth
 *
 *
 */
static void* handle_basic_auth(void* param)
{
    struct auth_param *p = (struct auth_param*)param;
    char buffer[1024] = {'\0'};
    ssize_t length = 1023;
    char *auth, *h, *authentication;
    static const char *request_auth_response_template=
        "HTTP/1.0 401 Authorization Required\r\n"
        "Server: Motion/"VERSION"\r\n"
        "Max-Age: 0\r\n"
        "Expires: 0\r\n"
        "Cache-Control: no-cache, private\r\n"
        "Pragma: no-cache\r\n"
        "WWW-Authenticate: Basic realm=\""STREAM_REALM"\"\r\n\r\n";

    pthread_mutex_lock(&stream_auth_mutex);
    p->thread_count++;
    pthread_mutex_unlock(&stream_auth_mutex);

    if (!read_http_request(p->sock,buffer, length, NULL, 0))
        goto Invalid_Request;


    auth = strstr(buffer, "Authorization: Basic");

    if (!auth)
        goto Error;

    auth += sizeof("Authorization: Basic");
    h = strstr(auth, "\r\n");

    if(!h)
        goto Error;

    *h='\0';

    if (p->conf->stream_authentication != NULL) {

        char *userpass = NULL;
        size_t auth_size = strlen(p->conf->stream_authentication);

        authentication = mymalloc(BASE64_LENGTH(auth_size) + 1);
        userpass = mymalloc(auth_size + 4);
        /* motion_base64_encode can read 3 bytes after the end of the string, initialize it. */
        memset(userpass, 0, auth_size + 4);
        strcpy(userpass, p->conf->stream_authentication);
        motion_base64_encode(userpass, authentication, auth_size);
        free(userpass);

        if (strcmp(auth, authentication)) {
            free(authentication);
            char host[NI_MAXHOST] = "unknown";
            get_host(host, p->sock);
            MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
                ,_("motion-stream - failed auth attempt from %s"), host);
            goto Error;
        }
        free(authentication);
    }

    // OK - Access

    /* Set socket to non blocking */
    if (fcntl(p->sock, F_SETFL, p->sock_flags) < 0) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("fcntl"));
        goto Error;
    }

    /* Lock the mutex */
    pthread_mutex_lock(&stream_auth_mutex);

    stream_add_client(p->stm, p->sock);
    (*p->stream_count)++;
    p->thread_count--;

    /* Unlock the mutex */
    pthread_mutex_unlock(&stream_auth_mutex);

    free(p);
    pthread_exit(NULL);

Error:
    if (write(p->sock, request_auth_response_template, strlen (request_auth_response_template)) < 0)
        MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO
            ,_("write failure 1:handle_basic_auth"));

Invalid_Request:
    close(p->sock);

    pthread_mutex_lock(&stream_auth_mutex);
    p->thread_count--;
    pthread_mutex_unlock(&stream_auth_mutex);

    free(p);
    pthread_exit(NULL);
}


#define HASHLEN 16
typedef char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];
#define IN
#define OUT
/**
 * CvtHex
 *      Calculates H(A1) as per HTTP Digest spec -- taken from RFC 2617.
 */
static void CvtHex(IN HASH Bin, OUT HASHHEX Hex)
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
         else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
         else
            Hex[i*2+1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
};

/**
 * DigestCalcHA1
 *      Calculates H(A1) as per spec.
 */
static void DigestCalcHA1(
    IN char * pszAlg,
    IN char * pszUserName,
    IN char * pszRealm,
    IN char * pszPassword,
    IN char * pszNonce,
    IN char * pszCNonce,
    OUT HASHHEX SessionKey
    )
{
    MD5_CTX Md5Ctx;
    HASH HA1;

    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *)pszUserName, strlen(pszUserName));
    MD5Update(&Md5Ctx, (unsigned char *)":", 1);
    MD5Update(&Md5Ctx, (unsigned char *)pszRealm, strlen(pszRealm));
    MD5Update(&Md5Ctx, (unsigned char *)":", 1);
    MD5Update(&Md5Ctx, (unsigned char *)pszPassword, strlen(pszPassword));
    MD5Final((unsigned char *)HA1, &Md5Ctx);

    if (strcmp(pszAlg, "md5-sess") == 0) {
        MD5Init(&Md5Ctx);
        MD5Update(&Md5Ctx, (unsigned char *)HA1, HASHLEN);
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
        MD5Update(&Md5Ctx, (unsigned char *)pszNonce, strlen(pszNonce));
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
        MD5Update(&Md5Ctx, (unsigned char *)pszCNonce, strlen(pszCNonce));
        MD5Final((unsigned char *)HA1, &Md5Ctx);
    };
    CvtHex(HA1, SessionKey);
};

/**
 * DigestCalcResponse
 *      Calculates request-digest/response-digest as per HTTP Digest spec.
 */
static void DigestCalcResponse(
    IN HASHHEX HA1,           /* H(A1) */
    IN char * pszNonce,       /* nonce from server */
    IN char * pszNonceCount,  /* 8 hex digits */
    IN char * pszCNonce,      /* client nonce */
    IN char * pszQop,         /* qop-value: "", "auth", "auth-int" */
    IN char * pszMethod,      /* method from the request */
    IN char * pszDigestUri,   /* requested URL */
    IN HASHHEX HEntity,       /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response      /* request-digest or response-digest */
    )
{
    MD5_CTX Md5Ctx;
    HASH HA2;
    HASH RespHash;
    HASHHEX HA2Hex;

    // Calculate H(A2)
    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *)pszMethod, strlen(pszMethod));
    MD5Update(&Md5Ctx, (unsigned char *)":", 1);
    MD5Update(&Md5Ctx, (unsigned char *)pszDigestUri, strlen(pszDigestUri));

    if (strcmp(pszQop, "auth-int") == 0) {
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
        MD5Update(&Md5Ctx, (unsigned char *)HEntity, HASHHEXLEN);
    }
    MD5Final((unsigned char *)HA2, &Md5Ctx);
    CvtHex(HA2, HA2Hex);

    // Calculate response
    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *)HA1, HASHHEXLEN);
    MD5Update(&Md5Ctx, (unsigned char *)":", 1);
    MD5Update(&Md5Ctx, (unsigned char *)pszNonce, strlen(pszNonce));
    MD5Update(&Md5Ctx, (unsigned char *)":", 1);

    if (*pszQop) {
        MD5Update(&Md5Ctx, (unsigned char *)pszNonceCount, strlen(pszNonceCount));
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
        MD5Update(&Md5Ctx, (unsigned char *)pszCNonce, strlen(pszCNonce));
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
        MD5Update(&Md5Ctx, (unsigned char *)pszQop, strlen(pszQop));
        MD5Update(&Md5Ctx, (unsigned char *)":", 1);
    }
    MD5Update(&Md5Ctx, (unsigned char *)HA2Hex, HASHHEXLEN);
    MD5Final((unsigned char *)RespHash, &Md5Ctx);
    CvtHex(RespHash, Response);
};


/**
 * handle_md5_digest
 *
 *
 */
static void* handle_md5_digest(void* param)
{
    struct auth_param *p = (struct auth_param*)param;
    char buffer[1024] = {'\0'};
    ssize_t length = 1023;
    char *auth, *h, *username, *realm, *uri, *nonce, *response;
    int username_len, realm_len, uri_len, nonce_len, response_len;
#define SERVER_NONCE_LEN 17
    char server_nonce[SERVER_NONCE_LEN];
#define SERVER_URI_LEN 512
    char server_uri[SERVER_URI_LEN];
    char* server_user = NULL, *server_pass = NULL;
    unsigned int rand1,rand2;
    int retcd, len_user,len_pass;

    HASHHEX HA1;
    HASHHEX HA2 = "";
    HASHHEX server_response;
    static const char *request_auth_response_template=
        "HTTP/1.0 401 Authorization Required\r\n"
        "Server: Motion/"VERSION"\r\n"
        "Max-Age: 0\r\n"
        "Expires: 0\r\n"
        "Cache-Control: no-cache, private\r\n"
        "Pragma: no-cache\r\n"
        "WWW-Authenticate: Digest";
    static const char *auth_failed_html_template=
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>401 Authorization Required</title></head>\n"
        "<body>\n"
        "<h1>Authorization Required</h1>\n"
        "<p>This server could not verify that you are authorized to access the document "
        "requested.  Either you supplied the wrong credentials (e.g., bad password), "
        "or your browser doesn't understand how to supply the credentials required.</p>\n"
        "</body>\n"
        "</html>\n";
    static const char *internal_error_template=
        "HTTP/1.0 500 Internal Server Error\r\n"
        "Server: Motion/"VERSION"\r\n"
        "Content-Type: text/html\r\n"
        "Connection: Close\r\n\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>500 Internal Server Error</title></head>\n"
        "<body>\n"
        "<h1>500 Internal Server Error</h1>\n"
        "</body>\n"
        "</html>\n";

    pthread_mutex_lock(&stream_auth_mutex);
    p->thread_count++;
    pthread_mutex_unlock(&stream_auth_mutex);

    set_sock_timeout(p->sock, KEEP_ALIVE_TIMEOUT);
    srand(time(NULL));
    rand1 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    rand2 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(server_nonce, SERVER_NONCE_LEN, "%08x%08x", rand1, rand2);

    if (!p->conf->stream_authentication) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error no authentication data"));
        goto InternalError;
    }
    h = strstr(p->conf->stream_authentication, ":");

    if (!h) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error no authentication data (no ':' found)"));
        goto InternalError;
    }

    len_user = (h - p->conf->stream_authentication);
    len_pass = strlen(h);
    server_user = (char*)malloc(len_user + 1);
    server_pass = (char*)malloc(len_pass + 1);

    if (!server_user || !server_pass) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error malloc failed"));
        goto InternalError;
    }

    retcd = snprintf(server_user, len_user+1, "%s", p->conf->stream_authentication);
    if ((retcd < 0) || (retcd >= len_user)) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error server user"));
        goto InternalError;
    }

    retcd = snprintf(server_pass, len_pass+1, "%s", h);
    if ((retcd < 0) || (retcd >= len_user)) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error server pass"));
        goto InternalError;
    }


    while(1) {
        if(!read_http_request(p->sock, buffer, length, server_uri, SERVER_URI_LEN - 1))
            goto Invalid_Request;

        auth = strstr(buffer, "Authorization: Digest");
        if(!auth)
            goto Error;

        auth += sizeof("Authorization: Digest");
        h = strstr(auth, "\r\n");

        if (!h)
            goto Error;
        *h = '\0';

        // Username
        h=strstr(auth, "username=\"");

        if (!h)
            goto Error;

        username = h + 10;
        h = strstr(username + 1, "\"");

        if (!h)
            goto Error;

        username_len = h - username;

        // Realm
        h = strstr(auth, "realm=\"");
        if (!h)
            goto Error;

        realm = h + 7;
        h = strstr(realm + 1, "\"");

        if (!h)
            goto Error;

        realm_len = h - realm;

        // URI
        h = strstr(auth, "uri=\"");

        if (!h)
            goto Error;

        uri = h + 5;
        h = strstr(uri + 1, "\"");

        if (!h)
            goto Error;

        uri_len = h - uri;

        // Nonce
        h = strstr(auth, "nonce=\"");

        if (!h)
            goto Error;

        nonce = h + 7;
        h = strstr(nonce + 1, "\"");

        if (!h)
            goto Error;

        nonce_len = h - nonce;

        // Response
        h = strstr(auth, "response=\"");

        if (!h)
            goto Error;

        response = h + 10;
        h = strstr(response + 1, "\"");

        if (!h)
            goto Error;

        response_len = h - response;

        username[username_len] = '\0';
        realm[realm_len] = '\0';
        uri[uri_len] = '\0';
        nonce[nonce_len] = '\0';
        response[response_len] = '\0';

        DigestCalcHA1((char*)"md5", server_user, (char*)STREAM_REALM, server_pass, (char*)server_nonce, (char*)NULL, HA1);
        DigestCalcResponse(HA1, server_nonce, NULL, NULL, (char*)"", (char*)"GET", server_uri, HA2, server_response);

        if (strcmp(server_response, response) == 0){
            break;
        } else {
            char host[NI_MAXHOST] = "unknown";
            get_host(host, p->sock);
            MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
                ,_("motion-stream - failed auth attempt from %s"), host);
        }

Error:
        rand1 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
        rand2 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
        snprintf(server_nonce, SERVER_NONCE_LEN, "%08x%08x", rand1, rand2);
        snprintf(buffer, length, "%s realm=\""STREAM_REALM"\", nonce=\"%s\"\r\n"
                "Content-Type: text/html\r\n"
                "Keep-Alive: timeout=%i\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length: %zu\r\n\r\n",
                request_auth_response_template, server_nonce,
                KEEP_ALIVE_TIMEOUT, strlen(auth_failed_html_template));
        if (write(p->sock, buffer, strlen(buffer)) < 0)
            MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO
                ,_("write failure 1:handle_md5_digest"));
        if (write(p->sock, auth_failed_html_template, strlen(auth_failed_html_template)) < 0)
            MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO
                ,_("write failure 2:handle_md5_digest"));
    }

    // OK - Access

    /* Set socket to non blocking */
    if (fcntl(p->sock, F_SETFL, p->sock_flags) < 0) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("fcntl"));
        goto Error;
    }

    free(server_user);
    free(server_pass);

    /* Lock the mutex */
    pthread_mutex_lock(&stream_auth_mutex);

    stream_add_client(p->stm, p->sock);
    (*p->stream_count)++;

    p->thread_count--;
    /* Unlock the mutex */
    pthread_mutex_unlock(&stream_auth_mutex);

    free(p);
    pthread_exit(NULL);

InternalError:
    free(server_user);
    free(server_pass);

    if (write(p->sock, internal_error_template, strlen(internal_error_template)) < 0)
      MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO
        ,_("write failure 3:handle_md5_digest"));

Invalid_Request:
    close(p->sock);

    pthread_mutex_lock(&stream_auth_mutex);
    p->thread_count--;
    pthread_mutex_unlock(&stream_auth_mutex);

    free(p);
    pthread_exit(NULL);
}

/**
 * do_client_auth
 *
 *
 */
static void do_client_auth(struct context *cnt, struct stream *stm, int *stream_count, int sc)
{
    pthread_t thread_id;
    pthread_attr_t attr;
    auth_handler handle_func;
    struct auth_param* handle_param = NULL;
    int flags;
    static int first_call = 0;
    static int thread_count = 0;

    if(first_call == 0) {
        first_call = 1;
        /* Initialize the mutex */
        pthread_mutex_init(&stream_auth_mutex, NULL);
    }

    switch(cnt->conf.stream_auth_method)
    {
    case 1: // Basic
      handle_func = handle_basic_auth;
      break;
    case 2: // MD5 Digest
      handle_func = handle_md5_digest;
      break;
    default:
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
            ,_("Error unknown stream authentication method"));
      goto Error;
      break;
    }

    handle_param = mymalloc(sizeof(struct auth_param));
    handle_param->cnt = cnt;
    handle_param->stm = stm;
    handle_param->stream_count = stream_count;
    handle_param->sock = sc;
    handle_param->conf = &cnt->conf;
    handle_param->thread_count = &thread_count;

    /* Set socket to blocking */
    if ((flags = fcntl(sc, F_GETFL, 0)) < 0) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, _("fcntl"));
        goto Error;
    }
    handle_param->sock_flags = flags;

    if (fcntl(sc, F_SETFL, flags & (~O_NONBLOCK)) < 0) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, _("fcntl"));
        goto Error;
    }

    if (thread_count >= DEF_MAXSTREAMS)
        goto Error;

    if (pthread_attr_init(&attr)) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("Error pthread_attr_init"));
        goto Error;
    }

    if (pthread_create(&thread_id, &attr, handle_func, handle_param)) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("Error pthread_create"));
        goto Error;
    }
    pthread_detach(thread_id);

    if (pthread_attr_destroy(&attr))
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("Error pthread_attr_destroy"));

    return;

Error:
    close(sc);
    free(handle_param);
}

/**
 * http_bindsock
 *      Sets up a TCP/IP socket for incoming requests. It is called only during
 *      initialisation of Motion from the function stream_init
 *      The function sets up a a socket on the port number given by _port_.
 *      If the parameter _local_ is not zero the socket is setup to only accept connects from localhost.
 *      Otherwise any client IP address is accepted. The function returns an integer representing the socket.
 *
 * Returns: socket descriptor or -1 if any error happens
 */
int http_bindsock(int port, int local, int ipv6_enabled)
{
    int sd = socket(ipv6_enabled?AF_INET6:AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sd == -1){
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO,_("error creating socket"));
        return -1;
    }
    /* We can not do a SOCK_CLOEXEC on open since it is not supported on all platforms*/
    if (fcntl(sd, F_SETFD, FD_CLOEXEC) == -1){
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Unable to set FD_CLOEXEC"));
    };

    int yes = 1, no = 0;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
    {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO
            ,_("setting SO_REUSEADDR to yes failed"));
        /* we can carry on even if this failed */
    }

    if (ipv6_enabled)
    {
        if (setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) != 0)
        {
            MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO
                ,_("setting IPV6_V6ONLY to no failed"));
            /* we can carry on even if this failed */
        }
    }

    const char *addr_str;
    struct sockaddr_storage sin;
    socklen_t sinsize;
    bzero(&sin, sizeof(struct sockaddr_storage));
    sin.ss_family = ipv6_enabled?AF_INET6:AF_INET;
    if (ipv6_enabled) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&sin;
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        if(local) {
            addr_str = "::1";
            sin6->sin6_addr = in6addr_loopback;
        } else {
            addr_str = "any IPv4/IPv6 address";
            sin6->sin6_addr = in6addr_any;
        }
        sinsize = sizeof(*sin6);
    } else {
        struct sockaddr_in *sin4 = (struct sockaddr_in*)&sin;
        sin4->sin_family = AF_INET;
        sin4->sin_port = htons(port);
        if(local) {
            addr_str = "127.0.0.1";
            sin4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            addr_str = "any IPv4 address";
            sin4->sin_addr.s_addr = htonl(INADDR_ANY);
        }
        sinsize = sizeof(*sin4);
    }

    if (bind(sd, (struct sockaddr*)&sin, sinsize) != 0) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO
            ,_("error binding on %s port %d"), addr_str, port);
        close(sd);
        return -1;
    }

    if (listen(sd, DEF_MAXWEBQUEUE) != 0) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO,_("error listening"));
        close(sd);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        ,_("listening on %s port %d"), addr_str, port);

    return sd;
}

/**
 * http_acceptsock
 *
 *
 * Returns: socket descriptor or -1 if any error happens.
 */
static int http_acceptsock(int sl)
{
    int sc;
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    sc = accept(sl, (struct sockaddr*)&addr, &addr_len);

    if (sc < 0) {
        MOTION_LOG(CRT, TYPE_STREAM, SHOW_ERRNO,_("motion-stream accept()"));
        return -1;
    }

    unsigned long i = 1;
    ioctl(sc, FIONBIO, &i);
    return sc;
}


/**
 * stream_flush
 *      Sends any outstanding data to all connected clients.
 *      It continuously goes through the client list until no data is able
 *      to be sent (either because there isn't any, or because the clients
 *      are not able to accept it).
 */
static void stream_flush(struct stream *list, int *stream_count, int lim)
{
    int written;            /* The number of bytes actually written. */
    struct stream *client;  /* Pointer to the client being served. */
    int workdone = 0;       /* Flag set any time data is successfully
                               written. */

    client = list->next;

    while (client) {

        /* If data waiting for client, try to send it. */
        if (client->tmpbuffer) {

            /*
             * We expect that list->filepos < list->tmpbuffer->size
             * should always be true.  The check is more for safety,
             * in case of trouble is some other part of the code.
             * Note that if it is false, the following section will
             * clean up.
             */
            if (client->filepos < client->tmpbuffer->size) {

                /*
                 * Here we are finally ready to write out the
                 * data.  Remember that (because the socket
                 * has been set non-blocking) we may only
                 * write out part of the buffer.  The var
                 * 'filepos' contains how much of the buffer
                 * has already been written.
                 */
                written = write(client->socket,
                          client->tmpbuffer->ptr + client->filepos,
                          client->tmpbuffer->size - client->filepos);

                /*
                 * If any data has been written, update the
                 * data pointer and set the workdone flag.
                 */
                if (written > 0) {
                    client->filepos += written;
                    workdone = 1;
                }
            } else
                written = 0;

            /*
             * If we have written the entire buffer to the socket,
             * or if there was some error (other than EAGAIN, which
             * means the system couldn't take it), this request is
             * finished.
             */
            if ((client->filepos >= client->tmpbuffer->size) ||
                (written < 0 && errno != EAGAIN)) {
                /* If no other clients need this buffer, free it. */
                if (--client->tmpbuffer->ref <= 0) {
                    free(client->tmpbuffer->ptr);
                    free(client->tmpbuffer);
                    if (client->cors_header != NULL) free(client->cors_header);
                }

                /* Mark this client's buffer as empty. */
                client->tmpbuffer = NULL;
                client->nr++;
            }

            /*
             * If the client is no longer connected, or the total
             * number of frames already sent to this client is
             * greater than our configuration limit, disconnect
             * the client and free the stream struct.
             */
            if ((written < 0 && errno != EAGAIN) ||
                (lim && !client->tmpbuffer && client->nr > lim)) {
                void *tmp;

                close(client->socket);

                if (client->next)
                    client->next->prev = client->prev;

                client->prev->next = client->next;
                tmp = client;
                client = client->prev;
                free(tmp);
                (*stream_count)--;
            }
        }   /* End if (client->tmpbuffer) */

        /*
         * Step the the next client in the list.  If we get to the
         * end of the list, check if anything was written during
         * that loop; (if so) reset the 'workdone' flag and go back
         * to the beginning.
         */
        client = client->next;

        if (!client && workdone) {
            client = list->next;
            workdone = 0;
        }
    }   /* End while (client) */
}

/**
 * stream_tmpbuffer
 *      Routine to create a new "tmpbuffer", which is a common
 *      object used by all clients connected to a single camera.
 *
 * Returns: new allocated stream_buffer.
 */
static struct stream_buffer *stream_tmpbuffer(int size)
{
    struct stream_buffer *tmpbuffer = mymalloc(sizeof(struct stream_buffer));
    tmpbuffer->ref = 0;
    tmpbuffer->ptr = mymalloc(size);

    return tmpbuffer;
}

const char *base_header = "HTTP/1.0 200 OK\r\n"
                          "Server: Motion/"VERSION"\r\n"
                          "Connection: close\r\n"
                          "Max-Age: 0\r\n"
                          "Expires: 0\r\n"
                          "Cache-Control: no-cache, private\r\n"
                          "Pragma: no-cache\r\n"
                          "Content-Type: multipart/x-mixed-replace; "
                          "boundary=BoundaryString\r\n\r\n";
#define BASE_HEADER_LEN strlen(base_header)
/**
 * stream_add_client
 *
 *
 */
static void stream_add_client(struct stream *list, int sc)
{
    struct stream *new = mymalloc(sizeof(struct stream));
    memset(new, 0, sizeof(struct stream));
    new->socket = sc;

    // Copy the HTTP headers into tmpbuffer.

    if (list->cors_header == NULL) {

        new->tmpbuffer = stream_tmpbuffer(BASE_HEADER_LEN);
        if (new->tmpbuffer == NULL) {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
                ,_("Error creating tmpbuffer in stream_add_client"));
        } else {
            memcpy(new->tmpbuffer->ptr, base_header, BASE_HEADER_LEN);
            new->tmpbuffer->size = BASE_HEADER_LEN;
        }

    } else {

        const char *cors_header_key = "Access-Control-Allow-Origin: ";
        size_t cors_header_key_len = strlen(cors_header_key);
        size_t cors_header_len = strlen(list->cors_header);
        size_t size = BASE_HEADER_LEN-2 + cors_header_key_len + cors_header_len + 4;

        new->tmpbuffer = stream_tmpbuffer(size);
        if (new->tmpbuffer == NULL) {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
                ,_("Error creating tmpbuffer in stream_add_client"));
        } else {
            // Basically copy over the base headers (without the second \r\n),
            // and then the CORS header key, value, and \r\n\r\n.
            memcpy(new->tmpbuffer->ptr, base_header, BASE_HEADER_LEN-2);
            memcpy(&new->tmpbuffer->ptr[BASE_HEADER_LEN-2], cors_header_key, cors_header_key_len);
            memcpy(&new->tmpbuffer->ptr[BASE_HEADER_LEN-2 + cors_header_key_len], list->cors_header, cors_header_len);
            memcpy(&new->tmpbuffer->ptr[BASE_HEADER_LEN-2 + cors_header_key_len + cors_header_len], "\r\n\r\n", 4);
            new->tmpbuffer->size = size;
        }

    }

    new->prev = list;
    new->next = list->next;

    if (new->next)
        new->next->prev = new;

    list->next = new;
}

/**
 * stream_add_write
 *
 *
 */
static void stream_add_write(struct stream *list, struct stream_buffer *tmpbuffer, unsigned int fps)
{
    struct timeval curtimeval;
    unsigned long int curtime;

    gettimeofday(&curtimeval, NULL);
    curtime = curtimeval.tv_usec + 1000000L * curtimeval.tv_sec;

    while (list->next) {
        list = list->next;

        if (list->tmpbuffer == NULL && ((curtime - list->last) >= 1000000L / fps)) {
            list->last = curtime;
            list->tmpbuffer = tmpbuffer;
            tmpbuffer->ref++;
            list->filepos = 0;
        }
    }

    if (tmpbuffer->ref <= 0) {
        free(tmpbuffer->ptr);
        free(tmpbuffer);
    }
}


/**
 * stream_check_write
 *      We walk through the chain of stream structs until we reach the end.
 *      Here we check if the tmpbuffer points to NULL.
 *      We return 1 if it finds a list->tmpbuffer which is a NULL pointer which would
 *      be the next client ready to be sent a new image. If not a 0 is returned.
 *
 * Returns:
 */
static int stream_check_write(struct stream *list)
{
    while (list->next) {
        list = list->next;

        if (list->tmpbuffer == NULL)
            return 1;
    }
    return 0;
}


/**
 * stream_init
 *      This function is called from motion.c for each motion thread starting up.
 *      The function setup the incoming tcp socket that the clients connect to.
 *      The function returns an integer representing the socket.
 *
 * Returns: stream socket descriptor.
 */
int stream_init(struct stream *stm,
                int port,
                int localhost,
                int ipv6_enabled,
                const char *cors_header)
{
    stm->socket = http_bindsock(port, localhost, ipv6_enabled);
    stm->next = NULL;
    stm->prev = NULL;
    stm->cors_header = NULL;

    if (cors_header != NULL) {

        size_t size = strlen(cors_header) + 1;
        stm->cors_header = mymalloc(size);
        if (stm->cors_header == NULL) {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO
                ,_("Error allocated cors_header in stream_init"));
            return stm->socket;
        }
        memcpy(stm->cors_header, cors_header, size);

    }

    return stm->socket;
}

/**
 * stream_stop
 *      This function is called from the motion_loop when it ends
 *      and motion is terminated or restarted.
 */
void stream_stop(struct stream *stm)
{
    struct stream *list;
    struct stream *next = stm->next;

    /* TODO friendly info which socket is closing */
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        ,_("Closing motion-stream listen socket & active motion-stream sockets"));

    close(stm->socket);
    stm->socket = -1;
    free(stm->cors_header);

    while (next) {
        list = next;
        next = list->next;

        if (list->tmpbuffer) {
            free(list->tmpbuffer->ptr);
            free(list->tmpbuffer);
            if (list->cors_header != NULL) free(list->cors_header);
        }

        close(list->socket);
        free(list);
    }

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        ,_("Closed motion-stream listen socket & active motion-stream sockets"));
}

/*
 * stream_put
 *      Is the starting point of the stream loop. It is called from
 *      the motion_loop with the argument 'image' pointing to the latest frame.
 *      If config option 'stream_motion' is 'on' this function is called once
 *      per second (frame 0) and when Motion is detected excl pre_capture.
 *      If config option 'stream_motion' is 'off' this function is called once
 *      per captured picture frame.
 *      It is always run in setup mode for each picture frame captured and with
 *      the special setup image.
 *      The function does two things:
 *          It looks for possible waiting new clients and adds them.
 *          It sends latest picture frame to all connected clients.
 *      Note: Clients that have disconnected are handled in the stream_flush()
 *          function.
 */
void stream_put(struct context *cnt, struct stream *stm, int *stream_count, unsigned char *image,
            int do_scale_down)
{
    struct timeval timeout;
    struct stream_buffer *tmpbuffer;
    fd_set fdread;
    int sl = stm->socket;
    int sc;
    /* Tthe following string has an extra 16 chars at end for length. */
    const char jpeghead[] = "--BoundaryString\r\n"
                            "Content-type: image/jpeg\r\n"
                            "Content-Length:                ";
    int headlength = sizeof(jpeghead) - 1;    /* Don't include terminator. */
    char len[20];    /* Will be used for sprintf, must be >= 16 */

    /* will point either to the original image or a scaled down */
    unsigned char *img = image;
    int image_width = cnt->imgs.width;
    int image_height = cnt->imgs.height;
    int image_size = cnt->imgs.size_norm;
    /*
     * Timeout struct used to timeout the time we wait for a client
     * and we do not wait at all.
     */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&fdread);
    FD_SET(stm->socket, &fdread);

    /*
     * If we have not reached the max number of allowed clients per
     * thread we will check to see if new clients are waiting to connect.
     * If this is the case we add the client as a new stream struct and
     * add this to the end of the chain of stream structs that are linked
     * to each other.
     */
    if ((*stream_count < DEF_MAXSTREAMS) &&
        (select(sl + 1, &fdread, NULL, NULL, &timeout) > 0)) {
        sc = http_acceptsock(sl);
        if (cnt->conf.stream_auth_method == 0) {
            stream_add_client(stm, sc);
            (*stream_count)++;
        } else  {
            do_client_auth(cnt, stm, stream_count, sc);
        }
    }

    /* if there is no connected clients - nothing to do, return */
    if (*stream_count <= 0)
        return;

    /* Lock the mutex */
    if (cnt->conf.stream_auth_method != 0)
        pthread_mutex_lock(&stream_auth_mutex);


    /* Call flush to send any previous partial-sends which are waiting. */
    stream_flush(stm, stream_count, cnt->conf.stream_limit);

    /* Check if any clients have available buffers. */
    if (stream_check_write(stm)) {
        /*
         * Yes - create a new tmpbuffer for current image.
         * Note that this should create a buffer which is *much* larger
         * than necessary, but it is difficult to estimate the
         * minimum size actually required.
         */
        tmpbuffer = stream_tmpbuffer(cnt->imgs.size_norm);

        /* Check if allocation was ok. */
        if (tmpbuffer) {
            int imgsize;

            /*
             * We need a pointer that points to the picture buffer
             * just after the mjpeg header. We create a working pointer wptr
             * to be used in the call to put_picture_memory which we can change
             * and leave tmpbuffer->ptr intact.
             */
            unsigned char *wptr = tmpbuffer->ptr;

            /*
             * For web protocol, our image needs to be preceded
             * with a little HTTP, so we put that into the buffer
             * first.
             */
            memcpy(wptr, jpeghead, headlength);

            /* Update our working pointer to point past header. */
            wptr += headlength;

            /* Create a jpeg image and place into tmpbuffer. */
            tmpbuffer->size = put_picture_memory(cnt, wptr, image_size, img,
                                       cnt->conf.stream_quality, image_width, image_height);

            /* Fill in the image length into the header. */
            imgsize = sprintf(len, "%9ld\r\n\r\n", tmpbuffer->size);
            memcpy(wptr - imgsize, len, imgsize);

            /* Append a CRLF for good measure. */
            memcpy(wptr + tmpbuffer->size, "\r\n", 2);

            /*
             * Now adjust tmpbuffer->size to reflect the
             * header at the beginning and the extra CRLF
             * at the end.
             */
            tmpbuffer->size += headlength + 2;

            /*
             * And finally put this buffer to all clients with
             * no outstanding data from previous frames.
             */
            stream_add_write(stm, tmpbuffer, cnt->conf.stream_maxrate);
        } else {
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,_("Error creating tmpbuffer"));
        }
    }

    /*
     * Now we call flush again.  This time (assuming some clients were
     * ready for the new frame) the new data will be written out.
     */
    stream_flush(stm, stream_count, cnt->conf.stream_limit);

    /* Unlock the mutex */
    if (cnt->conf.stream_auth_method != 0)
        pthread_mutex_unlock(&stream_auth_mutex);

    /* free resized image buffer */
    if (do_scale_down)
    {
        free (img);
    }

    return;
}
