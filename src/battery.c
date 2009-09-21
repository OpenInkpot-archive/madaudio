/*
 * MadShelf - bookshelf application.
 *
 * Copyright (C) 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdio.h>

#include <Edje.h>
#include <Ecore.h>

#include "battery.h"

typedef struct
{
    char* now;
    char* min;
    char* max;
} battery_info_t;

static battery_info_t batteries[] =
{
    {
        "/sys/class/power_supply/n516-battery/charge_now",
        "/sys/class/power_supply/n516-battery/charge_empty_design",
        "/sys/class/power_supply/n516-battery/charge_full_design",
    },
    {
        "/sys/class/power_supply/lbookv3_battery/charge_now",
        "/sys/class/power_supply/lbookv3_battery/charge_empty_design",
        "/sys/class/power_supply/lbookv3_battery/charge_full_design",
    },
};

static int _find_battery()
{
    int i;
    for(i = 0; i < sizeof(batteries)/sizeof(batteries[0]); ++i)
    {
        if(!access(batteries[i].now, R_OK))
            return i;
    }
    return -1;
}

static int _read_int_file(const char* filename)
{
    int res = 0;
    FILE* f = fopen(filename, "r");
    fscanf(f, "%d", &res);
    fclose(f);
    return res;
}

/*
 * -1 - unknown
 * 0..100 - charge
 */
static int _get_state()
{
    int batt = _find_battery();

    if(batt == -1)
        return -1;

    int now = _read_int_file(batteries[batt].now);
    int min = _read_int_file(batteries[batt].min);
    int max = _read_int_file(batteries[batt].max);

    return 100 * (now - min) / (max - min);
}

static void update_battery(Evas_Object* top)
{
    int charge = _get_state();

    char signal[256];
    snprintf(signal, 256, "battery-level,%d", charge);

    edje_object_signal_emit(top, signal, "");
}

static void
detach_battery_timer(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Ecore_Timer* timer = (Ecore_Timer*) data;
        ecore_timer_del(timer);
}

static int update_batt_cb(void* param)
{
    update_battery((Evas_Object*)param);
    return 1;
}

void init_battery(Evas_Object* top)
{
    update_battery(top);
    Ecore_Timer* timer=ecore_timer_add(5*60, &update_batt_cb, top);
    evas_object_event_callback_add(top, EVAS_CALLBACK_DEL,
            &detach_battery_timer, timer);
}
