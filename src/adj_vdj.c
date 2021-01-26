
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdatomic.h>

#include <cdj/cdj.h>
#include <cdj/vdj.h>
#include <cdj/vdj_net.h>
#include <cdj/vdj_discovery.h>

#include "adj.h"
#include "adj_vdj.h"
#include "tui.h"

static void adj_vdj_lock_on(vdj_t* v);
static void adj_vdj_lock_off(vdj_t* v);
static void adj_vdj_beat_hook(vdj_t* v, unsigned char player_id);

static unsigned _Atomic adj_trigger_from = ATOMIC_VAR_INIT(0);
static unsigned _Atomic adj_lock_on = ATOMIC_VAR_INIT(0);

//SNIP_adj_vdj_tui

#define MAX_PLAYERS     4
#define BACKLINE_Y      15

#define Y_MDL      4
#define Y_PLR      3
#define Y_BPM      2
#define Y_DIF      1

static void render_slot(int id);

static int rendered_self = 0;

// players with player_id > 4 e.g. rekordbox get mapped to a lower id so they fit on screen.
unsigned char next_slot = 5;
unsigned char high_slots[VDJ_MAX_BACKLINE];
// keep track of time differences between our midi sequencer and the CDJs.
// not real time, we keep diffs and this encorporates network and player latency
int32_t diffs[VDJ_MAX_BACKLINE];

// difflock aka auto-cync, this holds the player_id we should lock to
static unsigned _Atomic difflock_player = ATOMIC_VAR_INIT(0);
// this hold the milliseconds difference we want to maintain
int32_t difflock_ms = 0;

static unsigned char
get_slot(unsigned char player_id)
{
    if (player_id > MAX_PLAYERS) {
        if ( ! high_slots[player_id] ){
            high_slots[player_id] = next_slot++;
            render_slot(high_slots[player_id]);
        }
        return high_slots[player_id];
    } else {
        return player_id;
    }
}

static int
slot_x(int id)
{
    return 10 + (15 * (id - 1));
}

static void 
render_slot(int id)
{
    if (tui) {
        tui_text_at("[            ]", slot_x(id), BACKLINE_Y + Y_MDL);
        tui_text_at("[            ]", slot_x(id), BACKLINE_Y + Y_PLR);
        tui_text_at("[            ]", slot_x(id), BACKLINE_Y + Y_BPM);
        tui_text_at("[            ]", slot_x(id), BACKLINE_Y + Y_DIF);
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + 2);
        printf("%s%02i%s", TUI_GREY, id, TUI_NORMAL);
    }
}

static void
render_model(int id, char* name, int myself)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + Y_MDL);
        if (myself) {
            printf("%s%s%s", TUI_YELLOW, name, TUI_NORMAL);
        } else {
            printf("%s%s%s", TUI_BOLD, name, TUI_NORMAL);
        }
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + Y_PLR);
        printf("%s%02i%s", TUI_BOLD, id, TUI_NORMAL);
    }
}

static void
render_status(int id, char* status)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 4, BACKLINE_Y + Y_PLR);
        fputs(status, stdout);
    }
}

static void
render_bpm(int id, float bpm)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + Y_BPM);
        printf("%06.2f", bpm);
        fflush(stdout);
    }
}

// render CDJs version of master
static void
render_master(int id)
{
    if (tui) {
        tui_set_cursor_pos(11, BACKLINE_Y);
        printf("%s%02i%s", TUI_RED, id, TUI_NORMAL);
        fflush(stdout);
    }
}

// render which deck midi is locked to
static void
render_lock(int id, int32_t diff)
{
    if (tui) {
        tui_set_cursor_pos(15, BACKLINE_Y);
        if (id) {
            printf("[%s%02i%s] [%+04i]", TUI_YELLOW, id, TUI_NORMAL, diff);
        } else {
            printf("[--] [----]");
        }
        fflush(stdout);
    }
}

