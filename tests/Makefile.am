# $Id$

SUBDIRS =								\
	data

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"tests\"					\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)

TESTS =									\
	test-thunar-vfs-path

check_PROGRAMS =							\
	test-thunar-vfs-path

test_thunar_vfs_path_SOURCES =						\
	test-thunar-vfs-path.c

test_thunar_vfs_path_DEPENDENCIES =					\
	$(top_builddir)/thunar-vfs/libthunar-vfs-$(THUNAR_VERSION_API).la

test_thunar_vfs_path_LDADD =						\
	$(GTHREAD_LIBS)							\
	$(top_builddir)/thunar-vfs/libthunar-vfs-$(THUNAR_VERSION_API).la

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
