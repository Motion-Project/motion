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


#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "dbse.hpp"

static int dbse_global_edits(struct ctx_cam **cam_list)
{

    int retcd = 0;

    if (cam_list[0]->conf->database_dbname == "") {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Invalid database name"));
        retcd = -1;
    }
    if ((((cam_list[0]->conf->database_type == "mysql")) ||
         ((cam_list[0]->conf->database_type == "mariadb")) ||
         ((cam_list[0]->conf->database_type == "pgsql"))) &&
        (cam_list[0]->conf->database_port == 0)) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Must specify database port for mysql/mariadb/pgsql"));
        retcd = -1;
    }

    if (retcd == -1) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Database functionality disabled."));
        cam_list[0]->conf->database_type = "";
    }

    return retcd;

}

void dbse_global_deinit(struct ctx_motapp *motapp)
{

    int indx;

    #if defined(HAVE_MYSQL)
        if (motapp->cam_list[0]->conf->database_type != "") {
            if ((motapp->cam_list[0]->conf->database_type == "mysql")) {
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing MYSQL"));
                mysql_library_end();
            }
        }
    #else
        (void)motapp;
    #endif /* HAVE_MYSQL */

    #if defined(HAVE_MARIADB)
        if (motapp->cam_list[0]->conf->database_type != "") {
            if ((motapp->cam_list[0]->conf->database_type == "mariadb")) {
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing MYSQL"));
                mysql_library_end();
            }
        }
    #else
        (void)motapp;
    #endif /* HAVE_MYSQL */

    indx = 0;
    while (motapp->cam_list[indx] != NULL) {
        myfree(&motapp->cam_list[indx]->dbse);
        indx++;
    }

}

void dbse_global_init(struct ctx_motapp *motapp)
{
    int indx;

    indx = 0;
    while (motapp->cam_list[indx] != NULL) {
        motapp->cam_list[indx]->dbse = (struct ctx_dbse *)mymalloc(sizeof(struct ctx_dbse));
        indx++;
    }

    if (motapp->cam_list[0]->conf->database_type != "") {
        if (dbse_global_edits(motapp->cam_list) == -1) {
            return;
        }

        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("Initializing database"));
        /* Initialize all the database items */
        #if defined(HAVE_MYSQL)
            if ((motapp->cam_list[0]->conf->database_type == "mysql")) {
                if (mysql_library_init(0, NULL, NULL)) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        ,_("Could not initialize database %s")
                        ,motapp->cam_list[0]->conf->database_type.c_str());
                    motapp->cam_list[0]->conf->database_type = "";
                    return;
                }
            }
        #endif /* HAVE_MYSQL */

        #if defined(HAVE_MARIADB)
            if ((motapp->cam_list[0]->conf->database_type == "mariadb")) {
                if (mysql_library_init(0, NULL, NULL)) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        ,_("Could not initialize database %s")
                        ,motapp->cam_list[0]->conf->database_type.c_str());
                    motapp->cam_list[0]->conf->database_type = "";
                    return;
                }
            }
        #endif /* HAVE_MARIADB */

        #ifdef HAVE_SQLITE3
            /* database_sqlite3 == NULL if not changed causes each thread to create their own
            * sqlite3 connection this will only happens when using a non-threaded sqlite version */
            motapp->cam_list[0]->dbse->database_sqlite3=NULL;
            if ((motapp->cam_list[0]->conf->database_type == "sqlite3") &&
                (motapp->cam_list[0]->conf->database_dbname != "")) {
                MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                    ,_("SQLite3 Database filename %s")
                    ,motapp->cam_list[0]->conf->database_dbname.c_str());

                int thread_safe = sqlite3_threadsafe();
                if (thread_safe > 0) {
                    MOTION_LOG(NTC, TYPE_DB, NO_ERRNO, _("SQLite3 is threadsafe"));
                    MOTION_LOG(NTC, TYPE_DB, NO_ERRNO, _("SQLite3 serialized %s")
                        ,(sqlite3_config(SQLITE_CONFIG_SERIALIZED)?_("FAILED"):_("SUCCESS")));
                    if (sqlite3_open(motapp->cam_list[0]->conf->database_dbname.c_str()
                        , &motapp->cam_list[0]->dbse->database_sqlite3) != SQLITE_OK) {
                        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                            ,_("Can't open database %s : %s")
                            ,motapp->cam_list[0]->conf->database_dbname.c_str()
                            ,sqlite3_errmsg(motapp->cam_list[0]->dbse->database_sqlite3));
                        sqlite3_close(motapp->cam_list[0]->dbse->database_sqlite3);
                        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                            ,_("Could not initialize database %s")
                            ,motapp->cam_list[0]->conf->database_dbname.c_str());
                        motapp->cam_list[0]->conf->database_type = "";
                        return;
                    }
                    MOTION_LOG(NTC, TYPE_DB, NO_ERRNO,_("database_busy_timeout %d msec"),
                            motapp->cam_list[0]->conf->database_busy_timeout);
                    if (sqlite3_busy_timeout(motapp->cam_list[0]->dbse->database_sqlite3
                        , motapp->cam_list[0]->conf->database_busy_timeout) != SQLITE_OK)
                        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO,_("database_busy_timeout failed %s")
                            ,sqlite3_errmsg(motapp->cam_list[0]->dbse->database_sqlite3));
                }
            }
            /* Cascade to all threads */
            indx = 1;
            while (motapp->cam_list[indx] != NULL) {
                motapp->cam_list[indx]->dbse->database_sqlite3 = motapp->cam_list[0]->dbse->database_sqlite3;
                indx++;
            }

        #endif /* HAVE_SQLITE3 */
    }
}

