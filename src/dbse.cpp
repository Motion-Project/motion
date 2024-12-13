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

#include "motionplus.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "dbse.hpp"

static void *dbse_handler(void *arg)
{
    ((cls_dbse *)arg)->handler();
    return nullptr;
}

#ifdef HAVE_DBSE

void cls_dbse::cols_vec_add(std::string nm, std::string typ)
{
    ctx_col_item col_itm;
    col_itm.found = false;
    col_itm.col_nm = nm;
    col_itm.col_typ = typ;
    col_names.push_back(col_itm);
}

void cls_dbse::cols_vec_create()
{
    col_names.clear();
    cols_vec_add("device_id","int");
    cols_vec_add("file_typ","text");
    cols_vec_add("file_nm","text");
    cols_vec_add("file_dir","text");
    cols_vec_add("full_nm","text");
    cols_vec_add("file_sz","int");
    cols_vec_add("file_dtl","int");
    cols_vec_add("file_tmc","text");
    cols_vec_add("file_tml","text");
    cols_vec_add("diff_avg","int");
    cols_vec_add("sdev_min","int");
    cols_vec_add("sdev_max","int");
    cols_vec_add("sdev_avg","int");
}

void cls_dbse::item_default()
{
    file_item.found = false;
    file_item.record_id = -1;
    file_item.device_id = -1;
    file_item.file_typ = "null";
    file_item.file_nm = "null";
    file_item.file_dir = "null";
    file_item.full_nm = "null";
    file_item.file_sz  = 0;
    file_item.file_dtl = 0;
    file_item.file_tmc = "null";
    file_item.file_tml = "null";
    file_item.diff_avg  = 0;
    file_item.sdev_min  = 0;
    file_item.sdev_max  = 0;
    file_item.sdev_avg  = 0;

}

/* Assign values to rec from the database */
void cls_dbse::item_assign(std::string col_nm, std::string col_val)
{
    struct stat statbuf;

    if (col_nm == "record_id") {
        file_item.record_id = mtoi(col_val);
    } else if (col_nm == "device_id") {
        file_item.device_id = mtoi(col_val);
    } else if (col_nm == "file_typ") {
        file_item.file_typ = col_val;
    } else if (col_nm == "file_nm") {
        file_item.file_nm = col_val;
    } else if (col_nm == "file_dir") {
        file_item.file_dir = col_val;
    } else if (col_nm == "full_nm") {
        file_item.full_nm = col_val;
        if (stat(file_item.full_nm.c_str(), &statbuf) == 0) {
            file_item.found = true;
        }
    } else if (col_nm == "file_sz") {
        file_item.file_sz = mtoi(col_val);
    } else if (col_nm == "file_dtl") {
        file_item.file_dtl =mtoi(col_val);
    } else if (col_nm == "file_tmc") {
        file_item.file_tmc = col_val;
    } else if (col_nm == "file_tml") {
        file_item.file_tml = col_val;
    } else if (col_nm == "diff_avg") {
        file_item.diff_avg = mtoi(col_val);
    } else if (col_nm == "sdev_min") {
        file_item.sdev_min = mtoi(col_val);
    } else if (col_nm == "sdev_max") {
        file_item.sdev_max = mtoi(col_val);
    } else if (col_nm == "sdev_avg") {
        file_item.sdev_avg = mtoi(col_val);
    }
}

void cls_dbse::sql_motpls(std::string &sql)
{
    std::string delimit;

    if ((is_open == false) || (finish == true)) {
        return;
    }

    sql = "";

    if (dbse_action == DBSE_TBL_CHECK) {
        if (app->cfg->database_type == "mariadb") {
            sql = "Select table_name "
                " from information_schema.tables "
                " where table_name = 'motionplus';";
        } else if (app->cfg->database_type == "postgresql") {
            sql = " select tablename as table_nm "
                " from pg_catalog.pg_tables "
                " where schemaname != 'pg_catalog' "
                " and schemaname != 'information_schema' "
                " and tablename = 'motionplus';";
        } else if (app->cfg->database_type == "sqlite3") {
            sql = "select name from sqlite_master"
                " where type='table' "
                " and name='motionplus';";
        }
    } else if (dbse_action == DBSE_TBL_CREATE) {
        sql = "create table motionplus (";
        if ((app->cfg->database_type == "mariadb") ||
            (app->cfg->database_type == "postgresql")) {
            sql += " record_id serial ";
        } else if (app->cfg->database_type == "sqlite3") {
            /* Autoincrement is discouraged but I want compatibility*/
            sql += " record_id integer primary key autoincrement ";
        }
        sql += ");";

    } else if ((dbse_action == DBSE_COLS_LIST) || (dbse_action == DBSE_COLS_CURRENT)) {
        sql = " select * from motionplus;";

    }

}

