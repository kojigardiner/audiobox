#include "AudioProcessor.h"

#include "Utils.h"

// Constructor to define FFT post-processing
AudioProcessor::AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning, bool volume_scaling) {
    _WHITE_NOISE_EQ = white_noise_eq;
    _A_WEIGHTING_EQ = a_weighting_eq;
    _PERCEPTUAL_BINNING = perceptual_binning;
    _VOLUME_SCALING = volume_scaling;

    if (!_i2s_init()) {
        _is_active = false;
    } else {
        _is_active = true;
    }
    _init_variables();
    _real_fft_plan = fft_init(FFT_SAMPLES, FFT_REAL, FFT_FORWARD, _v_real, _v_imag);
}

// Destructor (make sure to destroy the fft)
AudioProcessor::~AudioProcessor() {
    fft_destroy(_real_fft_plan);
}

// Return true if the audio processor initialization succeeded
bool AudioProcessor::is_active() {
    return _is_active;
}

// Initialize I2S for audio ADC
bool AudioProcessor::_i2s_init() {
    i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // I2S mic transfer only works with 32b
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = FFT_SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_BCK,          // Serial Clock (SCK)
        .ws_io_num = PIN_I2S_WS,            // Word Select (WS)
        .data_out_num = I2S_PIN_NO_CHANGE,  // not used (only for speakers)
        .data_in_num = PIN_I2S_DIN          // Serial Data (SD)
    };

    // Workaround for SPH0645 timing issue (see comments: https://hackaday.io/project/162059-street-sense/log/160705-new-i2s-microphone)
    REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));
    REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
        print("Audio Error (i2s_driver_install): Check that microphone is connected to Audio In port\n");
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        print("Audio Error (i2s_set_pin): Check that microphone is connected to Audio In port\n");
        return false;
    }

    print("Audio I2S init complete\n");
    return true;
}

// Initialize instance variables
void AudioProcessor::_init_variables() {
    _setup_audio_bins();

    // Initialize arrays
    memset(_intensity, 0, sizeof(_intensity));

    memset(_last_intensity, 0, sizeof(_last_intensity));
    memset(_fft_interp, 0, sizeof(_fft_interp));

    memset(_v_real, 0, sizeof(_v_real));
    memset(_v_imag, 0, sizeof(_v_imag));

    memset(_fft_bin, 0, sizeof(_fft_bin));

    _max_fft_val = 0;

    _audio_first_loop = true;
    _curr_volume = 0;
    _avg_volume = 0;
    _avg_peak_volume = 0;

    print("Audio processor variables initialized\n");
}

// Get audio data from I2S mic (non-overlapping)
void AudioProcessor::get_audio_samples() {
    int samples_to_read = FFT_SAMPLES;

    int32_t audio_val, audio_val_avg;
    int32_t audio_val_sum = 0;
    int32_t buffer[samples_to_read];
    size_t bytes_read = 0;
    int samples_read = 0;

    // First audio samples are junk, so prime the system then delay
    if (_audio_first_loop) {
        i2s_read(I2S_PORT, (void *)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);  // no timeout
        delay(1000);
    }
    i2s_read(I2S_PORT, (void *)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);  // no timeout
    _audio_first_loop = false;

    // Check that we got the correct number of samples
    samples_read = int(int(bytes_read) / sizeof(buffer[0]));  // since a sample is 32 bits (4 bytes)
    if (int(samples_read) != samples_to_read) {
        print("Warning: %d audio samples read, %d expected!\n", int(samples_read), int(samples_to_read));
    }

    uint8_t bit_shift_amount = 32 - I2S_MIC_BIT_DEPTH;

    // Bitshift the data
    for (int i = 0; i < int(samples_read); i++) {
        audio_val = buffer[i] >> bit_shift_amount;
        audio_val_sum += audio_val;

        _v_real[i] = double(audio_val);
        _v_imag[i] = 0;
    }

    double scale_val = (1 << (I2S_MIC_BIT_DEPTH - 1));
    audio_val_avg = double(audio_val_sum) / samples_read;  // DC offset
    for (int i = 0; i < int(samples_read); i++) {
        _v_real[i] = (_v_real[i] - audio_val_avg) / scale_val;  // Subtract DC offset and scale to ±1
    }
}