static void dbse_init_mysql(struct ctx_cam *cam)
{

    #if defined(HAVE_MYSQL)
        // close database to be sure that we are not leaking
        mysql_close(cam->dbse->database_mysql);
        cam->dbse->database_event_id = 0;

        cam->dbse->database_mysql = (MYSQL *) mymalloc(sizeof(MYSQL));
        mysql_init(cam->dbse->database_mysql);

        if (!mysql_real_connect(cam->dbse->database_mysql
            , cam->conf->database_host.c_str(), cam->conf->database_user.c_str()
            , cam->conf->database_password.c_str(), cam->conf->database_dbname.c_str()
            , cam->conf->database_port, NULL, 0)) {

            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Cannot connect to MySQL database %s on host %s with user %s")
                ,cam->conf->database_dbname.c_str(), cam->conf->database_host.c_str()
                ,cam->conf->database_user.c_str());
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("MySQL error was %s"), mysql_error(cam->dbse->database_mysql));
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Disabling database functionality"));
            dbse_global_deinit(cam->motapp);
            cam->conf->database_type = "";
            return;
        }
        #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
            bool my_true = true;
            mysql_options(cam->dbse->database_mysql, MYSQL_OPT_RECONNECT, &my_true);
        #endif
    #else
        (void)cam;  /* Avoid compiler warnings */
    #endif /* HAVE_MYSQL */

    return;

}

static void dbse_init_mariadb(struct ctx_cam *cam)
{

    #if defined(HAVE_MARIADB)
        // close database to be sure that we are not leaking
        mysql_close(cam->dbse->database_mariadb);
        cam->dbse->database_event_id = 0;

        cam->dbse->database_mariadb = (MYSQL *) mymalloc(sizeof(MYSQL));
        mysql_init(cam->dbse->database_mariadb);

        if (!mysql_real_connect(cam->dbse->database_mariadb
            , cam->conf->database_host.c_str(), cam->conf->database_user.c_str()
            , cam->conf->database_password.c_str(), cam->conf->database_dbname.c_str()
            , cam->conf->database_port, NULL, 0)) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Cannot connect to MySQL database %s on host %s with user %s")
                ,cam->conf->database_dbname.c_str(), cam->conf->database_host.c_str()
                ,cam->conf->database_user.c_str());
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("MySQL error was %s"), mysql_error(cam->dbse->database_mariadb));
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Disabling database functionality"));
            dbse_global_deinit(cam->motapp);
            cam->conf->database_type = "";
            return;
        }
        #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
            bool my_true = true;
            mysql_options(cam->dbse->database_mariadb, MYSQL_OPT_RECONNECT, &my_true);
        #endif
    #else
        (void)cam;  /* Avoid compiler warnings */
    #endif /* HAVE_MARIADB */

    return;

}

static void dbse_init_sqlite3(struct ctx_cam *cam)
{
    #ifdef HAVE_SQLITE3
        if (cam->motapp->cam_list[0]->dbse->database_sqlite3 != 0) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO,_("SQLite3 using shared handle"));
            cam->dbse->database_sqlite3 = cam->motapp->cam_list[0]->dbse->database_sqlite3;
        } else {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("SQLite3 Database filename %s"), cam->conf->database_dbname.c_str());
            if (sqlite3_open(cam->conf->database_dbname.c_str(), &cam->dbse->database_sqlite3) != SQLITE_OK) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Can't open database %s : %s")
                    ,cam->conf->database_dbname.c_str(), sqlite3_errmsg(cam->dbse->database_sqlite3));
                sqlite3_close(cam->dbse->database_sqlite3);
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Disabling database functionality"));
                cam->conf->database_type = "";
                return;
            }
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("database_busy_timeout %d msec"), cam->conf->database_busy_timeout);
            if (sqlite3_busy_timeout(cam->dbse->database_sqlite3, cam->conf->database_busy_timeout) != SQLITE_OK)
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("database_busy_timeout failed %s")
                    ,sqlite3_errmsg(cam->dbse->database_sqlite3));
        }
    #else
        (void)cam;  /* Avoid compiler warnings */
    #endif /* HAVE_SQLITE3 */

    return;

}

