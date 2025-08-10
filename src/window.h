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

#ifndef GBCC_WINDOW_H
#define GBCC_WINDOW_H

#include "constants.h"
#include "fontmap.h"
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <epoxy/gl.h>
#endif
#include <stdint.h>
#include <time.h>

#define MSG_BUF_SIZE 128

struct gbcc;

struct shader {
	const char *name;
	GLuint program;
};

struct fps_counter {
	uint64_t last_frame;
	struct timespec last_time;
	float dt;
	uint8_t last_ly;
	float fps;
	float previous[5];
	int idx;
};

struct gbcc_window {
	struct gbcc_fontmap font;
	uint32_t x;
	uint32_t y;
	int32_t width;
	int32_t height;
	float scale;
	uint32_t buffer[GBC_SCREEN_SIZE];
	uint32_t last_buffer[GBC_SCREEN_SIZE];
	struct {
		GLuint vbo_banner;
		GLuint vbo;
		GLuint vao;
		GLuint ebo;
		GLuint fbo;
		GLuint fbo_texture;
		GLuint last_frame_texture;
		GLuint rbo;
		GLuint texture;
		GLuint lut_texture;
		GLuint base_shader;
		int cur_shader;
		struct shader shaders[4];
	} gl;
	struct fps_counter fps;
	struct {
		char text[MSG_BUF_SIZE];
		uint8_t lines;
		int64_t time_left;
	} msg;
	bool screenshot;
	bool raw_screenshot;
	bool initialised;
};

void gbcc_window_initialise(struct gbcc *gbc);
void gbcc_window_deinitialise(struct gbcc *gbc);
void gbcc_window_update(struct gbcc *gbc);
void gbcc_window_clear(void);
void gbcc_window_show_message(struct gbcc *gbc, const char *msg, unsigned seconds, bool pad);
void gbcc_window_use_shader(struct gbcc *gbc, const char *name);
void gbcc_load_shader(GLuint shader, const char *filename);
GLuint gbcc_create_shader_program(const char *vert, const char *frag);

#endif /* GBCC_WINDOW_H */
