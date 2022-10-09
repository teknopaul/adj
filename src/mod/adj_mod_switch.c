/*
 * # Alsa DJ JoyStick controller
 *
 * Designed for a  Logic3 Faceoff Deluxe+ Audio Wired Controller for Nintendo Switch (0e6f:0184)
 *
 * main axis - sets tempo
 * both left/right and up/down - nudges
 * buttons A,B,X,Y - copy bpm from CDJ player 1 to 4
 *
 * top left triggers - start/stop
 * bottom left trigger - ctrl
 * top right trigger - retriggers on the down beat
 * bottom right trigger - quantized restart
 *
 * home button - cdj sync on
 * round buttonh - cdj sync off
 * + button - cdj difflock plus
 * - buttonh - cdj difflock minux
 *
 * left bottom (ctrl) acts like a control key,  when down
 *   button X exits adj
 *   button 2 pressed 4 times sets bpm (tap)
 *
 *
 * This controller has the cross hair/joypad/d-pad represented as an axis which sends fixed values
 *
 *
 * @author teknopaul 2021
 *
 */

#include <stdatomic.h>
#include <pthread.h>
#include <linux/joystick.h>
#include <linux/input.h>
#include <stdio.h>

#include "../adj_vdj.h"
#include "../adj_bpm_tap.h"
#include "adj_mod_api.h"

#define AXIS_TEMPO_HORIZ     4
#define AXIS_TEMPO_VIRT      5
#define AXIS_NUDGE_HORIZ     0
#define AXIS_NUDGE_VIRT      1
#define AXIS_NUDGE_XS_HORIZ  2
#define AXIS_NUDGE_XS_VIRT   3
#define BUTTON_1             0 // Y
#define BUTTON_2             1 // B
#define BUTTON_3             3 // X
#define BUTTON_4             2 // A
#define BUTTON_SYNC          9
#define BUTTON_UNSYNC        8
#define TRIGGER_START        4
#define TRIGGER_CTRL         6
#define TRIGGER_RETRIGGER    5
#define TRIGGER_RESTART      7
#define BUTTON_DIFF_NUDGE_UP 9
#define BUTTON_DIFF_NUDGE_DOWN 8
#define BUTTON_DIFFLOCK_ON   12
#define BUTTON_DIFFLOCK_OFF  13

static unsigned _Atomic adj_switch_running = ATOMIC_VAR_INIT(0);

static unsigned int js_flags;
static int js_fd = 0;
static int has_vdj = 0;
static int trigger_down = 0;    // stop button pressed down (used like a control key)
static char * device_id = NULL;
static uint8_t player_id = 1;


static int init_switch(char* dev)
{

    if ((js_fd = open(dev, O_RDONLY)) < 0) {
        fprintf(stderr, "could not open '%s'\n", dev);
        return ADJ_ERR;
    }

    char id[256];
    memset(id, 0, sizeof(id));
    if (ioctl(js_fd, JSIOCGNAME(sizeof(id)), id) < 0) {
        device_id = "Unknown";
    }
    else {
        device_id = id;
    }

    fprintf(stderr, "connected %s\n", device_id);
    return ADJ_OK;
}

