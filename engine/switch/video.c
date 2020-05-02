/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include "sdlport.h"

#include "video.h"
#include "vga.h"
#include "savedata.h"
#include "gfx.h"
#include "videocommon.h"
#include <switch.h>

SDL_Window *window = NULL;

static SDL_Renderer *renderer = NULL;

static SDL_Texture *texture = NULL;

s_videomodes stored_videomodes;

int stretch = 0;

int brightness = 0;

static unsigned pixelformats[4] = {SDL_PIXELFORMAT_INDEX8,
                                   SDL_PIXELFORMAT_BGR565,
                                   SDL_PIXELFORMAT_BGR888,
                                   SDL_PIXELFORMAT_ABGR8888};

static int display_width = 1280;
static int display_height = 720;
static int currently_docked = -1; // -1 means not yet determined
static int isDocked(); // determine if we are docked or not
static void updateResolution(); // updates resolution if docked mode changed

void initSDL(int w, int h) {

    if (window) {
        return;
    }

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK;
        if (SDL_Init(init_flags) < 0) {
            printf("SDL Failed to Init!!!! (%s)\n", SDL_GetError());
            borExit(0);
        }
        SDL_ShowCursor(SDL_DISABLE);
        atexit(SDL_Quit);
    }

    window = SDL_CreateWindow(
            NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return;
    }

    // switch only support software renderer for now
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        printf("SDL_CreateRenderer: %s\n", SDL_GetError());
        return;
    }
}

int SetVideoMode(int w, int h, int bpp, bool gl) {

    printf("SetVideoMode: %i x %i\n", w, h);

    if (savedata.fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, 0, &mode);
        mode.w = w > display_width ? display_width : w;
        mode.h = h > display_height ? display_height : h;
        SDL_SetWindowDisplayMode(window, &mode);
    } else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, w, h);
    }

    return 1;
}

int video_set_mode(s_videomodes videomodes) {

    stored_videomodes = videomodes;

    if (videomodes.hRes == 0 && videomodes.vRes == 0) {
        Term_Gfx();
        return 0;
    }

    videomodes = setupPreBlitProcessing(videomodes);

    // 8-bit color should be transparently converted to 32-bit
    assert(videomodes.pixel == 2 || videomodes.pixel == 4);

    if (!SetVideoMode((int) (videomodes.hRes * videomodes.hScale),
                      (int) (videomodes.vRes * videomodes.vScale),
                      videomodes.pixel * 8, false)) {
        return 0;
    }

    if (savedata.hwfilter ||
        (videomodes.hScale == 1 && videomodes.vScale == 1 && !savedata.fullscreen))
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    else
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTexture(renderer,
                                pixelformats[videomodes.pixel - 1],
                                SDL_TEXTUREACCESS_STREAMING,
                                videomodes.hRes, videomodes.vRes);

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    video_stretch(savedata.stretch);

    return 1;
}

void video_fullscreen_flip() {

    savedata.fullscreen ^= 1;
    if (window) video_set_mode(stored_videomodes);
}

void video_clearscreen() {

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

int isDocked() {
    switch (appletGetOperationMode()) {
        case AppletOperationMode_Handheld:
            return 0;
            break;
        case AppletOperationMode_Docked:
            return 1;
            break;
        default:
            return 0;
    }
}

void updateResolution() {
    int docked = isDocked();
    if ((docked && !currently_docked) || (!docked && currently_docked) || currently_docked == -1) {
        // docked mode has changed, update window size etc.
        if (docked) {
            display_width = 1920;
            display_height = 1080;
            currently_docked = 1;
        } else {
            display_width = 1280;
            display_height = 720;
            currently_docked = 0;
        }
        // remove leftover-garbage on screen
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_SetWindowSize(window, display_width, display_height);
    }
}

void blit() {

    updateResolution(); // update resolution if dock mode has changed
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    if (brightness > 0)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, brightness - 1);
    else if (brightness < 0)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (-brightness) - 1);
    SDL_RenderFillRect(renderer, NULL);

    SDL_RenderPresent(renderer);
}

int video_copy_screen(s_screen *src) {

    // do any needed scaling and color conversion
    s_videosurface *surface = getVideoSurface(src);

    SDL_UpdateTexture(texture, NULL, surface->data, surface->pitch);

    blit();

    return 1;
}

void video_stretch(int enable) {

    printf("video_stretch: %i (%ix%i)\n",
           enable, stored_videomodes.hRes, stored_videomodes.vRes);

    stretch = enable;
    if (window) {
        if (stretch)
            SDL_RenderSetLogicalSize(renderer, 0, 0);
        else {
            SDL_RenderSetLogicalSize(renderer, stored_videomodes.hRes, stored_videomodes.vRes);
        }
    }
}

void video_set_color_correction(int gm, int br) {
    brightness = br;
}

void vga_vwait(void) {
    static int prevtick = 0;
    int now = SDL_GetTicks();
    int wait = 1000 / 60 - (now - prevtick);
    if (wait > 0) {
        SDL_Delay(wait);
    } else SDL_Delay(1);
    prevtick = now;
}
