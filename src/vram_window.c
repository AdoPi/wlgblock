/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "gbcc.h"
#include "bit_utils.h"
#include "constants.h"
#include "debug.h"
#include "memory.h"
#include "nelem.h"
#include "window.h"
#include "vram_window.h"
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <epoxy/gl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifndef SHADER_PATH
#define SHADER_PATH "shaders/"
#endif

void gbcc_vram_window_initialise(struct gbcc *gbc)
{
	struct gbcc_vram_window *win = &gbc->vram_window;
	GLint read_framebuffer = 0;
	GLint draw_framebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

	/* Compile and link the shader programs */
	win->gl.shader = gbcc_create_shader_program(
			SHADER_PATH "vert.vert",
			SHADER_PATH "nothing.frag"
			);

	win->gl.flip_shader = gbcc_create_shader_program(
			SHADER_PATH "flipped.vert",
			SHADER_PATH "nothing.frag"
			);


	/* Create a vertex buffer for a quad filling the screen */
	float vertices[] = {
		//  Position      Texcoords
		-1.0f,  1.0f, 0.0f, 0.0f, // Top-left
		1.0f,  1.0f,  1.0f, 0.0f, // Top-right
		1.0f, -1.0f,  1.0f, 1.0f, // Bottom-right
		-1.0f, -1.0f, 0.0f, 1.0f  // Bottom-left
	};

	glGenBuffers(1, &win->gl.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, win->gl.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	/* Create a vertex array and enable vertex attributes for the shaders.
	 * TODO: why do we only need to do this once? */
	glGenVertexArrays(1, &win->gl.vao);
	glBindVertexArray(win->gl.vao);

	GLint posAttrib = glGetAttribLocation(win->gl.shader, "position");
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
	glEnableVertexAttribArray(posAttrib);

	GLint texAttrib = glGetAttribLocation(win->gl.shader, "texcoord");
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)(2*sizeof(float)));
	glEnableVertexAttribArray(texAttrib);

	glBindVertexArray(0);

	/* Create the element buffer object that will actually be drawn via
	 * glDrawElements(). */
	GLuint elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenBuffers(1, &win->gl.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, win->gl.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Framebuffer for post-processing */
	glGenFramebuffers(1, &win->gl.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, win->gl.fbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glGenTextures(1, &win->gl.fbo_texture);
	glBindTexture(GL_TEXTURE_2D, win->gl.fbo_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, win->gl.fbo_texture, 0);

	/* We don't care about the depth and stencil, so just use a
	 * renderbuffer */
	glGenRenderbuffers(1, &win->gl.rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, win->gl.rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, win->gl.rbo);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		gbcc_log_error("Framebuffer is not complete!\n");
		exit(EXIT_FAILURE);
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer);

	/* This is the texture we're going to render the raw screen output to
	 * before post-processing */
	glGenTextures(1, &win->gl.texture);
	glBindTexture(GL_TEXTURE_2D, win->gl.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Bind the actual bits we'll be using to render */
	glBindVertexArray(win->gl.vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, win->gl.ebo);

	win->initialised = true;
}

void gbcc_vram_window_deinitialise(struct gbcc *gbc)
{
	struct gbcc_vram_window *win = &gbc->vram_window;

	glDeleteBuffers(1, &win->gl.vbo);
	glDeleteVertexArrays(1, &win->gl.vao);
	glDeleteBuffers(1, &win->gl.ebo);
	glDeleteFramebuffers(1, &win->gl.fbo);
	glDeleteTextures(1, &win->gl.fbo_texture);
	glDeleteRenderbuffers(1, &win->gl.rbo);
	glDeleteTextures(1, &win->gl.texture);
	glDeleteProgram(win->gl.shader);
	win->initialised = false;
}

void gbcc_vram_window_update(struct gbcc *gbc)
{
	struct gbcc_vram_window *win = &gbc->vram_window;

	for (int bank = 0; bank < 2; bank++) {
		for (int j = 0; j < VRAM_WINDOW_HEIGHT_TILES / 2; j++) {
			for (int i = 0; i < VRAM_WINDOW_WIDTH_TILES; i++) {
				for (int y = 0; y < 8; y++) {
					uint8_t lo = gbc->core.memory.vram_bank[bank][16 * (j * VRAM_WINDOW_WIDTH_TILES + i) + 2*y];
					uint8_t hi = gbc->core.memory.vram_bank[bank][16 * (j * VRAM_WINDOW_WIDTH_TILES + i) + 2*y + 1];
					for (uint8_t x = 0; x < 8; x++) {
						uint8_t colour = (uint8_t)(check_bit(hi, 7 - x) << 1u) | check_bit(lo, 7 - x);
						uint32_t p = 0;
						switch (colour) {
							case 3:
								p = 0x000000ffu;
								break;
							case 2:
								p = 0x6b7353ffu;
								break;
							case 1:
								p = 0x8b956dffu;
								break;
							case 0:
								p = 0xc4cfa1ffu;
								break;
						}
						win->buffer[8 * (VRAM_WINDOW_WIDTH * (j + bank * VRAM_WINDOW_HEIGHT_TILES / 2) + i) + VRAM_WINDOW_WIDTH * y + x] = p;
					}
				}
			}
		}
	}

	
	GLint read_framebuffer = 0;
	GLint draw_framebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

	for (size_t i = 0; i < N_ELEM(win->buffer); i++) {
		uint32_t tmp = win->buffer[i];
		win->buffer[i] = (tmp & 0xFFu) << 24u
				| (tmp & 0xFF00u) << 8u
				| (tmp & 0xFF0000u) >> 8u
				| (tmp & 0xFF000000u) >> 24u;
	}


	/* First pass - render the gbc screen to the framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, win->gl.fbo);
	glViewport(0, 0, VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, win->gl.texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT, GL_RGBA,
			GL_UNSIGNED_BYTE, (GLvoid *)win->buffer);
	glUseProgram(win->gl.shader);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	/* Second pass - render the framebuffer to the screen */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer);

	int scale = min(win->width / VRAM_WINDOW_WIDTH, win->height / VRAM_WINDOW_HEIGHT);
	int width = scale * VRAM_WINDOW_WIDTH;
	int height = scale * VRAM_WINDOW_HEIGHT;
	win->x = (uint32_t)(win->width - width) / 2;
	win->y = (uint32_t)(win->height - height) / 2;
	glViewport(win->x, win->y, width, height);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, win->gl.fbo_texture);
	glUseProgram(win->gl.flip_shader);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
