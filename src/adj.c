/*
# Alsa DJ sync tools, Midi clock using ALSA sequencers and tick scheduling

## Features:

- configurable BPM
- keeps events on the sequencer queue
- start and stop with space bar
- nudge with arrow keys (i.e. you can use this to beat-mix midi devices)

@author teknopaul 2021

*/

#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <inttypes.h>

#include <cdj/vdj.h>

#include "adj.h"
#include "adj_keyb.h"
#include "adj_midiin.h"
#include "adj_vdj.h"
#include "tui.h"


static struct timespec adj_pause = { 0L, 50000000L };
static unsigned _Atomic adj_paused = ATOMIC_VAR_INIT(1);

static void usage()
{
    printf("options:\n");
    printf("    -b - set the bpm (default 120.0)\n");
    printf("    -a - auto start, dont wait for space bar\n");
    printf("    -p - aconnect adj:clock to a midi port, N.B. whitespace in port names e.g. -p 'TR-6S:TR-6S MIDI 1    '\n");
    printf("    -n - change the name of the sequencer in alsa (default 'adj')\n");
    printf("    -y - use alsa sync feature\n");
    printf("    -k - keyboard input\n");
    printf("    -i - aconnect a midi port to adj for midi control input, (check /etc/adj-midimap.adjm)\n");
    printf("    -v - connect as a Virtual CD to Pioneer decks\n");
    printf("    -h - display this text\n");
    exit(0);
}


// user interface

char tui = 0;
static char keyb_input = 0;
static int message_ticks = 0;


