dnl $Id: configure.ac 67 2005-05-29 19:41:22Z bmeurer $
dnl
dnl Copyright (c) 2004-2005 os-cillation.
dnl All rights reserved.
dnl
dnl Written by Benedikt Meurer <benny@xfce.org>.
dnl

dnl Version information
m4_define([thunar_version_major], [0])
m4_define([thunar_version_minor], [0])
m4_define([thunar_version_micro], [1])
m4_define([thunar_version_tag], [svn])
m4_define([thunar_version], [thunar_version_major().thunar_version_minor().thunar_version_micro()thunar_version_tag()])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2004-2005 os-cillation.
All rights reserved.
        
Written by Benedikt Meurer <benny@xfce.org>.])
AC_INIT([Thunar], [thunar_version()], [benny@xfce.org])
AC_PREREQ([2.50])
AC_REVISION([$Id$])

dnl Initialize automake
AM_INIT_AUTOMAKE([AC_PACKAGE_TARNAME()], [AC_PACKAGE_VERSION()])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE()

dnl check for UNIX variants
AC_AIX()
AC_ISC_POSIX()
AC_MINIX()

dnl check for basic programs
AC_PROG_CC()
AC_PROG_INSTALL()
AC_PROG_LIBTOOL()

dnl Check for required packages
XDT_CHECK_PACKAGE([EXO], [exo-0.3], [0.3.1])

dnl Check for debugging support
BM_DEBUG_SUPPORT()

AC_OUTPUT([
Makefile
thunar/Makefile
])
