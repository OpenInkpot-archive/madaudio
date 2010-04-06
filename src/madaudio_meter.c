#include <assert.h>
#include <stdio.h>
#include <Evas.h>
#include <Edje.h>
#include <libeoi_themes.h>
#include <libeoi_utils.h>


static void
madaudio_create_meter(Evas_Object *gui, const char *name, const char *swallow)
{
    Evas *evas = evas_object_evas_get(gui);
    Evas_Object *meter = eoi_create_themed_edje(evas, "madaudio", "meter");
    if(!meter)
        printf("epic fail: can't create meter\n");
    evas_object_name_set(meter, name);
    evas_object_show(meter);
    edje_object_part_swallow(gui, swallow, meter);
    madaudio_update_meter(gui, name, swallow, 0);
}

void
madaudio_init_meter(Evas_Object *gui)
{
    madaudio_create_meter(gui, "meter", "meter");
    madaudio_create_meter(gui, "recorder-meter", "recorder-meter");
}

void
madaudio_update_meter(Evas_Object* gui, const char *name,
                      const char *swallow, int value)
{
    int w, h;
    edje_object_part_geometry_get(gui, swallow, NULL, NULL, &w, &h);
    Evas *evas = evas_object_evas_get(gui);
    Evas_Object *meter = evas_object_name_find(evas, name);
    if(!meter)
    {
        printf("epic fail: Can't find meter\n");
        return;
    }
    char *src = xasprintf("%02f", 0.01 * (float) value );
    edje_object_signal_emit(meter, "set-meter", src);
//    printf("meter: %s\n", src);
    free(src);
}
