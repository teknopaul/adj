/*
# Alsa DJ sync tools, Midi clock using ALSA sequencers and tick scheduling

## Features:

- configurable BPM
- keeps events on the sequencer queue
- start and stop with space bar
- nudge with arrow keys (i.e. you can use this to beat-mix midi devices)

This is the original app, its been split to be more modular.

@author teknopaul 2021

*/

//SNIP_constants

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>

int VERBOSE              = 0;
int PPQ                  = 96;    // ticks per quarter note
int CLOCKS_PER_BEAT      = 24;    // clock signals required per beat (defined by midi spec)
float BEATS_QUEUED       = 1;     // we queue up clock signals on the sequencer, and so only loop once every bar
int MAX_CLIENT_NAME_LEN  = 2048;

//SNIP_constants

// Global state
static unsigned _Atomic running = ATOMIC_VAR_INIT(0);
static unsigned _Atomic paused = ATOMIC_VAR_INIT(0);
static struct termios term_orig;


static int nudge(int client_id, snd_seq_t* alsa_seq, int alsa_port, int q, float bpm, int multiplier);
static int midi_start(snd_seq_t* alsa_seq, int alsa_port, int q);
static int midi_stop(snd_seq_t* alsa_seq, int alsa_port, int q);

static int setup_alsa(char* display_name, snd_seq_t** alsa_seq, int* alsa_port, int* client_id);
static int send_midi_clock(snd_seq_t* alsa_seq, int alsa_port, int q, snd_seq_tick_time_t when);

static void init_term();
static void keyboard_input(int client_id, snd_seq_t* alsa_seq, int alsa_port, int q, float bpm);
static void* read_keys(void* arg);

static void usage()
{
    printf("options:\n");
    printf("    -b - bpm\n");
    printf("    -p - aconnect clock to port, N.B. whitespace in port names e.g. -p 'TR-6S:TR-6S MIDI 1    '\n");
    printf("    -n - change the name of the clock port in alsa\n");
    printf("    -y - use alsa sync feature\n");
    printf("    -k - keyboard input\n");
    printf("    -h - display this text\n");
    printf("    -v - verbose\n");
    exit(1);
}

static snd_seq_tick_time_t next_tick(snd_seq_tick_time_t prev_tick)
{ 
    return prev_tick + 4;
}

static void signal_exit(int sig)
{
    running = 0;
    tcsetattr(0, TCSANOW, &term_orig);
}

static void set_tempo(snd_seq_t* alsa_seq, int q, float bpm)
{
    snd_seq_queue_tempo_t* tempo;
    snd_seq_queue_tempo_alloca(&tempo);
    snd_seq_queue_tempo_set_tempo(tempo, (unsigned int) (60000000 / bpm)); // microseconds in a minute / bpm = micros per beat
    snd_seq_queue_tempo_set_ppq(tempo, PPQ);
    snd_seq_set_queue_tempo(alsa_seq, q, tempo);
}

/**
 * @returns 4 beats as a time spec
 */
//SNIP_sleep_time
static struct timespec loop_sleep_time(double bpm)
{
    long int nanos = (long int) (BEATS_QUEUED * (60000000000.0 / bpm)); // length of time to sleep, in nanos (4 beats)
    struct timespec req = {0};
    req.tv_sec = nanos / 1000000000L;
    req.tv_nsec = nanos % 1000000000L;
    return req;
}

/**
 * Sleeps for BEATS_QUEUED beats
 * problem here is that code execution takes _some_ time.
 */
static void loop_sleep(double bpm)
{
    struct timespec sl = loop_sleep_time(bpm);
    nanosleep(&sl, (struct timespec*)NULL);
}

static struct timespec one_beat_sleep_time(float bpm)
{
    long int nanos = (long int) (60000000000.0 / bpm); // length of time to sleep, in nanos (1 beats)
    struct timespec req = {0};
    req.tv_sec = nanos / 1000000000L;
    req.tv_nsec = nanos % 1000000000L;
    return req;
}

static void one_beat_sleep(float bpm)
{
    struct timespec sl = one_beat_sleep_time(bpm);
    nanosleep(&sl, (struct timespec*)NULL);
}
//SNIP_sleep_time


