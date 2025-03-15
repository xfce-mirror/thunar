AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"thunar-wallpaper-plugin\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-$(THUNARX_VERSION_API)
extensions_LTLIBRARIES =						\
	thunar-wallpaper-plugin.la

thunar_wallpaper_plugin_la_SOURCES =					\
	twp-provider.h							\
	twp-provider.c							\
	thunar-wallpaper-plugin.c

thunar_wallpaper_plugin_la_CFLAGS =					\
	$(GLIB_CFLAGS)							\
	$(GTK_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_wallpaper_plugin_la_LDFLAGS =					\
	-avoid-version							\
	-export-dynamic							\
	-no-undefined							\
	-export-symbols-regex "^thunar_extension_(shutdown|initialize|list_types)" \
	-module								\
	$(PLATFORM_LDFLAGS)

thunar_wallpaper_plugin_la_LIBADD =					\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la	\
	$(GLIB_LIBS)							\
	$(GTK_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(XFCONF_LIBS)						\
	$(LIBX11_LIBS)

thunar_wallpaper_plugin_la_DEPENDENCIES =				\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

EXTRA_DIST =								\
	README.md \
	meson.build \
	$(NULL)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
