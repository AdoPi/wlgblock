/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "camera.h"
#include "bit_utils.h"
#include "debug.h"
#include "gbcc.h"
#include <string.h>

#define GB_CAMERA_WIDTH 128
#define GB_CAMERA_HEIGHT 112
#define GB_CAMERA_WIDTH_TILES (GB_CAMERA_WIDTH / 8)
#define GB_CAMERA_HEIGHT_TILES (GB_CAMERA_HEIGHT / 8)

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define CLAMP_UINT8(x) ((uint8_t)(MIN(MAX((x), 0), UINT8_MAX)))

/*
 * 3x3 matrix filtering kernels, applied as part of the edge control.
 */
static const int8_t filter_kernel[3][3][3] = {
	/* Horizontal enhancement / extraction */
	/* N = 0, VH = 1 */
	{
		{0,  0,  0},
		{-1, 2, -1},
		{0,  0,  0}
	},
	/* Vertical enhancement / extraction */
	/* N = 1, VH = 2 */
	{
		{0, -1,  0},
		{0,  2,  0},
		{0, -1,  0}
	},
	/* 2D enhancement / extraction */
	/* N = 1, VH = 3 */
	{
		{0, -1,  0},
		{-1, 4, -1},
		{0, -1,  0}
	}
};

static void write_tile_data(struct gbcc_core *gbc, uint8_t image[GB_CAMERA_SENSOR_SIZE]);

void gbcc_camera_initialise(struct gbcc *gbc) {
	if (gbc->core.cart.mbc.type == CAMERA) {
		gbcc_camera_platform_initialise(&gbc->camera);
	}
}

void gbcc_camera_destroy(struct gbcc *gbc) {
	if (gbc->core.cart.mbc.type == CAMERA) {
		gbcc_camera_platform_destroy(&gbc->camera);
	}
}

void gbcc_camera_clock(struct gbcc *gbc) {
	struct gbcc_mbc *mbc = &gbc->core.cart.mbc;
	if (mbc->type != CAMERA) {
		return;
	}
	if (mbc->camera.capture_request) {
		gbcc_camera_capture_image(gbc);
		mbc->camera.capture_request = false;
	}

	/* camera.capture_timer is in "Game Boy clocks", i.e. 1MHz */
	if (gbc->core.cpu.clock != 0) {
		return;
	}
	if (mbc->camera.capture_timer > 0) {
		mbc->camera.capture_timer--;
	}
}

