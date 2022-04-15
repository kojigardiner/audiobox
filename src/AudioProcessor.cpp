#include "AudioProcessor.h"

// Default constructors
AudioProcessor::AudioProcessor() {
  init_variables();
}

// Alternate constructor to define FFT post-processing
AudioProcessor::AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning, bool volume_scaling) {
  _WHITE_NOISE_EQ = white_noise_eq;
  _A_WEIGHTING_EQ = a_weighting_eq;
  _PERCEPTUAL_BINNING = perceptual_binning;
  _VOLUME_SCALING = volume_scaling;

  init_variables();
}

void AudioProcessor::init_variables() {
    _setup_audio_bins();

   // Initialize arrays 
    memset(intensity, 0, sizeof(intensity));
    
    memset(_last_intensity, 0, sizeof(_last_intensity));
    memset(_fft_interp, 0, sizeof(_fft_interp));

    memset(_v_real, 0, sizeof(_v_real));
    memset(_v_imag, 0, sizeof(_v_imag));
    
    memset(_fft_clean, 0, sizeof(_fft_clean));
    memset(_fft_bin, 0, sizeof(_fft_bin));

    _max_val = 0;
    
    audio_first_loop = true;
    curr_volume = 0;
    avg_volume = 0;
    avg_peak_volume = 0;
    
    Serial.print("Audio processor variables initialized\n");
}

void AudioProcessor::set_audio_samples(double *samples) {
  for (int i = 0; i < FFT_SAMPLES; i++) {
    _v_real[i] = samples[i];
    _v_imag[i] = 0;
  }
}

void AudioProcessor::update_volume() {
  // Use exponential moving average, simpler than creating an actual moving avg array
  curr_volume = _calc_rms(_v_real, FFT_SAMPLES) * VOL_FACTOR;
  if (audio_first_loop) { // the first time through the loop, we have no history, so set the avg volume to the curr volume
    avg_volume = curr_volume;
  } else {
    avg_volume = (VOL_AVG_SCALE * curr_volume) + (avg_volume * (1 - VOL_AVG_SCALE));
  }

  if (curr_volume > VOL_PEAK_FACTOR * avg_volume) {   // we have a peak
    avg_peak_volume = (VOL_AVG_SCALE * curr_volume) + (avg_peak_volume * (1 - VOL_AVG_SCALE));
  }
  
  if (_VOLUME_SCALING) {
    _max_val = avg_volume * VOL_MULT;
    if (_max_val < 1.0) {
      _max_val = 1.0;        // since max_val is a divider, avoid less than 1
    }
  } else {
    _max_val = FFT_FIXED_MAX_VAL; // fixed value if we are not scaling
  }

  // Serial.print(curr_volume);
  // Serial.print(",");
  // Serial.println(avg_volume);

}

void AudioProcessor::run_fft() {
  _perform_fft();
  _postprocess_fft();
  _detect_beat();
  // Serial.print(_fft_bin[1]); Serial.print(","); Serial.print((_fft_bin[4] + _fft_bin[5] + _fft_bin[6] + _fft_bin[7] + _fft_bin[8]) / 5);
  // Serial.print("\n");
}

void AudioProcessor::_setup_audio_bins() {
  int sample_rate = I2S_SAMPLE_RATE;
  double nyquist_freq = sample_rate / 2.0;
  int nsamples = FFT_SAMPLES;
  int nbands = NUM_AUDIO_BANDS;

  int lowest_freq = 100;
  int highest_freq = 12000;


  if (highest_freq > nyquist_freq) {
    highest_freq = int(nyquist_freq);
    Serial.println("WARNING: highest frequency bin set to Nyquist, " + String(highest_freq));
  }

  double freq_mult_per_band = pow(double(highest_freq) / lowest_freq, 1.0 / (nbands - 1));
  
  
  double bin_width = double(sample_rate) / nsamples;
  double nbins = nsamples / 2.0 - 1;

  _highest_bass_bin = int(ceil(HIGHEST_BASS_FREQ / bin_width) - 1);
  Serial.println("highest bass bin = " + String(_highest_bass_bin) + ", highest bass freq = " + String(((_highest_bass_bin + 1) * bin_width)));

  Serial.print(String(freq_mult_per_band) + "," + String(nyquist_freq) + "," + String(bin_width) + "," + String(nbins) + "\n");
  double center_bins[nbands] = { 0 };
  double freqs[nbands] = { 0 };

  for (int band = 0; band < nbands; band++) {
    freqs[band] = lowest_freq * pow(freq_mult_per_band, band);
    center_bins[band] = freqs[band] / bin_width;
  }

  for (int band = 0; band < nbands-1; band++) {
    high_bins[band] = (center_bins[band+1] - center_bins[band])/2.0 + center_bins[band];
  }
  high_bins[nbands-1] = nbins;

  for (int band = nbands-1; band >= 1; band--) {
    low_bins[band] = high_bins[band-1];
  }
  low_bins[0] = 0;

  for (int i=0; i<nbands; i++) {
    Serial.print(String(int(round(freqs[i]))) + "," + String(int(round(low_bins[i]))) + "," + String(int(round(high_bins[i]))) + "\n");
  }
}

