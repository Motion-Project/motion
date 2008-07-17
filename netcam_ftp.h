
/**
 *      Much of the FTP routines was inspired by the nanoftp.c module from
 *      libxml2 (Copyright Daniel Veillard, 2003).  The routines have been
 *      modified to fit the needs of the Motion project.
 *
 *      Copyright 2005, William M. Brack
 *      This software is distributed under the GNU Public license Version 2.
 *      See also the file 'COPYING'.
 *      
 */
#ifndef _INCLUDE_NETCAM_FTP_H
#define _INCLUDE_NETCAM_FTP_H

#define FTP_BUF_SIZE    1024

typedef struct ftp_context {
    char      *path;               /* the path within the URL */
    char      *user;               /* user string */
    char      *passwd;             /* passwd string */
    struct    sockaddr_in ftp_address; /* the socket addr structure */
    int       passive;             /* flag show passive/active mode used */
    int       control_file_desc;   /* file descriptor for the control socket */
    int       data_file_desc;      /* file descriptor for the data socket */
    int       state;               /* WRITE / READ / CLOSED */
    int       returnValue;         /* the protocol return value */

    /* buffer for data received from the control connection */
    char      control_buffer[FTP_BUF_SIZE + 1];
    int       control_buffer_index;
    int       control_buffer_used;
    int       control_buffer_answer;
} ftp_context, *ftp_context_pointer;

/* The public interface */
ftp_context_pointer ftp_new_context(void);
void ftp_free_context(ftp_context_pointer);
ftp_context_pointer ftpOpen(const char *);
int ftp_connect(netcam_context_ptr);
int ftp_send_type(ftp_context_pointer, const char);
int ftp_get_socket(ftp_context_pointer);
int ftp_read(ftp_context_pointer, void *, int);
int ftp_close(ftp_context_pointer);
#endif
