

#include "adj.h"


static void usage()
{
    printf("options:\n");
    printf("    -i - aconnect a midi port\n");
    printf("    -f - output file\n");
    printf("    -h - display this text\n");
    exit(0);
}


static void print_midi(FILE* f, char* item, snd_seq_event_t* ev)
{
    snd_seq_ev_ctrl_t ctrl = ev->data.control;

    fprintf(f, "%s    %i    %i    %i\n", item, ctrl.channel, ctrl.param, ctrl.value);
}

static void midi_discard(adj_seq_info_t* adj)
{
    int i = 0;
    snd_seq_event_t* ev = NULL;
    while (i++ < 2) {
        while ( snd_seq_event_input_pending(adj->alsa_seq, 1) ) {
            snd_seq_event_input(adj->alsa_seq, &ev);
        }
        sleep(1);
    }
}

static void midi_learn(adj_seq_info_t* adj, FILE* f, char* item)
{
    fprintf(stderr, "control for: %s\n", item);
    snd_seq_event_t* ev = NULL;

    // read and print the first event
    snd_seq_event_input(adj->alsa_seq, &ev);
    if (ev && ev->type == SND_SEQ_EVENT_CONTROLLER) {
        print_midi(f, item, ev);
    }

    midi_discard(adj);
}

int main(int argc, char* argv[])
{
    char* in_port_name = NULL;
    adj_seq_info_t* adj = adj_calloc();

    FILE* f = stdout;

    // parse command line

    int c;
    while ( ( c = getopt(argc, argv, "f:i:n:h") ) != EOF) {
        switch (c) {
            case 'h':
                usage();
                break;
            case 'i':
                in_port_name = optarg;
                break;
            case 'f':
                f = fopen(optarg, "w");
                if (f == NULL) {
                    fprintf(stderr, "error: unable to write to adjmm file\n");
                    return 1;
                }
                break;
            case 'n': 
                adj->seq_name = optarg;
        }
    }

    // setup alsa sequencer
    if ( adj_init_alsa(adj) != ADJ_OK) {
        fprintf(stderr, "alsa init failed\n");
        return 1;
    }

    // set blocking mode, snd_seq_event_input() returns if there is an event,or timeout
    snd_seq_nonblock(adj->alsa_seq, 0);

    // wire up midi controller
    if (in_port_name) {
        char cli[2048];
        cli[2047] = '0';
        snprintf(cli, 2047, "aconnect '%s' '%s:clock' ", in_port_name, adj->seq_name);
        if ( system(cli) ) {
            fprintf(stderr, "aconnect '%s' '%s:clock' failed\n", in_port_name, adj->seq_name);
            return 1;
        }
    } else {
        fprintf(stderr, "error: specify midi device with -i\n");
        return 1;
    }

    // seems there is something at the start of the queue put there by alsa
    midi_discard(adj);

    fprintf(f, "#\n# auto-generated adjmm file\n#\n");

    midi_learn(adj, f, "start");
    midi_learn(adj, f, "stop");
    midi_learn(adj, f, "q_restart");
    midi_learn(adj, f, "nudge_fwd");
    midi_learn(adj, f, "nudge_ffwd");
    midi_learn(adj, f, "nudge_rew");
    midi_learn(adj, f, "nudge_frew");
    // increment and decrement tempo with butttons
    midi_learn(adj, f, "tempo_inc_min");
    midi_learn(adj, f, "tempo_inc");
    midi_learn(adj, f, "tempo_inc_1");
    midi_learn(adj, f, "tempo_dec_min");
    midi_learn(adj, f, "tempo_dec");
    midi_learn(adj, f, "tempo_dec_1");

    // increment and decrement tempo with two buttons and a slider
    midi_learn(adj, f, "slider");
    midi_learn(adj, f, "slider_on");
    midi_learn(adj, f, "slider_off");

    return 0;
}