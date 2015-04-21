# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunar\"					\
	-DLIBDIR=\"$(libdir)\"						\
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DTHUNAR_VERSION_API=\"$(THUNAR_VERSION_API)\"			\
	-DSN_API_NOT_YET_FROZEN						\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE				\
	$(PLATFORM_CPPFLAGS)

bin_SCRIPTS =								\
	thunar-settings

bin_PROGRAMS =								\
	thunar

thunar_built_sources =							\
	thunar-marshal.c						\
	thunar-marshal.h						\
	thunar-abstract-icon-view-ui.h					\
	thunar-details-view-ui.h					\
	thunar-launcher-ui.h						\
	thunar-location-buttons-ui.h					\
	thunar-shortcuts-pane-ui.h					\
	thunar-renamer-dialog-ui.h					\
	thunar-standard-view-ui.h					\
	thunar-thumbnail-frame.c					\
	thunar-window-ui.h


thunar_SOURCES =							\
	$(thunar_include_HEADERS)					\
	$(thunar_built_sources)						\
	$(thunar_dbus_sources)						\
	main.c								\
	thunar-abstract-dialog.c					\
	thunar-abstract-dialog.h					\
	thunar-abstract-icon-view.c					\
	thunar-abstract-icon-view.h					\
	thunar-application.c						\
	thunar-application.h						\
	thunar-browser.c						\
	thunar-browser.h						\
	thunar-chooser-button.c						\
	thunar-chooser-button.h						\
	thunar-chooser-dialog.c						\
	thunar-chooser-dialog.h						\
	thunar-chooser-model.c						\
	thunar-chooser-model.h						\
	thunar-clipboard-manager.c					\
	thunar-clipboard-manager.h					\
	thunar-column-editor.c						\
	thunar-column-editor.h						\
	thunar-column-model.c						\
	thunar-column-model.h						\
	thunar-compact-view.c						\
	thunar-compact-view.h						\
	thunar-component.c						\
	thunar-component.h						\
	thunar-create-dialog.c						\
	thunar-create-dialog.h						\
	thunar-deep-count-job.h						\
	thunar-deep-count-job.c						\
	thunar-details-view.c						\
	thunar-details-view.h						\
	thunar-dialogs.c						\
	thunar-dialogs.h						\
	thunar-device.c							\
	thunar-device.h							\
	thunar-device-monitor.c						\
	thunar-device-monitor.h						\
	thunar-dnd.c							\
	thunar-dnd.h							\
	thunar-emblem-chooser.c						\
	thunar-emblem-chooser.h						\
	thunar-enum-types.c						\
	thunar-enum-types.h						\
	thunar-exec.c							\
	thunar-exec.h							\
	thunar-file.c							\
	thunar-file.h							\
	thunar-file-monitor.c						\
	thunar-file-monitor.h						\
	thunar-folder.c							\
	thunar-folder.h							\
	thunar-gdk-extensions.c						\
	thunar-gdk-extensions.h						\
	thunar-gio-extensions.c						\
	thunar-gio-extensions.h						\
	thunar-gobject-extensions.c					\
	thunar-gobject-extensions.h					\
	thunar-gtk-extensions.c						\
	thunar-gtk-extensions.h						\
	thunar-history-action.c						\
	thunar-history-action.h						\
	thunar-history.c						\
	thunar-history.h						\
	thunar-ice.c							\
	thunar-ice.h							\
	thunar-icon-factory.c						\
	thunar-icon-factory.h						\
	thunar-icon-renderer.c						\
	thunar-icon-renderer.h						\
	thunar-icon-view.c						\
	thunar-icon-view.h						\
	thunar-image.c							\
	thunar-image.h							\
	thunar-io-jobs.c						\
	thunar-io-jobs.h						\
	thunar-io-jobs-util.c						\
	thunar-io-jobs-util.h						\
	thunar-io-scan-directory.c					\
	thunar-io-scan-directory.h					\
	thunar-job.c							\
	thunar-job.h							\
	thunar-launcher.c						\
	thunar-launcher.h						\
	thunar-list-model.c						\
	thunar-list-model.h						\
	thunar-location-bar.c						\
	thunar-location-bar.h						\
	thunar-location-button.c					\
	thunar-location-button.h					\
	thunar-location-buttons.c					\
	thunar-location-buttons.h					\
	thunar-location-dialog.c					\
	thunar-location-dialog.h					\
	thunar-location-entry.c						\
	thunar-location-entry.h						\
	thunar-misc-jobs.c						\
	thunar-misc-jobs.h						\
	thunar-notify.c							\
	thunar-notify.h							\
	thunar-navigator.c						\
	thunar-navigator.h						\
	thunar-pango-extensions.c					\
	thunar-pango-extensions.h					\
	thunar-path-entry.c						\
	thunar-path-entry.h						\
	thunar-permissions-chooser.c					\
	thunar-permissions-chooser.h					\
	thunar-preferences-dialog.c					\
	thunar-preferences-dialog.h					\
	thunar-preferences.c						\
	thunar-preferences.h						\
	thunar-private.h						\
	thunar-progress-dialog.c					\
	thunar-progress-dialog.h					\
	thunar-progress-view.c						\
	thunar-progress-view.h						\
	thunar-properties-dialog.c					\
	thunar-properties-dialog.h					\
	thunar-renamer-dialog.c						\
	thunar-renamer-dialog.h						\
	thunar-renamer-model.c						\
	thunar-renamer-model.h						\
	thunar-renamer-pair.c						\
	thunar-renamer-pair.h						\
	thunar-renamer-progress.c					\
	thunar-renamer-progress.h					\
	thunar-sendto-model.c						\
	thunar-sendto-model.h						\
	thunar-session-client.c						\
	thunar-session-client.h						\
	thunar-shortcuts-icon-renderer.c				\
	thunar-shortcuts-icon-renderer.h				\
	thunar-shortcuts-model.c					\
	thunar-shortcuts-model.h					\
	thunar-shortcuts-pane.c						\
	thunar-shortcuts-pane.h						\
	thunar-shortcuts-view.c						\
	thunar-shortcuts-view.h						\
	thunar-side-pane.c						\
	thunar-side-pane.h						\
	thunar-simple-job.c						\
	thunar-simple-job.h						\
	thunar-size-label.c						\
	thunar-size-label.h						\
	thunar-standard-view.c						\
	thunar-standard-view.h						\
	thunar-statusbar.c						\
	thunar-statusbar.h						\
	thunar-stock.c							\
	thunar-stock.h							\
	thunar-templates-action.c					\
	thunar-templates-action.h					\
	thunar-text-renderer.c						\
	thunar-text-renderer.h						\
	thunar-thumbnail-cache.c					\
	thunar-thumbnail-cache.h					\
	thunar-thumbnailer.c						\
	thunar-thumbnailer.h						\
	thunar-thumbnail-frame.h					\
	thunar-transfer-job.c						\
	thunar-transfer-job.h						\
	thunar-trash-action.c						\
	thunar-trash-action.h						\
	thunar-tree-model.c						\
	thunar-tree-model.h						\
	thunar-tree-pane.c						\
	thunar-tree-pane.h						\
	thunar-tree-view.c						\
	thunar-tree-view.h						\
	thunar-user.c							\
	thunar-user.h							\
	thunar-util.c							\
	thunar-util.h							\
	thunar-view.c							\
	thunar-view.h							\
	thunar-window.c							\
	thunar-window.h

