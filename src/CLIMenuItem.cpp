#include "CLIMenuItem.h"

#include <Arduino.h>

#include "CLIMenu.h"

// Base constructor
CLIMenuItem::CLIMenuItem(const char *desc) {
    strncpy(this->_desc, desc, MAX_CLI_MENU_TEXT);
    this->_command = 0;
    this->_menu = 0;
};

// Basic function execution type (runs the base constructor first)
CLIMenuItem::CLIMenuItem(const char *desc, void (*command)()) : CLIMenuItem(desc) {
    this->_command = command;
}

// New menu execution type (runs the base constructor first)
CLIMenuItem::CLIMenuItem(const char *desc, CLIMenu *menu) : CLIMenuItem(desc) {
    this->_menu = menu;
}

// Base method to return item description
const char *CLIMenuItem::get_desc() {
    return this->_desc;
}

// Execute the associated command
void CLIMenuItem::execute() {
    if (this->_command != 0) {
        _command();
    }
    if (this->_menu != 0) {
        _menu->run_cli();
    }
}
