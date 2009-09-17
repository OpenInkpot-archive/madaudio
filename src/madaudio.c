/*
 * MadAudio - audioplayer application.
 *
 * Copyright (C) 2009 by Alexander v. Nikolaev <avn@daemon.hole.ru>
 * Copyright (C) 2008,2009 Mikhail Gusarov <dottedmag@dottedmag.net>
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

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Ecore_Config.h>
#include <Ecore_Con.h>
#include <Ecore_X.h>
#include <Edje.h>
#include <libeoi.h>
#include <libkeys.h>

#include "madaudio.h"

#include "battery.h"
#include "clock.h"


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

static void main_win_resize_handler(Ecore_Evas* main_win)
{
    Evas* canvas = ecore_evas_get(main_win);
    int w, h;
    evas_output_size_get(canvas, &w, &h);

    Evas_Object* main_edje = evas_object_name_find(canvas, "main_edje");
    evas_object_resize(main_edje, w, h);

    Evas_Object* main_menu = evas_object_name_find(canvas, "main_menu");
    if(main_menu)
    {
        evas_object_resize(main_menu, w/2, h);
    }

}

static int update_batt_cb(void* param)
{
    update_battery((Evas_Object*)param);
    return 1;
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

        bool raise = false;

        if(msg->msg[0] == '/') {
            madaudio_play_file(player, msg->msg);
            raise = true;
        } else {
            raise = madaudio_command(player, msg->msg);
        };
        if(raise){
            ecore_evas_show(win);
            ecore_evas_raise(win);
        };
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

        ecore_con_server_send(server, cmd, strlen(cmd));
        ecore_con_server_flush(server);
        ecore_con_server_del(server);

        return true;
    }

    return false;
}

static int sighup_signal_handler(void* data, int type, void* event)
{
    return 1;
}

static void exit_all(void *param)
{
    ecore_main_loop_quit();
}

static int exit_handler(void *param, int ev_type, void *event)
{
   ecore_main_loop_quit();
   return 1;
}

static void exit_app(void* param)
{
    exit(1);
}

int main(int argc, char** argv)
{
    madaudio_player_t* player = calloc(1, sizeof(madaudio_player_t));
    player->retry = 10;

    printf("This is madaudio\n");
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

    ecore_x_io_error_handler_set(exit_app, NULL);

    if(!evas_init())
        err(1, "Unable to initialize Evas");
    if(!ecore_evas_init())
        err(1, "Unable to initialize Ecore_Evas");
    if(!edje_init())
        err(1, "Unable to initialize Edje");
    madaudio_opener_init();

    setlocale(LC_ALL, "");
    textdomain("madaudio");

    /* End of state */

    Ecore_Evas* main_win = ecore_evas_software_x11_new(0, 0, 0, 0, 600, 800);
    player->win = main_win;
    ecore_evas_title_set(main_win, "Madaudio");
    ecore_evas_name_class_set(main_win, "Madaudio", "Madaudio");
    ecore_evas_callback_delete_request_set(main_win, main_win_close_handler);
    ecore_evas_callback_resize_set(main_win, main_win_resize_handler);

    Evas* main_canvas = ecore_evas_get(main_win);
    player->canvas = main_canvas;
    Evas_Object* main_edje = eoi_main_window_create(main_canvas);

    evas_object_name_set(main_edje, "main_edje");

    evas_object_move(main_edje, 0, 0);
    evas_object_resize(main_edje, 600, 800);


    Evas_Object* contents = edje_object_add(main_canvas);
    evas_object_name_set(contents, "player");

    int w, h;
    evas_output_size_get(main_canvas, &w, &h);

    if(h > 600)
        edje_object_file_set(contents, THEMEDIR "/madaudio.edj", "vertical");
    else
        edje_object_file_set(contents, THEMEDIR "/madaudio.edj", "horizontal");

    edje_object_part_swallow(main_edje, "contents", contents);
    evas_object_focus_set(contents, true);

    player->keys = keys_alloc("madaudio");
    evas_object_event_callback_add(contents, EVAS_CALLBACK_KEY_UP,
                                    &madaudio_key_handler, player);

    player->gui = contents;
    evas_object_show(contents);
    evas_object_show(main_edje);
    ecore_evas_show(main_win);

    update_batt_cb(main_edje);
    ecore_timer_add(5*60, &update_batt_cb, main_edje);

    init_clock(main_edje);

    ecore_x_io_error_handler_set(exit_all, NULL);
    ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, exit_handler, NULL);
    ecore_event_handler_add(ECORE_EVENT_SIGNAL_HUP, sighup_signal_handler,
        player);

    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_ADD, _client_add, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _client_data, NULL);
    ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DEL, _client_del, &player);

    madaudio_draw_captions(player);
    if(argc == 2)
        player->filename = strdup(argv[1]);
    madaudio_connect(player);
    ecore_main_loop_begin();

    madaudio_free_state(player);

    madaudio_opener_shutdown();
    edje_shutdown();
    ecore_evas_shutdown();
    evas_shutdown();
    ecore_shutdown();

    return 0;
}

