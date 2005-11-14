# $Id$

AUTOMAKE_OPTIONS = 1.8

# The name of the module.
DOC_MODULE=thunar-vfs

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="EXO_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../thunar-vfs

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/thunar-vfs/*.h
CFILE_GLOB=$(top_srcdir)/thunar-vfs/*.c

# Header files to ignore when scanning
IGNORE_HFILES=								\
	thunar-vfs-alias.h						\
	thunar-vfs-marshal.h						\
	thunar-vfs-volume-manager-bsd.h					\
	thunar-vfs-volume-manager-impl.h				\
	thunar-vfs-volume-manager-sysv.h

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES=

# Images to copy into HTML directory
HTML_IMAGES =

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files = 							\
	version.xml

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES =	 							\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	$(EXO_CFLAGS)							\
	$(GTHREAD_CFLAGS)

GTKDOC_LIBS =  								\
	$(EXO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(top_builddir)/thunar-vfs/libthunar-vfs-$(THUNAR_VERSION_API).la

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=								\
	version.xml.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
