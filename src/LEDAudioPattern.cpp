#include "LEDAudioPattern.h"

#include "LEDPanel.h"

// constructor
LEDAudioPattern::LEDAudioPattern(LEDPanel *lp) {
    this->lp = lp;
}

LEDAudioPattern::~LEDAudioPattern() {
}

// Initialize static variables
uint8_t LEDNoisePattern::ihue = 0;
uint16_t LEDNoisePattern::x = random16();
uint16_t LEDNoisePattern::y = random16();
uint16_t LEDNoisePattern::z = random16();
uint16_t LEDNoisePattern::target_x = x;
uint16_t LEDNoisePattern::target_y = y;
uint16_t LEDNoisePattern::speed = 1;
uint16_t LEDNoisePattern::scale = 40;
uint16_t LEDNoisePattern::target_scale = scale;
uint8_t LEDNoisePattern::noise[GRID_H][GRID_W] = {0};

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void LEDNoisePattern::_fill_noise8() {
    // If we're runing at a low "speed", some 8-bit artifacts become visible
    // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
    // The amount of data smoothing we're doing depends on "speed".
    uint8_t dataSmoothing = 0;

    scale = int(round((target_scale * 0.3) + (scale * 0.7)));  // slowly move toward the new target_scale
    x = int(round((target_x * 0.3) + (x * 0.7)));              // slowly move toward the new target_x
    y = int(round((target_y * 0.3) + (y * 0.7)));              // slowly move toward the new target_y

    if (speed < 50) {
        dataSmoothing = 200 - (speed * 4);
    }

    for (int i = 0; i < GRID_W; i++) {
        int ioffset = scale * (i - int(GRID_W / 2));  // center the scale shift
        for (int j = 0; j < GRID_H; j++) {
            int joffset = scale * (j - int(GRID_H / 2));  // center the scale shift

            uint8_t data = inoise8(x + ioffset, y + joffset, z);

            // The range of the inoise8 function is roughly 16-238.
            // These two operations expand those values out to roughly 0..255
            // You can comment them out if you want the raw noise data.
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));

            if (dataSmoothing) {
                uint8_t olddata = noise[i][j];
                uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
                data = newdata;
            }

            noise[i][j] = data;
        }
    }

    z += speed;

    // apply slow drift to X and Y, just for visual variation.
    // x += speed / 8;
    // y -= speed / 16;
}

void LEDNoisePattern::_map_noise_to_leds_using_palette() {
    for (int i = 0; i < GRID_W; i++) {
        for (int j = 0; j < GRID_H; j++) {
            // We use the value at the (i,j) coordinate in the noise
            // array for our brightness, and the flipped value from (j,i)
            // for our pixel's index into the color palette.

            uint8_t index = noise[j][i];
            uint8_t bri = noise[i][j];

            // // if this palette is a 'loop', add a slowly-changing base value
            // if( colorLoop) {
            //   index += ihue;
            // }

            // brighten up, as the color palette itself often contains the
            // light/dark dynamic range desired
            if (bri > 127) {
                bri = 255;
            } else {
                bri = dim8_raw(bri * 2);
            }

            CRGB color = ColorFromPalette(lp->curr_palette, index, bri);
            lp->set_xy(i, j, color);
        }
    }

    ihue += 1;
}

void LEDNoisePattern::set_leds(int *intensity) {
    // if (ap->beat_detected) {
    //     uint8_t change_type = random8(6);  // select a random behavior

    //     switch (change_type) {  // either change the scale or scan in XY
    //         case 0:
    //             target_scale = constrain(scale + random8(30, 40), 20, 50);
    //             break;
    //         case 1:
    //             target_scale = constrain(scale - random8(30, 40), 20, 50);
    //             break;
    //         case 2:
    //             target_x = constrain(x + 16 * (random8(3, 7)) * scale / 10, 0, 65535);  // scale by "scale" so that the moves look similar
    //             break;
    //         case 3:
    //             target_y = constrain(y + 16 * (random8(3, 7)) * scale / 10, 0, 65535);
    //             break;
    //         case 4:
    //             target_x = constrain(x - 16 * (random8(3, 7)) * scale / 10, 0, 65535);
    //             break;
    //         case 5:
    //             target_y = constrain(y - 16 * (random8(3, 7)) * scale / 10, 0, 65535);
    //             break;
    //     }
    // }

    // generate noise data
    _fill_noise8();

    // convert the noise data to colors in the LED array
    // using the current palette
    _map_noise_to_leds_using_palette();
}