void AudioProcessor::_perform_fft() {
  _FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  _FFT.Compute(FFT_FORWARD);
  _FFT.ComplexToMagnitude();
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
    _fft_clean[i] = (double(_v_real[i]) - double(FFT_REMOVE[i])) / _max_val;        // remove noise and scale by max_val
    //_fft_clean[i] = (double(_v_real[i]) - 0) / _max_val;        // remove noise and scale by max_val
    if (_fft_clean[i] < 0) {
      _fft_clean[i] = 0;
    }

    if (_WHITE_NOISE_EQ) {    // apply white noise eq
      eq_mult = 255.0 / double(FFT_EQ[i]);
      _fft_clean[i] *= eq_mult;
    }
    if (_A_WEIGHTING_EQ) {    // apply A-weighting eq
      a_weighting_mult = (double(A_WEIGHTING[i]) / 255.0);
      _fft_clean[i] *= a_weighting_mult;
    }
    if (_PERCEPTUAL_BINNING) {           // apply perceptual binning
      curr_bin = i;
      mapped_bin = pgm_read_byte(&GAMMA8_FFT[i]);

      // accumulate FFT samples in the new mapped bin
      _fft_bin[mapped_bin] += _fft_clean[curr_bin];

    // if (_PERCEPTUAL_BINNING) {
    //   if (i == 0) _fft_bin[i] += _fft_clean[i];
    //   else {
    //     for (int j=0; j < NUM_AUDIO_BANDS; j++) {
    //       if (i>int(round(low_bins[j])) && i<=int(round(high_bins[j]))) _fft_bin[j]  += _fft_clean[i];
    //     }
    //   }
    } else {
      _fft_bin[i] = _fft_clean[i];
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
  for (int i = 0; i < _highest_bass_bin; i++) {
    // accumulate the bass from the clean (e.g. not perceptually-binned) FFT
    curr_bass += constrain(_fft_clean[i], 0, 1); // constrain to 0 to 1 to ensure avg & variance calcs work
  }
  curr_bass /= _highest_bass_bin; // average the bass bins
  _bass_arr[FFTS_PER_SEC - 1] = curr_bass;

  double bass_sum = 0.0;
  double bass_minus_avg_sq_sum = 0.0;
  double bass_max = 0.0;
  for (int i = 0; i < FFTS_PER_SEC; i++) {
    bass_sum += _bass_arr[i]; // compute the sum of the history
    bass_minus_avg_sq_sum += ((_bass_arr[i] - _avg_bass) * (_bass_arr[i] - _avg_bass));
    if (_bass_arr[i] > bass_max) {
      bass_max = _bass_arr[i];
    }
  }

  _avg_bass = bass_sum / FFTS_PER_SEC;  // compute the avg
  _var_bass = bass_minus_avg_sq_sum / FFTS_PER_SEC;  // compute the var
  //double _thresh_bass = (-15 * _var_bass + 1.55) * _avg_bass;
  //double _thresh_bass = (bass_max - _avg_bass) * 0.5 + _avg_bass;
  double _thresh_bass = _avg_bass + 7 * _var_bass;

  // must be loud enough, over thresh, and not too soon after the latest beat
  if (curr_bass > 0.2 && curr_bass > _thresh_bass && (millis() - _last_beat_ms) > 200) {
    beat_detected = true;
    _last_beat_ms = millis();
  } else {
    beat_detected = false;
  }

  //Serial.print(String(curr_bass*10) + "," + String(_avg_bass*10) + "," + String(_var_bass*10) + "," + String(_thresh_bass*10) + "\n");
}

void AudioProcessor::calc_intensity(int length) {
  _interpolate_fft(FFT_SAMPLES/2, length);  // interpolate the FFT to the length of LEDs we want to illuminate

  for (int i = 0; i < length; i++) {
    if (_fft_interp[i] < 0) {
      _fft_interp[i] = 0;
    }

    double constrain_val = constrain(_fft_interp[i], 0, 1);                  // cap the range at [0 1]
    double pow_val = pow(constrain_val, FFT_SCALE_POWER) * _max_val;          // raise to the power to increase sensitivity, multiply by max value to increase resolution
    int map_val = map(pow_val, 0, _max_val, 0, BRIGHT_LEVELS);                // scale this into discrete LED brightness levels

    intensity[i] -= FADE;            // fade first

    if ((map_val >= intensity[i]) && (curr_volume > VOL_THRESH) && (map_val >= MIN_BRIGHT_UPDATE)) {   // if the FFT value is brighter AND our volume is loud enough that we trust the data AND it is brighter than our min thresh, update
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
    if (i == 0) _fft_bin[i] += _fft_clean[i];
    else {
      for (int j=0; j < NUM_AUDIO_BANDS; j++) {
        if (i>int(round(low_bins[j])) && i<=int(round(high_bins[j]))) _fft_bin[j]  += _fft_clean[i];
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
  //   double pow_val = pow(constrain_val, FFT_SCALE_POWER) * _max_val;          // raise to the power to increase sensitivity, multiply by max value to increase resolution
  //   int map_val = map(pow_val, 0, _max_val, 0, BRIGHT_LEVELS);                // scale this into discrete LED brightness levels

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

double AudioProcessor::_calc_rms(double *arr, int len) {
  double sum = 0;
  for (int i=0; i<len; i++) {
    sum = sum + arr[i]*arr[i];
  }
  return sqrt(sum/len);
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
