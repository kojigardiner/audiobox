#ifndef _AUDIOPROCESSOR_H
#define _AUDIOPROCESSOR_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2c_reg.h>

#include "Constants.h"
#include "fft.h"

class AudioProcessor {
   public:
    // Constructor
    AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning, bool volume_scaling);
    ~AudioProcessor();

    // Methods

    // Collect audio samples from I2S sequentially
    void get_audio_samples();

    // Collect audio samples from I2S, in gapless mode with 50% overlap between successive calls
    void get_audio_samples_gapless();

    // Update volume variable using latest audio data
    void update_volume();

    // Perform FFT calculations and associated post-processing
    void run_fft();

    // Calculate intensity values based on FFT, scaled to provided array length
    void calc_intensity(int length);

    // Calculate "simple" version of intensity based on FFT and audio bands
    void calc_intensity_simple();

    // Helper function to print array values to serial
    void print_double_array(double *arr, int len);

    // Return pointer to intensity array
    int *get_intensity();

    // Return true if the audio processor initialization succeeded
    bool is_active();

   private:
    // Methods

    // Init all private variables
    void _init_variables();

    // Init I2S driver in ESP32
    bool _i2s_init();

    // Setup audio bins for use with calc_intensity_simple
    void _setup_audio_bins();

    // Set fft_bin array to zeros
    void _clear_fft_bin();

    // Calculate rms value for given array
    double _calc_rms(float *arr, int len);

    // Claculate rms value scaled by sqrt(2) / 2
    double _calc_rms_scaled(float *arr, int len);

    // Perform FFT on audio samples
    void _perform_fft();

    // Postprocess the FFT results to remove noise and apply weighting/binning as set during initialization
    void _postprocess_fft();

    // Interpolate FFT results to a new array size
    void _interpolate_fft(int old_length, int new_length);

    // Detect a beat based on ongoing audio samples
    void _detect_beat();

    bool _is_active = false;

    // Variables for FFT
    fft_config_t *_real_fft_plan;
    float _v_real[FFT_SAMPLES] = {0.0};        // will store audio samples, then replaced by FFT real data (up to FFT_SAMPLES / 2 length)
    float _v_imag[FFT_SAMPLES] = {0.0};        // will store all zeros, then replaced by FFT imaginary data (up to FFT_SAMPLES / 2 length)
    double _fft_bin[FFT_SAMPLES / 2] = {0.0};  // will store "binned" FFT data
    double _fft_interp[NUM_LEDS] = {0.0};      // will store interpolated FFT data

    // Variables for beat detection
    int _lowest_bass_bin = 0;
    int _highest_bass_bin = 0;
    double _avg_bass = 0.0;
    double _var_bass = 0.0;
    double _bass_arr[FFTS_PER_SEC] = {0.0};
    unsigned long _last_beat_ms = 0;

    // FFT post-processing options
    bool _WHITE_NOISE_EQ = true;
    bool _A_WEIGHTING_EQ = true;
    bool _PERCEPTUAL_BINNING = true;
    bool _VOLUME_SCALING = true;

    // Variables for LEDs
    int _last_intensity[NUM_LEDS]{0};  // holds the last frame intensities for smoothing

    // Other variables
    double _max_fft_val = 0;  // this is used to normalize the FFT outputs to [0 1]. set it lower to make the system "more sensitive". is auto-adjusted based on volume but empirically 1500 works pretty well.

    // Variables for LEDs
    int _intensity[NUM_LEDS] = {0};  // holds the brightness values to set the LEDs to

    // Variables for volume
    double _curr_volume = 0;      // store instantaneous volume
    double _avg_volume = 0;       // running average of volumes
    double _avg_peak_volume = 0;  // running average of peak volumes

    // Variables for audio FFT bins
    float _center_bins[NUM_AUDIO_BANDS] = {0};
    float _bin_freqs[NUM_AUDIO_BANDS] = {0};
    float _low_bins[NUM_AUDIO_BANDS] = {0};
    float _high_bins[NUM_AUDIO_BANDS] = {0};

    bool _audio_first_loop = true;
    bool _beat_detected = false;
};

#endif  // _AUDIOPROCESSOR_H
