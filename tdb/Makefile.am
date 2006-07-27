# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	$(PLATFORM_CFLAGS)

noinst_LTLIBRARIES =							\
	libtdb.la

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
	libtdb.la

tdbtool_LDADD =								\
	libtdb.la

TESTS =									\
	tdbspeed							\
	tdbtorture

check_PROGRAMS =							\
	tdbspeed							\
	tdbtorture

tdbspeed_SOURCES =							\
	tdbspeed.c

tdbspeed_DEPENDENCIES =							\
	libtdb.la

tdbspeed_LDADD =							\
	libtdb.la

tdbtorture_CFLAGS =							\
	-DNLOOPS=1000							\
	-DNPROCS=10

tdbtorture_SOURCES =							\
	tdbtorture.c

tdbtorture_DEPENDENCIES =						\
	libtdb.la

tdbtorture_LDADD =							\
	libtdb.la

CLEANFILES =								\
	test.tdb							\
	torture.tdb

EXTRA_DIST =								\
	README

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
