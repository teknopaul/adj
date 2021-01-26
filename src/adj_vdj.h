#include <cdj/cdj.h>
#include <cdj/vdj.h>

#include "adj.h"

vdj_t* adj_init_vdj(adj_seq_info_t* adj, char* iface, uint32_t flags, float bpm);

/**
 * Copy the bpm from a CDJ player to the alsa sequncer
 */
void adj_copy_bpm(adj_seq_info_t* adj, unsigned char player_id);
/**
 * indicate to other CDJs we are playing now
 */
void adj_set_playing(adj_seq_info_t* adj, int playing);

/**
 * indicate to other CDJs we have sync mode on (makes no difference but its visible in posh mixers)
 */
void adj_set_sync(adj_seq_info_t* adj, int sync);

/**
 * set the bpm in our virtual cdj, it will be forwarded in updates and beats.
 */
void adj_vdj_set_bpm(adj_seq_info_t* adj, float bpm);

/**
 * Fire the beat message to the ProLink network
 */
void adj_vdj_beat(adj_seq_info_t* adj, unsigned char bar_pos);

/**
 * any thread, keyb/midi/ui can call this mehtod to indicate that hte loop
 * should stop and restart ont eh next down beat from this player.
 */
void adj_vdj_trigger_from_player(adj_seq_info_t* adj, unsigned char player_id);

/**
 * put ya bpm on lockdown, i.e. synced to a CDJ, N.B. does not have to be master.
 */
void adj_vdj_difflock(adj_seq_info_t* adj, unsigned char player_id);

/**
 * turn off lock/sync
 */
void adj_vdj_difflock_arff(adj_seq_info_t* adj);

/**
 * Adjust the lock offset
 */
void adj_vdj_difflock_nudge(adj_seq_info_t* adj, int32_t amount);
