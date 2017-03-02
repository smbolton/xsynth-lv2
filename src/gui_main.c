/* Xsynth DSSI software synthesizer GUI
 *
 * Copyright (C) 2004, 2009, 2010 Sean Bolton and others.
 *
 * Portions of this file may have come from Chris Cannam and Steve
 * Harris's public domain DSSI example code.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <lo/lo.h>
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include "xsynth_types.h"
#include "xsynth.h"
#include "gui_main.h"
#include "gui_callbacks.h"
#include "gui_interface.h"
#include "gui_data.h"

#define XSYNTH_UI_URI  "http://smbolton.com/lv2-plugins/xsynth-lv2#ui"

/* ==== global variables ==== */

char *     osc_host_url;
char *     osc_self_url;
lo_address osc_host_address;
char *     osc_configure_path;
char *     osc_control_path;
char *     osc_exiting_path;
char *     osc_hide_path;
char *     osc_midi_path;
char *     osc_program_path;
char *     osc_quit_path;
char *     osc_rate_path;
char *     osc_show_path;
char *     osc_update_path;

xsynth_patch_t *patches = NULL;
int             patch_section_dirty[4];
char *          project_directory = NULL;

int host_requested_quit = 0;
int gui_test_mode = 0;

/* ==== OSC handling ==== */

static char *
osc_build_path(char *base_path, char *method)
{
    char buffer[256];
    char *full_path;

    snprintf(buffer, 256, "%s%s", base_path, method);
    if (!(full_path = strdup(buffer))) {
        GDB_MESSAGE(GDB_OSC, ": out of memory!\n");
        exit(1);
    }
    return full_path;
}

static void
osc_error(int num, const char *msg, const char *path)
{
    GDB_MESSAGE(GDB_OSC, " error: liblo server error %d in path \"%s\": %s\n",
            num, (path ? path : "(null)"), msg);
}

int
osc_debug_handler(const char *path, const char *types, lo_arg **argv,
                  int argc, lo_message msg, void *user_data)
{
    int i;

    GDB_MESSAGE(GDB_OSC, " warning: unhandled OSC message to <%s>:\n", path);

    for (i = 0; i < argc; ++i) {
        fprintf(stderr, "arg %d: type '%c': ", i, types[i]);
fflush(stderr);
        lo_arg_pp((lo_type)types[i], argv[i]);  /* -FIX- Ack, mixing stderr and stdout... */
        fprintf(stdout, "\n");
fflush(stdout);
    }

    return 1;  /* try any other handlers */
}

int
osc_action_handler(const char *path, const char *types, lo_arg **argv,
                  int argc, lo_message msg, void *user_data)
{
    if (!strcmp(user_data, "show")) {

        /* GDB_MESSAGE(GDB_OSC, " osc_action_handler: received 'show' message\n"); */
        if (!GTK_WIDGET_MAPPED(main_window))
            gtk_widget_show(main_window);
        else
            gdk_window_raise(main_window->window);

    } else if (!strcmp(user_data, "hide")) {

        /* GDB_MESSAGE(GDB_OSC, " osc_action_handler: received 'hide' message\n"); */
        gtk_widget_hide(main_window);

    } else if (!strcmp(user_data, "quit")) {

        /* GDB_MESSAGE(GDB_OSC, " osc_action_handler: received 'quit' message\n"); */
        host_requested_quit = 1;
        gtk_main_quit();

    } else if (!strcmp(user_data, "sample-rate")) {

        /* GDB_MESSAGE(GDB_OSC, " osc_action_handler: received 'sample-rate' message, rate = %d\n", argv[0]->i); */
        /* ignore it */

    } else {

        return osc_debug_handler(path, types, argv, argc, msg, user_data);

    }
    return 0;
}

int
osc_configure_handler(const char *path, const char *types, lo_arg **argv,
                  int argc, lo_message msg, void *user_data)
{
    char *key, *value;

    if (argc < 2) {
        GDB_MESSAGE(GDB_OSC, " error: too few arguments to osc_configure_handler\n");
        return 1;
    }

    key   = &argv[0]->s;
    value = &argv[1]->s;

    if (strlen(key) == 8 && !strncmp(key, "patches", 7) &&
        key[7] >= '0' && key[7] <= '3') {

        update_patches(key, value);

    } else if (!strcmp(key, "polyphony")) {

        update_polyphony(value);

    } else if (!strcmp(key, "monophonic")) {

        update_monophonic(value);

    } else if (!strcmp(key, "glide")) {

        update_glide(value);

    } else if (!strcmp(key, "bendrange")) {

        update_bendrange(value);

    } else if (!strcmp(key, DSSI_PROJECT_DIRECTORY_KEY)) {

        if (project_directory)
            free(project_directory);
        project_directory = strdup(value);

    } else if (!strcmp(key, "load")) {

        display_notice("Warning: your DSSI host sent an obsolete 'load' "
                       "configure message with the filename shown below. This "
                       "probably means you are trying to use a session "
                       "created with Xsynth-DSSI version 0.1.x, which is not "
                       "compatible with Xsynth-DSSI version 0.9 and later "
                       "plugins. See the README file for more information.",
                       value);

    } else {

        return osc_debug_handler(path, types, argv, argc, msg, user_data);

    }

    return 0;
}

