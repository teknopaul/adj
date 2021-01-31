#ifndef _ADJ_NUMPAD_INCLUDED_
#define _ADJ_NUMPAD_INCLUDED_

#include "adj.h"

/**
 * Number pad PC keyboard input
 *
 * @author teknopaul 2021
 */

void adj_numpad_reset_term();

int adj_numpad_input(adj_seq_info_t* adj, unsigned int flags);

void adj_numpad_exit();

#endif // _ADJ_NUMPAD_INCLUDED_