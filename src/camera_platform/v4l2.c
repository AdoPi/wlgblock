/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

/*
 * This file is heavily based on the
 * video capture example provided with the V4L2 API.
 */

#include "../camera.h"
#include "../debug.h"
#include "../gbcc.h"
#include "v4l2.h"

#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP_UINT8(x) ((uint8_t)(MIN(MAX((x), 0), UINT8_MAX)))

static bool initialise_v4l2(struct gbcc_camera_platform *camera);
static void log_errno(const char *s);
static int xioctl(int fh, unsigned long request, void *arg);
static void process_image(struct gbcc_camera_platform *camera,
		uint8_t image[GB_CAMERA_SENSOR_SIZE],
		uint32_t buffer_index);
static int read_frame(struct gbcc_camera_platform *camera, uint8_t image[GB_CAMERA_SENSOR_SIZE]);

static bool init_read(struct gbcc_camera_platform *camera);
static void destroy_read(struct gbcc_camera_platform *camera);

static bool init_userptr(struct gbcc_camera_platform *camera);
static void destroy_userptr(struct gbcc_camera_platform *camera);

static bool init_mmap(struct gbcc_camera_platform *camera);
static void destroy_mmap(struct gbcc_camera_platform *camera);

static void initialise_default_image(void);
static void get_default_image(uint8_t image[GB_CAMERA_SENSOR_SIZE]);

void gbcc_camera_platform_initialise(struct gbcc_camera_platform *camera)
{
	if (!initialise_v4l2(camera)) {
		gbcc_log_error("Initialising V4L2 failed, "
				"falling back to default camera image.\n");
		/* Use the default image */
		initialise_default_image();
	}
}

void gbcc_camera_platform_destroy(struct gbcc_camera_platform *camera)
{
	switch (camera->method) {
		case GBCC_CAMERA_IO_METHOD_NONE:
			/* We didn't do any of the setup, so just return */
			return;
		case GBCC_CAMERA_IO_METHOD_READ:
			destroy_read(camera);
			break;
		case GBCC_CAMERA_IO_METHOD_MMAP:
			destroy_mmap(camera);
			break;
		case GBCC_CAMERA_IO_METHOD_USERPTR:
			destroy_userptr(camera);
			break;
	}
	free(camera->greyscale_buffer);
        if (close(camera->fd) == -1) {
                log_errno("close");
	}

        camera->fd = -1;
}

void gbcc_camera_platform_capture_image(struct gbcc_camera_platform *camera,
		uint8_t image[GB_CAMERA_SENSOR_SIZE])
{
	if (camera->method == GBCC_CAMERA_IO_METHOD_NONE) {
		get_default_image(image);
		return;
	}
	while (true) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO(&fds);
		FD_SET(camera->fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(camera->fd + 1, &fds, NULL, NULL, &tv);

		if (r == -1) {
			if (EINTR == errno)
				continue;
			log_errno("select");
		}

		if (r == 0) {
			gbcc_log_error("select timeout\n");
			continue;
		}

		if (read_frame(camera, image)) {
			break;
		}
		/* EAGAIN - continue select loop. */
	}
}

