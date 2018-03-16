
/*
 *    translate.c
 *
 *    Translations for Web User control interface.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 */

#include <locale.h>
#include "motion.h"
#include "translate.h"

void translate_locale_chg(const char *langcd){
#ifdef HAVE_INTL

    setenv ("LANGUAGE", langcd, 1);
    /* Invoke external function to change locale*/
    ++_nl_msg_cat_cntr;

#else
    if (langcd != NULL) MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
#endif
}

void translate_init(void){
#ifdef HAVE_INTL

    setlocale (LC_ALL, "");

    //translate_locale_chg("li");

    bindtextdomain ("motion", LOCALEDIR);
    bind_textdomain_codeset ("motion", "UTF-8");
    textdomain ("motion");

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"%s",_("Language: English"));

#else
    /* This avoids a unused function warning */
    translate_locale_chg("en");
#endif
}

