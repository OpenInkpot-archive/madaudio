/*
 * MadAudio - audioplayer application.
 *
 * Copyright (C) 2009, 2010 by Alexander V. Nikolaev <avn@daemon.hole.ru>
 * Copyright (C) 2008, 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include <err.h>
#include <libintl.h>
#include <locale.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Ecore_Config.h>
#include <Ecore_Con.h>
#include <Ecore_X.h>
#include <Edje.h>
#include <libeoi.h>
#include <libeoi_themes.h>
#include <libeoi_battery.h>
#include <libeoi_clock.h>
#include <libkeys.h>

#include "madaudio.h"

//#define DEBUG 1


/* FIXME */

static void madaudio_free_state(madaudio_player_t* player)
{
    keys_free(player->keys);
    free(player);
}




static void main_win_close_handler(Ecore_Evas* main_win)
{
    ecore_main_loop_quit();
}


typedef struct
{
    char* msg;
    int size;
} client_data_t;

static int _client_add(void* param, int ev_type, void* ev)
{
    Ecore_Con_Event_Client_Add* e = ev;
    client_data_t* msg = malloc(sizeof(client_data_t));
    msg->msg = strdup("");
    msg->size = 0;
    ecore_con_client_data_set(e->client, msg);
    return 0;
}

static int _client_del(void* param, int ev_type, void* ev)
{
    Ecore_Con_Event_Client_Del* e = ev;
    client_data_t* msg = ecore_con_client_data_get(e->client);

    /* Handle */

    if(!msg->msg[0])
    {
        /* Skip it: ecore-con internal bug */
    }
    else
    {
        madaudio_player_t* player = (madaudio_player_t*)param;
        Ecore_Evas* win = player->win;

        char *cmd = strndup(msg->msg, msg->size);
        if(cmd[0] == '/') {
            free(player->filename);
            player->filename = strdup(cmd);
            madaudio_play_file(player);
        }
        else
            madaudio_action(player, cmd);
        free(cmd);
        ecore_evas_show(win);
        ecore_evas_raise(win);
    }

    free(msg->msg);
    free(msg);
    return 0;
}

static int _client_data(void* param, int ev_type, void* ev)
{
    Ecore_Con_Event_Client_Data* e = ev;
    client_data_t* msg = ecore_con_client_data_get(e->client);
    msg->msg = realloc(msg->msg, msg->size + e->size);
    memcpy(msg->msg + msg->size, e->data, e->size);
    msg->size += e->size;
    return 0;
}

static bool check_running_instance(const char* cmd)
{
    Ecore_Con_Server* server
        = ecore_con_server_add(ECORE_CON_LOCAL_USER, "madaudio-singleton", 0, NULL);

    if(!server)
    {
        /* Somebody already listens there */
        server = ecore_con_server_connect(ECORE_CON_LOCAL_USER,
                                          "madaudio-singleton", 0, NULL);

        if(!server)
            return false;

        syslog(LOG_INFO, "send %s to running copy\n");
        ecore_con_server_send(server, cmd, strlen(cmd));
        ecore_con_server_flush(server);
        ecore_con_server_del(server);

        return true;
    }

    return false;
}

static int sighup_signal_handler(void* data, int type, void* event)
{
    madaudio_stop_record((madaudio_player_t* ) data);
    return 1;
}

static void exit_all(void *param)
{
    madaudio_stop_record((madaudio_player_t* ) param);
    ecore_main_loop_quit();
}

static int exit_handler(void *param, int ev_type, void *event)
{
   madaudio_stop_record((madaudio_player_t* ) param);
   ecore_main_loop_quit();
   return 1;
}

static void exit_app(void* param)
{
    madaudio_stop_record((madaudio_player_t* ) param);
    exit(1);
}


static const char *
player_orientation(int w, int h)
{
    if(w > h)
        return "horizontal";
    else
        return "vertical";
}


static void player_edje_resize(Ecore_Evas *ee __attribute__((unused)),
                               Evas_Object *edje,
                               int w, int h,
                               void *param __attribute__((unused)))
{
    madaudio_player_t* player = (madaudio_player_t *) param;
    const char *file;
    const char *collection;
    const char *replacement = player_orientation(w, h);
    edje_object_file_get(edje, &file, &collection);
    if(!strcmp(collection, replacement))
        return;
    edje_object_file_set(edje, file, replacement);
    madaudio_draw_song(player);
}

