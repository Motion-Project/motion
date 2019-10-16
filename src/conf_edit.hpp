/*    dbse.hpp
 *
 *    This file is part of the Motion application
 *    Copyright (C) 2019  Motion-Project Developers(motion-project.github.io)
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public
 *    License along with this library; if not, write to the
 *    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *    Boston, MA  02110-1301, USA.
*/

/*
 *    Header files for the database functionality.
 */
#ifndef _INCLUDE_CONF_EDIT_H_
#define _INCLUDE_CONF_EDIT_H_

void conf_edit_set(struct ctx_motapp *motapp, int threadnbr, char *cmd, char *arg1);
int conf_edit_get(struct ctx_cam *cam, const char *cmd, char *arg1 , enum PARM_CAT pcat);
void conf_edit_dflt_app(struct ctx_motapp *motapp);
void conf_edit_dflt_cam(struct ctx_cam *cam);

#endif