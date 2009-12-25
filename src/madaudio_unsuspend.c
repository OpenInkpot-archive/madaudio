#define _GNU_SOURCE 1
#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpd/client.h>
#include <mpd/error.h>

#define MADAUDIO_SOCKET "/tmp/madaudio-mpd.socket"

static int _debug;
static enum mpd_state oldstate;
static const char* autosuspend = "/sys/power/autosuspend";
static int saved=-1; /* Saved /sys/power/autosuspend value */
static int saved_by_signal = -1;
static const char* usb0 = "/sys/class/net/usb0/carrier";

static void
debug(const char *fmt,...)
{
    if(_debug)
    {
        fprintf(stderr, "unsleep: ");
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");
    }
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
set_autosuspend(int mode)
{
    FILE* file = fopen(autosuspend, "a+");
    if(!file)
        err(1, "Can't open %s\n", autosuspend);
    fprintf(file, "%d\n", mode);
    fclose(file);
    debug("Set autosuspend to %d", mode);
}


static int
read_int_from_file(const char* filename)
{
    char buf[16];
    FILE* file = fopen(autosuspend, "r+");
    if(!file)
        err(1, "Can't open %s\n", autosuspend);
    if(!fgets(buf, 16, file))
        err(1, "Can't read value from %s\n", autosuspend);
    fclose(file);
    return atoi(buf);
}

int
get_autosuspend()
{
    return read_int_from_file(autosuspend);
}

static void
sigusr(int signo)
{
    debug("Got signal: %s", strsignal(signo));
    if(signo == SIGUSR1)
    {
        if(oldstate == MPD_STATE_PLAY && saved_by_signal == -1)
        {
            debug("saving saved state\n");
            saved_by_signal = saved;
        }
    }
    else if(signo == SIGUSR2)
    {
        if(oldstate == MPD_STATE_PLAY)
        {
            debug("restore saved state\n");
            saved = saved_by_signal;
            set_autosuspend(0);
        }
        saved_by_signal = -1;
    }
    signal(signo, sigusr);
}

int
main(int argc, char **argv)
{
    int t;
    if(access(autosuspend, R_OK))
    {
        fprintf(stderr, "Autosuspend unsupported, exitting...\n");
        exit(1);
    }
    _debug = (bool) getenv("MADAUDIO_NOSLEEP_DEBUG");
    if(!_debug)
        daemon(0, 0);
    struct mpd_connection *conn;
    for(t=500; t > 0; t++) {
        conn = mpd_connection_new(MADAUDIO_SOCKET, 0, 0);
        if(conn && mpd_connection_get_error(conn)==MPD_ERROR_SUCCESS)
            break;
        debug("can't connect to mpd, retry");
        usleep(200);
    };
    debug("connected...");
    signal(SIGUSR1, sigusr);
    signal(SIGUSR2, sigusr);
    while(true)
    {
        if(!access(usb0, R_OK))
        {
            debug("USBNET files exists, check them");
            if(read_int_from_file(usb0))
            {
                debug("USBNET active, do nothing\n");
                goto idle;
            }
        }
        struct mpd_status * status = mpd_run_status(conn);
        check(conn);
        if(!status)
            err(1, "Can't get status\n");
        enum mpd_state state = mpd_status_get_state(status);
        if(state != oldstate)
        {
            if(state == MPD_STATE_PLAY)
            {
                saved = get_autosuspend();
                set_autosuspend(0);
            }
            else
            {
                if(saved == -1)
                    debug("not saved");
                else
                    set_autosuspend(saved);
            }
        };
        oldstate = state;
        mpd_status_free(status);
idle:
        mpd_run_idle_mask(conn, MPD_IDLE_PLAYER);
        check(conn);
    }
}
