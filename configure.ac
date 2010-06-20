AC_PREREQ(2.57)
AC_INIT(madaudio, 0.1, avn@daemon.hole.ru)
AM_INIT_AUTOMAKE(foreign)

AC_PROG_CC

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(EFL, [eina-0 ecore-evas ecore efreet efreet-mime edje])
PKG_CHECK_MODULES(LIBEOI, [libeoi])
PKG_CHECK_MODULES(LIBKEYS, [libkeys])
PKG_CHECK_MODULES(LIBLOPS, [liblops])

AC_OUTPUT(Makefile src/Makefile themes/Makefile themes/azbooka/Makefile
themes/default/Makefile po/Makefile data/Makefile 
help/Makefile scripts/Makefile codecs/Makefile)
