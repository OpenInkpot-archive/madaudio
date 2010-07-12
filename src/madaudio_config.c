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

