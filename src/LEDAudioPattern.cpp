#include "LEDAudioPattern.h"

#include "LEDPanel.h"

// Constructor for base class, called by all subclasses
LEDAudioPattern::LEDAudioPattern(LEDPanel *lp) {
    this->_lp = lp;  // set pointer to LED panel that will be updated
}

LEDAudioPattern::~LEDAudioPattern() {
}

// Initialize static variable used to maintain consistency of noise pattern across mode changes
uint8_t LEDNoisePattern::_ihue = 0;
uint16_t LEDNoisePattern::_x = random16();
uint16_t LEDNoisePattern::_y = random16();
uint16_t LEDNoisePattern::_z = random16();
uint16_t LEDNoisePattern::_target_x = _x;
uint16_t LEDNoisePattern::_target_y = _y;
uint16_t LEDNoisePattern::_speed = 1;
uint16_t LEDNoisePattern::_scale = 40;
uint16_t LEDNoisePattern::_target_scale = _scale;
uint8_t LEDNoisePattern::_noise[GRID_H][GRID_W] = {0};

// Fill the x/y array of 8-bit noise values using the inoise8 function. From FastLED.
void LEDNoisePattern::_fill_noise8() {
    // If we're runing at a low "speed", some 8-bit artifacts become visible
    // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
    // The amount of data smoothing we're doing depends on "speed".
    uint8_t dataSmoothing = 0;

    _scale = int(round((_target_scale * 0.3) + (_scale * 0.7)));  // slowly move toward the new target_scale
    _x = int(round((_target_x * 0.3) + (_x * 0.7)));              // slowly move toward the new target_x
    _y = int(round((_target_y * 0.3) + (_y * 0.7)));              // slowly move toward the new target_y

    if (_speed < 50) {
        dataSmoothing = 200 - (_speed * 4);
    }

    for (int i = 0; i < GRID_W; i++) {
        int ioffset = _scale * (i - int(GRID_W / 2));  // center the scale shift
        for (int j = 0; j < GRID_H; j++) {
            int joffset = _scale * (j - int(GRID_H / 2));  // center the scale shift

            uint8_t data = inoise8(_x + ioffset, _y + joffset, _z);

            // The range of the inoise8 function is roughly 16-238.
            // These two operations expand those values out to roughly 0..255
            // You can comment them out if you want the raw noise data.
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));

            if (dataSmoothing) {
                uint8_t olddata = _noise[i][j];
                uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
                data = newdata;
            }

            _noise[i][j] = data;
        }
    }

    _z += _speed;

    // apply slow drift to X and Y, just for visual variation.
    _x += _speed / 8;
    _y -= _speed / 16;
}

// Sets LEDs based on noise grid and current palette. From FastLED.
void LEDNoisePattern::_map_noise_to_leds_using_palette() {
    for (int i = 0; i < GRID_W; i++) {
        for (int j = 0; j < GRID_H; j++) {
            // We use the value at the (i,j) coordinate in the noise
            // array for our brightness, and the flipped value from (j,i)
            // for our pixel's index into the color palette.

            uint8_t index = _noise[j][i];
            uint8_t bri = _noise[i][j];

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

            CRGB color = ColorFromPalette(_lp->get_palette(), index, bri);
            _lp->set_xy(i, j, color);
        }
    }

    _ihue += 1;
}

// Generates a simplex noise pattern of LEDs. Based on FastLED implementation.
void LEDNoisePattern::set_leds(int *intensity, double tempo) {
    _fill_noise8();
    _map_noise_to_leds_using_palette();
}

// Generates vertical bar pattern, with peaks that decay over time. Based on ESP32 FFT VU code.
void LEDBarsPattern::set_leds(int *intensity, double tempo) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        // Move peak up
        if (max_y > _peaks[bar_x]) {
            _peaks[bar_x] = max_y;
        }

        // Light up or darken each grid element
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            if (bar_y < max_y) {
                int color_index = int(round(float(bar_x) / GRID_W * 255));
                CRGB color = ColorFromPalette(_lp->get_palette(), color_index, 255, _lp->get_blending());
                _lp->set_xy(bar_x, bar_y, color);
            } else {
                _lp->set_xy(bar_x, bar_y, CRGB::Black);
            }
        }
        _lp->set_xy(bar_x, _peaks[bar_x], CRGB::White);  // light up the peak

        if (_counter % PEAK_DECAY_RATE == 0) {  // every X frames, shift peak down
            if (_peaks[bar_x] > 0) {
                _peaks[bar_x] -= 1;
            }
        }
    }
    _counter++;
}

