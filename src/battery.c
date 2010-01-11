/*
 * Madaudio - music player
 *
 * Copyright (C) 2009,2010 Mikhail Gusarov <dottedmag@dottedmag.net>
 * Copyright (C) 2009 Alexander Nikolaev <avn@daemon.hole.ru>
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

#include <libeoi_battery.h>

#include <Ecore.h>

#include "battery.h"

static void update_battery(Evas_Object* top)
{
    eoi_draw_battery_info(top);
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
