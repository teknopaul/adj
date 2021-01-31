#ifndef _ADJ_VDJ_INCLUDED_
#define _ADJ_VDJ_INCLUDED_

#include <cdj/cdj.h>
#include <cdj/vdj.h>

#include "adj.h"

vdj_t* adj_vdj_init(adj_seq_info_t* adj, char* iface, uint32_t flags, float bpm, uint32_t vdj_offset);

/**
 * Copy the bpm from a CDJ player to the alsa sequncer
 */
void adj_vdj_copy_bpm(adj_seq_info_t* adj, uint8_t player_id);

/**
 * Copy BPM from the master player,
 * returns master id
 */
uint8_t adj_vdj_copy_master(adj_seq_info_t* adj);

/**
 * Copy BPM from the other player (presumes only two) if not,the lowest player_id is retgurned
 * returns id used
 */
uint8_t adj_vdj_copy_other(adj_seq_info_t* adj);

/**
 * indicate to other CDJs we are playing now
 */
void adj_vdj_set_playing(adj_seq_info_t* adj, int playing);

/**
 * indicate to other CDJs we have sync mode on (makes no difference but its visible in posh mixers)
 */
void adj_vdj_set_sync(adj_seq_info_t* adj, int sync);

/**
 * set the bpm in our virtual cdj, it will be forwarded in updates and beats.
 */
void adj_vdj_set_bpm(adj_seq_info_t* adj, float bpm);

/**
 * Fire the beat message to the ProLink network
 */
void adj_vdj_beat(adj_seq_info_t* adj, uint8_t bar_pos);

/**
 * any thread, keyb/midi/ui can call this mehtod to indicate that hte loop
 * should stop and restart ont eh next down beat from this player.
 */
void adj_vdj_trigger_from_player(adj_seq_info_t* adj, uint8_t player_id);

/**
 * put ya bpm on lockdown, i.e. synced to a CDJ.
 * N.B. selected deck does not have to be master: you can lock midi to a deck that is not audible yet
 * and them bring in both into the mix at the same time and sync manuallyby beat mixing on the CDJ.
 */
void adj_vdj_difflock(adj_seq_info_t* adj, uint8_t player_id, int use_default);

/**
 * turn off lock/sync
 */
void adj_vdj_difflock_arff(adj_seq_info_t* adj);

/**
 * Adjust the lock offset
 */
void adj_vdj_difflock_nudge(adj_seq_info_t* adj, int32_t amount);

/**
 * automatically swap difflock to the player that is master when master changes.
 * N.B. master does not have to be sync, so you can use this just to tell midi devices
 * which deck to follow from the CDJs without the need to touch adj or keyb or midi controller.
 * To do this only switch master when everything is in sync.
 */
void adj_vdj_difflock_master(adj_seq_info_t* adj, int on_off);


void adj_vdj_become_master(adj_seq_info_t* adj);

#endif // _ADJ_VDJ_INCLUDED_