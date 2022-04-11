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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
 */


/*
 *    Header files for the database functionality.
 */
#ifndef _INCLUDE_DBSE_HPP_
#define _INCLUDE_DBSE_HPP_

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

    enum DBSE_ACT {
        DBSE_ACT_CHKTBL     = 0,
        DBSE_ACT_GETCNT     = 1,
        DBSE_ACT_GETTBL     = 2
    };


    /* Database structure for user specified database*/
    struct ctx_dbse {
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

        int             sql_mask;
        uint64_t        database_event_id;
    };

    /* Record structure of motionplus database */
    struct ctx_dbsemp_rec {
        int64_t     rowid;      /*rowid from database*/
        int         camid;      /*camera id */
        char        *movie_nm;  /*Name of the movie file*/
        char        *full_nm;   /*Name of the movie file*/
        int64_t     movie_sz;   /*Size of the movie file in bytes*/
        int         movie_dtl;  /*Date in yyyymmdd format for the movie file*/
        bool        found;      /*Bool for whether the file exists in dir*/
    };

    /* Database structure for motionplus dedicated database*/
    struct ctx_dbsemp {
        #ifdef HAVE_SQLITE3
            sqlite3 *database_sqlite3;
        #endif
        enum DBSE_ACT           dbse_action;    /* action to perform with query*/
        bool                    table_ok;       /* bool of whether table exists*/
        int                     movie_indx;     /* index of movie_list */
        int                     movie_cnt;      /* count of movie_list */
        struct ctx_dbsemp_rec   *movie_list;    /* list of movies from the database*/
    };


    void dbse_global_deinit(struct ctx_motapp *motapp);
    void dbse_global_init(struct ctx_motapp *motapp);
    void dbse_init(struct ctx_cam *cam);
    void dbse_deinit(struct ctx_cam *cam);
    void dbse_sqlmask_update(struct ctx_cam *cam);
    void dbse_firstmotion(struct ctx_cam *cam);
    void dbse_newfile(struct ctx_cam *cam, char *filename, int sqltype, struct timespec *ts1);
    void dbse_fileclose(struct ctx_cam *cam, char *filename, int sqltype, struct timespec *ts1);
    void dbse_motpls_init(struct ctx_cam *cam);
    void dbse_motpls_deinit(struct ctx_cam *cam);
    void dbse_motpls_exec(char *sqlquery, struct ctx_cam *cam);
    int dbse_motpls_getlist(struct ctx_cam *cam);
    void dbse_motpls_addrec(struct ctx_cam *cam,char *fname, struct timespec *ts1);

#endif /* _INCLUDE_DBSE_HPP_ */