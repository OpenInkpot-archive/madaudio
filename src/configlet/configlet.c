#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <err.h>

#include <Evas.h>
#include <Ecore.h>
#include <Edje.h>
#include <Ecore_File.h>

#include <libeoi_utils.h>
#include <libchoicebox.h>
#include <gm-configlet.h>

#include "madaudio.h"

#define _(x) x

#define PATH_MAX 4096

typedef struct {
    char *filename;
    char *title;
    char *generic;
} madaudio_codec_t;

typedef struct {
    int num;
    Eina_List *codecs;
    const char *current;
} madaudio_configlet_t;


static int
_current_codec(madaudio_configlet_t *configlet)
{
    int num=0;
    Eina_List *each = configlet->codecs;
    while(each)
    {
        madaudio_codec_t *codec = eina_list_data_get(each);
        each = eina_list_next(each);
        if(!strcmp(codec->filename, configlet->current))
            return num;
        num++;
    }
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
                    Evas_Object *choicebox __attribute__((unused)),
                    int item_num,
                    bool is_alt __attribute__((unused)),
                    void *param __attribute__((unused)))
{
    gm_configlet_submenu_pop(choicebox);

    Evas_Object *parent = (Evas_Object*)param;
    choicebox_invalidate_item(parent, 1);
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
    Evas_Object * main_canvas_edje = evas_object_name_find(canvas,"main_canvas_edje");
    edje_object_part_text_set(main_canvas_edje, "title", gettext("Select recorder codec"));
    evas_object_data_set(choicebox, "configlet", data);
}

static void
madaudio_draw(void *data, Evas_Object *item)
{
    madaudio_configlet_t *configlet = data;
    madaudio_codec_t *codec = eina_list_nth(configlet->codecs,
        _current_codec(configlet));
    edje_object_part_text_set(item, "title", gettext("Recorder codec"));
    edje_object_part_text_set(item, "value", codec->title);
}

static void *
madaudio_load()
{
    madaudio_configlet_t *configlet = calloc(1, sizeof(madaudio_configlet_t));

    char *filename;
    Eina_List *ls = ecore_file_ls(MADAUDIO_CODEC_PATH);
    Eina_List *ls_free;
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

        eina_desktop_free(current);
        configlet->codecs = eina_list_append(configlet->codecs, codec);
    }
    EINA_LIST_FREE(ls_free, filename)
        free(filename);

    configlet->num = eina_list_count(configlet->codecs);

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
