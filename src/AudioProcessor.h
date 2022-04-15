#ifndef _AUDIOPROCESSOR_H
#define _AUDIOPROCESSOR_H

#include <Arduino.h>
#include "arduinoFFT.h"
#include "Constants.h"
#include <driver/i2s.h>
#include <soc/i2c_reg.h>

class AudioProcessor {
    public:
        // Constructors
        AudioProcessor();
        AudioProcessor(bool a_weighting_eq, bool white_noise_eq, bool perceptual_binning, bool volume_scaling);

        // Methods
        void set_audio_samples(double *samples);
        void update_volume();
        void run_fft();
        void calc_intensity(int length);
        void calc_intensity_simple(int length);
        void print_double_array(double *arr, int len);

        // Variables for LEDs
        int intensity[NUM_LEDS] = { 0 };            // holds the brightness values to set the LEDs to
        
        // Variables for volume
        double curr_volume = 0;                     // store instantaneous volume
        double avg_volume = 0;                      // running average of volumes
        double avg_peak_volume = 0;                 // running average of peak volumes

        // Variables for audio FFT bins
        double low_bins[NUM_AUDIO_BANDS] = { 0 };
        double high_bins[NUM_AUDIO_BANDS] = { 0 };

        bool audio_first_loop = true;
        bool beat_detected = false;

    private:
        // Methods
        void _init_variables();
        void _i2s_init();
        void _setup_audio_bins();
        void _clear_fft_bin();
        double _calc_rms(double *arr, int len);
        void _perform_fft();
        void _postprocess_fft();
        void _interpolate_fft(int old_length, int new_length);
        void _detect_beat();

        // Variables for FFT
        arduinoFFT _FFT  = arduinoFFT(_v_real, _v_imag, FFT_SAMPLES, I2S_SAMPLE_RATE);
        double _v_real[FFT_SAMPLES] = { 0.0 };   // will store audio samples, then replaced by FFT real data (up to FFT_SAMPLES / 2 length)
        double _v_imag[FFT_SAMPLES] = { 0.0 };   // will store all zeros, then replaced by FFT imaginary data (up to FFT_SAMPLES / 2 length)
        double _fft_clean[FFT_SAMPLES / 2] = { 0.0 };   // will store "cleaned" and equalized FFT data
        double _fft_bin[FFT_SAMPLES / 2] = { 0.0 };   // will store "binned" FFT data
        double _fft_interp[NUM_LEDS] = { 0.0 };   // will store interpolated FFT data

        // Variables for beat detection
        int _highest_bass_bin = 0;
        double _avg_bass = 0.0;
        double _var_bass = 0.0;
        double _bass_arr[FFTS_PER_SEC] = { 0.0 };
        unsigned long _last_beat_ms = 0;

        // FFT post-processing options
        bool _WHITE_NOISE_EQ = true;
        bool _A_WEIGHTING_EQ = true;
        bool _PERCEPTUAL_BINNING = true;
        bool _VOLUME_SCALING = true;

        // Variables for LEDs
        int _last_intensity[NUM_LEDS] { 0 };         // holds the last frame intensities for smoothing

        // Other variables
        double _max_val = 0;                         // this is used to normalize the FFT outputs to [0 1]. set it lower to make the system "more sensitive". is auto-adjusted based on volume but empirically 1500 works pretty well.
};

#endif // _AUDIOPROCESSOR_H
