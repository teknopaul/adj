
#include <stdio.h>
#include <stdlib.h>

#include "adj.h"
#include "adj_store.h"

#define BPM_FILE       "/var/tmp/adj-bpm"

void
adj_save_bpm(adj_seq_info_t* adj)
{
    if (adj->bpm) {
        FILE* p;
        if ( (p = fopen(BPM_FILE, "w")) ) {
            fprintf(p, "%f\n", adj->bpm);
            fflush(p);
            fclose(p);
            adj->data_change_handler(adj, ADJ_ITEM_OP, "saved");
        }
    }
}

void
adj_load_bpm(adj_seq_info_t* adj)
{
    // read bpm from /var/tmp/adj-bpm
    FILE* p;
    if ( (p = fopen(BPM_FILE, "r")) ) {
        if ( fscanf(p, "%f\n", &adj->bpm) == EOF ) {
            fprintf(stderr, "bpm load failed\n");
        }
        fclose(p);
    }
}
