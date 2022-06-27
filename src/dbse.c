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

/**
 *      dbse.c
 *
 *      Module of routines associated with database processing.
 *
 */

#include "translate.h"
#include "motion.h"
#include "util.h"
#include "logger.h"
#include "dbse.h"

pthread_mutex_t dbse_lock;

/** dbse_global_deinit */
void dbse_global_deinit(struct context **cntlist)
{
    struct context *cnt;
    int i;

    pthread_mutex_destroy(&dbse_lock);

    #if defined(HAVE_MYSQL)
        for (cnt=cntlist[i=0]; cnt; cnt=cntlist[++i]) {
            if (mystreq(cnt->conf.database_type, "mysql")) {
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing MySQL library"));
                mysql_library_end();
                break;
            }
        }
    #endif /* HAVE_MYSQL */

    #if defined(HAVE_MARIADB)
        for (cnt=cntlist[i=0]; cnt; cnt=cntlist[++i]) {
            if (mystreq(cnt->conf.database_type, "mariadb")) {
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing MariaDB library"));
                mysql_library_end();
                break;
            }
        }
    #endif /* HAVE_MARIADB */

    for (cnt=cntlist[i=0]; cnt; cnt=cntlist[++i]) {
        if (cnt->conf.database_type) {
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Database closed"));
            break;
        }
    }
}

/** dbse_global_init */
void dbse_global_init(struct context **cntlist)
{
    int indx;

    MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("Initializing database"));

    pthread_mutex_init(&dbse_lock, NULL);

    /* Initialize all the database items; different camera threads can use different DBMSs */
    #if defined(HAVE_MYSQL)
        indx = 0;
        while (cntlist[indx] != NULL) {
            if (mystreq(cntlist[indx]->conf.database_type, "mysql") && mysql_library_init(0, NULL, NULL)) {
                fprintf(stderr, "Could not initialize MySQL library\n");
                exit(1);
            } else {
                break;
            }
            indx++;
        }
    #endif /* HAVE_MYSQL */

    #if defined(HAVE_MARIADB)
        indx = 0;
        while (cntlist[indx] != NULL) {
            if (mystreq(cntlist[indx]->conf.database_type, "mariadb") && mysql_library_init(0, NULL, NULL)) {
                fprintf(stderr, "Could not initialize MariaDB library\n");
                exit(1);
            } else {
                break;
            }
            indx++;
        }
    #endif /* HAVE_MARIADB */

    #if defined(HAVE_SQLITE3)
        int retcd;
        cntlist[0]->database_sqlite3 = NULL;
        if ((cntlist[0]->conf.database_type != NULL) &&
            (mystreq(cntlist[0]->conf.database_type, "sqlite3")) &&
            (cntlist[0]->conf.database_dbname != NULL)) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                , _("SQLite3 Database filename %s")
                , cntlist[0]->conf.database_dbname);

            retcd = sqlite3_threadsafe();
            if (retcd != 0) {
                MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, _("SQLite3 is threadsafe"));
                retcd = sqlite3_open( cntlist[0]->conf.database_dbname
                    , &cntlist[0]->database_sqlite3);
                if (retcd != SQLITE_OK) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        , _("Can't open SQLite3 database %s. Error %d:%s")
                        , cntlist[0]->conf.database_dbname, retcd
                        , sqlite3_errmsg( cntlist[0]->database_sqlite3));
                    sqlite3_close( cntlist[0]->database_sqlite3);
                    exit(1);
                }
                MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                    , _("Setting busy timeout to %d msec")
                    , cntlist[0]->conf.database_busy_timeout);
                retcd = sqlite3_busy_timeout(cntlist[0]->database_sqlite3
                    , cntlist[0]->conf.database_busy_timeout);
                if (retcd != SQLITE_OK) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        , _("Setting busy timeout failed. Error %d:%s")
                        , retcd, sqlite3_errmsg( cntlist[0]->database_sqlite3));
                }
            }
        }
        /* Cascade to all threads */
        indx = 1;
        while (cntlist[indx] != NULL) {
            cntlist[indx]->database_sqlite3 = cntlist[0]->database_sqlite3;
            indx++;
        }
    #endif /* HAVE_SQLITE3 */

    #if !defined(HAVE_MYSQL) && !defined(HAVE_MARIADB) && !defined(HAVE_SQLITE3)
        (void)indx;
        (void)cntlist;
    #endif

}

/**
 * dbse_init_mysql
 *
 */
