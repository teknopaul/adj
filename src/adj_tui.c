

#include <stdatomic.h>
#include <pthread.h>

#include "adj.h"
#include "tui.h"

// user interface

static unsigned _Atomic adj_tui_running = ATOMIC_VAR_INIT(1);

static int message_ticks = 0;


static void data_item(int idx, char* name, char* value)
{
    tui_text_at(name, 2, 14 - idx);
    tui_data_at(value, 15, 14 - idx);
    tui_set_cursor_pos(0, 0);
}

static void data_item_fixed_width(int idx, char* name, char* value)
{
    tui_text_at(name, 2, 14 - idx);
    tui_data_at_fixed(value, 10, 15, 14 - idx);
}

static void tui_setup(int height)
{
    tui_init(height);
    tui_set_window_title("adj - powered by libadj and libvdj");

    data_item(ADJ_ITEM_PORT, "alsa port:", "");
    data_item(ADJ_ITEM_CLIENT_ID, "client_id:", "");
    data_item(ADJ_ITEM_MIDI_IN, "midi in:", "");
    data_item(ADJ_ITEM_MIDI_OUT, "midi out:", "");
    data_item(ADJ_ITEM_KEYB, "keyb:", "  ");
    data_item_fixed_width(ADJ_ITEM_STATE_SEQ, "seq state:", "uninit");
    data_item_fixed_width(ADJ_ITEM_STATE_Q, "q state:", "stopped");
    data_item_fixed_width(ADJ_ITEM_BPM, "bpm:", "0.0000000");
    data_item_fixed_width(ADJ_ITEM_EVENTS, "events:", "0");
    data_item_fixed_width(ADJ_ITEM_OP, "op:", "init");

    tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
    tui_set_cursor_pos(0, 0);
}

// Symbol to render at a given quarter beat, when the sequencer is not at this point
static char* symbol_off(int q_beat)
{
    if (q_beat % 16 == 0) return "|";
    if (q_beat % 4 == 0) return ":";
    return ".";
}

// Symbol to render at a given quarter beat, when the sequencer is at this point
static char* symbol_on(int q_beat)
{
    if (q_beat % 16 == 0) return "\033[7m|\033[m";  // 7m is negative
    if (q_beat % 4 == 0) return "\033[7m:\033[m";
    return "\033[7m.\033[m";
}

// x position of a given quarter beat
static int q_beat_x(int q_beat)
{
    return (q_beat % 64) + 2;
}

static void clear_message()
{
    if (message_ticks) {
        tui_lock();
        tui_set_cursor_pos(0, 0);
        tui_delete_line();
        tui_unlock();
        message_ticks = 0;
        fflush(stdout);
    }
}



// callbacks

static void init_error(adj_ui_t* ui, char* msg)
{
    tui_error_at(msg, 0, 15);
}


static void data_item_handler(adj_ui_t* ui, int idx, char* name, char* value)
{
    data_item(idx, name, value);
}

static void data_change_handler(adj_ui_t* ui, adj_seq_info_t* adj, int idx, char* value)
{
    if  (idx <= ADJ_ITEM_OP) {
        tui_lock();
        tui_data_at_fixed(value, 10, 15, 14 - idx);
        tui_unlock();
        fflush(stdout);
    }
}

static void message_handler(adj_ui_t* ui, adj_seq_info_t* adj, char* message)
{
    tui_lock();
    message_ticks = 1;
    tui_set_cursor_pos(0, 0);
    tui_delete_line();
    tui_error_at(message, 2, 0);
    tui_unlock();
    fflush(stdout);
}

static void tick_handler(adj_ui_t* ui, adj_seq_info_t* adj, snd_seq_tick_time_t tick)
{
    int quarter_beats = tick == 0 ? 0 : tick / ADJ_CLOCKS_PER_BEAT;

    tui_lock();
    if (quarter_beats > 0) {
        tui_set_cursor_pos(q_beat_x(quarter_beats - 1), 1);
        puts(symbol_off(quarter_beats - 1));
    }
    tui_set_cursor_pos(q_beat_x(quarter_beats), 1);
    puts(symbol_on(quarter_beats));
    tui_unlock();
    fflush(stdout);
}

static void exit_handler(adj_ui_t* ui, int sig)
{
    adj_tui_running = 0;
    tui_lock();
    tui_set_cursor_pos(0, 0);
    puts("\n\n");
    fflush(stdout);
    tui_exit();
    tui_unlock();
}

static void stop_handler(adj_ui_t* ui, adj_seq_info_t* adj)
{
    tui_lock();
    tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
    tui_unlock();
    fflush(stdout);
}

static void start_handler(adj_ui_t* ui, adj_seq_info_t* adj)
{
    //if (tui) tui_text_at("|...:...:...:...|...:...:...:...|...:...:...:...|...:...:...:...|", 2, 1);
}

static void beat_handler(adj_ui_t* ui, adj_seq_info_t* adj, unsigned char player_id)
{

}

static void* run(void* arg)
{
    sleep(2);
    adj_tui_running = 1;
    while (adj_tui_running) {
        usleep(50000);
        if (message_ticks && ++message_ticks > 50) {
            clear_message();
        }
    }
    return NULL;
}

void initialize_tui(adj_ui_t* ui, uint64_t flags)
{
    tui_setup(flags ? 21 : 15);

    ui->init_error_handler = init_error;
    ui->message_handler = message_handler;
    ui->data_item_handler = data_item_handler;
    ui->data_change_handler = data_change_handler;
    ui->tick_handler = tick_handler;
    ui->beat_handler = beat_handler;
    ui->stop_handler = stop_handler;
    ui->start_handler = start_handler;
    ui->exit_handler = exit_handler;
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, &run, NULL);
}