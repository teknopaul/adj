#include "adj_mod_seq_api.h"
/**
 * Example sequencer module.
 * Plays a note every 4 bars at the start of the bar. (e.g. a cymbal sample)
 * But does not play on the very first beat.
 *
 * If your drum machine just loops every 4 beats this might be useful.
 */

static adj_seq_event *ride_on;
static adj_seq_event *ride_off;
static adj_seq_cfg* cfg;
static uint8_t note = 43;

static int8_t bar_count = -1; // 1 - 4 for bar counter, 0 for not started yet

static void cfg_handler(char* name, char* value)
{
    if (strcmp("note", name) == 0 ) {
        note = atoi(value);
    }
}

int adj_mod_seq_init(adj_seq_info_t* adj, char* args, unsigned int flags)
{
    cfg = adj_mod_seq_cfg_parse(args, flags, &cfg_handler);
    ride_on = adj_mod_seq_noteon(adj, cfg->channel, note, cfg->velocity);
    ride_off = adj_mod_seq_noteoff(adj, cfg->channel, note);

    return 0;
}

/**
 * Called every 1/4 beat, return a linked list of the notes that should be played by the sequencer.
 *
 * @param bar_index 0 - 3 position in the bar
 * @param q - 0 - 3 position in the beat
 *
 * @return a NULL terminated linked list of events to play, or NULL
 */
adj_seq_event* adj_mod_seq_events_next(adj_seq_info_t* adj, uint8_t bar_index, uint8_t q)
{
    //printf("bar_count=%i\n", bar_count);
    // only consider starts of bars54
    if (bar_index == 0 && q == 0) {
        // skip the first beat at the start of the song
        if ( bar_count++ < 3) {
            return NULL;
        }
        // on bar 1, play the ride
        if (bar_count % 4 == 0) {
            return ride_on;
        }
        // on bar 2, send note off
        if (bar_count % 4 == 1) {
            return ride_off;
        }
    }
    return NULL;
}

void adj_mod_seq_events_free(adj_seq_info_t* adj, adj_seq_event* ev)
{
    // since we reuse events we dont free
}

void adj_mod_seq_stop(adj_seq_info_t* adj)
{
    bar_count = 0;
}

void adj_mod_exit()
{
}
