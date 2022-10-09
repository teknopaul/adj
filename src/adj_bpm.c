

// calculated bpm , this seems to differ from the reported bpm.
// on XDJ700's track BPM calculation seems to be correct but calculations per bar are not

#include <time.h>
#include <inttypes.h>

#include "adj.h"
#include "adj_bpm.h"

#define BPMS_MAX     32   // same as VDJ_MAX_BACKLINE but I dont want a dependency on vdj.h

// timestamp of last recorded beat
uint64_t last_beats[BPMS_MAX];

// timestamp of track start & number of beats since then
// array has 2 uint64_t per player_id
uint64_t track_total[BPMS_MAX * 2];

static uint64_t
time_micros(struct timespec now)
{
    uint64_t m = now.tv_sec * 1000000;
    return m + (now.tv_nsec / 1000);
}

// called once per bar
// @now is the time the beat packet arrived (close to now)
float
adj_estimate_bpm(uint8_t player_id, struct timespec now)
{
    float bpm = 0;
    uint64_t now_m = time_micros(now);
    uint64_t last_beat = last_beats[player_id];
    if (last_beat) {
        uint64_t beat_len = now_m - last_beat;
        bpm = adj_micros_to_bpm(beat_len / 4); // / 4 because called once per bar
    }
    last_beats[player_id] = now_m;
    track_total[1 + (player_id * 2)] += 4;
    return bpm;
}

// record now as the start of the track for the purposes of bpm estimation
void
adj_estimate_bpm_track_init(uint8_t player_id, struct timespec now)
{
    uint64_t now_m = time_micros(now);

    track_total[player_id * 2] = now_m;
    track_total[1 + (player_id * 2)] = 0;
}

// estimate the BPM over the whole track
float
adj_estimate_bpm_track(uint8_t player_id, struct timespec now)
{
    uint64_t now_m = time_micros(now);
    uint64_t first_beat = track_total[player_id * 2];
    uint64_t beat_count = track_total[1 + (player_id * 2)];
    if (first_beat && beat_count) {
        uint64_t avg_beat_len = (now_m - first_beat) / beat_count;
        //fprintf(stderr, "first_beat=%lli, now_m=%lli, (now_m - first_beat)=%lli, beat_count=%lli bpm=%f\n", first_beat, now_m, (now_m - first_beat), beat_count, adj_micros_to_bpm(avg_beat_len));
        return adj_micros_to_bpm(avg_beat_len);
    } else {
        return 0;
    }
}

void
adj_estimate_bpm_init()
{
    memset(last_beats, 0, sizeof(uint64_t) * BPMS_MAX);
    memset(track_total, 0, sizeof(uint64_t) * BPMS_MAX * 2);
}
