#include "AudioProcessor.h"

// Default constructor
AudioProcessor::AudioProcessor() {
  _i2s_init();
  _init_variables();
  _real_fft_plan = fft_init(FFT_SAMPLES, FFT_REAL, FFT_FORWARD, _v_real, _v_imag);
}

// Alternate constructor to define FFT post-processing
AudioProcessor::AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning, bool volume_scaling) {
  _WHITE_NOISE_EQ = white_noise_eq;
  _A_WEIGHTING_EQ = a_weighting_eq;
  _PERCEPTUAL_BINNING = perceptual_binning;
  _VOLUME_SCALING = volume_scaling;

  _i2s_init();
  _init_variables();
  _real_fft_plan = fft_init(FFT_SAMPLES, FFT_REAL, FFT_FORWARD, _v_real, _v_imag);
}

// Destructor (make sure to destroy the fft)
AudioProcessor::~AudioProcessor() {
  fft_destroy(_real_fft_plan);
}

/*** Initialize I2S for audio ADC ***/
void AudioProcessor::_i2s_init() {
  i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate =  I2S_SAMPLE_RATE,             
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // I2S mic transfer only works with 32b
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = FFT_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCK,   // Serial Clock (SCK)
    .ws_io_num = PIN_I2S_WS,    // Word Select (WS)
    .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
    .data_in_num = PIN_I2S_DIN   // Serial Data (SD)
  };

  // Workaround for SPH0645 timing issue
  REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));
  REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  
  Serial.print("Audio I2S init complete\n");
}

void AudioProcessor::_init_variables() {
    _setup_audio_bins();

   // Initialize arrays 
    memset(intensity, 0, sizeof(intensity));
    
    memset(_last_intensity, 0, sizeof(_last_intensity));
    memset(_fft_interp, 0, sizeof(_fft_interp));

    memset(_v_real, 0, sizeof(_v_real));
    memset(_v_imag, 0, sizeof(_v_imag));
    
    memset(_fft_bin, 0, sizeof(_fft_bin));

    _max_fft_val = 0;
    
    audio_first_loop = true;
    curr_volume = 0;
    avg_volume = 0;
    avg_peak_volume = 0;
    
    Serial.print("Audio processor variables initialized\n");
}

