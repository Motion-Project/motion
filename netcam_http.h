#ifndef _INCLUDE_NETCAM_HTTP_H
#define _INCLUDE_NETCAM_HTTP_H


#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MJPG_MH_MAGIC          "MJPG"
#define MJPG_MH_MAGIC_SIZE          4

typedef struct file_context {
    char      *path;               /* the path within the URL */
    int       control_file_desc;   /* file descriptor for the control socket */
    time_t    last_st_mtime;       /* time this image was modified */
} tfile_context;


/*
 * MJPG Chunk header for MJPG streaming.
 * Little-endian data is read from the network.
 */
typedef struct {
    char mh_magic[MJPG_MH_MAGIC_SIZE];     /* must contain the string MJP
                                              not null-terminated. */
    unsigned int mh_framesize;             /* Total size of the current
                                              frame in bytes (~45kb on WVC200) */
    unsigned short mh_framewidth;          /* Frame width in pixels */
    unsigned short mh_frameheight;         /* Frame height in pixels */
    unsigned int mh_frameoffset;           /* Offset of this chunk relative
                                              to the beginning of frame. */
    unsigned short mh_chunksize;           /* The size of the chunk data
                                              following this header. */
    char mh_reserved[30];                  /* Unknown data, seems to be
                                              constant between all headers */
} mjpg_header;


void netcam_disconnect(netcam_context_ptr netcam);
int netcam_connect(netcam_context_ptr netcam, int err_flag);
int netcam_read_first_header(netcam_context_ptr netcam);
int netcam_setup_html(netcam_context_ptr netcam, struct url_t *url);
int netcam_setup_mjpg(netcam_context_ptr netcam, struct url_t *url);
int netcam_setup_file(netcam_context_ptr netcam, struct url_t *url);
int netcam_read_next_header(netcam_context_ptr netcam);

#endif // _INCLUDE_NETCAM_HTTP_H
