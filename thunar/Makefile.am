# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"Thunar\"

bin_PROGRAMS =								\
	Thunar

Thunar_SOURCES =							\
	main.c								\
	thunar-application.c						\
	thunar-application.h						\
	thunar-computer-folder.c					\
	thunar-computer-folder.h					\
	thunar-desktop-view.c						\
	thunar-desktop-view.h						\
	thunar-details-view.c						\
	thunar-details-view.h						\
	thunar-details-view-icon-renderer.c				\
	thunar-details-view-icon-renderer.h				\
	thunar-fallback-icon.c						\
	thunar-fallback-icon.h						\
	thunar-favourites-model.c					\
	thunar-favourites-model.h					\
	thunar-favourites-pane.c					\
	thunar-favourites-pane.h					\
	thunar-favourites-view.c					\
	thunar-favourites-view.h					\
	thunar-file.c							\
	thunar-file.h							\
	thunar-folder.c							\
	thunar-folder.h							\
	thunar-icon-factory.c						\
	thunar-icon-factory.h						\
	thunar-icon-view.c						\
	thunar-icon-view.h						\
	thunar-list-model.c						\
	thunar-list-model.h						\
	thunar-local-file.c						\
	thunar-local-file.h						\
	thunar-local-folder.c						\
	thunar-local-folder.h						\
	thunar-location-bar.c						\
	thunar-location-bar.h						\
	thunar-location-buttons.c					\
	thunar-location-buttons.h					\
	thunar-navigator.c						\
	thunar-navigator.h						\
	thunar-preferences.c						\
	thunar-preferences.h						\
	thunar-side-pane.c						\
	thunar-side-pane.h						\
	thunar-statusbar.c						\
	thunar-statusbar.h						\
	thunar-trash-file.c						\
	thunar-trash-file.h						\
	thunar-trash-folder.c						\
	thunar-trash-folder.h						\
	thunar-view.c							\
	thunar-view.h							\
	thunar-window.c							\
	thunar-window.h

Thunar_CFLAGS =								\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)

Thunar_LDFLAGS =							\
	-no-undefined							\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)

Thunar_LDADD =								\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

Thunar_DEPENDENCIES =							\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la

# install symlink to 'thunar'
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	( cd $(DESTDIR)$(bindir) ; ln -sf Thunar thunar )

clean-local:
	rm -f *.core core core.*

if MAINTAINER_MODE
DISTCLEANFILES = thunar-fallback-icon.c
BUILT_SOURCES = thunar-fallback-icon.c

thunar-fallback-icon.c: $(srcdir)/thunar-fallback-icon.png Makefile
	(echo "#include <thunar/thunar-fallback-icon.h>" && gdk-pixbuf-csource --extern --raw --stream --name=thunar_fallback_icon $(srcdir)/thunar-fallback-icon.png) > thunar-fallback-icon.c
endif

EXTRA_DIST =								\
	thunar-fallback-icon.png

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
