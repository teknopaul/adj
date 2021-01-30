/*

# Midi clock using ALSA sequencers and tick scheduling

## Features:

- configurable BPM
- keeps events on the sequencer queue
- start and stop e.g. with space bar
- nudge, speed up or slow down the sequencer e.g. with arrow keys (i.e. you can use this to beat-mix midi devices)

@author teknopaul 2021

*/

#include "adj.h"

#include <pthread.h>
#include <stdatomic.h>

// Global state
static unsigned _Atomic adj_alsa_initialised = ATOMIC_VAR_INIT(0); // setup properly
static unsigned _Atomic adj_running = ATOMIC_VAR_INIT(0);          // main loop is alive
static unsigned _Atomic adj_paused = ATOMIC_VAR_INIT(0);           // alive but not making noises
static unsigned _Atomic adj_q_restart = ATOMIC_VAR_INIT(0);        // lock to start the alsa sequencer again

static pthread_mutex_t adj_sync_mutex = PTHREAD_MUTEX_INITIALIZER; // hang until we get a beat sync from an external device (e.g. CDJ)

//noop handlers
static void nop_message_handler(adj_seq_info_t* adj, char* message){}
static void nop_data_change_handler(adj_seq_info_t* adj, int item, char* data){}
static void nop_tick_handler(adj_seq_info_t* adj, snd_seq_tick_time_t tick){}
static void nop_stop_handler(adj_seq_info_t* adj){}
static void nop_start_handler(adj_seq_info_t* adj){}

// start midi

static int set_tempo(adj_seq_info_t* adj, float bpm)
{
    char* bpm_s;
    snd_seq_queue_tempo_t* tempo;
    snd_seq_queue_tempo_alloca(&tempo);
    snd_seq_queue_tempo_set_tempo(tempo, (unsigned int) (60000000 / bpm)); // microseconds in a minute / bpm = micros per beat
    snd_seq_queue_tempo_set_ppq(tempo, ADJ_PPQ);

    if ( snd_seq_set_queue_tempo(adj->alsa_seq, adj->q, tempo) == 0) {
        if ( (bpm_s = (char*) calloc(1, 11) )) {
            snprintf(bpm_s, 10, "%f", bpm);
            adj->data_change_handler(adj, ADJ_ITEM_BPM, bpm_s);
            free(bpm_s);
        }
        return ADJ_OK;
    } else {
        return ADJ_ALSA;
    }
}


static int set_tempo_micros(adj_seq_info_t* adj, unsigned int micros_per_beat)
{
    char* bpm_s;
    snd_seq_queue_tempo_t* tempo;
    snd_seq_queue_tempo_alloca(&tempo);
    snd_seq_queue_tempo_set_tempo(tempo, micros_per_beat);
    snd_seq_queue_tempo_set_ppq(tempo, ADJ_PPQ);

    if ( snd_seq_set_queue_tempo(adj->alsa_seq, adj->q, tempo) == 0) {
        if ( (bpm_s = (char*) calloc(1, 11) )) {
            snprintf(bpm_s, 10, "%f", adj_micros_to_bpm(micros_per_beat));
            adj->data_change_handler(adj, ADJ_ITEM_BPM, bpm_s);
            free(bpm_s);
        }
        return ADJ_OK;
    } else {
        return ADJ_ALSA;
    }
}

static int clear_queue(adj_seq_info_t* adj)
{
    snd_seq_remove_events_t* ev;
    snd_seq_remove_events_alloca(&ev);
    snd_seq_remove_events_set_queue(ev, adj->q);
    snd_seq_remove_events_set_condition(ev, SND_SEQ_REMOVE_OUTPUT | SND_SEQ_REMOVE_IGNORE_OFF);
    snd_seq_remove_events(adj->alsa_seq, ev);
    adj->data_change_handler(adj, ADJ_ITEM_EVENTS, "0");
    return ADJ_OK;
}

