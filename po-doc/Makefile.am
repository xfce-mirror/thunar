## Makefile.am file for the XML documents translation.
##
## Copyright (C) 2006 The Xfce development team.
##
## Written by Daichi Kawahata <daichi@xfce.org>, 2006.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This file is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

##
## Rules to auto-generate translated XML files
##
## At first, you need to install `xml2po' package maintained by
## GNOME team, too bad it's not a separated package (and depends
## libxml2 >= 2.5.11), to get a workable package, try the following
## steps:
##
##    cvs -d:pserver:anonymous@anoncvs.gnome.org:/cvs/gnome login
##    (RETURN)
##    cvs -d:pserver:anonymous@anoncvs.gnome.org:/cvs/gnome co \
##      gnome-doc-utils/xml2po
##    cd gnome-doc-utils/xml2po && ./autogen.sh && make
##
## For developers: Nothing, you need nothing to do basically,
##   however it's appreciated if you'll install the package above,
##   keep updating POT, PO files when you'll update your documents.
##
## For translators: Just translate your PO file with your favorite
##   editor, if you see no PO file for your language, then type
##   `msginit' in this directory and enter your contact address.
##
## For maintainers: You're supposed to watch every changes made
##   by the developers, translators; if POT, PO files are out-of-
##   dated or generated XML files are committed properly.
##
## A problem could be the matter is, it takes a time to update
## entire files due to its size and processing method in general
## (comparing with the gettext PO files), also with the same
## reason, it costs a certain disk spaces against the repository.
##

srcdir = @srcdir@
top_srcdir = @top_srcdir@
docdir = $(top_srcdir)/docs/manual

if ENABLE_XML2PO
all-local: $(PACKAGE).pot

$(PACKAGE).pot: $(docdir)/C/$(PACKAGE).xml.in
	$(XML2PO) -o $(PACKAGE).pot $(docdir)/C/$(PACKAGE).xml.in

update-po: Makefile $(PACKAGE).pot
	@echo "*** Updating PO ***"
	langs=`sed -e '/^#/d' $(srcdir)/LINGUAS`; \
	for lang in $$langs; do \
		$(XML2PO) -u $$lang.po $(docdir)/C/$(PACKAGE).xml.in; \
	done

update-xml: Makefile $(PACKAGE).pot
	@echo "*** Updating XML ***"
	langs=`sed -e '/^#/d' $(srcdir)/LINGUAS`; \
	for lang in $$langs; do \
		if test -n $(docdir)/$$lang; then \
			mkdir -p $(docdir)/$$lang; \
		fi; \
		$(XML2PO) -p $$lang.po $(docdir)/C/$(PACKAGE).xml.in > $(docdir)/$$lang/$(PACKAGE).xml.in; \
	done

else
all-local:
endif

if ENABLE_XML2PO
dist-check-xml2po: all
else
dist-check-xml2po:
	@echo "*** xml2po must be installed and enabled in order to make dist"
	@false
endif

DISTCLEANFILES = $(PACKAGE).pot

