

#include "tui.h"
#include <time.h>
#include <unistd.h>

static void tick_sleep()
{
    struct timespec sl = {0};
    sl.tv_nsec = 125000000L;
    nanosleep(&sl, (struct timespec*)NULL);
}

static char* symbol_off(int tick)
{
    if (tick % 16 == 0) return "|";
    if (tick % 4 == 0) return ":";
    return ".";
}

static char* symbol_on(int tick)
{
    if (tick % 16 == 0) return "\033[7m|\033[m";
    if (tick % 4 == 0) return "\033[7m:\033[m";
    return "\033[7m.\033[m";
}

static int tick_x(int tick)
{
    return (tick % 64) + 2;
}

static void data_item(int idx, char* name, char* value)
{
    tui_text_at(name, 2, 14 - idx);
    tui_data_at(value, 15, 14 - idx);
}

static void data_item_fixed_width(int idx, char* name, char* value)
{
    tui_text_at(name, 2, 14 - idx);
    tui_data_at_fixed(value, 10, 15, 14 - idx);
}

static void render_tick(int tick)
{
    tui_set_cursor_pos(tick_x(tick), 1);
    puts(symbol_on(tick));
    tick_sleep();
    tui_set_cursor_pos(tick_x(tick), 1);
    puts(symbol_off(tick));
}


int main(int argc, char* argv[])
{

    if (isatty(STDOUT_FILENO)) {
        tui_init(15);

        tui_set_window_title("adj - powered by libadj");

        data_item(1, "alsa port:", "adj:clock");
        data_item(2, "client_id:", "128:0");
        data_item(3, "midi in:", "nanoKONTROL Studio:nanoKONTROL Studio MIDI 1");
        data_item(4, "midi out:", "Roland TR-6S:TR-6S MIDI 1");
        data_item(5, "keyb:", "active");
        data_item_fixed_width(6, "q state:", "running");
        data_item_fixed_width(7, "seq state:", "paused");
        data_item_fixed_width(8, "bpm:", "121.234");
        data_item_fixed_width(9, "op:", "");


        tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);

        for (int i = 0; i < 128 ; i++) {
            render_tick(i);
        }

        printf(TUI_NORMAL);
        tui_exit();
    }
    else {
        printf("  alsa port:   [adj:clock]\n");
        printf("  client_id:   [128:0]\n");
        printf("  midi in:     [nanoKONTROL Studio:nanoKONTROL Studio MIDI 1]\n");
        printf("  midi out:    [Roland TR-6S:TR-6S MIDI 1]\n");
        printf("  keyb:        [active]\n");
    }

    return 0;
}
