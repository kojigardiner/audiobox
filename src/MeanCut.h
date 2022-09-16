#ifndef _MEANCUT_H
#define _MEANCUT_H

#define LUMA_THRESH 40  // threshold to determine pixels that are bright enough to consider

#include <Arduino.h>

// This header and its associated MeanCut.cpp file define functions that take an array
// of pixels and find the most dominant colors using a recursive strategy similar to 
// median cut (https://en.wikipedia.org/wiki/Median_cut). The key difference is that the
// mean is used instead of the median to separate pixels during each iteration.
//
// The algorithm runs as follows:
//      1. Sort the pixels by their luma value and ignore all pixels below a specific
//          LUMA_THRESH (see Constants.h). This helps to ignore pixels that have little
//          color information and would not contribute to a pleasing display.
//      2. Find the color channel (R, G, or B) with the largest min/max difference.
//      3. Sort the pixels by the values in the channel determined in #2.
//      4. Find the index closest to the mean value of the channel determine #2.
//      5. Split the pixels into two buckets at the index determined in #4.
//      6. Recursively apply steps 2-5 on each bucket. Stop when a bucket has length 0
//          or when we have reached the max recursion depth MEAN_CUT_DEPTH (see Constants.h).
//          If we have reached the max recursion depth, return the average R, average G,
//          and average B value as the dominant color for that bucket.
//      7. Sort the resulting dominant colors by their hue. This helps provide a more pleasing
//          color palette to be used for LED visualizations.
//
// Finally, note that in this implementation the input is an array of 16-bit pixels encoded
// in RGB565 format (5 bits red, 6 bits green, 5 bits blue). This follows from the JPEG decoder
// used in the prior process in the program, which outputs pixels in this packed format. Thus
// there are a number of conversions that occur throughout the algorithm between RGB565 and
// the more typical RGB888 format.
//

struct rgb888_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Entry point for the mean cut algorithm, which serves as a wrapper for the recursive function
// mean_cut_recursive. Arguments:
//      rgb565_arr: an array of 16-bit RGB565 pixel values
//      length: the length of the rgb565_arr array
//      depth: the recursion depth. 2^depth colors will be returned by the algorithm.
//      result: an array in which the resulting dominant colors will be stored.
//          IMPORTANT: results must have a size of at least (2 ^ depth) * 3 bytes!
//          results will store an array of 8-bit RGB triplets (e.g. {R1, G1, B1, R2, G2, B2, R3, G3, B3, ...})
void mean_cut(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);

// Computes the most dominant colors recursively. Called by mean_cut().
void mean_cut_recursive(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results);

// Converts an RGB888 struct to an 8-bit hue value.
uint8_t rgb888_to_hue(rgb888_t rgb888);

// Converts an array of RGB88 values to an 8-bit luma value.
uint8_t rgb888_to_luma(uint8_t *rgb888_arr);

// Converts a 16-bit RGB565 value to a corresponding array of 8-bit RGB values
void rgb565_to_rgb888(uint16_t rgb565_val, uint8_t *rgb888_arr);

// Returns the color channel with the longest dimension (i.e. largest min/max difference).
// Takes in an RGB565 array of pixels and its length.
int get_longest_dim(uint16_t *rgb565_arr, uint32_t length);

// Comparators used by sorting functions.
bool red_is_larger(uint16_t first, uint16_t second);
bool green_is_larger(uint16_t first, uint16_t second);
bool blue_is_larger(uint16_t first, uint16_t second);
bool hue_is_larger(rgb888_t first, rgb888_t second);

// Sorts an RGB565 array of pixels along a specific color channel/dimension.
void sort_by_channel(uint16_t *rgb565_arr, uint32_t length, uint8_t channel);

// Calculates the average R, average G, average B for a given RGB565 array of pixels and
// stores the result in the mean_rgb888_arr.
void get_mean_color(uint16_t *rgb565_arr, uint32_t length, uint8_t *mean_rgb888_arr);

#endif // _MEANCUT