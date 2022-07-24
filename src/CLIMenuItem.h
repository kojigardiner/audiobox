#ifndef _CLIMENUITEM_H
#define _CLIMENUITEM_H

#define MAX_CLI_MENU_ITEMS 9
#define MAX_CLI_MENU_TEXT 256

// Forward declaration
class CLIMenu;

// Defines a menu item in a CLI menu system
class CLIMenuItem {
   public:
    // Constructors
    CLIMenuItem() = default;
    CLIMenuItem(const char *desc);                     // base constructor
    CLIMenuItem(const char *desc, void (*command)());  // basic function execution
    CLIMenuItem(const char *desc, CLIMenu *menu);      // new menu execution

    void execute();
    const char *get_desc();

   protected:
    char _desc[MAX_CLI_MENU_TEXT];
    void (*_command)();  // function pointer
    CLIMenu *_menu;      // pointer to a menu
};

#endif  // _CLIMENUITEM_H