bool initialise_v4l2(struct gbcc_camera_platform *camera)
{
	gbcc_log_debug("Initialising camera...\n");

	camera->device_name = "/dev/video0";
	camera->method = GBCC_CAMERA_IO_METHOD_NONE;

	/* Open the device */
        camera->fd = open(camera->device_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (camera->fd == -1) {
                gbcc_log_error("Cannot open '%s': %d, %s\n",
                         camera->device_name, errno, strerror(errno));
		return false;
        }

	/* Check that the specified path is a character device */
	{
		struct stat st;
		if (stat(camera->device_name, &st) == -1) {
			gbcc_log_error("Cannot identify '%s': %d, %s\n",
					camera->device_name, errno, strerror(errno));
			close(camera->fd);
			camera->fd = -1;
			return false;
		}

		if (!S_ISCHR(st.st_mode)) {
			gbcc_log_error("%s is not a device\n", camera->device_name);
			close(camera->fd);
			camera->fd = -1;
			return false;
		}
	}


	/* Initialise v4l2 */
        struct v4l2_capability cap = {0};

        if (xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) {
                if (errno == EINVAL) {
			gbcc_log_error("%s is not a V4L2 device\n", camera->device_name);
			close(camera->fd);
			return false;
                } else {
                        log_errno("VIDIOC_QUERYCAP");
			close(camera->fd);
			return false;
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                gbcc_log_error("%s is not a video capture device\n", camera->device_name);
		close(camera->fd);
		return false;
        }

	/* Reset the crop if cropping is supported */
	{
		struct v4l2_cropcap cropcap = {
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE
		};

		if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0) {
			struct v4l2_crop crop = {
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.c = cropcap.defrect
			};

			/*
			 * We can ignore any errors here,
			 * as it just means cropping isn't supported.
			 */
			xioctl(camera->fd, VIDIOC_S_CROP, &crop);
		}
	}

        /* Select the video format we want */
	{
		struct v4l2_format fmt = {0};
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width       = 320;
		fmt.fmt.pix.height      = 240;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		fmt.fmt.pix.field       = V4L2_FIELD_ANY;

		if (xioctl(camera->fd, VIDIOC_S_FMT, &fmt) == -1) {
			log_errno("VIDIOC_S_FMT");
			close(camera->fd);
			return false;
		}
		camera->format = fmt.fmt.pix;
	}

	/* Print the format we actually got */
	gbcc_log_debug("\tResolution: %dx%d\n", camera->format.width, camera->format.height);
	switch (camera->format.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			gbcc_log_debug("\tFormat: YUYV\n");
			break;
		case V4L2_PIX_FMT_UYVY:
			gbcc_log_debug("\tFormat: UYVY\n");
			break;
		default:
			{
				uint32_t px = camera->format.pixelformat;
				unsigned char str[5] = {
					(px >> 0u) & 0xFFu,
					(px >> 8u) & 0xFFu,
					(px >> 16u) & 0xFFu,
					(px >> 24u) & 0xFFu,
					0
				};
				gbcc_log_error("Unsupported pixel format %s.\n", str);
				close(camera->fd);
				return false;
			}
	}

	switch (camera->format.field) {
		case 1:
			gbcc_log_debug("\tScan: Progressive\n");
			break;
		case 2:
			gbcc_log_debug("\tScan: Top\n");
			break;
		case 3:
			gbcc_log_debug("\tScan: Bottom\n");
			break;
		case 4:
			gbcc_log_debug("\tScan: Interlaced\n");
			break;
		default:
			gbcc_log_error("Unknown scan type.\n");
			close(camera->fd);
			return false;
	}

        /* Buggy driver paranoia. */
        unsigned int min = camera->format.width;
        if (camera->format.bytesperline < min) {
                camera->format.bytesperline = min;
	}
        min = camera->format.bytesperline * camera->format.height;
        if (camera->format.sizeimage < min) {
                camera->format.sizeimage = min;
	}

	/*
	 * First try read/write I/O, as it's the simplest.
	 * If that fails, fall back to mmap, then userptr, then fail.
	 */
	camera->method = GBCC_CAMERA_IO_METHOD_NONE;
	if (cap.capabilities & V4L2_CAP_READWRITE) {
		gbcc_log_debug("\tUsing read/write I/O\n");
		camera->method = GBCC_CAMERA_IO_METHOD_READ;
		init_read(camera);
	} else if (cap.capabilities & V4L2_CAP_STREAMING) {
		gbcc_log_debug("\tUsing streaming I/O\n");
		gbcc_log_debug("\tTrying memory-mapped streaming...\n");
		if (init_mmap(camera)) {
			gbcc_log_debug("\tSuccess\n");
			camera->method = GBCC_CAMERA_IO_METHOD_MMAP;
		} else {
			gbcc_log_debug("\tTrying user pointer streaming...\n");
			if (init_userptr(camera)) {
				gbcc_log_debug("\tSuccess\n");
				camera->method = GBCC_CAMERA_IO_METHOD_USERPTR;
			}
		}
	}
	if (camera->method == GBCC_CAMERA_IO_METHOD_NONE) {
		gbcc_log_error("%s does not support video capture, "
				"falling back to static image.\n",
				camera->device_name);
		close(camera->fd);
		return false;
	}

	size_t min_dim = MIN(camera->format.width, camera->format.height);
	camera->greyscale_buffer = malloc(min_dim * min_dim);
	return true;
}

void log_errno(const char *s)
{
        gbcc_log_error("%s error %d, %s\n", s, errno, strerror(errno));
}

int xioctl(int fd, unsigned long request, void *arg)
{
        int r;

	/* Retry if ioctl failed due to an interrupt */
        do {
                r = ioctl(fd, request, arg);
        } while (r == -1 && errno == EINTR);

        return r;
}

void process_image(struct gbcc_camera_platform *camera,
		uint8_t image[GB_CAMERA_SENSOR_SIZE],
		uint32_t buffer_index)
{
	uint32_t width = camera->format.width;
	uint32_t height = camera->format.height;
	uint32_t min_dim = MIN(width, height);
	uint8_t *raw_buffer = camera->raw_buffers[buffer_index].data;

	/* First, extract a centered, square, greyscale image */
	unsigned int offset = 0;

	/* If the format if UYVY, we need to extract the odd-numbered bytes */
	if (camera->format.pixelformat == V4L2_PIX_FMT_UYVY) {
		offset = 1;
	}
	for (uint32_t jdst = 0, jsrc = (height - min_dim) / 2; jdst < min_dim; jsrc++, jdst++) {
		for (uint32_t idst = 0, isrc = (width - min_dim) / 2; idst < min_dim; isrc++, idst++) {
			size_t src_idx = jsrc * camera->format.bytesperline + 2 * isrc + offset;
			size_t dst_idx = jdst * min_dim + idst;
			camera->greyscale_buffer[dst_idx] = raw_buffer[src_idx];
		}
	}

	/*
	 * Perform box-blur downsampling of the sensor image
	 * to get our 128x128 gb camera image
	 */
	uint32_t box_size = min_dim / GB_CAMERA_SENSOR_WIDTH / 2 + 1;

	/* First we perform the horizontal blur */
	uint8_t *new_row = calloc(min_dim, 1);
	for (uint32_t j = 0; j < min_dim; j++) {
		uint8_t *row = &camera->greyscale_buffer[j * min_dim];
		unsigned int sum = 0;
		unsigned int div = 0;

		for (uint32_t i = 0; i < min_dim + box_size; i++) {
			if (i < min_dim) {
				sum += row[i];
			} else {
				div--;
			}
			if (i >= 2 * box_size) {
				sum -= row[i - 2 * box_size];
			} else {
				div++;
			}

			if (i >= box_size) {
				new_row[i - box_size] = CLAMP_UINT8(sum / div);
			}
		}
		memcpy(row, new_row, min_dim);
	}
	free(new_row);

	/* Then the vertical blur */
	uint8_t *new_col = calloc(min_dim, 1);
	for (uint32_t i = 0; i < min_dim; i++) {
		unsigned int sum = 0;
		unsigned int div = 0;

		for (uint32_t j = 0; j < min_dim + box_size; j++) {
			if (j < min_dim) {
				sum += camera->greyscale_buffer[j * min_dim + i];
			} else {
				div--;
			}
			if (j >= 2 * box_size) {
				sum -= camera->greyscale_buffer[(j - 2 * box_size) * min_dim + i];
			} else {
				div++;
			}

			if (j >= box_size) {
				new_col[j - box_size] = CLAMP_UINT8(sum / div);
			}
		}
		for (uint32_t j = 0; j < min_dim; j++) {
			camera->greyscale_buffer[j * min_dim + i] = new_col[j];
		}
	}
	free(new_col);

	/* Then we nearest-neighbour downscale */
	double scale = min_dim / (double)GB_CAMERA_SENSOR_WIDTH;
	for (int j = 0; j < GB_CAMERA_SENSOR_HEIGHT; j++) {
		for (int i = 0; i < GB_CAMERA_SENSOR_WIDTH; i++) {
			size_t src_idx = (size_t)(j * scale) * min_dim + (size_t)(i * scale);
			int dst_idx = j * GB_CAMERA_SENSOR_WIDTH + i;

			image[dst_idx] = camera->greyscale_buffer[src_idx];
		}
	}
}

int read_frame(struct gbcc_camera_platform *camera, uint8_t image[GB_CAMERA_SENSOR_SIZE])
{
	ssize_t res;
        struct v4l2_buffer buf;

	switch (camera->method) {
		case GBCC_CAMERA_IO_METHOD_READ:
			res = read(camera->fd, camera->raw_buffers[0].data,
					camera->format.sizeimage);
			if (res == -1) {
				switch (errno) {
					case EAGAIN:
						return 0;
					default:
						log_errno("read");
				}
			}
			process_image(camera, image, 0);
			break;
		case GBCC_CAMERA_IO_METHOD_MMAP:
			buf = (struct v4l2_buffer) {
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_MMAP
			};

			/* Dequeue a buffer */
			if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
				switch (errno) {
					case EAGAIN:
						return 0;
					default:
						log_errno("VIDIOC_DQBUF");
				}
			}

			process_image(camera, image, buf.index);

			if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
				log_errno("VIDIOC_QBUF");
			}
			break;
		case GBCC_CAMERA_IO_METHOD_USERPTR:
			buf = (struct v4l2_buffer) {
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_USERPTR
			};

			/* Dequeue a buffer */
			if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
				switch (errno) {
					case EAGAIN:
						return 0;
					default:
						log_errno("VIDIOC_DQBUF");
				}
			}

			for (uint32_t i = 0; i < camera->n_buffers; i++) {
				if (buf.m.userptr == (unsigned long)camera->raw_buffers[i].data
						&& buf.length == camera->raw_buffers[i].size) {
					process_image(camera, image, i);
					break;
				}
			}

			if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
				log_errno("VIDIOC_QBUF");
			}
			break;
		case GBCC_CAMERA_IO_METHOD_NONE:
			gbcc_log_error("Impossible case in" __FILE__ ", line %d", __LINE__);
			abort();
	}

	return 1;
}