static int midi_start(adj_seq_info_t* adj)
{
    clear_queue(adj);
    // start the queue, tell midi devices about it
    snd_seq_start_queue(adj->alsa_seq, adj->q, NULL);
    adj->data_change_handler(adj, ADJ_ITEM_STATE_Q, "running");

    // send the start midi event
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_START;
    snd_seq_ev_set_source(&ev, adj->alsa_port);
    snd_seq_ev_set_subs(&ev);

    snd_seq_ev_schedule_tick(&ev, adj->q, SND_SEQ_TIME_MODE_ABS, ADJ_TICK0);
    snd_seq_event_output(adj->alsa_seq, &ev);
    snd_seq_drain_output(adj->alsa_seq);
    adj->start_handler(adj);

    return ADJ_OK;
}

static int midi_stop(adj_seq_info_t* adj)
{

    clear_queue(adj);

    // send STOP now/direct (tick = rel 0)
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_STOP;
    snd_seq_ev_set_source(&ev, adj->alsa_port);
    snd_seq_ev_set_subs(&ev);

    snd_seq_ev_schedule_tick(&ev, adj->q, SND_SEQ_TIME_MODE_REL, ADJ_TICK0);
    snd_seq_event_output_direct(adj->alsa_seq, &ev);
    adj->stop_handler(adj);

    snd_seq_stop_queue(adj->alsa_seq, adj->q, NULL);
    snd_seq_drain_output(adj->alsa_seq) ;
    adj->data_change_handler(adj, ADJ_ITEM_STATE_Q, "paused");

    return ADJ_OK;
}

static int send_midi_clock(adj_seq_info_t* adj, snd_seq_tick_time_t when)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_CLOCK;
    snd_seq_ev_set_source(&ev, adj->alsa_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_schedule_tick(&ev, adj->q, SND_SEQ_TIME_MODE_ABS, when);
    snd_seq_event_output(adj->alsa_seq, &ev);

    return ADJ_OK;
}

static snd_seq_tick_time_t adj_next_tick(adj_seq_info_t* adj)
{
    return adj->tick += 4;
}

static int report_events(adj_seq_info_t* adj, snd_seq_queue_status_t* info)
{
    char buf[23];
    snd_seq_get_queue_status(adj->alsa_seq, adj->q, info);
    int events = snd_seq_queue_status_get_events(info);

    snprintf(buf, 23, "%i", events);
    adj->data_change_handler(adj, ADJ_ITEM_EVENTS, buf);
    return events;
}

// end midi

// nudge, thread handles nudges and bpm changes

static unsigned _Atomic adj_nudge_running = ATOMIC_VAR_INIT(1);  // loop is running
static unsigned _Atomic adj_nudge_exec = ATOMIC_VAR_INIT(0);     // instruction to nudge
static signed _Atomic adj_nudge_multiplier = ATOMIC_VAR_INIT(0); // how much by as multiplier
static signed _Atomic adj_nudge_ms = ATOMIC_VAR_INIT(0);         // how much by as milliseconds
static float _Atomic adj_set_bpm = ATOMIC_VAR_INIT(0.0);         // new bpm
static float _Atomic adj_adjust_bpm = ATOMIC_VAR_INIT(0.0);      // bpm increment or decrement

/**
 * Returns a value that is greater than or less than the passed in bpm.
 * This is the amount the sequence will be speed up for a beat to beat mix midi instruments
 * Sequence is slowed down if multiplier is negative.
 * Resolution is 0.1 bpm, which is pretty fine, 10 or 20 is more convenient for beat mixing by ear at 120 bpm.
 */
static float adj_get_nudge_bpm(float bpm, int multiplier)
{
    float tmp_bpm = bpm + (0.1 * multiplier);
    if ( tmp_bpm < 0 ) {
        tmp_bpm = 0;
    }
    return tmp_bpm;
}

