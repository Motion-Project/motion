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

#ifndef _INCLUDE_SCHEDULE_HPP_
#define _INCLUDE_SCHEDULE_HPP_

class cls_schedule {
    public:
        cls_schedule(cls_motapp *p_app);
        ~cls_schedule();

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();

        bool    restart;
        bool    finish;

    private:
        cls_motapp          *app;

        int watchdog;

        void handler_startup();
        void handler_shutdown();
        void timing();
        void cleandir_cam(cls_camera *p_cam);
        void cleandir_run(cls_camera *p_cam);
        void cleandir_remove(std::string sql, bool removedir);
        void cleandir_remove_dir(std::string dirnm);
        void cleandir_sql(int device_id, std::string &sql, struct timespec ts);
        void schedule_cam(cls_camera *p_cam);

};

#endif /*_INCLUDE_SCHEDULE_HPP_*/