// Get audio data from I2S mic (overlapping)
void AudioProcessor::get_audio_samples_gapless() {
    int samples_to_read = FFT_SAMPLES / 2;
    int offset = FFT_SAMPLES - samples_to_read;

    int32_t audio_val, audio_val_avg;
    int32_t audio_val_sum = 0;
    int32_t buffer[samples_to_read];
    size_t bytes_read = 0;
    int samples_read = 0;

    // First audio samples are junk, so prime the system then delay
    if (_audio_first_loop) {
        i2s_read(I2S_PORT, (void *)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);  // no timeout
        delay(1000);
    }
    i2s_read(I2S_PORT, (void *)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);  // no timeout
    _audio_first_loop = false;

    // Check that we got the correct number of samples
    samples_read = int(int(bytes_read) / sizeof(buffer[0]));  // since a sample is 32 bits (4 bytes)
    if (int(samples_read) != samples_to_read) {
        print("Warning: %d audio samples read, %d expected!\n", int(samples_read), int(samples_to_read));
    }

    uint8_t bit_shift_amount = 32 - I2S_MIC_BIT_DEPTH;

    // Bitshift the data
    for (int i = 0; i < int(samples_read); i++) {
        audio_val = buffer[i] >> bit_shift_amount;
        audio_val_sum += audio_val;

        _v_real[i] = _v_real[i + offset];         // shift previous data forward
        _v_real[i + offset] = double(audio_val);  // put new data in back of array
        _v_imag[i] = 0;
    }

    double scale_val = (1 << (I2S_MIC_BIT_DEPTH - 1));
    audio_val_avg = double(audio_val_sum) / samples_read;  // DC offset
    for (int i = 0; i < int(samples_read); i++) {
        _v_real[i + offset] = (_v_real[i + offset] - audio_val_avg) / scale_val;  // Subtract DC offset and scale to ±1
    }
}

// Use the current audio samples to calculate an instantaneous volume, then uses exponential
// moving average to update the _avg_volume variable.
void AudioProcessor::update_volume() {
    // Use exponential moving average, simpler than creating an actual moving avg array
    _curr_volume = _calc_rms_scaled(_v_real, FFT_SAMPLES);

    if (_audio_first_loop) {  // the first time through the loop, we have no history, so set the avg volume to the curr volume
        _avg_volume = _curr_volume;
    } else {
        _avg_volume = (VOL_AVG_SCALE * _curr_volume) + (_avg_volume * (1 - VOL_AVG_SCALE));
    }

    if (_curr_volume > VOL_PEAK_FACTOR * _avg_volume) {  // we have a peak
        _avg_peak_volume = (VOL_AVG_SCALE * _curr_volume) + (_avg_peak_volume * (1 - VOL_AVG_SCALE));
    }
    
    // Use the volume to scale/normalize the FFT values
    if (_VOLUME_SCALING) {
        _max_fft_val = _avg_volume * VOL_MULT;
        // if (_max_fft_val < 1.0) {
        //   _max_fft_val = 1.0;        // since max_val is a divider, avoid less than 1
        // }
    } else {
        _max_fft_val = FFT_FIXED_MAX_VAL;  // fixed value if we are not scaling
    }
}

void AudioProcessor::run_fft() {
    _perform_fft();
    _postprocess_fft();
    _detect_beat();
}