static void dbse_init_pgsql(struct ctx_cam *cam)
{
    #ifdef HAVE_PGSQL
        char connstring[255];
        /* Create the connection string.
         * Quote the values so we can have null values (blank)
        */
        snprintf(connstring, 255,
                    "dbname='%s' host='%s' user='%s' password='%s' port='%d'",
                    cam->conf->database_dbname.c_str(), /* dbname */
                    (cam->conf->database_host=="" ? cam->conf->database_host.c_str() : ""), /* host (may be blank) */
                    (cam->conf->database_user=="" ? cam->conf->database_user.c_str() : ""), /* user (may be blank) */
                    (cam->conf->database_password=="" ? cam->conf->database_password.c_str() : ""), /* password (may be blank) */
                    cam->conf->database_port
        );

        cam->dbse->database_pg = PQconnectdb(connstring);
        if (PQstatus(cam->dbse->database_pg) == CONNECTION_BAD) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Connection to PostgreSQL database '%s' failed: %s")
            ,cam->conf->database_dbname.c_str(), PQerrorMessage(cam->dbse->database_pg));
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Disabling database functionality"));
            cam->conf->database_type = "";
            return;
        }
    #else
        (void)cam;  /* Avoid compiler warnings */
    #endif /* HAVE_PGSQL */

    return;
}

void dbse_init(struct ctx_cam *cam)
{

    if (cam->conf->database_type != "") {
        MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
            ,_("Database backend %s"), cam->conf->database_type.c_str());
        if (cam->conf->database_type == "mysql") {
            dbse_init_mysql(cam);
        } else if (cam->conf->database_type == "mariadb") {
            dbse_init_mariadb(cam);
        } else if (cam->conf->database_type == "postgresql") {
            dbse_init_pgsql(cam);
        } else if (cam->conf->database_type == "sqlite3") {
            dbse_init_sqlite3(cam);
        } else {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
            ,_("Invalid Database backend %s")
            , cam->conf->database_type.c_str());
        }
    }

    return;
}

void dbse_deinit(struct ctx_cam *cam)
{
    if (cam->conf->database_type != "") {
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing database"));

        #if defined(HAVE_MYSQL)
            if (cam->conf->database_type == "mysql") {
                mysql_close(cam->dbse->database_mysql);
                cam->dbse->database_event_id = 0;
            }
        #endif /* HAVE_MYSQL */

        #if defined(HAVE_MARIADB)
            if (cam->conf->database_type == "mariadb") {
                mysql_close(cam->dbse->database_mariadb);
                cam->dbse->database_event_id = 0;
            }
        #endif /* HAVE_MYSQL */

        #ifdef HAVE_PGSQL
            if (cam->conf->database_type == "postgresql") {
                PQfinish(cam->dbse->database_pg);
            }
        #endif /* HAVE_PGSQL */

        #ifdef HAVE_SQLITE3
            /* Close the SQLite database */
            if (cam->conf->database_type == "sqlite3") {
                sqlite3_close(cam->dbse->database_sqlite3);
                cam->dbse->database_sqlite3 = NULL;
            }
        #endif /* HAVE_SQLITE3 */
    }

}

static void dbse_mysql_exec(char *sqlquery,struct ctx_cam *cam, int save_id)
{

    #if defined(HAVE_MYSQL)
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing mysql query");
        if (mysql_query(cam->dbse->database_mysql, sqlquery) != 0) {
            int error_code = mysql_errno(cam->dbse->database_mysql);

            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                ,_("Mysql query failed %s error code %d")
                ,mysql_error(cam->dbse->database_mysql), error_code);
            /* Try to reconnect ONCE if fails continue and discard this sql query */
            if (error_code >= 2000) {
                // Close connection before start a new connection
                mysql_close(cam->dbse->database_mysql);

                cam->dbse->database_mysql = (MYSQL *) mymalloc(sizeof(MYSQL));
                mysql_init(cam->dbse->database_mysql);

                if (!mysql_real_connect(cam->dbse->database_mysql, cam->conf->database_host.c_str(),
                                        cam->conf->database_user.c_str(), cam->conf->database_password.c_str(),
                                        cam->conf->database_dbname.c_str(), cam->conf->database_port, NULL, 0)) {
                    MOTION_LOG(ALR, TYPE_DB, NO_ERRNO
                        ,_("Cannot reconnect to MySQL"
                        " database %s on host %s with user %s MySQL error was %s"),
                        cam->conf->database_dbname.c_str(),
                        cam->conf->database_host.c_str(), cam->conf->database_user.c_str(),
                        mysql_error(cam->dbse->database_mysql));
                } else {
                    MOTION_LOG(INF, TYPE_DB, NO_ERRNO
                        ,_("Re-Connection to Mysql database '%s' Succeed")
                        ,cam->conf->database_dbname.c_str());
                    if (mysql_query(cam->dbse->database_mysql, sqlquery) != 0) {
                        int error_my = mysql_errno(cam->dbse->database_mysql);
                        MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                            ,_("after re-connection Mysql query failed %s error code %d")
                            ,mysql_error(cam->dbse->database_mysql), error_my);
                    }
                }
            }
        }
        if (save_id) {
            cam->dbse->database_event_id = (uint64_t) mysql_insert_id(cam->dbse->database_mysql);
        }
    #else
        (void)sqlquery;
        (void)cam;
        (void)save_id;
    #endif /* HAVE_MYSQL  HAVE_MARIADB*/

}

