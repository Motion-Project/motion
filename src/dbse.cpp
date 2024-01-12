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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 */

#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "movie.hpp"
#include "dbse.hpp"

/* Forward Declare */
void dbse_close(ctx_motapp *motapp);

static void dbse_edits(ctx_motapp *motapp)
{
    int retcd = 0;

    if ((motapp->dbse->database_type != "") &&
        (motapp->dbse->database_dbname == "")) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Invalid database name"));
        retcd = -1;
    }
    if (((motapp->dbse->database_type == "mariadb") ||
         (motapp->dbse->database_type == "pgsql")) &&
         (motapp->dbse->database_port == 0)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Must specify database port for mariadb/pgsql"));
        retcd = -1;
    }

    if ((motapp->dbse->database_type == "sqlite3") &&
        (motapp->dbse->database_dbname == "")) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Must specify database name for sqlite3"));
        retcd = -1;
    }

    if ((motapp->dbse->database_type != "") && (retcd == -1)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Database functionality disabled."));
        motapp->dbse->database_type = "";
    }

}

/* Free the cols lists*/
static void dbse_cols_free(ctx_motapp *motapp)
{
    int indx;

    if (motapp->dbse == NULL) {
        return;
    }

    if (motapp->dbse->cols_list != NULL) {
        for (indx=0; indx<motapp->dbse->cols_cnt; indx++) {
            myfree(&motapp->dbse->cols_list[indx].col_nm);
            myfree(&motapp->dbse->cols_list[indx].col_typ);
        }
        myfree(&motapp->dbse->cols_list);
    }
    motapp->dbse->cols_cnt = 0;

}

/* Free the movies lists*/
static void dbse_movies_free(ctx_motapp *motapp)
{
    int indx;

    if (motapp->dbse->movie_list != NULL) {
        for (indx=0; indx<motapp->dbse->movie_cnt; indx++) {
            myfree(&motapp->dbse->movie_list[indx].movie_nm);
            myfree(&motapp->dbse->movie_list[indx].movie_dir);
            myfree(&motapp->dbse->movie_list[indx].full_nm);
            myfree(&motapp->dbse->movie_list[indx].movie_tmc);
            myfree(&motapp->dbse->movie_list[indx].movie_tml);

        }
        myfree(&motapp->dbse->movie_list);
    }
    motapp->dbse->movie_cnt = 0;

}

#ifdef HAVE_DBSE

/* Create array of all the columns in current version */
static void dbse_cols_list(ctx_motapp *motapp)
{
    int indx;

    dbse_cols_free(motapp);

    /* 50 is an arbitrary "high" number */
    motapp->dbse->cols_cnt = 50;
    motapp->dbse->cols_list =(ctx_dbse_col *)
        mymalloc(sizeof(ctx_dbse_col) * motapp->dbse->cols_cnt);

    /* The size of 30 is arbitrary */
    for (indx=0; indx<motapp->dbse->cols_cnt; indx++) {
        motapp->dbse->cols_list[indx].col_nm = (char*)mymalloc(30);
        motapp->dbse->cols_list[indx].col_typ = (char*)mymalloc(30);
        motapp->dbse->cols_list[indx].found = false;
    }

    indx=0;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "device_id");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_nm");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "text");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_dir");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "text");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "full_nm");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "text");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_sz");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_dtl");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_tmc");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "text");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "movie_tml");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "text");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "diff_avg");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "sdev_min");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "sdev_max");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

    indx++;
    snprintf(motapp->dbse->cols_list[indx].col_nm,  30, "%s", "sdev_avg");
    snprintf(motapp->dbse->cols_list[indx].col_typ, 30, "%s", "int");

}

/* Assign default values for records from database*/
static void dbse_rec_default(ctx_dbse_rec *rec)
{
    rec->found     = false;
    rec->record_id  = -1;
    rec->device_id     = -1;

    rec->movie_nm = (char*)mymalloc(5);
    snprintf(rec->movie_nm, 5,"%s", "null");

    rec->movie_dir = (char*)mymalloc(5);
    snprintf(rec->movie_dir, 5,"%s", "null");

    rec->full_nm = (char*)mymalloc(5);
    snprintf(rec->full_nm, 5,"%s", "null");

    rec->movie_sz  = 0;
    rec->movie_dtl = 0;

    rec->movie_tmc = (char*)mymalloc(5);
    snprintf(rec->movie_tmc, 5,"%s", "null");

    rec->movie_tml = (char*)mymalloc(5);
    snprintf(rec->movie_tml, 5,"%s", "null");

    rec->diff_avg  = 0;
    rec->sdev_min  = 0;
    rec->sdev_max  = 0;
    rec->sdev_avg  = 0;

}

