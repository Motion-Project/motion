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
 *      Much of the FTP routines was inspired by the nanoftp.c module from
 *      libxml2 (Copyright Daniel Veillard, 2003).  The routines have been
 *      modified to fit the needs of the Motion project.
 *
 *      Copyright 2005, William M. Brack
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
int ftp_close(ftp_context_pointer ctxt);
int ftp_connect(netcam_context_ptr netcam);
int netcam_setup_ftp(netcam_context_ptr netcam, struct url_t *url);

#endif
