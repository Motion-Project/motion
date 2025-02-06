/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
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
    DBSE_COLS_LIST,
    DBSE_COLS_CURRENT,
    DBSE_COLS_ADD,
    DBSE_COLS_RENAME,
    DBSE_END
};

/* Record structure of table */
struct ctx_file_item {
    bool        found;      /*Bool for whether the file exists*/
    int64_t     record_id;  /*record_id*/
    int         device_id;  /*camera id */
    std::string file_typ;   /*type of file (pic/movie)*/
    std::string file_nm;    /*Name of the file*/
    std::string file_dir;   /*Directory of the file */
    std::string full_nm;    /*Full name of the file with dir*/
    int64_t     file_sz;    /*Size of the file in bytes*/
    int         file_dtl;   /*Date in yyyymmdd format for the file*/
    std::string file_tmc;   /*File time 12h format*/
    std::string file_tml;   /*File time 24h format*/
    int         diff_avg;   /*Average diffs for motion frames */
    int         sdev_min;   /*std dev min */
    int         sdev_max;   /*std dev max */
    int         sdev_avg;   /*std dev average */
};
typedef std::vector<ctx_file_item> vec_files;

/* Column item attributes in the motion table */
struct ctx_col_item {
    bool        found;      /*Bool for whether the col in existing db*/
    std::string col_nm;     /*Name of the column*/
    std::string col_typ;    /*Data type of the column*/
    int         col_idx;    /*Sequence index*/
};
typedef std::vector<ctx_col_item> vec_cols;

class cls_dbse {
    public:
        cls_dbse(cls_motapp *p_app);
        ~cls_dbse();
        void sqlite3db_cb (int arg_nb, char **arg_val, char **col_nm);
        pthread_mutex_t     mutex_dbse;
        void exec(cls_camera *cam, std::string filename, std::string cmd);
        void exec_sql(std::string sql);
        void filelist_add(cls_camera *cam, timespec *ts1, std::string ftyp
            ,std::string filenm, std::string fullnm, std::string dirnm);
        void filelist_get(std::string sql, vec_files &p_flst);
        bool restart;
        bool finish;
        void shutdown();
        void startup();

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();

    private:
        #ifdef HAVE_SQLITE3DB
            sqlite3 *database_sqlite3db;
            void sqlite3db_exec(std::string sql);
            void sqlite3db_cols_verify();
            void sqlite3db_cols_rename();
            void sqlite3db_init();
            void sqlite3db_close();
            void sqlite3db_filelist(std::string sql);
        #endif
        #ifdef HAVE_MARIADB
            MYSQL *database_mariadb;
            void mariadb_exec(std::string sql);
            void mariadb_recs(std::string sql);
            void mariadb_cols_verify();
            void mariadb_cols_rename();
            void mariadb_setup();
            void mariadb_init();
            void mariadb_close();
            void mariadb_filelist(std::string sql);
        #endif
        #ifdef HAVE_PGSQLDB
            PGconn *database_pgsqldb;
            void pgsqldb_exec(std::string sql);
            void pgsqldb_recs(std::string sql);
            void pgsqldb_cols_verify();
            void pgsqldb_cols_rename();
            void pgsqldb_setup();
            void pgsqldb_init();
            void pgsqldb_close();
            void pgsqldb_filelist(std::string sql);
        #endif
        cls_motapp          *app;
        enum DBSE_ACT       dbse_action;    /* action to perform with query*/
        bool                table_ok;       /* bool of whether table exists*/
        bool                is_open;

        vec_cols            col_names;
        vec_files           filelist;
        ctx_file_item       file_item;

        void handler_startup();
        void handler_shutdown();
        void timing();
        bool check_exit();
        void dbse_clean();
        void dbse_edits();
        bool dbse_open();

        void cols_vec_add(std::string nm, std::string typ);
        void cols_vec_create();
        void item_default();
        void item_assign(std::string col_nm, std::string col_val);

        void sql_motion(std::string &sql);
        void sql_motion(std::string &sql, std::string col_p1, std::string col_p2);
};

#endif /* _INCLUDE_DBSE_HPP_ */
