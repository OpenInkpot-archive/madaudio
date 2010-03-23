#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <Ecore.h>
#include <Ecore_File.h>
#include <Efreet.h>
#include <mpd/client.h>
#include <libeoi_utils.h>

#include "madaudio.h"

#define FILENAME_LEN 512
#define DEFAULT_COMMAND "madaudio-dictophone arecord %s"
#define DEFAULT_FILETEMPLATE  "%F-%H_%M_%S.wav"
#define DEFAULT_PATH "/mnt/storage/dictophone"
#define MADAUDIO_RECORDER_SECTION "recorder"
#define MADAUDIO_INI "/etc/madaudio.ini"

static bool
ensure_dir(const char *path)
{
    if(!ecore_file_is_dir(path))
    {
        if(!ecore_file_mkdir(path))
        {
            printf("Can't create dir %s\n", path);
            return false;
        }
    }
    return true;
}

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

static void
madaudio_ini_value_get(Efreet_Ini *ini, const char *key, char **value)
{
    const char *tmp = efreet_ini_string_get(ini, key);
    if(tmp && value)
        *value = tmp;
}

static void
madaudio_read_config(char **template, char **path, char **command)
{
    Efreet_Ini *ini = efreet_ini_new(MADAUDIO_INI);
    if(!ini)
        return;
    efreet_ini_section_set(ini, MADAUDIO_RECORDER_SECTION);
    madaudio_ini_value_get(ini, "template", template);
    madaudio_ini_value_get(ini, "path", path);
    madaudio_ini_value_get(ini, "command", command);
    efreet_ini_free(ini);
}

static int
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

void
madaudio_start_record(madaudio_player_t *player)
{
    char *command = DEFAULT_COMMAND;
    char *template = DEFAULT_FILETEMPLATE;
    char *path = DEFAULT_PATH;

    if(player->recorder)
    {
        printf("Already recording...\n");
        return;
    }
    if(player->status == MPD_STATE_PLAY)
        madaudio_play_pause(player);

    player->context = "recording";

    madaudio_read_config(&template, &path, &command);

    if(!ensure_dir(path))
        return;

    printf("Recording...\n");
    char *fullname = xasprintf("%s/%s", path, madaudio_strftime(template));
    char *line = xasprintf(command, fullname);
    char *cmdline = xasprintf("chpst -P %s", line);
    player->recorder_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                       madaudio_callback,
                                                       player);
    printf("Run: %s\n", cmdline);
    player->recorder = ecore_exe_run(cmdline, NULL);
    free(line);
    free(cmdline);
    free(fullname);
    madaudio_draw_recorder_start(player);
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
    madaudio_read_config(NULL, &path, NULL);

    if(!ensure_dir(path))
        return;
    char *cmd = xasprintf("/usr/bin/madshelf --filter=audio %s", path);
    Ecore_Exe *exe = ecore_exe_run(cmd, NULL);
    if(exe)
        ecore_exe_free(exe);
    free(cmd);
}
