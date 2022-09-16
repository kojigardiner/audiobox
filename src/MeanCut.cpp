#include "MeanCut.h"

#include "ArduinoSort.h"
#include "Utils.h"

// Takes an rgb565 value and and a pointer to an array of 3 uint8_t values in which to store the rgb888 equivalent
void rgb565_to_rgb888(uint16_t rgb565_val, uint8_t *rgb888_arr) {
    uint8_t r5 = (rgb565_val >> 11) & 0x1F;
    uint8_t g6 = (rgb565_val >> 5) & 0x3F;
    uint8_t b5 = (rgb565_val)&0x1F;

    rgb888_arr[0] = int(round((float(r5) / 31) * 255));
    rgb888_arr[1] = int(round((float(g6) / 63) * 255));
    rgb888_arr[2] = int(round((float(b5) / 31) * 255));
}

// Determine which color channel has the largest max/min difference
int get_longest_dim(uint16_t *rgb565_arr, uint32_t length) {
    uint8_t rgb888_arr[3];
    uint8_t rgb_max_arr[3] = {0, 0, 0};
    uint8_t rgb_min_arr[3] = {255, 255, 255};
    uint8_t rgb_ranges[3];

    // loop through all pixels
    for (int i = 0; i < length; i++) {
        rgb565_to_rgb888(rgb565_arr[i], rgb888_arr);  // decode current pixel into rgb888_arr

        // loop through rgb components
        for (int c = 0; c < 3; c++) {
            if (rgb888_arr[c] > rgb_max_arr[c]) {
                rgb_max_arr[c] = rgb888_arr[c];
            }
            if (rgb888_arr[c] < rgb_min_arr[c]) {
                rgb_min_arr[c] = rgb888_arr[c];
            }
        }
    }

    for (int c = 0; c < 3; c++) {
        rgb_ranges[c] = rgb_max_arr[c] - rgb_min_arr[c];
    }

    if ((rgb_ranges[0] >= rgb_ranges[1]) && (rgb_ranges[0] >= rgb_ranges[1])) {
        return 0;
    }
    if ((rgb_ranges[1] >= rgb_ranges[0]) && (rgb_ranges[1] >= rgb_ranges[2])) {
        return 1;
    }
    if ((rgb_ranges[2] >= rgb_ranges[0]) && (rgb_ranges[2] >= rgb_ranges[1])) {
        return 2;
    }

    return -1;  // should never get here
}

// Sorting function for red color channel
bool red_is_larger(uint16_t first, uint16_t second) {
    uint8_t rgb888_first_arr[3];
    uint8_t rgb888_second_arr[3];

    // Convert to rgb888
    rgb565_to_rgb888(first, rgb888_first_arr);
    rgb565_to_rgb888(second, rgb888_second_arr);

    if (rgb888_first_arr[0] > rgb888_second_arr[0]) {
        return true;
    } else {
        return false;
    }
}

// Sorting function for green color channel
bool green_is_larger(uint16_t first, uint16_t second) {
    uint8_t rgb888_first_arr[3];
    uint8_t rgb888_second_arr[3];

    // Convert to rgb888
    rgb565_to_rgb888(first, rgb888_first_arr);
    rgb565_to_rgb888(second, rgb888_second_arr);

    if (rgb888_first_arr[1] > rgb888_second_arr[1]) {
        return true;
    } else {
        return false;
    }
}

// Sorting function for blue color channel
bool blue_is_larger(uint16_t first, uint16_t second) {
    uint8_t rgb888_first_arr[3];
    uint8_t rgb888_second_arr[3];

    // Convert to rgb888
    rgb565_to_rgb888(first, rgb888_first_arr);
    rgb565_to_rgb888(second, rgb888_second_arr);

    if (rgb888_first_arr[2] > rgb888_second_arr[2]) {
        return true;
    } else {
        return false;
    }
}

// Sorting function for hue
bool hue_is_larger(rgb888_t first, rgb888_t second) {
    uint8_t first_hue = rgb888_to_hue(first);
    uint8_t second_hue = rgb888_to_hue(second);

    if (first_hue > second_hue) {
        return true;
    } else {
        return false;
    }
}

// Calculates hue
uint8_t rgb888_to_hue(rgb888_t rgb888) {
    // normalize
    float r_norm = rgb888.r / 255.0;
    float g_norm = rgb888.g / 255.0;
    float b_norm = rgb888.b / 255.0;

    float x_max = max(max(r_norm, g_norm), b_norm);
    float x_min = min(min(r_norm, g_norm), b_norm);
    float chroma = x_max - x_min;
    float value = x_max;

    float hue = 0;

    if (chroma == 0) {
        hue = 0;
    } else if (value == r_norm) {
        hue = 60 * (0 + (g_norm - b_norm) / chroma);
    } else if (value == g_norm) {
        hue = 60 * (2 + (b_norm - r_norm) / chroma);
    } else if (value == b_norm) {
        hue = 60 * (4 + (r_norm - g_norm) / chroma);
    } else {
        // should never get here
        print("Error in hue computation\n");
    }

    return uint8_t(round(hue / 360.0 * 255));
}

// Using Rec.709 Y conversion
uint8_t rgb888_to_luma(uint8_t *rgb888_arr) {
    return 0.2126 * rgb888_arr[0] + 0.7152 * rgb888_arr[1] + 0.0722 * rgb888_arr[2];
}