void gbcc_camera_capture_image(struct gbcc *gbc)
{
	struct gbcc_camera *cam = &gbc->core.cart.mbc.camera;
	uint8_t *sensor_image = calloc(GB_CAMERA_SENSOR_SIZE, sizeof(*sensor_image));
	uint8_t *buffer1 = calloc(GB_CAMERA_SENSOR_SIZE, sizeof(*buffer1));

	gbcc_camera_platform_capture_image(&gbc->camera, sensor_image);

	/* Set the image processing registers */
	cam->reg.z = (cam->reg0 & 0xC0u) >> 6;
	cam->reg.o = cam->reg0 & 0x3Fu;
	cam->reg.n = check_bit(cam->reg1, 7);
	cam->reg.vh = (cam->reg1 & 0x70u) >> 5;
	cam->reg.g = cam->reg1 & 0x1Fu;
	cam->reg.exposure_steps = cat_bytes(cam->reg3, cam->reg2);
	cam->reg.p = cam->reg4;
	cam->reg.m = cam->reg5;
	cam->reg.x = cam->reg6;
	cam->reg.e = (cam->reg7 & 0xF0u) >> 4;
	cam->reg.i = check_bit(cam->reg7, 3);
	cam->reg.v = cam->reg7 & 0x07u;

	/* Fake exposure time */
	cam->capture_timer = 32446 + cam->reg.n * 512 + 16 * cam->reg.exposure_steps;
	for (int i = 0; i < GB_CAMERA_SENSOR_SIZE; i++) {
		/*
		 * Some minor magic to make the image look correct.
		 *
		 * The Gameboy Camera seems to expect the values to be in a
		 * pretty small range, starting at 128 and maxing out around
		 * 220-ish for a normal exposure.
		 */
		int tmp = 128 + sensor_image[i] * cam->reg.exposure_steps / 0x2000u;
		sensor_image[i] = CLAMP_UINT8(tmp);
	}

	if (cam->reg.i) {
		/* Invert the image */
		for (int i = 0; i < GB_CAMERA_SENSOR_SIZE; i++) {
			sensor_image[i] = ~sensor_image[i];
		}
	}

	if (cam->reg.vh) {
		uint8_t vh = cam->reg.vh;

		float edge_enhancement_ratio = 0.5f;
		edge_enhancement_ratio += 0.25f * (cam->reg.e & 0x03u);
		if (check_bit(cam->reg.e, 2)) {
			edge_enhancement_ratio *= 4;
		}

		/*
		 * Apply 3x3 filtering kernel
		 *
		 * This skips some rows at the top & bottom of the image,
		 * depending on which kernel is selected. It doesn't matter
		 * though, as the Gameboy Camera ignores the top & bottom 8
		 * rows anyway.
		 */
		for (int y = 3; y < GB_CAMERA_SENSOR_HEIGHT - 3; y++) {
			for (int x = 0; x < GB_CAMERA_SENSOR_WIDTH; x++) {
				int idx = y * GB_CAMERA_SENSOR_WIDTH + x;
				int16_t res = 0;
				for (int ky = 0; ky < 3; ky++) {
					for (int kx = 0; kx < 3; kx++) {
						int k_idx = (ky - 1) * GB_CAMERA_SENSOR_WIDTH + (kx - 1);
						res += sensor_image[idx + k_idx] * filter_kernel[vh - 1][ky][kx];
					}
				}
				res = (int16_t)(res * edge_enhancement_ratio);
				if (!check_bit(cam->reg.e, 3)) {
					res += sensor_image[idx];
				}
				buffer1[idx] = CLAMP_UINT8(res);
			}
		}
	} else {
		memcpy(buffer1, sensor_image, GB_CAMERA_SENSOR_SIZE);
	}

	if (!cam->reg.n) {
		/* 1-D filtering kernel */
		int p0 = check_bit(cam->reg.p, 0);
		int p1 = check_bit(cam->reg.p, 1);
		int m0 = check_bit(cam->reg.m, 0);
		int m1 = check_bit(cam->reg.m, 1);
		for (int y = GB_CAMERA_SENSOR_HEIGHT - 4; y >= 3; y--) {
			for (int x = GB_CAMERA_SENSOR_WIDTH - 1; x >= 0; x--) {
				int idx = y * GB_CAMERA_SENSOR_WIDTH + x;
				int idx1 = idx - GB_CAMERA_SENSOR_WIDTH;
				int res = 0;
				res += buffer1[idx] * (p0 - m0);
				res += buffer1[idx1] * (p1 - m1);
				buffer1[idx] = CLAMP_UINT8(res);
			}
		}
	}

	/* Dithering & contrast matrix */
	for (int y = 0; y < GB_CAMERA_SENSOR_HEIGHT; y++) {
		for (int x = 0; x < GB_CAMERA_SENSOR_WIDTH; x++) {
			int idx = y * GB_CAMERA_SENSOR_WIDTH + x;
			uint8_t px = buffer1[idx];
			uint8_t *cmp = cam->matrix[y % 4][x % 4];
			if (px < cmp[0]) {
				buffer1[idx] = 3;
			} else if (px < cmp[1]) { 
				buffer1[idx] = 2;
			} else if (px < cmp[2]) { 
				buffer1[idx] = 1;
			} else {
				buffer1[idx] = 0;
			}
		}
	}
	


	write_tile_data(&gbc->core, buffer1);

	free(buffer1);
	free(sensor_image);
}


void write_tile_data(struct gbcc_core *gbc, uint8_t image[GB_CAMERA_SENSOR_SIZE])
{
	for (uint8_t ty = 0; ty < GB_CAMERA_HEIGHT_TILES; ty++) {
		for (uint8_t tx = 0; tx < GB_CAMERA_WIDTH_TILES; tx++) {
			uint32_t base_idx = 8 * ty * GB_CAMERA_WIDTH + 8 * tx + 8 * GB_CAMERA_WIDTH;
			uint8_t *tile = &gbc->cart.ram[0x0100u + (ty * GB_CAMERA_WIDTH_TILES + tx) * 16];

			for (uint8_t y = 0; y < 8; y++) {
				uint8_t lower = 0;
				uint8_t upper = 0;
				for (uint8_t x = 0; x < 8; x++) {
					uint8_t px = image[base_idx + y * GB_CAMERA_WIDTH + x];
					lower <<= 1;
					upper <<= 1;
					lower |= check_bit(px, 0);
					upper |= check_bit(px, 1);
				}
				tile[2 * y] = lower;
				tile[2 * y + 1] = upper;
			}
		}
	}
}
