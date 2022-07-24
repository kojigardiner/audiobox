# 16x16 LED Box

Code for an ESP32-driven music-reactive 16x16 LED panel in a wooden enclosure (box). 

## Features
- Display album art corresponding to the music playing on a linked Spotify account
- Extract the color palette from the current album art and use it to color visualizations
- Display a variety of FFT-based music visualizations using an optional external microphone
- Automatically adjust LED configuration for diffuse vs sharply defined art
- Two push-buttons for mode control
- Command line interface for establishing wifi connection and Spotify account authorization
- (future) User-selected BMP and GIF support
- (future) Web app control
- (future) Text display

## Design
Under the hood (inside the box), the LED panel is attached to a rack-and-pinion system that moves the display with respect to the diffuser. This allows for different LED effects (soft/diffuse vs sharply defined). The ESP32 drives this behavior via commands to a servo motor.

## Attributions
https://github.com/s-marley/ESP32_FFT_VU - VU-meter and waterfall FFT effects
https://www.modustrialmaker.com/blog/tag/diy+bluetooth+speaker - inspiration for LED distance adjustment
