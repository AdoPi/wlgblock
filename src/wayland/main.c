/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 * All modifications made to the code to
 * transform this project into a locker 
 * are under the GPLv3 license.
 * Copyright (C) 2025 Adonis Najimi
 *
 * Licensed under the GPLv3 License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/license/gpl-3-0
 *
 */
#include "../apu.h"
#include "../args.h"
#include "../audio.h"
#include "../camera.h"
#include "../cheats.h"
#include "../config.h"
#include "../cpu.h"
#include "../debug.h"
#include "../memory.h"
#include "../palettes.h"
#include "../paths.h"
#include "../save.h"
#include "../time_diff.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>
#include "../ext-session-lock-v1-client-protocol.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-egl.h>

struct display {
    struct wl_display *wl_display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct wl_output *output;
    struct ext_session_lock_manager_v1 *lock_mgr;
};

struct lock_surface {
    struct gbcc gbcc;
    struct display *disp;
    struct ext_session_lock_v1 *lock;
    struct ext_session_lock_surface_v1 *surf;
    struct wl_surface *wl_surf;
    uint32_t width, height;
    EGLDisplay egl_dpy;
    EGLContext egl_ctx;
    EGLSurface egl_surf;
    struct wl_egl_window *egl_win;
    GLuint program, vbo;
    GLint uni_offset;
    float offset, dir;
    bool configured;
    bool locked;
    bool running;
    bool should_quit;
};


static void setup_gl(struct lock_surface *ls) {
    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    ls->egl_dpy = eglGetDisplay((EGLNativeDisplayType)ls->disp->wl_display);
    if (ls->egl_dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay failed\n");
        exit(1);
    }

    if (!eglInitialize(ls->egl_dpy, NULL, NULL)) {
        fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
        exit(1);
    }

    EGLConfig cfg;
    EGLint num;
    if (!eglChooseConfig(ls->egl_dpy, cfg_attr, &cfg, 1, &num) || num < 1) {
        fprintf(stderr, "eglChooseConfig failed: 0x%x\n", eglGetError());
        exit(1);
    }

    // create surface with egl_win

    ls->egl_surf = eglCreateWindowSurface(ls->egl_dpy, cfg, (EGLNativeWindowType)ls->egl_win, NULL);
    if (ls->egl_surf == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        exit(1);
    }

    ls->egl_ctx = eglCreateContext(ls->egl_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ls->egl_ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed: 0x%x\n", eglGetError());
        exit(1);
    }

    if (!eglMakeCurrent(ls->egl_dpy, ls->egl_surf, ls->egl_surf, ls->egl_ctx)) {
        fprintf(stderr, "eglMakeCurrent failed: 0x%x\n", eglGetError());
        exit(1);
    }

    gbcc_window_initialise(&ls->gbcc);
    return;
}



static void registry_global(void *data, struct wl_registry *reg,
                            uint32_t id, const char *iface, uint32_t ver) {
    struct display *d = data;
    if (strcmp(iface, "ext_session_lock_manager_v1") == 0)
        d->lock_mgr = wl_registry_bind(reg, id,
            &ext_session_lock_manager_v1_interface, 1);
    else if (strcmp(iface, "wl_compositor") == 0)
        d->compositor = wl_registry_bind(reg, id,
            &wl_compositor_interface, 4);
    else if (strcmp(iface, "wl_seat") == 0)
        d->seat = wl_registry_bind(reg, id,
            &wl_seat_interface, 1);
    else if (strcmp(iface, "wl_output") == 0)
        d->output = wl_registry_bind(reg, id,
            &wl_output_interface, 2);
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    NULL
};

static void lock_locked(void *data, struct ext_session_lock_v1 *lock) {
	struct lock_surface *ls = data;
	ls->locked = true;
}
static void lock_finished(void *data, struct ext_session_lock_v1 *lock) {
    fprintf(stderr, "Lock failed\n");
    exit(1);
}
static const struct ext_session_lock_v1_listener lock_listener = {
    lock_locked, lock_finished
};


static void surf_configure(void *data,
                           struct ext_session_lock_surface_v1 *surf,
                           uint32_t serial, uint32_t w, uint32_t h) {
    struct lock_surface *ls = data;
    ls->width = w; ls->height = h;

    printf("surf_configure() called with size %ux%u, serial=%u\n", w, h, serial);
    ext_session_lock_surface_v1_ack_configure(surf, serial);


    ls->egl_win = wl_egl_window_create(ls->wl_surf, w, h);
    if (!ls->egl_win) {
	    fprintf(stderr, "wl_egl_window_create failed\n");
	    exit(1);
    }

    // Setup EGL 
    setup_gl(ls);

    ls->configured = true;
}

static const struct ext_session_lock_surface_v1_listener surf_listener = {
    surf_configure
};


static void draw_gbcc(struct lock_surface *ls) {
    struct gbcc *gbc = &ls->gbcc;
    gbc->window.width = ls->width;
    gbc->window.height = ls->height;

    gbcc_window_update(&ls->gbcc);
    eglSwapBuffers(ls->egl_dpy, ls->egl_surf);
}

static void seat_handle_capabilities(void *d, struct wl_seat *s, uint32_t caps) {}
static void seat_handle_name(void *d, struct wl_seat *s, const char *n) {}
static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities, seat_handle_name
};

static void keyboard_keymap(void *d, struct wl_keyboard *k,
                            uint32_t format, int fd, uint32_t size) {}