/* Assign values to rec from the database */
static void dbse_rec_assign(ctx_dbse_rec *rec, char *col_nm, char *col_val)
{
    int flen;
    struct stat statbuf;

    if (mystrceq(col_nm,"record_id")) {
        rec->record_id = atoi(col_val);

    } else if (mystrceq(col_nm,"device_id")) {
        rec->device_id = atoi(col_val);

    } else if (mystrceq(col_nm,"movie_nm")) {
        free(rec->movie_nm);
        flen = (int)strlen(col_val);
        rec->movie_nm = (char*)mymalloc(flen + 1);
        snprintf(rec->movie_nm, flen+1,"%s",col_val);

    } else if (mystrceq(col_nm,"movie_dir")) {
        free(rec->movie_dir);
        flen = (int)strlen(col_val);
        rec->movie_dir = (char*)mymalloc(flen + 1);
        snprintf(rec->movie_dir, flen+1,"%s",col_val);

    } else if (mystrceq(col_nm,"full_nm")) {
        free(rec->full_nm);
        flen = (int)strlen(col_val);
        rec->full_nm = (char*)mymalloc(flen + 1);
        snprintf(rec->full_nm, flen+1,"%s",col_val);
        if (stat(rec->full_nm, &statbuf) == 0) {
            rec->found = true;
        }

    } else if (mystrceq(col_nm,"movie_sz")) {
        rec->movie_sz =atoi(col_val);

    } else if (mystrceq(col_nm,"movie_dtl")) {
        rec->movie_dtl =atoi(col_val);

    } else if (mystrceq(col_nm,"movie_tmc")) {
        free(rec->movie_tmc);
        flen = (int)strlen(col_val);
        rec->movie_tmc =
            (char*)mymalloc(flen + 1);
        snprintf(rec->movie_tmc
            ,flen+1,"%s",col_val);

    } else if (mystrceq(col_nm,"movie_tml")) {
        free(rec->movie_tml);
        flen = (int)strlen(col_val);
        rec->movie_tml =
            (char*)mymalloc(flen + 1);
        snprintf(rec->movie_tml
            ,flen+1,"%s",col_val);

    } else if (mystrceq(col_nm,"diff_avg")) {
        rec->diff_avg =atoi(col_val);
    } else if (mystrceq(col_nm,"sdev_min")) {
        rec->sdev_min =atoi(col_val);
    } else if (mystrceq(col_nm,"sdev_max")) {
        rec->sdev_max =atoi(col_val);
    } else if (mystrceq(col_nm,"sdev_avg")) {
        rec->sdev_avg =atoi(col_val);
    }

}

static void dbse_sql_motpls(ctx_dbse *dbse, std::string &sql)
{
    std::string delimit;
    int indx;

    sql = "";

    if (dbse->dbse_action == DBSE_TBL_CHECK) {
        if (dbse->database_type == "mariadb") {
            sql = "Select table_name "
                " from information_schema.tables "
                " where table_name = 'motionplus';";
        } else if (dbse->database_type == "postgresql") {
            sql = " select tablename as table_nm "
                " from pg_catalog.pg_tables "
                " where schemaname != 'pg_catalog' "
                " and schemaname != 'information_schema' "
                " and tablename = 'motionplus';";
        } else if (dbse->database_type == "sqlite3") {
            sql = "select name from sqlite_master"
                " where type='table' "
                " and name='motionplus';";
        }
    } else if (dbse->dbse_action == DBSE_TBL_CREATE) {
        sql = "create table motionplus (";
        if ((dbse->database_type == "mariadb") ||
            (dbse->database_type == "postgresql")) {
            sql += " record_id serial ";
        } else if (dbse->database_type == "sqlite3") {
            /* Autoincrement is discouraged but I want compatibility*/
            sql += " record_id integer primary key autoincrement ";
        }
        sql += ");";

    } else if (dbse->dbse_action == DBSE_COLS_LIST) {
        sql = " select * from motionplus;";

    } else if (dbse->dbse_action == DBSE_MOV_CLEAN) {
        sql = " delete from motionplus "
            " where record_id in (";
        delimit = " ";
        for (indx=0; indx<dbse->movie_cnt; indx++) {
            if (dbse->movie_list[indx].found == false) {
                sql += delimit + std::to_string(
                    dbse->movie_list[indx].record_id);
                delimit = ",";
            }
            /* 5000 is arbitrary */
            if (sql.length() > 5000) {
                indx = dbse->movie_cnt;
            }
        }
        if (delimit == ",") {
            sql += ");";
        } else {
            sql = "";
        }
    }

}

static void dbse_sql_motpls(ctx_dbse *dbse, std::string &sql, int device_id)
{
    sql = "";

    if (dbse->dbse_action == DBSE_MOV_COUNT) {
        sql  = " select ";
        sql += "   count(*) as movie_cnt ";
        sql += " from motionplus ";
        sql += " where ";
        sql += "   device_id = " + std::to_string(device_id);
        sql += ";";

    } else if (dbse->dbse_action == DBSE_MOV_SELECT) {
        sql  = " select * ";
        sql += " from motionplus ";
        sql += " where ";
        sql += "   device_id = " + std::to_string(device_id);
        sql += " order by ";
        sql += "   movie_dtl, movie_tml;";

    }

}

static void dbse_sql_motpls(ctx_dbse *dbse, std::string &sql, char *col_nm, char *col_typ)
{
    sql = "";

    if ((dbse->dbse_action == DBSE_COLS_ADD) &&
        (strlen(col_nm)  > 0) && (strlen(col_typ) > 0)) {
        sql = "Alter table motionplus add column ";
        sql += std::string(col_nm) + " ";
        sql += std::string(col_typ) + " ;";
    }

}

#endif /* HAVE_DBSE */

#ifdef HAVE_SQLITE3

static void dbse_sqlite3_exec(ctx_motapp *motapp, const char *sqlquery)
{
    int retcd;
    char *errmsg = NULL;

    if ((motapp->dbse->database_sqlite3 == NULL) ||
        (strlen(sqlquery) == 0)) {
        return;
    };

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing query");
    retcd = sqlite3_exec(
        motapp->dbse->database_sqlite3
        , sqlquery, NULL, 0, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("SQLite error was %s"), errmsg);
        sqlite3_free(errmsg);
    }
    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Finished query");
}

