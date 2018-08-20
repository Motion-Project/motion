#ifndef _INCLUDE_NETCAM_HTTP_H
#define _INCLUDE_NETCAM_HTTP_H


#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

void netcam_disconnect(netcam_context_ptr netcam);
int netcam_connect(netcam_context_ptr netcam, int err_flag);
int netcam_read_first_header(netcam_context_ptr netcam);
int netcam_setup_html(netcam_context_ptr netcam, struct url_t *url);
int netcam_read_next_header(netcam_context_ptr netcam);

#endif // _INCLUDE_NETCAM_HTTP_H
