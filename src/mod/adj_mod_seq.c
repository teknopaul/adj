#include "adj_mod_seq_api.h"

#include <string.h>

adj_seq_event* adj_mod_seq_noteon(adj_seq_info_t* adj, uint8_t channel, uint8_t note, uint8_t velocity)
{
    adj_seq_event *aevt = (adj_seq_event *) calloc(1, sizeof(adj_seq_event));
    if (!aevt) return NULL;

    snd_seq_ev_set_noteon(&aevt->ev, channel, note, velocity);

    aevt->next = NULL; // important!
    return aevt;
}

adj_seq_event* adj_mod_seq_noteoff(adj_seq_info_t* adj, uint8_t channel, uint8_t note)
{
    adj_seq_event *aevt = (adj_seq_event *) calloc(1, sizeof(adj_seq_event));
    if (!aevt) return NULL;

    snd_seq_ev_set_noteon(&aevt->ev, channel, note, 0);

    return aevt;
}


adj_seq_cfg* adj_mod_seq_cfg_parse(char* args, unsigned int flags, adj_mod_seq_cfg_handler_pt cfg_handler)
{
    char *split;
    char *next;

    adj_seq_cfg *cfg = calloc(1, sizeof(adj_seq_cfg));
    if (!cfg) return NULL;

    cfg->channel = 1;
    cfg->velocity = 127;

    if (args) {
        next = strtok(args, ",");
        while (next) {
            next = strtok(NULL, ",");
            if (next) {
                split = strchr(next, '=');
                if (split) {
                    *split = '0';
                    cfg_handler(next, split + 1);
                }
            }
        }
    }
    return cfg;
}