//SNIP_nudge_bpm
static float nudge_bpm(float bpm, int multiplier)
{
    float tmp_bpm = bpm + (0.1 * multiplier);
    if ( tmp_bpm < 0 ) {
        tmp_bpm = 0;
    }
    //printf("temporary bpm %f\n", tmp_bpm);
    return tmp_bpm;
}
//SNIP_nudge_bpm




int main(int argc, char* argv[])
{
    int i, b = 1;
    float bpm = 120.0;
    double bpm_double = 120.0;
    char* display_name = NULL;
    char* out_port_name = NULL;
    char alsa_sync = 0, show_bpm = 1, keyb_input = 0;
    snd_seq_tick_time_t tick = 0;

    running = 1;
    tcgetattr(0, &term_orig);

    // parse command line
    if (argc == 1) usage();

    int c;
    while ( ( c = getopt(argc, argv, "b:n:p:hvyk") ) != EOF) {
        switch (c) {
            default:
            case 'h':
                usage();
                break;
            case 'b':
                bpm = strtof(optarg, NULL);
                show_bpm = 0;
                break;
            case 'p':
                out_port_name = optarg;
                break;
            case 'n': 
                display_name = optarg;
                break;
            case 'v': 
                VERBOSE = 1;
                break;
            case 'y': 
                alsa_sync = 1;
                break;
            case 'k': 
                keyb_input = 1;
                break;
        }
    }
    
    if ( ! display_name) display_name = "amidiclock";

    // setup alsa sequencer
    snd_seq_t* alsa_seq = NULL;
    int alsa_port = 0;
    int client_id = 0;
    int ret = setup_alsa(display_name, &alsa_seq, &alsa_port, &client_id);
    if (ret != 0) {
        return ret;
    }

    // wire up midi devices
    if (out_port_name) {
        char cli[2048];
        cli[2047] = '0';
        snprintf(cli, 2047, "aconnect '%s:clock' '%s'", display_name, out_port_name);
        if ( system(cli) ) printf("failed\n");
        if (VERBOSE) {
            printf("%s\n", cli);
            if ( system("aconnect -l") ) printf("\n");
        }
    } else {
        if (VERBOSE) {
            printf("wire up alsa e.g.  aconnect amidiclock:clock 'TR-6S:TR-6S MIDI 1    '\n");
            printf("N.B. use quotes for aconnect parameters if the device name has whitespace\n");
        } else {
            printf("waiting for aconnect\n");
        }
        sleep(5);
    }

    // create sequencer queue
    int q = snd_seq_alloc_named_queue(alsa_seq, "amidiclockqueue");
    if (q < 0) {
        fprintf(stderr, "could not create queue\n");
        return 1;
    }

    // set bpm
    bpm_double = (double)bpm;
    set_tempo(alsa_seq, q, bpm);
    if (show_bpm) {
        printf("bpm %f\n", bpm);
    }

    // KEy board handling
    signal(SIGINT, signal_exit);
    if (keyb_input) {
        init_term();
        keyboard_input(client_id, alsa_seq, alsa_port, q, bpm);
    }

    if (VERBOSE) printf("start\n");

    // start the queue and sent midi start
    midi_start(alsa_seq, alsa_port, q);
    snd_seq_drain_output(alsa_seq);

    // give our loop one beat of butffer time
    for (i = 0 ; i < CLOCKS_PER_BEAT; i++) {
        send_midi_clock(alsa_seq, alsa_port, q, tick);
        tick = next_tick(tick);
    }
    snd_seq_drain_output(alsa_seq);

    snd_seq_queue_status_t* info;
    snd_seq_queue_status_malloc(&info);

    // start the midi clock loop
    int was_paused = 0;
    while (running) {

        while (paused) {
            was_paused = 1;
            struct timespec pause = {0};
            pause.tv_sec = 0L;
            pause.tv_nsec = 50000000L;
            nanosleep(&pause, (struct timespec*)NULL);
        }
        if (was_paused) {
            was_paused = 0;
            tick = 0;
            midi_start(alsa_seq, alsa_port, q);
            if (VERBOSE) printf("go\n");
        }

        for (i = 0 ; i < CLOCKS_PER_BEAT * BEATS_QUEUED; i++) {
            send_midi_clock(alsa_seq, alsa_port, q, tick);
            tick = next_tick(tick);
        }
        snd_seq_drain_output(alsa_seq);

        if (alsa_sync) {
            snd_seq_sync_output_queue(alsa_seq);
        } else {
            // non alsa-sync means we have beats on the queue, but we have to compensate from CPU cycles taking time
            // if we ever have less than a beats worth of events on the queue skip the sleep and bufgfer more events
            snd_seq_get_queue_status(alsa_seq, q, info);
            int events = snd_seq_queue_status_get_events(info);
            if (events > CLOCKS_PER_BEAT) {
                if (VERBOSE) {
                    if (b % 16 == 0) {
                        putc(':', stdout);
                    }
                    else if(b % 4 == 0) {
                        putc(',', stdout);
                    }
                    else {
                        putc('.', stdout);
                    }

                    if (b % 64 == 0) {
                        putc('\n', stdout);
                    }
                    fflush(stdout);
                }
                loop_sleep(bpm_double);
            } else {
                if (VERBOSE) {
                    putc('-', stdout);
                    fflush(stdout);
                }
            }
            b++;
        }
    }

    // stop midi devices
    midi_stop(alsa_seq, alsa_port, q);

    // free
    snd_seq_free_queue(alsa_seq, q);
    snd_seq_close(alsa_seq);

    signal_exit(0);
    return 0;
}

