#include "adj_mod_seq_api.h"

/**
 * Example sequencer module.
 * Plays a note every beat, e.g. a kick
 */
static adj_seq_event bombo;
static adj_seq_event bombooff;
static snd_seq_ev_note_t bombo_noteon;
static snd_seq_ev_note_t bombo_noteoff;

int adj_mod_seq_init(adj_seq_info_t* adj, char* args, unsigned int flags)
{
    memset(&bombo, 0, sizeof(adj_seq_event));
    memset(&bombooff, 0, sizeof(adj_seq_event));
    memset(&bombo_noteon, 0, sizeof(snd_seq_ev_note_t));
    memset(&bombo_noteoff, 0, sizeof(snd_seq_ev_note_t));

    snd_seq_ev_set_source(&bombo.ev, adj->alsa_port);
    snd_seq_ev_set_subs(&bombo.ev);

    bombo.ev.type = SND_SEQ_EVENT_NOTEON;
    bombo.ev.data.channel = 1;
    bombo.ev.data.note = bombo_noteon;
    bombo.ev.data.note.note = 48; // C3
    bombo.ev.data.note.velocity = 127;

    snd_seq_ev_set_source(&bombooff.ev, adj->alsa_port);
    snd_seq_ev_set_subs(&bombooff.ev);

    bombooff.ev.type = SND_SEQ_EVENT_NOTEOFF;
    bombooff.ev.data.channel = 1;
    bombooff.ev.data.note = bombo_noteon;
    bombooff.ev.data.note.note = 48; // C3
    bombooff.ev.data.note.velocity = 0;
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
    if (q == 0) {
        return &bombo;
    }
    if (q == 3) {
        return &bombo_off;
    }
    return NULL;
}

void adj_mod_seq_events_free(adj_seq_info_t* adj, adj_seq_event* ev)
{
}

void adj_mod_seq_stop(adj_seq_info_t* adj)
{
}

void adj_mod_exit()
{
}
