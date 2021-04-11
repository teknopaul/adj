
#include <stdio.h>
#include "adj.h"

// user interface

static void init_error(adj_ui_t* ui, char* msg)
{
    puts(msg);
    putchar('\n');
}

static void data_item(adj_ui_t* ui, int idx, char* name, char* value)
{
    printf("%s  %s\n", name, value);
}

static void signal_exit(int sig)
{
    putchar('\n');
}

static char* symbol_off(int q_beat)
{
    if (q_beat % 16 == 0) return "|";
    if (q_beat % 4 == 0) return ":";
    return ".";
}

// callbacks

static void data_change_handler(adj_ui_t* ui, adj_seq_info_t* adj, int idx, char* value)
{

}

static void message_handler(adj_ui_t* ui, adj_seq_info_t* adj, char* message)
{
    puts(message);
    fflush(stdout);
}

static void tick_handler(adj_ui_t* ui, adj_seq_info_t* adj, snd_seq_tick_time_t tick)
{
    int quarter_beats = tick == 0 ? 0 : tick / ADJ_CLOCKS_PER_BEAT;

    fputs(symbol_off(quarter_beats), stdout);
    if (quarter_beats % 64 == 63) {
        putchar('|');
        putchar('\n');
    }
    fflush(stdout);
}

static void exit_handler(adj_ui_t* ui, int sig)
{
    signal_exit(sig);
}

static void stop_handler(adj_ui_t* ui, adj_seq_info_t* adj)
{
    puts("");
}

static void start_handler(adj_ui_t* ui, adj_seq_info_t* adj)
{

}


void initialize_cli(adj_ui_t* ui, uint64_t flags)
{
    ui->init_error_handler = init_error;
    ui->message_handler = message_handler;
    ui->data_item_handler = data_item;
    ui->data_change_handler = data_change_handler;
    ui->tick_handler = tick_handler;
    ui->beat_handler = NULL;
    ui->stop_handler = stop_handler;
    ui->start_handler = start_handler;
    ui->exit_handler = exit_handler;
}