bool init_read(struct gbcc_camera_platform *camera)
{
	camera->raw_buffers = malloc(sizeof(*camera->raw_buffers));
        camera->raw_buffers[0].data = malloc(camera->format.sizeimage);
	camera->raw_buffers[0].size = camera->format.sizeimage;
	return true;
}

void destroy_read(struct gbcc_camera_platform *camera)
{
	free(camera->raw_buffers[0].data);
	free(camera->raw_buffers);
}

bool init_userptr(struct gbcc_camera_platform *camera)
{
	/* Request some buffers */
	struct v4l2_requestbuffers req = {
		.count = 2,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_USERPTR
	};

        if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) {
                if (EINVAL == errno) {
			/* Device does not support user pointer I/O */
			return false;
                } else {
			/* Something else went wrong */
			log_errno("VIDIOC_REQBUFS");
			return false;
                }
        }

	/* Allocate & enqueue our buffers */
	camera->n_buffers = req.count;
        camera->raw_buffers = calloc(camera->n_buffers, sizeof(*camera->raw_buffers));

	for (uint32_t i = 0; i < camera->n_buffers; i++) {
                camera->raw_buffers[i].size = camera->format.sizeimage;
                camera->raw_buffers[i].data = malloc(camera->format.sizeimage);

		struct v4l2_buffer buf = {
			.index = i,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_USERPTR,
			.m.userptr = (unsigned long)camera->raw_buffers[i].data,
			.length = camera->raw_buffers[i].size
		};

		if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
			log_errno("VIDIOC_QBUF");

			/* Deallocate any allocated buffers */
			for (uint32_t j = 0; j < i; j++) {
				free(camera->raw_buffers[j].data);
			}

			/* Attempt to clear our requested buffers. */
			struct v4l2_requestbuffers err_req = {
				.count = 0,
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_USERPTR
			};
			xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
			return false;
		}
	}

	/* Start the actual capturing */
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) {
		log_errno("VIDIOC_STREAMON");

		/* Deallocate any allocated buffers */
		for (uint32_t j = 0; j < camera->n_buffers; j++) {
			free(camera->raw_buffers[j].data);
		}

		/* Attempt to clear our requested buffers. */
		struct v4l2_requestbuffers err_req = {
			.count = 0,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_USERPTR
		};
		xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
		return false;
	}

	return true;
}

