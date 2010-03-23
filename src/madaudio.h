#ifndef _MADAUDIO_H
#define _MADAUDIO_H 1

#include <Eina.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <libkeys.h>
#include <mpd/connection.h>

typedef struct madaudio_player_t madaudio_player_t;
struct madaudio_player_t {
    Ecore_Evas* win;
    Evas* canvas;
    keys_t* keys;
    Evas_Object* main_edje;
    Evas_Object* gui;
    struct mpd_connection* conn;
    struct mpd_status* status;
    Eina_List* playlist;
    unsigned long long  playlist_stamp;
    Ecore_Timer* poll_timer;
    bool poll_mode;
    char* filename;
    int retry;
    bool mpd_run;

    bool extended_controls;
    const char *context;

    Ecore_Exe *recorder;
    Ecore_Event_Handler *recorder_handler;
    Ecore_Timer *recorder_timer;
    time_t *recorder_start_time;
    time_t *recorder_current_time;
};


bool
madaudio_check_error(madaudio_player_t* player);

void madaudio_play_file(madaudio_player_t*);
bool madaudio_command(madaudio_player_t*, const char*);

void madaudio_status(madaudio_player_t* player);
void madaudio_play(madaudio_player_t* player, int track);
void madaudio_pause(madaudio_player_t* player);
void madaudio_play_pause(madaudio_player_t*);

void madaudio_draw_captions(madaudio_player_t*);
void madaudio_draw_song(madaudio_player_t*);

void madaudio_start_record(madaudio_player_t*);
void madaudio_stop_record(madaudio_player_t*);
void madaudio_draw_recorder_start(madaudio_player_t *);
void madaudio_draw_recorder_stop(madaudio_player_t *);

void madaudio_recorder_folder(madaudio_player_t *);

void
madaudio_polling_start(madaudio_player_t* player);
void
madaudio_polling_stop(madaudio_player_t* player);

void madaudio_connect(madaudio_player_t*);
void madaudio_opener_init();
void madaudio_opener_shutdown();

void madaudio_update_meter(Evas_Object* gui, int value);
void madaudio_init_meter(Evas_Object* gui);

void
madaudio_key_handler(void* param, Evas* e, Evas_Object* o, void* event_info);

#endif
