#ifndef _MADAUDIO_H
#define _MADAUDIO_H 1

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <libkeys.h>
#include "empd.h"

typedef struct madaudio_player_t madaudio_player_t;
struct madaudio_player_t {
    Ecore_Evas* win;
    Evas* canvas;
    keys_t* keys;
    Evas_Object* main_edje;
    Evas_Object* gui;
    empd_connection_t* conn;
    Ecore_Timer* poll_timer;
    bool poll_mode;
    char* filename;
    int retry;
    bool mpd_run;
};

void madaudio_play_file(madaudio_player_t*, const char*);
bool madaudio_command(madaudio_player_t*, const char*);

void madaudio_play(madaudio_player_t* player);
void madaudio_pause(madaudio_player_t* player);
void madaudio_play_pause(madaudio_player_t*);

void madaudio_draw_captions(madaudio_player_t*);
void madaudio_draw_song(madaudio_player_t*);


void
madaudio_polling_start(madaudio_player_t* player);
void
madaudio_polling_stop(madaudio_player_t* player);

void madaudio_connect(madaudio_player_t*);
void madaudio_opener_init();
void madaudio_opener_shutdown();

void
madaudio_key_handler(void* param, Evas* e, Evas_Object* o, void* event_info);

#endif
