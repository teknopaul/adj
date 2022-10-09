
#include <pthread.h>
#include <stdatomic.h>

#include "adj_midiin.h"

// Global state
static unsigned _Atomic adj_midiin_running = ATOMIC_VAR_INIT(1);


// slider magic, use two buttons and any volume type control as a pitch adjust slider

static unsigned int slider_on = 0;
static int slider_value = -1;

static void do_slider(adj_seq_info_t *adj, int value)
{
    if (slider_value == -1) {
        slider_value = value;
        return;
    }
    if (value - slider_value) {
        adj_adjust_tempo(adj, (value - slider_value) * 0.1);
        slider_value = value;
    }
}

/**
 * MIDI in module for receiving cue, nudges and tempo from a midi controller
 */

struct mm_thread_info {
    adj_seq_info_t* adj;
    adj_midiin_map* map;
};

//SNIP_midiin_parser

static int range_check(const char* item, int line_no, long int num, char* endptr, long int min, long int max)
{
    // strtol wierdness
    if (num == 0 && *endptr != '\0') {
        fprintf(stderr, "syntax error line:%i NaN %s: '%s'\n", line_no, item, endptr);
        return ADJ_SYNTAX;
    }
    if (num < min) {
        fprintf(stderr, "syntax error line:%i invalid %s: '%li'\n", line_no, item, num);
        return ADJ_SYNTAX;
    }
    if (num > max) {
        fprintf(stderr, "syntax error line:%i invalid %s: '%li'\n", line_no, item, num);
        return ADJ_SYNTAX;
    }

    return ADJ_OK;
}

/**
 * parse a line in the midimap file validating it
 ()
 * # name      ch ctrl  val
 * start       0  80    127
 */
static int parse_line(char* line, int line_no, adj_midiin_map* map) 
{
    if (line[0] == '#') return ADJ_OK;
    if (strlen(line) == 0) return ADJ_OK;
    if (strcmp("\n", line) == 0) return ADJ_OK;

    int idx;
    char* endptr = "";
    long int ch, ctl, val;

    char* name = strtok(line, " ");
    char* channel = strtok(NULL, " ");
    char* controller = strtok(NULL, " ");
    char* value = strtok(NULL, " ");

    if (!value) {
        fprintf(stderr, "syntax error, expect 4 entries line:%i\n", line_no);
        return ADJ_SYNTAX;
    }

         if ( strcmp("start", name) == 0 )      idx = ADJ_MIDIIN_START - 1;
    else if ( strcmp("stop", name) == 0 )       idx = ADJ_MIDIIN_STOP - 1;
    else if ( strcmp("toggle", name) == 0 )     idx = ADJ_MIDIIN_TOGGLE - 1;
    else if ( strcmp("q_restart", name) == 0 )  idx = ADJ_MIDIIN_Q_RESTART - 1;
    else if ( strcmp("nudge_fwd", name) == 0 )  idx = ADJ_MIDIIN_NUDGE_FWD - 1;
    else if ( strcmp("nudge_ffwd", name) == 0 ) idx = ADJ_MIDIIN_NUDGE_FFWD - 1;
    else if ( strcmp("nudge_rew", name) == 0 )  idx = ADJ_MIDIIN_NUDGE_REW - 1;
    else if ( strcmp("nudge_frew", name) == 0 ) idx = ADJ_MIDIIN_NUDGE_FREW - 1;
    else if ( strcmp("tempo_inc_min", name) == 0 ) idx = ADJ_MIDIIN_TEMPO_INC_MIN - 1;
    else if ( strcmp("tempo_inc", name) == 0 )     idx = ADJ_MIDIIN_TEMPO_INC - 1;
    else if ( strcmp("tempo_inc_1", name) == 0 )   idx = ADJ_MIDIIN_TEMPO_INC_1 - 1;
    else if ( strcmp("tempo_dec_min", name) == 0 ) idx = ADJ_MIDIIN_TEMPO_DEC_MIN - 1;
    else if ( strcmp("tempo_dec", name) == 0 )     idx = ADJ_MIDIIN_TEMPO_DEC - 1;
    else if ( strcmp("tempo_dec_1", name) == 0 )   idx = ADJ_MIDIIN_TEMPO_DEC_1 - 1;


    else if ( strcmp("slider", name) == 0 )      idx = ADJ_MIDIIN_SLIDER - 1;
    else if ( strcmp("slider_on", name) == 0 )   idx = ADJ_MIDIIN_SLIDER_ON - 1;
    else if ( strcmp("slider_off", name) == 0 )  idx = ADJ_MIDIIN_SLIDER_OFF - 1;

    else {
        fprintf(stderr, "syntax error line:%i invalid name: '%s'\n", line_no, name);
        return ADJ_SYNTAX;
    }

    // channel
    if (strncmp("*", channel, 1) == 0) {
        map->op[idx].channel = ADJ_ANY_CHANNNEL;
    }
    else {
        ch = strtol(channel, &endptr, 10);
        if ( range_check("midi channel", line_no, ch, endptr, 0, 16) ) {
            return ADJ_SYNTAX;
        }
        map->op[idx].channel = (unsigned char) ch;
    }

    // controller
    ctl = strtol(controller, &endptr, 10);
    if ( range_check("controller", line_no, ctl, endptr, 0, 127) ) {
        return ADJ_SYNTAX;
    }
    map->op[idx].controller = (unsigned char) ctl;

    // value
    if (strncmp("*", value, 1) == 0) {
        map->op[idx].value = ADJ_ANY_VALUE;
    }
    else {
        val = strtol(value, &endptr, 10);
        if ( range_check("ctrl value", line_no, val, endptr, 0, 127) ) {
            return ADJ_SYNTAX;
        }
        map->op[idx].value = (unsigned char) val;
    }

    return ADJ_OK;
}

