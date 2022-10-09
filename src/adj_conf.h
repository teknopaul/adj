#ifndef _ADJ_CONF_INCLUDED_
#define _ADJ_CONF_INCLUDED_

typedef struct adj_conf_s adj_conf;

struct adj_conf_s {
    char*       alsa_name;
    uint8_t     alsa_sync;
    uint8_t     keyb_in;
    uint8_t     numpad_in;
    uint8_t     joystick_in;
    uint8_t     scan_usb_in;
    char*       midi_in;
    char*       midi_out;
    float       bpm;
    uint8_t     vdj;
    char*       vdj_iface;
    uint8_t     vdj_player;
    int32_t     vdj_offset;
};

adj_conf* adj_conf_init();
adj_conf* adj_conf_init_file(char* file_name);

#endif // _ADJ_CONF_INCLUDED_