int
osc_control_handler(const char *path, const char *types, lo_arg **argv,
                  int argc, lo_message msg, void *user_data)
{
    int port;
    float value;

    if (argc < 2) {
        GDB_MESSAGE(GDB_OSC, " error: too few arguments to osc_control_handler\n");
        return 1;
    }

    port = argv[0]->i;
    value = argv[1]->f;

    GDB_MESSAGE(GDB_OSC, " osc_control_handler: control %d now %f\n", port, value);

    update_voice_widget(port, value);

    return 0;
}

int
osc_program_handler(const char *path, const char *types, lo_arg **argv,
                  int argc, lo_message msg, void *user_data)
{
    int bank, program;

    if (argc < 2) {
        GDB_MESSAGE(GDB_OSC, " error: too few arguments to osc_program_handler\n");
        return 1;
    }

    bank = argv[0]->i;
    program = argv[1]->i;

    if (bank || program < 0 || program >= 128) {
        GDB_MESSAGE(GDB_OSC, ": out-of-range program select (bank %d, program %d)\n", bank, program);
        return 0;
    }

    GDB_MESSAGE(GDB_OSC, " osc_program_handler: received program change, bank %d, program %d\n", bank, program);

    update_from_program_select(bank, program);

    return 0;
}

void
osc_data_on_socket_callback(gpointer data, gint source,
                            GdkInputCondition condition)
{
    lo_server server = (lo_server)data;

    lo_server_recv_noblock(server, 0);
}

gint
update_request_timeout_callback(gpointer data)
{
    if (!gui_test_mode) {

        /* send our update request */
        lo_send(osc_host_address, osc_update_path, "s", osc_self_url);

    } else {

        gtk_widget_show(main_window);

    }

    return FALSE;  /* don't need to do this again */
}

/* ==== main ==== */


