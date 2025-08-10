/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_TIME_DIFF_H
#define GBCC_TIME_DIFF_H

#include <stdint.h>
#include <time.h>

#define SECOND 1000000000ul
#define MINUTE 60000000000ul
#define HOUR 3600000000000ul
#define DAY 86400000000000ul

uint64_t gbcc_time_diff(const struct timespec *cur, const struct timespec *old);

#endif /* GBCC_TIME_DIFF_H */
