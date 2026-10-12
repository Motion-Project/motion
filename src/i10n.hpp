/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
*/
#ifndef _INCLUDE_I10N_HPP_
#define _INCLUDE_I10N_HPP_

    #ifdef HAVE_GETTEXT
        #include <libintl.h>
        extern int  _nl_msg_cat_cntr;    /* Required for changing the locale dynamically */
    #endif

    #define _(STRING) i10n_text(STRING)

    char* i10n_text(const char *msgid);
    std::string i10n_text(std::string msgid);
    void i10n_set(bool setnls);
    void i10n_init(std::string langcd);
    void i10n_locale_chg(std::string langcd);
    void i10n_log();
    void i10n_config_names();
    void i10n_config_options();

#endif /* _INCLUDE_I10N_HPP_ */