
#include <stdatomic.h>

#include "adj.h"
#include "adj_keyb.h"
#include "adj_vdj.h"

/**
 * Number pad PC keyboard input
 *
 * @author teknopaul 2021
 */

static struct termios* term_orig = NULL;
static unsigned int key_flags;
static unsigned int difflock = 0;

static unsigned _Atomic adj_numpad_running = ATOMIC_VAR_INIT(0);


struct thread_info {
    pthread_t       thread_id;
    adj_seq_info_t* adj;
};

static void init_term()
{
    term_orig = (struct termios*) calloc(1, sizeof(struct termios));
    tcgetattr(0, term_orig);
    struct termios new_term;
    memcpy(&new_term, term_orig, sizeof(struct termios));
    new_term.c_lflag &= ~ICANON;         // disable buffered i/o
    new_term.c_lflag &= ~ECHO;           // echo off
    tcsetattr(0, TCSANOW, &new_term);
}


static int difflock_master = 0;

//SNIP_set_bpm

// ability to set bpm with a number pad, rather than a full keyboard
// users types the bpm with 2 decimal places e.g. 120.00
// when we have 6 chars we set the bpm

static char new_bpm[7] ;
static int new_bpm_pos = 0;

static void reset_char_bpm()
{
    new_bpm_pos = 0;
    memset(new_bpm, 0 , 7);
}

static int add_char_bpm(char next)
{
    if (new_bpm_pos < 6) {
        new_bpm[new_bpm_pos++] = next;
    }
    return new_bpm_pos == 6 ? 1 : 0;
}

static float get_bpm()
{
    if (new_bpm_pos == 6 ) {
        float bpm =  strtof(new_bpm, NULL);
        if (bpm > ADJ_MIN_BPM && bpm < ADJ_MAX_BPM) {
            return bpm;
        }
    }
    return -1.0;
}

//SNIP_set_bpm

void adj_numpad_reset_term()
{
    if (term_orig) {
        tcsetattr(0, TCSANOW, term_orig);
        free(term_orig);
        term_orig = NULL;
    }
}

static void* read_keys(void* arg)
{
    adj_seq_info_t* adj = arg;
    char ch;
    uint8_t player_id;

    while (adj_numpad_running) {
        if (adj_is_running()) {
            // arrows keys come as Esc[A
            ch = getchar();
            if (EOF == ch) {
                fprintf(stderr, "EOF keyboard input failed\n");
                break;
            }
            //printf("keyb input '%i'\n", (int)ch);
            else if (ch == '\033') {
                ch = getchar();
                if (ch == '[') {
                    ch = getchar();
                    switch(ch) {
                        case 'C':// right
                            adj_nudge(adj, 10);
                            continue;
                        case 'D':// left
                            adj_nudge(adj, -10);
                            continue;
                        case 'A': // up
                            adj_nudge(adj, 20);
                            continue;
                        case 'B':// down
                            adj_nudge(adj, -20);
                            continue;
                        case 'H':// home key
                            adj_quantized_restart(adj);
                            continue;
                        case 'F':// end key
                            adj_vdj_difflock_arff(adj);
                            difflock_master = 0;
                            continue;
                        default: 
                            continue;
                    }
                }

                continue;
            } 
            // enter toggles on off
            else if (ch == '\n') {
                adj_toggle(adj);
            } 
            // plus and minus, 0.1 resolution only
            else if (ch == '+') {
                if (difflock) {
                    adj_vdj_difflock_nudge(adj, 1);
                } else {
                    adj_adjust_tempo(adj, 0.1);
                }
            } 
            else if (ch == '-') {
                if (difflock) {
                    adj_vdj_difflock_nudge(adj, -1);
                } else {
                    adj_adjust_tempo(adj, -0.1);
                }
            }
            // copy bpm and sync to master player
            else if (ch == '*') {
                player_id = adj_vdj_copy_master(adj);
                if (player_id) {
                    adj_vdj_difflock(adj, player_id, 0);
                    difflock = 1;
                }
            }
            // copy bpm and sync to the other player
            else if (ch == '/') {
                player_id = adj_vdj_copy_other(adj);
                if (player_id) {
                    adj_vdj_difflock(adj, player_id, 0);
                    difflock = 1;
                }
            }
            // numbers, typing bpm directly
            else if (ch == '.' || (ch >= '0' && ch <= '9')) {
                    if (add_char_bpm(ch)) {
                        float bpm = get_bpm();
                        if (bpm > 0) {
                            adj_set_tempo(adj, bpm);
                            adj_vdj_difflock_arff(adj);
                        }
                        reset_char_bpm();
                    }
                continue;
            } else {
                //printf("%i\n", (int)ch);
            }
        }

        reset_char_bpm();
        // quarter beat sleep
        adj_one_beat_sleep(adj->bpm * 4);
    }

    return NULL;
}


int adj_numpad_input(adj_seq_info_t* adj, unsigned int flags)
{
    key_flags = flags;
    pthread_t thread_id;
    init_term();
    reset_char_bpm();
    adj_numpad_running = 1;

    int s = pthread_create(&thread_id, NULL, &read_keys, adj);
    if (s != 0) {
        return ADJ_THREAD;
    }

    return ADJ_OK;
}


void adj_numpad_exit()
{
    adj_numpad_running = 0;
}