static int dbse_sqlite3_cb (
    void *ptr, int arg_nb, char **arg_val, char **col_nm)
{
    ctx_motapp *motapp = (ctx_motapp *)ptr;
    int indx, indx2, rnbr;

    if (motapp->dbse->dbse_action == DBSE_TBL_CHECK) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystrceq(arg_val[indx],"motionplus")) {
                motapp->dbse->table_ok = true;
            }
        }
    } else if (motapp->dbse->dbse_action == DBSE_MOV_COUNT) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystrceq(col_nm[indx],"movie_cnt")) {
                motapp->dbse->movie_cnt =atoi(arg_val[indx]);
            }
        }
    } else if (motapp->dbse->dbse_action == DBSE_COLS_LIST) {
        for (indx=0; indx < arg_nb; indx++) {
            for (indx2=0; indx2 < motapp->dbse->cols_cnt; indx2++) {
                if (mystrceq(col_nm[indx]
                    , motapp->dbse->cols_list[indx2].col_nm)) {
                    motapp->dbse->cols_list[indx2].found = true;
                }
            }
        }
    } else if (motapp->dbse->dbse_action == DBSE_MOV_SELECT) {
        rnbr = motapp->dbse->rec_indx;
        if (rnbr < motapp->dbse->movie_cnt) {
            dbse_rec_default(&motapp->dbse->movie_list[rnbr]);
            for (indx=0; indx < arg_nb; indx++) {
                if (arg_val[indx] != NULL) {
                    dbse_rec_assign(&motapp->dbse->movie_list[rnbr]
                        , (char*)col_nm[indx]
                        , (char*)arg_val[indx]);
                }
            }
        }
        motapp->dbse->rec_indx++;
    }

    return 0;
}

static void dbse_sqlite3_cols(ctx_motapp *motapp)
{
    int retcd, indx;
    char *errmsg = 0;
    std::string sql;

    dbse_cols_list(motapp);

    motapp->dbse->dbse_action = DBSE_COLS_LIST;
    dbse_sql_motpls(motapp->dbse, sql);
    retcd = sqlite3_exec(motapp->dbse->database_sqlite3
        , sql.c_str(), dbse_sqlite3_cb, motapp, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error retrieving table columns: %s"), errmsg);
        sqlite3_free(errmsg);
        return;
    }

    for (indx=0; indx<motapp->dbse->cols_cnt; indx++) {
        if (motapp->dbse->cols_list[indx].found == false) {
            motapp->dbse->dbse_action = DBSE_COLS_ADD;
            dbse_sql_motpls(motapp->dbse, sql
                , motapp->dbse->cols_list[indx].col_nm
                , motapp->dbse->cols_list[indx].col_typ);
            dbse_sqlite3_exec(motapp, sql.c_str());
        }
    }

    dbse_cols_free(motapp);

}

static void dbse_sqlite3_init(ctx_motapp *motapp)
{
    int retcd;
    const char *err_open  = NULL;
    char *err_qry  = NULL;
    std::string sql;

    motapp->dbse->database_sqlite3 = NULL;

    if (motapp->dbse->database_type != "sqlite3") {
        return;
    }

    MOTPLS_LOG(NTC, TYPE_DB, NO_ERRNO
        , _("SQLite3 Database filename %s")
        , motapp->dbse->database_dbname.c_str());
    retcd = sqlite3_open(
        motapp->dbse->database_dbname.c_str()
        , &motapp->dbse->database_sqlite3);
    if (retcd != SQLITE_OK) {
        err_open =sqlite3_errmsg(motapp->dbse->database_sqlite3);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Can't open database %s : %s")
            , motapp->dbse->database_dbname.c_str()
            , err_open);
        sqlite3_close(motapp->dbse->database_sqlite3);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Could not initialize database %s")
            , motapp->dbse->database_dbname.c_str());
        motapp->dbse->is_open = false;
        motapp->dbse->database_sqlite3 = NULL;
        return;
    }

    motapp->dbse->is_open = true;
    MOTPLS_LOG(NTC, TYPE_DB, NO_ERRNO
        ,  _("database_busy_timeout %d msec")
        , motapp->dbse->database_busy_timeout);
    retcd = sqlite3_busy_timeout(
        motapp->dbse->database_sqlite3
        , motapp->dbse->database_busy_timeout);
    if (retcd != SQLITE_OK) {
        err_open = sqlite3_errmsg(
            motapp->dbse->database_sqlite3);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("database_busy_timeout failed %s"), err_open);
    }

    motapp->dbse->table_ok = false;
    motapp->dbse->dbse_action = DBSE_TBL_CHECK;
    dbse_sql_motpls(motapp->dbse, sql);
    retcd = sqlite3_exec(
        motapp->dbse->database_sqlite3
        , sql.c_str(), dbse_sqlite3_cb, motapp, &err_qry);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error checking table: %s"), err_qry);
        sqlite3_free(err_qry);
        return;
    }

    if (motapp->dbse->table_ok == false) {
        motapp->dbse->dbse_action = DBSE_TBL_CREATE;
        dbse_sql_motpls(motapp->dbse, sql);
        retcd = sqlite3_exec(motapp->dbse->database_sqlite3
            , sql.c_str(), 0, 0, &err_qry);
        if (retcd != SQLITE_OK ) {
            MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Error creating table: %s"), err_qry);
                sqlite3_free(err_qry);
            return;
        }
    }

    dbse_sqlite3_cols(motapp);

}