// This method runs once at init time to pre-calculate audio bands that are then used for the
// column-based LED visualizations.
void AudioProcessor::_setup_audio_bins() {
    double nyquist_freq = I2S_SAMPLE_RATE / 2.0;
    double lowest_freq = LOWEST_FREQ_BAND;
    double highest_freq = HIGHEST_FREQ_BAND;

    if (highest_freq > nyquist_freq) {
        highest_freq = int(nyquist_freq);
        print("WARNING: highest frequency bin set to Nyquist, %f\n", highest_freq);
    }

    double freq_mult_per_band = pow(double(highest_freq) / lowest_freq, 1.0 / (NUM_AUDIO_BANDS - 1));

    double bin_width = double(I2S_SAMPLE_RATE) / FFT_SAMPLES;
    double nbins = FFT_SAMPLES / 2.0 - 1;

    // We want to be conservative here and not pick up too much out-of-band noise. So use ceil and floor
    _lowest_bass_bin = int(ceil(LOWEST_BASS_FREQ / bin_width) - 1);
    _highest_bass_bin = int(floor(HIGHEST_BASS_FREQ / bin_width) - 1);
    print("lowest bass bin = %d, lowest bass freq = %f\n", _lowest_bass_bin, (_lowest_bass_bin + 1) * bin_width);
    print("highest bass bin = %d, highest bass freq = %f\n", _highest_bass_bin, (_highest_bass_bin + 1) * bin_width);

    print("freq_mult_per_band: %f, nyq_freq: %f, bin_width: %f, num_bins: %d\n", freq_mult_per_band, nyquist_freq, bin_width, int(round(nbins)));

    for (int band = 0; band < NUM_AUDIO_BANDS; band++) {
        _bin_freqs[band] = lowest_freq * pow(freq_mult_per_band, band);
        _center_bins[band] = _bin_freqs[band] / bin_width;
    }

    for (int band = 0; band < NUM_AUDIO_BANDS - 1; band++) {
        _high_bins[band] = (_center_bins[band + 1] - _center_bins[band]) / 2.0 + _center_bins[band];
    }
    _high_bins[NUM_AUDIO_BANDS - 1] = nbins;

    for (int band = NUM_AUDIO_BANDS - 1; band >= 1; band--) {
        _low_bins[band] = _high_bins[band - 1];
    }
    _low_bins[0] = 0;

    for (int i = 0; i < NUM_AUDIO_BANDS; i++) {
        print("%d, %d, %d\n", int(round(_bin_freqs[i])), int(round(_low_bins[i])), int(round(_high_bins[i])));
    }
}

void AudioProcessor::_perform_fft() {
    fft_execute(_real_fft_plan);
    // FFT results go into _v_imag, so we need to extract them and put them back int to _v_real

    memset(_v_real, 0, sizeof(_v_real));  // clear out the _v_real array

    // Data format from documentation
    // Input  : [ x[0], x[1], x[2], ..., x[NFFT-1] ]
    // Output : [ X[0], X[NFFT/2], Re(X[1]), Im(X[1]), ..., Re(X[NFFT/2-1]), Im(X[NFFT/2-1]) ]

    //_v_real[0] = _v_imag[0] / FFT_SAMPLES;  // DC term is zeroth element

    // Other terms alternate between real & imag so we need to calculate the amplitude
    for (int i = 1; i < FFT_SAMPLES / 2; i++) {
        double real, imag, magnitude, amplitude;
        real = _v_imag[i * 2];      // index: 2, 4, 6, ... , (FFT_SAMPLES/2 - 1) * 2
        imag = _v_imag[i * 2 + 1];  // index: 3, 5, 7, ... , (FFT_SAMPLES/2 - 1) * 2 + 1
        magnitude = sqrt(pow(real, 2) + pow(imag, 2));
        amplitude = magnitude * 2 / FFT_SAMPLES;  // scale factor
        _v_real[i - 1] = amplitude;               // skip the 0th term because it is the DC value, which we don't want to plot
    }
}

