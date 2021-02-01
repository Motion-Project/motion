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
 *      dbse.h
 *
 *      Headers associated with functions in the dbse.c module.
 *
 */

#ifndef _INCLUDE_DBSE_H
#define _INCLUDE_DBSE_H

void dbse_global_init(struct context **cntlist);
void dbse_global_deinit(struct context **cntlist);

int dbse_init(struct context *cnt, struct context **cntlist);
void dbse_deinit(struct context *cnt);

void dbse_sqlmask_update(struct context *cnt);
void dbse_firstmotion(struct context *cnt);
void dbse_newfile(struct context *cnt, char *filename, int sqltype, struct timeval *tv1);
void dbse_fileclose(struct context *cnt, char *filename, int sqltype, struct timeval *tv1);


#endif