static void dbse_sqlite3_movlst(ctx_motapp *motapp, int device_id)
{
    int retcd;
    char *errmsg  = NULL;
    std::string sql;

    motapp->dbse->dbse_action = DBSE_MOV_COUNT;
    dbse_sql_motpls(motapp->dbse, sql, device_id);
    retcd = sqlite3_exec(
        motapp->dbse->database_sqlite3
        , sql.c_str(), dbse_sqlite3_cb, motapp, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error counting table: %s"), errmsg);
        sqlite3_free(errmsg);
        return;
    }

    if (motapp->dbse->movie_cnt > 0) {
        motapp->dbse->movie_list =(ctx_dbse_rec *)
            mymalloc(sizeof(ctx_dbse_rec)*motapp->dbse->movie_cnt);
        motapp->dbse->rec_indx = 0;

        motapp->dbse->dbse_action = DBSE_MOV_SELECT;
        dbse_sql_motpls(motapp->dbse, sql, device_id);
        retcd = sqlite3_exec(
            motapp->dbse->database_sqlite3
            , sql.c_str(), dbse_sqlite3_cb, motapp, &errmsg);
        if (retcd != SQLITE_OK ) {
            MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Error retrieving table: %s"), errmsg);
            sqlite3_free(errmsg);
            return;
        }

        motapp->dbse->dbse_action = DBSE_MOV_CLEAN;
        dbse_sql_motpls(motapp->dbse, sql, device_id);
        dbse_sqlite3_exec(motapp, sql.c_str());

        sql = " vacuum;";
        dbse_sqlite3_exec(motapp, sql.c_str());

    }
    return;
}

static void dbse_sqlite3_close(ctx_motapp *motapp)
{
    if (motapp->dbse->database_type == "sqlite3") {
        if (motapp->dbse->database_sqlite3 != NULL) {
            sqlite3_close(motapp->dbse->database_sqlite3);
            motapp->dbse->database_sqlite3 = NULL;
        }
        motapp->dbse->is_open = false;
    }
}

#endif  /*HAVE_SQLITE3*/

#ifdef HAVE_MARIADB

static void dbse_mariadb_exec (ctx_motapp *motapp, const char *sqlquery)
{
    int retcd;

    if ((motapp->dbse->database_mariadb == NULL) ||
        (strlen(sqlquery) == 0)) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing MariaDB query");
    retcd = mysql_query(motapp->dbse->database_mariadb, sqlquery);
    if (retcd != 0) {
        retcd = mysql_errno(motapp->dbse->database_mariadb);
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , _("MariaDB query '%s' failed. %s error code %d")
            , sqlquery, mysql_error(motapp->dbse->database_mariadb)
            , retcd);
        if (retcd >= 2000) {
            dbse_close(motapp);
            return;
        }
    }
    retcd = mysql_query(motapp->dbse->database_mariadb, "commit;");
    if (retcd != 0) {
        retcd = mysql_errno(motapp->dbse->database_mariadb);
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , _("MariaDB query commit failed. %s error code %d")
            , mysql_error(motapp->dbse->database_mariadb), retcd);
        if (retcd >= 2000) {
            dbse_close(motapp);
            return;
        }
    }

}

static void dbse_mariadb_recs (ctx_motapp *motapp, const char *sqlquery)
{
    int retcd, indx, indx2;
    int qry_fields, rnbr;
    MYSQL_RES *qry_result;
    MYSQL_ROW qry_row;
    MYSQL_FIELD *qry_col;
    ctx_dbse_col *cols;

    retcd = mysql_query(motapp->dbse->database_mariadb, sqlquery);
    if (retcd != 0){
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Query error: %s"),sqlquery);
        dbse_close(motapp);
        return;
    }

    qry_result = mysql_store_result(motapp->dbse->database_mariadb);
    if (qry_result == NULL) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Query store error: %s"),sqlquery);
        dbse_close(motapp);
        return;
    }

    qry_fields = mysql_num_fields(qry_result);
    cols =(ctx_dbse_col *)
        mymalloc(sizeof(ctx_dbse_col) * qry_fields);
    for(indx = 0; indx < qry_fields; indx++) {
        qry_col = mysql_fetch_field(qry_result);
        cols[indx].col_nm = (char*)mymalloc(qry_col->name_length + 1);
        snprintf(cols[indx].col_nm, qry_col->name_length + 1
            , "%s", qry_col->name);
    }

    qry_row = mysql_fetch_row(qry_result);

    if (motapp->dbse->dbse_action == DBSE_TBL_CHECK) {
        motapp->dbse->table_ok = false;
        while (qry_row != NULL) {
            for(indx = 0; indx < qry_fields; indx++) {
                if (qry_row[indx] != NULL) {
                    if (mystrceq(qry_row[indx], "motionplus")) {
                        motapp->dbse->table_ok = true;
                    }
                }
            }
            qry_row = mysql_fetch_row(qry_result);
        }

    } else if (motapp->dbse->dbse_action == DBSE_MOV_COUNT) {
        motapp->dbse->movie_cnt = 0;
        while (qry_row != NULL) {
            for(indx = 0; indx < qry_fields; indx++) {
                if (mystrceq(cols[indx].col_nm, "movie_cnt")) {
                    motapp->dbse->movie_cnt =atoi(qry_row[indx]);
                }
            }
            qry_row = mysql_fetch_row(qry_result);
        }

    } else if (motapp->dbse->dbse_action == DBSE_COLS_LIST) {
        for(indx = 0; indx < qry_fields; indx++) {
            for (indx2=0; indx2 < motapp->dbse->cols_cnt; indx2++) {
                if (mystrceq(cols[indx].col_nm
                    , motapp->dbse->cols_list[indx2].col_nm)) {
                    motapp->dbse->cols_list[indx2].found = true;
                }
            }
        }

    } else if (motapp->dbse->dbse_action == DBSE_MOV_SELECT) {
        motapp->dbse->rec_indx = 0;
        while (qry_row != NULL) {
            rnbr = motapp->dbse->rec_indx;
            dbse_rec_default(&motapp->dbse->movie_list[rnbr]);
            for(indx = 0; indx < qry_fields; indx++) {
                if (qry_row[indx] != NULL) {
                    dbse_rec_assign(&motapp->dbse->movie_list[rnbr]
                        , (char*)cols[indx].col_nm
                        , (char*)qry_row[indx]);
                }
            }
            motapp->dbse->rec_indx++;
            qry_row = mysql_fetch_row(qry_result);
        }
    }
    mysql_free_result(qry_result);

    for(indx = 0; indx < qry_fields; indx++) {
        free(cols[indx].col_nm);
    }
    free(cols);

    return;
}

