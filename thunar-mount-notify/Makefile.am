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

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"thunar-mount-notify\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

libexec_PROGRAMS =							\
	thunar-mount-notify-1

thunar_mount_notify_1_SOURCES =						\
	main.c

thunar_mount_notify_1_CFLAGS =						\
	$(GTK_CFLAGS)							\
	$(LIBNOTIFY_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

thunar_mount_notify_1_LDFLAGS =						\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

thunar_mount_notify_1_LDADD =						\
	$(GTK_LIBS)							\
	$(LIBNOTIFY_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)
