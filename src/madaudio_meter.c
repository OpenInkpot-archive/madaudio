#include <assert.h>
#include <stdio.h>
#include <Evas.h>
#include <Edje.h>
#include <edrawable.h>

#define METER_WIDTH   275
#define METER_HEIGHT  10

static void
madaudio_create_meter(Evas_Object *gui, const char *name, const char *swallow)
{
    int w, h;
    assert(gui);
    edje_object_part_geometry_get(gui, swallow, NULL, NULL, &w, &h);
    Evas *evas = evas_object_evas_get(gui);
    assert(evas);
    printf("%p %p %d %d\n", evas, gui, w, h);
    Evas_Object *meter = edrawable_add(evas, w, h);
    if(!meter)
        printf("epic fail: can't create meter\n");
    evas_object_move(meter, 0, 0);
    evas_object_resize(meter, w, h);
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
    edrawable_set_colors(meter, 0xd1, 0xd1, 0xd1, 255);
    edrawable_draw_rectangle_fill(meter, 0, 0, w, h);
    edrawable_set_colors(meter, 0, 0, 0, 255);
    edrawable_draw_rectangle_fill(meter, 0, h-3,
                            w, h);
    if(value)
    {
        int width = w * value / 100 ;
        edrawable_draw_rectangle_fill(meter, 0, 0, width, h);
    }
    edrawable_commit(meter);
}
