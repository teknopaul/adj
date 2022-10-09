#ifndef _ADJ_MOD_SEQ_INCLUDED_
#define _ADJ_MOD_SEQ_INCLUDED_

#include "../adj.h"

typedef struct adj_seq_event_s adj_seq_event;
typedef struct adj_seq_cfg_s adj_seq_cfg;

/**
 * ALSA sequencer events list, with relative time offset to the quarter beat (typically adj is for loop machined, so notes can be resued and repeated)
 *
 * Events should be created as follows...
 *
 *   snd_seq_event_t ev;
 *   snd_seq_ev_clear(&ev);
 *   ev.type = SND_SEQ_EVENT_NOTEON;
 *   snd_seq_ev_set_source(&ev, adj->alsa_port);
 *   snd_seq_ev_set_subs(&ev);
 *
 * When returning data to adj_mod_seq_events_next() the midi note information is needed
 *
 *   ev.data.control.channel = 1;
 *   ev.data.note.note = 15;
 *   ev.data.note.velocity = 100;
 *   ev.data.note.channel = 1;
 *
 * Control changes and raw data can be returned but, naturally, queue control (start/stop) should not be, behaviour will be undefined.
 *
 * libadj will call snd_seq_ev_schedule_tick() applying the tick offset and send the event to the sequencer.
 */

struct adj_seq_event_s  {
    snd_seq_event_t        ev;         // partially populated event (excluding time)
    uint8_t                offset;     // tick position ADJ_PPQ (0-95), libadj main_loop() provides the song position
    uint8_t                flags_sys;  // flags reserved for use by ADJ
    uint16_t               flags_usr;  // flags reserved for use by the module
    uint32_t               data;       // bit of arbitrary storage, e.g. for an LFO, or tick position in a loop
    adj_seq_event         *next;       // NULL or next in the list
};

struct adj_seq_cfg_s  {
    uint8_t                channel;    // midi channel
    uint8_t                velocity;   // defuilt velocity
};
/**
 * Initialize the sequencer module, configure the module and perhaps preallocating events.
 */
int adj_mod_seq_init(adj_seq_info_t* adj, char* args, unsigned int flags);

/**
 * Called every 1/4 beat, module should return a linked list of the notes that should be played by the sequencer.
 *
 * @param bar_index 0 -3 position in the bar
 * @param q - 0-3 position in the beat
 *
 * @return a NULL terminated linked list of events to play, or NULL
 */
adj_seq_event* adj_mod_seq_events_next(adj_seq_info_t* adj, uint8_t bar_index, uint8_t q);

/**
 * Called after passing the events to alsa, if the module dynamically allocated data it should free them.
 */
void adj_mod_seq_events_free(adj_seq_info_t* adj, adj_seq_event* ev);

/**
 * Called if the sequencer is stopped or restarted so the module can reset internal position counters
 */
void adj_mod_seq_stop(adj_seq_info_t* adj);

void adj_mod_exit();

// util methods

adj_seq_event* adj_mod_seq_noteon(adj_seq_info_t* adj, uint8_t channel, uint8_t note, uint8_t velocity);

adj_seq_event* adj_mod_seq_noteoff(adj_seq_info_t* adj, uint8_t channel, uint8_t note);

typedef void (*adj_mod_seq_cfg_handler_pt)(char* name, char* value);

adj_seq_cfg* adj_mod_seq_cfg_parse(char* args, unsigned int flags, adj_mod_seq_cfg_handler_pt cfg_handler);

#endif // _ADJ_MOD_SEQ_INCLUDED_
