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
        DBSE_ACT_GETTBL     = 2,
        DBSE_ACT_GETCOLS    = 3
    };

    /* Record structure of motionplus table */
    struct ctx_dbse_rec {
        bool        found;      /*Bool for whether the file exists*/
        int64_t     record_id;  /*record_id*/
        int         camera_id;  /*camera id */
        char        *movie_nm;  /*Name of the movie file*/
        char        *movie_dir; /*Directory of the movie file */
        char        *full_nm;   /*Full name of the movie file with dir*/
        int64_t     movie_sz;   /*Size of the movie file in bytes*/
        int         movie_dtl;  /*Date in yyyymmdd format for the movie file*/
        char        *movie_tmc; /*Movie time 12h format*/
        char        *movie_tml; /*Movie time 24h format*/
        int         diff_avg;   /*Average diffs for motion frames */
        int         sdev_min;   /*std dev min */
        int         sdev_max;   /*std dev max */
        int         sdev_avg;   /*std dev average */
    };

    /* Columns in the motionplus table */
    struct ctx_dbse_col {
        bool        found;      /*Bool for whether the col in existing db*/
        char        *col_nm;    /*Name of the column*/
        char        *col_typ;   /*Data type of the column*/
    };

    /* Database context structure*/
    struct ctx_dbse {
        #ifdef HAVE_SQLITE3
            sqlite3 *database_sqlite3;
        #endif
        #ifdef HAVE_MARIADB
            MYSQL *database_mariadb;
        #endif
        #ifdef HAVE_PGSQL
            PGconn *database_pgsql;
        #endif

        std::string     database_type;
        std::string     database_dbname;
        std::string     database_host;
        std::string     database_user;
        std::string     database_password;

        int             database_port;
        int             database_busy_timeout;

        pthread_mutex_t     mutex_dbse;     /* mutex for database*/
        enum DBSE_ACT       dbse_action;    /* action to perform with query*/
        bool                table_ok;       /* bool of whether table exists*/
        int                 rec_indx;       /* index of recordset */
        int                 movie_cnt;      /* count of movie_list */
        struct ctx_dbse_rec *movie_list;    /* list of movies from the database*/
        int                 cols_cnt;       /* count of columns */
        struct ctx_dbse_col *cols_list;     /* columns of table from the database*/


    };

    void dbse_init(ctx_motapp *motapp);
    void dbse_init_motpls(ctx_motapp *motapp);
    void dbse_deinit(ctx_motapp *motapp);
    void dbse_deinit_motpls(ctx_motapp *motapp);
    void dbse_exec(ctx_cam *cam, char *filename
        , int sqltype, timespec *ts1, const char *cmd);
    void dbse_movies_getlist(ctx_motapp *motapp, int camera_id);
    void dbse_movies_addrec(ctx_cam *cam, ctx_movie *movie, timespec *ts1);

#endif /* _INCLUDE_DBSE_HPP_ */