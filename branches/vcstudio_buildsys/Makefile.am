SUBDIRS = common po

if BUILD_WIN
SUBDIRS += win
else
SUBDIRS += unix
endif

ACLOCAL_AMFLAGS = -I m4

if X86_64

udmg: all
	sh $(srcdir)/release/makemacpkg ${PACKAGE_NAME} ${VERSION} ${BUILD} \
		${srcdir} universal

endif

dmg: all
	sh $(srcdir)/release/makemacpkg ${PACKAGE_NAME} ${VERSION} ${BUILD} ${srcdir}
