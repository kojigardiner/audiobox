#ifndef _LEDAUDIOPATTERN_H
#define _LEDAUDIOPATTERN_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// Forward declaration
class LEDPanel;

// Abstract class for LED pattern generation. Create a subclass for new LED pattern functionality.
class LEDAudioPattern {
   public:
    LEDAudioPattern(LEDPanel *lp);
    virtual ~LEDAudioPattern();

    virtual void set_leds(int *intensity, double tempo) = 0;  // abstract method to be implemented by all subclasses

   protected:
    LEDPanel *_lp;  // pointer to LED panel object whose pixels will be updated
};

// Simplex noise pattern
class LEDNoisePattern : public LEDAudioPattern {
   public:
    LEDNoisePattern(LEDPanel *lp) : LEDAudioPattern(lp){};

    void set_leds(int *intensity, double tempo) override;

   private:
    // Helper methods
    void _fill_noise8();
    void _map_noise_to_leds_using_palette();

    // Static variables that will remain consistent between instantiation of the class for
    // visual consistency when switching modes
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
    static uint16_t _target_scale;

    // This is the array that we keep our computed noise values in
    static uint8_t _noise[GRID_H][GRID_W];
};

// Vertical filled bar pattern
class LEDBarsPattern : public LEDAudioPattern {
   public:
    LEDBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;

   private:
    uint8_t _peaks[GRID_W] = {0};
    unsigned long _counter = 0;
};

// Vertical bar peaks-only pattern
class LEDOutrunBarsPattern : public LEDAudioPattern {
   public:
    LEDOutrunBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;

   private:
    uint8_t _peaks[GRID_W] = {0};
    unsigned long _counter = 0;
};

// Vertical bar pattern, symmetric about center
class LEDCenterBarsPattern : public LEDAudioPattern {
   public:
    LEDCenterBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

// Side-scrolling waterfall pattern, similar to spectrogram display
class LEDWaterfallPattern : public LEDAudioPattern {
   public:
    LEDWaterfallPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

// Symmetric serpentine grid pattern
class LEDSymSnakeGridPattern : public LEDAudioPattern {
   public:
    LEDSymSnakeGridPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity, double tempo) override;
};

#endif  // _LEDAUDIOPATTERN_H