static void dbse_mariadb_cols(ctx_motapp *motapp)
{
    int indx;
    std::string sql;

    dbse_cols_list(motapp);

    motapp->dbse->dbse_action = DBSE_COLS_LIST;
    dbse_sql_motpls(motapp->dbse, sql);
    dbse_mariadb_recs(motapp, sql.c_str());

    for (indx=0; indx<motapp->dbse->cols_cnt; indx++) {
        if (motapp->dbse->cols_list[indx].found == false) {
            motapp->dbse->dbse_action = DBSE_COLS_ADD;
            dbse_sql_motpls(motapp->dbse, sql
                , motapp->dbse->cols_list[indx].col_nm
                , motapp->dbse->cols_list[indx].col_typ);
            dbse_mariadb_exec(motapp, sql.c_str());
        }
    }

    dbse_cols_free(motapp);

}

static void dbse_mariadb_setup(ctx_motapp *motapp)
{
    std::string sql;

    motapp->dbse->dbse_action = DBSE_TBL_CHECK;
    dbse_sql_motpls(motapp->dbse, sql);
    dbse_mariadb_recs(motapp, sql.c_str());

    if (motapp->dbse->table_ok == false) {
        MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
            , _("Creating motionplus table"));
        motapp->dbse->dbse_action = DBSE_TBL_CREATE;
        dbse_sql_motpls(motapp->dbse, sql);
        dbse_mariadb_exec(motapp,sql.c_str());
    }

    dbse_mariadb_cols(motapp);

}

static void dbse_mariadb_init(ctx_motapp *motapp)
{
    bool my_true = true;

    motapp->dbse->database_mariadb = NULL;

    if (motapp->dbse->database_type != "mariadb") {
        return;
    }

    if (mysql_library_init(0, NULL, NULL)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Could not initialize database %s")
            , motapp->dbse->database_type.c_str());
        motapp->dbse->is_open = false;
        return;
    }

    motapp->dbse->database_mariadb = (MYSQL *) mymalloc(sizeof(MYSQL));
    mysql_init(motapp->dbse->database_mariadb);

    if (mysql_real_connect(
        motapp->dbse->database_mariadb
        , motapp->dbse->database_host.c_str()
        , motapp->dbse->database_user.c_str()
        , motapp->dbse->database_password.c_str()
        , motapp->dbse->database_dbname.c_str()
        , motapp->dbse->database_port, NULL, 0) == NULL) {

        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Cannot connect to MariaDB database %s on host %s with user %s")
            , motapp->dbse->database_dbname.c_str()
            , motapp->dbse->database_host.c_str()
            , motapp->dbse->database_user.c_str());
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("MariaDB error was %s")
            , mysql_error(motapp->dbse->database_mariadb));
        dbse_close(motapp);
        return;
    }
    motapp->dbse->is_open = true;
    mysql_options(motapp->dbse->database_mariadb
        , MYSQL_OPT_RECONNECT, &my_true);

    dbse_mariadb_setup(motapp);

    MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
        , _("%s database opened")
        , motapp->dbse->database_dbname.c_str() );
}

static void dbse_mariadb_close(ctx_motapp *motapp)
{
    if (motapp->dbse->database_type == "mariadb") {
        mysql_library_end();
        if (motapp->dbse->database_mariadb != NULL) {
            mysql_close(motapp->dbse->database_mariadb);
            free(motapp->dbse->database_mariadb);
            motapp->dbse->database_mariadb = NULL;
        }
        motapp->dbse->is_open = false;
    }
}

static void dbse_mariadb_movlst(ctx_motapp *motapp, int device_id )
{
    std::string sql;

    motapp->dbse->dbse_action = DBSE_MOV_COUNT;
    dbse_sql_motpls(motapp->dbse, sql, device_id);
    dbse_mariadb_recs(motapp, sql.c_str());

    if (motapp->dbse->movie_cnt > 0) {
        motapp->dbse->movie_list =(ctx_dbse_rec *)
            mymalloc(sizeof(ctx_dbse_rec)*motapp->dbse->movie_cnt);

        motapp->dbse->dbse_action = DBSE_MOV_SELECT;
        dbse_sql_motpls(motapp->dbse, sql, device_id);
        dbse_mariadb_recs(motapp, sql.c_str());

        motapp->dbse->dbse_action = DBSE_MOV_CLEAN;
        dbse_sql_motpls(motapp->dbse, sql);
        dbse_mariadb_exec(motapp, sql.c_str());
    }
}