static void
render_lock_amount(int32_t amount)
{
    if (tui) {
        tui_set_cursor_pos(20, BACKLINE_Y);
        printf("[%+04i]", amount);
        fflush(stdout);
    }
}

static void
render_bar_pos(unsigned char id, unsigned char bar_pos)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 8 + 1, BACKLINE_Y + Y_BPM);
        printf("____");
        tui_set_cursor_pos(slot_x(id) + 8 + bar_pos, BACKLINE_Y + Y_BPM);
        printf("♪");
        //printf("%i", bar_pos);
        fflush(stdout);
    }
}

static void
render_diff(int id, int64_t diff)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + Y_DIF);
        printf("%+04li", diff);
        fflush(stdout);
    }
}

static void
render_backline()
{
    if (tui) {
        int p;
        tui_text_at("model:",  2, BACKLINE_Y + Y_MDL);
        tui_text_at("player:", 2, BACKLINE_Y + Y_PLR);
        tui_text_at("bpm:",    2, BACKLINE_Y + Y_BPM);
        tui_text_at("diff:",    2, BACKLINE_Y + Y_DIF);
        tui_text_at("master: [  ] [  ] [    ]", 2, BACKLINE_Y + 0);
        for (p = 1; p <= MAX_PLAYERS; p++) {
            render_slot(p);
        }
        fflush(stdout);
    }
}

//SNIP_adj_vdj_tui

static void
adj_discovery_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    cdj_discovery_packet_t* d_pkt;
    if (cdj_packet_type(packet, len) == CDJ_KEEP_ALIVE) {
        d_pkt = cdj_new_discovery_packet(packet, len);
        if ( d_pkt ) {
            if (d_pkt->player_id && v->backline->link_members[d_pkt->player_id]) {
                char* model = cdj_model_name(packet, len, CDJ_DISCOVERY_PORT);
                if (model) {
                    tui_lock();
                    render_model(get_slot(d_pkt->player_id), model, 0);
                    tui_unlock();
                    free(model);
                }

            }
            if (d_pkt->player_id == v->player_id) {
                if (!rendered_self) {
                    tui_lock();
                    render_model(get_slot(d_pkt->player_id), "Alsa VDJ", 1); // even if pretending to be a CDJ or XDJ
                    tui_unlock();
                    rendered_self = 1;
                }
            }
        }

        if (d_pkt) free(d_pkt);
    }
}

static void
adj_update_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    unsigned char slot;
    vdj_link_member_t* m;
    unsigned char flags;
    cdj_cdj_status_packet_t* cs_pkt = cdj_new_cdj_status_packet(packet, len);
    if ( cs_pkt ) {
        if ( cs_pkt->player_id ) {
            if ( (m = v->backline->link_members[cs_pkt->player_id]) ) {
                flags = cdj_status_flags(cs_pkt);
                char* status = cdj_flags_to_emoji(flags);
                tui_lock();
                slot = get_slot(cs_pkt->player_id);
                render_bpm(slot, m->bpm);
                if (flags & CDJ_STAT_FLAG_MASTER) {
                    render_master(cs_pkt->player_id);
                }
                render_status(slot, status);
                tui_unlock();
                free(status);
            }
        }
        free(cs_pkt);
    }
}

