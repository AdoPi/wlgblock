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


#ifndef GBCC_H
#define GBCC_H

#ifdef __ANDROID__
#define ANDROID_INLINE __attribute__((always_inline))
#else
#define ANDROID_INLINE
#endif

#include "audio.h"
#include "core.h"
#include "camera.h"
#include "menu.h"
#include "window.h"
#include "vram_window.h"

struct gbcc {
	struct gbcc_core core;
	struct gbcc_window window;
	struct gbcc_vram_window vram_window;
	struct gbcc_audio audio;
	struct gbcc_menu menu;
	struct gbcc_camera_platform camera;
	
	char save_directory[4096];
	char default_shader[32];
	float turbo_speed;
	bool quit;
	bool pause;
	int8_t save_state;
	int8_t load_state;
	bool background_play;
	bool has_focus;
	bool autoresume;
	bool autosave;
	bool fractional_scaling;
	bool frame_blending;
	bool interlacing;
	bool vram_display;
	bool show_fps;

	// animation
	float time; 
	bool animating;

};

void *gbcc_emulation_loop(void *_gbc);

#endif /* GBCC_H */