static int dbse_init_mysql(struct context *cnt)
{
    #if defined(HAVE_MYSQL)
        int dbport;
        if ((mystreq(cnt->conf.database_type, "mysql")) && (cnt->conf.database_dbname)) {
            cnt->database_mysql = mymalloc(sizeof(MYSQL));
            mysql_init(cnt->database_mysql);
            if ((cnt->conf.database_port < 0) || (cnt->conf.database_port > 65535)) {
                dbport = 0;
            } else {
                dbport = cnt->conf.database_port;
            }
            if (!mysql_real_connect(cnt->database_mysql, cnt->conf.database_host, cnt->conf.database_user,
                cnt->conf.database_password, cnt->conf.database_dbname, dbport, NULL, 0)) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Cannot connect to MySQL database %s on host %s with user %s")
                    ,cnt->conf.database_dbname, cnt->conf.database_host
                    ,cnt->conf.database_user);
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("MySQL error was %s"), mysql_error(cnt->database_mysql));
                return -2;
            }
            #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
                int my_true = TRUE;
                mysql_options(cnt->database_mysql, MYSQL_OPT_RECONNECT, &my_true);
            #endif
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_MYSQL */

    return 0;

}

/**
 * dbse_init_mariadb
 *
 */
static int dbse_init_mariadb(struct context *cnt)
{
    #if defined(HAVE_MARIADB)
        int dbport;
        if ((mystreq(cnt->conf.database_type, "mariadb")) && (cnt->conf.database_dbname)) {
            cnt->database_mariadb = mymalloc(sizeof(MYSQL));
            mysql_init(cnt->database_mariadb);
            if ((cnt->conf.database_port < 0) || (cnt->conf.database_port > 65535)) {
                dbport = 0;
            } else {
                dbport = cnt->conf.database_port;
            }
            if (!mysql_real_connect(cnt->database_mariadb, cnt->conf.database_host, cnt->conf.database_user,
                cnt->conf.database_password, cnt->conf.database_dbname, dbport, NULL, 0)) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Cannot connect to MariaDB database %s on host %s with user %s")
                    ,cnt->conf.database_dbname, cnt->conf.database_host
                    ,cnt->conf.database_user);
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("MariaDB error was %s"), mysql_error(cnt->database_mariadb));
                return -2;
            }
            #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
                int my_true = TRUE;
                mysql_options(cnt->database_mariadb, MYSQL_OPT_RECONNECT, &my_true);
            #endif
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_MARIADB */

    return 0;

}

/** dbse_init_sqlite3 */
static int dbse_init_sqlite3(struct context *cnt,struct context **cntlist)
{
    #ifdef HAVE_SQLITE3
        int retcd;
        if (cntlist[0]->database_sqlite3 != NULL) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                , _("Using common database %s")
                , cntlist[0]->conf.database_dbname);
            cnt->database_sqlite3 = cntlist[0]->database_sqlite3;
        } else if ((mystreq(cnt->conf.database_type, "sqlite3")) &&
            (cnt->conf.database_dbname != NULL)) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("Opening database %s"), cnt->conf.database_dbname);
            retcd = sqlite3_open(cnt->conf.database_dbname, &cnt->database_sqlite3);
            if (retcd != SQLITE_OK) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Can't open SQLite3 database %s.  Error %d : %s")
                    , cnt->conf.database_dbname, retcd
                    , sqlite3_errmsg(cnt->database_sqlite3));
                sqlite3_close(cnt->database_sqlite3);
                return -2;
            }
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                , _("Setting busy timeout to %d msec")
                , cnt->conf.database_busy_timeout);
            retcd = sqlite3_busy_timeout(cnt->database_sqlite3
                , cnt->conf.database_busy_timeout);
            if (retcd != SQLITE_OK) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    , _("Setting busy timeout failed.  Error %d:%s")
                    , retcd, sqlite3_errmsg(cnt->database_sqlite3));
            }
        }
    #else
        (void)cnt;
        (void)cntlist;
    #endif /* HAVE_SQLITE3 */

    return 0;

}

/**
 * dbse_init_pgsql
 *
 */
static int dbse_init_pgsql(struct context *cnt)
{
    #ifdef HAVE_PGSQL
        if ((mystreq(cnt->conf.database_type, "postgresql")) && (cnt->conf.database_dbname)) {
            char connstring[255];
            snprintf(connstring, 255,
                     "dbname='%s' host='%s' user='%s' password='%s' port='%d'",
                      cnt->conf.database_dbname, /* dbname */
                      (cnt->conf.database_host ? cnt->conf.database_host : ""), /* host (may be blank) */
                      (cnt->conf.database_user ? cnt->conf.database_user : ""), /* user (may be blank) */
                      (cnt->conf.database_password ? cnt->conf.database_password : ""), /* password (may be blank) */
                      cnt->conf.database_port
            );

            cnt->database_pgsql = PQconnectdb(connstring);
            if (PQstatus(cnt->database_pgsql) == CONNECTION_BAD) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Connection to PostgreSQL database '%s' failed: %s")
                    ,cnt->conf.database_dbname, PQerrorMessage(cnt->database_pgsql));
                return -2;
            }
            cnt->eid_db_format = dbeid_undetermined;
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_PGSQL */

    return 0;
}

