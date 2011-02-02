top_builddir = ..

# -- Build

POFILES = $(wildcard *.po)
MOFILES = $(patsubst %.po,%.mo,$(POFILES)) 

DESKTOP_INS=$(wildcard $(srcdir)/*.desktop.in)
DESKTOP_INS_H=$(patsubst %,$(abs_builddir)/%.h, $(DESKTOP_INS))
DESKTOPS = $(patsubst %.in,$(abs_builddir)/%, $(DESKTOP_INS))

CLEANFILES = $(MOFILES) $(DESKTOPS) $(DESKTOP_INS_H)

localedir=$(datadir)/locale

MSGFMT = msgfmt

all: $(MOFILES)

%.mo: %.po
	$(MSGFMT) -o t-$@ $< && mv t-$@ $@

install-data-hook: $(MOFILES)
	for cat in "" $(MOFILES); do \
	  if test -z "$$cat"; then continue; fi; \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed -e 's/\.mo$$//'`; \
	  dir=$(localedir)/$$lang/LC_MESSAGES; \
	  $(mkdir_p) $(DESTDIR)$$dir; \
	  if test -r $$cat; then realcat=$$cat; else realcat=$(srcdir)/$$cat; fi; \
	  $(INSTALL) -m 644 $$realcat $(DESTDIR)$$dir/$(PACKAGE).mo; \
	 done

desktopdir = $(datarootdir)/applications
desktop_DATA = madaudio.desktop recorder.desktop

codecsdesktopdir = $(pkgdatadir)/codecs
codecsdesktop_DATA = hq.desktop mp3.desktop

%.desktop: %.desktop.in $(POFILES)
	intltool-merge -d -u $(srcdir) $< $@

# -- Manage translations

SOURCES = $(shell cd $(top_builddir); find . -name '*.[hc]')

$(PACKAGE).pot: $(GENFILES)
	cd $(top_builddir); xgettext -k_ -kN_ -d $(PACKAGE) \
		--foreign-user \
		--package-name="$(PACKAGE)" --package-version="$(VERSION)" \
		--msgid-bugs-address="$(PACKAGE_BUGREPORT)" \
		-o $(abs_srcdir)/$@ $(shell cd $(top_builddir); find . -name '*.[hc]')

# .desktop

GENFILES = $(DESKTOP_INS_H)

$(abs_builddir)/%.desktop.in.h: %.desktop.in
	intltool-extract --TYPE="gettext/ini" $<

# .tx

push-source: $(PACKAGE).pot
	tx push -s

push-translations:
	if [ "$fORCE" = yes ]; then tx push -t; fi

pull-translations:
	tx pull -a

.PHONY: $(PACKAGE).pot