static void handle_js_event(adj_seq_info_t* adj, struct js_event *evt)
{
    unsigned int type = evt->type & ~JS_EVENT_INIT;
    if (type == JS_EVENT_AXIS) {
        // fprintf(stderr, "axis event, number=%d val=%d\n", evt->number, evt->value);
        if (evt->value == 0) return; // key off

        switch (evt->number) {
            // 0 and 1 send +/-32767 only
            case AXIS_TEMPO_HORIZ:
                adj_adjust_tempo(adj, evt->value > 0 ? 0.1 : -0.1);
                break;
            case AXIS_TEMPO_VIRT:
                adj_adjust_tempo(adj, evt->value < 0 ? 1.0 : -1.0);
                break;
            // axis send a number from =/- 1 to +/-32767
            // its easy to accidentally fire up and down (axis 3) with left rigth (2), 
            case AXIS_NUDGE_XS_HORIZ:
                if (evt->value > 30000) adj_adjust_tempo(adj, 0.01);
                if (evt->value < -30000) adj_adjust_tempo(adj, -0.01);
                break;
            case AXIS_NUDGE_XS_VIRT:
                if (evt->value < -30000) {
                    adj_adjust_tempo(adj, 0.01);
                    adj_nudge(adj, 10);
                }
                if (evt->value > 30000) {
                    adj_adjust_tempo(adj, -0.01);
                    adj_nudge(adj, -10);
                }
                break;
            case AXIS_NUDGE_HORIZ:
                adj_nudge(adj, evt->value / 6400);
                break;
            case AXIS_NUDGE_VIRT:
                adj_nudge(adj, -1 * evt->value / 3276);
                break;
        }

    }
    else if (type == JS_EVENT_BUTTON) {
        // fprintf(stderr, "button event, number=%d value=%d\n", evt->number, evt->value);
        if (evt->value == 0) {
            if (evt->number == 6) {
                trigger_down = 0;
                adj_bpm_tap_reset();
            }
            return; // key off
        }
        switch (evt->number) {
            case BUTTON_1:
                if (trigger_down) {
                    adj_exit(adj);
                }
                else {
                    if (has_vdj) {
                        adj_vdj_copy_bpm(adj, player_id = 1);
                        adj_vdj_track_start(adj, player_id);
                    }
                }
                break;
            case BUTTON_2:
                if (trigger_down) {
                    adj_bpm_tap(adj);
                }
                else {
                    if (has_vdj) {
                        adj_vdj_copy_bpm(adj, player_id = 2);
                        adj_vdj_track_start(adj, player_id);
                    }
                }
                break;
            case BUTTON_3:
                if (has_vdj) {
                    adj_vdj_copy_bpm(adj, player_id = 3);
                    adj_vdj_track_start(adj, player_id);
                }
                break;
            case BUTTON_4:
                if (trigger_down) {
                    adj_exit(adj);
                }
                else {
                    if (has_vdj) {
                        adj_vdj_copy_bpm(adj, player_id = 4);
                        adj_vdj_track_start(adj, player_id);
                    }
                }
                break;
            case TRIGGER_START:
                adj_toggle(adj);
                break;
            case TRIGGER_RETRIGGER:
                if (has_vdj) adj_vdj_trigger_from_player(adj, player_id);
                break;
            case TRIGGER_CTRL:
                trigger_down = 1;
                break;
            case TRIGGER_RESTART:
                adj_quantized_restart(adj);
                break;
            case BUTTON_DIFF_NUDGE_DOWN:
                if (has_vdj) adj_vdj_difflock_nudge(adj, -1);
                break;
            case BUTTON_DIFF_NUDGE_UP:
                if (has_vdj) adj_vdj_difflock_nudge(adj, 1);
                break;
            case BUTTON_DIFFLOCK_ON:
                if (has_vdj) adj_vdj_difflock(adj, player_id, 0);
                break;
            case BUTTON_DIFFLOCK_OFF:
                if (has_vdj) adj_vdj_difflock_arff(adj);
                break;
        }
    }
    else if (type == JS_EVENT_INIT) {

    }
}

/**
 * Thread that reads events from the joystick
 */
static void* read_js_events(void* arg)
{
    adj_seq_info_t* adj = arg;
    ssize_t sz = sizeof(struct js_event);
    struct js_event evt;
    while (adj_switch_running) {
        if (read(js_fd, &evt, sz) == sz) {
            handle_js_event(adj, &evt);
        }
    }
    close(js_fd);

    return NULL;
}

/**
 * init joystick / gamepad input
 */
int adj_mod_init(adj_seq_info_t* adj, char* args, unsigned int flags)
{
    int rv;
    char* dev = "/dev/input/js0";
    js_flags = flags;
    if (js_flags & ADJ_HAS_VDJ) has_vdj = 1;

    pthread_t thread_id;
    if (args && strncmp(args, "/dev/input/", 11) == 0 ) {
        dev = args;
    }

    if ((rv = init_switch(dev)) != ADJ_OK) {
        return rv;
    }
    adj_switch_running = 1;

    int s = pthread_create(&thread_id, NULL, &read_js_events, adj);
    if (s != 0) {
        close(js_fd);
        return ADJ_THREAD;
    }

    return ADJ_OK;
}


void adj_mod_exit()
{
    adj_switch_running = 0;
}
