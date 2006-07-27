# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_builddir)/plugins					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/plugins						\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DG_LOG_DOMAIN=\"thunar-tpa\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

pluginsdir = $(libexecdir)/xfce4/panel-plugins
plugins_PROGRAMS =							\
	thunar-tpa

thunar_tpa_SOURCES =							\
	main.c								\
	thunar-tpa-bindings.h						\
	thunar-tpa-icon.c						\
	thunar-tpa-icon.h

thunar_tpa_CFLAGS =							\
	$(DBUS_CFLAGS)							\
	$(EXO_CFLAGS)							\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_tpa_LDADD =							\
	$(DBUS_LIBS)							\
	$(EXO_LIBS)							\
	$(LIBXFCE4PANEL_LIBS)						\
	$(PLATFORM_LDFLAGS)

if MAINTAINER_MODE
BUILT_SOURCES =								\
	thunar-tpa-bindings.h

DISTCLEANFILES =							\
	thunar-tpa-bindings.h

thunar-tpa-bindings.h: $(srcdir)/thunar-tpa-bindings.xml Makefile
	dbus-binding-tool --mode=glib-client $(srcdir)/thunar-tpa-bindings.xml > thunar-tpa-bindings.h
endif

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = thunar-tpa.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)						\
	thunar-tpa-bindings.xml						\
	README

CLEANFILES =								\
	$(desktop_in_files)						\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