// This method cleans up the FFT results and prepares them for LED intensity calculations.
// Set FFT_WHITE_NOISE_CAL to true in Constants.h in order to run in "calibration" mode. In calibration
// mode, the microphone should be set up to record a speaker playing white noise. The method will calculate
// the scaler values to put in the FFT_EQ constant and print them over serial.
void AudioProcessor::_postprocess_fft() {
    // 1. Remove baseline FFT "noise"
    // 2. Apply white noise eq (optional)
    // 3. Apply A-weighting eq (optional)
    // 4. Apply perceptual binning (optional)

    double eq_mult, a_weighting_mult;
    int curr_bin, mapped_bin;

#ifdef FFT_WHITE_NOISE_CAL
    static int counter = 0;
    static const int cal_samples = 10 * FFTS_PER_SEC;
    static double cum_fft[FFT_SAMPLES / 2] = {0};

    if (counter == cal_samples) {
        print_double_array(cum_fft, FFT_SAMPLES / 2);
        counter = 0;
        memset(cum_fft, 0, sizeof(cum_fft));
    } else {
        counter++;
    }
#endif

    _clear_fft_bin();  // clear out the fft_bin array as it has stale values

    for (int i = 0; i < FFT_SAMPLES / 2; i++) {
        _v_real[i] = (double(_v_real[i]) - double(FFT_REMOVE[i])) / _max_fft_val;  // remove noise and scale by max_val
        if (_v_real[i] < 0) {
            _v_real[i] = 0;
        }

#ifdef FFT_WHITE_NOISE_CAL
        cum_fft[i] += (_v_real[i] / cal_samples);
#endif

        if (_WHITE_NOISE_EQ) {  // apply white noise eq
            eq_mult = 255.0 / double(FFT_EQ[i]);
            _v_real[i] *= eq_mult;
        }
        if (_A_WEIGHTING_EQ) {  // apply A-weighting eq
            a_weighting_mult = (double(A_WEIGHTING[i]) / 255.0);
            _v_real[i] *= a_weighting_mult;
        }
        // if (_PERCEPTUAL_BINNING) {           // apply perceptual binning, old version
        //   curr_bin = i;
        //   mapped_bin = pgm_read_byte(&GAMMA16_FFT[i]);

        //   // accumulate FFT samples in the new mapped bin
        //   _fft_bin[mapped_bin] += _v_real[curr_bin];

        if (_PERCEPTUAL_BINNING) {  // apply perceptual binning, only maps the lower half of the samples
            if (i < FFT_SAMPLES / 4) {
                curr_bin = i;
                mapped_bin = GAMMA16_FFT[i] * 2;  // multiply by 2 to get the full mapping

                // accumulate FFT samples in the new mapped bin
                _fft_bin[mapped_bin] += _v_real[curr_bin];
            }
            // if (_PERCEPTUAL_BINNING) {                     // this is too slow!
            //   if (i == 0) _fft_bin[i] += _v_real[i];
            //   else {
            //     for (int j=0; j < NUM_AUDIO_BANDS; j++) {
            //       if (i>int(round(low_bins[j])) && i<=int(round(high_bins[j]))) _fft_bin[j]  += _v_real[i];
            //     }
            //   }
            // if (_PERCEPTUAL_BINNING) {
            //   _fft_bin[i] += _v_real[int(round(center_bins[i]))];
        } else {
            _fft_bin[i] = _v_real[i];
        }
    }
    if (_PERCEPTUAL_BINNING) {
        // Since not all bins will get data, fill the bins that are still zero with an avg of the neighboring bins
        // Note that this creates FFT energy where there was none previously.
        for (int i = 1; i < FFT_SAMPLES / 2 - 1; i++) {
            if (_fft_bin[i] == 0) {
                _fft_bin[i] = int(double(_fft_bin[i - 1] + _fft_bin[i + 1]) / 2);
            }
        }
    }
}

// This method needs more work -- it is intended to track specific FFT bins (e.g. bass) and
// detect peaks; however, it currently is too erratic to be used for visualizations.
void AudioProcessor::_detect_beat() {
    for (int i = 0; i < FFTS_PER_SEC - 1; i++) {
        _bass_arr[i] = _bass_arr[i + 1];  // shift elements forward
    }
    _bass_arr[FFTS_PER_SEC - 1] = 0;  // reset last element

    double curr_bass = 0.0;
    for (int i = _lowest_bass_bin; i <= _highest_bass_bin; i++) {  // up to and including the highest bin
        // accumulate the bass from the clean (e.g. not perceptually-binned) FFT
        curr_bass += (_v_real[i]);
    }
    curr_bass /= (_highest_bass_bin - _lowest_bass_bin + 1);  // average the bass bins (note we went up to and including the highest bin so add 1 here)
    _bass_arr[FFTS_PER_SEC - 1] = curr_bass;

    double bass_sum = 0.0;
    double bass_max = 0.0;
    for (int i = 0; i < FFTS_PER_SEC; i++) {
        bass_sum += _bass_arr[i];  // compute the sum of the history
        if (_bass_arr[i] > bass_max) {
            bass_max = _bass_arr[i];
        }
    }
    _avg_bass = bass_sum / FFTS_PER_SEC;  // compute the avg

    double bass_minus_avg_sq_sum = 0.0;
    for (int i = 0; i < FFTS_PER_SEC; i++) {
        bass_minus_avg_sq_sum += ((_bass_arr[i] - _avg_bass) * (_bass_arr[i] - _avg_bass));
    }
    _var_bass = bass_minus_avg_sq_sum / FFTS_PER_SEC;  // compute the var

    // double _thresh_bass = (-15 * _var_bass + 1.55) * _avg_bass;
    // double _thresh_bass = (bass_max - _avg_bass) * 0.5 + _avg_bass;
    double _thresh_bass = _avg_bass + 3 * sqrt(_var_bass);

    // must be loud enough, over thresh, and not too soon after the latest beat
    if (curr_bass > _thresh_bass && (20 * log10(_curr_volume)) > VOL_THRESH_DB && (millis() - _last_beat_ms) > 200) {
        _beat_detected = true;
        _last_beat_ms = millis();
    } else {
        _beat_detected = false;
    }

    // Serial.print(String(20*log10(curr_volume)) + "," + String(20*log10(avg_volume)) + "," + String(curr_bass) + "," + String(_avg_bass) + "," + String(_thresh_bass) + "," + String(curr_bass / bass_max) + "\n");
}

