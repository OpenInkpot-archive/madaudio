#include <stdio.h>
#include <Evas.h>
#include <Edje.h>
#include <edrawable.h>

#define METER_WIDTH   275
#define METER_HEIGHT  10

void
madaudio_init_meter(Evas_Object* gui)
{
    Evas* evas = evas_object_evas_get(gui);
    Evas_Object* meter = edrawable_add(evas, METER_WIDTH, METER_HEIGHT);
    if(!meter)
        printf("epic fail: can't create meter\n");
    evas_object_move(meter, 0, 0);
    evas_object_resize(meter, METER_WIDTH, METER_HEIGHT);
    evas_object_name_set(meter, "meter");
    evas_object_show(meter);
    edje_object_part_swallow(gui, "meter", meter);
}

void
madaudio_update_meter(Evas_Object* gui, int value)
{
    Evas_Object* meter = evas_object_name_find(evas_object_evas_get(gui),
                                                "meter");
    if(!meter)
    {
        printf("epic fail: Can't find meter\n");
        return;
    }
    edrawable_set_colors(meter, 0xd1, 0xd1, 0xd1, 255);
    edrawable_draw_rectangle_fill(meter, 0, 0, METER_WIDTH, METER_HEIGHT);
    edrawable_set_colors(meter, 0, 0, 0, 255);
    edrawable_draw_rectangle_fill(meter, 0, METER_HEIGHT-3,
                            METER_WIDTH, METER_HEIGHT);
    if(value)
    {
        int width = METER_WIDTH * value / 100 ;
        edrawable_draw_rectangle_fill(meter, 0, 0, width, METER_HEIGHT);
    }
    edrawable_commit(meter);
}
