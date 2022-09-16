#include "LEDPanel.h"

#include "LEDAudioPattern.h"
#include "Utils.h"

// Default constructor
LEDPanel::LEDPanel(int width, int height, int num_leds, int led_pin, uint8_t brightness, bool serpentine, first_pixel_location_t first_pixel) {
    if (!serpentine || first_pixel != BOTTOM_LEFT) {
        print("Error: Unsupported LEDPanel config!\n");
    }

    this->_w = width;
    this->_h = height;
    this->_num_leds = num_leds;
    this->_led_pin = led_pin;
    this->_brightness = brightness;
    this->_serpentine = serpentine;
    this->_first_pixel = first_pixel;
}

LEDPanel::~LEDPanel() {
}

void LEDPanel::init() {
    // LED Setup
    FastLED.addLeds<WS2812, PIN_LED_CONTROL, GRB>(_leds, _num_leds);
    FastLED.setBrightness(_brightness);
    FastLED.clear();
    FastLED.show();
    _curr_palette = Sunset_Real_gp;
    _audio_pattern = new LEDNoisePattern(this);
}

void LEDPanel::set(int idx, CRGB value) {
    _leds[idx] = value;
}

void LEDPanel::set_xy(int x, int y, CRGB value, bool start_top_left) {
    int idx = grid_to_idx(x, y, start_top_left);
    if (idx >= 0 && idx < _num_leds) _leds[idx] = value;
}

int LEDPanel::grid_to_idx(int x, int y, bool start_top_left) {
    // Coordinate system starts with (0, 0) at the bottom left corner of grid.
    // Assumes a serpentine grid with the first pixel at the bottom left corner.
    // Even rows increment left->right, odd rows increment right->left.

    // TODO: add support for non-serpentine and non-bottom-left-first-pixel orientations.
    
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
    return _leds[idx];
}

CRGB LEDPanel::get_xy(int x, int y, bool start_top_left) {
    int idx = grid_to_idx(x, y, start_top_left);
    return _leds[idx];
}

void LEDPanel::copy_leds(CRGB *dest, int length) {
    memcpy(dest, this->_leds, sizeof(CRGB) * length);
}

void LEDPanel::set_audio_pattern(int mode) {
    delete _audio_pattern;

    switch (mode) {
        case MODE_AUDIO_SNAKE_GRID:
            _audio_pattern = new LEDSymSnakeGridPattern(this);
            break;
        case MODE_AUDIO_NOISE:
            _audio_pattern = new LEDNoisePattern(this);
            break;
        case MODE_AUDIO_BARS:
            _audio_pattern = new LEDBarsPattern(this);
            break;
        case MODE_AUDIO_OUTRUN_BARS:
            _audio_pattern = new LEDOutrunBarsPattern(this);
            break;
        case MODE_AUDIO_CENTER_BARS:
            _audio_pattern = new LEDCenterBarsPattern(this);
            break;
        case MODE_AUDIO_WATERFALL:
            _audio_pattern = new LEDWaterfallPattern(this);
            break;
        default:
            _audio_pattern = new LEDNoisePattern(this);
            break;
    }
}

void LEDPanel::display_audio(int *intensity, double tempo) {
    _audio_pattern->set_leds(intensity, tempo);
}

void LEDPanel::set_palette(CRGBPalette16 palette) {
    this->_curr_palette = palette;
}

CRGBPalette16 LEDPanel::get_palette() {
    return this->_curr_palette;
}

void LEDPanel::set_target_palette(CRGBPalette16 target_palette) {
    this->_target_palette = target_palette;
}

CRGBPalette16 LEDPanel::get_target_palette() {
    return this->_target_palette;
}

void LEDPanel::set_blending(TBlendType blending) {
    this->_curr_blending = blending;
}

TBlendType LEDPanel::get_blending() {
    return this->_curr_blending;
}

void LEDPanel::blend_palettes(int change_rate) {
    nblendPaletteTowardPalette(this->_curr_palette, this->_target_palette, change_rate);
}

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
    