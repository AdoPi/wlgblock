/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_CAMERA_PLATFORM_V4L2_H
#define GBCC_CAMERA_PLATFORM_V4L2_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <linux/videodev2.h>

enum gbcc_camera_io_method {
	GBCC_CAMERA_IO_METHOD_NONE,
	GBCC_CAMERA_IO_METHOD_READ,
	GBCC_CAMERA_IO_METHOD_MMAP,
	GBCC_CAMERA_IO_METHOD_USERPTR
};

struct gbcc_camera_platform {
	const char *device_name;
	int fd;
	struct {
		uint8_t *data;
		uint32_t size;
	} *raw_buffers;
	uint32_t n_buffers;
	uint8_t *greyscale_buffer;
	enum gbcc_camera_io_method method;
	struct v4l2_pix_format format;
};

#endif /* GBCC_CAMERA_PLATFORM_V4L2_H */