/**
 * return how much we need to change the bpm by for one beat to 
 * shift the sequencer by the specified number of milliseconds.
 * value returned is new bpm in micros_per_beat
 */
static unsigned int adj_get_nudge_micros(float bpm, int millis)
{
    return adj_bpm_to_micros(bpm) + millis * 1000;
}

/**
 * Thread that implements all tempo changes.  Nudge, adjust bpm and set new bpm
 */
static void* nudge_loop(void* arg)
{
    adj_seq_info_t* adj = (adj_seq_info_t*) arg;

    while (adj_nudge_running) {
        if (adj_nudge_exec) {
            if (adj_nudge_multiplier) {
                set_tempo(adj, adj_get_nudge_bpm(adj->bpm, adj_nudge_multiplier));
                adj_one_beat_sleep(adj->bpm);
                set_tempo(adj, adj->bpm);
            } else if (adj_nudge_ms) {
                set_tempo_micros(adj, adj_get_nudge_micros(adj->bpm, adj_nudge_ms));
                adj_one_beat_sleep(adj->bpm);
                set_tempo(adj, adj->bpm);
            }
            adj_nudge_multiplier = 0;
            adj_nudge_ms = 0;
            adj_nudge_exec = 0;
        } else {
            usleep(100000);
        }
        if (adj_set_bpm > 0) {
            set_tempo(adj, adj_set_bpm);
            adj->bpm = adj_set_bpm;
            adj_set_bpm = 0;
        }
        if (adj_adjust_bpm != 0.0) {
            float new_bpm = adj->bpm + adj_adjust_bpm;
            set_tempo(adj, new_bpm);
            adj->bpm = new_bpm;
            adj_adjust_bpm = 0.0;
        }
    }
    return ADJ_OK;
}

static int init_nudge(adj_seq_info_t* adj)
{
    pthread_t thread_id;
    return pthread_create(&thread_id, NULL, &nudge_loop, adj);
}

// timing

/**
 * @returns loop wait time time spec, potentially removing a tick or two to catch up if the loop is slow
 */
static struct timespec adj_beats_queued_time(float bpm, int drop_ticks)
{
    long int nanos = (long int) (ADJ_BEATS_QUEUED * (60000000000.0 / bpm)); // length of time to sleep, in nanos (0.25 beats)
    if (drop_ticks) {
        long int dropping =  (nanos / 6) * drop_ticks;
        nanos -= dropping;
    }

    struct timespec req = {0};
    req.tv_sec = nanos / 1000000000L;
    req.tv_nsec = nanos % 1000000000L;
    return req;
}

static void adj_loop_sleep(double bpm, int drop_ticks)
{
    struct timespec sl = adj_beats_queued_time(bpm, drop_ticks >= 0 ? drop_ticks : 0);
    nanosleep(&sl, (struct timespec*) NULL);
}
// timing