void cls_dbse::sql_motpls(std::string &sql, std::string col_p1, std::string col_p2)
{
    if ((is_open == false) || (finish == true)) {
        return;
    }

    sql = "";

    if ((dbse_action == DBSE_COLS_ADD) &&
        (col_p1 != "") && (col_p2 != "")) {
        sql = "Alter table motionplus add column ";
        sql += col_p1 + " " + col_p2 + " ;";
    } else if ((dbse_action == DBSE_COLS_RENAME) &&
        (col_p1 != "") && (col_p2 != "")) {
        sql = "Alter table motionplus rename column ";
        sql += col_p1 + " to " + col_p2 + " ;";
    }
}

#endif /* HAVE_DBSE */

#ifdef HAVE_SQLITE3DB

static int dbse_sqlite3db_cb (void *ptr, int arg_nb, char **arg_val, char **col_nm)
{
    cls_dbse *dbse = (cls_dbse*)ptr;
    dbse->sqlite3db_cb(arg_nb, arg_val, col_nm);
    return 0;
}

void cls_dbse::sqlite3db_exec(std::string sql)
{
    int retcd;
    char *errmsg = nullptr;

    if ((finish == true) || (database_sqlite3db == nullptr) || (is_open == false)) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing query");
    retcd = sqlite3_exec(database_sqlite3db
        , sql.c_str(), nullptr, 0, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("SQLite error was %s"), errmsg);
        sqlite3_free(errmsg);
    }
    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Finished query");
}

void cls_dbse::sqlite3db_cb (int arg_nb, char **arg_val, char **col_nm)
{
    int indx, indx2;

    if ((finish == true) || (database_sqlite3db == nullptr) || (is_open == false)) {
        return;
    }

    if (dbse_action == DBSE_TBL_CHECK) {
        for (indx=0; indx < arg_nb; indx++) {
            if (mystrceq(arg_val[indx],"motionplus")) {
                table_ok = true;
            }
        }

    } else if (dbse_action == DBSE_COLS_LIST) {
        for (indx=0; indx < arg_nb; indx++) {
            for (indx2=0; indx2 < col_names.size(); indx2++) {
                if (mystrceq(col_nm[indx], col_names[indx2].col_nm.c_str())) {
                    col_names[indx2].found = true;
                }
            }
        }
    } else if (dbse_action == DBSE_COLS_CURRENT) {
        col_names.clear();
        for (indx=0; indx < arg_nb; indx++) {
            cols_vec_add(col_nm[indx],"");
        }
    } else if (dbse_action == DBSE_MOV_SELECT) {
        item_default();
        for (indx=0; indx < arg_nb; indx++) {
            if (arg_val[indx] != nullptr) {
                item_assign((char*)col_nm[indx], (char*)arg_val[indx]);
            }
        }
        filelist.push_back(file_item);
    }
}

void cls_dbse::sqlite3db_cols_verify()
{
    int retcd, indx;
    char *errmsg = 0;
    std::string sql;

    if ((finish == true) || (database_sqlite3db == nullptr) || (is_open == false)) {
        return;
    }

    cols_vec_create();

    dbse_action = DBSE_COLS_LIST;
    sql_motpls(sql);
    retcd = sqlite3_exec(database_sqlite3db
        , sql.c_str(), dbse_sqlite3db_cb, this, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error retrieving table columns: %s"), errmsg);
        sqlite3_free(errmsg);
        return;
    }

    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].found == false) {
            dbse_action = DBSE_COLS_ADD;
            sql_motpls(sql
                , col_names[indx].col_nm
                , col_names[indx].col_typ);
            sqlite3db_exec(sql.c_str());
        }
    }
}

void cls_dbse::sqlite3db_cols_rename()
{
    int retcd, indx;
    char *errmsg = 0;
    std::string sql, tmp;

    if ((finish == true) || (database_sqlite3db == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_COLS_CURRENT;
    sql_motpls(sql);
    retcd = sqlite3_exec(database_sqlite3db
        , sql.c_str(), dbse_sqlite3db_cb, this, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error retrieving table columns: %s"), errmsg);
        sqlite3_free(errmsg);
        return;
    }
    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].col_nm.length() > 5) {
            if (col_names[indx].col_nm.substr(0,5) == "movie") {
                dbse_action = DBSE_COLS_RENAME;
                tmp = "file"+col_names[indx].col_nm.substr(5);
                sql_motpls(sql,col_names[indx].col_nm, tmp);
                sqlite3db_exec(sql.c_str());
            }
        }
    }
}

