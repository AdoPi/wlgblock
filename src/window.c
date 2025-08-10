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

#include <stdlib.h>  // pour rand()
#include <time.h>    // pour time()
#include <math.h>

#include "gbcc.h"
#include "colour.h"
#include "constants.h"
#include "debug.h"
#include "fontmap.h"
#include "memory.h"
#include "nelem.h"
#include "screenshot.h"
#include "time_diff.h"
#include "window.h"
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GLES2/gl2.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifndef SHADER_PATH
#define SHADER_PATH "shaders/"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void update_timers(struct gbcc *gbc);

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, NULL, buffer);
        printf("Shader compile error: %s\n", buffer);
        exit(1);
    }
    return shader;
}

GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buffer[512];
        glGetProgramInfoLog(program, 512, NULL, buffer);
        printf("Program link error: %s\n", buffer);
        exit(1);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
} 


    float quad[] = {
        -1, -1, 0, 1,
         1, -1, 1, 1,
        -1,  1, 0, 0,
         1,  1, 1, 0
    };

// Vertex shader
const char* vertex_shader_src =
    "attribute vec2 aPos;\n"
    "attribute vec2 aTex;\n"
    "varying vec2 vTex;\n"
    "void main() {\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "  vTex = aTex;\n"
    "}\n";

// Fragment shader
const char* fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 vTex;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, vTex);\n"
    "}\n";


const char* vert_fade_out = 
"attribute vec2 aPos;\n"
"varying vec2 uv;\n"
"void main() {\n"
"    uv = aPos * 0.5 + 0.5;\n"
"    gl_Position = vec4(aPos, 0.0, 1.0);\n"
"}\n";


const char* frag_fade_out_rainbow =
"precision mediump float;\n"
"varying vec2 uv;\n"
"uniform float time;\n"
"void main() {\n"
"    float fade = clamp(time / 5.0, 0.0, 1.0);\n"
"    vec3 baseColor = vec3(uv.x, uv.y, 0.5);\n"
"    vec3 finalColor = mix(baseColor, vec3(0.0), fade);\n"
"    gl_FragColor = vec4(finalColor, 1.0);\n"
"}\n";

const char* frag_fade_out =
    "precision mediump float;\n"
    "uniform float time;\n"
    "void main() {\n"
    "    float alpha = clamp(time / 1.8, 0.0, 1.0);\n"
    "    vec3 color = vec3(0.0);\n"
    "    gl_FragColor = vec4(color, alpha);\n"
    "}\n";

float vertices[] = {
    // pos     // tex
    -1.0f,  0.8f,  0.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
     1.0f,  0.8f,  1.0f, 0.0f,
};


uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };

GLuint loadTexture(const char *filename) {
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
        fprintf(stderr, "Error loading image %s\n", filename);
        return 0;
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    return textureId;
}

GLfloat quadVertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
};

GLuint simple_program;
GLuint fadeout_program;

GLuint texBanner;
GLuint vboBanner;
GLuint vboFadeout;
GLuint fadeOuttimeLoc;
GLuint fadeOutAposLoc;

void init_fadeout(struct gbcc *gbc) {
	fadeout_program = createProgram(vert_fade_out, frag_fade_out);

	glGenBuffers(1, &vboFadeout);
	glBindBuffer(GL_ARRAY_BUFFER, vboFadeout);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	fadeOutAposLoc = glGetAttribLocation(fadeout_program, "aPos");
	glEnableVertexAttribArray(fadeOutAposLoc);
	glVertexAttribPointer(fadeOutAposLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);

	fadeOuttimeLoc = glGetUniformLocation(fadeout_program, "time");

}

