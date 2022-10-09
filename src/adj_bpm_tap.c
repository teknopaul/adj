
#include <time.h>
#include <inttypes.h>


#include "adj.h"
#include "adj_bpm_tap.h"

// timestamp of last recorded beat
static uint64_t tap_start;
static int pos = 0;

static uint64_t
time_micros()
{
    struct timespec now;
    clock_gettime(CDJ_CLOCK, &now);
    uint64_t m = now.tv_sec * 1000000;
    return m + (now.tv_nsec / 1000);
}

int adj_bpm_tap(adj_seq_info_t* adj)
{
    uint64_t now = time_micros();
    if (pos++ == 0) {
        tap_start = now;
    } else if (pos == 4) {
        adj_set_tempo(adj, adj_micros_to_bpm( (now - tap_start) / 3));
        pos = 0;
        return 4;
    }
    return 0;
}

void adj_bpm_tap_reset()
{
    pos = 0;
}

