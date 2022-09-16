#ifndef _AUDIOPROCESSOR_H
#define _AUDIOPROCESSOR_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2c_reg.h>

#include "Constants.h"
#include "fft.h"

// The AudioProcessor class is meant to be instantiated once, and encapsulates the interactions with the microphone 
// for audio capture as well as the implementation of the FFT and subsequent post-processing.
class AudioProcessor {
   public:
    // Constructor to be called with flags defining post-FFT processing options as follows:
    //      white_noise_eq: multiplies each FFT element with a scale factor provided in Constants.h. this is dependent
    //          on the audio setup being used (i.e. speaker, room environment, etc)
    //      a_weighting_eq: multiplies each FFT element with a scale factor to approximate A-weighting 
    //          (see: https://en.wikipedia.org/wiki/A-weighting)
    //      perceptual_binning: applies a gamma curve to re-bin the FFT results to match human perception
    //      volume_scaling: normalizes the FFT output by the current volume
    AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning, bool volume_scaling);
    ~AudioProcessor();

    // Collects audio samples from I2S bus. Each call collects the next FFT_SAMPLES (defined in Constants.h).
    // Because each call fills the buffer independently, the FFT results may be erratic.
    // Use get_audio_samples_gapless() instead for smoother FFT results.
    void get_audio_samples();

    // Collects audio samples from I2S bus. Each call collects the next FFT_SAMPLES/2 samples (defined in 
    // Constants.h). Each call shifts the existing back *half* of the buffer forward, and fills new samples 
    // in the back half of the buffer. This creates a 50% overlap between successive FFT calls, giving
    // smoother results.
    void get_audio_samples_gapless();

    // Updates the internal volume variable using the most recent audio samples.
    void update_volume();

    // Performs FFT calculations and associated post-processing as defined in the constructor.
    void run_fft();

    // Calculates LED intensity values based on FFT results, scaled to the provided array length.
    // Typically this would be called with the number of LEDs we wish to illuminate with independent
    // FFT values. This calculation is used for LED intensities that smoothly vary over time.
    void calc_intensity(int length);

    // Calculates a "simple" version of LED intensity values based on FFT results. This calculation
    // is used for LED intensities that sharply vary over time, and is most useful for column-based
    // visualizations. The length of the LED intensity output is defined by NUM_AUDIO_BANDS in
    // Constants.h.
    void calc_intensity_simple();

    // Helper function that prints array values to serial port.
    void print_double_array(double *arr, int len);

    // Returns a pointer to the LED intensity array.
    int *get_intensity();

    // Returns true if the AudioProcessor initialization succeeded
    bool is_active();

   private:
    // Methods

    // Initializes all private variables.
    void _init_variables();

    // Initializes I2S driver in the ESP32.
    bool _i2s_init();

    // Sets up audio bins for use with calc_intensity_simple.
    void _setup_audio_bins();

    // Sets fft_bin array to zeros.
    void _clear_fft_bin();

    // Calculates rms value for a given array.
    double _calc_rms(float *arr, int len);

    // Calculates rms value scaled by sqrt(2) / 2.
    double _calc_rms_scaled(float *arr, int len);

    // Performs FFT on audio samples.
    void _perform_fft();

    // Postprocesses the FFT results to remove noise and apply weighting/binning as set during initialization.
    void _postprocess_fft();

    // Interpolates FFT results to a new array size.
    void _interpolate_fft(int old_length, int new_length);

    // Detects a beat based on  audio samples.
    void _detect_beat();

    bool _is_active = false;

    // Variables for FFT
    fft_config_t *_real_fft_plan;
    float _v_real[FFT_SAMPLES] = {0.0};        // stores audio samples, then replaced by FFT real data (up to FFT_SAMPLES / 2 length)
    float _v_imag[FFT_SAMPLES] = {0.0};        // stores all zeros prior to FFT, then replaced by FFT imaginary data (up to FFT_SAMPLES / 2 length)
    double _fft_bin[FFT_SAMPLES / 2] = {0.0};  // stores perceptually binned FFT data
    double _fft_interp[NUM_LEDS] = {0.0};      // stores interpolated FFT data (up to user-specified length)

    // Variables for beat detection
    int _lowest_bass_bin = 0;
    int _highest_bass_bin = 0;
    double _avg_bass = 0.0;
    double _var_bass = 0.0;
    double _bass_arr[FFTS_PER_SEC] = {0.0};
    unsigned long _last_beat_ms = 0;
    bool _beat_detected = false;

    // FFT post-processing options
    bool _WHITE_NOISE_EQ = true;
    bool _A_WEIGHTING_EQ = true;
    bool _PERCEPTUAL_BINNING = true;
    bool _VOLUME_SCALING = true;

    // Variables for LEDs
    int _last_intensity[NUM_LEDS]{0};  // holds the last frame intensities for smoothing

    // Other variables
    double _max_fft_val = 0;  // used to normalize the FFT outputs to [0 1]

    // Variables for LEDs
    int _intensity[NUM_LEDS] = {0};  // holds the brightness values for LEDs

    // Variables for volume
    double _curr_volume = 0;      // stores instantaneous volume
    double _avg_volume = 0;       // running average of volumes
    double _avg_peak_volume = 0;  // running average of peak volumes

    // Variables for audio FFT bins
    float _center_bins[NUM_AUDIO_BANDS] = {0};
    float _bin_freqs[NUM_AUDIO_BANDS] = {0};
    float _low_bins[NUM_AUDIO_BANDS] = {0};
    float _high_bins[NUM_AUDIO_BANDS] = {0};

    bool _audio_first_loop = true;  // tracks first iteration through audio loop
};

#endif  // _AUDIOPROCESSOR_H
