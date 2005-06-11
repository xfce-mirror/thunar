# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunar-tests\"					\
	$(EXO_CFLAGS)

TESTS =									\
	test-thunar-vfs-uri

check_PROGRAMS =							\
	test-thunar-vfs-uri

test_thunar_vfs_uri_SOURCES =						\
	test-thunar-vfs-uri.c

test_thunar_vfs_uri_DEPENDENCIES =					\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

test_thunar_vfs_uri_LDADD =						\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
