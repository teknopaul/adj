/**
 * ADJ module loading functions
 */

#include <dlfcn.h>
//#include <libusb.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "adj.h"

/**
 * code to read /etc/adj/modules.conf and auto laod any modules for 
 * known USB devices
 */

//SNIP SNIP_mod

static int MAX_DEVS = 640;  // should be enough for everone
static char* usb_ids[640];  // NULL terminated list of USB devices


/**
 * iterate all usb devices
 */
static int
find_all_usb_devs()
{
    libusb_device **list;
    struct libusb_device_descriptor desc;
    ssize_t num_devs, i, devi;
    devi = 0;
    usb_ids[devi] = NULL;

    libusb_context *ctx;
    if (libusb_init(&ctx)) {
        fprintf(stderr, "unable to initialize libusb\n");
        return ADJ_ERR;
    }

    num_devs = libusb_get_device_list(ctx, &list);
    if (num_devs < 0) return ADJ_ERR;

    for (i = 0; i < num_devs && i < MAX_DEVS -1; ++i) {
        libusb_device *dev = list[i];
        libusb_get_device_descriptor(dev, &desc);

        char* dev_s = calloc(1, 10);
        snprintf(dev_s, 10, "%04x:%04x", desc.idVendor, desc.idProduct);
        //printf("dev found %s\n", dev_s);
        usb_ids[devi++] = dev_s;
        usb_ids[devi] = NULL;
    }

    return ADJ_OK;
}

static char*
copy(char* value)
{
    char* s =  (char*) calloc(1, strlen(value) + 1);
    if (s == NULL) {
        return NULL;
    }
    strcpy(s, value);
    return s;
}

static char*
trim(char* value)
{
    while ( isspace((unsigned char) value[0]) ) value++;
    int i = strlen(value);
    while ( isspace((unsigned char) value[i]) ) value[i--] = '\0';
    return value;
}

static int
find_device(char* usb_id) 
{
    int i;
    for (i = 0 ; i < MAX_DEVS; i++) {
        if ( usb_ids[i] == NULL ) return 0;
        if ( strncmp(usb_ids[i], usb_id, 10) == 0 ) {
            return 1;
        }
    }
    return 0;
}

static char*
parse_modules_conf(int in)
{
    int rc;
    char  c;
    char line[256];
    char* value = NULL;
    int line_pos = 0;
    int is_name = 1;
    int is_comment = 0;

    for (line_pos = 0 ; line_pos < 256;) {

        rc = read(in, &c, 1);

        if ( rc == 0 ) {
            // EoF
            return NULL;
        }
        else if ( rc < 0 ) {
            fprintf(stderr, "read error\n");
            return NULL;
        }

        if (line_pos == 0 && c == '#') {
            is_comment = 1;
            continue;
        }

        if ( c == '\n' ) {
            if (is_comment == 0 && line_pos > 3) {
                line[line_pos++] = '\0';
                if ( find_device(trim(line)) ) {
                    return copy(trim(value));
                }
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


static char*
select_mod()
{
    int in = open("/etc/adj/modules.conf", O_RDONLY);
    if ( in == -1 ) {
        fprintf(stderr, "cannot open /etc/adj/modules.conf\n");
        return NULL;
    }
    return parse_modules_conf(in);
}

//SNIP SNIP_mod


static void*
adj_load_mod(adj_seq_info_t* adj, char* module, char* args, uint32_t flags)
{
    char mod_path[256];
    snprintf(mod_path, 255, "/usr/lib/adj/adj_%s.so", module);

    void* mod = dlopen(mod_path, RTLD_NOW | RTLD_GLOBAL);
    if (mod == NULL) {
        fprintf(stderr, "loading module failed: %s\n", mod_path);
        return NULL;
    }

    int ( * adj_mod_init)(adj_seq_info_t*, char*, uint32_t);
    void ( * adj_mod_exit)();

    adj_mod_init = (int (*)(adj_seq_info_t*, char*, uint32_t)) dlsym(mod, "adj_mod_init");
    if (adj_mod_init) {
        adj_mod_init(adj, args, flags);
    }
    adj_mod_exit = (void (*)()) dlsym(mod, "adj_mod_exit");
    if (adj_mod_exit) {
        return adj_mod_exit;
    }
    return NULL;
}


/**
 * returns exit function if there is one
 */
void*
adj_mod_autoconfigure(adj_seq_info_t* adj, uint32_t flags)
{
    find_all_usb_devs();
    char *module = select_mod();
    char *args = NULL;
    char *c = NULL;
    if (module) {
        if ( (c = index(module, ' ')) ) {
            *c++ = '\0';
            args = c;
        }
        return adj_load_mod(adj, module, args, flags);
    }
    return NULL;
}

void*
adj_mod_manual_configure(adj_seq_info_t* adj, char *module, uint32_t flags)
{
    return adj_load_mod(adj, module, NULL, flags);
}
