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

#ifndef _INCLUDE_DRAW_HPP_
#define _INCLUDE_DRAW_HPP_
    #define ASCII_MAX 127
    #define NEWLINE "\\n"

    class cls_draw {
        public:
            cls_draw(cls_camera *p_cam);
            ~cls_draw();
            int text(u_char *image
                , int width, int height, int startx, int starty
                , const char *text, int factor);
            void locate();
            void smartmask();
            void fixed_mask();
            void largest_label();

        private:
            cls_camera *cam;

            u_char *char_arr_ptr[ASCII_MAX];

            int textn(u_char *image
                , int startx,  int starty,  int width
                , const char *text, int len, int factor);
            void init_chars(void);
            void init_scale();
            void location(ctx_coord *cent
                , ctx_images *imgs, int width, u_char *new_var);
            void red_location(ctx_coord *cent
                , ctx_images *imgs, int width, u_char *new_var);

    };

#endif /* _INCLUDE_DRAW_HPP_ */