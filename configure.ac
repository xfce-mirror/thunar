# vi:set et ai sw=2 sts=2 ts=2: */
#-
# Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
# Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
# Copyright (c) 2004-2024 Xfce Development Team <xfce4-dev@xfce.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

dnl ***************************
dnl *** Version information ***
dnl ***************************
m4_define([thunarx_verinfo], [0:0:0])
m4_define([thunarx_version_api], [3])

XDT_VERSION_INIT([4.20.0])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2004-2024 The Thunar development team. All rights reserved.])
AC_INIT([thunar], [xdt_version], [https://gitlab.xfce.org/xfce/thunar], [thunar])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])
AC_CANONICAL_TARGET()
AC_REVISION([xdt_version_build])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.11 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl *******************************
dnl *** Check for UNIX variants ***
dnl *******************************
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])

if test "x$target_os" = "xlinux-gnu"; then
  AC_DEFINE([HAVE_LINUX], [], [Whether we are building on Linux or not])
fi

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
LT_PATH_LD()
AM_PROG_CC_C_O()
AC_PROG_INSTALL()
AC_CHECK_PROGS([PERL], [perl5 perl])
m4_ifdef([AC_PROG_SED], [AC_PROG_SED], [AC_CHECK_PROG(SED, sed, sed)])

dnl **************************
dnl *** Initialize libtool ***
dnl **************************
LT_PREREQ([2.4])
LT_INIT([disable-static])

dnl **************************************
dnl *** Substitute version information ***
dnl **************************************
THUNARX_VERINFO=thunarx_verinfo()
THUNARX_VERSION_API=thunarx_version_api()
THUNAR_VERSION_MAJOR=xdt_version_major()
THUNAR_VERSION_MINOR=xdt_version_minor()
THUNAR_VERSION_MICRO=xdt_version_micro()
AC_SUBST([THUNARX_VERINFO])
AC_SUBST([THUNARX_VERSION_API])
AC_SUBST([THUNAR_VERSION_MAJOR])
AC_SUBST([THUNAR_VERSION_MINOR])
AC_SUBST([THUNAR_VERSION_MICRO])

dnl ******************************
dnl *** Set helper path prefix ***
dnl ******************************
AC_ARG_WITH([helper-path-prefix],
            [AS_HELP_STRING([--with-helper-path-prefix=PATH],
                            [Path prefix under which helper executables will be installed (default: $libdir)])],
            [HELPER_PATH_PREFIX="$withval"],
            [HELPER_PATH_PREFIX="$libdir"])
AC_SUBST([HELPER_PATH_PREFIX])

dnl ***************************************
dnl *** Set default thunarx directories ***
dnl ***************************************
AC_ARG_WITH([thunarx-dirs],
            [AS_HELP_STRING([--with-thunarx-dirs=PATH],
                            [Used to set different default paths (separated by colon) for thunarx plugin lookup])],
            [DEFAULT_THUNARX_DIRS="$withval"],
            [DEFAULT_THUNARX_DIRS="$libdir/thunarx-$THUNARX_VERSION_API"])
AC_SUBST([DEFAULT_THUNARX_DIRS])

dnl *******************************************************
dnl *** Optional support for custom thunarx directories ***
dnl *******************************************************
AC_ARG_WITH([custom-thunarx-dirs-enabled],
            [AS_HELP_STRING([--with-custom-thunarx-dirs-enabled],
                            [Enables the posibillity to set custom thunarx directories by using the environment variable THUNARX_DIRS])],
            [CUSTOM_THUNARX_DIRS_ENABLED="TRUE"],
            [CUSTOM_THUNARX_DIRS_ENABLED="FALSE"])
AC_SUBST([CUSTOM_THUNARX_DIRS_ENABLED])

dnl ***********************************************
dnl *** Work-around system-specific limitations ***
dnl ***********************************************
AC_SYS_LARGEFILE()

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h grp.h limits.h locale.h memory.h \
                  paths.h pwd.h sched.h signal.h stdarg.h stdlib.h string.h \
                  sys/mman.h sys/param.h sys/stat.h sys/time.h sys/types.h \
                  sys/uio.h sys/wait.h time.h])

dnl ************************************
dnl *** Check for standard functions ***
dnl ************************************
AC_FUNC_MMAP()
AC_CHECK_FUNCS([localeconv mkdtemp pread pwrite sched_yield setgroupent \
                setpassent strcoll strlcpy strptime symlink atexit realpath \
                statx])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AC_SEARCH_LIBS([bind_textdomain_codeset], [intl],
	       [AC_DEFINE([HAVE_BIND_TEXTDOMAIN_CODESET], [1], [Define to 1 if you have the 'bind_textdomain_codeset' function.])],
	       [])

dnl *********************
dnl *** Check for X11 ***
dnl *********************
XDT_CHECK_LIBX11_REQUIRE()

