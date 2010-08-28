SUBDIRS = \
	images \
	po

STYLESHEET = $(PACKAGE)-html.xsl
DOCUMENT = $(PACKAGE).xml
LINGUAS = $(shell cd $(srcdir)/po && echo `/bin/ls *.po | sed 's,[.]po,,g'`)
ALL_LINGUAS = C $(LINGUAS)

# install the html files to the destdir
install-data-local:
	for lang in $(ALL_LINGUAS); do \
		if test -d $(srcdir)/html/$$lang; then \
			dest=$(DESTDIR)$(docdir)/html/$$lang; \
			$(mkinstalldirs) $$dest; \
			installfiles=`echo $(srcdir)/html/$$lang/*`; \
			for file in $$installfiles; do \
				echo "--- Installing "$$file; \
				$(INSTALL_DATA) $$file $$dest; \
			done; \
			if test x"$$lang" != x"C" -a ! -d $(srcdir)/images/$$lang; then \
				echo "--- Creating symlink to C images for $$lang"; \
				ln -sf $(docdir)/html/C/images $(DESTDIR)$(docdir)/html/$$lang/images; \
			fi \
		fi \
	done;

# remove installed files from the destdir
uninstall-local:
	for lang in $(ALL_LINGUAS); do \
		dest=$(DESTDIR)$(docdir)/html/$$lang; \
		if test -d $$dest; then \
			if test -L $$dest/images; then \
				rm -rf $$dest; \
			else \
				rm -f $$dest/*.html; \
			fi \
		fi \
	done

# remove generate html from the srcdir
clean-local:
	rm -rf $(srcdir)/html

if GENERATE_DOCUMENTATION

# regenerate po(t) files
update-po:
	rm -f $(srcdir)/po/$(GETTEXT_PACKAGE).pot; \
	$(XML2PO) -o $(srcdir)/po/$(GETTEXT_PACKAGE).pot $(srcdir)/$(DOCUMENT); \
	for lang in $(LINGUAS); do \
		pofile=$(srcdir)/po/$$lang.po; \
		$(XML2PO) -u $(srcdir)/po/$$lang.po $(srcdir)/$(DOCUMENT); \
	done

# build the translated help files
doc-build.stamp: $(srcdir)/$(DOCUMENT) $(srcdir)/$(STYLESHEET)
	@chmod -R u+w $(srcdir); \
	rm -rf $(srcdir)/html; \
	echo; \
	for lang in $(ALL_LINGUAS); do \
		pofile=$(srcdir)/po/$$lang.po; \
		echo "*** Generating $$lang documentation"; \
		if test x"$$lang" = x"C"; then \
			cp $(srcdir)/$(DOCUMENT) xgen-doc; \
		else \
			echo "Building the translated DocBook XML file"; \
			$(XML2PO) -e -l $$lang -p $$pofile $(srcdir)/$(DOCUMENT) > xgen-doc; \
			rm -f .xml2po.mo; \
		fi; \
		sed -e "s,\@PACKAGE_NAME\@,$(PACKAGE_NAME),g" \
			-e "s,\@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g" \
			-i xgen-doc; \
		echo "Validating DocBook XML file"; \
		$(XMLLINT) --noout --valid xgen-doc || exit 1; \
		echo "Creating the HTML pages"; \
		$(XSLTPROC) --nonet -o $(srcdir)/html/$$lang/ $(srcdir)/$(STYLESHEET) xgen-doc; \
		chmod -R u+w $(srcdir)/html/$$lang; \
		rm -f xgen-doc; \
		echo; \
	done; \
	touch doc-build.stamp

dist-check-doc: all

else

update-po:
	@echo "*** You need to compile with --enable-gen-doc in order to make update-po"
	@false

doc-build.stamp:
	@if test ! -d $(srcdir)/html; then \
		echo; \
		echo "*** No generated documentation found. If you want to"; \
		echo "*** generate the (translated) documentation, run configure"; \
		echo "*** with --enable-gen-doc."; \
		echo; \
	fi

dist-check-doc:
	@echo "*** You need to compile with --enable-gen-doc in order to make dist"
	@false

endif

# always try to build (if compiling with --enable-build-doc) or print
# a message if no html files were found in the tarball
all: doc-build.stamp

# include the html and man files in the dist
dist-hook: dist-check-doc doc-build.stamp
	for lang in $(ALL_LINGUAS); do \
		if test -d $(srcdir)/html/$$lang; then \
			mkdir -p $(distdir)/html/$$lang; \
			cp $(srcdir)/html/$$lang/* $(distdir)/html/$$lang/; \
		fi \
	done

# css file
cssdir = $(docdir)/html
css_DATA = \
	thunar.css

EXTRA_DIST = \
	$(css_DATA) \
	$(STYLESHEET) \
	$(DOCUMENT)

CLEANFILES = \
	xgen-doc \
	doc-build.stamp
