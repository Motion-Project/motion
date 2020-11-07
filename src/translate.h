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

/*
 *      translate.h
 *
 *      Include file for translate.c
 *
 */
#ifndef _INCLUDE_TRANSLATE_H_
#define _INCLUDE_TRANSLATE_H_

extern int nls_enabled;

#ifdef HAVE_GETTEXT
    #include <libintl.h>
    extern int  _nl_msg_cat_cntr;    /* Required for changing the locale dynamically */
#endif

#define _(STRING) translate_text(STRING)

char* translate_text(const char *msgid);
void translate_init(void);
void translate_locale_chg(const char *langcd);

#endif // _INCLUDE_TRANSLATE_H_
