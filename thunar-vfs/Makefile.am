# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunar-vfs\"

lib_LTLIBRARIES =							\
	libthunar-vfs.la

libthunar_vfs_la_SOURCES =						\
	thunar-vfs-info.c						\
	thunar-vfs-info.h						\
	thunar-vfs-monitor.c						\
	thunar-vfs-monitor.h						\
	thunar-vfs-util.c						\
	thunar-vfs-util.h						\
	thunar-vfs-uri.c						\
	thunar-vfs-uri.h						\
	thunar-vfs.h

libthunar_vfs_la_CFLAGS =						\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)

libthunar_vfs_la_LDFLAGS =						\
	-no-undefined							\
	$(EXO_LIBS)							\
	$(GTHREADS_LIBS)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
