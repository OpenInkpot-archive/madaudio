SUBDIRS = configlet

INCLUDES = @EFL_CFLAGS@ @LIBEOI_CFLAGS@ @LIBKEYS_CFLAGS@ @LIBLOPS_CFLAGS@
AM_CFLAGS=-std=gnu99 -Wall -Wno-unused -DSYSCONFDIR="\"$(sysconfdir)\"" -DTHEMEDIR='"$(datadir)/madaudio"'

bin_PROGRAMS = madaudio madaudio-unsuspend

madaudio_SOURCES = madaudio.c madaudio_cmd.c madaudio_play.c madaudio_draw.c madaudio_load.c madaudio_meter.c madaudio_record.c madaudio_config.c
madaudio_LDADD = @EFL_LIBS@ @LIBEOI_LIBS@ @LIBKEYS_LIBS@ @LIBLOPS_LIBS@ -lmpdclient -lm

madaudio_unsuspend_SOURCES = madaudio_unsuspend.c 
madaudio_unsuspend_LDADD = -lmpdclient