static void main_edje_resize(Ecore_Evas *ee __attribute__((unused)),
                             Evas_Object *main_edje,
                             int w, int h,
                             void *param __attribute__((unused)))
{
    if (w > h)
        eoi_main_window_footer_hide(main_edje);
    else
        eoi_main_window_footer_show(main_edje);

}

#ifdef DEBUG
static void
_emission(void *data, Evas_Object *o, const char *emission, const char *source)
{
    printf("Emission: %s, source: %s\n", emission, source);
}
#endif

static Evas_Object *
player_edje(Evas *canvas, int w, int h, madaudio_player_t* player)
{
    Evas_Object* contents = eoi_create_themed_edje(canvas, "madaudio",
                                player_orientation(w, h));
    evas_object_name_set(contents, "player");

    evas_object_focus_set(contents, true);

    evas_object_event_callback_add(contents, EVAS_CALLBACK_KEY_UP,
                                    &madaudio_key_handler, player);

    player->gui = contents;
    madaudio_init_meter(contents);
    evas_object_show(contents);
    return contents;
}


int main(int argc, char** argv)
{
    static madaudio_player_t* player;
    player = calloc(1, sizeof(madaudio_player_t));

    player->retry = 10;
    player->context = "player";

    int flags = LOG_NDELAY | LOG_PID;
    if (getenv("MADAUDIO_DEBUG"))
        flags |= LOG_PERROR;

    openlog("madaudio", flags, LOG_USER);

    if(!ecore_init())
        err(1, "Unable to initialize Ecore");

    char* cmd;
    if(argc == 1)
        cmd = "raise";
    else
        cmd = argv[1];

    if(check_running_instance(cmd))
    {
        ecore_con_shutdown();
        ecore_shutdown();
        return 0;
    }

    ecore_x_io_error_handler_set(exit_app, player);

    if(!evas_init())
        err(1, "Unable to initialize Evas");
    if(!ecore_evas_init())
        err(1, "Unable to initialize Ecore_Evas");
    if(!edje_init())
        err(1, "Unable to initialize Edje");
    if(!ecore_con_init())
        err(1, "Unable to initialize Ecore Con");
    madaudio_opener_init();

    setlocale(LC_ALL, "");
    textdomain("madaudio");

    player->keys = keys_alloc("madaudio");
    /* End of state */

    Ecore_Evas* main_win = ecore_evas_software_x11_new(0, 0, 0, 0, 600, 800);
    player->win = main_win;
    ecore_evas_title_set(main_win, "Madaudio");
    ecore_evas_name_class_set(main_win, "Madaudio", "Madaudio");
    ecore_evas_callback_delete_request_set(main_win, main_win_close_handler);

    Evas* main_canvas = ecore_evas_get(main_win);
    player->canvas = main_canvas;

    int w, h;
    evas_output_size_get(main_canvas, &w, &h);
    Evas_Object* main_edje = eoi_main_window_create(main_canvas);
#ifdef DEBUG
//    edje_object_signal_callback_add(main_edje, "*", "*", _emission, NULL);
#endif
    evas_object_name_set(main_edje, "main_edje");
    evas_object_move(main_edje, 0, 0);
    evas_object_resize(main_edje, w, h);
    eoi_fullwindow_object_register(main_win, main_edje);
    eoi_resize_object_register(main_win, main_edje, main_edje_resize, NULL);

    Evas_Object *contents = player_edje(main_canvas, w, h, player);
    edje_object_part_swallow(main_edje, "contents", contents);
    eoi_resize_object_register(main_win, contents, player_edje_resize, player);
#ifdef DEBUG
    edje_object_signal_callback_add(contents, "*", "*", _emission, NULL);
#endif

    evas_object_show(main_edje);
    eoi_run_clock(main_edje);
    eoi_run_battery(main_edje);
    ecore_evas_show(main_win);


    ecore_x_io_error_handler_set(exit_all, player);
    ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, exit_handler, player);
    ecore_event_handler_add(ECORE_EVENT_SIGNAL_HUP, sighup_signal_handler,
        player);

    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_ADD, _client_add, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _client_data, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DEL, _client_del, player);

    madaudio_draw_captions(player);
    if(argc == 2 && argv[1][0] == '/')
        player->filename = strdup(argv[1]);
    else
        player->filename = NULL;
    madaudio_connect(player);
    ecore_main_loop_begin();

    madaudio_free_state(player);

    madaudio_opener_shutdown();
    ecore_evas_shutdown();
    evas_shutdown();
    edje_shutdown();
    ecore_shutdown();
    closelog();

    return 0;
}

