#ifndef CLI_H
#define CLI_H

#define MAX_CHARS 256
#include <Arduino.h>
#include <Preferences.h>

#include "Constants.h"
#include "NetworkConstants.h"

void cli_main();
void cli_wifi();
void cli_spotify();
void cli_clear_prefs();
int get_input(char *rcvd);

#endif  // CLI_H