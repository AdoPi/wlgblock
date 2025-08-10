/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "core.h"
#include "config.h"
#include "debug.h"
#include "nelem.h"
#include "save.h"
#include "window.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Maximum number of config file errors before we give up */
#define MAX_ERRORS 5

/* Anyone with a 10M config file is doing something very wrong */
#define MAX_CONFIG_SIZE (10*1024*1024)

static char *strip(const char *str);
static bool parse_option(struct gbcc *gbc, size_t lineno, const char *option, const char *value);
static char *get_config_path(void);

bool parse_bool(size_t lineno, const char *str, bool *err);

/*
 * Function-like macro. Yuck.
 */
#define PARSE_ERROR_NO_ARGS(lineno, fmt) \
		gbcc_log_error("\tLine %zu: ", (lineno));\
		gbcc_log_append_error((fmt))

#define PARSE_ERROR(lineno, fmt, ...) \
		gbcc_log_error("\tLine %zu: ", (lineno));\
		gbcc_log_append_error((fmt), __VA_ARGS__)

void gbcc_load_config(struct gbcc *gbc, const char *filename)
{
	char *default_filename = NULL;
	if (!filename) {
		default_filename = get_config_path();
		if (!default_filename) {
			return;
		}
		filename = default_filename;
	}
	char *config;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		if (!default_filename || errno != ENOENT) {
			gbcc_log_error("Failed to open config file %s: %s\n", filename, strerror(errno));
		}
		goto CLEANUP_FILENAME;
	}
	if (fseek(fp, 0, SEEK_END)) {
		gbcc_log_error("Failed to seek in config file: %s\n", strerror(errno));
		fclose(fp);
		goto CLEANUP_FILENAME;
	}
	size_t size;
	{
		long ssize = ftell(fp);
		if (ssize < 0) {
			gbcc_log_error("Failed to determine config file size: %s\n", strerror(errno));
			fclose(fp);
			goto CLEANUP_FILENAME;
		}
		if (ssize > MAX_CONFIG_SIZE) {
			gbcc_log_error("Config file too big (> %d MiB)! Are you sure it's a file?\n", MAX_CONFIG_SIZE / 1024 / 1024);
			fclose(fp);
			goto CLEANUP_FILENAME;
		}
		size = (size_t)ssize;
	}
	config = malloc(size + 1);
	if (!config) {
		gbcc_log_error("Failed to malloc buffer for config file.\n");
		fclose(fp);
		goto CLEANUP_FILENAME;
	}
	rewind(fp);
	if (fread(config, 1, size, fp) != size) {
		gbcc_log_error("Failed to read config file: %s\n", strerror(errno));
		fclose(fp);
		goto CLEANUP_CONFIG;
	}
	fclose(fp);
	config[size] = '\0';

	char *config_copy = strdup(config);
	if (!config_copy) {
		gbcc_log_error("Failed to malloc second buffer for config file.\n");
		goto CLEANUP_ALL;
	}
	
	gbcc_log_info("Loading config from %s...\n", filename);
	
	char *saveptr1 = NULL;
	char *saveptr2 = NULL;

	char *copy_pos = config_copy;
	size_t lineno = 1;
	size_t num_errs = 0;
	for (char *str1 = config; ; str1 = NULL, saveptr2 = NULL) {
		if (num_errs > MAX_ERRORS) {
			gbcc_log_error("Too many config file errors (>%u), giving up.\n", MAX_ERRORS);
			break;
		}
		char *line = strtok_r(str1, "\r\n", &saveptr1);
		if (!line) {
			/* We're done here */
			break;
		}
		while ((copy_pos - config_copy) < (line - config)) {
			if (*copy_pos == '\n') {
				lineno++;
			}
			copy_pos++;
		}
		{
			char *line_stripped = strip(line);
			if (!line_stripped) {
				/* Skip blank lines */
				continue;
			}
			char first_char = line_stripped[0];
			free(line_stripped);
			/*
			 * Comment characters.
			 * N.B. treating section headers as comments for now.
			 */
			switch (first_char) {
				case '#':
				case ';':
				case '[':
					continue;
			}
		}
		if (line[0] == '=') {
			PARSE_ERROR_NO_ARGS(lineno, "Missing option.\n");
			num_errs++;
			continue;
		}
		char *option = strtok_r(line, "=", &saveptr2);
		if (!option) {
			char *tmp = strip(line);
			PARSE_ERROR(lineno, "Config option \"%s\" missing value.\n", tmp);
			num_errs++;
			free(tmp);
			continue;
		}
		char *option_stripped = strip(option);
		if (!option_stripped) {
			PARSE_ERROR_NO_ARGS(lineno, "Missing option.\n");
			num_errs++;
			continue;
		}
		char *value = strtok_r(NULL, "#;\r\n", &saveptr2);
		if (!value) {
			PARSE_ERROR(lineno, "Config option \"%s\" missing value.\n", option_stripped);
			num_errs++;
			free(option_stripped);
			continue;
		}
		char *value_stripped = strip(value);
		if (!value_stripped) {
			PARSE_ERROR(lineno, "Config option \"%s\" missing value.\n", option_stripped);
			num_errs++;
			free(option_stripped);
			continue;
		}
		if (!parse_option(gbc, lineno, option_stripped, value_stripped)) {
			num_errs++;
		}

		/* Cleanup */
		free(value_stripped);
		free(option_stripped);
	}

