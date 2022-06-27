#ifndef _LEDPANEL_H
#define _LEDPANEL_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

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

    void display_audio(int mode, int *intensity);
    void set_leds_noise();
    void set_leds_bars(int *intensity);
    void set_leds_outrun_bars(int *intensity);
    void set_leds_center_bars(int *intensity);
    void set_leds_waterfall(int *intensity);
    void set_leds_sym_snake_grid(int *intensity);
    void set_leds_scrolling(int *intensity);

    // Variables
    CRGB leds[NUM_LEDS];  // array that will hold LED values

    CRGBPalette16 curr_palette;              // current color palette
    CRGBPalette16 target_palette;            // target pallette that we will blend toward over time
    TBlendType curr_blending = LINEARBLEND;  // type of blending to use between palettes

    CRGB colors_grid_wide[GRID_W * SCROLL_AVG_FACTOR][GRID_H] = {{CRGB::Black}};  // wider version of led array for use with the scrolling animatino

    // The 16 bit version of our coordinates
    uint16_t x;
    uint16_t y;
    uint16_t z;

    uint16_t target_x;
    uint16_t target_y;

    // We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
    // use the z-axis for "time".  speed determines how fast time moves forward.  Try
    // 1 for a very slow moving effect, or 60 for something that ends up looking like
    // water.
    uint16_t speed = 1;  // speed is set dynamically once we've started up

    // Scale determines how far apart the pixels in our noise matrix are.  Try
    // changing these values around to see how it affects the motion of the display.  The
    // higher the value of scale, the more "zoomed out" the noise iwll be.  A value
    // of 1 will be so zoomed in, you'll mostly see solid colors.
    uint16_t scale = 40;         // scale is set dynamically once we've started up
    uint16_t target_scale = 40;  // scale is set dynamically once we've started up

    // This is the array that we keep our computed noise values in
    uint8_t noise[GRID_H][GRID_W];

   private:
    // Methods
    void _fill_noise8();
    void _map_noise_to_leds_using_palette();

    // Variables
};

DECLARE_GRADIENT_PALETTE(Sunset_Real_gp);

#endif  // _LEDPANEL_H