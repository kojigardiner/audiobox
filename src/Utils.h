#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>
#include <Preferences.h>

// This header and its associated Utils.cpp file define utility functions that are used
// throughout the project by various classes.

// Attempts to connect to the wifi network using the SSID and password stored in Preferences.
// Returns true if connection is successful and false otherwise. Accepts an optional async
// flag which returns immediately without waiting for a successful connection to be reported.
bool connect_wifi(bool async = false);

// Checks that all necessary preferences (i.e. for WiFi and Spotify) have been set in the 
// non-volatile Preferences memory on the ESP32. Returns true if all are set properly and
// false otherwise.
bool check_prefs();

// Prompts a user for input over the serial connection and terminates on a line feed ('\n'). 
// Returns the number of bytes written into rcvd. rcvd should hold at least CLI_MAX_CHARS bytes.
int get_input(char *rcvd);

// Writes a specific value for the given key into the Preferences dictionary in the ESP32's
// non-volatile memory. If value is NULL, requests user input via serial and stores that
// input as the value.
void set_pref(Preferences *prefs, const char *key, const char *value = NULL);

// Computes a base64 encoding of a string composed of "<user>:<pass>" and stores it in
// auth_b64. auth_b64 should hold at least CLI_MAX_CHARS bytes.
void compute_auth_b64(const char *user, const char *pass, char *auth_b64);


// Wrapper for printing formatted strings to the serial port using c-strings. Accepts 
// standard printf format strings. Will generate an error if the formatted string
// exceeds HTTP_MAX_CHARS bytes.
void print(const char *format, ...);

// Prints details on heap and stack memory usage along with the calling function's name
// and line of code. Debug function that is called by the get_memory_stats() macro.
void get_memory_stats_with_caller(char const *caller_name, int line);

// Macro for printing memory stats for debug.
#define get_memory_stats() get_memory_stats_with_caller(__func__, __LINE__)

#endif  // _UTILS_H