
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include <cdj/cdj.h>
#include <cdj/vdj.h>
#include <cdj/vdj_net.h>
#include <cdj/vdj_discovery.h>

#include "adj_vdj.h"
#include "tui.h"

//SNIP_adj_vdj_tui

#define MAX_PLAYERS    4
#define BACKLINE_Y      15

static int slot_x(int id)
{
    return 10 + (15 * (id - 1));
}

static void render_slot(int id)
{
    tui_text_at("[            ]", slot_x(id), BACKLINE_Y + 3);
    tui_text_at("[            ]", slot_x(id), BACKLINE_Y + 2);
    tui_text_at("[            ]", slot_x(id), BACKLINE_Y + 1);
    tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + 2);
    printf("%s%02i%s", TUI_GREY, id, TUI_NORMAL);
}

static void render_model(int id, char* name)
{
    tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + 3);
    printf("%s%s%s", TUI_BOLD, name, TUI_NORMAL);
    tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + 2);
    printf("%s%02i%s", TUI_BOLD, id, TUI_NORMAL);
}

static void render_status(int id, char* status)
{
    tui_set_cursor_pos(slot_x(id) + 4, BACKLINE_Y + 2);
    fputs(status, stdout);
}

static void render_bpm(int id, float bpm)
{
    tui_set_cursor_pos(slot_x(id) + 1, BACKLINE_Y + 1);
    printf("%06.2f", bpm);
    fflush(stdout);
}

static void render_master(int id)
{
    tui_set_cursor_pos(11, BACKLINE_Y);
    printf("%s%02i%s", TUI_RED, id, TUI_NORMAL);
    fflush(stdout);
}

static void render_bar_pos(unsigned char id, unsigned char bar_pos)
{
    tui_set_cursor_pos(slot_x(id) + 8 + 1, BACKLINE_Y + 1);
    printf("____");
    tui_set_cursor_pos(slot_x(id) + 8 + bar_pos, BACKLINE_Y + 1);
    printf("♪");
    //printf("%i", bar_pos);
    fflush(stdout);
}

static void render_backline()
{
    int p;
    tui_text_at("model:",  2, BACKLINE_Y + 3);
    tui_text_at("player:", 2, BACKLINE_Y + 2);
    tui_text_at("bpm:",    2, BACKLINE_Y + 1);
    tui_text_at("master: [  ]", 2, BACKLINE_Y + 0);
    for (p = 1; p <= MAX_PLAYERS; p++) {
        render_slot(p);
    }
    fflush(stdout);
}

//SNIP_adj_vdj_tui

static void adj_discovery_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    cdj_discovery_packet_t* d_pkt;
    if (cdj_packet_type(packet, len) == CDJ_KEEP_ALIVE) {
        d_pkt = cdj_new_discovery_packet(packet, len);
        if ( d_pkt &&
            d_pkt->player_id &&
            v->backline->link_members[d_pkt->player_id]
            ) {
            
            if (d_pkt->player_id <= 4) {
                char* model = cdj_model_name(packet, len, CDJ_DISCOVERY_PORT);
                if (model) {
                    tui_lock();
                    render_model(d_pkt->player_id, model);
                    tui_unlock();
                    free(model);
                }
            }
        }
        if (d_pkt) free(d_pkt);
    }
}

static void adj_update_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    vdj_link_member_t* m;
    unsigned char flags;
    cdj_cdj_status_packet_t* cs_pkt = cdj_new_cdj_status_packet(packet, len);
    if ( cs_pkt ) {
        if ( cs_pkt->player_id ) {
            if ( (m = v->backline->link_members[cs_pkt->player_id]) ) {
                flags = cdj_status_flags(cs_pkt);
                char* status = cdj_flags_to_emoji(flags);
                tui_lock();
                render_bpm(cs_pkt->player_id, m->bpm);
                if (flags & CDJ_STAT_FLAG_MASTER) {
                    render_master(cs_pkt->player_id);
                }
                render_status(cs_pkt->player_id, status);
                tui_unlock();
                free(status);
            }
        }
        free(cs_pkt);
    }
}

static void adj_broadcast_handler(vdj_t* v, unsigned char* packet, uint16_t len)
{
    vdj_link_member_t* m;
    cdj_beat_packet_t* b_pkt = cdj_new_beat_packet(packet, len);
    if ( b_pkt ) {
        if ( b_pkt->player_id ) {
            if ( (m = v->backline->link_members[b_pkt->player_id]) ) {
                tui_lock();
                render_bpm(b_pkt->player_id, b_pkt->bpm);
                render_bar_pos(b_pkt->player_id, b_pkt->bar_pos);
                tui_unlock();
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

    if ( vdj_init_broadcast_thread(v, adj_broadcast_handler) != CDJ_OK ) {
        fprintf(stderr, "error: init managed braodcast thread\n");
        sleep(1);
        vdj_destroy(v);
        return NULL;
    }

    return v;
}

void adj_copy_bpm(adj_seq_info_t* adj, unsigned char player_id)
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