/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_DEBUG_H
#define GBCC_DEBUG_H

#include "core.h"

void gbcc_print_registers(struct gbcc_core *gbc, bool debug);
void gbcc_print_op(struct gbcc_core *gbc);
__attribute__((format (printf, 1, 2)))
void gbcc_log_error(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_warning(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_debug(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_info(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_append_error(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_append_warning(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_append_debug(const char *fmt, ...);
__attribute__((format (printf, 1, 2)))
void gbcc_log_append_info(const char *fmt, ...);
void gbcc_vram_dump(struct gbcc_core *gbc, const char *filename);
void gbcc_sram_dump(struct gbcc_core *gbc, const char *filename);

#endif /* GBCC_DEBUG_H */
