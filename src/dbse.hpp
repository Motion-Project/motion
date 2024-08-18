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
 */

#ifndef _INCLUDE_DBSE_HPP_
#define _INCLUDE_DBSE_HPP_

#ifdef HAVE_MARIADB
    #include <mysql.h>
    #ifndef HAVE_DBSE
        #define HAVE_DBSE
    #endif
#endif

#ifdef HAVE_SQLITE3DB
    #include <sqlite3.h>
    #ifndef HAVE_DBSE
        #define HAVE_DBSE
    #endif
#endif

#ifdef HAVE_PGSQLDB
    #include <libpq-fe.h>
    #ifndef HAVE_DBSE
        #define HAVE_DBSE
    #endif
#endif

enum DBSE_ACT {
    DBSE_TBL_CHECK,
    DBSE_TBL_CREATE,
    DBSE_MOV_SELECT,
    DBSE_MOV_CLEAN,
    DBSE_COLS_LIST,
    DBSE_COLS_ADD,
    DBSE_END
};

/* Record structure of motionplus table */
struct ctx_movie_item {
    bool        found;      /*Bool for whether the file exists*/
    int64_t     record_id;  /*record_id*/
    int         device_id;  /*camera id */
    std::string movie_nm;  /*Name of the movie file*/
    std::string movie_dir; /*Directory of the movie file */
    std::string full_nm;   /*Full name of the movie file with dir*/
    int64_t     movie_sz;   /*Size of the movie file in bytes*/
    int         movie_dtl;  /*Date in yyyymmdd format for the movie file*/
    std::string movie_tmc; /*Movie time 12h format*/
    std::string movie_tml; /*Movie time 24h format*/
    int         diff_avg;   /*Average diffs for motion frames */
    int         sdev_min;   /*std dev min */
    int         sdev_max;   /*std dev max */
    int         sdev_avg;   /*std dev average */
};
typedef std::list<ctx_movie_item> lst_movies;
typedef lst_movies::iterator it_movies;

/* Column item attributes in the motionplus table */
struct ctx_col_item {
    bool        found;      /*Bool for whether the col in existing db*/
    std::string col_nm;     /*Name of the column*/
    std::string col_typ;    /*Data type of the column*/
    int         col_idx;    /*Sequence index*/
};
typedef std::list<ctx_col_item> lst_cols;
typedef lst_cols::iterator it_cols;

class cls_dbse {
    public:
        cls_dbse(cls_motapp *p_app);
        ~cls_dbse();
        void sqlite3db_cb (int arg_nb, char **arg_val, char **col_nm);
        pthread_mutex_t     mutex_dbse;
        void exec(cls_camera *cam, std::string filename, std::string cmd);
        void movielist_add(cls_camera *cam, cls_movie *movie, timespec *ts1);
        void movielist_get(int p_device_id, lst_movies *p_movielist);
        bool    restart;
        void shutdown();
        void startup();
    private:
        #ifdef HAVE_SQLITE3DB
            sqlite3 *database_sqlite3db;
            void sqlite3db_exec(std::string sql);
            void sqlite3db_cols();
            void sqlite3db_init();
            void sqlite3db_movielist();
            void sqlite3db_close();
        #endif
        #ifdef HAVE_MARIADB
            MYSQL *database_mariadb;
            void mariadb_exec (std::string sql);
            void mariadb_recs (std::string sql);
            void mariadb_cols();
            void mariadb_setup();
            void mariadb_init();
            void mariadb_close();
            void mariadb_movielist();
        #endif
        #ifdef HAVE_PGSQLDB
            PGconn *database_pgsqldb;
            void pgsqldb_exec(std::string sql);
            void pgsqldb_close();
            void pgsqldb_recs (std::string sql);
            void pgsqldb_cols();
            void pgsqldb_setup();
            void pgsqldb_init();
            void pgsqldb_movielist();
        #endif
        cls_motapp      *app;
        enum DBSE_ACT       dbse_action;    /* action to perform with query*/
        bool                table_ok;       /* bool of whether table exists*/
        bool                is_open;
        lst_cols            col_names;
        lst_movies          *movielist;
        int                 device_id;
        ctx_movie_item      movie_item;

        void cols_add_itm(std::string nm, std::string typ);
        void get_cols_list();
        void movie_item_default();
        void movie_item_assign(std::string col_nm, std::string col_val);
        void sql_motpls(std::string &sql);
        void sql_motpls(std::string &sql, std::string col_nm, std::string col_typ);

        void dbse_edits();
        bool dbse_open();
        void exec_sql(std::string sql);

};

#endif /* _INCLUDE_DBSE_HPP_ */