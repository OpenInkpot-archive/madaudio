#define _GNU_SOURCE 1
#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpd/client.h>
#include <mpd/error.h>

#define MADAUDIO_SOCKET "/tmp/madaudio-mpd.socket"

static int _debug;
static enum mpd_state oldstate;
const char* autosuspend = "/sys/power/autosuspend";

void
debug(const char *fmt,...)
{
    fprintf(stderr, "unsleep: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void
check(struct mpd_connection* conn)
{
    if(mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
        return;
    debug("Connection to mpd lost, exitting...");
    exit(0);
}

void
set_autosuspend(int mode)
{
    FILE* file = fopen(autosuspend, "a+");
    if(!file)
        err(1, "Can't open %s\n", autosuspend);
    fprintf(file, "%d\n", mode);
    fclose(file);
    debug("Set autosuspend to %d", mode);
}

int
main(int argc, char **argv)
{
    int t;
    if(!access(autosuspend, R_OK))
    {
        fprintf(stderr, "Autosuspend unsupported, exitting...\n");
        exit(1);
    }
    _debug = (bool) getenv("MADAUDIO_NOSLEEP_DEBUG");
    daemon(0, 0);
    struct mpd_connection *conn;
    for(t=100; t > 0; t++) {
        conn = mpd_connection_new(MADAUDIO_SOCKET, 0, 0);
        if(conn && mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
            break;
        debug("can't connect to mpd, retry");
    };
    debug("connected...");
    while(true)
    {
        struct mpd_status * status = mpd_run_status(conn);
        check(conn);
        if(!status)
            err(1, "Can't get status\n");
        enum mpd_state state = mpd_status_get_state(status);
        if(state != oldstate)
        {
            if(state == MPD_STATE_PLAY)
                set_autosuspend(0);
            else
                set_autosuspend(1);
        };
        oldstate = state;
        mpd_status_free(status);
        mpd_run_idle_mask(conn, MPD_IDLE_PLAYER);
        check(conn);
    }
}
