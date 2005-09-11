# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"OpenTerminalHere\"

extensionsdir = $(libdir)/thunarx-$(THUNAR_VERSION_API)
extensions_LTLIBRARIES =						\
	open-terminal-here.la

open_terminal_here_la_SOURCES =						\
	open-terminal-here.c

open_terminal_here_la_CFLAGS =						\
	$(GTK_CFLAGS)

open_terminal_here_la_DEPENDENCIES =					\
	$(top_builddir)/thunarx/libthunarx-$(THUNAR_VERSION_API).la

open_terminal_here_la_LIBADD =						\
	$(top_builddir)/thunarx/libthunarx-$(THUNAR_VERSION_API).la	\
	$(GTK_LIBS)

open_terminal_here_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-module

EXTRA_DIST =								\
	README

# Don't install/uninstall examples by default
install:
uninstall:

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
