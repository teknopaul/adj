#ifndef _ADJ_JS_INCLUDED_
#define _ADJ_JS_INCLUDED_

#include "adj.h"

/**
 * enable joystick input
 */
int adj_js_input(adj_seq_info_t* adj, char* dev, unsigned int flags);

void adj_js_exit();


#endif // _ADJ_JS_INCLUDED_