/** dbse_init */
int dbse_init(struct context *cnt, struct context **cntlist)
{
    int retcd = 0;

    if (cnt->conf.database_type) {
        MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
            ,_("Database type %s"), cnt->conf.database_type);

        if (mystreq(cnt->conf.database_type,"mysql")) {
            retcd = dbse_init_mysql(cnt);
        } else if (mystreq(cnt->conf.database_type,"mariadb")) {
            retcd = dbse_init_mariadb(cnt);
        } else if (mystreq(cnt->conf.database_type,"postgresql")) {
            retcd = dbse_init_pgsql(cnt);
        } else if (mystreq(cnt->conf.database_type,"sqlite3")) {
            retcd = dbse_init_sqlite3(cnt, cntlist);
        }

        /* Set the sql mask file according to the SQL config options*/
        cnt->sql_mask = cnt->conf.sql_log_picture * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                        cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                        cnt->conf.sql_log_movie * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                        cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
    }

    return retcd;
}

/**
 * dbse_deinit
 *
 */
void dbse_deinit(struct context *cnt)
{
    if (cnt->conf.database_type) {
        #if defined(HAVE_MYSQL)
            if ( (mystreq(cnt->conf.database_type, "mysql")) && (cnt->conf.database_dbname)) {
                mysql_thread_end();
                mysql_close(cnt->database_mysql);
            }
        #endif /* HAVE_MYSQL */

        #if defined(HAVE_MARIADB)
            if ( (mystreq(cnt->conf.database_type, "mariadb")) && (cnt->conf.database_dbname)) {
                mysql_thread_end();
                mysql_close(cnt->database_mariadb);
            }
        #endif /* HAVE_MARIADB */

        #ifdef HAVE_PGSQL
                if ((mystreq(cnt->conf.database_type, "postgresql")) && (cnt->conf.database_dbname)) {
                    PQfinish(cnt->database_pgsql);
                    cnt->database_pgsql = NULL;
                }
        #endif /* HAVE_PGSQL */

        #ifdef HAVE_SQLITE3
            if (cnt->database_sqlite3 != NULL) {
                sqlite3_close(cnt->database_sqlite3);
                cnt->database_sqlite3 = NULL;
            }
        #endif /* HAVE_SQLITE3 */
        (void)cnt;
    }
}

/**
 * dbse_sqlmask_update
 *
 */
void dbse_sqlmask_update(struct context *cnt)
{
    /*
    * Set the sql mask file according to the SQL config options
    * We update it for every frame in case the config was updated
    * via remote control.
    */
    cnt->sql_mask = cnt->conf.sql_log_picture * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                    cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                    cnt->conf.sql_log_movie * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                    cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
}

/**
 * dbse_exec_mysql
 *
 */
static void dbse_exec_mysql(char *sqlquery, struct context *cnt)
{
    #if defined(HAVE_MYSQL)
        if (mystreq(cnt->conf.database_type, "mysql")) {
            MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, _("Executing MySQL query"));
            if (mysql_query(cnt->database_mysql, sqlquery) != 0) {
                int error_code = mysql_errno(cnt->database_mysql);

                MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                    ,_("MySQL query failed %s error code %d")
                    ,mysql_error(cnt->database_mysql), error_code);
                /* Try to reconnect ONCE if fails continue and discard this sql query */
                if (error_code >= 2000) {
                    // Close connection before start a new connection
                    mysql_close(cnt->database_mysql);

                    cnt->database_mysql = (MYSQL *) mymalloc(sizeof(MYSQL));
                    mysql_init(cnt->database_mysql);

                    if (!mysql_real_connect(cnt->database_mysql, cnt->conf.database_host,
                                            cnt->conf.database_user, cnt->conf.database_password,
                                            cnt->conf.database_dbname, 0, NULL, 0)) {
                        MOTION_LOG(ALR, TYPE_DB, NO_ERRNO
                            ,_("Cannot reconnect to MySQL"
                            " database %s on host %s with user %s MySQL error was %s"),
                            cnt->conf.database_dbname,
                            cnt->conf.database_host, cnt->conf.database_user,
                            mysql_error(cnt->database_mysql));
                    } else {
                        MOTION_LOG(INF, TYPE_DB, NO_ERRNO
                            ,_("Re-Connection to MySQL database '%s' Succeed")
                            ,cnt->conf.database_dbname);
                        if (mysql_query(cnt->database_mysql, sqlquery) != 0) {
                            int error_my = mysql_errno(cnt->database_mysql);
                            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                                ,_("after re-connection MySQL query failed %s error code %d")
                                ,mysql_error(cnt->database_mysql), error_my);
                        }
                    }
                }
            }
        }
    #else
        (void)sqlquery;
        (void)cnt;
    #endif /* HAVE_MYSQL*/
}

