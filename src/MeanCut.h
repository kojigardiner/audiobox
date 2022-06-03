#ifndef MEAN_CUT_H
#define MEAN_CUT_H

#define LUMA_THRESH 40  // threshold to determine pixels that are bright enough to consider

#include <Arduino.h>

// rgb888 struct type
struct rgb888_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

uint8_t rgb888_to_hue(rgb888_t rgb888);
uint8_t rgb888_to_luma(uint8_t *rgb888_arr);
void rgb565_to_rgb888(uint16_t rgb565_val, uint8_t *rgb888_arr);
int get_longest_dim(uint16_t *rgb565_arr, uint32_t length);
bool red_is_larger(uint16_t first, uint16_t second);
bool green_is_larger(uint16_t first, uint16_t second);
bool blue_is_larger(uint16_t first, uint16_t second);
bool hue_is_larger(rgb888_t first, rgb888_t second);
void sort_by_channel(uint16_t *rgb565_arr, uint32_t length, uint8_t channel);
void get_mean_color(uint16_t *rgb565_arr, uint32_t length, uint8_t *mean_rgb888_arr);
void mean_cut(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);
void mean_cut_recursive(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);

#endif
