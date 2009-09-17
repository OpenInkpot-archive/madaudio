THEMES=$(wildcard *.edc)
COMPILED_THEMES=$(patsubst %.edc,%.edj,$(THEMES))

theme_dir=$(datadir)/madaudio

#%.edj: %.edc
#	edje_cc $< $@

install-data-hook: $(COMPILED_THEMES)
	for theme in $(COMPILED_THEMES); do \
		$(mkdir_p) $(DESTDIR)$(theme_dir); \
		$(INSTALL_DATA) $$theme $(DESTDIR)$(theme_dir)/$$theme; \
	done