// start midi

static void clear_queue(snd_seq_t* alsa_seq, int q)
{
    snd_seq_remove_events_t* ev;
    snd_seq_remove_events_alloca(&ev);
    snd_seq_remove_events_set_queue(ev, q);
    snd_seq_remove_events_set_condition(ev, SND_SEQ_REMOVE_OUTPUT | SND_SEQ_REMOVE_IGNORE_OFF);
    snd_seq_remove_events(alsa_seq, ev);

}


static int midi_start(snd_seq_t* alsa_seq, int alsa_port, int q)
{
    clear_queue(alsa_seq, q);
    // start the queue, tell midi devices about it
    snd_seq_start_queue(alsa_seq, q, NULL);

    // send the start midi event
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_START;
    snd_seq_ev_set_source(&ev, alsa_port);
    snd_seq_ev_set_subs(&ev);
    
    snd_seq_tick_time_t first_tick = 0;
    snd_seq_ev_schedule_tick(&ev, q, SND_SEQ_TIME_MODE_ABS, first_tick);
    snd_seq_event_output(alsa_seq, &ev);
    snd_seq_drain_output(alsa_seq);

    return 0;
}

static int midi_stop(snd_seq_t* alsa_seq, int alsa_port, int q)
{

    clear_queue(alsa_seq, q);

    // send STOP now/direct (tick = rel 0)
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_STOP;
    snd_seq_ev_set_source(&ev, alsa_port);
    snd_seq_ev_set_subs(&ev);

    snd_seq_ev_schedule_tick(&ev, q, SND_SEQ_TIME_MODE_REL, 0);
    snd_seq_event_output_direct(alsa_seq, &ev);

    snd_seq_stop_queue(alsa_seq, q, NULL);

    return 0;
}

static int send_midi_clock(snd_seq_t* alsa_seq, int alsa_port, int q, snd_seq_tick_time_t when)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_CLOCK;
    snd_seq_ev_set_source(&ev, alsa_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_schedule_tick(&ev, q, SND_SEQ_TIME_MODE_ABS, when);
    snd_seq_event_output(alsa_seq, &ev);
    //snd_seq_drain_output(alsa_seq);
    return 0;
}

/**
 * Nudges the clock forward or back by changing the tempo by a bit then changing it back.
 */
static int nudge(int client_id, snd_seq_t* alsa_seq, int alsa_port, int q, float bpm, int multiplier)
{
    set_tempo(alsa_seq, q, nudge_bpm(bpm, multiplier));
    one_beat_sleep(bpm);
    set_tempo(alsa_seq, q, bpm);

    return 0;
}

