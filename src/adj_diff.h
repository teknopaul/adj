#ifndef _ADJ_DIFF_INCLUDED_
#define _ADJ_DIFF_INCLUDED_

#define AVG_SIZE      4    // size of our rolling average
#define DIFF_LIMIT    20   // dont report diffs over this size for auto-nudge

/**
 * set all values to zero, call this before using any functions here
 */
void adj_diff_reset();

/**
 * submit a diff for averaging
 */
void adj_diff_add(uint8_t player, int32_t diff);

/**
 * get the last submitted diff, e.g. for display
 */
int32_t adj_diff_get(uint8_t player);

/**
 * return the rolling average diff
 */
int32_t adj_diff_avg(uint8_t player);


#endif // _ADJ_DIFF_INCLUDED_