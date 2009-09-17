#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <Eina.h>
#include <Efreet.h>
#include <Efreet_Mime.h>
#include <Ecore_File.h>
#include <mpd/status.h>
#include "empd.h"
#include "madaudio.h"

#define DESKTOP "/usr/share/applications/madaudio.desktop"

static Efreet_Desktop* desktop;

void
madaudio_opener_init()
{
    efreet_mime_init();
    desktop = efreet_desktop_get(DESKTOP);
}

bool
madaudio_mime_match(const char* filename)
{
    const char* probe_type = efreet_mime_type_get(filename);
    Eina_List* j;
    printf("File '%s' is '%s'\n", filename, probe_type);
    for(j = desktop->mime_types; j; j = eina_list_next(j))
    {
        const char* mime_type = (const char*)eina_list_data_get(j);
        if(!strcmp(mime_type, probe_type))
        {
            printf("matched\n");
            return true;
        }
    }
    return false;
}

void
madaudio_opener_shutdown()
{
    efreet_desktop_free(desktop);
    efreet_mime_shutdown();
}

static void
madaudio_load_dir(madaudio_player_t* player, const char* basedir)
{
    Eina_List* ls = ecore_file_ls(basedir);
    /* ls = eina_list_sort(ls, eina_list_count(ls),
                  state->sort == MADSHELF_SORT_NAME ? &_name : &_namerev) */
    Eina_List* playlist = NULL;
    Eina_List* next;
    for(next = ls; next; next = eina_list_next(next))
    {
        char *filename;
        asprintf(&filename, "%s/%s", basedir,
            (const char*) eina_list_data_get(next));
        if(madaudio_mime_match(filename))
            playlist = eina_list_append(playlist, filename);
            /* filenames freed later by libempd */
        else
            free(filename);
    }
    if(playlist)
        empd_enqueue_files(player->conn, playlist);
    else
        printf("Nothing to play\n");
    eina_list_free(ls);
}

static void
madaudio_play_callback(void* data, void* cb_data)
{
    printf("playing\n");
    madaudio_player_t* player = (madaudio_player_t*) cb_data;
    madaudio_polling_start(player);
}

static void
madaudio_got_playlist_callback(void* data, void* cb_data)
{
    printf("playlist synced\n");
    madaudio_player_t* player = (madaudio_player_t*) cb_data;

    int track_no = 0;
    Eina_List* next;

    for(next = player->conn->playlist; next; next = eina_list_next(next))
    {
        struct mpd_song* song = eina_list_data_get(next);
        if(!strcmp(mpd_song_get_tag(song, MPD_TAG_FILENAME, 0),
            player->filename))
            track_no = mpd_song_get_pos(song);
    }

    empd_play(player->conn, madaudio_play_callback, player, track_no);
}

static void
madaudio_playlist_callback(void* data, void* cb_data)
{
    printf("playlist loaded\n");
    madaudio_player_t* player = (madaudio_player_t*) cb_data;
    empd_playlistinfo(player->conn, madaudio_got_playlist_callback, player);
}

static void
madaudio_clear_callback(void* data, void* cb_data)
{
    printf("playlist cleared\n");
    madaudio_player_t* player = (madaudio_player_t*) cb_data;
    const char* dir = ecore_file_dir_get(player->filename);
    empd_callback_set(&player->conn->next_callback, madaudio_playlist_callback,
        player);
    madaudio_load_dir(player, dir);
}

void
madaudio_play_file(madaudio_player_t* player, const char* filename)
{
    if(player->filename)
        free(player->filename);
    player->filename = strdup(filename);
    empd_clear(player->conn, madaudio_clear_callback, player);
}