static int setup_alsa(char* display_name, snd_seq_t** alsa_seq, int* alsa_port, int* client_id)
{
  
    // Setup Alsa
    int err = 0;

    int alsa_err;
    if ( ( alsa_err = snd_seq_open(alsa_seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK)) <  0) {
        fprintf(stderr, "Could not init alsa sequencer: %s\n", snd_strerror(alsa_err));
        return 1;
    } else {
        snd_seq_set_client_name(*alsa_seq, display_name);
    }

    *alsa_port = snd_seq_create_simple_port(*alsa_seq, "clock",
            SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
            SND_SEQ_PORT_TYPE_APPLICATION);

    if (*alsa_port < 0) {
        fprintf(stderr, "Could not create alsa sequencer port\n");
        return 1;
    }

    if (VERBOSE) printf("Initialised ALSA sequencer: '%s:clock'\n", display_name);

    // get alsa client_id, and print it because its useful for connect
    snd_seq_client_info_t* info;
    snd_seq_client_info_malloc(&info);

    if (snd_seq_get_client_info(*alsa_seq, info) == 0){
        *client_id = snd_seq_client_info_get_client(info);
        if (VERBOSE) printf("client %i:0\n", *client_id);
    } else {
        err = 1;
    }
    snd_seq_client_info_free(info);

    return err;
}

// end midi



// start keybard input

// data needed on this thread
// N.B. therefore must be const

struct thread_info {
    pthread_t   thread_id;
    int         client_id;
    snd_seq_t*  alsa_seq;
    int         alsa_port;
    int         q;
    int         bpm;
};

static void init_term()
{
    struct termios new_term = term_orig;
    new_term.c_lflag &= ~ICANON;         // disable buffered i/o
    new_term.c_lflag &= ~ECHO;           // echo off
    tcsetattr(0, TCSANOW, &new_term);
}


static void keyboard_input(int client_id, snd_seq_t* alsa_seq, int alsa_port, int q, float bpm)
{
    init_term();

    long unsigned int thread_id =0;

    struct thread_info *td = calloc(1, sizeof(struct thread_info));
    if (td == NULL) exit(1);

    td->thread_id = 0;
    td->client_id = client_id;
    td->alsa_seq = alsa_seq;
    td->alsa_port = alsa_port;
    td->q = q;
    td->bpm = bpm;

    int s = pthread_create(&thread_id, NULL, &read_keys, td);
    if (s != 0) {
        exit(1);
    }

}

static void* read_keys(void* arg)
{
    struct thread_info* td = arg;
    char ch;

    while (running) {
        // arrows keys come as Esc[A
        ch = getchar();
        if (ch == '\033') {
            if (getchar() == '[') {
                ch = getchar();
                switch(ch) {
                    case 'C':// right
                        nudge(td->client_id, td->alsa_seq, td->alsa_port, td->q, td->bpm, 10);
                        if (VERBOSE) printf(">");
                        continue;
                    case 'D':// left
                        nudge(td->client_id, td->alsa_seq, td->alsa_port, td->q, td->bpm, -10);
                        if (VERBOSE) printf("<");
                        continue;
                    case 'A': // up
                        nudge(td->client_id, td->alsa_seq, td->alsa_port, td->q, td->bpm, 20);
                        if (VERBOSE) printf("^");
                        continue;
                    case 'B':// down
                        nudge(td->client_id, td->alsa_seq, td->alsa_port, td->q, td->bpm, -20);
                        if (VERBOSE) printf("v");
                        continue;
                    default: 
                        continue;
                }
            }
            continue;
        // space bar or enter
        } else if (ch == ' ' || ch == '\n') {
            if (paused) {
                // start is in the other thread
            } else {
                if (VERBOSE) printf("\nstop\n");
                midi_stop(td->alsa_seq, td->alsa_port, td->q);
            }
            paused = !paused;
        // quit
        } else if (ch == 'q') {
            if (VERBOSE) printf("\nquit\n");
            paused = 0;
            running = 0;
            break;
        } else {
            //printf("%i\n", (int)ch);
        }
        one_beat_sleep(td->bpm * 4);
    }

    return NULL;
}

// end keybard input