thunar_CFLAGS =								\
	$(EXO_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GUDEV_CFLAGS)							\
	$(LIBNOTIFY_CFLAGS)						\
	$(LIBSM_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_LDFLAGS =							\
	-no-undefined							\
	$(LIBSM_LDFLAGS)						\
	$(PLATFORM_LDFLAGS)

thunar_LDADD =								\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(EXO_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GUDEV_LIBS)							\
	$(LIBNOTIFY_LIBS)						\
	$(LIBSM_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)

thunar_DEPENDENCIES =							\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

if HAVE_DBUS
thunar_built_sources +=							\
	thunar-dbus-service-infos.h					\
	thunar-thumbnail-cache-proxy.h					\
	thunar-thumbnailer-proxy.h

thunar_dbus_sources =							\
	thunar-dbus-client.c						\
	thunar-dbus-client.h						\
	thunar-dbus-service.c						\
	thunar-dbus-service.h						\
	thunar-thumbnail-cache-proxy.h					\
	thunar-thumbnailer-proxy.h

thunar_CFLAGS +=							\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	$(DBUS_CFLAGS)

thunar_LDADD +=								\
	$(DBUS_LIBS)
endif

if HAVE_GIO_UNIX
thunar_CFLAGS +=							\
	$(GIO_UNIX_CFLAGS)

thunar_LDADD +=								\
	$(GIO_UNIX_LIBS)
endif

desktopdir = $(datadir)/applications
desktop_in_files = thunar-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

# install symlink to 'thunar'
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	-( cd $(DESTDIR)$(bindir) ; test -x Thunar || ln -sf thunar Thunar )

clean-local:
	rm -f *.core core core.* *.o

DISTCLEANFILES =							\
	thunar-settings.desktop

if MAINTAINER_MODE
CLEANFILES =								\
	xgen-tmc							\
	xgen-tmh

DISTCLEANFILES +=							\
	$(thunar_built_sources)						\
	stamp-thunar-marshal.h

BUILT_SOURCES =								\
	$(thunar_built_sources)

if HAVE_DBUS
thunar-dbus-service-infos.h: $(srcdir)/thunar-dbus-service-infos.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=thunar_dbus_service --mode=glib-server $(srcdir)/thunar-dbus-service-infos.xml > thunar-dbus-service-infos.h

thunar-thumbnailer-proxy.h: $(srcdir)/thunar-thumbnailer-dbus.xml Makefile
	$(AM_V_GEN) ( \
		dbus-binding-tool --mode=glib-client \
			$(srcdir)/thunar-thumbnailer-dbus.xml > thunar-thumbnailer-proxy.h.tmp \
		&& $(SED) -e 's/org_freedesktop_thumbnails_Thumbnailer1/thunar_thumbnailer_proxy/g' \
			thunar-thumbnailer-proxy.h.tmp > thunar-thumbnailer-proxy.h \
		&& $(RM) thunar-thumbnailer-proxy.h.tmp \
	)

thunar-thumbnail-cache-proxy.h: $(srcdir)/thunar-thumbnail-cache-dbus.xml Makefile
	$(AM_V_GEN) ( \
		dbus-binding-tool --mode=glib-client \
			$(srcdir)/thunar-thumbnail-cache-dbus.xml > thunar-thumbnail-cache-proxy.h.tmp \
		&& $(SED) -e 's/org_freedesktop_thumbnails_Cache1/thunar_thumbnail_cache_proxy/g' \
			thunar-thumbnail-cache-proxy.h.tmp > thunar-thumbnail-cache-proxy.h \
		&& $(RM) thunar-thumbnail-cache-proxy.h.tmp \
	)
endif

thunar-thumbnail-frame.c: $(srcdir)/thunar-thumbnail-frame.png Makefile
	$(AM_V_GEN) (echo "#include <thunar/thunar-thumbnail-frame.h>" && gdk-pixbuf-csource --extern --raw --stream --name=thunar_thumbnail_frame $(srcdir)/thunar-thumbnail-frame.png) > thunar-thumbnail-frame.c

thunar-abstract-icon-view-ui.h: Makefile $(srcdir)/thunar-abstract-icon-view-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_abstract_icon_view_ui $(srcdir)/thunar-abstract-icon-view-ui.xml > thunar-abstract-icon-view-ui.h

thunar-details-view-ui.h: Makefile $(srcdir)/thunar-details-view-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_details_view_ui $(srcdir)/thunar-details-view-ui.xml > thunar-details-view-ui.h

thunar-launcher-ui.h: Makefile $(srcdir)/thunar-launcher-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_launcher_ui $(srcdir)/thunar-launcher-ui.xml > thunar-launcher-ui.h

thunar-location-buttons-ui.h: Makefile $(srcdir)/thunar-location-buttons-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_location_buttons_ui $(srcdir)/thunar-location-buttons-ui.xml > thunar-location-buttons-ui.h

thunar-shortcuts-pane-ui.h: Makefile $(srcdir)/thunar-shortcuts-pane-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_shortcuts_pane_ui $(srcdir)/thunar-shortcuts-pane-ui.xml > thunar-shortcuts-pane-ui.h

thunar-renamer-dialog-ui.h: Makefile $(srcdir)/thunar-renamer-dialog-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_renamer_dialog_ui $(srcdir)/thunar-renamer-dialog-ui.xml > thunar-renamer-dialog-ui.h

thunar-standard-view-ui.h: Makefile $(srcdir)/thunar-standard-view-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_standard_view_ui $(srcdir)/thunar-standard-view-ui.xml > thunar-standard-view-ui.h

thunar-window-ui.h: Makefile $(srcdir)/thunar-window-ui.xml
	$(AM_V_GEN) exo-csource --strip-comments --strip-content --static --name=thunar_window_ui $(srcdir)/thunar-window-ui.xml > thunar-window-ui.h

thunar-marshal.h: stamp-thunar-marshal.h
	@true

stamp-thunar-marshal.h: thunar-marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#ifndef __THUNAR_MARSHAL_H__" > xgen-tmh \
		&& echo "#define __THUNAR_MARSHAL_H__" >> xgen-tmh \
		&& ( glib-genmarshal \
			--prefix=_thunar_marshal \
			--header thunar-marshal.list ) >> xgen-tmh \
		&& echo "#endif /* !__THUNAR_MARSHAL_H__ */" >> xgen-tmh \
		&& ( cmp -s xgen-tmh thunar-marshal.h || cp xgen-tmh thunar-marshal.h ) \
		&& rm -f xgen-tmh \
		&& echo timestamp > $(@F) \
	)

thunar-marshal.c: thunar-marshal.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#include <thunar/thunar-marshal.h>" > xgen-tmc \
		&& ( glib-genmarshal \
			--prefix=_thunar_marshal \
			--body thunar-marshal.list ) >> xgen-tmc \
		&& cp xgen-tmc thunar-marshal.c \
		&& rm -f xgen-tmc \
	)
endif

EXTRA_DIST =								\
	thunar-abstract-icon-view-ui.xml				\
	thunar-dbus-service-infos.xml					\
	thunar-details-view-ui.xml					\
	thunar-launcher-ui.xml						\
	thunar-location-buttons-ui.xml					\
	thunar-marshal.list						\
	thunar-renamer-dialog-ui.xml					\
	thunar-standard-view-ui.xml					\
	thunar-thumbnail-frame.png					\
	thunar-window-ui.xml						\
	thunar-settings							\
	$(desktop_in_files)
