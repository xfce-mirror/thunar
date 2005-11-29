# $Id$

if ENABLE_UCA_PLUGIN
SUBDIRS_THUNAR_UCA = thunar-uca
endif

SUBDIRS =								\
	icons								\
	pixmaps								\
	po								\
	tdb								\
	thunarx								\
	thunar-vfs							\
	thunar								\
	tests								\
	docs								\
	examples							\
	$(SUBDIRS_THUNAR_UCA)

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

distclean-local:
	rm -rf *.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

desktopdir = $(datadir)/applications
desktop_in_files = Thunar.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	FAQ								\
	HACKING								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
