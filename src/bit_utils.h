/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_BIT_UTILS_H
#define GBCC_BIT_UTILS_H

#include <stdbool.h>
#include <stdint.h>

uint8_t high_byte(uint16_t x);
uint8_t low_byte(uint16_t x);
uint16_t cat_bytes(uint8_t low, uint8_t high);

uint8_t set_bit(uint8_t byte, uint8_t bit);
uint8_t clear_bit(uint8_t byte, uint8_t bit);
uint8_t toggle_bit(uint8_t byte, uint8_t bit);
uint8_t check_bit(uint8_t byte, uint8_t bit);
uint16_t check_bit16(uint16_t value, uint8_t b);
uint8_t cond_bit(uint8_t byte, uint8_t b, bool cond);


uint8_t bit(uint8_t b);
uint16_t bit16(uint8_t b);

#endif /* GBCC_BIT_UTILS_H */
