#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <Eina.h>
#include <Efreet.h>
#include <Efreet_Mime.h>
#include <Ecore_File.h>
#include <mpd/client.h>
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
    Eina_List* next;
    for(next = ls; next; next = eina_list_next(next))
    {
        char* filename;
        char* url;
        asprintf(&filename, "%s/%s", basedir,
            (const char*) eina_list_data_get(next));
        if(madaudio_mime_match(filename))
        {
            asprintf(&url, "file://%s", filename);
            printf("load %s\n",  filename);
            mpd_run_add(player->conn, url);
            if(madaudio_check_error(player))
                printf("Fail to load %s\n",  filename);
        }
        free(filename);
    }
    eina_list_free(ls);
}


static int
madaudio_find_song_by_filename(madaudio_player_t* player, const char* filename)
{
    int track_no = 0;
    Eina_List* next;

    for(next = player->playlist; next; next = eina_list_next(next))
    {
        struct mpd_song* song = eina_list_data_get(next);
        if(!strcmp(mpd_song_get_uri(song), filename))
        {
            track_no = mpd_song_get_pos(song);
            break;
        }
    }
    printf("track_no = %d\n", track_no);
    return track_no;
}

void
madaudio_play_file(madaudio_player_t* player)
{
    syslog(LOG_INFO, "madaudio_play_file(\"%s\")\n", player->filename);
    const char* dir = ecore_file_dir_get(player->filename);
    mpd_run_clear(player->conn);
    madaudio_check_error(player);
    madaudio_load_dir(player, dir);
    madaudio_status(player);
    int track_no = madaudio_find_song_by_filename(player, player->filename);
    madaudio_play(player, track_no);
    madaudio_status(player);
}

