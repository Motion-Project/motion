/*
 *    sdl.c
 *
 *    sdl functions for motion.
 *    Copyright 2009 by Peter Holik (peter@holik.at)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 */
#include "sdl.h"
#include <SDL/SDL.h>

static int cur_width;
static int cur_height;
static int is_full_screen;
static int fs_screen_width;
static int fs_screen_height;

static SDL_Surface *screen;
static SDL_Overlay *overlay;

static int sdl_video_open(int width, int height)
{
    int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
    int w,h;

    if (is_full_screen) flags |= SDL_FULLSCREEN;
    else                flags |= SDL_RESIZABLE;

    if (is_full_screen && fs_screen_width) {
        w = fs_screen_width;
        h = fs_screen_height;
    } else if (width > fs_screen_width || height > fs_screen_height) {
        w = fs_screen_width;
        h = fs_screen_height;
    } else {
        w = width;
        h = height;
    }
    /* 32 because framebuffer is usually initalized to 8 and
       you have to use fbset with -depth to make it working */
    screen = SDL_SetVideoMode(w, h, 32, flags);

    if (!screen) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,"%s: Unable to set video mode: %s",
                   SDL_GetError());
        return -1;
    }

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: SDL dimension %d x %d fullscreen %d BytesPerPixel %d",
                screen->w, screen->h, is_full_screen,
               screen->format->BytesPerPixel);

    SDL_WM_SetCaption("motion", "motion");
    SDL_ShowCursor(SDL_DISABLE);

    if (cur_width != width || cur_height != height) {
        cur_width  = width;
        cur_height = height;

        if (overlay) SDL_FreeYUVOverlay(overlay);

        overlay = SDL_CreateYUVOverlay(cur_width, cur_height,
                                   SDL_YV12_OVERLAY, screen);
        if (!overlay) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Could not create overlay: %s",
                        SDL_GetError());
            sdl_stop();
        } else
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: SDL created %dx%dx%d %s overlay",
                        overlay->w,overlay->h,overlay->planes,
                       overlay->hw_overlay?"hardware":"software");
    }
    return overlay == NULL;
}

int sdl_start(int width, int height)
{
    //putenv("SDL_NOMOUSE=1");
    setenv("SDL_NOMOUSE", "1", 1);

    if (screen) return 0;

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: SDL start");

    if (SDL_Init(SDL_INIT_VIDEO)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Could not initialize SDL - %s",
                    SDL_GetError());
        return -1;
    }
    const SDL_VideoInfo *vi = SDL_GetVideoInfo();
    fs_screen_width  = vi->current_w;
    fs_screen_height = vi->current_h;

    if (sdl_video_open(width, height)) return -1;

    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
    SDL_EventState(SDL_KEYUP, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_JOYBUTTONUP, SDL_IGNORE);
    SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYBALLMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYHATMOTION, SDL_IGNORE);
    SDL_EventState(SDL_VIDEORESIZE, SDL_IGNORE);

    return 0;
}

void sdl_put(unsigned char *image, int width, int height)
{
    SDL_Event event;

    if (screen && overlay) {
        SDL_Rect rect;
        float aspect_ratio = (float)width / height;
        int pic_width, pic_height;

        if (width != cur_width || height != cur_height)
            sdl_video_open(width, height);

        if (SDL_MUSTLOCK(screen))
            if (SDL_LockSurface(screen) < 0) return;

        SDL_LockYUVOverlay(overlay);
        memcpy(overlay->pixels[0], image, width * height);
        memcpy(overlay->pixels[2], image + (width * height), (width * height / 4));
        memcpy(overlay->pixels[1], image + (width * height * 5 / 4), (width * height / 4));
        SDL_UnlockYUVOverlay(overlay);

        if (SDL_MUSTLOCK(screen))
            SDL_UnlockSurface(screen);

        pic_height = screen->h;
        pic_width = pic_height * aspect_ratio;
        if (pic_width > screen->w) {
            pic_width = screen->w;
            pic_height = pic_width / aspect_ratio;
        }
        rect.x = (screen->w - pic_width) / 2;
        rect.y = (screen->h - pic_height) / 2;
        rect.w = pic_width;
        rect.h = pic_height;

        if (SDL_DisplayYUVOverlay(overlay, &rect))
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: SDL_DisplayYUVOverlay: %s",
                        SDL_GetError());

        if (SDL_PollEvent(&event)) {
            if ((event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN &&
                 event.key.keysym.sym == SDLK_ESCAPE)))
                sdl_stop();
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f) {
                is_full_screen = !is_full_screen;
                sdl_video_open(width, height);
            }
            else if (event.type == SDL_VIDEORESIZE)
                screen = SDL_SetVideoMode(event.resize.w, event.resize.h,
                                          screen->format->BitsPerPixel,
                                          screen->flags);
        }
    }
}

void sdl_stop(void)
{
    if (screen) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: SDL quit");
        SDL_ShowCursor(SDL_ENABLE);
        if (overlay) {
            SDL_FreeYUVOverlay(overlay);
            overlay = NULL;
        }
        SDL_Quit();
        screen = NULL;
    }
}