static void dbse_mariadb_exec(char *sqlquery,struct ctx_cam *cam, int save_id)
{

    #if defined(HAVE_MARIADB)
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing mysql query");
        if (mysql_query(cam->dbse->database_mariadb, sqlquery) != 0) {
            int error_code = mysql_errno(cam->dbse->database_mariadb);

            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                ,_("Mysql query failed %s error code %d")
                ,mysql_error(cam->dbse->database_mariadb), error_code);
            /* Try to reconnect ONCE if fails continue and discard this sql query */
            if (error_code >= 2000) {
                // Close connection before start a new connection
                mysql_close(cam->dbse->database_mariadb);

                cam->dbse->database_mariadb = (MYSQL *) mymalloc(sizeof(MYSQL));
                mysql_init(cam->dbse->database_mariadb);

                if (!mysql_real_connect(cam->dbse->database_mariadb, cam->conf->database_host.c_str(),
                                        cam->conf->database_user.c_str(), cam->conf->database_password.c_str(),
                                        cam->conf->database_dbname.c_str(),cam->conf->database_port, NULL, 0)) {
                    MOTION_LOG(ALR, TYPE_DB, NO_ERRNO
                        ,_("Cannot reconnect to MySQL"
                        " database %s on host %s with user %s MySQL error was %s"),
                        cam->conf->database_dbname.c_str(),
                        cam->conf->database_host.c_str(), cam->conf->database_user.c_str(),
                        mysql_error(cam->dbse->database_mariadb));
                } else {
                    MOTION_LOG(INF, TYPE_DB, NO_ERRNO
                        ,_("Re-Connection to Mysql database '%s' Succeed")
                        ,cam->conf->database_dbname.c_str());
                    if (mysql_query(cam->dbse->database_mariadb, sqlquery) != 0) {
                        int error_my = mysql_errno(cam->dbse->database_mariadb);
                        MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                            ,_("after re-connection Mysql query failed %s error code %d")
                            ,mysql_error(cam->dbse->database_mariadb), error_my);
                    }
                }
            }
        }
        if (save_id) {
            cam->dbse->database_event_id = (uint64_t) mysql_insert_id(cam->dbse->database_mariadb);
        }
    #else
        (void)sqlquery;
        (void)cam;
        (void)save_id;
    #endif /* HAVE_MARIADB*/

}

static void dbse_pgsql_exec(char *sqlquery,struct ctx_cam *cam, int save_id)
{
    #ifdef HAVE_PGSQL
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing postgresql query");
        PGresult *res;

        res = PQexec(cam->dbse->database_pg, sqlquery);

        if (PQstatus(cam->dbse->database_pg) == CONNECTION_BAD) {

            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Connection to PostgreSQL database '%s' failed: %s")
                ,cam->conf->database_dbname.c_str(), PQerrorMessage(cam->dbse->database_pg));

        // This function will close the connection to the server and attempt to reestablish a new connection to the same server,
        // using all the same parameters previously used. This may be useful for error recovery if a working connection is lost
            PQreset(cam->dbse->database_pg);

            if (PQstatus(cam->dbse->database_pg) == CONNECTION_BAD) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Re-Connection to PostgreSQL database '%s' failed: %s")
                    ,cam->conf->database_dbname.c_str(), PQerrorMessage(cam->dbse->database_pg));
            } else {
                MOTION_LOG(INF, TYPE_DB, NO_ERRNO
                    ,_("Re-Connection to PostgreSQL database '%s' Succeed")
                    ,cam->conf->database_dbname.c_str());
            }

        } else if (!(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK)) {
            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "PGSQL query failed: [%s]  %s %s",
                    sqlquery, PQresStatus(PQresultStatus(res)), PQresultErrorMessage(res));
        }
        if (save_id) {
            //ToDO:  Find the equivalent option for pgsql
            cam->dbse->database_event_id = 0;
        }

        PQclear(res);
    #else
        (void)sqlquery;
        (void)cam;
        (void)save_id;
    #endif /* HAVE_PGSQL */

}

static void dbse_sqlite3_exec(char *sqlquery,struct ctx_cam *cam, int save_id)
{
    #ifdef HAVE_SQLITE3
        int res;
        char *errmsg = 0;
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing sqlite query");
        res = sqlite3_exec(cam->dbse->database_sqlite3, sqlquery, NULL, 0, &errmsg);
        if (res != SQLITE_OK ) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, _("SQLite error was %s"), errmsg);
            sqlite3_free(errmsg);
        }
        if (save_id) {
            cam->dbse->database_event_id = 0;
        }
    #else
        (void)sqlquery;
        (void)cam;
        (void)save_id;
    #endif /* HAVE_SQLITE3 */
}

