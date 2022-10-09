/*
 * # Alsa DJ JoyStick controller
 *
 * Designed for a  Sony Corp. Batoh Device / PlayStation 3 Controller (054c:0268)
 *
 * main axis - sets tempo
 * left axis left/right and up/down - nudges
 * right axis left/right - micro tempo
 * right axis up/down - micro nudges + micro tempo
 * buttons 1 - 4 - copy bpm from CDJ player 1 to 4
 *
 * top left triggers - start(toggle)
 * bottom left trigger - ctrl
 * top right trigger - retriggers on the down beat
 * bottom right - quantized restart
 *
 * left bottom acts like a control key,  when down
 *   button 1 exits adj
 *   button 2 four times sets bpm (tap)
 *   button 3 saves bpm
 *
 *
 * This controller has the cross hair/joypad/d-pad represented as button presses.
 * The values from the second analog sticks vertical axis are inverted  (-ve up, +ve down)
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
#include <libusb-1.0/libusb.h>

#include "../adj_vdj.h"
#include "../adj_store.h"
#include "../adj_bpm_tap.h"
#include "adj_mod_api.h"


#define BUTTON_TEMPO_RIGHT   16
#define BUTTON_TEMPO_UP      13
#define BUTTON_TEMPO_LEFT    15
#define BUTTON_TEMPO_DOWN    14
#define AXIS_NUDGE_HORIZ     0
#define AXIS_NUDGE_VIRT      1
#define AXIS_NUDGE_XS_HORIZ  3
#define AXIS_NUDGE_XS_VIRT   4
#define BUTTON_1             3 // □
#define BUTTON_2             0 // X
#define BUTTON_3             1 // Δ
#define BUTTON_4             2 // O
#define BUTTON_SYNC          9
#define BUTTON_UNSYNC        8
#define TRIGGER_START        4
#define TRIGGER_CTRL         6
#define TRIGGER_RETRIGGER    5
#define TRIGGER_RESTART      7
#define BUTTON_P3            10

static unsigned _Atomic adj_ps3_running = ATOMIC_VAR_INIT(0);

static unsigned int js_flags;
static int js_fd = 0;
static int has_vdj = 0;
static int trigger_down = 0;    // left bottom trigger held down (used like a control key)
static char * device_id = NULL;
static uint8_t player_id = 1;

#define VENDOR      0x054c
#define PRODUCT     0x0268

#define USB_DIR_IN  0x80
#define USB_DIR_OUT 0

// from sixpair.c
static int set_master(libusb_device_handle *devh, int iface, int mac[6])
{
    unsigned char msg[8]= { 0x01, 0x00, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };
    return libusb_control_transfer(devh,
                 LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                 0x09,
                 0x03f5, iface, msg, sizeof(msg),
                 5000);
}

static int process_device(struct libusb_device *dev, struct libusb_device_descriptor *desc, int iface) {
    int mac[6];

    FILE *f = popen("hcitool dev", "r");
    if ( !f || fscanf(f, "%*s\n%*s %x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6 ) {
        fprintf(stderr, "hcitool dev failed\n");
        return ADJ_ERR;
    }
    pclose(f);

    libusb_device_handle *devh = NULL;
    libusb_open(dev, &devh);
    if ( ! devh ) {
        fprintf(stderr, "libusb_open failed\n");
        return ADJ_ERR;
    }

    libusb_detach_kernel_driver(devh, iface);

    int res = libusb_claim_interface(devh, iface);
    if ( res < 0 ) fprintf(stderr, "usb_claim_interface");

    set_master(devh, iface, mac);

    sleep(30);

    libusb_release_interface(devh, iface);

    libusb_attach_kernel_driver(devh, iface);

    libusb_close(devh);

    return ADJ_OK;
}

// while connected over USB ps3 controllers can be paired for later use with bluetooth
static int pair_bluetooth()
{
    int d = 0, i = 0, c = 0, a = 0, rv = 0;
    libusb_device **devs;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config;
    struct libusb_context *ctx;

    rv = libusb_init(&ctx);
    if (rv < 0) {
        return ADJ_ERR;
    }
    rv = libusb_get_device_list(ctx, &devs);
    if (rv < 0) {
        return ADJ_ERR;
    }

    while ((dev = devs[d++]) != NULL) {
        rv = libusb_get_device_descriptor(dev, &desc);
        if (rv < 0) {
            return ADJ_ERR;
        }
        if (desc.idVendor == VENDOR && desc.idProduct == PRODUCT) {
            for (c = 0; c < desc.bNumConfigurations; c++) {

                rv = libusb_get_config_descriptor(dev, c, &config);
                if (LIBUSB_SUCCESS != rv) {
                    continue;
                }

                for (i = 0; i < config->bNumInterfaces; i++) {
                    for (a = 0; a < config->interface[i].num_altsetting; a++) {
                        if (config->interface[i].altsetting[a].bInterfaceClass == 3) {
                            process_device(dev, &desc, config->interface[i].altsetting[a].bInterfaceNumber);
                        }
                    }
                }
                libusb_free_config_descriptor(config);
            }
        }
    }
    libusb_exit(ctx);

    return 0;
}


static int init_logi(char* dev)
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

    //fprintf(stderr, "connected %s\n", device_id);
    return ADJ_OK;
}

static void handle_js_event(adj_seq_info_t* adj, struct js_event *evt)
{
    unsigned int type = evt->type & ~JS_EVENT_INIT;
    if (type == JS_EVENT_AXIS) {
        // fprintf(stderr, "axis event, number=%d val=%d\n", evt->number, evt->value);
        if (evt->value == 0) return; // key off
        switch (evt->number) {
            // all axis send a number from =/- 1 to +/-32767
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
                if (evt->value > 30000) adj_nudge(adj, evt->value / 3276);  // +/- 10
                if (evt->value < -30000) adj_nudge(adj, evt->value / 3276);  // +/- 10
                break;
            case AXIS_NUDGE_VIRT:
                // this axis is upside down
                if (evt->value > 30000) adj_nudge(adj, -1 * evt->value / 1638);  // +/- 20
                if (evt->value < -30000) adj_nudge(adj, -1 * evt->value / 1638);  // +/- 20
                break;
        }

    }
    else if (type == JS_EVENT_BUTTON) {
        // key off
        if (evt->value == 0) {
            if (evt->number == TRIGGER_CTRL) {
                trigger_down = 0;
                adj_bpm_tap_reset();
            }
            return;
        }
        switch (evt->number) {
            // PS3 pad sends button presses for the cross axis not an axis event
            case BUTTON_TEMPO_RIGHT:
                adj_adjust_tempo(adj, 0.1);
                break;
            case BUTTON_TEMPO_UP:
                adj_adjust_tempo(adj, 1.0);
                break;
            case BUTTON_TEMPO_LEFT:
                adj_adjust_tempo(adj, -0.1);
                break;
            case BUTTON_TEMPO_DOWN:
                adj_adjust_tempo(adj, -1.0);
                break;

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
                if (trigger_down) {
                    adj_save_bpm(adj);
                }
                else {
                    if (has_vdj) {
                        adj_vdj_copy_bpm(adj, player_id = 3);
                        adj_vdj_track_start(adj, player_id);
                    }
                }
                break;
            case BUTTON_4:
                if (has_vdj) {
                    adj_vdj_copy_bpm(adj, player_id = 4);
                    adj_vdj_track_start(adj, player_id);
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
            case BUTTON_SYNC:
                if (has_vdj) adj_vdj_difflock(adj, player_id, 0);
                break;
            case BUTTON_UNSYNC:
                if (has_vdj) adj_vdj_difflock_arff(adj);
                break;
            case BUTTON_P3:
                if (trigger_down) {
                    adj_bpm_tap_reset();
                }
                adj_bpm_tap(adj);
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
    while (adj_ps3_running) {
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

    if (js_flags & ADJ_PAIR_BLUETOOTH) pair_bluetooth();

    pthread_t thread_id;
    if ( args == NULL ) {
        dev = "/dev/input/js0";
    }
    else if ( strstr(args, "pair_bluetooth") ) {
        pair_bluetooth();
    }
    else if ( strncmp(args, "/dev/input/", 11) == 0 ) {
        dev = args;
    }

    if ((rv = init_logi(dev)) != ADJ_OK) {
        return rv;
    }
    adj_ps3_running = 1;

    int s = pthread_create(&thread_id, NULL, &read_js_events, adj);
    if (s != 0) {
        close(js_fd);
        return ADJ_THREAD;
    }

    return ADJ_OK;
}


void adj_mod_exit()
{
    adj_ps3_running = 0;
}
