#include "LEDPanel.h"

#include "LEDAudioPattern.h"

// Default constructor
LEDPanel::LEDPanel() {
}

LEDPanel::~LEDPanel() {
}

void LEDPanel::init(uint8_t led_pin, int num_leds, uint8_t brightness) {
    // LED Setup
    FastLED.addLeds<WS2812, PIN_LED_CONTROL, GRB>(leds, num_leds);
    FastLED.setBrightness(brightness);
    FastLED.clear();
    FastLED.show();
    curr_palette = Sunset_Real_gp;
    _audio_pattern = new LEDNoisePattern(this);
}

void LEDPanel::set(int idx, CRGB value) {
    leds[idx] = value;
}

void LEDPanel::set_xy(int x, int y, CRGB value, bool start_top_left) {
    int idx = grid_to_idx(x, y, start_top_left);
    leds[idx] = value;
}

/*** Get Index of LED on a serpentine grid ***/
int LEDPanel::grid_to_idx(int x, int y, bool start_top_left) {
    /**
     * Coordinate system starts with (0, 0) at the bottom corner of grid.
     * Assumes a serpentine grid with the first pixel at the bottom left corner.
     * Even rows count left->right, odd rows count right->left.
     **/

    int idx = 0;

    if ((x >= GRID_W) || (y >= GRID_H)) {  // we are outside the grid
        return -1;
    }

    if (start_top_left) {
        y = GRID_H - 1 - y;
    }

    if (y % 2 == 0) {
        idx = GRID_W * y + x;
    } else {
        idx = GRID_W * y + (GRID_W - x - 1);
    }

    return idx;
}

CRGB LEDPanel::get(int idx) {
    return leds[idx];
}

CRGB LEDPanel::get_xy(int x, int y, bool start_top_left) {
    int idx = grid_to_idx(x, y, start_top_left);
    return leds[idx];
}

void LEDPanel::set_audio_pattern(int mode) {
    delete _audio_pattern;

    switch (mode) {
        case AUDIO_SNAKE_GRID:
            _audio_pattern = new LEDSymSnakeGridPattern(this);
            break;
        case AUDIO_NOISE:
            _audio_pattern = new LEDNoisePattern(this);
            break;
        case AUDIO_SCROLLING:
            _audio_pattern = new LEDScrollingPattern(this);
            break;
        case AUDIO_BARS:
            _audio_pattern = new LEDBarsPattern(this);
            break;
        case AUDIO_OUTRUN_BARS:
            _audio_pattern = new LEDOutrunBarsPattern(this);
            break;
        case AUDIO_CENTER_BARS:
            _audio_pattern = new LEDCenterBarsPattern(this);
            break;
        case AUDIO_WATERFALL:
            _audio_pattern = new LEDWaterfallPattern(this);
            break;
        default:
            _audio_pattern = new LEDNoisePattern(this);
            break;
    }
}

void LEDPanel::display_audio(int *intensity) {
    _audio_pattern->set_leds(intensity);
}

/*** Color Palettes ***/
// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.
DEFINE_GRADIENT_PALETTE(Sunset_Real_gp){
    0, 120, 0, 0,
    22, 179, 22, 0,
    51, 255, 104, 0,
    85, 167, 22, 18,
    135, 100, 0, 103,
    198, 16, 0, 130,
    255, 0, 0, 160};