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
*/

#ifndef _INCLUDE_WEBU_TEXT_HPP_
#define _INCLUDE_WEBU_TEXT_HPP_
    class cls_webu_text {
        public:
            cls_webu_text(cls_webu_ans *p_webua);
            ~cls_webu_text();
            void main();
        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;
            cls_webu_post   *webu_post;
            void status_vars(int indx_cam);
            void status();
            void connection();
    };

#endif /* _INCLUDE_WEBU_TEXT_HPP_ */
