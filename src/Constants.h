#ifndef CONSTANTS_H
#define CONSTANTS_H

/*** Defines ***/
#define PIN_LED_CONTROL     12              // LED strip control GPIO
#define PIN_LED_STATUS      2               // Status LED on HiLetgo board
#define PIN_BUTTON_MODE     14
#define PIN_BUTTON_UP       26
#define PIN_BUTTON_DOWN     27
#define PIN_I2S_BCK         5
#define PIN_I2S_DIN         17
#define PIN_I2S_WS          16
#define PIN_SERVO           18

#define MAX_BRIGHT          100              // sets max brightness for LEDs
#define JPG_GAMMA           2.2
#define LED_GAMMA           2.8

// LED defines
#define GRID_H              16
#define GRID_W              16
#define NUM_LEDS            GRID_H * GRID_W
#define FPS                 60

// Audio defines
#define I2S_PORT            I2S_NUM_0       // I2S port
#define I2S_SAMPLE_RATE     22050           // audio sampling rate (per Nyquist, we can get up to sample rate/2 freqs in FFT
#define I2S_MIC_BIT_DEPTH   18              // SPH0645 bit depth, per datasheet (18-bit 2's complement in 24-bit container)
#define FFT_SAMPLES         256

// Specific to audio volume control
#define VOL_FACTOR          10              // empirically found that RMS of signal needs x10 to match RMS of FFT
#define VOL_MULT            2.5             // multiplier to go from volume to FFT max value
#define VOL_PEAK_FACTOR     1.5             // factor above avg for finding peaks
#define VOL_AVG_SCALE       0.01 * 30/FPS            // exponential moving averaging scale factor for calculating running avg of volume
#define VOL_THRESH          400             // threshold below which we shouldn't update LEDs as FFT data may not be reliable

// Specific to audio LED
#define BRIGHT_LEVELS       255             // number of levels of brightness to use
#define MIN_BRIGHT_FADE     0               // Cut-off as we fade (this is a 5 in the gamma table)
#define MIN_BRIGHT_UPDATE   32              // Cut-off for new values (this is a 5 in the gamma table)
#define FADE                BRIGHT_LEVELS/32 * 30/FPS     // Rate at which LEDs will fade out (remember, gamma will be applied so fall off will seem faster).                                                           // Scale by FPS so that the fade speed is always the same.
#define LED_SMOOTHING       0.75 * 30/FPS            // smoothing factor for updating LEDs
#define FFT_SCALE_POWER     1.5             // power by which to scale the FFT for LED intensity
#define PALETTE_CHANGE_RATE 24              // default from https://gist.github.com/kriegsman/1f7ccbbfa492a73c015e

// Specific to scrolling grid
#define SCROLL_AVG_FACTOR   int(4 * 60/FPS) // number of frames to average to create a single vertical slice that scrolls

// Servo defines
#define SERVO_MIN_US        700
#define SERVO_MAX_US        2400
#define SERVO_START_POS     130
#define SERVO_END_POS       0
#define SERVO_BUTTON_HOLD_DELAY_MS 50

// Macros
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define FASTLED_ALLOW_INTERRUPTS 0          // TODO: check if we still need this
#define FASTLED_INTERRUPT_RETRY_COUNT 1     // TODO: check if we still need this

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