/**
 * read a midi mapper file, e.g. /etc/adj-midimap.adjmm
 */
static adj_midiin_map* read_map(adj_seq_info_t *adj, const char* path)
{
    int i = 0;

    adj_midiin_map* map = calloc(1, sizeof(adj_midiin_map));
    for ( i = 0; i < ADJ_MAX_OP; i++ ) {
        map->op[i].channel = ADJ_UNSET_VALUE;    // 128 == impossible channel value
        map->op[i].controller = ADJ_UNSET_VALUE; // 128 == impossible controller value
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        adj->message_handler(adj, "error: unable to read adjmm file");
        return NULL;
    }

    for (i = 1; (read = getline(&line, &len, fp)) != -1 ; i++) {
        if (read) {
            parse_line(line, i, map);
        }
    }

    free(line);
    fclose(fp);

    /*
    for ( i = 0; i < ADJ_MAX_OP; i++ ) {
        if (map->op[i].controller == ADJ_UNSET_VALUE) {
            adj->message_handler(adj, "warn: midi map is missing an operation");
        }
    }
    */

    return map;
}

//SNIP_midiin_parser

/**
 * match a read midi control event to an adj operation
 */
static int match_midi(adj_midiin_map* map, snd_seq_event_t* ev)
{
    int i;
    snd_seq_ev_ctrl_t ctrl = ev->data.control;

    for ( i = 0; i < ADJ_MAX_OP; i++ ) {
        if (
            (map->op[i].channel == ADJ_ANY_CHANNNEL || map->op[i].channel == ctrl.channel) &&
            map->op[i].controller == ctrl.param &&
            ( map->op[i].value == ADJ_ANY_VALUE || map->op[i].value == ctrl.value)
            ) {
            return i + 1;
        }
    }

    return 0;
}