dnl *************************
dnl *** Check for gtk-doc ***
dnl *************************
GTK_DOC_CHECK([1.9])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([EXO], [exo-2], [4.19.0])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.72.0])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [2.72.0])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.72.0])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [2.72.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.24.0])
XDT_CHECK_PACKAGE([GDK_PIXBUF], [gdk-pixbuf-2.0], [2.42.8])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.17.2])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.17.6])
XDT_CHECK_PACKAGE([LIBXFCE4KBD_PRIVATE], [libxfce4kbd-private-3], [4.17.2])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.12.0])
XDT_CHECK_PACKAGE([PANGO], [pango], [1.38.0])

dnl ******************************
dnl *** GObject Instrospection ***
dnl ******************************
GOBJECT_INTROSPECTION_CHECK([1.72.0])

dnl ********************************************
dnl *** Check for session management support ***
dnl ********************************************
XDT_CHECK_LIBSM()

dnl **********************************
dnl *** Optional GIO UNIX features ***
dnl **********************************
XDT_CHECK_OPTIONAL_PACKAGE([GIO_UNIX], [gio-unix-2.0],
                           [2.72.0], [gio-unix], [GIO UNIX features])

dnl ***************************************************************
dnl *** Optional support for GUDev (required for thunar-volman) ***
dnl ***************************************************************
XDT_CHECK_OPTIONAL_PACKAGE([GUDEV], [gudev-1.0], [145], [gudev],
                           [GUDev (required for thunar-volman)])

dnl **************************************
dnl *** Optional support for libnotify ***
dnl **************************************
XDT_CHECK_OPTIONAL_PACKAGE([LIBNOTIFY], [libnotify], [0.4.0], [notifications],
                           [Mount notification support], [yes])

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

dnl **************************************
dnl *** Check for linker optimizations ***
dnl **************************************
XDT_FEATURE_LINKER_OPTS()

dnl dnl ****************************************
dnl dnl *** Check for ELF visibility support ***
dnl dnl ****************************************
dnl XDT_FEATURE_VISIBILITY()

dnl *********************************
dnl *** Substitute platform flags ***
dnl *********************************
AC_MSG_CHECKING([PLATFORM_CPPFLAGS])
AC_MSG_RESULT([$PLATFORM_CPPFLAGS])
AC_SUBST([PLATFORM_CPPFLAGS])
AC_MSG_CHECKING([PLATFORM_CFLAGS])
AC_MSG_RESULT([$PLATFORM_CFLAGS])
AC_SUBST([PLATFORM_CFLAGS])
AC_MSG_CHECKING([PLATFORM_LDFLAGS])
AC_MSG_RESULT([$PLATFORM_LDFLAGS])
AC_SUBST([PLATFORM_LDFLAGS])

dnl ************************************
dnl *** Check for additional plugins ***
dnl ************************************
BM_THUNAR_PLUGIN_APR()
BM_THUNAR_PLUGIN_SBR()
BM_THUNAR_PLUGIN_TPA()
BM_THUNAR_PLUGIN_UCA()
BM_THUNAR_PLUGIN_WALLPAPER()

AC_CONFIG_FILES([
Makefile
docs/Makefile
docs/design/Makefile
docs/papers/Makefile
docs/reference/Makefile
docs/reference/thunarx/Makefile
docs/reference/thunarx/version.xml
docs/reference/thunar/Makefile
docs/reference/thunar/version.xml
examples/Makefile
examples/tex-open-terminal/Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/48x48/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
plugins/Makefile
plugins/thunar-apr/Makefile
plugins/thunar-sbr/Makefile
plugins/thunar-sendto-email/Makefile
plugins/thunar-tpa/Makefile
plugins/thunar-uca/Makefile
plugins/thunar-wallpaper/Makefile
po/Makefile.in
thunar/Makefile
thunarx/Makefile
thunarx/thunarx-3.pc
thunarx/thunarx-config.h
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
if test x"$GIO_UNIX_FOUND" = x"yes"; then
echo "* GIO UNIX features:                  yes"
else
echo "* GIO UNIX features:                  no"
fi
if test x"$GUDEV_FOUND" = x"yes"; then
echo "* GUDev (required for thunar-volman): yes"
else
echo "* GUDev (required for thunar-volman): no"
fi
if test x"$LIBNOTIFY_FOUND" = x"yes"; then
echo "* Mount notification support:         yes"
else
echo "* Mount notification support:         no"
fi
echo "* Debug Support:                      $enable_debug"
echo "* GObject Instrospection support:     $enable_introspection"
echo
echo "Additional Plugins:"
echo
echo "* Advanced Properties:                $ac_bm_thunar_plugin_apr"
echo "* Simple Builtin Renamers:            $ac_bm_thunar_plugin_sbr"
echo "* Trash Panel Applet:                 $ac_bm_thunar_plugin_tpa"
echo "* User Customizable Actions:          $ac_bm_thunar_plugin_uca"
echo "* Wallpaper support:                  $ac_bm_thunar_plugin_wallpaper"
echo