void cls_dbse::sqlite3db_init()
{
    int retcd;
    const char *err_open  = nullptr;
    char *err_qry  = nullptr;
    std::string sql;

    database_sqlite3db = nullptr;

    if (app->cfg->database_type != "sqlite3") {
        return;
    }

    MOTPLS_LOG(NTC, TYPE_DB, NO_ERRNO
        , _("SQLite3 Database filename %s")
        , app->cfg->database_dbname.c_str());
    retcd = sqlite3_open(
        app->cfg->database_dbname.c_str()
        , &database_sqlite3db);
    if (retcd != SQLITE_OK) {
        err_open =sqlite3_errmsg(database_sqlite3db);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Can't open database %s : %s")
            , app->cfg->database_dbname.c_str()
            , err_open);
        sqlite3_close(database_sqlite3db);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Could not initialize database %s")
            , app->cfg->database_dbname.c_str());
        is_open = false;
        database_sqlite3db = nullptr;
        return;
    }

    is_open = true;
    MOTPLS_LOG(NTC, TYPE_DB, NO_ERRNO
        ,  _("database_busy_timeout %d msec")
        , app->cfg->database_busy_timeout);
    retcd = sqlite3_busy_timeout(database_sqlite3db, app->cfg->database_busy_timeout);
    if (retcd != SQLITE_OK) {
        err_open = sqlite3_errmsg(database_sqlite3db);
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("database_busy_timeout failed %s"), err_open);
    }

    table_ok = false;
    dbse_action = DBSE_TBL_CHECK;
    sql_motpls(sql);
    retcd = sqlite3_exec(database_sqlite3db
        , sql.c_str(), dbse_sqlite3db_cb, this, &err_qry);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error checking table: %s"), err_qry);
        sqlite3_free(err_qry);
        return;
    }

    if (table_ok == false) {
        dbse_action = DBSE_TBL_CREATE;
        sql_motpls(sql);
        retcd = sqlite3_exec(database_sqlite3db, sql.c_str(), 0, 0, &err_qry);
        if (retcd != SQLITE_OK ) {
            MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Error creating table: %s"), err_qry);
                sqlite3_free(err_qry);
            return;
        }
    }

    sqlite3db_cols_rename();
    sqlite3db_cols_verify();

}

void cls_dbse::sqlite3db_filelist(std::string sql)
{
    int retcd;
    char *errmsg  = nullptr;

    if ((finish == true) || (database_sqlite3db == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_MOV_SELECT;
    retcd = sqlite3_exec(database_sqlite3db, sql.c_str()
        , dbse_sqlite3db_cb, this, &errmsg);
    if (retcd != SQLITE_OK ) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Error retrieving table: %s"), errmsg);
        sqlite3_free(errmsg);
        return;
    }
}

void cls_dbse::sqlite3db_close()
{
    if (app->cfg->database_type == "sqlite3") {
        if (database_sqlite3db != nullptr) {
            sqlite3_close(database_sqlite3db);
            database_sqlite3db = nullptr;
        }
        is_open = false;
    }
}

#endif  /*HAVE_SQLITE3*/

#ifdef HAVE_MARIADB

void cls_dbse::mariadb_exec (std::string sql)
{
    int retcd;

    if ((finish == true) || (database_mariadb == nullptr) || (is_open == false)) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing MariaDB query");
    retcd = mysql_query(database_mariadb, sql.c_str());
    if (retcd != 0) {
        retcd = (int)mysql_errno(database_mariadb);
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , _("MariaDB query '%s' failed. %s error code %d")
            , sql.c_str()
            , mysql_error(database_mariadb)
            , retcd);
        if (retcd >= 2000) {
            shutdown();
            return;
        }
    }
    retcd = mysql_query(database_mariadb, "commit;");
    if (retcd != 0) {
        retcd = (int)mysql_errno(database_mariadb);
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , _("MariaDB query commit failed. %s error code %d")
            , mysql_error(database_mariadb), retcd);
        if (retcd >= 2000) {
            shutdown();
            return;
        }
    }

}