static void* main_loop(void* arg)
{
    int i;
    char buf[161];

    adj_seq_info_t* adj = arg;

    set_tempo(adj, adj->bpm);

    snd_seq_queue_status_t* info;
    snd_seq_queue_status_malloc(&info);
    snd_seq_get_queue_status(adj->alsa_seq, adj->q, info);
    snprintf(buf, 160, "%i", snd_seq_queue_status_get_events(info));
    adj->data_change_handler(adj, ADJ_ITEM_EVENTS, buf);

    // start the midi clock loop
    adj_running = 1;
    adj_paused = 1;
    adj->data_change_handler(adj, ADJ_ITEM_STATE_SEQ, "running");

    init_nudge(adj);

    int was_paused = 1;
    while (adj_running) {

        // here this thread is in sync with the sequencer to within a tick
        if (adj_q_restart && 0 == adj->tick % ADJ_PPQ * 4 * ADJ_BEATS_PER_BAR) {
            midi_stop(adj);
            adj->tick = ADJ_TICK0;
            midi_start(adj);
            adj_q_restart = 0;
        }

        while (adj_paused) {
            if (! was_paused) {
                midi_stop(adj);
            }
            was_paused = 1;
            if ( ! adj_running ) goto quit;
            usleep(10000);
        }

        if (was_paused) {
            pthread_mutex_lock(&adj_sync_mutex);
            pthread_mutex_unlock(&adj_sync_mutex);
            was_paused = 0;
            adj->tick = ADJ_TICK0;
            midi_start(adj);
        }
        

        // send first clock _before_ tick_handler() which may be slow, handler has 1/24th of a beat to finish
        send_midi_clock(adj, adj_next_tick(adj));
 
        adj->tick_handler(adj, adj->tick);

        for (i = 1 ; i < ADJ_CLOCKS_PER_BEAT * ADJ_BEATS_QUEUED; i++) {
            send_midi_clock(adj, adj_next_tick(adj));
        }
        snd_seq_drain_output(adj->alsa_seq);

        int events = report_events(adj, info);

        if (adj->alsa_sync) {
            // hang until queue is empty
            snd_seq_sync_output_queue(adj->alsa_seq);
        } else {
            // non alsa-sync occasionally we have to compensate from CPU cycles taking time
            // if we ever have less than a beat's worth of events on the queue, sleep for less

            // potentially drop ticks to catch up 
            adj_loop_sleep(adj->bpm, (int) (ADJ_CLOCKS_PER_BEAT * ADJ_BEATS_QUEUED) - events);
        }

    }
    quit:

    // stop midi devices
    midi_stop(adj);

    // free
    //snd_seq_free_queue(adj->alsa_seq, adj->q);
    //snd_seq_close(adj->alsa_seq);

    return ADJ_OK;
}

// start public api

adj_seq_info_t* adj_calloc()
{
    adj_seq_info_t* adj = (adj_seq_info_t*) calloc(1, sizeof(adj_seq_info_t));
    if (adj == NULL) {
        return NULL;
    }

    adj->bpm = 120.0;
    // TODO might be better to check for NULL
    adj->message_handler = nop_message_handler;
    adj->data_change_handler = nop_data_change_handler;
    adj->tick_handler = nop_tick_handler;
    adj->stop_handler = nop_stop_handler;
    adj->start_handler = nop_start_handler;
    return adj;
}

void adj_free(adj_seq_info_t* adj)
{
    free(adj);
}

int adj_init_alsa(adj_seq_info_t* adj)
{
    int rv = ADJ_OK;

    if ( ! adj->seq_name ) adj->seq_name = "adj";

    // Setup Alsa
    int alsa_err;
    if ( ( alsa_err = snd_seq_open(&adj->alsa_seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK)) <  0) {
        return ADJ_ALSA_SEQ_OPEN;
    } else {
        snd_seq_set_client_name(adj->alsa_seq, adj->seq_name);
    }

    adj->alsa_port = snd_seq_create_simple_port(adj->alsa_seq, "clock", SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_APPLICATION);

    if (adj->alsa_port < 0) {
        return ADJ_ALSA_PORT_OPEN;
    }

    // get alsa client_id, and print it because its useful for connect
    snd_seq_client_info_t* info;
    snd_seq_client_info_malloc(&info);

    if (snd_seq_get_client_info(adj->alsa_seq, info) == 0){
        adj->client_id = snd_seq_client_info_get_client(info);
    }
    snd_seq_client_info_free(info);

        // create sequencer queue
    adj->q = snd_seq_alloc_named_queue(adj->alsa_seq, "adjq");
    if (adj->q < 0) {
        rv = ADJ_ALSA_QUEUE_ALLOC;
    }

    adj_alsa_initialised = 1;

    return rv;
}

