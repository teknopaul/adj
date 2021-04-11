
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdatomic.h>

#include <cdj/cdj.h>
#include <cdj/vdj.h>
#include <cdj/vdj_net.h>
#include <cdj/vdj_discovery.h>
#include <cdj/vdj_pselect.h>
#include <cdj/vdj_master.h>

#include "adj.h"
#include "adj_bpm.h"
#include "adj_diff.h"
#include "adj_vdj.h"
#include "tui.h"

/**
 * Code for becoming a Virtual CDJ and talking to Pioneer CDJs. 
 * This has mashed up tui code and VDJ handlers tui can be turned off but not easily replace with a GUI.
 * TODO separate code and use handlers for all ui updates
 */

static void adj_vdj_lock_on(vdj_t* v);
static void adj_vdj_lock_off(vdj_t* v);
static void adj_vdj_beat_hook(vdj_t* v, uint8_t player_id);

static unsigned _Atomic adj_trigger_from = ATOMIC_VAR_INIT(0);    
static unsigned _Atomic adj_lock_on = ATOMIC_VAR_INIT(0);         // trigger lock, sync to downbeat
static unsigned _Atomic adj_difflock_master = ATOMIC_VAR_INIT(0); // swap difflock when master changes
static unsigned _Atomic adj_follow_tempo = ATOMIC_VAR_INIT(0);    // copy tempo changes from the followed deck
static unsigned _Atomic adj_track_start = ATOMIC_VAR_INIT(0);     // set when we manually mark track start for bpm analysis


// difflock aka auto-sync, this holds the player_id we should lock to
static unsigned _Atomic difflock_player = ATOMIC_VAR_INIT(0);

// this holds the milliseconds difference we want to maintain
static int32_t difflock_ms = 0;

// default difflock ms offset
static int32_t difflock_default = 0;

// our notion of master, so we can detect change
static uint8_t master = 0;

// our notion of bpm, so we can detect change
static float bpm = 120.0;

static int tui = 1;

//SNIP_adj_vdj_tui

#define MAX_PLAYERS     4
#define BACKLINE_Y      15

// UI offsets
#define Y_MDL      4 // model
#define Y_PLR      3 // player
#define Y_BPM      2 // beats per minute
#define Y_DIF      1 // difference in ms

static void render_slot(int id);

static int rendered_self = 0;

// players with player_id > 4 e.g. rekordbox get mapped to a lower id so they fit on screen.
static uint8_t next_slot = 5;
static uint8_t high_slots[VDJ_MAX_BACKLINE];


// a slot is the column number of the CDJ info on screen
static uint8_t
get_slot(uint8_t player_id)
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