static void keyboard_enter(void *d, struct wl_keyboard *k,
                           uint32_t serial, struct wl_surface *s,
                           struct wl_array *keys) {}
static void keyboard_leave(void *d, struct wl_keyboard *k,
                           uint32_t serial, struct wl_surface *s) {}
static void keyboard_key(void *d, struct wl_keyboard *k,
                         uint32_t serial, uint32_t time,
                         uint32_t key, uint32_t state) {

    struct lock_surface *ls = d;
    bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

    // process input
    switch (key) {
        case 44: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_A, pressed);
            break;
        case 45: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_B, pressed);
            break;
        case 28: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_START, pressed);
            break;
        case 57: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_SELECT, pressed);
            break;
        case 103: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_UP, pressed);
            break;
        case 108: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_DOWN, pressed);
            break;
        case 105: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_LEFT, pressed);
            break;
        case 106: 
            gbcc_input_process_key(&ls->gbcc, GBCC_KEY_RIGHT, pressed);
            break;
        case 1: // ESC
            if (pressed) {
		    ls->should_quit = true;
            }
            break;
    }

}
static void keyboard_modifiers(void *d, struct wl_keyboard *k,
                               uint32_t serial,
                               uint32_t depressed, uint32_t latched,
                               uint32_t locked, uint32_t group) {}
static void keyboard_repeat_info(void *d, struct wl_keyboard *k,
                                 int32_t rate, int32_t delay) {}
static const struct wl_keyboard_listener kbd_listener = {
    keyboard_keymap, keyboard_enter, keyboard_leave,
    keyboard_key, keyboard_modifiers, keyboard_repeat_info
};


int main(int argc, char **argv) {

    struct lock_surface ls = { .running = true};

    // init gbcc
    struct gbcc *gbc = &ls.gbcc;
    if (!gbcc_parse_args(gbc, true, argc, argv)) {
	    exit(EXIT_FAILURE);
    }
    gbc->quit = false;

    gbcc_audio_initialise(gbc, 96000, 2048);

    pthread_t emu_thread;
    pthread_create(&emu_thread, NULL, gbcc_emulation_loop, gbc);
    pthread_setname_np(emu_thread, "EmulationThread");

    printf("success initialization of gbcc\n");

    struct display d = {0};
    d.wl_display = wl_display_connect(NULL);
    d.registry = wl_display_get_registry(d.wl_display);
    wl_registry_add_listener(d.registry, &registry_listener, &d);
    wl_display_roundtrip(d.wl_display);

    if(!d.lock_mgr || !d.compositor || !d.seat || !d.output) {
        fprintf(stderr,"Missing Wayland globals\n");
        return 1;
    }
    ls.disp = &d; 
    struct ext_session_lock_v1 *lock = ext_session_lock_manager_v1_lock(d.lock_mgr);
    ls.lock = lock;
//    struct lock_surface ls = { .disp = &d, .lock = lock, .running = true};

    ext_session_lock_v1_add_listener(lock, &lock_listener, &ls);

    // printf("First roundtrip\n");
    wl_display_roundtrip(d.wl_display);

    ls.wl_surf = wl_compositor_create_surface(d.compositor);
    ls.surf = ext_session_lock_v1_get_lock_surface(lock, ls.wl_surf, d.output);
    ext_session_lock_surface_v1_add_listener(ls.surf, &surf_listener, &ls);

    // printf("Second roundtrip\n");
    wl_display_roundtrip(d.wl_display);

    wl_seat_add_listener(d.seat, &seat_listener, &d);
    struct wl_keyboard *kbd = wl_seat_get_keyboard(d.seat);
    wl_keyboard_add_listener(kbd, &kbd_listener, &ls);

    double time = 0;
    double elapsed_time = 0;
    clock_t start,end;
    while(ls.running && wl_display_dispatch(d.wl_display) != -1) {
	start = clock();
	wl_display_dispatch_pending(ls.disp->wl_display);
        draw_gbcc(&ls);

	uint8_t m = gbcc_memory_read(&gbc->core, 0xDCC7);
	if (m == 99) {
		ls.should_quit = true;
	}
	if (ls.should_quit) {
		time += elapsed_time;
		if (time >= 0.6 && time < 1.3) {
			gbc->animating = true;
		} else if (time >= 1.4) {
			ls.running = false;
			gbc->quit = true;
			ext_session_lock_v1_unlock_and_destroy(ls.lock);
		}
	}
	usleep(16000); 
	wl_display_flush(d.wl_display);
	end = clock();
	elapsed_time = ((double)(end - start)) / CLOCKS_PER_SEC; 
    }

    wl_display_flush(d.wl_display);

    // end gbcc
    sem_post(&gbc->core.ppu.vsync_semaphore);
    pthread_join(emu_thread, NULL);
    gbcc_audio_destroy(gbc);

    if (ls.egl_surf != EGL_NO_SURFACE)
	    eglDestroySurface(ls.egl_dpy, ls.egl_surf);
    if (ls.egl_ctx != EGL_NO_CONTEXT)
	    eglDestroyContext(ls.egl_dpy, ls.egl_ctx);
    if (ls.egl_dpy != EGL_NO_DISPLAY)
	    eglTerminate(ls.egl_dpy);
    if (ls.egl_win)
	    wl_egl_window_destroy(ls.egl_win);
    if (kbd)
	    wl_keyboard_destroy(kbd);
    if (ls.wl_surf)
	    wl_surface_destroy(ls.wl_surf);

    wl_display_disconnect(d.wl_display);

    return 0;
}

