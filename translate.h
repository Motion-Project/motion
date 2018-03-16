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


#ifdef HAVE_INTL

#   include <libintl.h>
#   define _(STRING) gettext(STRING)

    /* Required for changing the locale dynamically */
    extern int  _nl_msg_cat_cntr;
#else
#   define _(STRING) STRING
#endif

    void translate_init(void);
    void translate_locale_chg(const char *langcd);

#endif // _INCLUDE_TRANSLATE_H_