void LEDBarsPattern::set_leds(int *intensity) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        // Move peak up
        if (max_y > peaks[bar_x]) {
            peaks[bar_x] = max_y;
        }

        // Light up or darken each grid element
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            if (bar_y < max_y) {
                int color_index = int(round(float(bar_x) / GRID_W * 255));
                CRGB color = ColorFromPalette(lp->curr_palette, color_index, 255, lp->curr_blending);
                lp->set_xy(bar_x, bar_y, color);
            } else {
                lp->set_xy(bar_x, bar_y, CRGB::Black);
            }
        }
        lp->set_xy(bar_x, peaks[bar_x], CRGB::White);  // light up the peak

        if (counter % PEAK_DECAY_RATE == 0) {  // every X frames, shift peak down
            if (peaks[bar_x] > 0) {
                peaks[bar_x] -= 1;
            }
        }
    }
    counter++;
}

void LEDOutrunBarsPattern::set_leds(int *intensity) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        // Move peak up
        if (max_y > peaks[bar_x]) {
            peaks[bar_x] = max_y;
        }

        // Only light up the peak
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            lp->set_xy(bar_x, bar_y, CRGB::Black);
        }
        int color_index = int(round(float(peaks[bar_x]) / GRID_H * 255));
        CRGB color = ColorFromPalette(lp->curr_palette, color_index, 255, lp->curr_blending);

        lp->set_xy(bar_x, peaks[bar_x], color);  // light up the peak

        if (counter % PEAK_DECAY_RATE == 0) {  // every X frames, shift peak down
            if (peaks[bar_x] > 0) {
                peaks[bar_x] -= 1;
            }
        }
    }
    counter++;
}

void LEDCenterBarsPattern::set_leds(int *intensity) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        if (max_y % 2 == 0) max_y--;  // since we are only going to display half the bars, make sure we have an odd value

        int y_start = ((GRID_H - max_y) / 2);
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            if (bar_y >= y_start && bar_y <= (y_start + max_y)) {
                int color_index = constrain((bar_y - y_start) * (255 / max_y), 0, 255);
                CRGB color = ColorFromPalette(lp->curr_palette, color_index, 255, lp->curr_blending);

                lp->set_xy(bar_x, bar_y, color);
            } else {
                lp->set_xy(bar_x, bar_y, CRGB::Black);
            }
        }
    }
}

void LEDWaterfallPattern::set_leds(int *intensity) {
    for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
        // Draw right line
        lp->set_xy(GRID_W - 1, bar_y, CHSV(constrain(map(intensity[bar_y], 0, 255, 160, 0), 0, 160), 255, 255));

        // Move screen left starting at 2nd row from left
        if (bar_y == GRID_H - 1) {  // do this on the last cycle
            for (int bar_x = 1; bar_x < GRID_W; bar_x++) {
                for (int y = 0; y < GRID_H; y++) {
                    int pixelIndexX = lp->grid_to_idx(bar_x - 1, y);
                    int pixelIndex = lp->grid_to_idx(bar_x, y);
                    lp->leds[pixelIndexX] = lp->leds[pixelIndex];
                }
            }
        }
    }
}

