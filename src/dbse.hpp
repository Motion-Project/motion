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


/*
 *    Header files for the database functionality.
 */
#ifndef _INCLUDE_DBSE_H_
#define _INCLUDE_DBSE_H_

#ifdef HAVE_MYSQL
    #include <mysql.h>
#endif

#ifdef HAVE_MARIADB
    #include <mysql.h>
#endif

#ifdef HAVE_SQLITE3
    #include <sqlite3.h>
#endif

#ifdef HAVE_PGSQL
    #include <libpq-fe.h>
#endif

struct ctx_dbse {
    int                sql_mask;
    unsigned long long database_event_id;

    #ifdef HAVE_SQLITE3
        sqlite3 *database_sqlite3;
    #endif

    #ifdef HAVE_MYSQL
        MYSQL *database_mysql;
    #endif

    #ifdef HAVE_MARIADB
        MYSQL *database_mariadb;
    #endif

    #ifdef HAVE_PGSQL
        PGconn *database_pg;
    #endif

};

void dbse_global_deinit(struct ctx_motapp *motapp);
void dbse_global_init(struct ctx_motapp *motapp);
void dbse_init(struct ctx_cam *cam);
void dbse_deinit(struct ctx_cam *cam);
void dbse_sqlmask_update(struct ctx_cam *cam);
void dbse_firstmotion(struct ctx_cam *cam);
void dbse_newfile(struct ctx_cam *cam, char *filename, int sqltype, struct timespec *ts1);
void dbse_fileclose(struct ctx_cam *cam, char *filename, int sqltype, struct timespec *ts1);

#endif