void destroy_userptr(struct gbcc_camera_platform *camera)
{
	/* Stop capturing first */
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1) {
		log_errno("VIDIOC_STREAMOFF");
	}

	struct v4l2_requestbuffers req = {
		.count = 0,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_USERPTR
	};

        xioctl(camera->fd, VIDIOC_REQBUFS, &req);

        for (uint32_t i = 0; i < camera->n_buffers; i++) {
		free(camera->raw_buffers[i].data);
        }
	free(camera->raw_buffers);
}


bool init_mmap(struct gbcc_camera_platform *camera)
{
	/* Request some buffers */
	struct v4l2_requestbuffers req = {
		.count = 2,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP
	};

        if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) {
                if (EINVAL == errno) {
			/* Device does not support mmap I/O */
			return false;
                } else {
			/* Something else went wrong */
			log_errno("VIDIOC_REQBUFS");
			return false;
                }
        }

	if (req.count < 2) {
		gbcc_log_error("Insufficient buffer memory on %s\n",
				camera->device_name);
		/* Attempt to clear our requested buffers. */
		struct v4l2_requestbuffers err_req = {
			.count = 0,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_MMAP
		};
		xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
		return false;
	}

	/* Map & enqueue our buffers */
	camera->n_buffers = req.count;
        camera->raw_buffers = calloc(camera->n_buffers, sizeof(*camera->raw_buffers));

	for (uint32_t i = 0; i < camera->n_buffers; i++) {
		struct v4l2_buffer buf = {
			.index = i,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_MMAP
		};

                if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1) {
                        log_errno("VIDIOC_QUERYBUF");

			/* Unmap any mapped buffers */
			for (uint32_t j = 0; i > 0 && j < i - 1; j++) {
				munmap(camera->raw_buffers[j].data,
					camera->raw_buffers[j].size);
			}

			/* Attempt to clear our requested buffers. */
			struct v4l2_requestbuffers err_req = {
				.count = 0,
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_MMAP
			};
			xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
			return false;
		}

                camera->raw_buffers[i].size = buf.length;
                camera->raw_buffers[i].data =
			mmap(NULL /* Start anywhere */,
				buf.length,
				PROT_READ | PROT_WRITE, /* Required */
				MAP_SHARED,
				camera->fd,
				buf.m.offset);

		if (camera->raw_buffers[i].data == MAP_FAILED) {
			log_errno("mmap");

			/* Unmap any mapped buffers */
			for (uint32_t j = 0; i > 0 && j < i - 1; j++) {
				munmap(camera->raw_buffers[j].data,
					camera->raw_buffers[j].size);
			}

			/* Attempt to clear our requested buffers. */
			struct v4l2_requestbuffers err_req = {
				.count = 0,
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_MMAP
			};
			xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
			return false;
		}

		/* Enqueue the buffer */
		if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
			log_errno("VIDIOC_QBUF");

			/* Unmap any mapped buffers */
			for (uint32_t j = 0; j < i; j++) {
				munmap(camera->raw_buffers[j].data,
					camera->raw_buffers[j].size);
			}

			/* Attempt to clear our requested buffers. */
			struct v4l2_requestbuffers err_req = {
				.count = 0,
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
				.memory = V4L2_MEMORY_MMAP
			};
			xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
			return false;
		}
	}

	/* Start the actual capturing */
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) {
		log_errno("VIDIOC_STREAMON");

		/* Unmap any mapped buffers */
		for (uint32_t j = 0; j < camera->n_buffers; j++) {
			munmap(camera->raw_buffers[j].data,
				camera->raw_buffers[j].size);
		}

		/* Attempt to clear our requested buffers. */
		struct v4l2_requestbuffers err_req = {
			.count = 0,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_MMAP
		};
		xioctl(camera->fd, VIDIOC_REQBUFS, &err_req);
		return false;
	}

	return true;
}

