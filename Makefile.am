# $Id$

SUBDIRS =								\
	docs								\
	po								\
	thunar-vfs							\
	thunarx								\
	thunar								\
	tests

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

distclean-local:
	rm -rf *.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

EXTRA_DIST =								\
	AUTHORS								\
	COPYING								\
	COPYING.LIB							\
	ChangeLog							\
	INSTALL								\
	NEWS								\
	README								\
	THANKS								\
	TODO

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
