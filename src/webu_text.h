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

#ifndef _INCLUDE_WEBU_TEXT_H_
#define _INCLUDE_WEBU_TEXT_H_


void webu_text_badreq(struct webui_ctx *webui);
void webu_text_main(struct webui_ctx *webui);
void webu_text_status(struct webui_ctx *webui);
void webu_text_connection(struct webui_ctx *webui);
void webu_text_list(struct webui_ctx *webui);
void webu_text_get_query(struct webui_ctx *webui);

#endif