static void init_error(char* msg)
{
    if (tui) tui_error_at(msg, 0, 15);
}
static void init_error_i(const char* msg, int i)
{
    if (tui) {
        int len = strlen(msg) + 15;
        char* text = (char*) calloc(1, len + 1);
        snprintf(text, len, msg, i);
        tui_error_at(text, 0, 15);
        free(text);
    }
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

static void tui_setup(int height)
{
    tui_init(height);
    tui_set_window_title("adj - powered by libadj and libvdj");

    data_item(ADJ_ITEM_PORT, "alsa port:", "");
    data_item(ADJ_ITEM_CLIENT_ID, "client_id:", "");
    data_item(ADJ_ITEM_MIDI_IN, "midi in:", "");
    data_item(ADJ_ITEM_MIDI_OUT, "midi out:", "");
    data_item(ADJ_ITEM_KEYB, "keyb:", "  ");
    data_item_fixed_width(ADJ_ITEM_STATE_SEQ, "seq state:", "uninit");
    data_item_fixed_width(ADJ_ITEM_STATE_Q, "q state:", "stopped");
    data_item_fixed_width(ADJ_ITEM_BPM, "bpm:", "0.0000000");
    data_item_fixed_width(ADJ_ITEM_EVENTS, "events:", "0");
    data_item_fixed_width(ADJ_ITEM_OP, "op:", "init");

    tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
}

// Symbol to render at a given quarter beat, when the sequencer is not at this point
static char* symbol_off(int q_beat)
{
    if (q_beat % 16 == 0) return "|";
    if (q_beat % 4 == 0) return ":";
    return ".";
}

// Symbol to render at a given quarter beat, when the sequencer is at this point
static char* symbol_on(int q_beat)
{
    if (q_beat % 16 == 0) return "\033[7m|\033[m";  // 7m is negative
    if (q_beat % 4 == 0) return "\033[7m:\033[m";
    return "\033[7m.\033[m";
}

// x position of a given quarter beat
static int q_beat_x(int q_beat)
{
    return (q_beat % 64) + 2;
}

static void message_off()
{
    if (message_ticks && tui) {
        tui_lock();
        tui_set_cursor_pos(0, 0);
        tui_delete_line();
        tui_unlock();
        message_ticks = 0;
        fflush(stdout);
    }
}

static void signal_exit(int sig)
{
    adj_keyb_exit();
    adj_midiin_exit();

    if (tui) {
        tui_lock();
        tui = 0;
        tui_exit();
        tui_unlock();
    } else {
        putchar('\n');
    }

    if (keyb_input) {
        adj_keyb_reset_term();
    }

    // Ctrl+C is not a clean exit because signal handlers cannot interact with atomics
    // n.b neither adjh, midiin nor keyb shutdown cleanly
    if (sig == 2) {
        exit(0);
    } else if (sig) {
        exit(1);
    }
}


// callbacks

static void data_change_handler(adj_seq_info_t* adj, int idx, char* value)
{
    if (tui) {
        if  (idx <= ADJ_ITEM_OP) {
            tui_lock();
            tui_data_at_fixed(value, 10, 15, 14 - idx);
            tui_unlock();
            fflush(stdout);
        }
    }
}

static void message_handler(adj_seq_info_t* adj, char* message)
{
    if (tui) {
        tui_lock();
        message_ticks = 1;
        tui_set_cursor_pos(0, 0);
        tui_delete_line();
        tui_error_at(message, 2, 0);
        tui_unlock();
    }
    else puts(message);
    fflush(stdout);
}

static void tick_handler(adj_seq_info_t* adj, snd_seq_tick_time_t tick)
{
    int quarter_beats = tick == 0 ? 0 : tick / ADJ_CLOCKS_PER_BEAT;

    if (tui) {
        tui_lock();
        if (quarter_beats > 0) {
            tui_set_cursor_pos(q_beat_x(quarter_beats - 1), 1);
            puts(symbol_off(quarter_beats - 1));
        }
        tui_set_cursor_pos(q_beat_x(quarter_beats), 1);
        puts(symbol_on(quarter_beats));
        tui_unlock();
    }
    else {
        fputs(symbol_off(quarter_beats), stdout);
        if (quarter_beats % 64 == 63) {
            putchar('|');
            putchar('\n');
        }
    }
    fflush(stdout);
    if (adj->vdj) {
        if (quarter_beats % 4 == 0) {
            unsigned char bar_pos = 1 + (quarter_beats % 16) / 4;
            adj_vdj_beat(adj, bar_pos);
        }
    }
}

static void exit_handler(adj_seq_info_t* adj)
{
    adj_paused = 0;
    signal_exit(0);
}

static void stop_handler(adj_seq_info_t* adj)
{
    if (tui) {
        tui_lock();
        tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
        tui_unlock();
    }
    else puts("");
}
static void start_handler(adj_seq_info_t* adj)
{
    //if (tui) tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
}

int main(int argc, char* argv[])
{
    int rv;
    char data_change[161];
    char cli[2048];
    char* out_port_name = NULL;
    char* in_port_name = NULL;
    char auto_start = 0;
    char vdj = 0;
    uint32_t vdj_flags = VDJ_FLAG_DEV_XDJ | VDJ_FLAG_AUTO_ID;
    unsigned int enter_toggles = 0;
    vdj_t* v;
    adj_seq_info_t* adj = adj_calloc();
    adj->message_handler = message_handler;
    adj->data_change_handler = data_change_handler;
    adj->tick_handler = tick_handler;
    adj->stop_handler = stop_handler;
    adj->start_handler = start_handler;
    adj->exit_handler = exit_handler;

    // parse command line

    int c;
    while ( ( c = getopt(argc, argv, "b:n:p:i:heykav") ) != EOF) {
        switch (c) {
            case 'h':
                usage();
                break;
            case 'b':
                adj->bpm = strtof(optarg, NULL);
                break;
            case 'p':
                out_port_name = optarg;
                break;
            case 'i':
                in_port_name = optarg;
                break;
            case 'n': 
                adj->seq_name = optarg;
                break;
            case 'y': 
                adj->alsa_sync = 1;
                break;
            case 'k': 
                keyb_input = 1;
                break;
            case 'a': 
                auto_start = 1;
                break;
            case 'v': 
                vdj = 1;
                break;
            case 'e': 
                enter_toggles = 1;
                break;
        }
    }

    if ( isatty(STDOUT_FILENO) ) {
        tui = 1;
        tui_setup(vdj ? 21 : 15);
    }

    // setup alsa sequencer
    if ( adj_init_alsa(adj) != ADJ_OK) {
        init_error("alsa init failed");
        return 1;
    }

    if (tui) {
        snprintf(data_change, 161, "%s:clock", adj->seq_name);
        data_item(ADJ_ITEM_PORT, "alsa port:", data_change);

        snprintf(data_change, 161, "%i:0", adj->client_id);
        data_item(ADJ_ITEM_CLIENT_ID, "client_id:", data_change);
    }

    // wire up midi devices
    if (out_port_name) {
        cli[2047] = '0';
        snprintf(cli, 2047, "aconnect '%s:clock' '%s'", adj->seq_name, out_port_name);
        if (tui) tui_set_cursor_pos(0, 0);
        if ( system(cli) ) {
            fprintf(stderr, "aconnect '%s:clock' '%s' failed\n", adj->seq_name, out_port_name);
            return 1;
        } else {
            if (tui) data_item(ADJ_ITEM_MIDI_OUT, "midi out:", out_port_name);
        }
    }

    // wire up midi controllers
    if (in_port_name) {
        if ((rv = adj_midiin(adj)) == ADJ_OK) {
            cli[2047] = '0';
            snprintf(cli, 2047, "aconnect '%s' '%s:clock' ", in_port_name, adj->seq_name);
            if (tui) tui_set_cursor_pos(0, 0);
            if ( system(cli) ) {
                fprintf(stderr, "aconnect '%s' '%s:clock' failed\n", in_port_name, adj->seq_name);
                return 1;
            } else {
                if (tui) data_item(ADJ_ITEM_MIDI_IN, "midi in:", in_port_name);
            }
        } else {
            if (rv == ADJ_SYNTAX) {
                init_error_i("error: unable to read midi map /etc/adj-midimap.adjmm: %i\n", rv);
            } else {
                init_error_i("error: midi input failed: %i\n", rv);
            }
        }
    }

    // Key board handling
    signal(SIGINT, signal_exit);
    if (keyb_input) {
        if ( (rv = adj_keyb_input(adj, enter_toggles)) != ADJ_OK ) {
            init_error_i("error: keyb init failed: %i\n", rv);
        } else {
            if (tui) data_item(ADJ_ITEM_KEYB, "keyb:", "on");
        }
    }

    if (vdj) {
        if ( ! (v = adj_init_vdj(adj, NULL, vdj_flags, adj->bpm)) ) {
            init_error_i("error: vdj start failed: %i\n", 0);
            signal_exit(0);
            return 1;
        } else {
            adj->vdj = v;
        }
    }

    // start the midi sequencer
    if ( (rv = adj_init(adj)) != ADJ_OK ) {
        init_error_i("error: sequencer start failed: %i\n", rv);
        signal_exit(0);
        return 1;
    } else {

    }

    sched_yield();
    nanosleep(&adj_pause, (struct timespec *)NULL);

    if (auto_start) adj_start(adj);

    while (adj_paused) {
        nanosleep(&adj_pause, (struct timespec *)NULL);
        if (message_ticks && ++message_ticks > 50) {
            message_off();
        }
    }

    return 0;
}