static void* read_midiin(void* arg)
{
    struct mm_thread_info* tinfo = arg;
    adj_seq_info_t* adj = tinfo->adj;
    adj_midiin_map* map = tinfo->map;

    //int unread = 0;

    // set blocking mode, snd_seq_event_input() returns if there is an event,or timeout
    snd_seq_nonblock(adj->alsa_seq, 0);

    snd_seq_event_t* ev = NULL;

    while (adj_midiin_running) {
        
        //unread = snd_seq_event_input_pending(adj->alsa_seq, 1);
        //while (unread && snd_seq_event_input_pending(adj->alsa_seq, 0) > 0) {

            if ( snd_seq_event_input(adj->alsa_seq, &ev) < 0 ) continue;

            if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
                switch (match_midi(map, ev)) {
                    case 0: {
                        // printf("unused midi in message\n");
                        continue;
                    }
                    case ADJ_MIDIIN_START: {
                        adj_start(adj);
                        continue;
                    }
                    case ADJ_MIDIIN_STOP: {
                        adj_stop(adj);
                        continue;
                    }
                    case ADJ_MIDIIN_TOGGLE: {
                        adj_toggle(adj);
                        continue;
                    }
                    case ADJ_MIDIIN_Q_RESTART: {
                        adj_quantized_restart(adj);
                        continue;
                    }

                    case ADJ_MIDIIN_NUDGE_FWD: {
                        adj_nudge(adj, 10);
                        continue;
                    }
                    case ADJ_MIDIIN_NUDGE_FFWD: {
                        adj_nudge(adj, 20);
                        continue;
                    }
                    case ADJ_MIDIIN_NUDGE_REW: {
                        adj_nudge(adj, -10);
                        continue;
                    }
                    case ADJ_MIDIIN_NUDGE_FREW: {
                        adj_nudge(adj, -20);
                        continue;
                    }

                    case ADJ_MIDIIN_TEMPO_INC_MIN: {
                        adj_adjust_tempo(adj, 0.01);
                        continue;
                    }
                    case ADJ_MIDIIN_TEMPO_INC: {
                        adj_adjust_tempo(adj, 0.1);
                        continue;
                    }
                    case ADJ_MIDIIN_TEMPO_INC_1: {
                        adj_adjust_tempo(adj, 1.0);
                        continue;
                    }
                    case ADJ_MIDIIN_TEMPO_DEC_MIN: {
                        adj_adjust_tempo(adj, -0.01);
                        continue;
                    }
                    case ADJ_MIDIIN_TEMPO_DEC: {
                        adj_adjust_tempo(adj, -0.1);
                        continue;
                    }
                    case ADJ_MIDIIN_TEMPO_DEC_1: {
                        adj_adjust_tempo(adj, -1.0);
                        continue;
                    }

                    case ADJ_MIDIIN_SLIDER_ON: {
                        adj->data_change_handler(adj, ADJ_ITEM_OP, "slide on");
                        slider_on = 1;
                        slider_value = -1;
                        continue;
                    }
                    case ADJ_MIDIIN_SLIDER_OFF: {
                        adj->data_change_handler(adj, ADJ_ITEM_OP, "slide off");
                        slider_on = 0;
                        slider_value = -1;
                        continue;
                    }
                    case ADJ_MIDIIN_SLIDER: {
                        if (slider_on) {
                            do_slider(adj, ev->data.control.value);
                        } else {
                            adj->message_handler(adj, "slider is off");
                        }
                        continue;
                    }
                    // default, is sleep
                }
            }
        //}
        // thus stop resolution is 1/4 beat
        //adj_one_beat_sleep(adj->bpm * 4);
    }

    return NULL;
}


int adj_midiin(adj_seq_info_t *adj)
{
    pthread_t thread_id;

    adj_midiin_map* map = read_map(adj, "/etc/adj-midimap.adjmm");
    if (map == NULL) {
        return ADJ_SYNTAX;
    }

    struct mm_thread_info *tinfo = calloc(1, sizeof(struct mm_thread_info));
    if (tinfo == NULL) {
        return ADJ_ALLOC;
    }

    tinfo->adj = adj;
    tinfo->map = map;

    int s = pthread_create(&thread_id, NULL, &read_midiin, tinfo);
    if (s != 0) {
        return ADJ_THREAD;
    }

    return ADJ_OK;
}

void adj_midiin_exit()
{
    adj_midiin_running = 0;
}