void cls_dbse::mariadb_recs(std::string sql)
{
    int retcd, indx, indx2;
    int qry_fields;
    MYSQL_RES *qry_result;
    MYSQL_ROW qry_row;
    MYSQL_FIELD *qry_col;
    ctx_col_item dbcol_itm;
    vec_cols dbcol_lst;

    if ((finish == true) || (database_mariadb == nullptr) || (is_open == false)) {
        return;
    }

    retcd = mysql_query(database_mariadb, sql.c_str());
    if (retcd != 0){
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Query error: %s"),sql.c_str());
        shutdown();
        return;
    }

    qry_result = mysql_store_result(database_mariadb);
    if (qry_result == nullptr) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Query store error: %s"),sql.c_str());
        shutdown();
        return;
    }

    qry_fields = (int)mysql_num_fields(qry_result);
    for(indx = 0; indx < qry_fields; indx++) {
        qry_col = mysql_fetch_field(qry_result);
        dbcol_itm.col_nm = qry_col->name;
        dbcol_itm.col_idx = indx;
        dbcol_lst.push_back(dbcol_itm);
    }

    qry_row = mysql_fetch_row(qry_result);

    if (dbse_action == DBSE_TBL_CHECK) {
        table_ok = false;
        while (qry_row != nullptr) {
            for(indx = 0; indx < qry_fields; indx++) {
                if (qry_row[indx] != nullptr) {
                    if (mystrceq(qry_row[indx], "motionplus")) {
                        table_ok = true;
                    }
                }
            }
            qry_row = mysql_fetch_row(qry_result);
        }

    } else if (dbse_action == DBSE_COLS_LIST) {
        for (indx=0;indx < dbcol_lst.size(); indx++) {
            for (indx2=0;indx2<col_names.size();indx2++) {
                if (dbcol_lst[indx].col_nm == col_names[indx2].col_nm) {
                    col_names[indx2].found = true;
                }
            }
        }

    } else if (dbse_action == DBSE_COLS_CURRENT) {
        col_names.clear();
        for (indx=0;indx < dbcol_lst.size(); indx++) {
            cols_vec_add(dbcol_lst[indx].col_nm,"");
        }

    } else if (dbse_action == DBSE_MOV_SELECT) {
        while (qry_row != nullptr) {
            if (finish == true) {
                mysql_free_result(qry_result);
                return;
            }
            item_default();
            for (indx=0;indx<dbcol_lst.size();indx++) {
                if (qry_row[dbcol_lst[indx].col_idx] != nullptr) {
                    item_assign(dbcol_lst[indx].col_nm
                        , (char*)qry_row[dbcol_lst[indx].col_idx]);
                }
            }
            filelist.push_back(file_item);
            qry_row = mysql_fetch_row(qry_result);
        }
    }
    mysql_free_result(qry_result);
}

void cls_dbse::mariadb_cols_verify()
{
    std::string sql;
    int indx;

    if ((finish == true) || (database_mariadb == nullptr) || (is_open == false)) {
        return;
    }

    cols_vec_create();

    dbse_action = DBSE_COLS_LIST;
    sql_motpls(sql);
    mariadb_recs(sql.c_str());

    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].found == false) {
            dbse_action = DBSE_COLS_ADD;
            sql_motpls(sql
                ,col_names[indx].col_nm
                ,col_names[indx].col_typ);
            mariadb_exec(sql.c_str());
        }
    }
}

void cls_dbse::mariadb_cols_rename()
{
    std::string sql, tmp;
    int indx;

    if ((finish == true) || (database_mariadb == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_COLS_CURRENT;
    sql_motpls(sql);
    mariadb_recs(sql.c_str());

    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].col_nm.length() > 5) {
            if (col_names[indx].col_nm.substr(0,5) == "movie") {
                dbse_action = DBSE_COLS_RENAME;
                tmp = "file"+col_names[indx].col_nm.substr(5);
                sql_motpls(sql,col_names[indx].col_nm, tmp);
                mariadb_exec(sql.c_str());
            }
        }
    }
}

