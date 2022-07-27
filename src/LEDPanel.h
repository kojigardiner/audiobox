#ifndef _LEDPANEL_H
#define _LEDPANEL_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// Forward declaration
class LEDAudioPattern;

// Class that describes a rectangular grid LED panel
class LEDPanel {
   public:
    enum first_pixel_location_t {
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        TOP_RIGHT,
        TOP_LEFT
    };

    // Constructor
    LEDPanel(int width, int height, int num_leds, int led_pin, uint8_t brightness, bool serpentine, first_pixel_location_t first_pixel);
    ~LEDPanel();

    // Methods

    // Setup FastLED with parameters from constructor
    void init();

    // Convert an XY coordinate to a linear LED index
    int grid_to_idx(int x, int y, bool start_top_left = false);

    // Set an LED to a value, directly by index or by XY position
    void set(int idx, CRGB value);
    void set_xy(int x, int y, CRGB value, bool start_top_left = false);

    // Get an LED value, directly by index or by XY position
    CRGB get(int idx);
    CRGB get_xy(int x, int y, bool start_top_left = false);

    // Copy LED data into the array provided
    void copy_leds(CRGB *dest, int length);

    // Set/get color palette and blend mode
    void set_palette(CRGBPalette16 palette);
    CRGBPalette16 get_palette();

    void set_blending(TBlendType blending);
    TBlendType get_blending();

    // Set and display audio reactive patterns
    void set_audio_pattern(int mode);
    void display_audio(int *intensity, double tempo = 0.0);

    // Blend current palette toward target palette
    void blend_palettes(int change_rate);

    // Set the target palette to blend toward
    void set_target_palette(CRGBPalette16 target_palette);

   private:
    // Variables
    LEDAudioPattern *_audio_pattern;
    int _w;
    int _h;
    first_pixel_location_t _first_pixel;
    bool _serpentine;
    int _led_pin;
    uint8_t _brightness;
    int _num_leds;

    CRGB _leds[NUM_LEDS];  // array that will hold LED values

    CRGBPalette16 _curr_palette;              // current color palette
    CRGBPalette16 _target_palette;            // target pallette that we will blend toward over time
    TBlendType _curr_blending = LINEARBLEND;  // type of blending to use between palettes

    CRGB _colors_grid_wide[GRID_W * SCROLL_AVG_FACTOR][GRID_H] = {{CRGB::Black}};  // wider version of led array for use with the scrolling animatino
};

DECLARE_GRADIENT_PALETTE(Sunset_Real_gp);

#endif  // _LEDPANEL_H