static void
render_bpm_estimate(int id, float bpm, float bpm_track)
{
    if (tui) {
        tui_debug("bpm est. = %07.3f / %07.3f", bpm, bpm_track);
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
        if (id == 0) {
            printf("[--] [----]");
        } else if (difflock_player && adj_difflock_master && id) {
            printf("[%s%02i%s] [%+04i]", TUI_RED, id, TUI_NORMAL, diff);
        } else if (difflock_player && id) {
            printf("[%s%02i%s] [%+04i]", TUI_YELLOW, id, TUI_NORMAL, diff);
        } else {
            printf("[--] [----] [----]");
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
render_bar_pos(uint8_t id, uint8_t bar_pos)
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
render_diff(int id, int32_t diff, int32_t avg)
{
    if (tui) {
        tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + Y_DIF);
        printf("%+04i/%+04i", diff, avg);
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
        tui_text_at("diff:",   2, BACKLINE_Y + Y_DIF);
        tui_text_at("master: [  ] [  ] [    ]", 2, BACKLINE_Y + 0);
        for (p = 1; p <= MAX_PLAYERS; p++) {
            render_slot(p);
        }
        fflush(stdout);
    }
}

//SNIP_adj_vdj_tui

static int32_t
limit(int32_t diff)
{
    if (diff < -20) diff = -20;
    if (diff > 20) diff = 20;
    return diff;
}

/**
 * Limit the amount we nudge in one beat, seems sometimes beats are delayed.
 */
static int32_t
auto_nudge_amount(int8_t player_id, int32_t difflock_ms)
{
    return limit(adj_diff_avg(player_id) - difflock_ms);
}

static void
adj_discovery_ph(vdj_t* v, cdj_discovery_packet_t* d_pkt)
{
    if (d_pkt->type == CDJ_KEEP_ALIVE) {
        if (d_pkt->player_id && v->backline->link_members[d_pkt->player_id]) {
            tui_lock();
            render_model(get_slot(d_pkt->player_id), cdj_discovery_model(d_pkt), 0);
            tui_unlock();
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
}

// CDJ_UPDATE packet handler
static void
adj_update_ph(vdj_t* v, cdj_cdj_status_packet_t* cs_pkt)
{
    uint8_t slot;
    vdj_link_member_t* m;
    uint8_t flags;
    if ( cs_pkt->player_id ) {
        if ( (m = vdj_get_link_member(v, cs_pkt->player_id)) ) {
            flags = cdj_status_flags(cs_pkt);
            char* status = cdj_flags_to_emoji(flags);
            tui_lock();
            slot = get_slot(cs_pkt->player_id);
            render_bpm(slot, m->bpm);
            if (flags & CDJ_STAT_FLAG_MASTER) {
                if (master != cs_pkt->player_id) {
                    // change of master
                    render_master(cs_pkt->player_id);
                    master = cs_pkt->player_id;
                    if (adj_difflock_master) {
                        difflock_player = master;
                    }
                }
            }
            render_status(slot, status);
            tui_unlock();
            free(status);
        }
    }
}

// CDJ_BEAT packet handler
static void
adj_beat_ph(vdj_t* v, cdj_beat_packet_t* b_pkt)
{
    uint8_t slot;
    int32_t diff;
    vdj_link_member_t* m;
    float est = 0.0, est_track = 0.0;

    if (b_pkt->bar_pos == 1) {
        est = adj_estimate_bpm(b_pkt->player_id, b_pkt->timestamp);
        est_track = adj_estimate_bpm_track(b_pkt->player_id, b_pkt->timestamp);
        if (adj_track_start == b_pkt->player_id) {
            adj_estimate_bpm_track_init(b_pkt->player_id, b_pkt->timestamp);
            adj_track_start = 0;
        }
    }

    if (adj_trigger_from > 0) {
        adj_vdj_lock_on(v);
    }

    if ( (m = vdj_get_link_member(v, b_pkt->player_id)) ) {
        adj_vdj_beat_hook(v, b_pkt->player_id);
        diff = vdj_time_diff(v, m);
        slot = get_slot(b_pkt->player_id);
        tui_lock();
        render_bpm(slot, b_pkt->bpm);
        if (b_pkt->bar_pos == 1) {
            render_bpm_estimate(b_pkt->player_id, est, est_track);
        }

        render_bar_pos(slot, b_pkt->bar_pos);
        // if you are behind, render on your beat, (if you are ahead render on our beat)
        if (diff > 0) {
            render_diff(slot, diff, adj_diff_avg(b_pkt->player_id));
            adj_diff_add(b_pkt->player_id, diff);
        }
        tui_unlock();

        // trigger from OR beat lock not both
        if (adj_trigger_from) {
            if (adj_trigger_from == b_pkt->player_id && b_pkt->bar_pos == 1) {
                adj_vdj_lock_off(v);
            }
        // auto nudge on the last beat, in theory the down beat should then always be in time.
        } else if (difflock_player == b_pkt->player_id && b_pkt->bar_pos == 4) {
            if (diff - difflock_ms) {
                adj_nudge_millis((adj_seq_info_t*) v->client, auto_nudge_amount(difflock_player, difflock_ms));
            }
        }
        if (difflock_player == b_pkt->player_id && adj_follow_tempo && bpm != b_pkt->bpm) {
            bpm = b_pkt->bpm;
            adj_set_tempo((adj_seq_info_t*) v->client, bpm);
        }
    }
}

/**
 * Virtual CDJ application entry point.
 */
vdj_t*
adj_vdj_init(adj_seq_info_t* adj, char* iface, uint32_t flags, float bpm, uint32_t vdj_offset)
{

    memset(high_slots, 0, VDJ_MAX_BACKLINE);
    adj_diff_reset();
    difflock_default = vdj_offset;
    adj_estimate_bpm_init();
    adj_lock_on = 0;

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

    if (vdj_pselect_init(v, adj_discovery_ph, NULL, adj_beat_ph, NULL, adj_update_ph ) != CDJ_OK) {
        fprintf(stderr, "error: cdj pselect\n");
        vdj_pselect_stop(v);
        usleep(200000);
        vdj_destroy(v);
        return NULL;
    } 

    v->client = adj;
    return v;
}

void
adj_vdj_copy_bpm(adj_seq_info_t* adj, uint8_t player_id)
{
    vdj_link_member_t* m;
    if (adj->vdj && adj->vdj->backline) {
        if ( (m = vdj_get_link_member(adj->vdj, player_id))) {
            if (m->bpm >= ADJ_MIN_BPM && m->bpm <= ADJ_MAX_BPM) {
                adj_set_tempo(adj, m->bpm);
                return;
            }
        }
    }
}

uint8_t
adj_vdj_copy_master(adj_seq_info_t* adj)
{

    vdj_link_member_t* m;

    if (master && adj->vdj && adj->vdj->backline) {
        if ( (m = vdj_get_link_member(adj->vdj, master))) {
            adj_set_tempo(adj, m->bpm);
            return master;
        }
    }
    return 0;
}

uint8_t
adj_vdj_copy_other(adj_seq_info_t* adj)
{
    uint8_t i;
    vdj_link_member_t* m;

    if (adj->vdj && adj->vdj->backline) {
        for (i = 1; i <= MAX_PLAYERS; i++) {
            if (i == master || i == adj->vdj->player_id) continue;
            if ( (m = vdj_get_link_member(adj->vdj, i))) {
                adj_set_tempo(adj, m->bpm);
                return i;
            }
        }
    }
    return 0;
}

void
adj_vdj_set_playing(adj_seq_info_t* adj, int playing)
{
    vdj_set_playing(adj->vdj, playing);
}

void
adj_vdj_set_sync(adj_seq_info_t* adj, int sync)
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
adj_vdj_beat(adj_seq_info_t* adj, uint8_t bar_pos)
{
    int i;
    vdj_link_member_t* m;
    int64_t diff;
    vdj_t* v = adj->vdj;

    vdj_broadcast_beat(v, adj->bpm, bar_pos); // sets v->last_beat as a side effect, TODO bad practice?
    tui_lock();
    render_bar_pos(get_slot(v->player_id), bar_pos);

    // dont calculate beat diffs if we are hanging on a time jump
    if (! adj_trigger_from) {
        for (i = 1; i <= MAX_PLAYERS; i++) {
            if (i == v->player_id) continue;
            m = vdj_get_link_member(v, i);
            if (m) {
                diff = vdj_time_diff(v, m);
                // if we are behind render on our beat (if we are ahead, render on your beat)
                if (diff < 0) {
                    render_diff(i, diff, adj_diff_avg(i));
                    adj_diff_add(i, diff);
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
adj_vdj_beat_hook(vdj_t* v, uint8_t player_id)
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
// the state required via atomics to the broadcast handlers (which it trigger by beats)
// N.B. this is far from in time to the beat
void
adj_vdj_trigger_from_player(adj_seq_info_t* adj, uint8_t player_id)
{
    adj_trigger_from = player_id;
    //tui_debug("adj_trigger_from");
}

// ie freeze now,
static void
adj_vdj_lock_on(vdj_t* v)
{
    if (! adj_lock_on) {
        adj_lock_on = 1;
        adj_seq_info_t* adj = (adj_seq_info_t*)v->client;
        adj_beat_lock(adj);
        adj->data_change_handler(adj, ADJ_ITEM_OP, "lockon");
    }
}

// ie start again
static void
adj_vdj_lock_off(vdj_t* v)
{
    if (adj_lock_on) {
        adj_lock_on = 0;
        adj_seq_info_t* adj = (adj_seq_info_t*)v->client;

        // TODO if we have to be behind to drop on time, we can delay
        //int32_t diff = adj_diff_get(adj_trigger_from);
        //if (diff > 0) usleep(diff * 1000);
        adj_trigger_from = 0;
        // TODO how to come in early
        adj_beat_unlock(adj);
    }
}

// difflock , keep the offset between midi and CDJs in sync.
// something similar to what deep symetry call "degraded sync"

void
adj_vdj_difflock(adj_seq_info_t* adj, uint8_t player_id, int use_default)
{
    if (!vdj_get_link_member(adj->vdj, player_id)) {
        return;
    }

    difflock_player = player_id;
    if (use_default) {
        difflock_ms = difflock_default;
    } else {
        difflock_ms = adj_diff_get(player_id);  // TODO maybe avg would be better?
    }

    // if explicit lock against not master, stop follow master
    if (master != player_id) adj_difflock_master = 0;

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
    adj_diff_reset();
    adj_estimate_bpm_init();
}

void
adj_vdj_difflock_nudge(adj_seq_info_t* adj, int32_t amount)
{
    difflock_ms += amount;
    tui_lock();
    render_lock_amount(difflock_ms);
    tui_unlock();
}

void
adj_vdj_difflock_master(adj_seq_info_t* adj, int on_off)
{
    adj_difflock_master = on_off;
}

void
adj_vdj_follow_tempo(adj_seq_info_t* adj, int on_off)
{
    adj_follow_tempo = on_off;
}

void
adj_vdj_become_master(adj_seq_info_t* adj)
{
    vdj_request_master(adj->vdj);
}


void
adj_vdj_track_start(adj_seq_info_t* adj, uint8_t player_id)
{
    adj_track_start = player_id;
}