/**
 * dbse_exec_mariadb
 *
 */
static void dbse_exec_mariadb(char *sqlquery, struct context *cnt)
{
    #if defined(HAVE_MARIADB)
        if (mystreq(cnt->conf.database_type, "mariadb")) {
            MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, _("Executing MariaDB query"));
            if (mysql_query(cnt->database_mariadb, sqlquery) != 0) {
                int error_code = mysql_errno(cnt->database_mariadb);

                MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                    ,_("MariaDB query failed %s error code %d")
                    ,mysql_error(cnt->database_mariadb), error_code);
                /* Try to reconnect ONCE if fails continue and discard this sql query */
                if (error_code >= 2000) {
                    // Close connection before start a new connection
                    mysql_close(cnt->database_mariadb);

                    cnt->database_mariadb = (MYSQL *) mymalloc(sizeof(MYSQL));
                    mysql_init(cnt->database_mariadb);

                    if (!mysql_real_connect(cnt->database_mariadb, cnt->conf.database_host,
                                            cnt->conf.database_user, cnt->conf.database_password,
                                            cnt->conf.database_dbname, 0, NULL, 0)) {
                        MOTION_LOG(ALR, TYPE_DB, NO_ERRNO
                            ,_("Cannot reconnect to MariaDB"
                            " database %s on host %s with user %s MariaDB error was %s"),
                            cnt->conf.database_dbname,
                            cnt->conf.database_host, cnt->conf.database_user,
                            mysql_error(cnt->database_mariadb));
                    } else {
                        MOTION_LOG(INF, TYPE_DB, NO_ERRNO
                            ,_("Re-Connection to MariaDB database '%s' Succeed")
                            ,cnt->conf.database_dbname);
                        if (mysql_query(cnt->database_mariadb, sqlquery) != 0) {
                            int error_my = mysql_errno(cnt->database_mariadb);
                            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO
                                ,_("after re-connection MariaDB query failed %s error code %d")
                                ,mysql_error(cnt->database_mariadb), error_my);
                        }
                    }
                }
            }
        }
    #else
        (void)sqlquery;
        (void)cnt;
    #endif /* HAVE_MYSQL HAVE_MARIADB*/

}

/**
 * dbse_exec_pgsql
 *
 */
static void dbse_exec_pgsql(char *sqlquery, struct context *cnt)
{
    #ifdef HAVE_PGSQL
        if (mystreq(cnt->conf.database_type, "postgresql") && cnt->database_pgsql) {
            PGresult *res;
            ExecStatusType estat;
            PostgresPollingStatusType pstat;

            if (cnt->eid_db_format == dbeid_recovery) {
                /* lost DB session recovery pending */
                pstat = PQresetPoll(cnt->database_pgsql);
                if (pstat == PGRES_POLLING_OK) {
                    MOTION_LOG(WRN, TYPE_DB, NO_ERRNO
                        ,_("Re-connected to PostgreSQL database '%s'")
                        ,cnt->conf.database_dbname);
                    cnt->eid_db_format = dbeid_undetermined;  /* reinit eid logic */
                } else if (pstat == PGRES_POLLING_FAILED) {
                    cnt->eid_db_format = dbeid_rec_fail;  /* retry PGresetStart() */
                } else {  /* session recovery in process but not complete */
                    return;  /* discard this sqlquery; check again on next one */
                }
            }

            /* attempt sqlquery unless previous DB session break recovery failed */
            if (cnt->eid_db_format > dbeid_recovery) {
              MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, _("Executing PostgreSQL query"));
              res  = PQexec(cnt->database_pgsql, sqlquery);
              estat = PQresultStatus(res);
            } else {
              res = NULL;  /* skip PQclear() below */
              estat = PGRES_NONFATAL_ERROR;
            }

            if (PQstatus(cnt->database_pgsql) != PGSQL_CONNECTION_OK) {  /* DB session break! */
                /* Non-blocking broken session recovery (PGSQL Doc.sec.32.1; PGSQL>v9, 33.1)
                 * Note: to avoid blocking, each sqlquery during a session break is discarded. */
                if (cnt->eid_db_format != dbeid_rec_fail) {  /* log only first attempt */
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        ,_("Attempting reconnect to PostgreSQL database '%s': %s")
                        ,cnt->conf.database_dbname, PQerrorMessage(cnt->database_pgsql));
                }
                if (PQresetStart(cnt->database_pgsql)) {  /* reset request delivered */
                    cnt->eid_db_format = dbeid_recovery;  /* suspend SQL operations */
                } else {  /* reset request fails if PGSQL server is (temporarily?) unreachable */
                    cnt->eid_db_format = dbeid_rec_fail;  /* try again next sqlquery */
                }
            } else if (!(estat == PGRES_COMMAND_OK || estat == PGRES_TUPLES_OK)) {
                MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, _("PGSQL query failed: [%s]  %s %s"),
                    sqlquery, PQresStatus(PQresultStatus(res)), PQresultErrorMessage(res));
            }
            if (res) {
                PQclear(res);
            }
        }
    #else
        (void)sqlquery;
        (void)cnt;
    #endif /* HAVE_PGSQL */

}

