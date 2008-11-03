INCLUDES =										\
	-I$(top_builddir)							\
	-I$(top_srcdir)								\
	-DG_LOG_DOMAIN=\"thunar-wallpaper-plugin\"	\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DLIBEXECDIR=\"$(libexecdir)\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-1
extensions_LTLIBRARIES =					\
	thunar-wallpaper-plugin.la

thunar_wallpaper_plugin_la_SOURCES =		\
	twp-provider.h							\
	twp-provider.c							\
	thunar-wallpaper-plugin.c

thunar_wallpaper_plugin_la_CFLAGS =			\
	$(PLATFORM_CFLAGS)						\
	$(THUNARX_CFLAGS)						\
	$(EXO_CFLAGS)							\
	$(THUNAR_VFS_CFLAGS)

thunar_wallpaper_plugin_la_LDFLAGS =		\
	-avoid-version							\
	-export-dynamic							\
	-module									\
	$(PLATFORM_LDFLAGS)						\
	$(THUNARX_LDFLAGS)						\
	$(THUNAR_VFS_LDFLAGS)
