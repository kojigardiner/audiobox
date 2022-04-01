#include "AudioProcessor.h"

// Default constructors
AudioProcessor::AudioProcessor() {
}

// Alternate constructor to define FFT post-processing
AudioProcessor::AudioProcessor(bool white_noise_eq, bool a_weighting_eq, bool perceptual_binning) {
  _WHITE_NOISE_EQ = white_noise_eq;
  _A_WEIGHTING_EQ = a_weighting_eq;
  _PERCEPTUAL_BINNING = perceptual_binning;
}

void AudioProcessor::init_variables() {
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
  
  _max_val = avg_volume * VOL_MULT;
  if (_max_val < 1.0) {
    _max_val = 1.0;        // since max_val is a divider, avoid less than 1
  }

  // Serial.print(curr_volume);
  // Serial.print(",");
  // Serial.println(avg_volume);

}

void AudioProcessor::run_fft() {
  _perform_fft();
  _postprocess_fft();
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

void AudioProcessor::calc_intensity(int length) {
  _interpolate_fft(length);  // interpolate the FFT to the length of LEDs we want to illuminate

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

void AudioProcessor::_interpolate_fft(int new_length) {
  // TODO: there is a lot of repeated code here, refactor it
  
  // Three scenarios:
  // 1) NUM_LEDS > FFT_SAMPLES/2 --> linearly interpolate up to NUM_LEDS
  // 2) NUM_LEDS == FFT_SAMPLES/2 --> do nothing!
  // 3) NUM_LEDS < FFT_SAMPLES/2 --> find smallest integer multiple that gets NUM_LEDS*MULT > FFT_SAMPLES/2
  //                                 interpolate up to NUM_LEDS*MULT
  //                                 moving avg down to NUM_LEDS
  
  double scaled_index, fractional_index, floor_val, next_val, interp_val;
  int16_t floor_index;
  double fft_upscale[int(ceil(double(FFT_SAMPLES / 2) / double(NUM_LEDS))) * NUM_LEDS];   // this is used temporarily when interpolating the fft array to enable us to downscale via simple averaging

  // Case 1: straight interpolate up
  if (new_length > FFT_SAMPLES / 2) {           
    for (int i = 0; i < new_length; i++) {
      scaled_index = (i / double((new_length - 1))) * double(FFT_SAMPLES / 2 - 1);  // find the index where we should be within the FFT bins
      floor_index = uint16_t(floor(scaled_index));                                // take the integer part of the index
      fractional_index = scaled_index - floor_index;                              // take the fractional part of the index
      floor_val = _fft_bin[floor_index];                                           // take the FFT at the integer index
      if ((floor_index + 1) < FFT_SAMPLES / 2) {                                  // find the FFT at the next index
        next_val = _fft_bin[floor_index + 1];
      } else {
        next_val = _fft_bin[floor_index];
      }

      interp_val = double(floor_val) + double(next_val - floor_val) * fractional_index;   // linear interpolation between the indices
      _fft_interp[i] = interp_val;
    }
  // Case 2: do nothing  
  } else if (new_length == FFT_SAMPLES / 2) {   
    for (int i = 0; i < new_length; i++) {      
      _fft_interp[i] = _fft_bin[i];
    }
  // Case 3: interpolate up then average to decimate down
  } else if (new_length < FFT_SAMPLES / 2) {    
    int multiple = ceil(double(FFT_SAMPLES / 2) / double(new_length)); // 2
    int upscale_len = multiple * new_length; // 168
    // upscale to 168
    for (int i = 0; i < upscale_len; i++) {
      scaled_index = (i / double((upscale_len - 1))) * double(FFT_SAMPLES / 2 - 1);   // find the index where we should be within the FFT bins
      floor_index = uint16_t(floor(scaled_index));                                    // take the integer part of the index
      fractional_index = scaled_index - floor_index;                                  // take the fractional part of the index
      floor_val = _fft_bin[floor_index];                                               // take the FFT at the integer index
      if ((floor_index + 1) < FFT_SAMPLES / 2) {                                      // find the FFT at the next index
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