// Generates vertical peaks that decay and change color over time. Based on ESP32 FFT VU code.
void LEDOutrunBarsPattern::set_leds(int *intensity, double tempo) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        // Move peak up
        if (max_y > _peaks[bar_x]) {
            _peaks[bar_x] = max_y;
        }

        // Only light up the peak
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            _lp->set_xy(bar_x, bar_y, CRGB::Black);
        }
        int color_index = int(round(float(_peaks[bar_x]) / GRID_H * 255));
        CRGB color = ColorFromPalette(_lp->get_palette(), color_index, 255, _lp->get_blending());

        _lp->set_xy(bar_x, _peaks[bar_x], color);  // light up the peak

        if (_counter % PEAK_DECAY_RATE == 0) {  // every X frames, shift peak down
            if (_peaks[bar_x] > 0) {
                _peaks[bar_x] -= 1;
            }
        }
    }
    _counter++;
}

// Generates centered symmetric vertical bar pattern. Based on ESP32 FFT VU code.
void LEDCenterBarsPattern::set_leds(int *intensity, double tempo) {
    for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
        int max_y = int(round((float(intensity[bar_x]) / 255) * GRID_H));  // scale the intensity by the grid height
        max_y = constrain(max_y, 0, GRID_H - 1);

        if (max_y % 2 == 0) max_y--;  // since we are only going to display half the bars, make sure we have an odd value

        int y_start = ((GRID_H - max_y) / 2);
        for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
            if (bar_y >= y_start && bar_y <= (y_start + max_y)) {
                int color_index = constrain((bar_y - y_start) * (255 / max_y), 0, 255);
                CRGB color = ColorFromPalette(_lp->get_palette(), color_index, 255, _lp->get_blending());

                _lp->set_xy(bar_x, bar_y, color);
            } else {
                _lp->set_xy(bar_x, bar_y, CRGB::Black);
            }
        }
    }
}

// Generates side-scrolling spectrogram. Based on ESP32 FFT VU code.
void LEDWaterfallPattern::set_leds(int *intensity, double tempo) {
    for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
        // Draw right line
        //_lp->set_xy(GRID_W - 1, bar_y, CHSV(constrain(map(intensity[bar_y], 0, 255, 160, 0), 0, 160), 255, 255));
        _lp->set_xy(GRID_W - 1, bar_y, ColorFromPalette(_lp->get_palette(), intensity[bar_y], 255, _lp->get_blending()));
        //_lp->set_xy(GRID_W - 1, bar_y, CHSV(constrain(intensity[bar_y], rgb2hsv_approximate(_lp->get_palette()[0]).h, rgb2hsv_approximate(_lp->get_palette()[15]).h), 255, 255));

        // Move screen left starting at 2nd row from left
        if (bar_y == GRID_H - 1) {  // do this on the last cycle
            for (int bar_x = 1; bar_x < GRID_W; bar_x++) {
                for (int y = 0; y < GRID_H; y++) {
                    int pixelIndexX = _lp->grid_to_idx(bar_x - 1, y);
                    int pixelIndex = _lp->grid_to_idx(bar_x, y);
                    _lp->set(pixelIndexX, _lp->get(pixelIndex));
                    //_lp->leds[pixelIndexX] = _lp->leds[pixelIndex];
                }
            }
        }
    }
}

// Generates left-right symmetric serpentine grid pattern that illuminates and fades over time.
void LEDSymSnakeGridPattern::set_leds(int *intensity, double tempo) {
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
                _lp->set(GRID_W * i + GRID_W / 2 - 1 - j, ColorFromPalette(_lp->get_palette(), int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), _lp->get_blending()));
                color_index += int(round(255.0 / (NUM_LEDS / 2.0)));
                curr_index += 1;
            }
        } else {
            for (int j = 0; j < GRID_W / 2; j++) {
                _lp->set(GRID_W * (i + 1) - 1 - j, ColorFromPalette(_lp->get_palette(), int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), _lp->get_blending()));
                color_index += int(round(255.0 / (NUM_LEDS / 2.0)));
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
                _lp->set(GRID_W * i + GRID_W / 2 + j, ColorFromPalette(_lp->get_palette(), int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), _lp->get_blending()));
                // TODO: Pull color from palette first at intensity[curr_index]; _then_ apply gamma to adjust.
                // TODO: Use same gammas for RGB that are used on RaspPi
                color_index += int(round(255.0 / (NUM_LEDS / 2.0)));
                curr_index += 1;
            }
        } else {
            for (int j = 0; j < GRID_W / 2; j++) {
                _lp->set(GRID_W * i + j, ColorFromPalette(_lp->get_palette(), int(color_index), pgm_read_byte(&GAMMA8[int(intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), _lp->get_blending()));
                color_index += int(round(255.0 / (NUM_LEDS / 2.0)));
                curr_index += 1;
            }
        }
    }
}
