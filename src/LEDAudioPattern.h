#ifndef _LEDAUDIOPATTERN_H
#define _LEDAUDIOPATTERN_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// Forward declaration
class LEDPanel;

// The LEDAudioPattern is an abstract class that is used for LED pattern generation.
// The LEDAudioPattern object has a pointer to an LEDPanel, and will set the LEDs on that panel
// to specific intensity values based on the implementation.
class LEDAudioPattern {
   public:
    // Constructor
    LEDAudioPattern(LEDPanel *lp);
    virtual ~LEDAudioPattern();

    // Abstract method to be implemented by all subclasses
    // Sets the LED intensity values for the given array. Accepts a music tempo parameter that can be used
    // to alter the noise pattern.
    virtual void set_leds(int *intensity, double tempo) = 0;

   protected:
    LEDPanel *_lp;  // pointer to LED panel object whose pixels will be updated
};

// Creates a simplex noise pattern ("lava lamp")
// Code adapted from FastLED's "Noise" function: https://github.com/FastLED/FastLED/blob/master/examples/NoisePlusPalette/NoisePlusPalette.ino
class LEDNoisePattern : public LEDAudioPattern {
   public:
    LEDNoisePattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;

   private:
    // Fill the _noise array with simplex noise values
    void _fill_noise8();

    // Sets LED intensities based on current noise values and current color palette.
    void _map_noise_to_leds_using_palette();

    // Below are static variables that will remain consistent between calls to the class for
    // visual consistency when switching modes.

    static uint8_t _ihue;  // hue counter
    
    // Coordinates for noise pattern
    static uint16_t _x;
    static uint16_t _y;
    static uint16_t _z;
    static uint16_t _target_x;
    static uint16_t _target_y;

    // We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
    // use the z-axis for "time".  speed determines how fast time moves forward.  Try
    // 1 for a very slow moving effect, or 60 for something that ends up looking like
    // water.
    static uint16_t _speed;  // speed is set dynamically once we've started up

    // Scale determines how far apart the pixels in our noise matrix are.  Try
    // changing these values around to see how it affects the motion of the display.  The
    // higher the value of scale, the more "zoomed out" the noise iwll be.  A value
    // of 1 will be so zoomed in, you'll mostly see solid colors.
    static uint16_t _scale;  // scale is set dynamically once we've started up
    static uint16_t _target_scale;  // used when adjusting scale dynamically

    // This is the array that we keep our computed noise values in
    static uint8_t _noise[GRID_H][GRID_W];
};

// Creates a vertical filled bar pattern
class LEDBarsPattern : public LEDAudioPattern {
   public:
    LEDBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;

   private:
    uint8_t _peaks[GRID_W] = {0};
    unsigned long _counter = 0;
};

// Creates vertical peaks pattern
class LEDOutrunBarsPattern : public LEDAudioPattern {
   public:
    LEDOutrunBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;

   private:
    uint8_t _peaks[GRID_W] = {0};
    unsigned long _counter = 0;
};

// Creates vertical bar pattern, symmetric about centerline
class LEDCenterBarsPattern : public LEDAudioPattern {
   public:
    LEDCenterBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

// Creates a side-scrolling waterfall pattern, similar to a spectrogram display
class LEDWaterfallPattern : public LEDAudioPattern {
   public:
    LEDWaterfallPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

// Creates a left/right symmetric serpentine grid pattern that illuminates and fades over time
class LEDSymSnakeGridPattern : public LEDAudioPattern {
   public:
    LEDSymSnakeGridPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

#endif  // _LEDAUDIOPATTERN_H