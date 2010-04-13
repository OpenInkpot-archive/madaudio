#define _GNU_SOURCE 1
#include <assert.h>
#include <libintl.h>
#include <err.h>
#include <string.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <mpd/client.h>
#include <mpd/error.h>
#include <libeoi_help.h>
#include "madaudio.h"

#define MADAUDIO_SOCKET "/tmp/madaudio-mpd.socket"

#define MADAUDIO_CHECK_ERROR(conn) \
    if(madaudio_check_error(conn)) \
        return;

#define INVERT(x)  ((x) ? false : true)

bool
madaudio_check_error(madaudio_player_t* player)
{
//    mpd_response_finish(player->conn);
    if (mpd_connection_get_error(player->conn)==MPD_ERROR_SUCCESS)
        return false;

    printf("Error code: %d\n", mpd_connection_get_error(player->conn));
    if(mpd_connection_get_error(player->conn) == MPD_ERROR_SERVER)
        printf("mpd: ack = %d\n",
            mpd_connection_get_server_error(player->conn));

    printf("mpd error: %s\n", mpd_connection_get_error_message(player->conn));
    if(!mpd_connection_clear_error(player->conn))
        err(1, "Impossible to recover\n");
    return true;
}

static int
reconnect_callback(void* data)
{
    madaudio_player_t* player = (madaudio_player_t *) data;
    madaudio_connect(player);
    return 0;
}


static int
poll_callback(void* data)
{
    madaudio_player_t* player = (madaudio_player_t *) data;
    madaudio_status(player);
    return 1;
}

void
madaudio_polling_stop(madaudio_player_t* player)
{
    if(player->poll_mode)
    {
        player->poll_mode = false;
        ecore_timer_del(player->poll_timer);
        printf("Stop polling\n");
    }
}

void
madaudio_polling_start(madaudio_player_t* player)
{
    if(player->poll_mode)
        return;
    printf("Start polling\n");
    player->poll_timer = ecore_timer_loop_add(5.0, poll_callback, player);
    player->poll_mode = true;
    poll_callback(player);
}

