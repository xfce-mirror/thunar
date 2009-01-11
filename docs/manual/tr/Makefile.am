SUBDIRS =								\
	images

TARGET_DIR = $(datadir)/doc/Thunar/html/tr
STYLESHEET = ../thunar.xsl
DOCUMENT = Thunar.xml

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
GPATH = $(srcdir)

DOC_STAMPS = html-build.stamp

EXTRA_DIST = $(DOCUMENT)
CLEANFILES = $(DOC_STAMPS)

if ENABLE_XSLTPROC
all-local: html-build.stamp

html-build.stamp: $(srcdir)/$(DOCUMENT) $(srcdir)/$(STYLESHEET)
	@echo "*** Building HTML ***"
	@-chmod -R u+w $(srcdir)
	rm -rf $(srcdir)/html
	mkdir $(srcdir)/html
	$(XSLTPROC) --nonet -o $(srcdir)/html/ $(srcdir)/$(STYLESHEET) \
		$(srcdir)/$(DOCUMENT)
	touch html-build.stamp
else
all-local:
endif

maintainer-clean-local: clean
	(cd $(srcdir) && rm -rf html)

install-data-local:
	installfiles=`echo $(srcdir)/html/*`;				\
	if test "$$installfiles" = '$(srcdir)/html/*'; then		\
		echo "--- Nothing to install";				\
	else								\
		$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR);		\
		for file in $$installfiles; do				\
			echo "--- Installing "$$file;			\
			$(INSTALL_DATA) $$file $(DESTDIR)$(TARGET_DIR);	\
		done;							\
	fi

uninstall-local:
	rm -rf $(DESTDIR)$(TARGET_DIR)/*

if ENABLE_XSLTPROC
dist-check-xsltproc: all
else
dist-check-xsltproc:
	@echo "*** xsltproc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-xsltproc dist-hook-local
	mkdir $(distdir)/html
	-cp $(srcdir)/html/* $(distdir)/html

.PHONY: dist-hook-local
