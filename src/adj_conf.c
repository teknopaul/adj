
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * code to read /etc/adj.conf
 */

#include "adj_conf.h"

static char*
ltrim(char *line)
{
    while (*line == ' ') line++;
    return line;
}

static char*
copy(char* value)
{
    char* s =  (char*) calloc(1, strlen(value));
    if (s == NULL) {
        return NULL;
    }
    strcpy(s, value);
    return s;
}

static void
set(adj_conf* conf, char* name, char* value)
{
    if (value == NULL) return;

    if (strcmp("alsa_name", name) == 0) {
        conf->alsa_name = copy(ltrim(value));
    }
    else if (strcmp("alsa_sync", name) == 0) {
        conf->alsa_sync = ltrim(value)[0] == 't';
    }
    else if (strcmp("keyb_in", name) == 0) {
        conf->keyb_in = ltrim(value)[0] == 't';
    }
    else if (strcmp("numpad_in", name) == 0) {
        conf->numpad_in = ltrim(value)[0] == 't';
    }
    else if (strcmp("midi_in", name) == 0) {
        conf->midi_in = copy(ltrim(value));
    }
    else if (strcmp("midi_out", name) == 0) {
        conf->midi_out = copy(ltrim(value));
    }
    else if (strcmp("bpm", name) == 0) {
        conf->bpm = strtof(ltrim(value), NULL);
    }
    else if (strcmp("vdj", name) == 0) {
        conf->vdj = ltrim(value)[0] == 't';
    }
    else if (strcmp("vdj_iface", name) == 0) {
        conf->vdj_iface = copy(ltrim(value));
    }
    else if (strcmp("vdj_player", name) == 0) {
        conf->vdj_player = atoi(value);
    }
    else if (strcmp("vdj_offset", name) == 0) {
        conf->vdj_offset = atoi(value);
    }
}

static adj_conf*
adj_parse(int in)
{
    int rc;
    char  c;
    char line[256];
    char* value = NULL;
    int line_pos = 0;
    int is_name = 1;
    int is_comment = 0;

    adj_conf* conf = (adj_conf*) calloc(1, sizeof(adj_conf));
    if (conf == NULL) return NULL;

    for (line_pos = 0 ; line_pos < 256;) {

        rc = read(in, &c, 1);

        if ( rc == 0 ) {
            // EoF
            return conf;
        }
        else if ( rc < 0 ) {
            fprintf(stderr, "read error\n");
            free(conf);
            return NULL;
        }

        if (line_pos == 0 && c == '#') {
            is_comment = 1;
            continue;
        }

        if ( c == '\n' ) {
            if (is_comment == 0 && line_pos > 3) {
                line[line_pos++] = '\0';
                set(conf, line, value);
            }
            value = NULL;
            is_name = 1;
            is_comment = 0;
            line_pos = 0;
            continue;
        }

        if (is_comment) {
            continue;
        }

        line[line_pos++] = c;

        if ( is_name && c == ' ' ) {
            line[line_pos - 1] = '\0';
            value = &line[++line_pos];
            is_name = 0;
        }

    }
    return NULL;
}



adj_conf*
adj_conf_init()
{
    int in = open("/etc/adj.conf", O_RDONLY);
    if ( in == -1 ) {
        fprintf(stderr, "cannot open /etc/adj.conf\n");
        return NULL;
    }
    return adj_parse(in);
}