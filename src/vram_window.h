/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_VRAM_WINDOW_H
#define GBCC_VRAM_WINDOW_H

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <epoxy/gl.h>
#endif
#include <stdint.h>

#define VRAM_WINDOW_WIDTH_TILES (24)
#define VRAM_WINDOW_HEIGHT_TILES (32)

#define VRAM_WINDOW_WIDTH (8*VRAM_WINDOW_WIDTH_TILES)
#define VRAM_WINDOW_HEIGHT (8*VRAM_WINDOW_HEIGHT_TILES)
#define VRAM_WINDOW_SIZE (VRAM_WINDOW_WIDTH * VRAM_WINDOW_HEIGHT)

struct gbcc;

struct gbcc_vram_window {
	uint32_t x;
	uint32_t y;
	int32_t width;
	int32_t height;
	uint32_t buffer[VRAM_WINDOW_SIZE];
	struct {
		GLuint vbo;
		GLuint vao;
		GLuint ebo;
		GLuint fbo;
		GLuint fbo_texture;
		GLuint rbo;
		GLuint texture;
		GLuint shader;
		GLuint flip_shader;
	} gl;
	bool initialised;
};

void gbcc_vram_window_initialise(struct gbcc *gbc);
void gbcc_vram_window_deinitialise(struct gbcc *gbc);
void gbcc_vram_window_update(struct gbcc *gbc);
void gbcc_vram_window_clear(void);

#endif /* GBCC_VRAM_WINDOW_H */
