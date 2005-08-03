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
	thunar-clipboard-manager.c					\
	thunar-clipboard-manager.h					\
	thunar-computer-folder.c					\
	thunar-computer-folder.h					\
	thunar-desktop-model.c						\
	thunar-desktop-model.h						\
	thunar-desktop-view.c						\
	thunar-desktop-view.h						\
	thunar-desktop-window.c						\
	thunar-desktop-window.h						\
	thunar-details-view.c						\
	thunar-details-view.h						\
	thunar-details-view-icon-renderer.c				\
	thunar-details-view-icon-renderer.h				\
	thunar-details-view-text-renderer.c				\
	thunar-details-view-text-renderer.h				\
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
	thunar-location-dialog.c					\
	thunar-location-dialog.h					\
	thunar-location-entry.c						\
	thunar-location-entry.h						\
	thunar-navigator.c						\
	thunar-navigator.h						\
	thunar-path-entry.c						\
	thunar-path-entry.h						\
	thunar-preferences.c						\
	thunar-preferences.h						\
	thunar-progress-dialog.c					\
	thunar-progress-dialog.h					\
	thunar-properties-dialog.c					\
	thunar-properties-dialog.h					\
	thunar-side-pane.c						\
	thunar-side-pane.h						\
	thunar-standard-view.c						\
	thunar-standard-view.h						\
	thunar-standard-view-ui.h					\
	thunar-statusbar.c						\
	thunar-statusbar.h						\
	thunar-trash-file.c						\
	thunar-trash-file.h						\
	thunar-trash-folder.c						\
	thunar-trash-folder.h						\
	thunar-view.c							\
	thunar-view.h							\
	thunar-window.c							\
	thunar-window.h							\
	thunar-window-ui.h

Thunar_CFLAGS =								\
	$(CAIRO_CFLAGS)							\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)

Thunar_LDFLAGS =							\
	-no-undefined							\
	$(CAIRO_LIBS)							\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)

Thunar_LDADD =								\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la			\
	$(top_builddir)/thunarx/libthunarx.la

Thunar_DEPENDENCIES =							\
	$(top_builddir)/thunar-vfs/libthunar-vfs.la			\
	$(top_builddir)/thunarx/libthunarx.la

# install symlink to 'thunar'
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	( cd $(DESTDIR)$(bindir) ; ln -sf Thunar thunar )

clean-local:
	rm -f *.core core core.*

if MAINTAINER_MODE
DISTCLEANFILES =							\
	thunar-fallback-icon.c						\
	thunar-standard-view-ui.h					\
	thunar-window-ui.h

BUILT_SOURCES =								\
	thunar-fallback-icon.c						\
	thunar-standard-view-ui.h					\
	thunar-window-ui.h

thunar-fallback-icon.c: $(srcdir)/thunar-fallback-icon.png Makefile
	(echo "#include <thunar/thunar-fallback-icon.h>" && gdk-pixbuf-csource --extern --raw --stream --name=thunar_fallback_icon $(srcdir)/thunar-fallback-icon.png) > thunar-fallback-icon.c

thunar-standard-view-ui.h: Makefile $(srcdir)/thunar-standard-view-ui.xml
	exo-csource --static --name=thunar_standard_view_ui $(srcdir)/thunar-standard-view-ui.xml > thunar-standard-view-ui.h

thunar-window-ui.h: Makefile $(srcdir)/thunar-window-ui.xml
	exo-csource --static --name=thunar_window_ui $(srcdir)/thunar-window-ui.xml > thunar-window-ui.h
endif

EXTRA_DIST =								\
	thunar-fallback-icon.png					\
	thunar-standard-view-ui.xml					\
	thunar-window-ui.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
