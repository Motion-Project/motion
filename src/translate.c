/*
 *    Translations for Web User control interface.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 */

#include <locale.h>
#include "motion.h"
#include "translate.h"

int nls_enabled;

void translate_locale_chg(const char *langcd){
#ifdef HAVE_GETTEXT
    /* This routine is for development testing only.  It is not used for
     * regular users because once this locale is change, it changes the
     * whole computer over to the new locale.  Therefore, we just return
     */
    return;

    setenv ("LANGUAGE", langcd, 1);
    /* Invoke external function to change locale*/
    ++_nl_msg_cat_cntr;
#else
    if (langcd != NULL) MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
#endif
}

void translate_init(void){
#ifdef HAVE_GETTEXT
    /* Set the flag to enable native language support */
    nls_enabled = 1;

    setlocale (LC_ALL, "");

    //translate_locale_chg("li");
    translate_locale_chg("es");

    bindtextdomain ("motion", LOCALEDIR);
    bind_textdomain_codeset ("motion", "UTF-8");
    textdomain ("motion");

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Language: English"));

#else
    /* Disable native language support */
    nls_enabled = 0;

    /* This avoids a unused function warning */
    translate_locale_chg("en");
#endif
}

char* translate_text(const char *msgid){
#ifdef HAVE_GETTEXT
    if (nls_enabled){
        return (char*)gettext(msgid);
    } else {
        return (char*)msgid;
    }
#else
    return (char*)msgid;
#endif
}