#endif  /*HAVE_MARIADB*/

#ifdef HAVE_PGSQL

static void dbse_pgsql_exec(ctx_motapp *motapp, const char *sqlquery)
{
    PGresult    *res;

    if ((motapp->dbse->database_pgsql == NULL) ||
        (strlen(sqlquery) == 0)) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing postgresql query");
    res = PQexec(motapp->dbse->database_pgsql, sqlquery);
    if (PQstatus(motapp->dbse->database_pgsql) == CONNECTION_BAD) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Connection to PostgreSQL database '%s' failed: %s")
            , motapp->dbse->database_dbname.c_str()
            , PQerrorMessage(motapp->dbse->database_pgsql));
        PQreset(motapp->dbse->database_pgsql);
        if (PQstatus(motapp->dbse->database_pgsql) == CONNECTION_BAD) {
            MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Re-Connection to PostgreSQL database '%s' failed: %s")
                , motapp->dbse->database_dbname.c_str()
                , PQerrorMessage(motapp->dbse->database_pgsql));
            PQclear(res);
            dbse_close(motapp);
            return;
        } else {
            MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
                , _("Re-Connection to PostgreSQL database '%s' Succeed")
                , motapp->dbse->database_dbname.c_str());
        }
    } else if (!(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK)) {
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , "PGSQL query failed: [%s]  %s %s"
            , sqlquery
            , PQresStatus(PQresultStatus(res))
            , PQresultErrorMessage(res));
    }
    PQclear(res);
}

static void dbse_pgsql_close(ctx_motapp *motapp)
{
    if (motapp->dbse->database_type == "postgresql") {
        if (motapp->dbse->database_pgsql != NULL) {
            PQfinish(motapp->dbse->database_pgsql);
            motapp->dbse->database_pgsql = NULL;
        }
        motapp->dbse->is_open = false;
    }
}

static void dbse_pgsql_recs (ctx_motapp *motapp, const char *sqlquery)
{
    PGresult    *res;
    int indx, indx2, rows, cols, rnbr;

    if (motapp->dbse->database_pgsql == NULL) {
        return;
    }

    res = PQexec(motapp->dbse->database_pgsql, sqlquery);

    if (motapp->dbse->dbse_action == DBSE_TBL_CHECK) {
        motapp->dbse->table_ok = false;
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return;
        }

        cols = PQnfields(res);
        rows = PQntuples(res);
        for(indx = 0; indx < rows; indx++) {
            for (indx2 = 0; indx2 < cols; indx2++){
                if (mystrceq("table_nm", PQfname(res, indx2)) &&
                    mystrceq("motionplus", PQgetvalue(res, indx, indx2))) {
                        motapp->dbse->table_ok = true;
                }
            }
        }
        PQclear(res);

    } else if (motapp->dbse->dbse_action == DBSE_MOV_COUNT) {
        motapp->dbse->movie_cnt = 0;
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return;
        }

        cols = PQnfields(res);
        rows = PQntuples(res);
        for(indx = 0; indx < rows; indx++) {
            for (indx2 = 0; indx2 < cols; indx2++){
                if (mystrceq("movie_cnt", PQfname(res, indx2))) {
                    motapp->dbse->movie_cnt =atoi(PQgetvalue(res, indx, indx2));
                }
            }
        }
        PQclear(res);

    } else if (motapp->dbse->dbse_action == DBSE_COLS_LIST) {
        cols = PQnfields(res);
        for(indx = 0; indx < cols; indx++) {
            for (indx2=0; indx2 < motapp->dbse->cols_cnt; indx2++) {
                if (mystrceq(PQfname(res, indx)
                    , motapp->dbse->cols_list[indx2].col_nm)) {
                    motapp->dbse->cols_list[indx2].found = true;
                }
            }
        }
        PQclear(res);

    } else if (motapp->dbse->dbse_action == DBSE_MOV_SELECT) {
        motapp->dbse->rec_indx = 0;
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return;
        }

        cols = PQnfields(res);
        rows = PQntuples(res);
        for(indx = 0; indx < rows; indx++) {
            rnbr = motapp->dbse->rec_indx;
            dbse_rec_default(&motapp->dbse->movie_list[rnbr]);
            for (indx2 = 0; indx2 < cols; indx2++) {
                if (PQgetvalue(res, indx, indx2) != NULL) {
                    dbse_rec_assign(&motapp->dbse->movie_list[rnbr]
                        , (char*)PQfname(res, indx2)
                        , (char*)PQgetvalue(res, indx, indx2));
                }
            }
            motapp->dbse->rec_indx++;
        }
        PQclear(res);
    }

    return;
}

static void dbse_pgsql_cols(ctx_motapp *motapp)
{
    int indx;
    std::string sql;

    dbse_cols_list(motapp);

    motapp->dbse->dbse_action = DBSE_COLS_LIST;
    dbse_sql_motpls(motapp->dbse, sql);
    dbse_pgsql_recs(motapp, sql.c_str());

    for (indx=0; indx<motapp->dbse->cols_cnt; indx++) {
        if (motapp->dbse->cols_list[indx].found == false) {
            motapp->dbse->dbse_action = DBSE_COLS_ADD;
            dbse_sql_motpls(motapp->dbse, sql
                , motapp->dbse->cols_list[indx].col_nm
                , motapp->dbse->cols_list[indx].col_typ);
            dbse_pgsql_exec(motapp,sql.c_str());
        }
    }

    dbse_cols_free(motapp);

}

