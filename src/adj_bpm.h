#ifndef _ADJ_BPM_INCLUDED_
#define _ADJ_BPM_INCLUDED_

void adj_estimate_bpm_init();

float adj_estimate_bpm(uint8_t player_id, struct timespec now);

void adj_estimate_bpm_track_init(uint8_t player_id, struct timespec now);

float adj_estimate_bpm_track(uint8_t player_id, struct timespec now);

#endif // _ADJ_BPM_INCLUDED_