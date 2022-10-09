
#include "adj.h"
#include "adj_midiout.h"

/**
 * Connect adj to any hardware midi outputs
 */
int adj_wire_midi_out(adj_seq_info_t* adj)
{
    int mask;
    snd_seq_port_subscribe_t *subs;
    snd_seq_addr_t sender, dest;
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;

    sender.client = adj->client_id;
    sender.port = 0;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(adj->alsa_seq, cinfo) >= 0) {
        dest.client = snd_seq_client_info_get_client(cinfo);
        /* reset query info */
        snd_seq_port_info_set_client(pinfo, dest.client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(adj->alsa_seq, pinfo) >= 0) {
            mask = snd_seq_port_info_get_type(pinfo);
            if (mask & SND_SEQ_PORT_CAP_WRITE && mask & SND_SEQ_PORT_TYPE_HARDWARE) {
                dest.port = snd_seq_port_info_get_port(pinfo);

                snd_seq_port_subscribe_alloca(&subs);
                snd_seq_port_subscribe_set_sender(subs, &sender);
                snd_seq_port_subscribe_set_dest(subs, &dest);
                snd_seq_port_subscribe_set_queue(subs, adj->q);

                if (snd_seq_subscribe_port(adj->alsa_seq, subs) < 0) {
                    fprintf(stderr, "Connection failed to %s (%s)\n", 
                        snd_seq_client_info_get_name(cinfo),
                        snd_strerror(errno));
                }
                else {
                    fprintf(stderr, "Connected %s:clock to %s:%d\n", adj->seq_name, snd_seq_client_info_get_name(cinfo), dest.port);
                    adj->ui->data_item_handler(adj->ui, ADJ_ITEM_MIDI_OUT, "midi out:", (char*) snd_seq_client_info_get_name(cinfo));

                }
                
            }
        }
    }
    return 0;
}