int
main(int argc, char *argv[])
{
    char *test_argv[5] = { NULL, NULL, "-", "-", "Xsynth-DSSI" };
    char *host, *port, *path, *tmp_url;
    lo_server osc_server;
    gint osc_server_socket_tag;
    gint update_request_timeout_tag;

    XSYNTH_DEBUG_INIT("Xsynth_gtk");

#ifdef XSYNTH_DEBUG
    GDB_MESSAGE(GDB_MAIN, " starting (pid %d)...\n", getpid());
#else
    fprintf(stderr, "Xsynth_gtk starting (pid %d)...\n", getpid());
#endif
    /* { int i; fprintf(stderr, "args:\n"); for(i=0; i<argc; i++) printf("%d: %s\n", i, argv[i]); } // debug */

    gtk_init(&argc, &argv);

    if (argc > 1 && !strcmp(argv[1], "-test")) {
        gui_test_mode = 1;
        test_argv[0] = argv[0];
        test_argv[1] = "osc.udp://localhost:9/test/mode";
        if (argc >= 5)
            test_argv[4] = argv[4];
        argc = 5;
        argv = test_argv;
    } else if (argc != 5) {
        fprintf(stderr, "usage: %s <osc url> <plugin dllname> <plugin label> <user-friendly id>\n"
                        "   or: %s -test\n", argv[0], argv[0]);
        exit(1);
    }

    /* set up OSC support */
    osc_host_url = argv[1];
    host = lo_url_get_hostname(osc_host_url);
    port = lo_url_get_port(osc_host_url);
    path = lo_url_get_path(osc_host_url);
    osc_host_address = lo_address_new(host, port);
    osc_configure_path = osc_build_path(path, "/configure");
    osc_control_path   = osc_build_path(path, "/control");
    osc_exiting_path   = osc_build_path(path, "/exiting");
    osc_hide_path      = osc_build_path(path, "/hide");
    osc_midi_path      = osc_build_path(path, "/midi");
    osc_program_path   = osc_build_path(path, "/program");
    osc_quit_path      = osc_build_path(path, "/quit");
    osc_rate_path      = osc_build_path(path, "/sample-rate");
    osc_show_path      = osc_build_path(path, "/show");
    osc_update_path    = osc_build_path(path, "/update");

    osc_server = lo_server_new(NULL, osc_error);
    lo_server_add_method(osc_server, osc_configure_path, "ss", osc_configure_handler, NULL);
    lo_server_add_method(osc_server, osc_control_path, "if", osc_control_handler, NULL);
    lo_server_add_method(osc_server, osc_hide_path, "", osc_action_handler, "hide");
    lo_server_add_method(osc_server, osc_program_path, "ii", osc_program_handler, NULL);
    lo_server_add_method(osc_server, osc_quit_path, "", osc_action_handler, "quit");
    lo_server_add_method(osc_server, osc_rate_path, "i", osc_action_handler, "sample-rate");
    lo_server_add_method(osc_server, osc_show_path, "", osc_action_handler, "show");
    lo_server_add_method(osc_server, NULL, NULL, osc_debug_handler, NULL);

    tmp_url = lo_server_get_url(osc_server);
    osc_self_url = osc_build_path(tmp_url, (strlen(path) > 1 ? path + 1 : path));
    free(tmp_url);

    /* set up GTK+ */
    create_windows(argv[4]);

    /* add OSC server socket to GTK+'s watched I/O */
    if (lo_server_get_socket_fd(osc_server) < 0) {
        fprintf(stderr, "Xsynth_gtk fatal: OSC transport does not support exposing socket fd\n");
        exit(1);
    }
    osc_server_socket_tag = gdk_input_add(lo_server_get_socket_fd(osc_server),
                                          GDK_INPUT_READ,
                                          osc_data_on_socket_callback,
                                          osc_server);

    /* default patches, temporary patchfile support */
    gui_data_friendly_patches();
    rebuild_patches_clist();

    /* schedule our update request */
    update_request_timeout_tag = gtk_timeout_add(50,
                                                 update_request_timeout_callback,
                                                 NULL);

    /* let GTK+ take it from here */
    gtk_main();

    /* clean up and exit */
    GDB_MESSAGE(GDB_MAIN, ": yep, we got to the cleanup!\n");

    /* GTK+ cleanup */
    gtk_timeout_remove(update_request_timeout_tag);
    gdk_input_remove(osc_server_socket_tag);

    /* say bye-bye */
    if (!host_requested_quit) {
        lo_send(osc_host_address, osc_exiting_path, "");
    }

    /* clean up patches */
    if (patches) free(patches);
    if (project_directory) free(project_directory);

    /* clean up OSC support */
    lo_server_free(osc_server);
    free(host);
    free(port);
    free(path);
    free(osc_configure_path);
    free(osc_control_path);
    free(osc_exiting_path);
    free(osc_hide_path);
    free(osc_midi_path);
    free(osc_program_path);
    free(osc_quit_path);
    free(osc_rate_path);
    free(osc_show_path);
    free(osc_update_path);
    free(osc_self_url);

    return 0;
}

/* ==== !FIX! LV2 stuff ==== */

static LV2UI_Handle
instantiate(const LV2UI_Descriptor *descriptor,
            const char *plugin_uri,
            const char *bundle_path,
            LV2UI_Write_Function write_function,
            LV2UI_Controller controller,
            LV2UI_Widget *widget,
            const LV2_Feature *const *features)
{
    // !FIX! save write_function, controller, (what else?)
    int i;
    LV2_URID_Map *map = NULL;
    xsynth_ui_t *ui;

    for (i = 0; features[i]; i++) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (LV2_URID_Map *)features[i]->data;
            break;
        }
    }
    if (!map) return NULL;

    ui = (xsynth_ui_t *)calloc(1, sizeof(xsynth_ui_t));
    if (!ui) return NULL;

    // uris->atom_Float = map->map(map->handle, LV2_ATOM__Float);

    *widget = gtk_label_new("Yes!");

    return ui;
}

static void
cleanup(LV2UI_Handle uih)
{
    xsynth_ui_t *ui = (xsynth_ui_t *)uih;
}

static void
port_event(LV2UI_Handle uih,
           uint32_t port_index,
           uint32_t buffer_size,
           uint32_t format,
           const void *buffer)
{
    xsynth_ui_t *ui = (xsynth_ui_t *)uih;
}

static LV2UI_Descriptor descriptor = {
    XSYNTH_UI_URI, instantiate, cleanup, port_event, NULL
};

const LV2UI_Descriptor *lv2ui_descriptor(uint32_t index)
{
    switch (index) {
      case 0:
        return &descriptor;
      default:
        return NULL;
    }
}
