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
#include "debug.h"
#include "screenshot.h"
#include "window.h"
#include <errno.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define MAX_NAME_LEN 4096

void gbcc_screenshot(struct gbcc *gbc)
{
	struct gbcc_window *win = &gbc->window;
	char *dir;
	char *fname = malloc(MAX_NAME_LEN);

	dir = getenv("HOME");
	if (!dir) {
		dir = ".";
	}

	strncpy(fname, dir, MAX_NAME_LEN);

	time_t raw_time = time(NULL);
	struct tm time_info;
	gmtime_r(&raw_time, &time_info);
	strftime(fname + strlen(dir), MAX_NAME_LEN - strlen(dir), "/%G-%m-%dT%H%M%SZ-gbcc.png", &time_info);

	FILE *fp = fopen(fname, "wb");
	if (!fp) {
		gbcc_log_error("Couldn't open %s: %s\n", fname, strerror(errno));
		free(fname);
		return;
	}
	png_structp png_ptr = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fp);
		free(fname);
		gbcc_log_error("Couldn't create PNG write struct.\n");
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		free(fname);
		gbcc_log_error("Couldn't create PNG info struct.\n");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		free(fname);
		gbcc_log_error("Couldn't setjmp for libpng.\n");
		return;
	}

	uint32_t width = GBC_SCREEN_WIDTH;
	uint32_t height = GBC_SCREEN_HEIGHT;
	uint8_t *buffer = NULL;
	if (!win->raw_screenshot) {
		width = (uint32_t)((float)width * win->scale);
		height = (uint32_t)((float)height * win->scale);
		buffer = malloc((size_t)width * (size_t)height * 4 * sizeof(*buffer));
		if (!buffer) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			free(fname);
			gbcc_log_error("Couldn't malloc screenshot buffer.\n");
			return;
		}
		glReadPixels(win->x, win->y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	}
	/* Initialize rows of PNG. */
	png_bytepp row_pointers = png_malloc(png_ptr, height * sizeof(png_bytep));
	if (!row_pointers) {
		fclose(fp);
		free(fname);
		gbcc_log_error("Couldn't allocate row pointers.\n");
		return;
	}
	for (uint32_t y = 0; y < height; y++) {
		png_bytep row = png_malloc(png_ptr, width * 3);
		if (!row) {
			fclose(fp);
			free(fname);
			gbcc_log_error("Couldn't allocate row pointer.\n");
			return;
		}
		row_pointers[y] = row;
		for (uint32_t x = 0; x < width; x++) {
			if (win->raw_screenshot) {
				uint32_t idx = y * width + x;
				uint32_t pixel = win->buffer[idx];
				*row++ = (pixel & 0x000000FFu) >> 0u;
				*row++ = (pixel & 0x0000FF00u) >> 8u;
				*row++ = (pixel & 0x00FF0000u) >> 16u;
			} else {
				uint32_t idx = 4 * ((height - y - 1) * width + x);
				*row++ = buffer[idx++];
				*row++ = buffer[idx++];
				*row++ = buffer[idx++];
			}
		}
	}

	png_init_io(png_ptr, fp);
	png_set_IHDR(png_ptr, info_ptr,
			width, height,
			8, PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	for (uint32_t y = 0; y < height; y++) {
		png_free(png_ptr, row_pointers[y]);
	}
	png_free(png_ptr, row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	if (!win->raw_screenshot) {
		free(buffer);
	}

	gbcc_log_info("Saved screenshot %s\n", fname);
	char *message = malloc(MAX_NAME_LEN);
	snprintf(message, MAX_NAME_LEN, "Saved screenshot: %s ", fname);
	gbcc_window_show_message(gbc, message, 2, false);
	win->screenshot = false;
	win->raw_screenshot = false;
	free(message);
	free(fname);
}
