#ifndef MEAN_CUT_H
#define MEAN_CUT_H

#include <Arduino.h>

void rgb565_to_rgb888(uint16_t rgb565_val, uint8_t *rgb888_arr);
int get_longest_dim(uint16_t *rgb565_arr, uint32_t length);
bool red_is_larger(uint16_t first, uint16_t second);
bool green_is_larger(uint16_t first, uint16_t second);
bool blue_is_larger(uint16_t first, uint16_t second);
void sort_by_channel(uint16_t *rgb565_arr, uint32_t length, uint8_t channel);
void get_mean_color(uint16_t *rgb565_arr, uint32_t length, uint8_t *mean_rgb888_arr);
void mean_cut(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);
void mean_cut_recursive(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);

#endif
