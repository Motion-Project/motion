/*
 *      translate.h
 *
 *      Include file for translate.c
 *
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_TRANSLATE_H_
#define _INCLUDE_TRANSLATE_H_

extern int nls_enabled;

#ifdef HAVE_GETTEXT
#   include <libintl.h>
    extern int  _nl_msg_cat_cntr;    /* Required for changing the locale dynamically */
#endif

#define _(STRING) translate_text(STRING)

    char* translate_text(const char *msgid);
    void translate_init(void);
    void translate_locale_chg(const char *langcd);

#endif // _INCLUDE_TRANSLATE_H_