static void dbse_pgsql_setup(ctx_motapp *motapp)
{
    std::string sql;

    motapp->dbse->dbse_action = DBSE_TBL_CHECK;
    dbse_sql_motpls(motapp->dbse, sql);
    dbse_pgsql_recs(motapp,sql.c_str());

    if (motapp->dbse->table_ok == false) {
        MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
            , _("Creating motionplus table"));
        motapp->dbse->dbse_action = DBSE_TBL_CREATE;
        dbse_sql_motpls(motapp->dbse, sql);
        dbse_pgsql_exec(motapp,sql.c_str());
    }

    dbse_pgsql_cols(motapp);

}

static void dbse_pgsql_init(ctx_motapp *motapp)
{
    std::string constr;

    motapp->dbse->database_pgsql = NULL;

    if (motapp->dbse->database_type != "postgresql") {
        return;
    }

    constr = "dbname='" + motapp->dbse->database_dbname + "' ";
    constr += " host='" + motapp->dbse->database_host + "' ";
    constr += " user='" + motapp->dbse->database_user + "' ";
    constr += " password='" + motapp->dbse->database_password + "' ";
    constr += " port="+std::to_string(motapp->dbse->database_port) + " ";
    motapp->dbse->database_pgsql = PQconnectdb(constr.c_str());
    if (PQstatus(motapp->dbse->database_pgsql) == CONNECTION_BAD) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Connection to PostgreSQL database '%s' failed: %s")
            , motapp->dbse->database_dbname.c_str()
            , PQerrorMessage(motapp->dbse->database_pgsql));
        dbse_close(motapp);
        return;
    }
    motapp->dbse->is_open = true;

    dbse_pgsql_setup(motapp);

    MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
        , _("%s database opened")
        , motapp->dbse->database_dbname.c_str() );
}

static void dbse_pgsql_movlst(ctx_motapp *motapp, int device_id)
{
    std::string sql;

    motapp->dbse->dbse_action = DBSE_MOV_COUNT;
    dbse_sql_motpls(motapp->dbse, sql, device_id);
    dbse_pgsql_recs(motapp, sql.c_str());

    if (motapp->dbse->movie_cnt > 0) {
        motapp->dbse->movie_list =(ctx_dbse_rec *)
            mymalloc(sizeof(ctx_dbse_rec)*motapp->dbse->movie_cnt);

        motapp->dbse->dbse_action = DBSE_MOV_SELECT;
        dbse_sql_motpls(motapp->dbse, sql, device_id);
        dbse_pgsql_recs(motapp, sql.c_str());

        motapp->dbse->dbse_action = DBSE_MOV_CLEAN;
        dbse_sql_motpls(motapp->dbse, sql);
        dbse_pgsql_exec(motapp, sql.c_str());
    }
    return;
}

#endif  /*HAVE_PGSQL*/

static bool dbse_open(ctx_motapp *motapp)
{

    if (motapp->dbse->database_type == "") {
        return false;
    }

    pthread_mutex_lock(&motapp->dbse->mutex_dbse);

        if (motapp->dbse->is_open) {
            pthread_mutex_unlock(&motapp->dbse->mutex_dbse);
            return true;
        }

        MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO,_("Opening database"));

        #ifdef HAVE_MARIADB
            if (motapp->dbse->database_type == "mariadb") {
                dbse_mariadb_init(motapp);
            }
        #endif
        #ifdef HAVE_PGSQL
            if (motapp->dbse->database_type == "postgresql") {
                dbse_pgsql_init(motapp);
            }
        #endif
        #ifdef HAVE_SQLITE3
            if (motapp->dbse->database_type == "sqlite3") {
                dbse_sqlite3_init(motapp);
            }
        #endif
    pthread_mutex_unlock(&motapp->dbse->mutex_dbse);

    return motapp->dbse->is_open;
}

void dbse_init(ctx_motapp *motapp)
{
    motapp->dbse = new ctx_dbse;
    motapp->dbse->database_busy_timeout = motapp->conf->database_busy_timeout;
    motapp->dbse->database_dbname = motapp->conf->database_dbname;
    motapp->dbse->database_host = motapp->conf->database_host;
    motapp->dbse->database_password = motapp->conf->database_password;
    motapp->dbse->database_port = motapp->conf->database_port;
    motapp->dbse->database_type = motapp->conf->database_type;
    motapp->dbse->database_user = motapp->conf->database_user;
    motapp->dbse->movie_cnt = 0;
    motapp->dbse->movie_list = NULL;
    motapp->dbse->cols_cnt = 0;
    motapp->dbse->cols_list = NULL;
    motapp->dbse->is_open = false;

    pthread_mutex_init(&motapp->dbse->mutex_dbse, NULL);

    dbse_edits(motapp);

    dbse_open(motapp);
}

/* Populate the list of the movies from the database*/
void dbse_movies_getlist(ctx_motapp *motapp, int device_id)
{

     if (dbse_open(motapp) == false) {
        return;
    }

    dbse_movies_free(motapp);

    pthread_mutex_lock(&motapp->dbse->mutex_dbse);
        #ifdef HAVE_MARIADB
            if (motapp->dbse->database_type == "mariadb") {
                dbse_mariadb_movlst(motapp, device_id);
            }
        #endif
        #ifdef HAVE_PGSQL
            if (motapp->dbse->database_type == "postgresql") {
                dbse_pgsql_movlst(motapp, device_id);
            }
        #endif
        #ifdef HAVE_SQLITE3
            if (motapp->dbse->database_type == "sqlite3") {
                dbse_sqlite3_movlst(motapp, device_id);
            }
        #endif
        #ifndef HAVE_DBSE
            (void)device_id;
        #endif
    pthread_mutex_unlock(&motapp->dbse->mutex_dbse);

}

