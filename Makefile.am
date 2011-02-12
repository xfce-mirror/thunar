# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
# -
# Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
# 
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of 
# the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public 
# License along with this program; if not, write to the Free 
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

SUBDIRS =								\
	icons								\
	pixmaps								\
	po								\
	tdb								\
	thunarx								\
	thunar								\
	docs								\
	examples							\
	plugins

distclean-local:
	rm -rf *.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

thunar_scriptsdir = $(HELPER_PATH_PREFIX)/Thunar
thunar_scripts_SCRIPTS =							\
	ThunarBulkRename						\
	ThunarHelp

ThunarBulkRename: ThunarBulkRename.in Makefile
	$(AM_V_GEN) (							\
		rm -f ThunarBulkRename.gen ThunarBulkRename		\
		&& sed -e "s,\@bindir\@,$(bindir),g" 			\
			< $(srcdir)/ThunarBulkRename.in			\
			> ThunarBulkRename.gen				\
		&& mv ThunarBulkRename.gen ThunarBulkRename 		\
	)

ThunarHelp: ThunarHelp.in Makefile
	$(AM_V_GEN) (							\
		rm -f ThunarHelp.gen ThunarHelp				\
		&& sed -e "s,\@docdir\@,$(docdir),g" 			\
			< $(srcdir)/ThunarHelp.in			\
			> ThunarHelp.gen				\
		&& mv ThunarHelp.gen ThunarHelp				\
	)

desktopdir = $(datadir)/applications
desktop_in_in_files = 							\
	Thunar.desktop.in.in						\
	Thunar-bulk-rename.desktop.in.in				\
	Thunar-folder-handler.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e "s,\@HELPERDIR\@,$(HELPER_PATH_PREFIX),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if HAVE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.FileManager.service.in org.xfce.Thunar.service.in
service_DATA = $(service_in_files:.service.in=.service)
%.service: %.service.in
	$(AM_V_GEN) sed -e "s,\@bindir\@,$(bindir),g" < $< > $@
endif

EXTRA_DIST =								\
	FAQ								\
	HACKING								\
	ThunarBulkRename.in						\
	Thunar.spec.in							\
	ThunarHelp.in							\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_in_files)						\
	$(service_in_files)

DISTCLEANFILES =							\
	ThunarBulkRename						\
	Thunar.spec							\
	ThunarHelp							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_in_files)						\
	$(desktop_DATA)							\
	$(service_DATA)

DISTCHECK_CONFIGURE_FLAGS =						\
	--enable-gtk-doc						\
	--enable-gen-doc