/*** Get audio data from I2S mic ***/
void AudioProcessor::get_audio_samples() {
  int samples_to_read = FFT_SAMPLES;

  int32_t audio_val, audio_val_avg;
  int32_t audio_val_sum = 0;
  int32_t buffer[samples_to_read];
  size_t bytes_read = 0;
  int samples_read = 0;

  // First audio samples are junk, so prime the system then delay
  if (audio_first_loop) {
    i2s_read(I2S_PORT, (void*) buffer, sizeof(buffer), &bytes_read, portMAX_DELAY); // no timeout
    delay(1000);
  }
  i2s_read(I2S_PORT, (void*) buffer, sizeof(buffer), &bytes_read, portMAX_DELAY); // no timeout
  audio_first_loop = false;

  // Check that we got the correct number of samples
  samples_read = int(int(bytes_read) / sizeof(buffer[0])); // since a sample is 32 bits (4 bytes)
  if (int(samples_read) != samples_to_read) {
    Serial.println("Warning: " + String(int(samples_read)) + " audio samples read, " + String(samples_to_read) + " expected!");
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
  audio_val_avg = double(audio_val_sum) / samples_read;     // DC offset
  for (int i = 0; i < int(samples_read); i++) {
    _v_real[i] = (_v_real[i] - audio_val_avg) / scale_val;    // Subtract DC offset and scale to ±1
  }
}

/*** Get audio data from I2S mic, only fill half the buffer each time and shift previous data up ***/
void AudioProcessor::get_audio_samples_gapless() {
  int samples_to_read = FFT_SAMPLES / 2;
  int offset = FFT_SAMPLES - samples_to_read;

  int32_t audio_val, audio_val_avg;
  int32_t audio_val_sum = 0;
  int32_t buffer[samples_to_read];
  size_t bytes_read = 0;
  int samples_read = 0;

  // First audio samples are junk, so prime the system then delay
  if (audio_first_loop) {
    i2s_read(I2S_PORT, (void*) buffer, sizeof(buffer), &bytes_read, portMAX_DELAY); // no timeout
    delay(1000);
  }
  i2s_read(I2S_PORT, (void*) buffer, sizeof(buffer), &bytes_read, portMAX_DELAY); // no timeout
  audio_first_loop = false;

  // Check that we got the correct number of samples
  samples_read = int(int(bytes_read) / sizeof(buffer[0])); // since a sample is 32 bits (4 bytes)
  if (int(samples_read) != samples_to_read) {
    Serial.println("Warning: " + String(int(samples_read)) + " audio samples read, " + String(samples_to_read) + " expected!");
  }

  uint8_t bit_shift_amount = 32 - I2S_MIC_BIT_DEPTH;

  // Bitshift the data
  for (int i = 0; i < int(samples_read); i++) {
    audio_val = buffer[i] >> bit_shift_amount;
    audio_val_sum += audio_val;

    _v_real[i] = _v_real[i + offset];        // shift previous data forward
    _v_real[i + offset] = double(audio_val); // put new data in back of array
    _v_imag[i] = 0;
  }

  double scale_val = (1 << (I2S_MIC_BIT_DEPTH - 1));
  audio_val_avg = double(audio_val_sum) / samples_read;     // DC offset
  for (int i = 0; i < int(samples_read); i++) {
    _v_real[i + offset] = (_v_real[i + offset] - audio_val_avg) / scale_val;      // Subtract DC offset and scale to ±1
  }
}

void AudioProcessor::update_volume() {
  // Use exponential moving average, simpler than creating an actual moving avg array
  // curr_volume = _calc_rms(_v_real, FFT_SAMPLES);
  curr_volume = _calc_rms_scaled(_v_real, FFT_SAMPLES);

  if (audio_first_loop) { // the first time through the loop, we have no history, so set the avg volume to the curr volume
    avg_volume = curr_volume;
  } else {
    avg_volume = (VOL_AVG_SCALE * curr_volume) + (avg_volume * (1 - VOL_AVG_SCALE));
  }

  if (curr_volume > VOL_PEAK_FACTOR * avg_volume) {   // we have a peak
    avg_peak_volume = (VOL_AVG_SCALE * curr_volume) + (avg_peak_volume * (1 - VOL_AVG_SCALE));
  }
  
  if (_VOLUME_SCALING) {
    _max_fft_val = avg_volume * VOL_MULT;
    // if (_max_fft_val < 1.0) {
    //   _max_fft_val = 1.0;        // since max_val is a divider, avoid less than 1
    // }
  } else {
    _max_fft_val = FFT_FIXED_MAX_VAL; // fixed value if we are not scaling
  }

  // Serial.print(curr_volume*10,8);
  // Serial.print(",");
  // Serial.println(avg_volume*10,8);
}

void AudioProcessor::run_fft() {
  _perform_fft();
  // for (int i=0; i < FFT_SAMPLES/2; i++) {
  //   Serial.print(_v_real[i], 16);
  //   Serial.print(",");
  // }
  // Serial.println("");

  _postprocess_fft();
  _detect_beat();

  // Serial.print(_fft_bin[1]); Serial.print(","); Serial.print((_fft_bin[4] + _fft_bin[5] + _fft_bin[6] + _fft_bin[7] + _fft_bin[8]) / 5);
  // Serial.print("\n");
}

void AudioProcessor::_setup_audio_bins() {
  double nyquist_freq = I2S_SAMPLE_RATE / 2.0;
  double lowest_freq = LOWEST_FREQ_BAND;
  double highest_freq = HIGHEST_FREQ_BAND;

  if (highest_freq > nyquist_freq) {
    highest_freq = int(nyquist_freq);
    Serial.println("WARNING: highest frequency bin set to Nyquist, " + String(highest_freq));
  }

  double freq_mult_per_band = pow(double(highest_freq) / lowest_freq, 1.0 / (NUM_AUDIO_BANDS - 1));
  
  
  double bin_width = double(I2S_SAMPLE_RATE) / FFT_SAMPLES;
  double nbins = FFT_SAMPLES / 2.0 - 1;

  // We want to be conservative here and not pick up too much out-of-band noise. So use ceil and floor
  _lowest_bass_bin = int(ceil(LOWEST_BASS_FREQ / bin_width) - 1);
  _highest_bass_bin = int(floor(HIGHEST_BASS_FREQ / bin_width) - 1);
  Serial.println("lowest bass bin = " + String(_lowest_bass_bin) + ", lowest bass freq = " + String(((_lowest_bass_bin + 1) * bin_width)));
  Serial.println("highest bass bin = " + String(_highest_bass_bin) + ", highest bass freq = " + String(((_highest_bass_bin + 1) * bin_width)));

  Serial.print(String(freq_mult_per_band) + "," + String(nyquist_freq) + "," + String(bin_width) + "," + String(nbins) + "\n");

  for (int band = 0; band < NUM_AUDIO_BANDS; band++) {
    bin_freqs[band] = lowest_freq * pow(freq_mult_per_band, band);
    center_bins[band] = bin_freqs[band] / bin_width;
  }

  for (int band = 0; band < NUM_AUDIO_BANDS-1; band++) {
    high_bins[band] = (center_bins[band+1] - center_bins[band])/2.0 + center_bins[band];
  }
  high_bins[NUM_AUDIO_BANDS-1] = nbins;

  for (int band = NUM_AUDIO_BANDS-1; band >= 1; band--) {
    low_bins[band] = high_bins[band-1];
  }
  low_bins[0] = 0;

  for (int i=0; i<NUM_AUDIO_BANDS; i++) {
    Serial.print(String(int(round(bin_freqs[i]))) + "," + String(int(round(low_bins[i]))) + "," + String(int(round(high_bins[i]))) + "\n");
  }
}

void AudioProcessor::_perform_fft() {
  fft_execute(_real_fft_plan);
  // FFT results go into _v_imag, so we need to extract them and put them back int to _v_real

  memset(_v_real, 0, sizeof(_v_real));      // clear out the _v_real array

  // Data format from documentation
  // Input  : [ x[0], x[1], x[2], ..., x[NFFT-1] ]
  // Output : [ X[0], X[NFFT/2], Re(X[1]), Im(X[1]), ..., Re(X[NFFT/2-1]), Im(X[NFFT/2-1]) ]

  //_v_real[0] = _v_imag[0] / FFT_SAMPLES;  // DC term is zeroth element

  // Other terms alternate between real & imag so we need to calculate the amplitude
  for (int i = 1; i < FFT_SAMPLES / 2; i++) {
    double real, imag, magnitude, amplitude;
    real = _v_imag[i * 2];            // index: 2, 4, 6, ... , (FFT_SAMPLES/2 - 1) * 2
    imag = _v_imag[i * 2 + 1];        // index: 3, 5, 7, ... , (FFT_SAMPLES/2 - 1) * 2 + 1
    magnitude = sqrt(pow(real, 2) + pow(imag, 2));
    amplitude = magnitude * 2 / FFT_SAMPLES;  // scale factor
    _v_real[i-1] = amplitude; // skip the 0th term because it is the DC value, which we don't want to plot
  }
}

void AudioProcessor::_postprocess_fft() {
  // 1. Remove baseline FFT "noise"
  // 2. Apply white noise eq (optional)
  // 3. Apply A-weighting eq (optional)
  // 4. Apply perceptual binning (optional)
  
  double eq_mult, a_weighting_mult;
  int curr_bin, mapped_bin;

  _clear_fft_bin();      // clear out the fft_bin array as it has stale values

  for (int i = 0; i < FFT_SAMPLES / 2; i++) {
    _v_real[i] = (double(_v_real[i]) - double(FFT_REMOVE[i])) / _max_fft_val;        // remove noise and scale by max_val
    //_v_real[i] = (double(_v_real[i]) - 0) / _max_fft_val;        // remove noise and scale by max_val
    if (_v_real[i] < 0) {
      _v_real[i] = 0;
    }

    if (_WHITE_NOISE_EQ) {    // apply white noise eq
      eq_mult = 255.0 / double(FFT_EQ[i]);
      _v_real[i] *= eq_mult;
    }
    if (_A_WEIGHTING_EQ) {    // apply A-weighting eq
      a_weighting_mult = (double(A_WEIGHTING[i]) / 255.0);
      _v_real[i] *= a_weighting_mult;
    }
    // if (_PERCEPTUAL_BINNING) {           // apply perceptual binning, old version
    //   curr_bin = i;
    //   mapped_bin = pgm_read_byte(&GAMMA16_FFT[i]);

    //   // accumulate FFT samples in the new mapped bin
    //   _fft_bin[mapped_bin] += _v_real[curr_bin];

    if (_PERCEPTUAL_BINNING) {           // apply perceptual binning, only maps the lower half of the samples
      if (i < FFT_SAMPLES / 4) {
        curr_bin = i;
        mapped_bin = GAMMA16_FFT[i]*2;  // multiply by 2 to get the full mapping

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
    // Note that this is technically wrong as it creates FFT energy where there was none
    for (int i = 1; i < FFT_SAMPLES / 2 - 1; i++) {
      if (_fft_bin[i] == 0) {
        _fft_bin[i] = int(double(_fft_bin[i - 1] + _fft_bin[i + 1]) / 2);
      }
    }
  }
}

void AudioProcessor::_detect_beat() {
  for (int i = 0; i < FFTS_PER_SEC - 1; i++) {
    _bass_arr[i] = _bass_arr[i+1];  // shift elements forward
  }
  _bass_arr[FFTS_PER_SEC - 1] = 0; // reset last element

  double curr_bass = 0.0;
  for (int i = _lowest_bass_bin; i <= _highest_bass_bin; i++) {  // up to and including the highest bin
    // accumulate the bass from the clean (e.g. not perceptually-binned) FFT
    curr_bass += (_v_real[i]);
  }
  curr_bass /= (_highest_bass_bin - _lowest_bass_bin + 1); // average the bass bins (note we went up to and including the highest bin so add 1 here)
  _bass_arr[FFTS_PER_SEC - 1] = curr_bass;

  double bass_sum = 0.0;
  double bass_max = 0.0;
  for (int i = 0; i < FFTS_PER_SEC; i++) {
    bass_sum += _bass_arr[i]; // compute the sum of the history
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

  //double _thresh_bass = (-15 * _var_bass + 1.55) * _avg_bass;
  //double _thresh_bass = (bass_max - _avg_bass) * 0.5 + _avg_bass;
  double _thresh_bass = _avg_bass + 1.5 * sqrt(_var_bass);

  // must be loud enough, over thresh, and not too soon after the latest beat
  if (curr_bass > _thresh_bass && (20*log10(curr_volume)) > VOL_THRESH_DB && (millis() - _last_beat_ms) > 200) {
    beat_detected = true;
    _last_beat_ms = millis();
  } else {
    beat_detected = false;
  }

  //Serial.print(String(20*log10(curr_volume)) + "," + String(20*log10(avg_volume)) + "," + String(curr_bass) + "," + String(_avg_bass) + "," + String(_thresh_bass) + "," + String(curr_bass / bass_max) + "\n");
}

void AudioProcessor::calc_intensity(int length) {
  _interpolate_fft(FFT_SAMPLES/2, length);  // interpolate the FFT to the length of LEDs we want to illuminate

  for (int i = 0; i < length; i++) {
    if (_fft_interp[i] < 0) {
      _fft_interp[i] = 0;
    }
    double constrain_val = constrain(_fft_interp[i], 0, 1);                  // cap the range at [0 1]
    double pow_val = pow(constrain_val, FFT_SCALE_POWER);          // raise to the power to increase sensitivity, multiply by max value to increase resolution
    //double pow_val = constrain_val;
    //int map_val = map(pow_val, 0, 1, 0, BRIGHT_LEVELS);                // scale this into discrete LED brightness levels
    int map_val = int(round(pow_val * BRIGHT_LEVELS));

    // Serial.print(constrain_val, 8);
    // Serial.print(",");
    // Serial.print(pow_val, 8);
    // Serial.print(",");
    // Serial.println(map_val);
  
    intensity[i] -= FADE;            // fade first
    if (intensity[i] < 0) intensity[i] = 0;

    if ((map_val >= intensity[i]) && (20*log10(avg_volume) > VOL_THRESH_DB) && (map_val >= MIN_BRIGHT_UPDATE)) {  // if the FFT value is brighter AND our volume is loud enough that we trust the data AND it is brighter than our min thresh, update
      intensity[i] = map_val;
      intensity[i] = _last_intensity[i] * (1 - LED_SMOOTHING) + intensity[i] * (LED_SMOOTHING); // apply a smoothing parameter
    }
    if (intensity[i] < MIN_BRIGHT_FADE) {  // if less than the min value, floor to zero to reduce flicker
      intensity[i] = 0;
    }

    _last_intensity[i] = intensity[i]; // update the previous intensity value
  }
}

// Simple version that does no scaling or fading
void AudioProcessor::calc_intensity_simple(int length) {
  _clear_fft_bin();

  // bin down using the audio bands
  for (int i=0; i < FFT_SAMPLES / 2; i++) {
    if (i == 0) _fft_bin[i] += _v_real[i];
    else {
      for (int j=0; j < NUM_AUDIO_BANDS; j++) {
        if (i>int(round(low_bins[j])) && i<=int(round(high_bins[j]))) _fft_bin[j]  += _v_real[i];
      }
    }
  }

  // interpolate back up to the length desired.
  _interpolate_fft(NUM_AUDIO_BANDS, length);

  // // Do the calculation we would normally do
  // for (int i = 0; i < length; i++) {
  //   if (_fft_interp[i] < 0) {
  //     _fft_interp[i] = 0;
  //   }

  //   double constrain_val = constrain(_fft_interp[i], 0, 1);                  // cap the range at [0 1]
  //   double pow_val = pow(constrain_val, FFT_SCALE_POWER) * _max_fft_val;          // raise to the power to increase sensitivity, multiply by max value to increase resolution
  //   int map_val = map(pow_val, 0, _max_fft_val, 0, BRIGHT_LEVELS);                // scale this into discrete LED brightness levels

  //   intensity[i] -= FADE * 20;            // fade first

  //   if ((map_val >= intensity[i]) && (curr_volume > VOL_THRESH) && (map_val >= MIN_BRIGHT_UPDATE)) {   // if the FFT value is brighter AND our volume is loud enough that we trust the data AND it is brighter than our min thresh, update
  //     intensity[i] = map_val;
  //     intensity[i] = _last_intensity[i] * (1 - LED_SMOOTHING) + intensity[i] * (LED_SMOOTHING); // apply a smoothing parameter
  //   }
  //   if (intensity[i] < MIN_BRIGHT_FADE) {  // if less than the min value, floor to zero to reduce flicker
  //     intensity[i] = 0;
  //   }

  //   _last_intensity[i] = intensity[i]; // update the previous intensity value
  // }

  for (int i = 0; i < length; i++) {
    intensity[i] = int(round(_fft_interp[i] * 16));
  }
}

void AudioProcessor::print_double_array(double *arr, int len) {
  for (int i = 0; i < len; i++) {
    Serial.print(arr[i]); 
    Serial.print(",");
  }
  Serial.print("\n");
}

double AudioProcessor::_calc_rms(float *arr, int len) {
  double sum = 0;

  for (int i=0; i<len; i++) {
    sum = sum + arr[i]*arr[i];
  }
  return sqrt(sum/len);
}

// Calculate RMS accounting for max RMS
double AudioProcessor::_calc_rms_scaled(float *arr, int len) {
  double sum = 0;
  double rms_scale_val = sqrt(2) / 2;

  for (int i=0; i<len; i++) {
    sum = sum + arr[i] * arr[i];
  }

  return constrain(sqrt(sum/len) / rms_scale_val, 0, 1);
}

void AudioProcessor::_clear_fft_bin() {
  memset(_fft_bin, 0, sizeof(_fft_bin));      // clear out the fft_bin array as it accumulates stale values
}

void AudioProcessor::_interpolate_fft(int old_length, int new_length) {
  if (new_length > NUM_LEDS) {
    Serial.println("WARNING: trying to interpolate into _fft_interp array that is too small!");
  }

  // TODO: there is a lot of repeated code here, refactor it
  
  // Three scenarios:
  // 1) new_length > old_length --> linearly interpolate up to new_length
  // 2) new_length == old_length --> do nothing!
  // 3) new_length < old_length --> find smallest integer multiple that gets new_length*MULT > FFT_SAMPLES/2
  //                                 interpolate up to new_length*MULT
  //                                 moving avg down to new_length
  
  double scaled_index, fractional_index, floor_val, next_val, interp_val;
  int16_t floor_index;
  double fft_upscale[int(ceil(double(old_length) / double(new_length))) * new_length];   // this is used temporarily when interpolating the fft array to enable us to downscale via simple averaging

  // Case 1: straight interpolate up
  if (new_length > old_length) {           
    for (int i = 0; i < new_length; i++) {
      scaled_index = (i / double((new_length - 1))) * double(old_length - 1);  // find the index where we should be within the FFT bins
      floor_index = uint16_t(floor(scaled_index));                                // take the integer part of the index
      fractional_index = scaled_index - floor_index;                              // take the fractional part of the index
      floor_val = _fft_bin[floor_index];                                           // take the FFT at the integer index
      if ((floor_index + 1) < old_length) {                                  // find the FFT at the next index
        next_val = _fft_bin[floor_index + 1];
      } else {
        next_val = _fft_bin[floor_index];
      }

      interp_val = double(floor_val) + double(next_val - floor_val) * fractional_index;   // linear interpolation between the indices
      _fft_interp[i] = interp_val;
    }
  // Case 2: do nothing  
  } else if (new_length == old_length) {   
    for (int i = 0; i < new_length; i++) {      
      _fft_interp[i] = _fft_bin[i];
    }
  // Case 3: interpolate up then average to decimate down
  } else if (new_length < old_length) {    
    int multiple = ceil(double(old_length) / double(new_length)); // 2
    int upscale_len = multiple * new_length; // 168
    // upscale to 168
    for (int i = 0; i < upscale_len; i++) {
      scaled_index = (i / double((upscale_len - 1))) * double(old_length - 1);   // find the index where we should be within the FFT bins
      floor_index = uint16_t(floor(scaled_index));                                    // take the integer part of the index
      fractional_index = scaled_index - floor_index;                                  // take the fractional part of the index
      floor_val = _fft_bin[floor_index];                                               // take the FFT at the integer index
      if ((floor_index + 1) < old_length) {                                      // find the FFT at the next index
        next_val = _fft_bin[floor_index + 1];
      } else {
        next_val = _fft_bin[floor_index];
      }

      interp_val = double(floor_val) + double(next_val - floor_val) * fractional_index;   // linear interpolation between the indices
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
    Serial.println("ERROR: interpolate_fft() failed, should not have gotten here");
  }
}
