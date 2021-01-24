#ifndef _ADJ_MIDIIN_INCLUDED_
#define _ADJ_MIDIIN_INCLUDED_

#include "adj.h"

//SNIP_midiin_constants

#define ADJ_MIDIIN_START       1
#define ADJ_MIDIIN_STOP        2
#define ADJ_MIDIIN_TOGGLE      3
#define ADJ_MIDIIN_Q_RESTART   4
#define ADJ_MIDIIN_NUDGE_FWD   5
#define ADJ_MIDIIN_NUDGE_FFWD  6
#define ADJ_MIDIIN_NUDGE_REW   7
#define ADJ_MIDIIN_NUDGE_FREW  8


#define ADJ_MIDIIN_TEMPO_INC_MIN    10
#define ADJ_MIDIIN_TEMPO_INC        11
#define ADJ_MIDIIN_TEMPO_INC_1      12
#define ADJ_MIDIIN_TEMPO_DEC_MIN    13
#define ADJ_MIDIIN_TEMPO_DEC        14
#define ADJ_MIDIIN_TEMPO_DEC_1      15

#define ADJ_MIDIIN_SLIDER           16
#define ADJ_MIDIIN_SLIDER_ON        17
#define ADJ_MIDIIN_SLIDER_OFF       18

#define ADJ_MAX_OP                  18

#define ADJ_ANY_CHANNNEL           0
#define ADJ_UNSET_VALUE            128
#define ADJ_ANY_VALUE              129

/**
 * midi map operation
 */
typedef struct {
    unsigned char channel;
    unsigned char controller;
    unsigned char value;
} adj_midiin_map_op;

/**
 * midi map
 */
typedef struct {
    adj_midiin_map_op op[ADJ_MAX_OP];
} adj_midiin_map;

//SNIP_midiin_constants


int adj_midiin(adj_seq_info_t* adj);
void adj_midiin_exit();

#endif // _ADJ_MIDIIN_INCLUDED_