static void
adj_broadcast_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    unsigned char slot;
    int64_t diff;
    vdj_link_member_t* m;

    if (adj_trigger_from > 0) {
        adj_vdj_lock_on(v);
    }

    cdj_beat_packet_t* b_pkt = cdj_new_beat_packet(packet, len);
    if ( b_pkt ) {
        if ( b_pkt->player_id ) {
            if ( (m = v->backline->link_members[b_pkt->player_id]) ) {
                adj_vdj_beat_hook(v, b_pkt->player_id);
                diff = vdj_time_diff(v, m);
                tui_lock();
                slot = get_slot(b_pkt->player_id);
                render_bpm(slot, b_pkt->bpm);
                render_bar_pos(slot, b_pkt->bar_pos);
                // if you are behind, render on your beat, (if you are ahead render on our beat)
                if (diff > 0) {
                    render_diff(slot, diff);
                    diffs[b_pkt->player_id] = diff;
                }
                tui_unlock();

                // trigger from OR beat lock not both
                if (adj_trigger_from) {
                    if (adj_trigger_from == b_pkt->player_id && b_pkt->bar_pos == 1) {
                        adj_vdj_lock_off(v);
                    }
                } else if (difflock_player == b_pkt->player_id && b_pkt->bar_pos == 4) {
                    if (diff - difflock_ms) {
                        adj_nudge_millis((adj_seq_info_t*) v->client, diff - difflock_ms);
                    }
                }
            }
        }
        free(b_pkt);
    }

}

/**
 * Virtual CDJ application entry point.
 */
vdj_t*
adj_init_vdj(adj_seq_info_t* adj, char* iface, uint32_t flags, float bpm)
{

    memset(high_slots, 0, VDJ_MAX_BACKLINE);
    memset(diffs, 0, MAX_PLAYERS);

    /**
     * init the vdj
     */
    vdj_t* v = vdj_init_iface(iface, flags);
    if (v == NULL) {
        fprintf(stderr, "error: creating virtual cdj\n");
        return NULL;
    }
    if (bpm > 1.0) v->bpm = bpm;

    tui_lock();
    render_backline();
    tui_unlock();

    if (vdj_open_sockets(v) != CDJ_OK) {
        fprintf(stderr, "error: failed to open sockets\n");
        vdj_destroy(v);
        return NULL;
    }

    if (vdj_exec_discovery(v) != CDJ_OK) {
        fprintf(stderr, "error: cdj initialization\n");
        vdj_destroy(v);
        return NULL;
    }

    if ( vdj_init_managed_discovery_thread(v, adj_discovery_handler) != CDJ_OK ) {
        fprintf(stderr, "error: init managed discovery thread\n");
        sleep(1);
        vdj_destroy(v);
        return NULL;
    }

    if ( vdj_init_managed_update_thread(v, adj_update_handler) != CDJ_OK ) {
        fprintf(stderr, "error: init managed status thread\n");
        sleep(1);
        vdj_destroy(v);
        return NULL;
    }

    if ( vdj_init_managegd_broadcast_thread(v, adj_broadcast_handler) != CDJ_OK ) {
        fprintf(stderr, "error: init managed braodcast thread\n");
        sleep(1);
        vdj_destroy(v);
        return NULL;
    }

    if ( vdj_init_status_thread(v) != CDJ_OK ) {
        fprintf(stderr, "error: init status thread\n");
        sleep(1);
        vdj_destroy(v);
        return NULL;
    }

    v->client = adj;
    return v;
}

void
adj_copy_bpm(adj_seq_info_t* adj, unsigned char player_id)
{
    vdj_link_member_t* m;
    if (adj->vdj && adj->vdj->backline) {
        if ( (m = vdj_get_link_member(adj->vdj, player_id))) {
            if (m->bpm >= ADJ_MIN_BPM && m->bpm <= ADJ_MAX_BPM) {
                adj_set_tempo(adj, m->bpm);
            }
        }
    }
}

void
adj_set_playing(adj_seq_info_t* adj, int playing)
{
    if (playing) {
        adj->vdj->master_state |=  CDJ_STAT_FLAG_PLAY;
        adj->vdj->master_state |=  CDJ_STAT_FLAG_ONAIR;
        adj->vdj->active = 1;
    } else {
        adj->vdj->master_state ^=  CDJ_STAT_FLAG_PLAY;
        adj->vdj->master_state ^=  CDJ_STAT_FLAG_ONAIR;
        adj->vdj->active = 0;
    }
}

