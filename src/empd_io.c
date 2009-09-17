#define _GNU_SOURCE 1
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <Eina.h>
#include <Ecore.h>

#include <mpd/async.h>
#include <mpd/pair.h>
#include <mpd/parser.h>
#include "empd.h"



static void
line_callback(void *data,  void *cb_data)
{
    empd_connection_t* conn = (empd_connection_t *) cb_data;
    char* line = (char *) data;
    struct mpd_pair pair;
    enum mpd_parser_result result;
    result = mpd_parser_feed(conn->parser, line);
    switch (result)
    {
        case MPD_PARSER_MALFORMED:
            printf("Fail to parse response\n");
            break;
        case MPD_PARSER_SUCCESS:
            if(conn->finish_callback)
            {
                empd_callback_once(&conn->finish_callback, conn);
                printf("returned from finish callback\n");
            }
            if(empd_pending_events(conn))
            {
                printf("we still busy\n");
                break;
            }
            conn->busy = false;
            if(conn->next_callback)
                empd_callback_once(&conn->next_callback, conn);
            break;
        case MPD_PARSER_ERROR:
            printf("Got MPD_PARSER_ERROR\n");
            break;
        case MPD_PARSER_PAIR:
            pair.name = mpd_parser_get_name(conn->parser);
            pair.value = mpd_parser_get_value(conn->parser);
            empd_callback_run(conn->pair_callback, &pair);
            break;
        default:
            printf("Must never reach\n");
    }
}


static void
idle_line_callback(void* data, void* cb_data)
{
    empd_connection_t* conn = (empd_connection_t *) cb_data;
    printf("idle signal\n");
    empd_callback_set(&conn->line_callback, line_callback, conn);
    if(conn->idle_mode) {
        conn->idle_mode = false;
        empd_callback_run(conn->idle_callback, conn);
    }
}

void
empd_enter_idle_mode(empd_connection_t * conn)
{
    empd_callback_set(&conn->line_callback, &idle_line_callback, conn);
    conn->idle_mode = true;
}

#define MPD_WELCOME_MESSAGE   "OK MPD "

static void
hello_line_callback(void* data, void* cb_data)
{
    empd_connection_t* conn = (empd_connection_t *) cb_data;
    char* line = (char *) data;
    if (strncmp(line, MPD_WELCOME_MESSAGE, strlen(MPD_WELCOME_MESSAGE)))
    {
//        empd_error_code(&conn->error, MPD_ERROR_NOTMPD);
//        empd_error_printf(&conn->error, "mpd not running");
        printf("mpd not running or has bad hello line\n");
        conn->errback(conn->sockpath, conn->connected->data);
        empd_connection_del(conn);
        return;
    }
    printf("Got hello line\n");
    empd_callback_set(&conn->line_callback, line_callback, conn);
    conn->busy = false;
    empd_callback_run(conn->connected, conn);
}


static int
io_callback(void *data, Ecore_Fd_Handler *fd_handler)
{
    empd_connection_t * conn = (empd_connection_t *) data;
    int events=0;

    if (ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_READ))
        events |= MPD_ASYNC_EVENT_READ;

    if (ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_WRITE))
        events |= MPD_ASYNC_EVENT_WRITE;

    if (ecore_main_fd_handler_active_get(conn->fdh, ECORE_FD_ERROR))
        events |= MPD_ASYNC_EVENT_HUP | MPD_ASYNC_EVENT_ERROR;

    mpd_async_io(conn->async, events);

    char *line = mpd_async_recv_line(conn->async);
    if(line) {
        printf("got: %s\n", line);
        assert(conn);
        assert(conn->line_callback);
        empd_callback_run(conn->line_callback, line);
    }
    return 1;
}

#define LENGTH_OF_SOCKADDR_UN(s) \
    (strlen((s)->sun_path) + (size_t)(((struct sockaddr_un *)NULL)->sun_path))

static int
empd_socket(const char *sockpath)
{
    struct sockaddr_un  socket_unix;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    int curstate = 0;
    if(fd < 0) goto err_sock;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) goto err_sock;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) goto err_sock;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &curstate, sizeof(curstate)) < 0) goto err_sock;
    socket_unix.sun_family = AF_UNIX;
    strncpy(socket_unix.sun_path, sockpath, sizeof(socket_unix.sun_path));
    int socket_unix_len = LENGTH_OF_SOCKADDR_UN(&socket_unix);
    if (connect(fd, (struct sockaddr *)&socket_unix, socket_unix_len) < 0)
        goto err_sock;
    return fd;
err_sock:
    printf("Err sock\n");
    return -1;
}

void
empd_connection_new(const char *sockpath,
    void (*callback)(void*, void*),
    void (*errback)(const char*, void*),
    void* data)
{
    empd_connection_t *conn = calloc(1, sizeof(empd_connection_t));
    empd_callback_set(&conn->connected, callback, data);
    conn->errback = errback;
    conn->sockpath = strdup(sockpath);

    int sock = -1;
    if(conn) {
        sock = empd_socket(sockpath);
        if( sock < 0 ) {
            printf("Can't get socket\n");
            goto sock_err;
        }
        conn->async = mpd_async_new(sock);
        empd_callback_set(&conn->line_callback, hello_line_callback, conn);
        conn->sock = sock;

        if(!conn->async) {
            printf("Can't create mpd_async\n");
            goto sock_err;
        }

        conn->parser = mpd_parser_new();
        if(!conn) {
            printf("Can't alloc parser\n");
            goto sock_err;
        }
        conn->fdh = ecore_main_fd_handler_add(sock,
            ECORE_FD_READ | ECORE_FD_WRITE | ECORE_FD_ERROR,
            io_callback, (void *)conn, NULL, NULL);
        printf("Connection initialized\n");
    }
    conn->busy = true;
    return;
sock_err:
    if(conn->async)
        mpd_async_free(conn->async);
    if(conn->parser)
        mpd_parser_free(conn->parser);
    if(sock >= 0)
        close(sock);
    free(conn);
    errback(sockpath, data);
}


void
empd_connection_del(empd_connection_t * conn) {
    if(conn) {
        if(conn->parser)
            mpd_parser_free(conn->parser);
        if(conn->async)
            mpd_async_free(conn->async);
        if(conn->fdh)
            ecore_main_fd_handler_del(conn->fdh);
        free(conn->sockpath);
        close(conn->sock);
    }
}