void dbse_exec(struct ctx_cam *cam, char *filename
    , int sqltype, struct timespec *ts1, const char *cmd)
{
    char sqlquery[PATH_MAX];

    if (cam->conf->database_type == "") {
        return;
    }

    if (mystreq(cmd,"pic_save")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_pic_save.c_str()
            , ts1, filename, sqltype);
    } else if (mystreq(cmd,"movie_start")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_movie_start.c_str()
            , ts1, filename, sqltype);
    } else if (mystreq(cmd,"movie_end")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_movie_end.c_str()
            , ts1, filename, sqltype);
    } else if (mystreq(cmd,"event_start")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_event_start.c_str()
            , ts1, filename, sqltype);
    } else if (mystreq(cmd,"event_end")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_event_end.c_str()
            , ts1, filename, sqltype);
    }

    if (strlen(sqlquery) <= 0) {
        MOTION_LOG(WRN, TYPE_DB, NO_ERRNO, "Ignoring empty sql query");
        return;
    }

    if (cam->conf->database_type == "mysql") {
        dbse_mysql_exec(sqlquery, cam, 0);
    } else if (cam->conf->database_type == "mariadb") {
        dbse_mariadb_exec(sqlquery, cam, 0);
    } else if (cam->conf->database_type == "postgresql") {
        dbse_pgsql_exec(sqlquery, cam, 0);
    } else if (cam->conf->database_type == "sqlite3") {
        dbse_sqlite3_exec(sqlquery, cam, 0);
    }

}

/* Execute query against the motionplus database */
void dbse_motpls_exec(const char *sqlquery, struct ctx_cam *cam)
{
    int retcd;
    char *errmsg = 0;

    if (cam->dbsemp == NULL) {
        return;
    }
    if (cam->dbsemp->database_sqlite3 == NULL) {
        return;
    }

    retcd = sqlite3_exec(cam->dbsemp->database_sqlite3, sqlquery, NULL, 0, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, _("SQLite error was %s"), errmsg);
        sqlite3_free(errmsg);
    }
}

/* sqlite query result call back */
static int dbse_motpls_cb(
    void *ptr, int arg_nb, char **arg_val, char **col_nm)
{
    ctx_cam *cam = (ctx_cam *)ptr;
    int indx;

    (void)col_nm;

    if (cam->dbsemp->dbse_action == DBSE_ACT_CHKTBL) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystrceq(arg_val[indx],"motionplus")) {
                cam->dbsemp->table_ok = true;
            }
        }
    } else if (cam->dbsemp->dbse_action == DBSE_ACT_GETCNT) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystrceq(col_nm[indx],"movie_cnt")) {
                cam->dbsemp->movie_cnt =atoi(arg_val[indx]);
            }
        }
    }

    return 0;
}

/* Process query records for movie list */
static int dbse_motpls_cb_movies(
    void *ptr, int arg_nb, char **arg_val, char **col_nm)
{
    ctx_cam *cam = (ctx_cam *)ptr;
    struct stat statbuf;
    int indx, rnbr, flen;

    if (cam->dbsemp->dbse_action == DBSE_ACT_GETTBL) {
        rnbr = cam->dbsemp->rec_indx;
        if (rnbr < cam->dbsemp->movie_cnt) {
            cam->dbsemp->movie_list[rnbr].found     = false;

            cam->dbsemp->movie_list[rnbr].rowid     = -1;
            cam->dbsemp->movie_list[rnbr].camid     = -1;
            cam->dbsemp->movie_list[rnbr].movie_nm  = NULL;
            cam->dbsemp->movie_list[rnbr].full_nm   = NULL;
            cam->dbsemp->movie_list[rnbr].movie_sz  = 0;
            cam->dbsemp->movie_list[rnbr].movie_dtl = 0;
            cam->dbsemp->movie_list[rnbr].movie_tmc = NULL;
            cam->dbsemp->movie_list[rnbr].movie_tmc = NULL;
            cam->dbsemp->movie_list[rnbr].diff_avg  = 0;
            cam->dbsemp->movie_list[rnbr].sdev_min  = 0;
            cam->dbsemp->movie_list[rnbr].sdev_max  = 0;
            cam->dbsemp->movie_list[rnbr].sdev_avg  = 0;

            for (indx=0; indx < arg_nb; indx++) {
                if (arg_val[indx] != NULL) {
                    if (mystreq(col_nm[indx],"rowid")) {
                        cam->dbsemp->movie_list[rnbr].rowid = atoi(arg_val[indx]);

                    } else if (mystreq(col_nm[indx],"camid")) {
                        cam->dbsemp->movie_list[rnbr].camid = atoi(arg_val[indx]);

                    } else if (mystreq(col_nm[indx],"movie_nm")) {
                        flen = strlen(arg_val[indx]);
                        cam->dbsemp->movie_list[rnbr].movie_nm =
                            (char*)mymalloc(flen + 1);
                        snprintf(cam->dbsemp->movie_list[rnbr].movie_nm
                            ,flen+1,"%s",arg_val[indx]);
                        flen += cam->conf->target_dir.length() + 2;
                        cam->dbsemp->movie_list[rnbr].full_nm =
                            (char*)mymalloc(flen + 1);
                        snprintf(cam->dbsemp->movie_list[rnbr].full_nm
                            , flen, "%s/%s", cam->conf->target_dir.c_str()
                            , arg_val[indx]);
                        if (stat(cam->dbsemp->movie_list[rnbr].full_nm,
                                &statbuf) == 0) {
                            cam->dbsemp->movie_list[rnbr].found = true;
                        }

                    } else if (mystreq(col_nm[indx],"movie_sz")) {
                        cam->dbsemp->movie_list[rnbr].movie_sz =atoi(arg_val[indx]);

                    } else if (mystreq(col_nm[indx],"movie_dtl")) {
                        cam->dbsemp->movie_list[rnbr].movie_dtl =atoi(arg_val[indx]);

                    } else if (mystreq(col_nm[indx],"movie_tmc")) {
                        flen = strlen(arg_val[indx]);
                        cam->dbsemp->movie_list[rnbr].movie_tmc =
                            (char*)mymalloc(flen + 1);
                        snprintf(cam->dbsemp->movie_list[rnbr].movie_tmc
                            ,flen+1,"%s",arg_val[indx]);

                    } else if (mystreq(col_nm[indx],"diff_avg")) {
                        cam->dbsemp->movie_list[rnbr].diff_avg =atoi(arg_val[indx]);
                    } else if (mystreq(col_nm[indx],"sdev_min")) {
                        cam->dbsemp->movie_list[rnbr].sdev_min =atoi(arg_val[indx]);
                    } else if (mystreq(col_nm[indx],"sdev_max")) {
                        cam->dbsemp->movie_list[rnbr].sdev_max =atoi(arg_val[indx]);
                    } else if (mystreq(col_nm[indx],"sdev_avg")) {
                        cam->dbsemp->movie_list[rnbr].sdev_avg =atoi(arg_val[indx]);

                    }
                }
            }
        }
        cam->dbsemp->rec_indx++;
    }

    return 0;
}

