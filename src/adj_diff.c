#include <string.h>
#include <inttypes.h>

// calculations of beat differences
// If we calculate how fast or slow we are compared to decks on every beat 
// we find it jumps about too much, since the beat packets dont seem to arrive with millisecond accuracy.
// This tracks diffs with a rolling average over AVG_SIZE beats.

// TODO recommend bpm adjusment
// - rolling average over 4 bars

#define AVG_SIZE      4    // size of our rolling average
#define DIFF_LIMIT    20   // dont report diffs over this size
#define DIFFS_MAX     32   // same as VDJ_MAX_BACKLINE but I dont want a dependency on vdj.h

// most recent diff, for display
int32_t diffs[DIFFS_MAX];
// how many diffs have we ever had
int32_t diff_count[DIFFS_MAX];
// position of next diff to add (0 - 3)
int32_t diff_pos[DIFFS_MAX];
// set of 4 diffs
int32_t diff_set[DIFFS_MAX * AVG_SIZE];

void
adj_diff_reset()
{
    memset(diffs, 0, sizeof(int32_t) * DIFFS_MAX);
    memset(diff_count, 0, sizeof(int32_t) * DIFFS_MAX);
    memset(diff_pos, 0, sizeof(int32_t) * DIFFS_MAX);
    memset(diff_set, 0, sizeof(int32_t) * DIFFS_MAX * AVG_SIZE);
}

void adj_diff_add(uint8_t player, int32_t diff)
{
    if (--player > DIFFS_MAX - 1) return; // zero based arrays

    diffs[player] = diff;
    int pos = diff_pos[player];
    diff_set[player * AVG_SIZE + pos] = diff;
    diff_pos[player] = ++pos > 3 ? 0 : pos;
    diff_count[player]++;
}

int32_t
adj_diff_get(uint8_t player)
{
    if (--player > DIFFS_MAX - 1) return 0;

    return diffs[player];
}

/**
 * return the rolling average, or 0 if we dont have data
 */
int32_t
adj_diff_avg(uint8_t player)
{
    if (--player > DIFFS_MAX - 1) return 0;

    int i;
    int32_t avg = 0;

    if (diff_count[player] >= AVG_SIZE) {
        for (i = 0; i < AVG_SIZE + 1; i++) {
            avg += diff_set[player * AVG_SIZE + i];
        }
        return avg / AVG_SIZE;
    }
    return 0;
}