void cls_dbse::mariadb_setup()
{
    std::string sql;

    if ((finish == true) || (database_mariadb == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_TBL_CHECK;
    sql_motpls(sql);
    mariadb_recs(sql.c_str());

    if (table_ok == false) {
        MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
            , _("Creating motionplus table"));
        dbse_action = DBSE_TBL_CREATE;
        sql_motpls(sql);
        mariadb_exec(sql.c_str());
    }

    mariadb_cols_rename();
    mariadb_cols_verify();

}

void cls_dbse::mariadb_init()
{
    bool my_true = true;

    database_mariadb = nullptr;

    if (app->cfg->database_type != "mariadb") {
        return;
    }

    if (mysql_library_init(0, nullptr, nullptr)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Could not initialize database %s")
            , app->cfg->database_type.c_str());
        is_open = false;
        return;
    }

    database_mariadb = (MYSQL *) mymalloc(sizeof(MYSQL));
    mysql_init(database_mariadb);

    if (mysql_real_connect(
        database_mariadb
        , app->cfg->database_host.c_str()
        , app->cfg->database_user.c_str()
        , app->cfg->database_password.c_str()
        , app->cfg->database_dbname.c_str()
        , (uint)app->cfg->database_port, nullptr, 0) == nullptr) {

        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Cannot connect to MariaDB database %s on host %s with user %s")
            , app->cfg->database_dbname.c_str()
            , app->cfg->database_host.c_str()
            , app->cfg->database_user.c_str());
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("MariaDB error was %s")
            , mysql_error(database_mariadb));
        shutdown();
        return;
    }
    is_open = true;
    mysql_options(database_mariadb
        , MYSQL_OPT_RECONNECT, &my_true);

    mariadb_setup();

    MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
        , _("%s database opened")
        , app->cfg->database_dbname.c_str() );
}

void cls_dbse::mariadb_close()
{
    if (app->cfg->database_type == "mariadb") {
        mysql_library_end();
        if (database_mariadb != nullptr) {
            mysql_close(database_mariadb);
            free(database_mariadb);
            database_mariadb = nullptr;
        }
        is_open = false;
    }
}

void cls_dbse::mariadb_filelist(std::string sql)
{
    dbse_action = DBSE_MOV_SELECT;
    mariadb_recs(sql.c_str());
}

#endif  /*HAVE_MARIADB*/

#ifdef HAVE_PGSQLDB

void cls_dbse::pgsqldb_exec(std::string sql)
{
    PGresult    *res;

    if ((database_pgsqldb == nullptr) || (sql == "") || (is_open == false)) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "Executing postgresql query");
    res = PQexec(database_pgsqldb, sql.c_str());
    if (PQstatus(database_pgsqldb) == CONNECTION_BAD) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Connection to PostgreSQL database '%s' failed: %s")
            , app->cfg->database_dbname.c_str()
            , PQerrorMessage(database_pgsqldb));
        PQreset(database_pgsqldb);
        if (PQstatus(database_pgsqldb) == CONNECTION_BAD) {
            MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
                , _("Re-Connection to PostgreSQL database '%s' failed: %s")
                , app->cfg->database_dbname.c_str()
                , PQerrorMessage(database_pgsqldb));
            PQclear(res);
            shutdown();
            return;
        } else {
            MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
                , _("Re-Connection to PostgreSQL database '%s' Succeed")
                , app->cfg->database_dbname.c_str());
        }
    } else if (!(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK)) {
        MOTPLS_LOG(ERR, TYPE_DB, SHOW_ERRNO
            , "PGSQL query failed: [%s]  %s %s"
            , sql.c_str()
            , PQresStatus(PQresultStatus(res))
            , PQresultErrorMessage(res));
    }
    PQclear(res);
}

void cls_dbse::pgsqldb_close()
{
    if (app->cfg->database_type == "postgresql") {
        if (database_pgsqldb != nullptr) {
            PQfinish(database_pgsqldb);
            database_pgsqldb = nullptr;
        }
        is_open = false;
    }
}

void cls_dbse::pgsqldb_recs (std::string sql)
{
    PGresult    *res;
    int indx, indx2, rows, cols;

    if ((finish == true) || (database_pgsqldb == nullptr) || (is_open == false)) {
        return;
    }

    res = PQexec(database_pgsqldb, sql.c_str());

    if (dbse_action == DBSE_TBL_CHECK) {
        table_ok = false;
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
                        table_ok = true;
                }
            }
        }
        PQclear(res);

    } else if (dbse_action == DBSE_COLS_LIST) {
        cols = PQnfields(res);
        for(indx=0;indx<cols;indx++) {
            for (indx2=0;indx2<col_names.size();indx2++) {
                if (mystrceq(PQfname(res, indx)
                    , col_names[indx2].col_nm.c_str())) {
                    col_names[indx2].found = true;
                }
            }
        }
        PQclear(res);

    } else if (dbse_action == DBSE_COLS_CURRENT) {
        col_names.clear();
        cols = PQnfields(res);
        for(indx=0;indx<cols;indx++) {
            cols_vec_add(PQfname(res, indx),"");
        }
        PQclear(res);

    } else if (dbse_action == DBSE_MOV_SELECT) {
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            return;
        }

        cols = PQnfields(res);
        rows = PQntuples(res);
        for(indx = 0; indx < rows; indx++) {
            if (finish == true) {
                PQclear(res);
                return;
            }
            item_default();
            for (indx2 = 0; indx2 < cols; indx2++) {
                if (PQgetvalue(res, indx, indx2) != nullptr) {
                    item_assign((char*)PQfname(res, indx2)
                        , (char*)PQgetvalue(res, indx, indx2));
                }
            }
            filelist.push_back(file_item);
        }
        PQclear(res);
    }
}

