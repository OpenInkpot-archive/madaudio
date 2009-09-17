#include <assert.h>
#include <stdio.h>
#include <mpd/entity.h>
#include "empd.h"

void
empd_finish_entity(empd_connection_t* conn)
{
    if(!conn->entity)
        return; /* we can accidently get here whe parsing status reply */
    switch(mpd_entity_get_type(conn->entity))
    {
        case MPD_ENTITY_TYPE_SONG:
            printf("append song\n");
            empd_playlist_append(conn, mpd_entity_get_song(conn->entity));
            break;
        default:
            printf("Unsupported entity: %d\n",
                mpd_entity_get_type(conn->entity));
    }
    conn->entity = NULL;
}


static void
_entity_finish(void* data, void* cb_data)
{
    printf("entity finish\n");
    empd_connection_t* conn = (empd_connection_t *) cb_data;
    empd_finish_entity(conn);
    empd_callback_run(conn->playlist_callback, conn->playlist);
}

static void
_entity_pairs(void *data, void *cb_data)
{
    empd_connection_t* conn = (empd_connection_t*) cb_data;
    struct mpd_pair* pair = (struct mpd_pair*) data;
    bool result = mpd_entity_feed(conn->entity, pair);
    if(!result)
    {
        printf("finish pairs\n");
        empd_finish_entity(conn);
        conn->entity = mpd_entity_begin(pair);
    }
}

static void
_entity_pairs_first(void *data, void *cb_data)
{
    empd_connection_t* conn = (empd_connection_t*) cb_data;

    empd_playlist_clear(conn);

    struct mpd_pair* pair = (struct mpd_pair*) data;
    struct mpd_entity* entity = mpd_entity_begin(pair);
    if(!entity)
    {
        printf("Not entity\n");
        return;
    }
    conn->entity = entity;
    empd_callback_set(&conn->pair_callback, _entity_pairs, conn);
    empd_callback_set(&conn->finish_callback, _entity_finish, conn);
}

void
empd_playlistinfo(empd_connection_t* conn,
            void (*callback)(void*, void *), void* data)
{
    EMPD_BUSY(conn, callback, data, empd_playlistinfo);
    empd_callback_set(&conn->pair_callback, _entity_pairs_first, conn);
    empd_callback_set(&conn->playlist_callback, callback, data);
    empd_send(conn, "playlistinfo");
}

static void
_status_finish(void* data, void* cb_data)
{
    printf("status finish\n");
    empd_connection_t* conn = (empd_connection_t *) data;
    struct mpd_status* status = (struct mpd_status *) cb_data;
    assert(conn);
    assert(status);
    struct mpd_status* old = conn->status;
    conn->status = status;
    if(old)
        mpd_status_free(old);
    empd_callback_run(conn->status_callback, status);
}

static void
_status_pairs(void* data, void* cb_data)
{
    struct mpd_pair* pair = (struct mpd_pair *) data;
    struct mpd_status* status = (struct mpd_status *) cb_data;
    mpd_status_feed(status, pair);
}

void
empd_status(empd_connection_t* conn,
            void (*callback)(void*, void *), void *data)
{
    EMPD_BUSY(conn, callback, data, empd_status);
    struct mpd_status* status = mpd_status_new();
    empd_callback_set(&conn->pair_callback, _status_pairs, status);
    empd_callback_set(&conn->finish_callback, _status_finish, status);
    empd_callback_set(&conn->status_callback, callback, data);
    empd_send(conn, "status");
}

static void
_playlistinfo_callback(void* data, void* cb_data)
{
    printf("_playlistinfo_callback()\n");
    empd_connection_t* conn = (empd_connection_t*) cb_data;
    empd_callback_run(conn->synced, conn);
}

static void
_status_callback(void* data, void* cb_data)
{
    empd_connection_t* conn = (empd_connection_t*) cb_data;
    empd_playlistinfo(conn, _playlistinfo_callback, conn);
}


void
empd_status_sync(empd_connection_t* conn,
                void (*callback)(void* , void *), void *data)
{
    empd_callback_set(&conn->synced, callback, data);
    empd_status(conn, _status_callback, conn);
}