void LEDSymSnakeGridPattern::set_leds(int *intensity) {
    // Left half of array
    /*
     * 0: 3 -> 0     (width * i + width/2 - 1)::-1
     * 1: 15 -> 12   (width * (i + 1) - 1)::-1
     * 2: 19 -> 16   (width * i + width/2 - 1)::-1
     * 3: 31 -> 28   (width * (i + 1) - 1)::-1
     * 4: 35 -> 32   (width * i + width/2 - 1)::-1
     * 5: 47 -> 44   (width * (i + 1) - 1)::-1
     * 6: 51 -> 48   (width * i + width/2 - 1)::-1
     * 7: 63 -> 60   (width * (i + 1) - 1)::-1
     */

    int curr_index = 0;
    int color_index = 0;
    for (int i = 0; i < GRID_H; i++) {
        if (i % 2 == 0) {
            for (int j = 0; j < GRID_W / 2; j++) {
                lp->leds[GRID_W * i + GRID_W / 2 - 1 - j] = ColorFromPalette(lp->curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), lp->curr_blending);
                color_index += 255.0 / (NUM_LEDS / 2);
                curr_index += 1;
            }
        } else {
            for (int j = 0; j < GRID_W / 2; j++) {
                lp->leds[GRID_W * (i + 1) - 1 - j] = ColorFromPalette(lp->curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), lp->curr_blending);
                color_index += 255.0 / (NUM_LEDS / 2);
                curr_index += 1;
            }
        }
    }

    // Right half of array
    /*
     * 0: 4 -> 7     (width * i + width/2)::+1
     * 1: 8 -> 11    (width * i)::+1
     * 2: 20 -> 23   (width * i + width/2)::+1
     * 3: 24 -> 27   (width * i)::+1
     * 4: 36 -> 39   (width * i + width/2)::+1
     * 5: 40 -> 43   (width * i)::+1
     * 6: 52 -> 55   (width * i + width/2)::+1
     * 7: 56 -> 59   (width * i)::+1
     */
    curr_index = 0;
    color_index = 0;
    CRGB curr_color;
    for (int i = 0; i < GRID_H; i++) {
        if (i % 2 == 0) {
            for (int j = 0; j < GRID_W / 2; j++) {
                lp->leds[GRID_W * i + GRID_W / 2 + j] = ColorFromPalette(lp->curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), lp->curr_blending);
                // TODO: Pull color from palette first at intensity[curr_index]; _then_ apply gamma to adjust.
                // TODO: Use same gammas for RGB that are used on RaspPi
                color_index += 255.0 / (NUM_LEDS / 2);
                curr_index += 1;
            }
        } else {
            for (int j = 0; j < GRID_W / 2; j++) {
                lp->leds[GRID_W * i + j] = ColorFromPalette(lp->curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), lp->curr_blending);
                color_index += 255.0 / (NUM_LEDS / 2);
                curr_index += 1;
            }
        }
    }
}

void LEDScrollingPattern::set_leds(int *intensity) {
    // Generate an extra wide representation of the colors
    int effective_w = GRID_W * SCROLL_AVG_FACTOR;
    int avg_r, avg_g, avg_b;

    int color_index = 0;

    // For leftmost columns, grab data from the next column over
    // For the last column, actually take the audio data
    for (int x = 0; x < effective_w; x++) {
        for (int y = 0; y < GRID_H; y++) {
            if (x == effective_w - 1) {
                lp->colors_grid_wide[x][y] = ColorFromPalette(lp->curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(intensity[y] * 255.0 / double(BRIGHT_LEVELS))]), lp->curr_blending);
                color_index += 255.0 / (GRID_H);
            } else {
                lp->colors_grid_wide[x][y] = lp->colors_grid_wide[x + 1][y];
                // colors_grid_wide[x][y].fadeToBlackBy(32); // note this is an exponential decay by XX/255. 32 makes this look like a standard spectrogram
            }
        }
    }
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H; y++) {
            avg_r = 0;
            avg_g = 0;
            avg_b = 0;
            for (int i = 0; i < SCROLL_AVG_FACTOR; i++) {
                avg_r += lp->colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].red;
                avg_g += lp->colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].green;
                avg_b += lp->colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].blue;
            }

            lp->set_xy(x, y, CRGB(avg_r / SCROLL_AVG_FACTOR, avg_g / SCROLL_AVG_FACTOR, avg_b / SCROLL_AVG_FACTOR));
        }
    }
}