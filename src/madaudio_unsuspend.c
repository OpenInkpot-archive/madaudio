#define _GNU_SOURCE 1
#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <mpd/client.h>
#include <mpd/error.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MADAUDIO_SOCKET "/tmp/madaudio-mpd.socket"

pid_t pid;
static int _debug;
static enum mpd_state oldstate;

static int unsuspend_fd;

#define UNSUSPENDD_SOCKET "/var/lib/unsuspendd/commands.sock"

static void
debug(const char *fmt,...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(LOG_INFO, fmt, ap);
    va_end(ap);
}

void
check(struct mpd_connection* conn)
{
    if(mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
        return;
    debug("Connection to mpd lost, exitting...");
    exit(0);
}

static void
lock_autosuspend()
{
    if (unsuspend_fd != -1)
        return;

    unsuspend_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unsuspend_fd == -1)
        err(1, "madaudio-unsuspend[%d]: Can't create socket\n", pid);

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    memcpy(addr.sun_path, UNSUSPENDD_SOCKET, strlen(UNSUSPENDD_SOCKET));

    if (connect(unsuspend_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        err(1, "madaudio-unsuspend[%d]: Can't connect to unsuspendd\n", pid);
}

static void
unlock_autosuspend()
{
    close(unsuspend_fd);
    unsuspend_fd = -1;
}

int
main(int argc, char **argv)
{
    int t;
    bool firsttime = true;
    pid=getpid();
    _debug = (bool) getenv("MADAUDIO_DEBUG");
    if(!_debug)
        daemon(0, 0);
    int flags = LOG_NDELAY | LOG_PID;
    if(_debug)
        flags |= LOG_PERROR;
    openlog("madaudio-unsuspend", flags, LOG_DAEMON);

    struct mpd_connection *conn;
    for(t=1000; t > 0; t++) {
        conn = mpd_connection_new(MADAUDIO_SOCKET, 0, 0);
        if(conn && mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
            break;
        debug("can't connect to mpd, retry", pid);
        usleep(1000);
    };
    debug("connected...");
    while(true)
    {
        struct mpd_status * status = mpd_run_status(conn);
        check(conn);
        if(!status)
            err(1, "madaudio-unsuspend[%d]: Can't get status\n");
        enum mpd_state state = mpd_status_get_state(status);
        if(state != oldstate)
        {
            if(state == MPD_STATE_PLAY)
                lock_autosuspend();
            else
            {
                if(!firsttime)
                    unlock_autosuspend();
            }
        };
        oldstate = state;
        firsttime = false;
        mpd_status_free(status);
        mpd_run_idle_mask(conn, MPD_IDLE_PLAYER);
        check(conn);
    }
}
