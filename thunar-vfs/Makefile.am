# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"thunar-vfs\"					\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSN_API_NOT_YET_FROZEN						\
	-DTHUNAR_VFS_COMPILATION					\
	-DTHUNAR_VFS_VERSION_API=\"$(THUNAR_VERSION_API)\"		\
	$(PLATFORM_CPPFLAGS)

lib_LTLIBRARIES =							\
	libthunar-vfs-1.la

libthunar_vfs_headers =							\
	thunar-vfs-config.h						\
	thunar-vfs-info.h						\
	thunar-vfs-interactive-job.h					\
	thunar-vfs-job.h						\
	thunar-vfs-mime-action.h					\
	thunar-vfs-mime-application.h					\
	thunar-vfs-mime-database.h					\
	thunar-vfs-mime-handler.h					\
	thunar-vfs-mime-info.h						\
	thunar-vfs-monitor.h						\
	thunar-vfs-path.h						\
	thunar-vfs-thumb.h						\
	thunar-vfs-types.h						\
	thunar-vfs-user.h						\
	thunar-vfs-util.h						\
	thunar-vfs-volume.h

libthunar_vfs_io_sources =						\
	thunar-vfs-io-jobs.c						\
	thunar-vfs-io-jobs.h						\
	thunar-vfs-io-local.c						\
	thunar-vfs-io-local.h						\
	thunar-vfs-io-local-xfer.c					\
	thunar-vfs-io-local-xfer.h					\
	thunar-vfs-io-ops.c						\
	thunar-vfs-io-ops.h						\
	thunar-vfs-io-scandir.c						\
	thunar-vfs-io-scandir.h						\
	thunar-vfs-io-trash.c						\
	thunar-vfs-io-trash.h

libthunar_vfs_os_sources =						\
	thunar-vfs-os.h
if THUNAR_VFS_OS_IMPL_BSD
libthunar_vfs_os_sources +=						\
	thunar-vfs-os-bsd.c
else
libthunar_vfs_os_sources +=						\
	thunar-vfs-os-generic.c
endif

libthunar_vfs_public_built_sources =					\
	thunar-vfs-enum-types.h

libthunar_vfs_built_sources =						\
	$(libthunar_vfs_public_built_sources)				\
	thunar-vfs-alias.h						\
	thunar-vfs-aliasdef.c						\
	thunar-vfs-enum-types.c						\
	thunar-vfs-marshal.c						\
	thunar-vfs-marshal.h

libthunar_vfs_includedir = 						\
	$(includedir)/thunar-vfs-$(THUNAR_VERSION_API)/thunar-vfs
libthunar_vfs_include_HEADERS =						\
	$(libthunar_vfs_headers)					\
	$(libthunar_vfs_public_built_sources)				\
	thunar-vfs.h

libthunar_vfs_1_la_SOURCES =						\
	$(libthunar_vfs_volume_impl_sources)				\
	$(libthunar_vfs_built_sources)					\
	$(libthunar_vfs_io_sources)					\
	$(libthunar_vfs_os_sources)					\
	$(libthunar_vfs_headers)					\
	thunar-vfs-config.c						\
	thunar-vfs-deep-count-job.c					\
	thunar-vfs-deep-count-job.h					\
	thunar-vfs-exec.c						\
	thunar-vfs-exec.h						\
	thunar-vfs-info.c						\
	thunar-vfs-interactive-job.c					\
	thunar-vfs-job.c						\
	thunar-vfs-job-private.h					\
	thunar-vfs-mime-action-private.h				\
	thunar-vfs-mime-action.c					\
	thunar-vfs-mime-application.c					\
	thunar-vfs-mime-cache.c						\
	thunar-vfs-mime-cache.h						\
	thunar-vfs-mime-database.c					\
	thunar-vfs-mime-database-private.h				\
	thunar-vfs-mime-handler-private.h				\
	thunar-vfs-mime-handler.c					\
	thunar-vfs-mime-info.c						\
	thunar-vfs-mime-legacy.c					\
	thunar-vfs-mime-legacy.h					\
	thunar-vfs-mime-parser.h					\
	thunar-vfs-mime-parser.c					\
	thunar-vfs-mime-provider.c					\
	thunar-vfs-mime-provider.h					\
	thunar-vfs-mime-sniffer.c					\
	thunar-vfs-mime-sniffer.h					\
	thunar-vfs-monitor.c						\
	thunar-vfs-monitor-private.h					\
	thunar-vfs-path-private.h					\
	thunar-vfs-path.c						\
	thunar-vfs-private.c						\
	thunar-vfs-private.h						\
	thunar-vfs-simple-job.c						\
	thunar-vfs-simple-job.h						\
	thunar-vfs-thumb-jpeg.c						\
	thunar-vfs-thumb-jpeg.h						\
	thunar-vfs-thumb-private.h					\
	thunar-vfs-thumb.c						\
	thunar-vfs-transfer-job.c					\
	thunar-vfs-transfer-job.h					\
	thunar-vfs-user.c						\
	thunar-vfs-util.c						\
	thunar-vfs-volume.c						\
	thunar-vfs-volume-manager.c					\
	thunar-vfs-volume-private.h					\
	thunar-vfs.c

