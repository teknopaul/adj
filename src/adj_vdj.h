#include <cdj/cdj.h>
#include <cdj/vdj.h>

#include "adj.h"

vdj_t* adj_init_vdj(adj_seq_info_t* adj, char* iface, uint32_t flags, float bpm);

/**
 * Copy the bpm from a CDJ player
 */
void adj_copy_bpm(adj_seq_info_t* adj, unsigned char player_id);