void cls_dbse::pgsqldb_cols_verify()
{
    std::string sql;
    int indx;

    if ((finish == true) || (database_pgsqldb == nullptr) || (is_open == false)) {
        return;
    }

    cols_vec_create();

    dbse_action = DBSE_COLS_LIST;
    sql_motpls(sql);
    pgsqldb_recs(sql.c_str());

    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].found == false) {
            dbse_action = DBSE_COLS_ADD;
            sql_motpls(sql
                , col_names[indx].col_nm
                , col_names[indx].col_typ);
            pgsqldb_exec(sql.c_str());
        }
    }
}

void cls_dbse::pgsqldb_cols_rename()
{
    int indx;
    std::string sql, tmp;

    if ((finish == true) || (database_pgsqldb == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_COLS_CURRENT;
    sql_motpls(sql);
    pgsqldb_recs(sql.c_str());

    for (indx=0;indx<col_names.size();indx++) {
        if (col_names[indx].col_nm.length() > 5) {
            if (col_names[indx].col_nm.substr(0,5) == "movie") {
                dbse_action = DBSE_COLS_RENAME;
                tmp = "file"+col_names[indx].col_nm.substr(5);
                sql_motpls(sql,col_names[indx].col_nm, tmp);
                pgsqldb_exec(sql.c_str());
            }
        }
    }
}

void cls_dbse::pgsqldb_setup()
{
    std::string sql;

    if ((finish == true) || (database_pgsqldb == nullptr) || (is_open == false)) {
        return;
    }

    dbse_action = DBSE_TBL_CHECK;
    sql_motpls(sql);
    pgsqldb_recs(sql.c_str());

    if (table_ok == false) {
        MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
            , _("Creating motionplus table"));
        dbse_action = DBSE_TBL_CREATE;
        sql_motpls(sql);
        pgsqldb_exec(sql.c_str());
    }

    pgsqldb_cols_rename();
    pgsqldb_cols_verify();

}

void cls_dbse::pgsqldb_init()
{
    std::string constr;

    database_pgsqldb = nullptr;

    if (app->cfg->database_type != "postgresql") {
        return;
    }

    constr = "dbname='" + app->cfg->database_dbname + "' ";
    constr += " host='" + app->cfg->database_host + "' ";
    constr += " user='" + app->cfg->database_user + "' ";
    constr += " password='" + app->cfg->database_password + "' ";
    constr += " port="+std::to_string(app->cfg->database_port) + " ";
    database_pgsqldb = PQconnectdb(constr.c_str());
    if (PQstatus(database_pgsqldb) == CONNECTION_BAD) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Connection to PostgreSQL database '%s' failed: %s")
            , app->cfg->database_dbname.c_str()
            , PQerrorMessage(database_pgsqldb));
        shutdown();
        return;
    }
    is_open = true;

    pgsqldb_setup();

    MOTPLS_LOG(INF, TYPE_DB, NO_ERRNO
        , _("%s database opened")
        , app->cfg->database_dbname.c_str() );
}

void cls_dbse::pgsqldb_filelist(std::string sql)
{
    dbse_action = DBSE_MOV_SELECT;
    pgsqldb_recs(sql.c_str());
}

#endif  /*HAVE_PGSQL*/

bool cls_dbse::dbse_open()
{
    if (is_open) {
        return true;
    }

    if (app->cfg->database_type == "") {
        is_open = false;
        return false;
    }

    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO,_("Opening database"));

    #ifdef HAVE_MARIADB
        if (app->cfg->database_type == "mariadb") {
            mariadb_init();
        }
    #endif
    #ifdef HAVE_PGSQLDB
        if (app->cfg->database_type == "postgresql") {
            pgsqldb_init();
        }
    #endif
    #ifdef HAVE_SQLITE3DB
        if (app->cfg->database_type == "sqlite3") {
            sqlite3db_init();
        }
    #endif

    return is_open;
}