CLEANUP_ALL:
	free(config_copy);
CLEANUP_CONFIG:
	free(config);
CLEANUP_FILENAME:
	if (default_filename) {
		free(default_filename);
	}
}

char *strip(const char *str)
{
	size_t start = 0;
	size_t end = strlen(str);
	while (start <= end && isspace(str[start])) {
		start++;
	}
	if (start == end) {
		return NULL;
	}
	while (end > start && (isspace(str[end]) || str[end] == '\0')) {
		end--;
	}
	if (end < start) {
		return NULL;
	}
	size_t len = end - start + 1;
	char *buf = calloc(len + 1, 1);
	strncpy(buf, str + start, len);
	buf[len] = '\0';
	return buf;
}

bool parse_option(struct gbcc *gbc, size_t lineno, const char *option, const char *value)
{
	bool err = false;
	if (strcasecmp(option, "autoresume") == 0) {
		gbc->autoresume = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "autosave") == 0) {
		gbc->autosave = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "background") == 0) {
		gbc->background_play = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "cheat") == 0) {
		gbcc_cheats_add_fuzzy(&gbc->core, value);
		gbc->core.cheats.enabled = true;
	} else if (strcasecmp(option, "fractional") == 0) {
		gbc->fractional_scaling = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "frame-blending") == 0) {
		gbc->frame_blending = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "interlacing") == 0) {
		gbc->interlacing = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "palette") == 0) {
		gbc->core.ppu.palette = gbcc_get_palette(value);
	} else if (strcasecmp(option, "shader") == 0) {
		strncpy(gbc->default_shader, value, N_ELEM(gbc->default_shader));
		gbc->default_shader[N_ELEM(gbc->default_shader) - 1] = '\0';
	} else if (strcasecmp(option, "save-dir") == 0) {
		strncpy(gbc->save_directory, value, sizeof(gbc->save_directory));
		gbc->save_directory[N_ELEM(gbc->save_directory) - 1] = '\0';
	} else if (strcasecmp(option, "turbo") == 0) {
		errno = 0;
		char *endptr;
		gbc->turbo_speed = strtof(value, &endptr);
		if (endptr == value) {
			PARSE_ERROR(lineno, "Failed to parse \"%s\" as float.\n", value);
		} else if (errno) {
			PARSE_ERROR(lineno, "Float value \"%s\" out of range.\n", value);
		}
	} else if (strcasecmp(option, "vsync") == 0) {
		gbc->core.sync_to_video = parse_bool(lineno, value, &err);
	} else if (strcasecmp(option, "vram-window") == 0) {
		gbc->vram_display = parse_bool(lineno, value, &err);
	} else {
		PARSE_ERROR(lineno, "Bad config file option \"%s\"\n", option);
		err = true;
	}
	return !err;
}

char *get_config_path()
{
	char *base_dir = getenv("XDG_CONFIG_HOME");
	char *ext = "";
	size_t len = strlen("/gbcc/config") + 1;
	if (!base_dir) {
		base_dir = getenv("HOME");
		ext = "/.config";
		if (!base_dir) {
			gbcc_log_error("Couldn't find XDG_CONFIG_HOME or HOME envvars\n");
			return NULL;
		}
	}
	len += strlen(base_dir) + strlen(ext) + 2;
	char *name = calloc(len, sizeof(*name));
	snprintf(name, len, "%s%s%s", base_dir, ext, "/gbcc/config");
	return name;
}

bool parse_bool(size_t lineno, const char *str, bool *err)
{
	if (strcasecmp(str, "true") == 0) {
		return true;
	} else if (strcasecmp(str, "false") == 0) {
		return false;
	}
	PARSE_ERROR(lineno, "Invalid boolean value \"%s\".\n", str);
	if (err) {
		*err = true;
	}
	return false;
}
