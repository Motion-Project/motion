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
 *    Translations for Web User control interface.
 *
 *
 */

#include <locale.h>
#include "motion.h"
#include "logger.h"
#include "translate.h"

int nls_enabled;

void translate_locale_chg(const char *langcd)
{
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
        if (langcd != NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
        }
    #endif
}

void translate_init(void)
{
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

char* translate_text(const char *msgid)
{
    #ifdef HAVE_GETTEXT
        if (nls_enabled) {
            return (char*)gettext(msgid);
        } else {
            return (char*)msgid;
        }
    #else
        return (char*)msgid;
    #endif
}