float currentTime;
void render_fadeout(struct gbcc *gbc) {
	if (!gbc->animating)
		return;

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	struct fps_counter *fps = &gbc->window.fps;

	glBindBuffer(GL_ARRAY_BUFFER, vboFadeout);
	fadeOutAposLoc = glGetAttribLocation(fadeout_program, "aPos");
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(fadeOutAposLoc);
	glVertexAttribPointer(fadeOutAposLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);

	currentTime += fps->dt;

	if (currentTime > 5) {
		gbc->animating = false;
	}

	glUseProgram(fadeout_program);
	glUniform1f(fadeOuttimeLoc, currentTime);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void init_banner(struct gbcc *gbc) {
	struct gbcc_window *win = &gbc->window;
	GLfloat banner_vertices[] = {
		//  X     Y     U     V
		-1.0f,  1.0f,  0.0f, 0.0f,  // Haut gauche
		1.0f,  1.0f,  1.0f, 0.0f,  // Haut droit
		-1.0f,  0.8f,  0.0f, 1.0f,  // Bas gauche
		1.0f,  0.8f,  1.0f, 1.0f   // Bas droit
	};
	glGenBuffers(1, &vboBanner);
	glBindBuffer(GL_ARRAY_BUFFER, vboBanner);
	glBufferData(GL_ARRAY_BUFFER, sizeof(banner_vertices), banner_vertices, GL_STATIC_DRAW);

	texBanner = loadTexture("banner.png");
}

void render_banner(struct gbcc *gbc) {
	struct gbcc_window *win = &gbc->window;
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindBuffer(GL_ARRAY_BUFFER, vboBanner);

	GLuint posLoc = glGetAttribLocation(simple_program, "aPos");
	GLuint texLoc = glGetAttribLocation(simple_program, "aTex");
	glEnableVertexAttribArray(posLoc);
	glEnableVertexAttribArray(texLoc);
	glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


	glBindTexture(GL_TEXTURE_2D, texBanner);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void init_simple_rendering(struct gbcc *gbc) {
    struct gbcc_window *win = &gbc->window;

    glEnable(GL_DEPTH_TEST);

    // simple_program = create_program();
    
    simple_program = createProgram(vertex_shader_src,fragment_shader_src);


    glUseProgram(simple_program);

    glGenBuffers(1, &win->gl.vbo);
    glGenBuffers(1, &win->gl.ebo);

    GLuint posLoc = glGetAttribLocation(simple_program, "aPos");
    GLuint texLoc = glGetAttribLocation(simple_program, "aTex");

    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);

    glBindBuffer(GL_ARRAY_BUFFER, win->gl.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, win->gl.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glGenTextures(1, &win->gl.texture);
    glBindTexture(GL_TEXTURE_2D, win->gl.texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GBC_SCREEN_WIDTH, GBC_SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


    init_banner(gbc);
    init_fadeout(gbc);

    win->fps.dt = 0;
    win->initialised = true;
}

void update_simple_rendering(struct gbcc *gbc, int w, int h) {
	glUseProgram(simple_program);
	struct gbcc_window *win = &gbc->window;
	glBindBuffer(GL_ARRAY_BUFFER, win->gl.vbo);

	GLuint posLoc = glGetAttribLocation(simple_program, "aPos");
	GLuint texLoc = glGetAttribLocation(simple_program, "aTex");
	glEnableVertexAttribArray(posLoc);
	glEnableVertexAttribArray(texLoc);
	glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


	glViewport(win->x, win->y, w, h);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, win->gl.texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GBC_SCREEN_WIDTH, GBC_SCREEN_HEIGHT, GL_RGBA,
			GL_UNSIGNED_BYTE, (GLvoid *)win->buffer);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	render_banner(gbc);
	render_fadeout(gbc);
}

void gbcc_window_initialise(struct gbcc *gbc)
{
	struct gbcc_window *win = &gbc->window;
	*win = (struct gbcc_window){0};

	GLint read_framebuffer = 0;
	GLint draw_framebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

	clock_gettime(CLOCK_REALTIME, &win->fps.last_time);
	gbcc_fontmap_load(&win->font);

	return init_simple_rendering(gbc);
}

void gbcc_window_deinitialise(struct gbcc *gbc)
{
	struct gbcc_window *win = &gbc->window;
	if (!win->initialised) {
		gbcc_log_error("Can't destroy window: Window not initialised!\n");
		return;
	}
	win->initialised = false;

	gbcc_fontmap_destroy(&win->font);
	glDeleteBuffers(1, &win->gl.vbo);
	glDeleteVertexArrays(1, &win->gl.vao);
	glDeleteBuffers(1, &win->gl.ebo);
	glDeleteFramebuffers(1, &win->gl.fbo);
	glDeleteTextures(1, &win->gl.fbo_texture);
	glDeleteRenderbuffers(1, &win->gl.rbo);
	glDeleteTextures(1, &win->gl.texture);
#ifdef GL_LUT
	glDeleteTextures(1, &win->gl.lut_texture);
#endif
	for (size_t i = 0; i < N_ELEM(win->gl.shaders); i++) {
		glDeleteProgram(win->gl.shaders[i].program);
	}
	glDeleteProgram(win->gl.base_shader);
}


float rand_float() {
    return (float)rand() / (float)RAND_MAX;
}

void gbcc_window_update(struct gbcc *gbc)
{

	struct gbcc_window *win = &gbc->window;
	if (!win->initialised) {
		gbcc_log_error("Can't update window: Window not initialised!\n");
		return;
	}
	update_timers(gbc);
	GLint read_framebuffer = 0;
	GLint draw_framebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

	memcpy(win->buffer, gbc->core.ppu.screen.sdl, GBC_SCREEN_SIZE * sizeof(win->buffer[0]));
	{
		int val = 0;
		sem_getvalue(&gbc->core.ppu.vsync_semaphore, &val);
		if (!val) {
			sem_post(&gbc->core.ppu.vsync_semaphore);
		}
	}


	for (size_t i = 0; i < N_ELEM(win->buffer); i++) {
		uint32_t tmp = win->buffer[i];
		win->buffer[i] = (tmp & 0xFFu) << 24u
				| (tmp & 0xFF00u) << 8u
				| (tmp & 0xFF0000u) >> 8u
				| (tmp & 0xFF000000u) >> 24u;
	}

	/* Setup - resize our screen textures if needed */
	if (gbc->fractional_scaling) {
		win->scale = min((float)win->width / GBC_SCREEN_WIDTH, (float)win->height / GBC_SCREEN_HEIGHT);
	} else {
		win->scale = min((float)(win->width / GBC_SCREEN_WIDTH), (float)(win->height / GBC_SCREEN_HEIGHT));
	}
	unsigned int width = (unsigned int)(win->scale * GBC_SCREEN_WIDTH);
	unsigned int height = (unsigned int)(win->scale * GBC_SCREEN_HEIGHT);
	win->x = ((unsigned int)win->width - width) / 2;
	win->y = ((unsigned int)win->height - height) / 2;


	return update_simple_rendering(gbc,width,height);
}

void gbcc_load_shader(GLuint shader, const char *filename)
{
	errno = 0;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		gbcc_log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		gbcc_log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	long size = ftell(fp);
	if (size <= 0) {
		gbcc_log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	unsigned long usize = (unsigned long) size;
	GLchar *source = malloc(usize + 1);
	rewind(fp);
	if (fread(source, 1, usize, fp) != usize) {
		gbcc_log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	source[usize] = '\0';
	glShaderSource(shader, 1, (const GLchar *const *)&source, NULL);
	free(source);

	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		gbcc_log_error("Failed to compile shader %s!\n", filename);

		GLint info_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);
		if (info_length > 1) {
			char *log = malloc((unsigned)info_length * sizeof(*log));
			glGetShaderInfoLog(shader, info_length, NULL, log);
			gbcc_log_append_error("%s\n", log);
			free(log);
		}
		exit(EXIT_FAILURE);
	}
}

GLuint gbcc_create_shader_program(const char *vert, const char *frag)
{
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	gbcc_load_shader(vertex_shader, vert);

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	gbcc_load_shader(fragment_shader, frag);

	GLuint shader = glCreateProgram();
	glAttachShader(shader, vertex_shader);
	glAttachShader(shader, fragment_shader);
	glLinkProgram(shader);
	return shader;
}

void gbcc_window_use_shader(struct gbcc *gbc, const char *name)
{
	struct gbcc_window *win = &gbc->window;
	if (!win->initialised) {
		gbcc_log_error("Can't load shader: Window not initialised!\n");
		return;
	}
	int num_shaders = N_ELEM(win->gl.shaders);
	int s;
	for (s = 0; s < num_shaders; s++) {
		if (strcasecmp(name, win->gl.shaders[s].name) == 0) {
			break;
		}
	}
	if (s >= num_shaders) {
		gbcc_log_error("Invalid shader \"%s\"\n", name);
	} else {
		win->gl.cur_shader = s;
	}
}

void gbcc_window_show_message(struct gbcc *gbc, const char *msg, unsigned seconds, bool pad)
{
	struct gbcc_window *win = &gbc->window;
	if (!win->initialised) {
		gbcc_log_error("Can't show message: window not initialised!\n");
		return;
	}
	if (pad) {
		snprintf(win->msg.text, MSG_BUF_SIZE, " %s ", msg);
	} else {
		strncpy(win->msg.text, msg, MSG_BUF_SIZE);
		win->msg.text[MSG_BUF_SIZE - 1] = '\0';
	}
	uint8_t width_tiles = GBC_SCREEN_WIDTH / win->font.tile_width;
	win->msg.lines = (uint8_t)(1 + strlen(win->msg.text) / width_tiles);
	win->msg.time_left = seconds * SECOND;
}


void update_timers(struct gbcc *gbc)
{
	struct gbcc_window *win = &gbc->window;
	struct fps_counter *fps = &win->fps;
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time);
	float dt = (float)gbcc_time_diff(&cur_time, &fps->last_time);
	float seconds = dt / 1e9f;
	dt /= GBC_FRAME_PERIOD;
	fps->dt = seconds;

	const float alpha = 0.001f;
	if (dt < 1.1f && dt > 0.9f) {
		gbc->audio.scale = alpha * dt + (1 - alpha) * gbc->audio.scale;
	}

	/* Update FPS counter */
	fps->last_time = cur_time;
	float df = (float)(gbc->core.ppu.frame - fps->last_frame);
	uint8_t ly = gbcc_memory_read_force(&gbc->core, LY);
	uint8_t tmp = ly;
	if (ly < fps->last_ly) {
		df -= 1;
		ly += (154 - fps->last_ly);
	} else {
		ly -= fps->last_ly;
	}
	df += ly / 154.0f;
	fps->last_frame = gbc->core.ppu.frame;
	fps->last_ly = tmp;
	fps->previous[fps->idx] = df / (dt * GBC_FRAME_PERIOD / 1e9f);
	fps->idx++;
	fps->idx %= N_ELEM(fps->previous);
	float avg = 0;
	for (size_t i = 0; i < N_ELEM(fps->previous); i++) {
		avg += fps->previous[i];
	}
	fps->fps = avg / N_ELEM(fps->previous);

	/* Update message timer */
	if (win->msg.time_left > 0) {
		win->msg.time_left -= (int64_t)(dt * GBC_FRAME_PERIOD);
	}
}
