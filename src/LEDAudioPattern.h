#ifndef _LEDAUDIOPATTERN_H
#define _LEDAUDIOPATTERN_H

#include <Arduino.h>

#include "Constants.h"
#include "FastLED.h"

// forward declaration
class LEDPanel;

// Abstract class for LED pattern generation
class LEDAudioPattern {
   public:
    LEDAudioPattern(LEDPanel *lp);
    ~LEDAudioPattern();
    virtual void set_leds(int *intensity) = 0;
    LEDPanel *lp;  // pointer to LED panel that will be updated
};

class LEDNoisePattern : public LEDAudioPattern {
   public:
    LEDNoisePattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;

   private:
    void _fill_noise8();
    void _map_noise_to_leds_using_palette();

    static uint8_t ihue;

    // The 16 bit version of our coordinates
    static uint16_t x;
    static uint16_t y;
    static uint16_t z;

    static uint16_t target_x;
    static uint16_t target_y;

    // We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
    // use the z-axis for "time".  speed determines how fast time moves forward.  Try
    // 1 for a very slow moving effect, or 60 for something that ends up looking like
    // water.
    static uint16_t speed;  // speed is set dynamically once we've started up

    // Scale determines how far apart the pixels in our noise matrix are.  Try
    // changing these values around to see how it affects the motion of the display.  The
    // higher the value of scale, the more "zoomed out" the noise iwll be.  A value
    // of 1 will be so zoomed in, you'll mostly see solid colors.
    static uint16_t scale;         // scale is set dynamically once we've started up
    static uint16_t target_scale;  // scale is set dynamically once we've started up

    // This is the array that we keep our computed noise values in, declare as static to avoid jumpiness when switching modes
    static uint8_t noise[GRID_H][GRID_W];
};

class LEDBarsPattern : public LEDAudioPattern {
   public:
    LEDBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;

   private:
    uint8_t peaks[GRID_W] = {0};
    unsigned long counter = 0;
};

class LEDOutrunBarsPattern : public LEDAudioPattern {
   public:
    LEDOutrunBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;

   private:
    uint8_t peaks[GRID_W] = {0};
    unsigned long counter = 0;
};

class LEDCenterBarsPattern : public LEDAudioPattern {
   public:
    LEDCenterBarsPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;
};

class LEDWaterfallPattern : public LEDAudioPattern {
   public:
    LEDWaterfallPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;
};

class LEDSymSnakeGridPattern : public LEDAudioPattern {
   public:
    LEDSymSnakeGridPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;
};

class LEDScrollingPattern : public LEDAudioPattern {
   public:
    LEDScrollingPattern(LEDPanel *lp) : LEDAudioPattern(lp){};
    void set_leds(int *intensity) override;
};

#endif  // _LEDAUDIOPATTERN_H