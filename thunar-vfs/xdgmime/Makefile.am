# $Id$

INCLUDES =								\
	-DXDG_PREFIX=_thunar_vfs_xdg

noinst_LTLIBRARIES =							\
	libxdgmime.la

libxdgmime_la_SOURCES =							\
	xdgmime.c							\
	xdgmime.h							\
	xdgmimealias.c							\
	xdgmimealias.h							\
	xdgmimecache.c							\
	xdgmimecache.h							\
	xdgmimeglob.c							\
	xdgmimeglob.h							\
	xdgmimeint.c							\
	xdgmimeint.h							\
	xdgmimemagic.c							\
	xdgmimemagic.h							\
	xdgmimeparent.c							\
	xdgmimeparent.h

TESTS =									\
	test-mime

check_PROGRAMS =							\
	test-mime

test_mime_SOURCES =							\
	test-mime.c

test_mime_DEPENDENCIES =						\
	$(top_builddir)/thunar-vfs/xdgmime/libxdgmime.la

test_mime_LDADD =							\
	$(top_builddir)/thunar-vfs/xdgmime/libxdgmime.la

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
