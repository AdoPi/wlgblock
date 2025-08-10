/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_ARGS_H
#define GBCC_ARGS_H

#include "gbcc.h"

bool gbcc_parse_args(struct gbcc *gbc, bool file_required, int argc, char **argv);

#endif /* GBCC_ARGS_H */
