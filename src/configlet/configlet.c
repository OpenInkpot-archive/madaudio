#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <err.h>
#include <unistd.h>

#include <Evas.h>
#include <Ecore.h>
#include <Edje.h>
#include <Ecore_File.h>

#include <libeoi_utils.h>
#include <libchoicebox.h>
#include <gm-configlet.h>

#include "madaudio.h"

#define _(x) x

typedef struct {
    char *filename;
    char *title;
    char *generic;
} madaudio_codec_t;

typedef struct {
    int num;
    Eina_List *codecs;
    char *current;
    madaudio_config_t *config;
} madaudio_configlet_t;


void
_save_user_codec(const char *codecname)
{
    char *home = getenv("HOME");
    if(!home)
        home = "/home";
    char *dirname = xasprintf("%s/%s", home, USER_CONFIG_DIR);
    madaudio_ensure_dir(dirname);
    char *filename = xasprintf("%s/%s", dirname, USER_CONFIG_FILE);

    int fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY);
    if(fd)
    {
        writen(fd, codecname, strlen(codecname));
        close(fd);
    }
    else
        fprintf(stderr, "Can't open %s\n", filename);
    free(filename);
    free(dirname);
}

static int
_current_codec(madaudio_configlet_t *configlet)
{
    int num=0;
    Eina_List *each = configlet->codecs;
    while(each)
    {
        madaudio_codec_t *codec = eina_list_data_get(each);
        if(!strcmp(codec->filename, configlet->current))
            return num;
        num++;
        each = eina_list_next(each);
    }
    fprintf(stderr, "Not found\n");
    return 0;
}

static void madaudio_submenu_draw(
                      Evas_Object *choicebox __attribute__((unused)),
                      Evas_Object *item,
                      int item_num,
                      int page_position __attribute__((unused)),
                      void* param __attribute__((unused)))
{
    madaudio_configlet_t *configlet =
             evas_object_data_get(choicebox, "configlet");
    madaudio_codec_t *codec = eina_list_nth(configlet->codecs, item_num);
    edje_object_part_text_set(item,
                              "title", codec->generic);
    edje_object_signal_emit(item, "icon-none", "");
}

static void madaudio_submenu_handler(
                    Evas_Object *choicebox,
                    int item_num,
                    bool is_alt __attribute__((unused)),
                    void *param __attribute__((unused)))
{
    madaudio_configlet_t *configlet =
             evas_object_data_get(choicebox, "configlet");
    madaudio_codec_t *codec = eina_list_nth(configlet->codecs, item_num);
    if(codec)
    {
        free(configlet->current);
        configlet->current = strdup(codec->filename);
        _save_user_codec(codec->filename);
        fprintf(stderr, "configlet=%x\n", configlet);
        gm_configlet_invalidate_parent(choicebox, configlet);
    }

    // last, because we pass choicebox to gm_configlet_invalidate_parent
    //    where choicebox must be valid and attached to evas
    gm_configlet_submenu_pop(choicebox);
}

static void
madaudio_select(void *data, Evas_Object *parent)
{
    madaudio_configlet_t *configlet = data;
    Evas *canvas = evas_object_evas_get(parent);
    Evas_Object *choicebox;
    choicebox = gm_configlet_submenu_push(parent,
               madaudio_submenu_handler,
               madaudio_submenu_draw,
               configlet->num,
               parent);
    if(!choicebox)
        printf("We all dead\n");
    choicebox_set_selection(choicebox, _current_codec(configlet));
    Evas_Object * main_canvas_edje =
        evas_object_name_find(canvas,"main_canvas_edje");
    edje_object_part_text_set(main_canvas_edje, "title",
        dgettext("madaudio", "Select recorder codec"));
    evas_object_data_set(choicebox, "configlet", data);
}

static void
madaudio_draw(void *data, Evas_Object *item)
{
    madaudio_configlet_t *configlet = data;
    madaudio_codec_t *codec = eina_list_nth(configlet->codecs,
        _current_codec(configlet));
    edje_object_part_text_set(item, "title",
        dgettext("madaudio","Recorder codec"));
    char *title = "N/A";
    if(codec)
        title = codec->title;
    edje_object_part_text_set(item, "value", title);
}

static void *
madaudio_load()
{
    madaudio_configlet_t *configlet = calloc(1, sizeof(madaudio_configlet_t));

    char *filename;
    Eina_List *ls = ecore_file_ls(MADAUDIO_CODEC_PATH);
    Eina_List *ls_free = ls;
    while(ls)
    {
        filename = eina_list_data_get(ls);
        ls = eina_list_next(ls);

        char *fullname = xasprintf("%s/%s", MADAUDIO_CODEC_PATH, filename);
        if(ecore_file_is_dir(fullname))
        {
            free(fullname);
            continue;
        }
        Efreet_Desktop *current = efreet_desktop_get(fullname);
        if(!current)
        {
            free(fullname);
            continue;
        }
        madaudio_codec_t *codec = calloc(1, sizeof(madaudio_codec_t));

        codec->filename = fullname;
        codec->title = strdup(current->name);
        codec->generic = strdup(current->generic_name);

        efreet_desktop_free(current);
        configlet->codecs = eina_list_append(configlet->codecs, codec);
        free(filename);
    }
    eina_list_free(ls_free);
    configlet->num = eina_list_count(configlet->codecs);
    configlet->config = madaudio_read_config();
    configlet->current = madaudio_get_current_codec_path(configlet->config);

    return configlet;
}

static void
madaudio_unload(void *data)
{
    madaudio_configlet_t *configlet = data;
    madaudio_codec_t *each;
    EINA_LIST_FREE(configlet->codecs, each)
    {
        free(each->filename);
        free(each->title);
        free(each->generic);
        free(each);
    }
    madaudio_free_config(configlet->config);
    free(configlet->current);
    free(configlet);
}

const configlet_plugin_t *
configlet_madaudio(void)
{
    static const configlet_plugin_t configlet = {
        .load = madaudio_load,
        .unload = madaudio_unload,
        .draw = madaudio_draw,
        .select = madaudio_select,
        .sort_key = "70madaudio",
    };
    return &configlet;
}
