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
#define DEFAULT_COMMAND "madaudio-dictophone arecord -f S16_LE -c1 -r8000 -t wav %s"
#define DEFAULT_FILETEMPLATE  "%F-%H_%M_%S.wav"
#define DEFAULT_CODEC "hq"
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
madaudio_ini_value_get(Efreet_Ini *ini, const char *key, char **value,
    const char *default_value)
{
    const char *tmp = efreet_ini_string_get(ini, key);
    if(tmp && value)
        *value = tmp;
    else
        *value = strdup(default_value);
}

void
madaudio_free_config(madaudio_config_t *config)
{
    free(config->template);
    free(config->path);
    free(config->default_codec);
    efreet_desktop_free(config->codec);
    free(config);
}

static char *
get_user_codec()
{
    char *home = getenv("HOME");
    if(!home)
        home="/home";

    char *path = xasprintf("%s/%s/%s", home, USER_CONFIG_DIR, USER_CONFIG_FILE);

    int fd = open(path, O_RDONLY);
    free(path);
    path = NULL;

    if(fd == -1)
        return NULL;

    char str[PATH_MAX + 1];
    int r = readn(fd, str, PATH_MAX);
    if(r > 0)
    {
        str[r-1] = '\0';
        char *c = strchr(str, '\n');
        if(c)
            *c = '\0';
        path = strdup(str);
    }
    close(fd);
    return path;
}

static Efreet_Desktop *
madaudio_get_codec(madaudio_config_t *config)
{
    Efreet_Desktop *codec = NULL;
    char *codec_path = NULL;

    codec_path = get_user_codec();
    if(!codec_path)
    {
        codec_path = xasprintf("/usr/share/madaudio/codecs/%s.desktop",
            config->default_codec);
    }

    if(!ecore_file_exists(codec_path))
    {
        fprintf(stderr, "Default codec not exists: %s\n", codec_path);
        return NULL;
    }

    codec = efreet_desktop_get(codec_path);
    free(codec_path);
    return codec;
}

madaudio_config_t *
madaudio_read_config(madaudio_player_t *player)
{
    madaudio_config_t *config = calloc(1, sizeof(madaudio_config_t));
    Efreet_Ini *ini = efreet_ini_new(MADAUDIO_INI);
    if(!ini)
    {
        config->template = strdup(DEFAULT_FILETEMPLATE);
        config->path = strdup(DEFAULT_PATH);
        config->default_codec = strdup(DEFAULT_CODEC);
        config->codec = madaudio_get_codec(config);
        return config;
    }
    efreet_ini_section_set(ini, MADAUDIO_RECORDER_SECTION);
    madaudio_ini_value_get(ini, "template", &config->template,
        DEFAULT_FILETEMPLATE);
    madaudio_ini_value_get(ini, "path", &config->path,
        DEFAULT_PATH);
    madaudio_ini_value_get(ini, "default_codec",
        &config->default_codec, DEFAULT_CODEC);

    config->codec = madaudio_get_codec(config);
    efreet_ini_free(ini);
    return config;
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

    if(!ensure_dir(player->config->path))
        return;

    printf("Recording...\n");
    char *fullname = xasprintf("%s/%s", player->config->path,
        madaudio_strftime(player->config->template));
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
    if(!ensure_dir(path))
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
