INCLUDES = @EFL_CFLAGS@ @LIBKEYS_CFLAGS@ @LIBEOI_CFLAGS@ @LIBLOPS_CFLAGS@ -I${top_srcdir}/src

libconfigletdir = "${libdir}/gm/configlets"
libconfiglet_LTLIBRARIES = madaudio.la
madaudio_la_SOURCES = configlet.c ${top_srcdir}/src/madaudio_config.c
madaudio_la_LDFLAGS = -module -avoid-version -nostatic @EFL_LIBS@ @LIBLOPS_LIBS@

