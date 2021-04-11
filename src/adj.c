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
#include "adj_numpad.h"
#include "adj_midiin.h"
#include "adj_conf.h"
#include "adj_tui.h"
#include "adj_cli.h"
#include "adj_vdj.h"

static void usage()
{
    printf("options:\n");
    printf("    -b - set the bpm (default 120.0)\n");
    printf("    -a - auto start, dont wait for space bar\n");
    printf("    -p - aconnect adj:clock to a midi port, N.B. whitespace in port names e.g. -p 'TR-6S:TR-6S MIDI 1    '\n");
    printf("    -n - change the name of the sequencer in alsa (default 'adj')\n");
    printf("    -y - use alsa sync feature\n");
    printf("    -k - keyboard input\n");
    printf("    -K - numpad input\n");
    printf("    -i - aconnect a midi port to adj for midi control input, (check /etc/adj-midimap.adjm)\n");
    printf("    -v - connect as a Virtual CDJ to Pioneer decks\n");
    printf("    -N - NIC for Virtual CDJ\n");
    printf("    -c - read config from /etc/adj.conf\n");
    printf("    -h - display this text\n");
    exit(0);
}

static unsigned _Atomic adj_running = ATOMIC_VAR_INIT(1);
static adj_ui_t ui = {0};
static char keyb_input = 0;
static char numpad_input = 0;


static void init_error(char* msg)
{
    ui.init_error_handler(&ui, msg);
}

static void init_error_i(const char* msg, int i)
{
    char txt[256];
    snprintf(txt, 1255, "%s : %i", msg, i);
    ui.init_error_handler(&ui, txt);
}

static void signal_exit(int sig)
{
    adj_running = 0;
    adj_keyb_exit();
    adj_midiin_exit();

    ui.exit_handler(&ui, sig);

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
    adj->ui->data_change_handler(adj->ui, adj, idx, value);
}

static void message_handler(adj_seq_info_t* adj, char* message)
{
    adj->ui->message_handler(adj->ui, adj, message);
}

static void tick_handler(adj_seq_info_t* adj, snd_seq_tick_time_t tick)
{
    int quarter_beats = tick == 0 ? 0 : tick / ADJ_CLOCKS_PER_BEAT;

    if (adj->vdj) {
        if (quarter_beats % 4 == 0) {
            unsigned char bar_pos = 1 + (quarter_beats % 16) / 4;
            adj_vdj_beat(adj, bar_pos);
        }
    }
    adj->ui->tick_handler(adj->ui, adj, tick);
}

static void exit_handler(adj_seq_info_t* adj)
{
    adj->ui->exit_handler(adj->ui, 0);
}

static void stop_handler(adj_seq_info_t* adj)
{
    adj->ui->stop_handler(adj->ui, adj);
    if (adj->vdj) adj_vdj_set_playing(adj, 0);
}

static void start_handler(adj_seq_info_t* adj)
{
    adj->ui->start_handler(adj->ui, adj);
    if (adj->vdj) adj_vdj_set_playing(adj, 1);
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
    char* iface = NULL;
    uint32_t vdj_offset = 20; // works on my machine
    char read_config = 0;
    uint32_t vdj_flags = VDJ_FLAG_DEV_XDJ | VDJ_FLAG_AUTO_ID;
    unsigned int enter_toggles = 0;
    vdj_t* v;
    adj_seq_info_t* adj = adj_calloc();
    adj->message_handler = message_handler;
    adj->data_change_handler = data_change_handler;
    adj->tick_handler = tick_handler;
    adj->beat_handler = NULL;
    adj->stop_handler = stop_handler;
    adj->start_handler = start_handler;
    adj->exit_handler = exit_handler;
    adj->ui = &ui;

    // parse command line

    int c;
    while ( ( c = getopt(argc, argv, "b:n:N:p:i:heykKvac") ) != EOF) {
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
            case 'N': 
                iface = optarg;
                break;
            case 'y': 
                adj->alsa_sync = 1;
                break;
            case 'k': 
                keyb_input = 1;
                break;
            case 'K': 
                numpad_input = 1;
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
            case 'c': 
                read_config = 1;
                break;
        }
    }

    if (argc == 1) read_config = 1;

    if (read_config) {
        adj_conf* conf = adj_conf_init();
        if (conf) {
            if (!out_port_name) out_port_name = conf->midi_out;
            if (!in_port_name) in_port_name = conf->midi_in;
            vdj = conf->vdj;
            vdj_flags |= conf->vdj_player;
            if (adj->bpm == 120.0) adj->bpm = conf->bpm;
            if (!iface) iface = conf->vdj_iface;
            vdj_offset = conf->vdj_offset;
            keyb_input = conf->keyb_in;
            numpad_input = conf->numpad_in;
            adj->alsa_sync = conf->alsa_sync;
        }
    }

    if (numpad_input) keyb_input = 0;

    if ( isatty(STDOUT_FILENO) ) {
        initialize_tui(&ui, vdj ? 1 : 0);
    } else {
        initialize_cli(&ui, vdj ? 1 : 0);
    }

    // setup alsa sequencer
    if ( adj_init_alsa(adj) != ADJ_OK) {
        init_error("alsa init failed");
        return 1;
    }

    snprintf(data_change, 161, "%s:clock", adj->seq_name);
    ui.data_item_handler(&ui, ADJ_ITEM_PORT, "alsa port:", data_change);

    snprintf(data_change, 161, "%i:0", adj->client_id);
    ui.data_item_handler(&ui, ADJ_ITEM_CLIENT_ID, "client_id:", data_change);


    // wire up midi devices
    if (out_port_name) {
        cli[2047] = '0';
        snprintf(cli, 2047, "aconnect '%s:clock' '%s'", adj->seq_name, out_port_name);
        if ( system(cli) ) {
            fprintf(stderr, "aconnect '%s:clock' '%s' failed\n", adj->seq_name, out_port_name);
            init_error("aconnect out port failed");
            return 1;
        } else {
            ui.data_item_handler(&ui, ADJ_ITEM_MIDI_OUT, "midi out:", out_port_name);
        }
    }

    // wire up midi controllers
    if (in_port_name) {
        if ((rv = adj_midiin(adj)) == ADJ_OK) {
            cli[2047] = '0';
            snprintf(cli, 2047, "aconnect '%s' '%s:clock' ", in_port_name, adj->seq_name);
            if ( system(cli) ) {
                fprintf(stderr, "aconnect '%s' '%s:clock' failed\n", in_port_name, adj->seq_name);
                init_error("aconnect in port failed");
                return 1;
            } else {
                ui.data_item_handler(&ui, ADJ_ITEM_MIDI_IN, "midi in:", in_port_name);
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
            ui.data_item_handler(&ui, ADJ_ITEM_KEYB, "keyb:", "on");
        }
    }

    // Numpad handling
    signal(SIGINT, signal_exit);
    if (numpad_input) {
        if ( (rv = adj_numpad_input(adj, enter_toggles)) != ADJ_OK ) {
            init_error_i("error: numpad init failed: %i\n", rv);
        } else {
            ui.data_item_handler(&ui, ADJ_ITEM_KEYB, "numpad:", "on");
        }
    }

    // Virtual CDJ
    if (vdj) {
        if ( ! (v = adj_vdj_init(adj, iface, vdj_flags, adj->bpm, vdj_offset)) ) {
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
    }

    sched_yield();
    usleep(50000);

    if (auto_start) adj_start(adj);

    adj_running = 1;
    while (adj_running) {
        usleep(50000);
    }

    return 0;
}
