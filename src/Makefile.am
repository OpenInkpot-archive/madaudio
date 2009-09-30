INCLUDES = @EFL_CFLAGS@ @LIBEOI_CFLAGS@ @LIBKEYS_CFLAGS@ @EDRAWABLE_CFLAGS@
AM_CFLAGS=-std=gnu99 -Wall -Wno-unused -Werror -DSYSCONFDIR="\"$(sysconfdir)\"" -DTHEMEDIR='"$(datadir)/madaudio"'

bin_PROGRAMS = madaudio

madaudio_SOURCES = madaudio.c madaudio_cmd.c madaudio_play.c madaudio_draw.c madaudio_load.c madaudio_meter.c battery.c clock.c empd_io.c empd_callback.c empd_playlist.c empd_recv.c empd_send.c
madaudio_LDADD = @EFL_LIBS@ @LIBEOI_LIBS@ @LIBKEYS_LIBS@ @EDRAWABLE_LIBS@ -lmpdclient -lm