/* Process query list of columns in motionplus table*/
static int dbse_motpls_cb_cols(
    void *ptr, int arg_nb, char **arg_val, char **col_nm)
{
    ctx_cam *cam = (ctx_cam *)ptr;
    int indx, indx2;

    if (cam->dbsemp->dbse_action == DBSE_ACT_GETCOLS) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystreq(col_nm[indx],"col_nm")) {
                for (indx2=0; indx2 < cam->dbsemp->cols_cnt; indx2++) {
                    if (mystreq(arg_val[indx]
                        , cam->dbsemp->cols_list[indx2].col_nm)) {
                        cam->dbsemp->cols_list[indx2].found = true;
                    }
                }
            }
        }
    }

    return 0;
}

/* Free the movies lists*/
static void dbse_motpls_free_movies(struct ctx_cam *cam)
{
    int indx;

    if (cam->dbsemp == NULL) {
        return;
    }

    if (cam->dbsemp->movie_list != NULL) {
        for (indx=0; indx<cam->dbsemp->movie_cnt; indx++) {
            myfree(&cam->dbsemp->movie_list[indx].movie_nm);
            myfree(&cam->dbsemp->movie_list[indx].full_nm);
            myfree(&cam->dbsemp->movie_list[indx].movie_tmc);
        }
        myfree(&cam->dbsemp->movie_list);
    }
    cam->dbsemp->movie_cnt = 0;

}

/* Free the cols lists*/
static void dbse_motpls_free_cols(struct ctx_cam *cam)
{
    int indx;

    if (cam->dbsemp == NULL) {
        return;
    }

    if (cam->dbsemp->cols_list != NULL) {
        for (indx=0; indx<cam->dbsemp->cols_cnt; indx++) {
            myfree(&cam->dbsemp->cols_list[indx].col_nm);
            myfree(&cam->dbsemp->cols_list[indx].col_typ);
        }
        myfree(&cam->dbsemp->cols_list);
    }
    cam->dbsemp->cols_cnt = 0;

}

/* Create array of all the columns in current version */
static void dbse_motpls_col_list(struct ctx_cam *cam)
{
    int indx;

    dbse_motpls_free_cols(cam);

    cam->dbsemp->cols_cnt = 9;

    cam->dbsemp->cols_list =(ctx_dbsemp_col *)
        mymalloc(sizeof(ctx_dbsemp_col) * cam->dbsemp->cols_cnt);

    /* The size of 30 is arbitrary */
    for (indx=0; indx<cam->dbsemp->cols_cnt; indx++) {
        cam->dbsemp->cols_list[indx].col_nm = (char*)mymalloc(30);
        cam->dbsemp->cols_list[indx].col_typ = (char*)mymalloc(30);
        cam->dbsemp->cols_list[indx].found = false;
    }

    snprintf(cam->dbsemp->cols_list[0].col_nm,  30, "%s", "camid");
    snprintf(cam->dbsemp->cols_list[0].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[1].col_nm,  30, "%s", "movie_nm");
    snprintf(cam->dbsemp->cols_list[1].col_typ, 30, "%s", "text");

    snprintf(cam->dbsemp->cols_list[2].col_nm,  30, "%s", "movie_sz");
    snprintf(cam->dbsemp->cols_list[2].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[3].col_nm,  30, "%s", "movie_dtl");
    snprintf(cam->dbsemp->cols_list[3].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[4].col_nm,  30, "%s", "movie_tmc");
    snprintf(cam->dbsemp->cols_list[4].col_typ, 30, "%s", "text");

    snprintf(cam->dbsemp->cols_list[5].col_nm,  30, "%s", "diff_avg");
    snprintf(cam->dbsemp->cols_list[5].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[6].col_nm,  30, "%s", "sdev_min");
    snprintf(cam->dbsemp->cols_list[6].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[7].col_nm,  30, "%s", "sdev_max");
    snprintf(cam->dbsemp->cols_list[7].col_typ, 30, "%s", "int");

    snprintf(cam->dbsemp->cols_list[8].col_nm,  30, "%s", "sdev_avg");
    snprintf(cam->dbsemp->cols_list[8].col_typ, 30, "%s", "int");

}