void
adj_set_sync(adj_seq_info_t* adj, int sync)
{
    if (sync) {
        adj->vdj->master_state |=  CDJ_STAT_FLAG_SYNC;
    } else {
        adj->vdj->master_state ^=  CDJ_STAT_FLAG_SYNC;
    }
}

void
adj_vdj_set_bpm(adj_seq_info_t* adj, float bpm)
{
    adj->vdj->bpm = bpm;
}

/**
 * inform CDJ players of a beat from the alsa sequencer
 */
void
adj_vdj_beat(adj_seq_info_t* adj, unsigned char bar_pos)
{
    vdj_link_member_t* m;
    int64_t diff;
    vdj_t* v = adj->vdj;

    vdj_broadcast_beat(v, bar_pos); // sets v->last_beat as a side effect, TODO bad practice?
    tui_lock();
    render_bar_pos(get_slot(v->player_id), bar_pos);

    // dont calculate beat diffs if we are nanging ona time jump
    if (! adj_trigger_from) {
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (i == v->player_id) continue;
            m = vdj_get_link_member(v, i);
            if (m) {
                diff = vdj_time_diff(v, m);
                // if we are behind render on our beat (if we are ahead, render on your beat)
                if (diff < 0) {
                    render_diff(i, diff);
                    diffs[i] = diff;
                }
            }
        }
    }
    tui_unlock();
}

/**
 * inform AlsaDJ about a beat on a CDJ player
 */
static void
adj_vdj_beat_hook(vdj_t* v, unsigned char player_id)
{
    if (v->client) {
        adj_seq_info_t* adj = (adj_seq_info_t*) v->client;
        if (adj->beat_handler) {
            adj->beat_handler(adj, player_id);
        }
    }
}


// Beat locking - facility to pause the sequencer and restart it on the down beat of 
// any one of the players, we dont have to follow master.
// Calling out to adj MUST be done in the same thread so we have to pass 
// the state requried via atomics to the broadcast handlers (which it trigger by beats)
// N.B. this is far from in time to the beat
void
adj_vdj_trigger_from_player(adj_seq_info_t* adj, unsigned char player_id)
{
    adj_trigger_from = player_id;
}

// ie freeze now,
static void
adj_vdj_lock_on(vdj_t* v)
{
    if (! adj_lock_on) {
        adj_lock_on = 1;
        adj_seq_info_t* adj = (adj_seq_info_t*)v->client;
        adj_beat_lock(adj);
        //tui_debug("lockon");
    }
}

// ie start again
static void
adj_vdj_lock_off(vdj_t* v)
{
    if (adj_lock_on) {
        //tui_debug("lockoff");
        adj_lock_on = 0;
        unsigned char trigger_from_player = adj_trigger_from;
        adj_trigger_from = 0;
        adj_seq_info_t* adj = (adj_seq_info_t*)v->client;

        // if we have to be behind to drop on time, we can delay
        int32_t diff = diffs[trigger_from_player];
        if (diff < 0 ) usleep(diff * 1000);
        // TODO how to come in early
        adj_beat_unlock(adj);
    }
}

void
adj_vdj_difflock(adj_seq_info_t* adj, unsigned char player_id)
{
    difflock_player = player_id;
    difflock_ms = diffs[player_id];

    switch(player_id) {
        case 1 : adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "01");
        case 2 : adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "02");
        case 3 : adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "03");
        case 4 : adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "04");
        default: adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "");
    }

    tui_lock();
    render_lock(player_id, difflock_ms);
    tui_unlock();
}

void
adj_vdj_difflock_arff(adj_seq_info_t* adj)
{
    difflock_player = 0;
    adj->data_change_handler(adj, ADJ_ITEM_DIFFLOCK, "off");
    tui_lock();
    render_lock(0, 0);
    tui_unlock();
}

void
adj_vdj_difflock_nudge(adj_seq_info_t* adj, int32_t amount)
{
    difflock_ms += amount;
    tui_lock();
    render_lock_amount(difflock_ms);
    tui_unlock();
}