/*
 * Minimal text ui for painting to a terminal
 */

#ifndef _LIBTUI_INCLUDED_
#define _LIBTUI_INCLUDED_


#define TUI_NORMAL     "\033[0m"
#define TUI_BOLD       "\033[1m"
#define TUI_RED        "\033[31m"
#define TUI_GREY       "\033[37m"
#define TUI_YELLOW     "\033[33m"
#define TUI_UNDERLINE  "\033[4m"
#define TUI_BLINK      "\033[5m"

/**
 * termios uses columns (x) and rows (height - y)
 */
typedef struct  {
    int row;
    int col;
} dim;

/*
 * Initialize tui.
 * Turns of visible cursor
 * Scrolls down y lines, to create space for the ui.
 * Should be called once inthe lifetime of hte process
 */
void tui_init(int y);

/**
 * Restore termios and cusrsor back on
 * Prints on \n like most other cli apps do on exit so the prompt is in the correct place.
 * Should be called only once at the end.
 */
void tui_exit();

/*
 * Write a string at the coordinates x,y from the bottom left, (as is customary with a graph)
 *
 * 0,1  1,1
 * 0,0  1,0
 *
 */
void tui_text_at(char* text, int x, int y);

/**
 * write variable lanegth string in bold wrapped with square brackets
 */
void tui_data_at(char* data, int x, int y);

/**
 * Write a fixed length data item, i.e. one that might change while the app is running
 */
void tui_data_at_fixed(char* data, int width, int x, int y);

/**
 * Write an error message
 */
void tui_error_at(char* message, int x, int y);

void tui_debug(const char* format, ...);

/**
 * Moove the cursor this is required before any draw operation
 */
void tui_set_cursor_pos(int x, int y);

/**
 * Get the current width of the users terminal
 */
int tui_get_width();

/**
 * Get the current height of the users terminal
 */
int tui_get_height();

/**
 * Delete from cursorposition to the end of the line
 */
void tui_delete_line();

/**
 * Sets the tab title in gnome console.
 */
void tui_set_window_title(const char* title);

// turn on and off the blinking cursor

void tui_cursor_off();
void tui_cursor_on();

// store and restore the position of the cursor on the screen
void tui_cursor_store();
void tui_cursor_restore();

// tui_ functions are not thread safe.
// these functions allow locking externally
void tui_lock();
void tui_unlock();

#endif // _LIBTUI_INCLUDED_