void cls_dbse::filelist_get(std::string sql, vec_files &p_flst)
{
    int indx;
    if (dbse_open() == false) {
        p_flst.clear();
        return;
    }
    if (finish == true) {
        p_flst.clear();
        return;
    }

    pthread_mutex_lock(&mutex_dbse);
        p_flst.clear();
        filelist.clear();
        #ifdef HAVE_MARIADB
            if (app->cfg->database_type == "mariadb") {
                mariadb_filelist(sql);
            }
        #endif
        #ifdef HAVE_PGSQLDB
            if (app->cfg->database_type == "postgresql") {
                pgsqldb_filelist(sql);
            }
        #endif
        #ifdef HAVE_SQLITE3DB
            if (app->cfg->database_type == "sqlite3") {
                sqlite3db_filelist(sql);
            }
        #endif
        for (indx=0;indx<filelist.size();indx++){
            p_flst.push_back(filelist[indx]);
        }
    pthread_mutex_unlock(&mutex_dbse);

}

void cls_dbse::shutdown()
{
    #ifdef HAVE_MARIADB
        mariadb_close();
    #endif
    #ifdef HAVE_PGSQLDB
        pgsqldb_close();
    #endif
    #ifdef HAVE_SQLITE3DB
        sqlite3db_close();
    #endif
}

void cls_dbse::exec_sql(std::string sql)
{
    if (dbse_open() == false) {
        return;
    }

    pthread_mutex_lock(&mutex_dbse);
        #ifdef HAVE_MARIADB
            if (app->cfg->database_type == "mariadb") {
                mariadb_exec(sql);
            }
        #endif
        #ifdef HAVE_PGSQLDB
            if (app->cfg->database_type == "postgresql") {
                pgsqldb_exec(sql);
            }
        #endif
        #ifdef HAVE_SQLITE3DB
            if (app->cfg->database_type == "sqlite3") {
                sqlite3db_exec(sql);
            }
        #endif
        #ifndef HAVE_DBSE
            (void)sql;
        #endif
    pthread_mutex_unlock(&mutex_dbse);

}

void cls_dbse::exec(cls_camera *cam, std::string fname, std::string cmd)
{
    std::string sql;

    if (dbse_open() == false) {
        return;
    }

    cam->watchdog = cam->cfg->watchdog_tmo;

    if (cmd == "pic_save") {
        mystrftime(cam, sql, cam->cfg->sql_pic_save, fname);
    } else if (cmd == "movie_start") {
        mystrftime(cam, sql, cam->cfg->sql_movie_start, fname);
    } else if (cmd == "movie_end") {
        mystrftime(cam, sql, cam->cfg->sql_movie_end, fname);
    } else if (cmd == "event_start") {
        mystrftime(cam, sql, cam->cfg->sql_event_start, fname);
    } else if (cmd == "event_end") {
        mystrftime(cam, sql, cam->cfg->sql_event_end, fname);
    }

    if (sql == "") {
        return;
    }
    MOTPLS_LOG(DBG, TYPE_DB, NO_ERRNO, "%s query: %s"
        , cmd.c_str(), sql.c_str());

    exec_sql(sql);

}

void cls_dbse::filelist_add(cls_camera *cam, timespec *ts1, std::string ftyp
    ,std::string filenm, std::string fullnm, std::string dirnm)
{
    std::string sqlquery;
    struct stat statbuf;
    int64_t bsz;
    char dtl[12];
    char tmc[12];
    char tml[12];

    uint64_t diff_avg, sdev_avg;
    struct tm timestamp_tm;

    if (dbse_open() == false) {
        return;
    }

    cam->watchdog = cam->cfg->watchdog_tmo;

    if (stat(fullnm.c_str(), &statbuf) == 0) {
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
    sqlquery += " (device_id, file_nm, file_typ, file_dir";
    sqlquery += " , full_nm, file_sz, file_dtl";
    sqlquery += " , file_tmc, file_tml, diff_avg";
    sqlquery += " , sdev_min, sdev_max, sdev_avg)";
    sqlquery += " values ("+std::to_string(cam->cfg->device_id);
    sqlquery += " ,'" + filenm + "'";
    sqlquery += " ,'" + ftyp + "'";
    sqlquery += " ,'" + dirnm + "'";
    sqlquery += " ,'" + fullnm + "'";
    sqlquery += " ,"  + std::to_string(bsz);
    sqlquery += " ,"  + std::string(dtl);
    sqlquery += " ,'" + std::string(tmc)+ "'";
    sqlquery += " ,'" + std::string(tml)+ "'";
    sqlquery += " ,"  + std::to_string(diff_avg);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_min);
    sqlquery += " ,"  + std::to_string(cam->info_sdev_max);
    sqlquery += " ,"  + std::to_string(sdev_avg);
    sqlquery += ")";

    exec_sql(sqlquery);

}

