/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "debug.h"
#include "fontmap.h"
#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TILESET_PATH
#define TILESET_PATH "tileset.png"
#endif

#define HEADER_BYTES 8

void gbcc_fontmap_load(struct gbcc_fontmap *fontmap)
{
	const char *filename = TILESET_PATH;
	FILE *fp = fopen(filename, "rb");
	uint8_t header[HEADER_BYTES];
	if (!fp) {
		gbcc_log_error("Couldn't open %s: %s\n", filename, strerror(errno));
		return;
	}
	if (fread(header, 1, HEADER_BYTES, fp) != HEADER_BYTES) {
		gbcc_log_error("Failed to read fontmap data: %s\n", filename);
		fclose(fp);
		return;
	}
	if (png_sig_cmp(header, 0, HEADER_BYTES)) {
		gbcc_log_error("Not a PNG file: %s\n", filename);
		fclose(fp);
		return;
	}

	png_structp png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr) {
		gbcc_log_error("Couldn't create PNG read struct.\n");
		fclose(fp);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't create PNG info struct.\n");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't setjmp for libpng.\n");
		return;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, HEADER_BYTES);
	png_read_info(png_ptr, info_ptr);

	uint32_t width = png_get_image_width(png_ptr, info_ptr);
	uint32_t height = png_get_image_height(png_ptr, info_ptr);
	uint32_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	
	fontmap->tile_width = width / 16;
	fontmap->tile_height = height / 16;
	fontmap->bitmap = calloc((size_t)width * (size_t)height, sizeof(*fontmap->bitmap));

	png_bytepp row_pointers = calloc(height, sizeof(png_bytep));
	for (uint32_t y = 0; y < height; y++) {
		row_pointers[y] = &fontmap->bitmap[y * width];
	}

	if (bit_depth < 8) {
		png_set_packing(png_ptr);
	}
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(row_pointers);
	fclose(fp);
}

void gbcc_fontmap_destroy(struct gbcc_fontmap *fontmap)
{
	free(fontmap->bitmap);
}