void
madaudio_connect(madaudio_player_t* player)
{
    struct mpd_connection *conn;
    conn = mpd_connection_new(MADAUDIO_SOCKET, 0, 0);
    if(conn && mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
    {
        player->conn = conn;
        if(madaudio_check_error(player))
            err(1, "mpd not ready or has wrong version\n");
        madaudio_status(player);
        if(player->filename)
            madaudio_play_file(player);
        else
        {
            madaudio_status(player);
            madaudio_polling_start(player);
        }
        return;
    }

    if(--player->retry)
    {
        if(!player->mpd_run)
        {
            player->mpd_run=true;
            Ecore_Exe* exe;
            printf("spawing mpd\n");
            exe = ecore_exe_run("/usr/bin/mpd /etc/madaudio/mpd.conf", NULL);
            if(exe)
                ecore_exe_free(exe);
            printf("spawing madaudio-unsuspend\n");
            exe = ecore_exe_run("/usr/bin/madaudio-unsuspend", NULL);
            if(exe)
                ecore_exe_free(exe);
        }
        ecore_timer_add(1.0, reconnect_callback, player);
    }
    else
    {
        printf("Can't wake up mpd, exitting\n");
        ecore_main_loop_quit();
    }
}
static void
kill_old_playlist(Eina_List *playlist)
{
    struct mpd_song * old;
    EINA_LIST_FREE(playlist, old)
        mpd_song_free(old);
}

static Eina_List *
get_playlist(madaudio_player_t *player)
{
    Eina_List *playlist = NULL;
    mpd_send_command(player->conn, "playlistinfo", NULL);
    struct mpd_entity* entity;
    while((entity = mpd_recv_entity(player->conn)) != NULL)
    {
        if(mpd_entity_get_type(entity) != MPD_ENTITY_TYPE_SONG)
        {
            printf("protocol: Out of sync: %d\n", mpd_entity_get_type(entity));
            mpd_entity_free(entity);
            return playlist;
        }
        const struct mpd_song * song;
        song = mpd_song_dup(mpd_entity_get_song(entity));
        playlist = eina_list_append(playlist, song);
        mpd_entity_free(entity);
    }
    mpd_response_finish(player->conn);
    return playlist;
}

static bool
cleanup_undeads(madaudio_player_t *player, Eina_List *playlist)
{
    Eina_List *tmp;
    struct mpd_song *song;
    bool modified = false;
    EINA_LIST_REVERSE_FOREACH(playlist, tmp, song)
    {
        const char *uri = mpd_song_get_uri(song);
        if(uri[0] == '/')
        {
            if(!ecore_file_exists(uri))
            {
                fprintf(stderr, "Undead: %s\n", uri);
                int pos = mpd_song_get_pos(song);
                mpd_run_delete(player->conn, pos);
            }
        }
    }
    return modified;
}

void
madaudio_status(madaudio_player_t *player)
{
    assert(player);
    struct mpd_status *status = mpd_run_status(player->conn);
    if(!status)
        MADAUDIO_CHECK_ERROR(player);
    if(player->status)
        mpd_status_free(player->status);
    player->status = status;
    if(player->playlist_stamp != mpd_status_get_queue_version(status))
    {
        kill_old_playlist(player->playlist);
        Eina_List *playlist = get_playlist(player);
        MADAUDIO_CHECK_ERROR(player);
        if(cleanup_undeads(player, playlist))
        {
            // playlist modified, refetch
            kill_old_playlist(playlist);
            playlist = get_playlist(player);
            MADAUDIO_CHECK_ERROR(player);
        }
        player->playlist = playlist;
    }
    madaudio_draw_song(player);
}

void
madaudio_pause(madaudio_player_t* player)
{
    mpd_run_pause(player->conn, true);
    MADAUDIO_CHECK_ERROR(player);
}

void
madaudio_stop(madaudio_player_t *player)
{
    mpd_run_stop(player->conn);
    MADAUDIO_CHECK_ERROR(player);
}

void
madaudio_play(madaudio_player_t* player, int track)
{
    /* be sure to stop recorder, to avoid races */
    madaudio_stop_record(player);
    madaudio_polling_start(player);
    mpd_run_play_pos(player->conn, track);
    MADAUDIO_CHECK_ERROR(player);
}

void
madaudio_play_pause(madaudio_player_t* player)
{
    if(player->recorder)
    {
        madaudio_stop_record(player);
        return;
    }
    if(mpd_status_get_state(player->status) == MPD_STATE_STOP &&
        player->playlist)
    {
        mpd_run_play_pos(player->conn, 0);
        return;
    }
    mpd_run_toggle_pause(player->conn);
    MADAUDIO_CHECK_ERROR(player);
}


void
madaudio_prev(madaudio_player_t* player)
{
    if(mpd_status_get_state(player->status) == MPD_STATE_STOP &&
        player->playlist)
    {
        mpd_run_play_pos(player->conn, eina_list_count(player->playlist)-1);
        return;
    }
    if((mpd_status_get_song_pos(player->status) > 0 ) ||
        mpd_status_get_repeat(player->status));
    {
        mpd_run_previous(player->conn);
        MADAUDIO_CHECK_ERROR(player);
    }
}

void
madaudio_next(madaudio_player_t* player)
{
    if((mpd_status_get_song_pos(player->status)
        < mpd_status_get_queue_length(player->status)-1)
        || mpd_status_get_repeat(player->status))
    {
        mpd_run_next(player->conn);
        MADAUDIO_CHECK_ERROR(player);
    }
}

void
madaudio_cycle(madaudio_player_t* player)
{
    int current = mpd_status_get_repeat(player->status);
    mpd_run_repeat(player->conn, INVERT(current));
    MADAUDIO_CHECK_ERROR(player);
}

void
madaudio_single(madaudio_player_t* player)
{
    int current = mpd_status_get_single(player->status);
    mpd_run_single(player->conn, INVERT(current));
    MADAUDIO_CHECK_ERROR(player);
}

static void
madaudio_seek(madaudio_player_t* player, int offset)
{
    int current = mpd_status_get_elapsed_time(player->status);
    int total = mpd_status_get_total_time(player->status);
    current += offset;
    int pos = mpd_status_get_song_pos(player->status);
    if(current >= 0 && current <= total)
    {
        mpd_run_seek_pos(player->conn,  pos, current);
        MADAUDIO_CHECK_ERROR(player);
        madaudio_status(player);
    }
    else
        if( current < 0)
            madaudio_prev(player);
        else
            madaudio_next(player);
}

static void
madaudio_volume(madaudio_player_t* player, int offset)
{
    int volume = mpd_status_get_volume(player->status);
    volume += offset;
    if(volume >= 0 && volume <= 100)
    {
        mpd_run_set_volume(player->conn, volume);
        MADAUDIO_CHECK_ERROR(player);
    }
}


static void
madaudio_close_recorder(madaudio_player_t *player)
{
    player->context = "player";
    edje_object_signal_emit(player->gui, "hide-recorder-controls", "");
}

static void
madaudio_replay_file(madaudio_player_t *player)
{
    if(player->recorder)
        madaudio_stop_record(player);
    madaudio_close_recorder(player);
    madaudio_play_file(player);
}


static void
madaudio_action_internal(Evas *e, madaudio_player_t *player, const char *action)
{

    if(!action)
        return;
    if(!strcmp(action, "Quit"))
    {
        madaudio_stop_record(player);
        ecore_main_loop_quit();
    }

    /* all commands except Quit require conn and conn->status */
    if(!player->conn || !player->status)
    {
        printf("Not connected\n");
        return;
    }

    if(!strcmp(action, "PlayPause"))
        madaudio_play_pause(player);

    /* Recorder */
    if(!strcmp(action, "StopRecording"))
        madaudio_stop_record(player);

    if(!strcmp(action, "RecorderFolder"))
        madaudio_recorder_folder(player);

    if(!strcmp(action, "RecorderStart"))
        madaudio_start_record(player);

    /* prev/next */
    if(!strcmp(action, "Previous"))
        madaudio_prev(player);
    if(!strcmp(action, "Next"))
        madaudio_next(player);

    /* volume */
    if(!strcmp(action, "VolumeUp"))
        madaudio_volume(player, 10);
    if(!strcmp(action, "VolumeDown"))
        madaudio_volume(player, -10);

    /* Seek fw/nw 10 sec */
    if(!strcmp(action, "Forward"))
        madaudio_seek(player, 10);
    if(!strcmp(action, "Backward"))
        madaudio_seek(player, -10);
    /* Seek fw/nw 1min  */
    if(!strcmp(action, "Forward_1m"))
        madaudio_seek(player, 60);
    if(!strcmp(action, "Backward_1m"))
        madaudio_seek(player, -60);
    /* Seek fw/nw 10 min */
    if(!strcmp(action, "Forward_10m"))
        madaudio_seek(player, 600);
    if(!strcmp(action, "Backward_10m"))
        madaudio_seek(player, -600);

    /* Repeat and cycling */
    if(!strcmp(action, "Cycle"))
        madaudio_cycle(player);
    if(!strcmp(action, "Single"))
        madaudio_single(player);
    if(!strcmp(action, "Help"))
        eoi_help_show(e, "madaudio", "index",
            gettext("Madaudio: Help"),
            player->keys, "player");

    if(!strcmp(action, "RecorderReplay"))
        madaudio_replay_file(player);

    if(!strcmp(action,"ToggleExtendedControls"))
    {
        if(player->extended_controls)
        {
            player->extended_controls = false;
            edje_object_signal_emit(player->gui, "hide-extended-controls", "");
        }
        else
        {
            player->extended_controls = true;
            edje_object_signal_emit(player->gui, "show-extended-controls", "");
        }
    }

    if(!strcmp(action, "RecorderDialogOpen"))
    {
        player->context = "recorder";
        madaudio_draw_recorder_window(player);
        edje_object_signal_emit(player->gui, "show-recorder-controls", "");
    }
    if(!strcmp(action, "RecorderDialogClose"))
        madaudio_close_recorder(player);

    madaudio_status(player);
}

void
madaudio_key_handler(void* param, Evas* e, Evas_Object* o, void* event_info)
{
    madaudio_player_t* player = (madaudio_player_t*)param;
    Evas_Event_Key_Up* ev = (Evas_Event_Key_Up*)event_info;
    const char* action = keys_lookup_by_event(player->keys,
                                              player->context, ev);
    madaudio_action_internal(e, player, action);
}

void
madaudio_action(madaudio_player_t *player, const char *key)
{
    const char *action = keys_lookup(player->keys, player->context, key);
    Evas *e = evas_object_evas_get(player->gui);
    printf("Action: %s -> %s\n", key, action);
    madaudio_action_internal(e, player, action);
}
