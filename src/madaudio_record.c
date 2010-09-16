#define _GNU_SOURCE 1
#include <sys/statvfs.h>
#include <limits.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <Ecore.h>
#include <Ecore_File.h>
#include <Efreet.h>
#include <liblops.h>
#include <mpd/client.h>
#include <libeoi_utils.h>

#include "madaudio.h"

#define FILENAME_LEN 512
#define DEFAULT_CODEC "hq"
#define DEFAULT_PATH "/mnt/storage/dictophone"
#define MADAUDIO_RECORDER_SECTION "recorder"
#define MADAUDIO_INI "/etc/madaudio.ini"

static char *
madaudio_strftime(const char *template)
{
    static char filename[FILENAME_LEN];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);
    strftime(filename, FILENAME_LEN, template, tmp);
    return filename;
}

static Eina_Bool
madaudio_callback(void *data, int type __attribute__((unused)),
    void *event __attribute__((unused)))
{
    struct madaudio_player_t *player = data;
    ecore_event_handler_del(player->recorder_handler);
    player->recorder_handler = NULL;
    player->recorder = NULL;
    printf("Recorder stopped\n");
    player->context = "recorder";
    madaudio_draw_recorder_stop(player);
    return ECORE_CALLBACK_CANCEL;
}

static void
_efreet_exec_cb(void *data, Efreet_Desktop *desktop __attribute__((unused)),
    char *cmdline, int remaining __attribute__((unused)))
{

    madaudio_player_t *player = (madaudio_player_t *) data;
    player->recorder_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                       madaudio_callback,
                                                       player);
    /* FIXME: chpst don't work from .desktop files */
    char *newcmdline = xasprintf("chpst -P %s", cmdline);
    printf("Run: %s\n", newcmdline);
    player->recorder = ecore_exe_run(cmdline, data);
    madaudio_draw_recorder_start(player);
    free(newcmdline);
}

void
madaudio_start_record(madaudio_player_t *player)
{
    if(player->recorder)
    {
        printf("Already recording...\n");
        return;
    }
    if(player->status == MPD_STATE_PLAY)
        madaudio_stop(player);

    player->context = "recording";

    if(!madaudio_ensure_dir(player->config->path))
        return;

    printf("Recording...\n");
    char *file_ext = efreet_desktop_x_field_get(player->config->codec,
        "X-FileExtension");
    if(!file_ext)
        file_ext="wav"; /* fallback */
    char *fullname = xasprintf("%s/%s.%s", player->config->path,
        madaudio_strftime(player->config->template), file_ext);
    free(player->filename);
    player->filename = fullname;

    efreet_desktop_command_get(player->config->codec,
        eina_list_append(NULL, fullname),
       _efreet_exec_cb,
       player);
}


void
madaudio_stop_record(madaudio_player_t *player)
{
    if(player->recorder)
    {
        printf("Sending SIGTERM to recorder\n");
//        ecore_exe_terminate(player->recorder);
        int pid = ecore_exe_pid_get(player->recorder);
        if(pid > 1)
        {
            printf("Signalling %d\n", pid);
            kill(-pid, SIGINT);
        }
    }
}

void
madaudio_recorder_folder(madaudio_player_t *player)
{
    char *path = DEFAULT_PATH;
    if(!madaudio_ensure_dir(path))
        return;
    char *cmd = xasprintf("/usr/bin/madshelf --filter=audio %s", path);
    Ecore_Exe *exe = ecore_exe_run(cmd, NULL);
    if(exe)
        ecore_exe_free(exe);
    free(cmd);
}


static int
_get_freespace(madaudio_player_t *player)
{
    char *path = DEFAULT_PATH;
    struct statvfs vfs;
    statvfs(player->config->path, &vfs);
    int k = ( vfs.f_bsize * vfs.f_bavail ) ;
    printf("k=%d %d %d\n", k, vfs.f_bsize, vfs.f_bavail);
    return k / (8000 * 2);
}

void
madaudio_update_freespace(madaudio_player_t *player)
{
    player->freespace = _get_freespace(player);
}
