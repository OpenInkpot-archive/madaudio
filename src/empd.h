#ifndef _EMPD_H
#define _EMPD_H

#include <mpd/async.h>
#include <mpd/parser.h>
#include <mpd/song.h>
#include <Eina.h>
#include <Ecore.h>

typedef void (*empd_callback_func_t)(void *data, void* value);

typedef struct empd_callback_t empd_callback_t;
typedef struct empd_file_queue_t empd_file_queue_t;
typedef struct empd_connection_t empd_connection_t;


struct empd_callback_t {
    empd_callback_func_t   func;
    void* data;
};

void empd_callback_set(empd_callback_t** cb, empd_callback_func_t, void*);
void empd_callback_run(empd_callback_t* cb, void *value);
void empd_callback_once(empd_callback_t** cb, void *value);
void empd_callback_free(empd_callback_t* cb);


typedef void (*empd_action_func_t)(empd_connection_t*, empd_callback_func_t, void*);
typedef struct empd_delayed_command_t empd_delayed_command_t;
struct empd_delayed_command_t {
    empd_callback_func_t callback;
    void* data;
    empd_action_func_t action;
    char* arg;
    empd_delayed_command_t* next;
};

#define EMPD_BUSY(conn, callback, data, action) \
    if(empd_busy(conn, callback, data, action, NULL)) return

bool empd_busy(empd_connection_t* , empd_callback_func_t,
                void*,  empd_action_func_t, char*);


struct empd_file_queue_t {
    empd_file_queue_t* next;
    char* file;
};

struct empd_connection_t  {
    struct mpd_async* async;
    struct mpd_parser* parser;
    Ecore_Fd_Handler* fdh;
    int sock;

    Eina_List* playlist;
    struct mpd_status* status;
    struct mpd_entity* entity;

    empd_file_queue_t* queue;

    /* protocol busy by sending or receiving something */
    bool busy;

    /* fire when we sync empd_connection_t with mpd */
    empd_callback_t* synced;
    empd_callback_t* next_callback;
    empd_callback_t* connected;

    /* internal callbacks */
    empd_callback_t* playlist_callback;
    empd_callback_t* status_callback;
    empd_callback_t* pair_callback;
    empd_callback_t* finish_callback;

    bool idle_mode;

    empd_callback_t* line_callback;
    empd_callback_t* idle_callback;

    empd_delayed_command_t* delayed;

    /* error signalling/reconnect support */
    char* sockpath;
    void (*errback)(const char*, void*);
};

void
empd_connection_new(const char *,
                    void (*callback)(void*, void*),
                    void (*errback)(const char*, void*),
                    void* );

void
empd_connection_del(empd_connection_t*);

void
empd_enter_idle_mode(empd_connection_t*);

void
empd_playlist_clear(empd_connection_t* conn);

void
empd_playlist_append(empd_connection_t* conn, const struct mpd_song* song);

void
empd_status_sync(empd_connection_t* conn,
                void (*callback)(void* , void *), void* data);

void
empd_send_wait(empd_connection_t* conn,
                void (*callback)(void*, void*), void* data, const char *, ...);

void
empd_send_int_wait(empd_connection_t* conn,
                void (*callback)(void*, void*),
                void* data, const char* cmd, int arg);

bool
empd_pending_events(empd_connection_t* conn);

void
empd_play(empd_connection_t* conn, void (*callback)(void*, void *),
        void* data, int pos);

void
empd_clear(empd_connection_t* conn, void (*callback)(void*, void *), void* data);

void
empd_seek(empd_connection_t* conn, void (*callback)(void*, void *),
         void* data, int pos);

void
empd_playlistinfo(empd_connection_t* conn,
            void (*callback)(void*, void *), void* data);

/* Destructive, list of files consumed and freed by empd_enqueue_files */
void
empd_enqueue_files(empd_connection_t* conn, Eina_List* list);

/* internal */
void empd_finish_entity(empd_connection_t* conn);

void
empd_send(empd_connection_t* conn, const char* cmd);
#endif
