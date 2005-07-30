# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunarx\"

noinst_LTLIBRARIES =							\
	libthunarx.la

libthunarx_la_SOURCES =							\
	thunarx-gdk-pixbuf-extensions.c					\
	thunarx-gdk-pixbuf-extensions.h					\
	thunarx-gtk-extensions.c					\
	thunarx-gtk-extensions.h

libthunarx_la_CFLAGS =							\
	$(EXO_CFLAGS)

libthunarx_la_LDFLAGS =							\
	-no-undefined

libthunarx_la_LIBADD =							\
	$(EXO_LIBS)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
