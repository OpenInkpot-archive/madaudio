#define _GNU_SOURCE 1
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <mpd/async.h>
#include <mpd/status.h>
#include "empd.h"

void
empd_send(empd_connection_t* conn, const char* cmd)
{
    assert(conn);
    mpd_async_send_command(conn->async, cmd, NULL);
    printf("send: %s\n", cmd);
}


void
empd_send_wait(empd_connection_t* conn,
                void (*callback)(void*, void*),
                void* data, const char *cmd, ...)
{
    va_list args;
    bool success;
    empd_callback_set(&conn->next_callback, callback, data);
    assert(conn->async);
    assert(cmd);
    printf("send: %s\n", cmd);
    va_start(args, cmd);
    success = mpd_async_send_command_v(conn->async, cmd, args);
    va_end(args);
    if(!success)
        printf("we all die\n");
}

void
empd_send_wait_unlocked(empd_connection_t* conn,
                void (*callback)(void*, void*),
                void* data, char* cmd)
{
    empd_callback_set(&conn->next_callback, callback, data);
    empd_send(conn, cmd);
    free(cmd);
}

void
empd_send_str_wait(empd_connection_t* conn,
                void (*callback)(void*, void*),
                void* data, const char* cmd, char* arg)
{
    char *line;
    asprintf(&line, "%s \"%s\"", cmd, arg);
    if(empd_busy(conn, callback, data, NULL, line))
        return;
    empd_send_wait_unlocked(conn, callback, data, line);
}

void
empd_send_int_wait(empd_connection_t* conn,
                void (*callback)(void*, void*),
                void* data, const char* cmd, int arg)
{
    char buf[20];
    snprintf(buf, 16, "%d", arg);
    empd_send_str_wait(conn, callback, data, cmd, buf);
}

void
empd_enqueue_file(empd_connection_t* conn, const char *file)
{
    empd_file_queue_t* ptr = conn->queue;
    empd_file_queue_t* newitem = calloc(1, sizeof(empd_file_queue_t));
    newitem->file = strdup(file);
    newitem->next = NULL;

    if(ptr)
    {
        while(ptr->next)
            ptr = ptr->next;
        ptr->next = newitem;
    }
    else
        conn->queue = newitem;
}



static void
empd_flush_queue(empd_connection_t* conn)
{
    char buf[1024];
    empd_file_queue_t* item = conn->queue;
    conn->queue = item->next;
    snprintf(buf, 1024, "file://%s", item->file);
    mpd_async_send_command(conn->async, "add", buf, NULL);
    free(item->file);
    free(item);
}

void
empd_enqueue_files(empd_connection_t* conn, Eina_List* list)
{
    char* item;
    EINA_LIST_FREE(list, item)
        empd_enqueue_file(conn, item);
    if(!conn->busy)
    {
        conn->busy = true;
        empd_flush_queue(conn);
    }
}

bool
empd_pending_events(empd_connection_t* conn)
{
    /* release lock, and re-run pending actions, we assume single-thread
    model and don't care about thread safety here */
    conn->busy = false;
    if(conn->queue)
    {
        empd_flush_queue(conn);
        return true;
    };
    if(conn->delayed)
    {
        empd_delayed_command_t* delayed = conn->delayed;
        conn->delayed = delayed->next;

        printf("flush delayed\n");
        if(delayed->arg)
        {
            assert(delayed->action == NULL);
            printf("flush delayed: %s\n", delayed->arg);
            empd_send_wait_unlocked(conn, delayed->callback, delayed->data,
                                    delayed->arg);
        }
        else
            delayed->action(conn, delayed->callback, delayed->data);
        free(delayed);
        return true;
    }
    return false;
}

bool empd_busy(empd_connection_t* conn, empd_callback_func_t callback,
                void* data, empd_action_func_t action, char *arg)
{
    if(!conn->busy)
    {
        printf("Not busy, proceeding\n");
        conn->busy = true;
        return false;
    }

    printf("queueing delayed\n");
    empd_delayed_command_t* cmd = calloc(1, sizeof(empd_delayed_command_t));
    cmd->next = conn->delayed;
    conn->delayed = cmd;

    cmd->action = action;
    cmd->data = data;
    cmd->callback = callback;
    cmd->arg = arg;
    return true;
}

void
empd_play(empd_connection_t* conn, void (*callback)(void*, void *),
        void* data, int pos)
{
    /* empd_send_int_wait has locking  built in */
    empd_send_int_wait(conn, callback, data, "play", pos);
}

void
empd_seek(empd_connection_t* conn, void (*callback)(void*, void *),
         void* data, int pos)
{
    char *line;
    int song_pos = mpd_status_get_song(conn->status);
    asprintf(&line, "seek \"%d\" \"%d\"", song_pos, pos);

    if(empd_busy(conn, callback, data, NULL, line))
        return;
    empd_send_wait_unlocked(conn, callback, data, line);
}

void
empd_clear(empd_connection_t* conn, void (*callback)(void*, void *),
        void* data)
{
    EMPD_BUSY(conn, callback, data, empd_clear);
    empd_send_wait(conn, callback, data, "clear", NULL);
}
