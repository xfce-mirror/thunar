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

libexec_SCRIPTS =							\
	ThunarHelp

ThunarHelp: ThunarHelp.in Makefile
	rm -f ThunarHelp.gen ThunarHelp
	sed -e "s,\@datadir\@,$(datadir),g" 				\
		< $(srcdir)/ThunarHelp.in				\
		> ThunarHelp.gen
	mv ThunarHelp.gen ThunarHelp

desktopdir = $(datadir)/applications
desktop_in_files = Thunar.desktop.in Thunar-folder-handler.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if HAVE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.FileManager.service.in org.xfce.Thunar.service.in
service_DATA = $(service_in_files:.service.in=.service)
%.service: %.service.in
	sed -e "s,\@bindir\@,$(bindir),g" < $< > $@
endif

EXTRA_DIST =								\
	FAQ								\
	HACKING								\
	ThunarHelp.in							\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_files)						\
	$(service_in_files)

DISTCLEANFILES =							\
	ThunarHelp							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)							\
	$(service_DATA)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-xsltproc

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