/* Validate table has our cols.  If not add the cols */
static int dbse_motpls_validate_cols(struct ctx_cam *cam)
{
    int retcd, indx;
    char *errmsg = 0;
    std::string sqlquery;

    dbse_motpls_col_list(cam);

    sqlquery =
        " select name as col_nm "
        " from pragma_table_info('motionplus');";
    cam->dbsemp->dbse_action = DBSE_ACT_GETCOLS;
    retcd = sqlite3_exec(cam->dbsemp->database_sqlite3
        , sqlquery.c_str(), dbse_motpls_cb_cols, cam, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error retrieving table columns: %s"), errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    for (indx=0; indx<cam->dbsemp->cols_cnt; indx++) {
        if (cam->dbsemp->cols_list[indx].found == false) {
            sqlquery = "Alter table motionplus add column ";
            sqlquery += std::string(cam->dbsemp->cols_list[indx].col_nm) + " ";
            sqlquery += std::string(cam->dbsemp->cols_list[indx].col_typ) + " ;";
            dbse_motpls_exec(sqlquery.c_str(), cam);
        }
    }

    dbse_motpls_free_cols(cam);

    return 0;
}

/* Validate database has our required table, if not create it */
static int dbse_motpls_validate(struct ctx_cam *cam)
{
    int retcd;
    char *errmsg = 0;
    std::string sqlquery;

    sqlquery =
        "select name from sqlite_master"
        " where type='table' "
        " and name='motionplus';";
    cam->dbsemp->table_ok = false;
    cam->dbsemp->dbse_action = DBSE_ACT_CHKTBL;
    retcd = sqlite3_exec(cam->dbsemp->database_sqlite3
        , sqlquery.c_str(), dbse_motpls_cb, cam, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error checking table: %s"), errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    if (cam->dbsemp->table_ok == false) {
        sqlquery =
            "create table motionplus ("
            " camid     int"
            ");";
        retcd = sqlite3_exec(cam->dbsemp->database_sqlite3
            , sqlquery.c_str(), 0, 0, &errmsg);
        if (retcd != SQLITE_OK ) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Error creating table: %s"), errmsg);
            sqlite3_free(errmsg);
            return -1;
        }
    }

    retcd = dbse_motpls_validate_cols(cam);

    return retcd;

}

/* Open and validate the motionplus database*/
void dbse_motpls_init(struct ctx_cam *cam)
{
    std::string dbname;

    cam->dbsemp = new ctx_dbsemp;
    cam->dbsemp->movie_cnt = 0;
    cam->dbsemp->movie_list = NULL;
    cam->dbsemp->cols_cnt = 0;
    cam->dbsemp->cols_list = NULL;

    dbname = cam->conf->target_dir + "/dbcam" +
        std::to_string(cam->camera_id)+".db";

    MOTION_LOG(NTC, TYPE_DB, NO_ERRNO, "%s", dbname.c_str());

    if (sqlite3_open(dbname.c_str(), &cam->dbsemp->database_sqlite3) != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Can't open database %s : %s")
            ,dbname.c_str(), sqlite3_errmsg(cam->dbsemp->database_sqlite3));
        sqlite3_close(cam->dbsemp->database_sqlite3);
        cam->dbsemp->database_sqlite3 = NULL;
        return;
    }
    if (sqlite3_busy_timeout(cam->dbsemp->database_sqlite3, 1000) != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO,_("database_busy_timeout failed %s")
            ,sqlite3_errmsg(cam->dbsemp->database_sqlite3));
    }
    if (dbse_motpls_validate(cam) != 0) {
        sqlite3_close(cam->dbsemp->database_sqlite3);
        cam->dbsemp->database_sqlite3 = NULL;
        return;
    };

    return;

}

/* Close the motionplus database */
void dbse_motpls_deinit(struct ctx_cam *cam)
{
    if (cam->dbsemp != NULL) {
        if (cam->dbsemp->database_sqlite3 != NULL) {
            sqlite3_close(cam->dbsemp->database_sqlite3);
        }
        cam->dbsemp->database_sqlite3 = NULL;
        dbse_motpls_free_movies(cam);
        dbse_motpls_free_cols(cam);
        delete cam->dbsemp;
    }
}

