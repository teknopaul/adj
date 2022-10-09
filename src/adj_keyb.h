#ifndef _ADJ_KEYB_INCLUDED_
#define _ADJ_KEYB_INCLUDED_

/**
 * Normal PC keyboard input (not a midi|piano keyboard)
 * N.B. can have both
 *
 * @author teknopaul 2021
 */

#include <pthread.h>
#include <termios.h>

void adj_keyb_reset_term();

/**
 * intialize the keyboard input thread so that the computer keyboard can control adj
 */
int adj_keyb_input(adj_seq_info_t* adj, unsigned int flags);

void adj_keyb_exit();

#endif // _ADJ_KEYB_INCLUDED_