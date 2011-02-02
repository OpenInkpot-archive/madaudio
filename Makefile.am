SUBDIRS = src po themes data help scripts
install-exec-hook:
	mkdir -p ${DESTDIR}/usr/share/help/madaudio
	oi-help-compiler -u $(srcdir)/help ${DESTDIR}/usr/share/help/madaudio
