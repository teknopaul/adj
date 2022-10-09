#include "adj_mod_seq_api.h"

/**
 * Play 4 beats of midi notes from a file in a loop
 */

// list of all the notes we will ever need, (4 beat loop)
static adj_seq_event notes[4 * 4];
static uint8_t loop_pos = 0;

struct adj_midi_event {
    uint32_t type
    uint32_t len
    uint16_t delta_time
    load_events
    event
    next*
}

midi_event* load_events(args)
{
    return NULL;
}

int adj_mod_seq_init(adj_seq_info_t* adj, char* args, unsigned int flags)
{
    //snprintf(path, "/var/adj/mod_midi/chan%02d.mid");
    // read_header(path);
    int i = 0;
    adj_midi_event *ev = NULL;
    for ( i = 0; ev = NULL; i++) {
        chunk = read_chunk(adj, path);
        while (ev) {
            ev.delta_time = chunk.delta_time; //ticks from last note, we want ticks since last beat
            next = (adj_seq_event*) calloc(1, sizeof(adj_seq_event));
            if (change_quarter(ev)) {
                note->next = next;
                note = next;
            } else {
                note = next;
            }

            snd_seq_ev_set_source(&note.ev, adj->alsa_port);
            snd_seq_ev_set_subs(&note.ev);
            note.offset = ev.delta_time;
            note.ev.type = get_type(ev);
            note.ev.data.channel = ev->channel;
            note.ev.data.note = ev->note;
            note.ev.data.velocity = ev->velocity;

            if (ev.delta_time > ADJ_PPQ * 4 * 4) return;
        }
    }
}

/**
 * Called every 1/4 beat, module should return a linked list of the notes that should be played by the sequencer.
 *
 * @param bar_index 0 -3 position in the bar
 * @param q - 0-3 position in the beat
 *
 * @return a NULL terminated linked list of events to play, or NULL
 */
adj_seq_event* adj_mod_seq_events_next(adj_seq_info_t* adj, uint8_t bar_index, uint8_t q)
{
    return notes[((bar_index * 4) + q) % 16];
}

void adj_mod_seq_events_free(adj_seq_info_t* adj, adj_seq_event* ev)
{
    // since we reuse events we dont free
}

void adj_mod_seq_stop(adj_seq_info_t* adj)
{
}

void adj_mod_exit()
{
}
