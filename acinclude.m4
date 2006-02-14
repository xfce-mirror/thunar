dnl $Id$
dnl
dnl Copyright (c) 2004-2006
dnl         The Thunar development team. All rights reserved.
dnl
dnl Written for Thunar by Benedikt Meurer <benny@xfce.org>.
dnl



dnl # BM_THUNAR_VFS_VOLUME_IMPL()
dnl #
dnl # Determines the volume manager implementation to
dnl # use for thunar-vfs.
dnl #
AC_DEFUN([BM_THUNAR_VFS_VOLUME_IMPL],
[
  dnl The --with-volume-manager option
  AC_ARG_WITH([volume-manager],
AC_HELP_STRING([--with-volume-manager=@<:@auto/freebsd/hal/none@:>@], [The volume manager implementation @<:@default=auto@:>@]),
    [], [with_volume_manager=auto])

  dnl # Check if we should try to auto-detect
  if test x"$with_volume_manager" = x"auto"; then
    dnl # Check target platform
    case "$target_os" in
    freebsd*)
      dnl # FreeBSD is fully supported
      with_volume_manager=freebsd
      ;;
    *)
      dnl # Otherwise, check if we have HAL
      XDT_CHECK_PACKAGE([HAL], [hal-storage], [0.5.0], [with_volume_manager=hal], [with_volume_manager=none])
      ;;
    esac
  fi

  dnl # We need HAL >= 0.5.x and D-BUS >= 0.23 for the HAL volume manager
  if test x"$with_volume_manager" = x"hal"; then
    XDT_CHECK_PACKAGE([HAL], [hal-storage], [0.5.0])
    XDT_CHECK_PACKAGE([HAL_DBUS], [dbus-glib-1], [0.23])
  fi

  dnl # Set config.h variables depending on what we're going to use
  AC_MSG_CHECKING([for the volume manager implemenation])
  case "$with_volume_manager" in
  freebsd)
    AC_DEFINE([THUNAR_VFS_VOLUME_IMPL_FREEBSD], [1], [Define to 1 if the FreeBSD volume manager implementation should be used])
    ;;

  hal)
    AC_DEFINE([THUNAR_VFS_VOLUME_IMPL_HAL], [1], [Define to 1 if the HAL volume manager implementation should be used])
    ;;

  *)
    AC_DEFINE([THUNAR_VFS_VOLUME_IMPL_NONE], [1], [Define to 1 if no volume manager implementation should be used])
    with_volume_manager=none
    ;;
  esac
  AC_MSG_RESULT([$with_volume_manager])

  dnl # Set automake conditionals appropriately
  AM_CONDITIONAL([THUNAR_VFS_VOLUME_IMPL_FREEBSD], [test x"$with_volume_manager" = x"freebsd"])
  AM_CONDITIONAL([THUNAR_VFS_VOLUME_IMPL_HAL], [test x"$with_volume_manager" = x"hal"])
  AM_CONDITIONAL([THUNAR_VFS_VOLUME_IMPL_NONE], [test x"$with_volume_manager" = x"none"])
])