libthunar_vfs_1_la_CFLAGS =						\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(LIBFAM_CFLAGS)						\
	$(LIBJPEG_CFLAGS)						\
	$(LIBPNG_CFLAGS)						\
	$(LIBSTARTUP_NOTIFICATION_CFLAGS)				\
	$(PLATFORM_CFLAGS)

libthunar_vfs_1_la_LDFLAGS =						\
	-export-dynamic							\
	-export-symbols-regex "^[^_].*"					\
	-version-info $(THUNAR_VERINFO)					\
	$(PLATFORM_LDFLAGS)

libthunar_vfs_1_la_LIBADD =						\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(LIBFAM_LIBS)							\
	$(LIBJPEG_LIBS)							\
	$(LIBPNG_LIBS)							\
	$(LIBSTARTUP_NOTIFICATION_LIBS)

libexec_PROGRAMS =							\
	thunar-vfs-mime-cleaner-1					\
	thunar-vfs-pixbuf-thumbnailer-1					\
	thunar-vfs-update-thumbnailers-cache-1

thunar_vfs_mime_cleaner_1_SOURCES =					\
	thunar-vfs-mime-cleaner.c

thunar_vfs_mime_cleaner_1_CFLAGS =					\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_vfs_mime_cleaner_1_LDADD =					\
	$(LIBXFCE4UTIL_LIBS)

thunar_vfs_mime_cleaner_1_LDFLAGS =					\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

thunar_vfs_pixbuf_thumbnailer_1_SOURCES =				\
	thunar-vfs-pixbuf-thumbnailer.c

thunar_vfs_pixbuf_thumbnailer_1_CFLAGS =				\
	$(EXO_CFLAGS)							\
	$(PLATFORM_CFLAGS)

thunar_vfs_pixbuf_thumbnailer_1_LDADD =					\
	$(EXO_LIBS)

thunar_vfs_pixbuf_thumbnailer_1_LDFLAGS =				\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

thunar_vfs_update_thumbnailers_cache_1_SOURCES =			\
	thunar-vfs-update-thumbnailers-cache.c

