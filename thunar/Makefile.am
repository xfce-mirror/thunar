# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"Thunar\"					\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DTHUNAR_VERSION_API=\"$(THUNAR_VERSION_API)\"			\
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS =								\
	Thunar

thunar_built_sources =							\
	thunar-marshal.c						\
	thunar-marshal.h

Thunar_SOURCES =							\
	$(thunar_include_HEADERS)					\
	$(thunar_built_sources)						\
	main.c								\
	thunar-application.c						\
	thunar-application.h						\
	thunar-chooser-dialog.c						\
	thunar-chooser-dialog.h						\
	thunar-chooser-model.c						\
	thunar-chooser-model.h						\
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
	thunar-dnd.c							\
	thunar-dnd.h							\
	thunar-extension-manager.c					\
	thunar-extension-manager.h					\
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
	thunar-gdk-pixbuf-extensions.c					\
	thunar-gdk-pixbuf-extensions.h					\
	thunar-gtk-extensions.c						\
	thunar-gtk-extensions.h						\
	thunar-icon-factory.c						\
	thunar-icon-factory.h						\
	thunar-icon-renderer.c						\
	thunar-icon-renderer.h						\
	thunar-icon-view.c						\
	thunar-icon-view.h						\
	thunar-icon-view-ui.h						\
	thunar-launcher.c						\
	thunar-launcher.h						\
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
	thunar-open-with-action.c					\
	thunar-open-with-action.h					\
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
	thunar-text-renderer.c						\
	thunar-text-renderer.h						\
	thunar-thumbnail-frame.c					\
	thunar-thumbnail-frame.h					\
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
	$(GTHREAD_CFLAGS)						\
	$(PLATFORM_CFLAGS)

Thunar_LDFLAGS =							\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

Thunar_LDADD =								\
	$(top_builddir)/thunar-vfs/libthunar-vfs-$(THUNAR_VERSION_API).la \
	$(top_builddir)/thunarx/libthunarx-$(THUNAR_VERSION_API).la	\
	$(CAIRO_LIBS)							\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)

Thunar_DEPENDENCIES =							\
	$(top_builddir)/thunar-vfs/libthunar-vfs-$(THUNAR_VERSION_API).la \
	$(top_builddir)/thunarx/libthunarx-$(THUNAR_VERSION_API).la

# install symlink to 'thunar'
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	( cd $(DESTDIR)$(bindir) ; ln -sf Thunar thunar )

clean-local:
	rm -f *.core core core.*

if MAINTAINER_MODE
CLEANFILES =								\
	xgen-tvmc							\
	xgen-tvmh

DISTCLEANFILES =							\
	$(thunar_built_sources)						\
	stamp-thunar-marshal.h						\
	thunar-fallback-icon.c						\
	thunar-icon-view-ui.h						\
	thunar-standard-view-ui.h					\
	thunar-thumbnail-frame.h					\
	thunar-window-ui.h

BUILT_SOURCES =								\
	$(thunar_built_sources)						\
	thunar-fallback-icon.c						\
	thunar-icon-view-ui.h						\
	thunar-standard-view-ui.h					\
	thunar-thumbnail-frame.c					\
	thunar-window-ui.h

thunar-fallback-icon.c: $(srcdir)/thunar-fallback-icon.png Makefile
	(echo "#include <thunar/thunar-fallback-icon.h>" && gdk-pixbuf-csource --extern --raw --stream --name=thunar_fallback_icon $(srcdir)/thunar-fallback-icon.png) > thunar-fallback-icon.c

thunar-thumbnail-frame.c: $(srcdir)/thunar-thumbnail-frame.png Makefile
	(echo "#include <thunar/thunar-thumbnail-frame.h>" && gdk-pixbuf-csource --extern --raw --stream --name=thunar_thumbnail_frame $(srcdir)/thunar-thumbnail-frame.png) > thunar-thumbnail-frame.c

thunar-icon-view-ui.h: Makefile $(srcdir)/thunar-icon-view-ui.xml
	exo-csource --static --name=thunar_icon_view_ui $(srcdir)/thunar-icon-view-ui.xml > thunar-icon-view-ui.h

thunar-standard-view-ui.h: Makefile $(srcdir)/thunar-standard-view-ui.xml
	exo-csource --static --name=thunar_standard_view_ui $(srcdir)/thunar-standard-view-ui.xml > thunar-standard-view-ui.h

thunar-window-ui.h: Makefile $(srcdir)/thunar-window-ui.xml
	exo-csource --static --name=thunar_window_ui $(srcdir)/thunar-window-ui.xml > thunar-window-ui.h

thunar-marshal.h: stamp-thunar-marshal.h
	@true
stamp-thunar-marshal.h: thunar-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_thunar_marshal \
		--header thunar-marshal.list ) >> xgen-tmh \
	&& ( cmp -s xgen-tmh thunar-marshal.h || cp xgen-tmh thunar-marshal.h ) \
	&& rm -f xgen-tmh \
	&& echo timestamp > $(@F)

thunar-marshal.c: thunar-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_thunar_marshal \
		--body thunar-marshal.list ) >> xgen-tmc \
	&& cp xgen-tmc thunar-marshal.c \
	&& rm -f xgen-tmc
endif

EXTRA_DIST =								\
	thunar-fallback-icon.png					\
	thunar-icon-view-ui.xml						\
	thunar-marshal.list						\
	thunar-standard-view-ui.xml					\
	thunar-thumbnail-frame.png					\
	thunar-window-ui.xml

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
