MAINTAINERCLEANFILES = Makefile.in
COMPILED_THEMES=madaudio.edj


themedir=$(datadir)/eoi/themes/default
theme_DATA=$(COMPILED_THEMES)

madaudio.edj: ${srcdir}/../madaudio.edc
	edje_cc -id .. -id . $< $@

CLEANFILES=$(COMPILED_THEMES)
