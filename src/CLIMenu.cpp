#include "CLIMenu.h"

#include <Arduino.h>

#include "CLIMenuItem.h"
#include "Constants.h"
#include "Utils.h"

CLIMenu::CLIMenu(const char *text, const CLIMenuItem *items, int nitems, CLIMenu *prev) {
    strncpy(this->_text, text, MAX_CLI_MENU_TEXT);
    for (int i = 0; i < nitems; i++) {
        memcpy(this->_items, items, sizeof(CLIMenuItem) * nitems);
    }
    this->_nitems = nitems;
    this->_prev = prev;
}

void CLIMenu::run_cli() {
    char rcvd[CLI_MAX_CHARS];
    while (true) {
        this->_show();
        get_input(rcvd);
        if (strlen(rcvd) > 0 && !this->_run_command(atoi(&rcvd[0]))) break;
    }
}

void CLIMenu::_show() {
    print("\n%s\n", this->_text);

    for (int i = 0; i < this->_nitems; i++) {
        print("\t%d. %s\n", i + 1, this->_items[i].get_desc());
    }
    print("\t0. ");
    if (this->_prev == 0) {
        print("Exit\n");
    } else {
        print("Return\n");
    }
}

bool CLIMenu::_run_command(int i) {
    if ((i >= 1) && (i <= this->_nitems)) {
        this->_items[i - 1].execute();
    } else if (i == 0) {  // exit
        return false;
    } else {
        print("Invalid selection\n");
    }
    return true;
}