int adj_init(adj_seq_info_t* adj)
{
    if (! adj_alsa_initialised) return ADJ_RTFM;
    pthread_t thread_id;

    int s = pthread_create(&thread_id, NULL, main_loop, adj);
    if (s != 0) {
        return ADJ_THREAD;
    }

    return ADJ_OK;
}

void adj_nudge(adj_seq_info_t* adj, int multiplier)
{
    adj_nudge_multiplier = multiplier;
    adj_nudge_exec = 1;

    if (multiplier > 10)   adj->data_change_handler(adj, ADJ_ITEM_OP, "  nudge ^");
    if (multiplier > 0)    adj->data_change_handler(adj, ADJ_ITEM_OP, "  nudge >");
    if (multiplier < 0)    adj->data_change_handler(adj, ADJ_ITEM_OP, "< nudge ");
    if (multiplier < -10)  adj->data_change_handler(adj, ADJ_ITEM_OP, "v nudge ");
}

void adj_nudge_millis(adj_seq_info_t* adj, int millis)
{
    adj_nudge_ms = millis;
    adj_nudge_exec = 1;

    if (millis > 0) adj->data_change_handler(adj, ADJ_ITEM_OP, "  nudge >");
    if (millis < 0) adj->data_change_handler(adj, ADJ_ITEM_OP, "< nudge ");
}

void adj_start(adj_seq_info_t* adj)
{
    adj->data_change_handler(adj, ADJ_ITEM_OP, "start");
    // midi start is on the loop
    adj_paused = 0;
}

void adj_stop(adj_seq_info_t* adj)
{
    adj->data_change_handler(adj, ADJ_ITEM_OP, "stop");
    adj_paused = 1;
}

void adj_toggle(adj_seq_info_t* adj)
{
    if (adj_paused) {
        adj_start(adj);
    } else {
        adj_stop(adj);
    }
}

// restart in time to the playing loop (not perfect)
void adj_quantized_restart(adj_seq_info_t* adj)
{
    adj_q_restart = 1;
}


void adj_beat_lock(adj_seq_info_t* adj)
{
    adj_stop(adj);
    pthread_mutex_lock(&adj_sync_mutex);
    usleep(250000);
    adj_start(adj);
}

void adj_beat_unlock(adj_seq_info_t* adj)
{
    pthread_mutex_unlock(&adj_sync_mutex);
}


int adj_exit(adj_seq_info_t* adj)
{
    adj->data_change_handler(adj, ADJ_ITEM_STATE_SEQ, "exit");
    int rv = adj_quit();
    usleep(500000);
    if (adj->exit_handler) adj->exit_handler(adj);
    return rv;
}

int adj_quit()
{
    int rv = 0;
    if (adj_running) {
        rv = 1;
    }
    adj_running = 0;
    return rv;
}

unsigned _Atomic adj_is_running()
{
    return adj_running;
}

unsigned _Atomic adj_is_paused()
{
    return adj_paused;
}

void adj_set_tempo(adj_seq_info_t* adj, float bpm)
{
    adj_set_bpm = bpm;
}

void adj_adjust_tempo(adj_seq_info_t* adj, float bpm_diff)
{
    adj_adjust_bpm = bpm_diff;
}

// end public api

// start util api

//SNIP_utils

unsigned int adj_bpm_to_micros(float bpm)
{
    return (unsigned int) (60000000 / bpm);
}

struct timespec adj_one_beat_time(float bpm)
{
    long int nanos = (long int) (60000000000.0 / bpm);
    struct timespec ts = {0};
    ts.tv_sec = nanos / 1000000000L;
    ts.tv_nsec = nanos % 1000000000L;
    return ts;
}

float adj_micros_to_bpm(unsigned int micros_per_beat)
{
    return 60 / (micros_per_beat / 1000000.0);
}

void adj_one_beat_sleep(float bpm)
{
    struct timespec sl = adj_one_beat_time(bpm);
    nanosleep(&sl, (struct timespec*) NULL);
}

//SNIP_utils

// end util api