// Reverse sorting function for luma
bool luma_is_smaller(uint16_t first, uint16_t second) {
    uint8_t rgb888_first_arr[3];
    uint8_t rgb888_second_arr[3];

    // Convert to rgb888
    rgb565_to_rgb888(first, rgb888_first_arr);
    rgb565_to_rgb888(second, rgb888_second_arr);

    uint8_t luma_first = rgb888_to_luma(rgb888_first_arr);
    uint8_t luma_second = rgb888_to_luma(rgb888_second_arr);

    if (luma_first < luma_second) {
        return true;
    } else {
        return false;
    }
}

uint32_t length_above_luma_thresh(uint16_t *rgb565_arr, uint32_t length, uint8_t threshold) {
    sortArray(rgb565_arr, length, luma_is_smaller);  // sort by descending luma
    uint32_t idx;
    uint8_t rgb888_arr[3];
    uint8_t luma;

    for (idx = 0; idx < length; idx++) {
        rgb565_to_rgb888(rgb565_arr[idx], rgb888_arr);
        luma = rgb888_to_luma(rgb888_arr);
        if (luma < threshold) break;
    }

    return idx;
}

void sort_by_channel(uint16_t *rgb565_arr, uint32_t length, uint8_t channel) {
    switch (channel) {
        case 0:
            sortArray(rgb565_arr, length, red_is_larger);
            break;
        case 1:
            sortArray(rgb565_arr, length, green_is_larger);
            break;
        case 2:
            sortArray(rgb565_arr, length, blue_is_larger);
            break;
    }
}

// Calculates the mean rgb888 value from an array of rgb565 values, and stores the result in a 3-element array passed in as an arg
void get_mean_color(uint16_t *rgb565_arr, uint32_t length, uint8_t *mean_rgb888_arr) {
    if (length == 0) {  // avoid a divide by zero
        return;
    }

    uint32_t sum_rgb888_arr[3] = {0, 0, 0};
    uint8_t curr_rgb888_arr[3];

    // loop through all pixels
    for (int i = 0; i < length; i++) {
        rgb565_to_rgb888(rgb565_arr[i], curr_rgb888_arr);

        // loop through each channel
        for (int c = 0; c < 3; c++) {
            sum_rgb888_arr[c] += curr_rgb888_arr[c];
        }
    }

    // loop through each channel
    for (int c = 0; c < 3; c++) {
        mean_rgb888_arr[c] = int(round(float(sum_rgb888_arr[c]) / length));
    }
}

// Finds the closest index to the mean for the sorted longest_dim channel
uint32_t get_mean_idx(uint16_t *rgb565_arr, uint32_t length, uint8_t channel) {
    if (length == 0) {  // avoid a divide by zero
        return 0;
    }

    uint32_t sum = 0;
    uint8_t curr_rgb888_arr[3];
    float mean_val;
    float min_diff = 255;
    uint32_t idx = 0;

    // Find the channel mean
    for (int i = 0; i < length; i++) {
        rgb565_to_rgb888(rgb565_arr[i], curr_rgb888_arr);
        sum += curr_rgb888_arr[channel];
    }
    mean_val = float(sum) / length;

    // Find the closest idx to the mean for the channel we care about
    for (int i = 0; i < length; i++) {
        rgb565_to_rgb888(rgb565_arr[i], curr_rgb888_arr);
        float curr_diff = abs(curr_rgb888_arr[channel] - mean_val);
        if (curr_diff < min_diff) {
            min_diff = curr_diff;
            idx = i;
        }
    }

    return idx;
}

// Prepares the data before calling the recursive function that will actually compute the results
void mean_cut(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results) {
    uint32_t new_length = length_above_luma_thresh(rgb565_arr, length, LUMA_THRESH);  // sort by luma and return how many values are above the threshold

    mean_cut_recursive(rgb565_arr, new_length, depth, results);

    // cast to rgb888_t struct type to enable accessing rgb elements in sorting function
    sortArray((rgb888_t *)results, 1 << depth, hue_is_larger);  // sort by increasing hue
}

// Recursive function that actually computes the results
// Results will hold an array of 8-bit RGB triplets (e.g. {R1, G1, B1, R2, G2, B2, R3, G3, B3, ...})
void mean_cut_recursive(uint16_t *rgb565_arr, uint32_t length, uint8_t depth, uint8_t *results) {
    if ((depth == 0) || (length == 0)) {  // if length is 0 we will get black
        // uint8_t mean_rgb888_arr[3];
        get_mean_color(rgb565_arr, length, results);
        return;
    }

    uint8_t longest_dim = get_longest_dim(rgb565_arr, length);

    sort_by_channel(rgb565_arr, length, longest_dim);
    uint32_t split_idx = get_mean_idx(rgb565_arr, length, longest_dim);

    // Determine lengths of each half of the array we are splitting into
    uint32_t length0 = split_idx;           // int(length / 2);
    uint32_t length1 = length - split_idx;  // length - length0;

    // Copy the contents of our original array into the new split arrays
    uint16_t rgb565_arr_0[length0];
    memcpy(rgb565_arr_0, rgb565_arr, sizeof(rgb565_arr_0));

    uint16_t rgb565_arr_1[length1];
    memcpy(rgb565_arr_1, rgb565_arr + length0, sizeof(rgb565_arr_1));

    // Spawn two recursive calls to handle each "half" of the pixels
    mean_cut(rgb565_arr_0, length0, depth - 1, results);

    // Pointer arithmetic to pass the appropriate array for the second half
    // (1 << (depth - 1) is half the total number of entries, 3 is the number of channels
    uint8_t *results1_ptr = results + ((1 << (depth - 1)) * 3 * sizeof(results[0]));
    mean_cut(rgb565_arr_1, length1, depth - 1, results1_ptr);
}