/** dbse_exec_sqlite3 */
static void dbse_exec_sqlite3(char *sqlquery, struct context *cnt)
{
    #ifdef HAVE_SQLITE3
        if (cnt->database_sqlite3 != NULL) {
            int retcd;
            char *errmsg = NULL;
            MOTION_LOG(DBG, TYPE_DB, NO_ERRNO, _("Executing %s"), sqlquery);
            retcd = sqlite3_exec(cnt->database_sqlite3, sqlquery, NULL, 0, &errmsg);
            if (retcd != SQLITE_OK ) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    , _("SQLite3 error %d : %s"), retcd, errmsg);
                sqlite3_free(errmsg);
            }
        }
    #else
        (void)sqlquery;
        (void)cnt;
    #endif /* HAVE_SQLITE3 */

}

/** dbse_firstmotion */
void dbse_firstmotion(struct context *cnt)
{
    char sqlquery[PATH_MAX];

    mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query_start,
                   &cnt->current_image->timestamp_tv, NULL, 0);

    if (strlen(sqlquery) <= 0) {
        MOTION_LOG(WRN, TYPE_DB, NO_ERRNO, "Ignoring empty sql query");
        return;
    }

    pthread_mutex_lock(&dbse_lock);
        if (mystreq(cnt->conf.database_type,"mysql")) {
            dbse_exec_mysql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"mariadb")) {
            dbse_exec_mariadb(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"postgresql")) {
            dbse_exec_pgsql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"sqlite3")) {
            dbse_exec_sqlite3(sqlquery, cnt);
        }
    pthread_mutex_unlock(&dbse_lock);

}

/** dbse_newfile */
void dbse_newfile(struct context *cnt, char *filename, int sqltype, struct timeval *tv1)
{
    char sqlquery[PATH_MAX];

    mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query,
                   tv1, filename, sqltype);

    if (strlen(sqlquery) <= 0) {
        MOTION_LOG(WRN, TYPE_DB, NO_ERRNO, "Ignoring empty sql query");
        return;
    }

    pthread_mutex_lock(&dbse_lock);
        if (mystreq(cnt->conf.database_type,"mysql")) {
            dbse_exec_mysql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"mariadb")) {
            dbse_exec_mariadb(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"postgresql")) {
            dbse_exec_pgsql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"sqlite3")) {
            dbse_exec_sqlite3(sqlquery, cnt);
        }
    pthread_mutex_unlock(&dbse_lock);

}

/** dbse_fileclose */
void dbse_fileclose(struct context *cnt, char *filename, int sqltype, struct timeval *tv1)
{
    char sqlquery[PATH_MAX];

    mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query_stop,
               tv1, filename, sqltype);
    if (strlen(sqlquery) <= 0) {
        MOTION_LOG(WRN, TYPE_DB, NO_ERRNO, "Ignoring empty sql query");
        return;
    }

    pthread_mutex_lock(&dbse_lock);
        if (mystreq(cnt->conf.database_type,"mysql")) {
            dbse_exec_mysql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"mariadb")) {
            dbse_exec_mariadb(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"postgresql")) {
            dbse_exec_pgsql(sqlquery, cnt);
        } else if (mystreq(cnt->conf.database_type,"sqlite3")) {
            dbse_exec_sqlite3(sqlquery, cnt);
        }
    pthread_mutex_unlock(&dbse_lock);

}

