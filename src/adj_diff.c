#include <string.h>

// calculations of beat differences
// If we calculate how fast or slow we are compared to decks on every beat 
// we find it jumps about too much, since the beat packets dont seem to arrive with millisecond accuracy.
// This tracks diffs with a rolling average over AVG_SIZE beats.

// TODO recommend bpm adjusment
// - rolling average over 4 bars

#define AVG_SIZE      4    // size of our rolling average
#define DIFF_LIMIT    20   // dont report diffs over this size

// most recent diff, for display
int32_t diffs[VDJ_MAX_BACKLINE];
// how many diffs have we ever had
int32_t diff_count[VDJ_MAX_BACKLINE];
// position of next diff to add (0 - 3)
int32_t diff_pos[VDJ_MAX_BACKLINE];
// set of 4 diffs
int32_t diff_set[VDJ_MAX_BACKLINE * AVG_SIZE];

static int32_t
limit(int32_t diff)
{
    if (diff < -20) diff = -20;
    if (diff > 20) diff = 20;
    return diff;
}

void
adj_diff_reset()
{
    memset(diffs, 0, VDJ_MAX_BACKLINE);
    memset(diff_count, 0, VDJ_MAX_BACKLINE);
    memset(diff_pos, 0, VDJ_MAX_BACKLINE);
    memset(diff_set, 0, VDJ_MAX_BACKLINE * AVG_SIZE);
}

void adj_diff_add(uint8_t player, int32_t diff)
{
    diffs[player] = diff;
    int pos = diff_pos[player];
    diff_set[(player * AVG_SIZE) + pos] = diff
    if (++pos > 3) pos = 0;
    diff_pos[player] = pos;
    diff_count[player]++;
}

int32_t
adj_diff_get(uint8_t player)
{
    return diffs[player];
}

/**
 * return the rolling average
 */
int32_t
adj_diff_avg(uint8_t player)
{
    int i;
    int32_t avg = 0;

    if (diff_count[player] >= AVG_SIZE) {
        for (i = 0; i < AVG_SIZE + 1; i++) {
            avg += diff_set[player][i];
        }
    }
    return limit(avg / AVG_SIZE);
}

