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
	thunar-desktop-view.c						\
	thunar-desktop-view.h						\
	thunar-file.c							\
	thunar-file.h							\
	thunar-folder.c							\
	thunar-folder.h							\
	thunar-list-model.c						\
	thunar-list-model.h						\
	thunar-preferences.c						\
	thunar-preferences.h						\
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

EXTRA_DIST =								\
	fallback-icon.h							\
	fallback-icon.png

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
