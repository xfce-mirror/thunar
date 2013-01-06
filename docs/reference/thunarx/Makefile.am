# $Id$

AUTOMAKE_OPTIONS = 1.8

# The name of the module.
DOC_MODULE=thunarx

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="EXO_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0);g_type_class_ref(G_TYPE_OBJECT)"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../thunarx

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/thunarx/*.h
CFILE_GLOB=$(top_srcdir)/thunarx/*.c

# Header files to ignore when scanning
IGNORE_HFILES=thunarx-alias.h

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES=

# Images to copy into HTML directory
HTML_IMAGES =								\
	$(srcdir)/images/abstraction.png				\
	$(srcdir)/images/bulk-rename.png				\
	$(srcdir)/images/menu-provider.png				\
	$(srcdir)/images/say-hello.png

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files = 							\
	version.xml

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS =	 							\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	$(GTK_CFLAGS)							\
	$(GTK_DOC_EXTRA_CFLAGS)

GTKDOC_LIBS =  								\
	$(top_builddir)/thunarx/libthunarx-$(THUNARX_VERSION_API).la

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=								\
	version.xml.in

# required for gtk-doc
dist-hook: all

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
