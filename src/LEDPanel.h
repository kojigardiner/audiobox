#ifndef _LEDPANEL_H
#define _LEDPANEL_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// Forward declaration
class LEDAudioPattern;

// The LEDPanel class describes a rectangular array of individually addressable RGB LEDs, along
// with methods for setting individual LEDs. Many of the methods are wrappers around FastLED
// functions which are used to control the LED strip, apply color palettes, and more.
class LEDPanel {
   public:

    // Definition of the start position for a given LED panel.
    enum first_pixel_location_t {
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        TOP_RIGHT,
        TOP_LEFT
    };

    // Constructor, which accepts the following arguments:
    //      width: number of LEDs in the horizontal dimension
    //      height: number of LEDs in the vertical dimension
    //      num_leds: number of total LEDs
    //      led_pin: GPIO pin on the ESP32 used for LED control
    //      brightness: 0 to 255 value to indicate how brightly the LEDs should be illuminated
    //      serpentine: flag to define if the LEDs are physically arranged in a serpentine pattern
    //          where first row increments left->right, second row increments right->left, etc.
    //      first_pixel: enum defining which corner of the array has the first pixel connected to the ESP32
    //
    // NOTE: serpentine and first_pixel are currently ignored. The default assumption is that the first pixel is in the 
    //      bottom left and the LEDs are arranged in a serpentine orientation.
    LEDPanel(int width, int height, int num_leds, int led_pin, uint8_t brightness, bool serpentine, first_pixel_location_t first_pixel);
    ~LEDPanel();

    // Sets up FastLED with parameters from constructor.
    void init();

    // Converts an XY coordinate to a linear LED index.
    // By default an (x, y) coordinate of (0, 0) is considered to be the bottom-left corner of the array. 
    // This can be changed to the top-left corner by setting start_top_left to true.
    int grid_to_idx(int x, int y, bool start_top_left = false);

    // Sets an LED value by linear array index.
    void set(int idx, CRGB value);

    // Sets an LED value by XY index. See grid_to_idx for XY coordinate assumption.
    void set_xy(int x, int y, CRGB value, bool start_top_left = false);

    // Gets an LED value by linear array index.
    CRGB get(int idx);

    // Gets an LED value by XY index. See grid_to_idx for XY coordinate assumption.
    CRGB get_xy(int x, int y, bool start_top_left = false);

    // Copies LED data into the array provided.
    void copy_leds(CRGB *dest, int length);

    // Sets/gets color palette.
    void set_palette(CRGBPalette16 palette);
    CRGBPalette16 get_palette();

    // Sets/gets blend mode.
    void set_blending(TBlendType blending);
    TBlendType get_blending();

    // Sets audio reactive pattern based on an AudioSubMode enum (see Constants.h).
    void set_audio_pattern(int mode);

    // Generates and displays an audio reactive pattern based on an array of LED intensity values.
    void display_audio(int *intensity, double tempo = 0.0);

    // Blends current color palette toward the target palette using a given change rate.
    // See FastLED nblendPaletteTowardPalette() for definition of change_rate.
    void blend_palettes(int change_rate);

    // Sets the target color palette to blend toward.
    void set_target_palette(CRGBPalette16 target_palette);

    // Returns the target color palette.
    CRGBPalette16 get_target_palette();

   private:
    LEDAudioPattern *_audio_pattern;        // pointer to an audio pattern object

    // Characteristics of the LED panel
    int _w;
    int _h;
    first_pixel_location_t _first_pixel;
    bool _serpentine;
    int _led_pin;
    uint8_t _brightness;
    int _num_leds;

    CRGB _leds[NUM_LEDS];                     // array that stores LED values to be displayed

    CRGBPalette16 _curr_palette;              // current color palette
    CRGBPalette16 _target_palette;            // target palette that we will blend toward over time
    TBlendType _curr_blending = LINEARBLEND;  // type of blending to use between palettes

    CRGB _colors_grid_wide[GRID_W * SCROLL_AVG_FACTOR][GRID_H] = {{CRGB::Black}};  // wider version of led array for use with the scrolling animation
};

DECLARE_GRADIENT_PALETTE(Sunset_Real_gp);   // declares color palette for use in default cases

#endif  // _LEDPANEL_H