/* Populate the list of the movies from the database*/
int dbse_motpls_getlist(struct ctx_cam *cam)
{
    int retcd, indx;
    char *errmsg = 0;
    std::string sqlquery;

     if (cam->dbsemp == NULL) {
        return -1;
    }

    dbse_motpls_free_movies(cam);

    if (cam->dbsemp->database_sqlite3 == NULL) {
        return -1;
    }

    sqlquery =
        "select count(*) as movie_cnt "
        " from motionplus "
        " where camid = " + std::to_string(cam->camera_id) + ";";
    cam->dbsemp->dbse_action = DBSE_ACT_GETCNT;
    retcd = sqlite3_exec(cam->dbsemp->database_sqlite3
        , sqlquery.c_str(), dbse_motpls_cb, cam, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error counting table: %s"), errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    if (cam->dbsemp->movie_cnt > 0) {
        cam->dbsemp->movie_list =(ctx_dbsemp_rec *)
            mymalloc(sizeof(ctx_dbsemp_rec)*cam->dbsemp->movie_cnt);
        cam->dbsemp->rec_indx = 0;
        sqlquery =
            " select rowid, * from motionplus "
            " where camid = " + std::to_string(cam->camera_id) + ";";
        cam->dbsemp->dbse_action = DBSE_ACT_GETTBL;
        retcd = sqlite3_exec(cam->dbsemp->database_sqlite3
            , sqlquery.c_str(), dbse_motpls_cb_movies, cam, &errmsg);
        if (retcd != SQLITE_OK ) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Error retrieving table: %s"), errmsg);
            sqlite3_free(errmsg);
            return -1;
        }
        /* Clean the database of files that were removed*/
        for (indx=0; indx<cam->dbsemp->movie_cnt; indx++) {
            if (cam->dbsemp->movie_list[indx].found == false) {
                sqlquery =
                    " delete from motionplus "
                    " where camid = " + std::to_string(cam->camera_id) +
                    " and rowid = " + std::to_string(
                    cam->dbsemp->movie_list[indx].rowid)+";";
                dbse_motpls_exec(sqlquery.c_str(),cam);
            }
            /*since we run vacuum after this, the rowids will no longer be valid*/
            cam->dbsemp->movie_list[indx].rowid = -1;
        }
        sqlquery ="vacuum";
        dbse_motpls_exec(sqlquery.c_str(),cam);
    }

    return 0;
}

/* Add a record for new movies */
void dbse_motpls_addrec(struct ctx_cam *cam,char *fname, struct timespec *ts1)
{
    std::string sqlquery;
    struct stat statbuf;
    char *errmsg = 0;
    int retcd, dirlen;
    int64_t bsz;
    char dtl[12];
    char tmc[12];
    uint64_t diff_avg, sdev_avg;

    struct tm timestamp_tm;

    if (cam->dbsemp == NULL) {
        return;
    }
    if (cam->dbsemp->database_sqlite3 == NULL) {
        return;
    }

    /* Movie file times */
    if (stat(fname, &statbuf) == 0) {
        bsz = statbuf.st_size;
    } else {
        bsz = 0;
    }
    localtime_r(&ts1->tv_sec, &timestamp_tm);
    strftime(dtl, 11, "%G%m%d", &timestamp_tm);
    strftime(tmc, 11, "%I:%M%p", &timestamp_tm);

    /* Add 1 for last slash */
    dirlen = cam->conf->target_dir.length() + 1;
    if (dirlen >= (int)strlen(fname)) {
        return;
    }

    if (cam->info_diff_cnt != 0) {
        diff_avg = (cam->info_diff_tot / cam->info_diff_cnt);
    } else {
        diff_avg =0;
    }
    if (cam->info_diff_cnt != 0) {
        sdev_avg = (cam->info_sdev_tot / cam->info_diff_cnt);
    } else {
        sdev_avg =0;
    }

    sqlquery =  "insert into motionplus ";
    sqlquery += " (camid, movie_nm, movie_sz, movie_dtl, movie_tmc ";
    sqlquery += " , diff_avg, sdev_min, sdev_max, sdev_avg)";
    sqlquery += " values ("+std::to_string(cam->camera_id);
    sqlquery += " ,'" + std::string(fname + dirlen) + "'";
    sqlquery += " ,"  + std::to_string(bsz);
    sqlquery += " ,"  + std::string(dtl);
    sqlquery += " ,'" + std::string(tmc)+ "'";
    sqlquery += " ,"  + std::to_string(diff_avg);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_min);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_max);
    sqlquery += " ,"  + std::to_string(sdev_avg);
    sqlquery += ")";

    retcd = sqlite3_exec(cam->dbsemp->database_sqlite3, sqlquery.c_str(), NULL, 0, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, _("SQLite error %d was %s"),retcd, errmsg);
        dbse_motpls_deinit(cam);
        dbse_motpls_init(cam);
        retcd = sqlite3_exec(cam->dbsemp->database_sqlite3, sqlquery.c_str(), NULL, 0, &errmsg);
        if (retcd != SQLITE_OK ) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, _("Serious error %d was %s"),retcd, errmsg);
            dbse_motpls_deinit(cam);
        }
    }
}
