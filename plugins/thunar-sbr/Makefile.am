# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_builddir)/plugins					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/plugins						\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DG_LOG_DOMAIN=\"thunar-sbr\"					\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNAR_VERSION_API)
extensions_LTLIBRARIES =						\
	thunar-sbr.la

thunar_sbr_la_SOURCES =							\
	thunar-sbr-case-renamer.c					\
	thunar-sbr-case-renamer.h					\
	thunar-sbr-enum-types.c						\
	thunar-sbr-enum-types.h						\
	thunar-sbr-insert-renamer.c					\
	thunar-sbr-insert-renamer.h					\
	thunar-sbr-number-renamer.c					\
	thunar-sbr-number-renamer.h					\
	thunar-sbr-plugin.c						\
	thunar-sbr-provider.c						\
	thunar-sbr-provider.h						\
	thunar-sbr-remove-renamer.c					\
	thunar-sbr-remove-renamer.h					\
	thunar-sbr-replace-renamer.c					\
	thunar-sbr-replace-renamer.h

thunar_sbr_la_CFLAGS =							\
	$(EXO_CFLAGS)							\
	$(PCRE_CFLAGS)							\
	$(PLATFORM_CFLAGS)

thunar_sbr_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module								\
	$(PLATFORM_LDFLAGS)

thunar_sbr_la_LIBADD =							\
	$(PCRE_LIBS)

EXTRA_DIST =								\
	README

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
