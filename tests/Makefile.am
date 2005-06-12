# $Id$

SUBDIRS =								\
	data

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"tests\"					\
	$(EXO_CFLAGS)

TESTS_ENVIRONMENT =							\
	PATH_FSTAB=$(srcdir)/data/test-thunar-vfs-volume-bsd.fstab

TESTS =									\
	test-thunar-vfs-uri						\
	test-thunar-vfs-volume-bsd

check_PROGRAMS =							\
	test-thunar-vfs-uri						\
	test-thunar-vfs-volume-bsd

test_thunar_vfs_uri_SOURCES =						\
	test-thunar-vfs-uri.c

test_thunar_vfs_uri_DEPENDENCIES =					\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

test_thunar_vfs_uri_LDADD =						\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

test_thunar_vfs_volume_bsd_SOURCES =					\
	test-thunar-vfs-volume-bsd.c

test_thunar_vfs_volume_bsd_DEPENDENCIES =				\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

test_thunar_vfs_volume_bsd_LDADD =					\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
