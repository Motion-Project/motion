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

#ifndef _INCLUDE_NETCAM_JPEG_H
#define _INCLUDE_NETCAM_JPEG_H

int netcam_proc_jpeg(struct netcam_context *netcam,  struct image_data *img_data);
void netcam_fix_jpeg_header(struct netcam_context *netcam);
void netcam_get_dimensions(struct netcam_context *netcam);


#endif
