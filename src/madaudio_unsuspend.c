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

#define MADAUDIO_SOCKET "/tmp/madaudio-mpd.socket"

pid_t pid;
static int _debug;
static enum mpd_state oldstate;
static const char* unsuspendd_pidfile = "/var/run/unsuspendd.pid";

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



static int
read_int_from_file(const char* filename)
{
    char buf[16];
    FILE* file = fopen(filename, "r+");
    if(!file)
        err(1, "madaudio-unsuspend[%d]: Can't open %s\n", pid, filename);
    if(!fgets(buf, 16, file))
        err(1, "madaudio-unsuspend[%d]: Can't read value from %s\n", pid, filename);
    fclose(file);
    return atoi(buf);
}

static void
set_autosuspend(int mode)
{
    int unsuspendd = read_int_from_file(unsuspendd_pidfile);
    int sig = mode ? SIGUSR2 : SIGUSR1;
    kill(sig, unsuspendd);
    debug("Sending %s to unsuspendd[%d]", strsignal(sig), unsuspendd);
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
            {
                set_autosuspend(0);
            }
            else
            {
                if(!firsttime)
                    set_autosuspend(1);
            }
        };
        oldstate = state;
        firsttime = false;
        mpd_status_free(status);
        mpd_run_idle_mask(conn, MPD_IDLE_PLAYER);
        check(conn);
    }
}