void destroy_mmap(struct gbcc_camera_platform *camera)
{
	/* Stop capturing first */
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1) {
		log_errno("VIDIOC_STREAMOFF");
	}

        for (uint32_t i = 0; i < camera->n_buffers; i++) {
		if (munmap(camera->raw_buffers[i].data, camera->raw_buffers[i].size) == -1) {
			log_errno("munmap");
		}
        }
	free(camera->raw_buffers);
}



/*
 * Copy of the null camera driver, to use as a fallback.
 */
#ifndef CAMERA_PATH
#define CAMERA_PATH "camera.png"
#endif

#define HEADER_BYTES 8

static uint8_t default_image[GB_CAMERA_SENSOR_SIZE];

void get_default_image(uint8_t image[GB_CAMERA_SENSOR_SIZE])
{
	memcpy(image, default_image, GB_CAMERA_SENSOR_SIZE);
}

void initialise_default_image(void)
{
	FILE *fp = fopen(CAMERA_PATH, "rb");
	uint8_t header[HEADER_BYTES];
	if (!fp) {
		gbcc_log_error("Couldn't open %s\n", CAMERA_PATH);
		return;
	}
	if (fread(header, 1, HEADER_BYTES, fp) != HEADER_BYTES) {
		gbcc_log_error("Failed to read camera data: %s\n", CAMERA_PATH);
		fclose(fp);
		return;
	}
	if (png_sig_cmp(header, 0, HEADER_BYTES)) {
		gbcc_log_error("Not a PNG file: %s\n", CAMERA_PATH);
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

	if (width != GB_CAMERA_SENSOR_WIDTH || height != GB_CAMERA_SENSOR_HEIGHT) {
		gbcc_log_error("Invalid camera image dimensions.\n");
		fclose(fp);
		return;
	}

	uint32_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	png_bytepp row_pointers = calloc(GB_CAMERA_SENSOR_HEIGHT, sizeof(png_bytep));
	for (uint32_t y = 0; y < GB_CAMERA_SENSOR_HEIGHT; y++) {
		row_pointers[y] = (unsigned char *)&default_image[y * GB_CAMERA_SENSOR_WIDTH];
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