void cls_dbse::dbse_edits()
{
    int retcd = 0;

    if ((app->cfg->database_type != "") &&
        (app->cfg->database_dbname == "")) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            , _("Invalid database name"));
        retcd = -1;
    }

    if ((app->cfg->database_type != "mariadb") &&
        (app->cfg->database_type != "postgresql") &&
        (app->cfg->database_type != "sqlite3") &&
        (app->cfg->database_type != "")) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Invalid database_type %s")
            , app->cfg->database_type.c_str());
        retcd = -1;
    }

    if (((app->cfg->database_type == "mariadb") ||
         (app->cfg->database_type == "postgresql")) &&
         (app->cfg->database_port == 0)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Must specify database port for mariadb/pgsql"));
        retcd = -1;
    }

    if ((app->cfg->database_type == "sqlite3") &&
        (app->cfg->database_dbname == "")) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Must specify database name for sqlite3"));
        retcd = -1;
    }

    if ((app->cfg->database_type != "") && (retcd == -1)) {
        MOTPLS_LOG(ERR, TYPE_DB, NO_ERRNO
            ,_("Database functionality disabled."));
        app->cfg->database_type = "";
    }

}

void cls_dbse::dbse_clean()
{
    int delcnt, indx, camindx;
    std::string sql, delimit;
    struct stat statbuf;
    vec_files flst;

    for (camindx=0;camindx<app->cam_cnt;camindx++) {
        if (check_exit() == true) {
            return;
        }

        sql  = " select * from motionplus ";
        sql += " where device_id = ";
        sql += std::to_string(app->cam_list[camindx]->cfg->device_id);
        sql += " order by file_dtl, file_tml;";
        filelist_get(sql, flst);

        delcnt = 0;
        sql = "";
        for (indx=0;indx<flst.size();indx++) {
            if (check_exit() == true) {
                return;
            }
            if (stat(flst[indx].full_nm.c_str(), &statbuf) != 0) {
                if (sql == "") {
                    sql  = " delete from motionplus ";
                    sql += " where record_id in (";
                    delimit = " ";
                    delcnt = 0;
                }
                sql += delimit + std::to_string(flst[indx].record_id);
                delimit = ",";
                delcnt++;
            }
            if (delcnt == 20) {
                sql += ");";
                exec_sql(sql);
                sql = "";
                delcnt = 0;
            }
        }
        if (delcnt != 0) {
            sql += ");";
            exec_sql(sql);
        }
    }

    if (app->cfg->database_type == "sqlite3") {
        sql = " vacuum;";
        exec_sql(sql);
    }

}

void cls_dbse::startup()
{
    is_open = false;
    dbse_edits();
    dbse_open();
}

bool cls_dbse::check_exit()
{
    if ((handler_stop == true) || (finish == true)) {
        return true;
    }
    return false;
}

void cls_dbse::timing()
{
    int waitcnt;

    waitcnt = 0;
    while (waitcnt < 30) {
        if (check_exit() == true) {
            return;
        }
        SLEEP(1,0);
        waitcnt++;
    }
}

void cls_dbse::handler()
{
    struct timespec ts2;
    struct tm lcl_tm;
    int hr_cur, hr_prev;

    mythreadname_set("dl", 0, "dbsl");

    hr_prev = 0;
    while (check_exit() == false) {
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        localtime_r(&ts2.tv_sec, &lcl_tm);
        hr_cur = lcl_tm.tm_hour;
        if (hr_cur != hr_prev) {
            dbse_clean();
            hr_prev = hr_cur;
        }
        timing();
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Database handler closed"));

    handler_running = false;
    pthread_exit(NULL);
}

void cls_dbse::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        restart = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &dbse_handler, this);
        if (retcd != 0) {
            MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start database handler thread."));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_dbse::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < app->cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == app->cfg->watchdog_tmo) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of database handler failed"));
            if (app->cfg->watchdog_kill > 0) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,app->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < app->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == app->cfg->watchdog_kill) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
    }
}

cls_dbse::cls_dbse(cls_motapp *p_app)
{
    app = p_app;

    pthread_mutex_init(&mutex_dbse, nullptr);
    restart = false;
    finish = false;
    handler_running = false;
    handler_stop = true;

    pthread_mutex_lock(&mutex_dbse);
        startup();
    pthread_mutex_unlock(&mutex_dbse);

    handler_startup();

}

cls_dbse::~cls_dbse()
{
    handler_shutdown();
    shutdown();
    pthread_mutex_destroy(&mutex_dbse);
}