void dbse_close(ctx_motapp *motapp)
{
    #ifdef HAVE_MARIADB
        dbse_mariadb_close(motapp);
    #endif
    #ifdef HAVE_PGSQL
        dbse_pgsql_close(motapp);
    #endif
    #ifdef HAVE_SQLITE3
        dbse_sqlite3_close(motapp);
    #endif
    #ifndef HAVE_DBSE
        (void)motapp;
    #endif
}

void dbse_deinit(ctx_motapp *motapp)
{
    dbse_movies_free(motapp);

    dbse_cols_free(motapp);

    dbse_close(motapp);

    pthread_mutex_destroy(&motapp->dbse->mutex_dbse);

    if (motapp->dbse != NULL) {
        delete motapp->dbse;
    }

}

/* Execute sql against database with mutex lock */
void dbse_exec_sql(ctx_motapp *motapp, const char *sqlquery)
{

    if (dbse_open(motapp) == false) {
        return;
    }

    pthread_mutex_lock(&motapp->dbse->mutex_dbse);
        #ifdef HAVE_MARIADB
            if (motapp->dbse->database_type == "mariadb") {
                dbse_mariadb_exec(motapp, sqlquery);
            }
        #endif
        #ifdef HAVE_PGSQL
            if (motapp->dbse->database_type == "postgresql") {
                dbse_pgsql_exec(motapp, sqlquery);
            }
        #endif
        #ifdef HAVE_SQLITE3
            if (motapp->dbse->database_type == "sqlite3") {
                dbse_sqlite3_exec(motapp, sqlquery);
            }
        #endif
        #ifndef HAVE_DBSE
            (void)sqlquery;
        #endif
    pthread_mutex_unlock(&motapp->dbse->mutex_dbse);

}

/* Create and execute user provided sql with mutex lock*/
void dbse_exec(ctx_dev *cam, char *filename, const char *cmd)
{
    char sqlquery[PATH_MAX];

    /* This is to prevent flooding log with open/fail messages*/
    if (mystrceq(cmd,"event_start")) {
        if (dbse_open(cam->motapp) == false) {
            return;
        }
    } else {
        if (cam->motapp->dbse->is_open == false) {
            return;
        }
    }

    if (mystrceq(cmd,"pic_save")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_pic_save.c_str()
            , &cam->current_image->imgts, filename);
    } else if (mystrceq(cmd,"movie_start")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_movie_start.c_str()
            , &cam->current_image->imgts, filename);
    } else if (mystrceq(cmd,"movie_end")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_movie_end.c_str()
            , &cam->current_image->imgts, filename);
    } else if (mystrceq(cmd,"event_start")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_event_start.c_str()
            , &cam->current_image->imgts, filename);
    } else if (mystrceq(cmd,"event_end")) {
        mystrftime(cam, sqlquery, sizeof(sqlquery)
            , cam->conf->sql_event_end.c_str()
            , &cam->current_image->imgts, filename);
    }

    if (strlen(sqlquery) <= 0) {
        return;
    }
    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "%s query: %s", cmd, sqlquery);

    dbse_exec_sql(cam->motapp, sqlquery);

}

/* Add a record to motionplus table for new movies */
void dbse_movies_addrec(ctx_dev *cam, ctx_movie *movie, timespec *ts1)
{
    std::string sqlquery;
    struct stat statbuf;
    int64_t bsz;
    char dtl[12];
    char tmc[12];
    char tml[12];

    uint64_t diff_avg, sdev_avg;
    struct tm timestamp_tm;

    if (dbse_open(cam->motapp) == false) {
        return;
    }

    /* Movie file times */
    if (stat(movie->full_nm, &statbuf) == 0) {
        bsz = statbuf.st_size;
    } else {
        bsz = 0;
    }
    localtime_r(&ts1->tv_sec, &timestamp_tm);
    strftime(dtl, 11, "%G%m%d"   , &timestamp_tm);
    strftime(tmc, 11, "%I:%M%p"  , &timestamp_tm);
    strftime(tml, 11, "%H:%M:%S" , &timestamp_tm);

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
    sqlquery += " (device_id, movie_nm, movie_dir, full_nm, movie_sz, movie_dtl";
    sqlquery += " , movie_tmc, movie_tml, diff_avg, sdev_min, sdev_max, sdev_avg)";
    sqlquery += " values ("+std::to_string(cam->device_id);
    sqlquery += " ,'" + std::string(movie->movie_nm) + "'";
    sqlquery += " ,'" + std::string(movie->movie_dir) + "'";
    sqlquery += " ,'" + std::string(movie->full_nm) + "'";
    sqlquery += " ,"  + std::to_string(bsz);
    sqlquery += " ,"  + std::string(dtl);
    sqlquery += " ,'" + std::string(tmc)+ "'";
    sqlquery += " ,'" + std::string(tml)+ "'";
    sqlquery += " ,"  + std::to_string(diff_avg);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_min);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_max);
    sqlquery += " ,"  + std::to_string(sdev_avg);
    sqlquery += ")";

    dbse_exec_sql(cam->motapp, sqlquery.c_str());

}

