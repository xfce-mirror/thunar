# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	$(PLATFORM_CFLAGS)

noinst_LTLIBRARIES =							\
	libtdb.la

libtdb_la_CFLAGS =							\
	-D_XOPEN_SOURCE=500

libtdb_la_SOURCES =							\
	spinlock.c							\
	spinlock.h							\
	tdb.c								\
	tdb.h								\
	tdbconfig.h

noinst_PROGRAMS =							\
	tdbtool

tdbtool_SOURCES =							\
	tdbtool.c

tdbtool_DEPENDENCIES =							\
	$(top_builddir)/tdb/libtdb.la

tdbtool_LDADD =								\
	$(top_builddir)/tdb/libtdb.la

TESTS =									\
	tdbspeed							\
	tdbtorture

check_PROGRAMS =							\
	tdbspeed							\
	tdbtorture

tdbspeed_SOURCES =							\
	tdbspeed.c

tdbspeed_DEPENDENCIES =							\
	$(top_builddir)/tdb/libtdb.la

tdbspeed_LDADD =							\
	$(top_builddir)/tdb/libtdb.la

tdbtorture_CFLAGS =							\
	-DNLOOPS=1000							\
	-DNPROCS=10

tdbtorture_SOURCES =							\
	tdbtorture.c

tdbtorture_DEPENDENCIES =						\
	$(top_builddir)/tdb/libtdb.la

tdbtorture_LDADD =							\
	$(top_builddir)/tdb/libtdb.la

CLEANFILES =								\
	test.tdb							\
	torture.tdb

EXTRA_DIST =								\
	README

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
