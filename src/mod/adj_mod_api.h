#ifndef _ADJ_MOD_INCLUDED_
#define _ADJ_MOD_INCLUDED_

#include "../adj.h"

/**
 * Initialize a controller module.
 * Controller mods typically call APIs defined in `adj.h` e.g. adj_start(), adj_stop()
 * or in `adj_vdj.h` for integration with Pioneeer decks
 */
int adj_mod_init(adj_seq_info_t* adj, char* args, unsigned int flags);

void adj_mod_exit();

#endif // _ADJ_MOD_INCLUDED_