/*** Gamma Tables ***/
// Gamma curve for perceptual brightness (looks like this is close to gamma=2.8)
const uint8_t PROGMEM GAMMA8[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

// Changed all 1s to 2s
//const uint8_t PROGMEM gamma8[] = {
//  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
//  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  2,  2,  2,
//  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
//  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
//  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
//  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
//  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
//  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
//  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
//  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
//  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
//  90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
//  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
//  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
//  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
//  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
//};

// Linear
//const uint8_t PROGMEM gamma8[] = {
//  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
//  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,
//  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
//  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
//  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
//  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
//  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,
//  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103,
//  104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
//  117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
//  130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
//  143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
//  156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
//  169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
//  182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
//  195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
//  208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
//  221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
//  234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
//  247, 248, 249, 250, 251, 252, 253, 254, 255
//};

/*** Audio stuff ***/
// This version is a reverse gamma=2, with more compression at the higher end
const uint8_t PROGMEM GAMMA8_FFT[] = {
  0, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 26, 28, 30, 32, 33, 35, 37, 39, 40, 42, 43, 45, 47, 48, 50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 65, 66, 67, 69, 70, 71, 73, 74, 75, 77, 78, 79, 80, 82, 83, 84, 85, 86, 87, 88, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 101, 102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 110, 111, 112, 112, 113, 114, 114, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127
};

/*** For FFT ***/
// This is the base "noise" levels to remove from the FFT output (determined empirically)
// For analog mic (sparkfun)
// const uint8_t fft_remove[FFT_SAMPLES / 2] = {63, 45, 30, 29, 28, 27, 27, 26, 24, 23, 24, 24, 23, 23, 24, 24, 24, 25, 24, 23, 25, 25, 25, 24, 24, 23, 24, 24, 24, 24, 24, 24, 24, 24, 25, 24, 24, 23, 24, 24, 25, 24, 24, 23, 25, 25, 24, 25, 23, 25, 26, 25, 26, 25, 24, 26, 26, 25, 26, 27, 27, 27, 26, 25, 25, 25, 26, 26, 25, 26, 25, 25, 26, 25, 23, 25, 25, 25, 25, 25, 25, 24, 26, 25, 24, 24, 24, 25, 24, 23, 23, 23, 24, 24, 24, 22, 23, 23, 25, 24, 24, 23, 23, 24, 24, 24, 23, 23, 23, 24, 24, 25, 25, 24, 24, 24, 24, 25, 25, 24, 22, 24, 25, 25, 23, 24, 24, 25};
// For digital mic (adafruit SPH0645)
const uint16_t FFT_REMOVE[FFT_SAMPLES / 2] = {1212, 2329, 1601, 246, 172, 150, 154, 184, 188, 119, 105, 120, 166, 90, 76, 69, 73, 63, 60, 58, 58, 60, 58, 52, 53, 58, 52, 49, 52, 50, 49, 52, 52, 44, 48, 48, 48, 50, 46, 45, 43, 44, 47, 45, 43, 43, 43, 46, 43, 40, 41, 45, 45, 46, 44, 46, 47, 43, 44, 41, 41, 38, 38, 39, 37, 35, 35, 33, 35, 33, 36, 39, 35, 34, 35, 33, 33, 31, 29, 29, 28, 29, 29, 29, 28, 29, 28, 26, 25, 24, 23, 25, 26, 25, 24, 25, 23, 21, 21, 22, 20, 20, 19, 19, 17, 16, 16, 16, 14, 14, 14, 13, 13, 12, 12, 12, 11, 9, 9, 9, 9, 8, 7, 7, 7, 6, 6, 6};

// This is the EQ to apply to get a "flat" response. Determined empirically using white noise through speakers. 255 divided by the values here determines the gain for that freq band.
// For analog mic (sparkfun)
// const uint8_t fft_eq[FFT_SAMPLES / 2] = {87, 199, 255, 219, 189, 171, 153, 141, 151, 173, 129, 132, 133, 131, 108, 76, 76, 72, 82, 94, 110, 111, 111, 98, 94, 97, 89, 97, 92, 85, 95, 107, 102, 98, 101, 98, 93, 93, 102, 101, 100, 93, 89, 96, 93, 87, 71, 72, 72, 73, 75, 76, 80, 65, 85, 100, 97, 122, 125, 136, 139, 139, 130, 126, 123, 118, 106, 94, 97, 94, 92, 84, 79, 81, 80, 82, 73, 73, 65, 66, 73, 68, 66, 64, 61, 59, 58, 63, 59, 60, 65, 68, 58, 53, 52, 55, 47, 47, 40, 40, 38, 36, 37, 40, 38, 42, 44, 43, 41, 36, 33, 31, 31, 28, 27, 28, 29, 29, 30, 35, 37, 35, 33, 32, 32, 35, 33, 33};
// For digital mic (adafruit SPH0645)
const uint8_t FFT_EQ[FFT_SAMPLES / 2] = {98, 197, 255, 238, 195, 186, 168, 162, 179, 201, 154, 174, 160, 148, 143, 122, 91, 73, 84, 111, 138, 132, 128, 126, 106, 107, 116, 113, 131, 119, 129, 134, 133, 139, 152, 128, 113, 115, 113, 130, 135, 132, 119, 112, 99, 103, 108, 108, 92, 87, 93, 87, 84, 81, 90, 93, 92, 101, 105, 101, 97, 96, 89, 88, 96, 101, 97, 91, 92, 90, 97, 100, 93, 92, 96, 95, 106, 105, 102, 98, 85, 95, 97, 95, 88, 85, 94, 89, 87, 80, 80, 73, 60, 60, 51, 47, 43, 44, 44, 46, 48, 46, 43, 42, 40, 36, 34, 32, 31, 27, 25, 23, 22, 19, 18, 17, 16, 15, 13, 12, 12, 10, 8, 7, 6, 4, 3, 2};

// Divide these values by 255 to create the a-weighting gain
// const uint8_t a_weighting[FFT_SAMPLES / 2] = {0, 18, 49, 78, 103, 125, 144, 159, 172, 183, 192, 199, 205, 211, 215, 219, 222, 224, 226, 228, 230, 231, 232, 233, 233, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 233, 233, 232, 232, 231, 231, 230, 229, 229, 228, 227, 226, 226, 225, 224, 223, 222, 221, 220, 219, 218, 217, 216, 216, 214, 213, 212, 211, 210, 209, 208, 207, 206, 205, 204, 203, 202, 201, 199, 198, 197, 196, 195, 194, 193, 191, 190, 189, 188, 187, 186, 185, 183, 182, 181, 180, 179, 178, 177, 175, 174, 173, 172, 171, 170, 169, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157, 155, 154, 153, 152, 151, 150, 149, 148, 147, 146, 145, 144, 143, 142, 141, 140};
// Shifted over by one so we get some low end
const uint8_t A_WEIGHTING[FFT_SAMPLES / 2] = {18, 49, 78, 103, 125, 144, 159, 172, 183, 192, 199, 205, 211, 215, 219, 222, 224, 226, 228, 230, 231, 232, 233, 233, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 233, 233, 232, 232, 231, 231, 230, 229, 229, 228, 227, 226, 226, 225, 224, 223, 222, 221, 220, 219, 218, 217, 216, 216, 214, 213, 212, 211, 210, 209, 208, 207, 206, 205, 204, 203, 202, 201, 199, 198, 197, 196, 195, 194, 193, 191, 190, 189, 188, 187, 186, 185, 183, 182, 181, 180, 179, 178, 177, 175, 174, 173, 172, 171, 170, 169, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157, 155, 154, 153, 152, 151, 150, 149, 148, 147, 146, 145, 144, 143, 142, 141, 140, 140};

#endif