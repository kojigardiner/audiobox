#ifndef _LEDPANEL_H
#define _LEDPANEL_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// forward declaration
class LEDAudioPattern;

class LEDPanel {
   public:
    // Constructors
    LEDPanel();
    ~LEDPanel();

    // Methods
    void init(uint8_t led_pin, int num_leds, uint8_t brightness);
    int grid_to_idx(int x, int y, bool start_top_left = false);

    void set(int idx, CRGB value);
    void set_xy(int x, int y, CRGB value, bool start_top_left = false);

    CRGB get(int idx);
    CRGB get_xy(int x, int y, bool start_top_left = false);

    void set_audio_pattern(int mode);
    void display_audio(int *intensity);

    // Variables
    CRGB leds[NUM_LEDS];  // array that will hold LED values

    CRGBPalette16 curr_palette;              // current color palette
    CRGBPalette16 target_palette;            // target pallette that we will blend toward over time
    TBlendType curr_blending = LINEARBLEND;  // type of blending to use between palettes

    CRGB colors_grid_wide[GRID_W * SCROLL_AVG_FACTOR][GRID_H] = {{CRGB::Black}};  // wider version of led array for use with the scrolling animatino

   private:
    // Variables
    LEDAudioPattern *_audio_pattern;
};

DECLARE_GRADIENT_PALETTE(Sunset_Real_gp);

#endif  // _LEDPANEL_H