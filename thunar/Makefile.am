# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DEXO_DISABLE_DEPRECATED					\
	-DG_LOG_DOMAIN=\"Thunar\"

bin_PROGRAMS =								\
	Thunar

Thunar_SOURCES =							\
	main.c

Thunar_CFLAGS =								\
	$(EXO_CFLAGS)

Thunar_LDFLAGS =							\
	-no-undefined							\
	$(EXO_LIBS)

# install symlink to 'thunar'
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	( cd $(DESTDIR)$(bindir) ; ln -sf Thunar thunar )

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
