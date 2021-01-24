/*
 * 
 * A small text ui library based on binjs
 * 
 * ./binjs/src/v8/term.cpp has code to read bytes and utf-8
 * 
 * To write, turn off the cursor, move it, putc and turn the cursor back on
 * 
 * aim of the game is to write this
 * 
 * alsa port:  [adj:clock]
 * client_id:  [128:0]
 * midi in:    [nanoKONTROL Studio:nanoKONTROL Studio MIDI 1]
 * midi out:   [Roland TR-6S:TR-6S MIDI 1]
 * midi out:   [LMMS:ctrl]
 * keyb:       [active   ]
 * q state:    [running  ]
 * seq state : [paused   ]
 * bpm:        [121.23000]
 * op:         [  nudge >]
 * 
 * |...!...!...!...|...!.█.!...!...|...!...!...!...|...!...!...!...|
 * 

  01: XDJ-1000
  bpm:    [0.0000000]
  state:  [0        ]
  
 */

#include "tui.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>

static struct termios* term_orig = NULL;
static pthread_mutex_t tui_mutex = PTHREAD_MUTEX_INITIALIZER;

static void tui_write_byte(char byte)
{
    putc(byte & 0xff, stdout);
}

static void tui_write_bytes(const char* bytes, int len)
{
    for ( int i = 0 ; i < len; i++) {
        putchar(bytes[i] & 0xff);
    }
}

static void tui_term_store()
{
    term_orig = (struct termios*) calloc(1, sizeof(struct termios));
    tcgetattr(0, term_orig);
}

static void tui_term_reset() {
    if (term_orig) {
        tcsetattr(0, TCSANOW, term_orig);
        free(term_orig);
        term_orig = NULL;
    }
}

/*
 * convert x ,y from bottom corner to row col used by termio
 */
static dim tui_translate(int x, int y) {
    dim rv;
    rv.row = tui_get_height() - y;
    rv.col = x + 1;
    return rv;
}

/**
 * crop the end off the string, if its too long (mallocs)
 */
static char* crop(char* string, int max_len) {
    char* new_string = calloc(1, max_len + 1);
    strncpy(new_string, string, max_len);
    new_string[max_len - 1] = '\0';
    return new_string;
}

/**
 * forces a string to a given length, cropping or padding with whitespace (mallocs)
 */
static char* fixed_length(char* string, int len) {
    int min = strlen(string) < len ? strlen(string) : len;
    int i;
    char* new_string = calloc(1, len + 1);
    for (i = min; i < len; i++) new_string[i] = ' ';
    strncpy(new_string, string, min);
    new_string[len - 1] = '\0';
    return new_string;
}

// public api (tui.h has comments)

void tui_init(int y)
{
    int i;
    tui_cursor_off();
    tui_term_store();
    for (i = 0 ; i < y - 1 ; i++) puts("");
}

void tui_exit()
{
    tui_set_cursor_pos(0, 0);
    printf("\n");
    tui_term_reset();
    tui_cursor_on();
}

void tui_text_at(char* string, int x, int y)
{
    if (y > tui_get_height()) return;

    tui_set_cursor_pos(x, y);
    char* new_string = crop(string, tui_get_width() - x);
    fputs(new_string, stdout);
    free(new_string);
}

void tui_data_at(char* string, int x, int y)
{
    if (y > tui_get_height()) return;

    tui_set_cursor_pos(x, y);
    putc('[', stdout);
    fputs(TUI_BOLD, stdout);
    char* new_string = crop(string, tui_get_width() - x - 2);
    fputs(new_string, stdout);
    free(new_string);
    fputs(TUI_NORMAL, stdout);
    putc(']', stdout);
}

void tui_data_at_fixed(char* string, int width, int x, int y)
{
    if (y > tui_get_height()) return;

    int avail_x = tui_get_width() - x - 2;
    if (avail_x < width) width = avail_x;
    
    tui_set_cursor_pos(x, y);
    putc('[', stdout);
    fputs(TUI_BOLD, stdout);
    char* new_string = fixed_length(string, width);
    fputs(new_string, stdout);
    free(new_string);
    fputs(TUI_NORMAL, stdout);
    putc(']', stdout);
}

void tui_error_at(char* string, int x, int y)
{
    if (y > tui_get_height()) return;

    tui_set_cursor_pos(x, y);
    fputs(TUI_RED, stdout);
    char* new_string = crop(string, tui_get_width() - x - 2);
    fputs(new_string, stdout);
    free(new_string);
    fputs(TUI_NORMAL, stdout);
    fflush(stdout);
}

int tui_get_width()
{
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return w.ws_col;
}

int tui_get_height()
{
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return w.ws_row;
}

void tui_delete_line() {
    const char delete_line[] = { 27, 91, 50, 75 }; // ESC[2K
    tui_write_bytes(delete_line, 4);
}

void tui_set_window_title(const char* title) {
    const char set_window_title[] = { 27, 93, 50, 59 } ;
    tui_write_bytes(set_window_title, 4); // ESC]2;
    fputs(title, stdout);
    tui_write_byte(7); // BEL
    //tui_write_bytes(27, 92); // ESC\ aka ST
    fflush(stdout);
}

void tui_set_cursor_pos(int x, int y) {
    dim p = tui_translate(x, y);
    
    const char set_cursor_pos[] = { 27, 91 }; // ESC[
    tui_write_bytes(set_cursor_pos, 2);
    printf("%i", p.row);
    tui_write_byte(59); // ;
    printf("%i", p.col);
    tui_write_byte(102); // f
    fflush(stdout);
}

void tui_cursor_off() {
    const char cursor_off[] = { 27, 91, 63, 50, 53, 108 }; // ESC[ ?25l
    tui_write_bytes(cursor_off, 6);
    fflush(stdout);
}

void tui_cursor_on() {
    const char cursor_on[] = { 27, 91, 63, 50, 53, 104 }; // ESC[ ?25h
    tui_write_bytes(cursor_on, 6);
    fflush(stdout);
}

void tui_cursor_store() {
    const char cursor_store[] = { 27, 91, 115 }; // ESC[s
    tui_write_bytes(cursor_store, 3);
    fflush(stdout);
}

void tui_cursor_restore() {
    const char cursor_restore[] = { 27, 91, 117 }; // ESC[u
    tui_write_bytes(cursor_restore, 3);
    fflush(stdout);
}

void tui_lock()
{
    pthread_mutex_lock(&tui_mutex);
}

void tui_unlock()
{
    pthread_mutex_unlock(&tui_mutex);
}

// public api