// Calculates intensity for LEDs based on FFT, scaling brightness with the FFT magnitude and applying a smoothing parameter over time.
void AudioProcessor::calc_intensity(int length) {
    _interpolate_fft(FFT_SAMPLES / 2, length);  // interpolate the FFT to the length of LEDs we want to illuminate

    for (int i = 0; i < length; i++) {
        if (_fft_interp[i] < 0) {
            _fft_interp[i] = 0;
        }
        double constrain_val = constrain(_fft_interp[i], 0, 1);  // cap the range at [0 1]
        double pow_val = pow(constrain_val, FFT_SCALE_POWER);    // raise to the power to increase sensitivity, multiply by max value to increase resolution
        int map_val = int(round(pow_val * BRIGHT_LEVELS));  // scale this into discrete LED brightness levels

        _intensity[i] -= FADE;  // fade first
        if (_intensity[i] < 0) _intensity[i] = 0;

        if ((map_val >= _intensity[i]) && (20 * log10(_avg_volume) > VOL_THRESH_DB) && (map_val >= MIN_BRIGHT_UPDATE)) {  // if the FFT value is brighter AND our volume is loud enough that we trust the data AND it is brighter than our min thresh, update
            _intensity[i] = map_val;
            _intensity[i] = _last_intensity[i] * (1 - LED_SMOOTHING) + _intensity[i] * (LED_SMOOTHING);  // apply a smoothing parameter
        }
        if (_intensity[i] < MIN_BRIGHT_FADE) {  // if less than the min value, floor to zero to reduce flicker
            _intensity[i] = 0;
        }

        _last_intensity[i] = _intensity[i];  // update the previous intensity value
    }
}

// Simple version that does no scaling or fading, and provides an intensity value for each column in the grid.
void AudioProcessor::calc_intensity_simple() {
    _clear_fft_bin();

    // bin down using the audio bands
    for (int i = 0; i < FFT_SAMPLES / 2; i++) {
        if (i == 0)
            _fft_bin[i] += _v_real[i];
        else {
            for (int j = 0; j < NUM_AUDIO_BANDS; j++) {
                if (i > int(round(_low_bins[j])) && i <= int(round(_high_bins[j]))) _fft_bin[j] += _v_real[i];
            }
        }
    }

    // interpolate back up to the length desired.
    _interpolate_fft(NUM_AUDIO_BANDS, GRID_W);

    for (int i = 0; i < GRID_W; i++) {
        _intensity[i] = int(round(_fft_interp[i] * 16));                                             // TODO: 4 is a fudge factor here to prevent values from peaking
        _intensity[i] = _last_intensity[i] * (1 - LED_SMOOTHING) + _intensity[i] * (LED_SMOOTHING);  // apply a smoothing parameter

        _last_intensity[i] = _intensity[i];
    }
}

void AudioProcessor::print_double_array(double *arr, int len) {
    for (int i = 0; i < len; i++) {
        print("%f", arr[i]);
        print(",");
    }
    print("\n");
}

