noinst_LTLIBRARIES = libjpeg.la

HDRS = jchuff.h jdct.h jdhuff.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
	jpegint.h jpeglib.h jversion.h jsimd.h jsimddct.h turbojpeg.h

libjpeg_la_SOURCES = $(HDRS) jcapimin.c jcapistd.c jccoefct.c jccolor.c \
	jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c \
	jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c jctrans.c \
	jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c \
	jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c \
	jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c \
	jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c \
	jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c \
	jsimd.c

if WITH_SIMD

SUBDIRS = simd
libjpeg_la_LIBADD = simd/libsimd.la

endif

TSTHDRS = turbojpeg.h rrutil.h rrtimer.h

noinst_PROGRAMS = jpgtest jpegut

jpgtest_SOURCES = $(TSTHDRS) jpgtest.cxx bmp.c turbojpegl.c

jpgtest_LDADD = $(top_srcdir)/libjpeg.la

jpegut_SOURCES = $(TSTHDRS) jpegut.c bmp.c turbojpegl.c

jpegut_LDADD = $(top_srcdir)/libjpeg.la