thunar_vfs_update_thumbnailers_cache_1_CFLAGS =				\
	$(GCONF_CFLAGS)							\
	$(GDK_PIXBUF_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_vfs_update_thumbnailers_cache_1_LDADD =				\
	$(GCONF_LIBS)							\
	$(GDK_PIXBUF_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)

thunar_vfs_update_thumbnailers_cache_1_LDFLAGS =			\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

##
## The font thumbnailer can only be built if freetype 2.x is available,
## because there's not really a way to make it work w/o freetype.
##
if HAVE_FREETYPE
libexec_PROGRAMS +=							\
	thunar-vfs-font-thumbnailer-1

thunar_vfs_font_thumbnailer_1_SOURCES =					\
	thunar-vfs-font-thumbnailer.c

thunar_vfs_font_thumbnailer_1_CFLAGS =					\
	$(FREETYPE_CFLAGS)						\
	$(GDK_PIXBUF_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_vfs_font_thumbnailer_1_LDADD =					\
	$(FREETYPE_LIBS)						\
	$(GDK_PIXBUF_LIBS)

thunar_vfs_font_thumbnailer_1_LDFLAGS =					\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = thunar-vfs-1.pc

EXTRA_DIST =								\
	abicheck.sh							\
	make-thunar-vfs-alias.pl					\
	thunar-vfs.symbols						\
	thunar-vfs-alias.h						\
	thunar-vfs-aliasdef.c						\
	thunar-vfs-marshal.list						\
	thunar-vfs-volume-freebsd.c					\
	thunar-vfs-volume-freebsd.h					\
	thunar-vfs-volume-hal.c						\
	thunar-vfs-volume-hal.h						\
	thunar-vfs-volume-none.c					\
	thunar-vfs-volume-none.h

if THUNAR_VFS_VOLUME_IMPL_FREEBSD
libthunar_vfs_volume_impl_sources =					\
	thunar-vfs-volume-freebsd.c					\
	thunar-vfs-volume-freebsd.h
endif

if THUNAR_VFS_VOLUME_IMPL_HAL
libthunar_vfs_volume_impl_sources =					\
	thunar-vfs-volume-hal.c						\
	thunar-vfs-volume-hal.h

libthunar_vfs_1_la_CFLAGS +=						\
	$(HAL_CFLAGS)							\
	$(HAL_DBUS_CFLAGS)

libthunar_vfs_1_la_LIBADD +=						\
	$(HAL_LIBS)							\
	$(HAL_DBUS_LIBS)
endif

if THUNAR_VFS_VOLUME_IMPL_NONE
libthunar_vfs_volume_impl_sources =					\
	thunar-vfs-volume-none.c					\
	thunar-vfs-volume-none.h
endif


##
## Rules to auto-generate built sources
##
## This is a bit tricky with automake, and non-trivial to implement. The
## rules below seem to work fine and don't seem to break the build, but
## they are only enabled in maintainer mode, so arbitrary users don't get
## trapped in automake's oddities. Therefore we ship the autogenerated
## files as part of the dist tarball.
##
if MAINTAINER_MODE
CLEANFILES =								\
	actual-abi							\
	expected-abi							\
	xgen-tvetc							\
	xgen-tveth							\
	xgen-tvmc							\
	xgen-tvmh

DISTCLEANFILES =							\
	stamp-thunar-vfs-enum-types.h					\
	stamp-thunar-vfs-marshal.h					\
	$(libthunar_vfs_built_sources)

BUILT_SOURCES =								\
	$(libthunar_vfs_built_sources)

if HAVE_GNUC_VISIBILITY
TESTS =									\
	abicheck.sh
endif

thunar-vfs-alias.h: make-thunar-vfs-alias.pl thunar-vfs.symbols
	$(PERL) $(srcdir)/make-thunar-vfs-alias.pl < $(srcdir)/thunar-vfs.symbols > thunar-vfs-alias.h

thunar-vfs-aliasdef.c: make-thunar-vfs-alias.pl thunar-vfs.symbols
	$(PERL) $(srcdir)/make-thunar-vfs-alias.pl -def < $(srcdir)/thunar-vfs.symbols > thunar-vfs-aliasdef.c

thunar-vfs-enum-types.h: stamp-thunar-vfs-enum-types.h
	@true
stamp-thunar-vfs-enum-types.h: $(libthunar_vfs_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __THUNAR_VFS_ENUM_TYPES_H__\n#define __THUNAR_VFS_ENUM_TYPES_H__\n#include <exo/exo.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define THUNAR_VFS_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* !__THUNAR_VFS_ENUM_TYPES_H__ */" \
		$(libthunar_vfs_headers) ) >> xgen-tveth \
	&& ( cmp -s xgen-tveth thunar-vfs-enum-types.h || cp xgen-tveth thunar-vfs-enum-types.h ) \
	&& rm -f xgen-tveth \
	&& echo timestamp > $(@F)
thunar-vfs-enum-types.c: $(libthunar_vfs_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
		--fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include <thunar-vfs/thunar-vfs.h>\n#include <thunar-vfs/thunar-vfs-alias.h>" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		--ftail "\n#define __THUNAR_VFS_ENUM_TYPES_C__\n#include \"thunar-vfs-aliasdef.c\"\n" \
		$(libthunar_vfs_headers) ) >> xgen-tvetc \
	&& cp xgen-tvetc thunar-vfs-enum-types.c \
	&& rm -f xgen-tvetc

thunar-vfs-marshal.h: stamp-thunar-vfs-marshal.h
	@true
stamp-thunar-vfs-marshal.h: thunar-vfs-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_thunar_vfs_marshal \
		--header thunar-vfs-marshal.list \
	     | sed -e 's/marshal_data);$$/marshal_data) G_GNUC_INTERNAL;/' ) >> xgen-tvmh \
	&& ( cmp -s xgen-tvmh thunar-vfs-marshal.h || cp xgen-tvmh thunar-vfs-marshal.h ) \
	&& rm -f xgen-tvmh \
	&& echo timestamp > $(@F)

thunar-vfs-marshal.c: thunar-vfs-marshal.list Makefile
	( cd $(srcdir) && glib-genmarshal \
		--prefix=_thunar_vfs_marshal \
		--body thunar-vfs-marshal.list ) >> xgen-tvmc \
	&& cp xgen-tvmc thunar-vfs-marshal.c \
	&& rm -f xgen-tvmc
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