int *AudioProcessor::get_intensity() {
    return _intensity;
}

double AudioProcessor::_calc_rms(float *arr, int len) {
    double sum = 0;

    for (int i = 0; i < len; i++) {
        sum = sum + arr[i] * arr[i];
    }
    return sqrt(sum / len);
}

// Calculate RMS accounting for max RMS
double AudioProcessor::_calc_rms_scaled(float *arr, int len) {
    double rms_scale_val = sqrt(2) / 2;

    return constrain(_calc_rms(arr, len) / rms_scale_val, 0, 1);
}

// Clears out the fft_bin array as it accumulates stale values
void AudioProcessor::_clear_fft_bin() {
    memset(_fft_bin, 0, sizeof(_fft_bin));
}

// Takes an array of FFT values and interpolates it to a new length.
// TODO: there is a lot of repeated code here, refactor it
void AudioProcessor::_interpolate_fft(int old_length, int new_length) {
    if (new_length > NUM_LEDS) {
        print("WARNING: trying to interpolate into _fft_interp array that is too small!\n");
    }

    // Three scenarios:
    // 1) new_length > old_length --> linearly interpolate up to new_length
    // 2) new_length == old_length --> do nothing!
    // 3) new_length < old_length --> find smallest integer multiple that gets new_length*MULT > FFT_SAMPLES/2
    //                                 interpolate up to new_length*MULT
    //                                 moving avg down to new_length

    double scaled_index, fractional_index, floor_val, next_val, interp_val;
    int16_t floor_index;
    double fft_upscale[int(ceil(double(old_length) / double(new_length))) * new_length];  // this is used temporarily when interpolating the fft array to enable us to downscale via simple averaging

    // Case 1: straight interpolate up
    if (new_length > old_length) {
        for (int i = 0; i < new_length; i++) {
            scaled_index = (i / double((new_length - 1))) * double(old_length - 1);  // find the index where we should be within the FFT bins
            floor_index = uint16_t(floor(scaled_index));                             // take the integer part of the index
            fractional_index = scaled_index - floor_index;                           // take the fractional part of the index
            floor_val = _fft_bin[floor_index];                                       // take the FFT at the integer index
            if ((floor_index + 1) < old_length) {                                    // find the FFT at the next index
                next_val = _fft_bin[floor_index + 1];
            } else {
                next_val = _fft_bin[floor_index];
            }

            interp_val = double(floor_val) + double(next_val - floor_val) * fractional_index;  // linear interpolation between the indices
            _fft_interp[i] = interp_val;
        }
        // Case 2: do nothing
    } else if (new_length == old_length) {
        for (int i = 0; i < new_length; i++) {
            _fft_interp[i] = _fft_bin[i];
        }
        // Case 3: interpolate up then average to decimate down
    } else if (new_length < old_length) {
        int multiple = ceil(double(old_length) / double(new_length));  // 2
        int upscale_len = multiple * new_length;                       // 168
        // upscale to 168
        for (int i = 0; i < upscale_len; i++) {
            scaled_index = (i / double((upscale_len - 1))) * double(old_length - 1);  // find the index where we should be within the FFT bins
            floor_index = uint16_t(floor(scaled_index));                              // take the integer part of the index
            fractional_index = scaled_index - floor_index;                            // take the fractional part of the index
            floor_val = _fft_bin[floor_index];                                        // take the FFT at the integer index
            if ((floor_index + 1) < old_length) {                                     // find the FFT at the next index
                next_val = _fft_bin[floor_index + 1];
            } else {
                next_val = _fft_bin[floor_index];
            }

            interp_val = double(floor_val) + double(next_val - floor_val) * fractional_index;  // linear interpolation between the indices
            fft_upscale[i] = interp_val;
        }

        // downscale with moving average
        for (int i = 0; i < new_length; i++) {
            interp_val = 0;
            for (int j = 0; j < multiple; j++) {
                interp_val += double(fft_upscale[i * multiple + j]) / double(multiple);
            }
            _fft_interp[i] = interp_val;
        }
    } else {
        print("ERROR: interpolate_fft() failed, should not have gotten here\n");
    }
}
