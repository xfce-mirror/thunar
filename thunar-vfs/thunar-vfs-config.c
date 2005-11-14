/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar-vfs/thunar-vfs-config.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * thunar_vfs_major_version:
 *
 * The major version number of the
 * <systemitem class="library">thunar-vfs</systemitem> library (e.g. in
 * version 0.5.1 this is 0).
 *
 * This variable is in the library, so represents the
 * <systemitem class="library">thunar-vfs</systemitem> library you have
 * linked against. Contrast with the #THUNAR_VFS_MAJOR_VERSION macro, which
 * represents the major version of the
 * <systemitem class="library">thunar-vfs</systemitem> headers you have
 * included.
 **/
const guint thunar_vfs_major_version = THUNAR_VFS_MAJOR_VERSION;



/**
 * thunar_vfs_minor_version:
 *
 * The minor version number of the
 * <systemitem class="library">thunar-vfs</systemitem> library (e.g. in
 * version 0.5.1 this is 5).
 *
 * This variable is in the library, so represents the
 * <systemitem class="library">thunar-vfs</systemitem> library you have
 * linked against. Contrast with the #THUNAR_VFS_MINOR_VERSION macro, which
 * represents the minor version of the
 * <systemitem class="library">thunar-vfs</systemitem> headers you have
 * included.
 **/
const guint thunar_vfs_minor_version = THUNAR_VFS_MINOR_VERSION;



/**
 * thunar_vfs_micro_version:
 *
 * The micro version number of the
 * <systemitem class="library">thunar-vfs</systemitem> library (e.g. in
 * version 0.5.1 this is 1).
 *
 * This variable is in the library, so represents the
 * <systemitem class="library">thunar-vfs</systemitem> library you have
 * linked against. Contrast with the #THUNAR_VFS_MICRO_VERSION macro, which
 * represents the micro version of the
 * <systemitem class="library">thunar-vfs</systemitem> headers you have
 * included.
 **/
const guint thunar_vfs_micro_version = THUNAR_VFS_MICRO_VERSION;



/**
 * thunar_vfs_check_version:
 * @required_major : the required major version.
 * @required_minor : the required minor version.
 * @required_micro : the required micro version.
 *
 * Checks that the <systemitem class="library">thunar-vfs</systemitem> library
 * in use is compatible with the given version. Generally you would pass in
 * the constants #THUNAR_VFS_MAJOR_VERSION, #THUNAR_VFS_MINOR_VERSION and
 * #THUNAR_VFS_VERSION_MICRO as the three arguments to this function; that produces
 * a check that the library in use is compatible with the version of
 * <systemitem class="library">thunar-vfs</systemitem> the extension was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the Thunar VFS library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = thunar_vfs_check_version (THUNAR_VFS_VERSION_MAJOR,
 *                                      THUNAR_VFS_VERSION_MINOR,
 *                                      THUNAR_VFS_VERSION_MICRO);
 * if (G_UNLIKELY (mismatch != NULL))
 *   g_error ("Version mismatch: %<!---->s", mismatch);
 * </programlisting>
 * </example>
 *
 * Return value: %NULL if the library is compatible with the given version,
 *               or a string describing the version mismatch. The returned
 *               string is owned by the library and must not be freed or
 *               modified by the caller.
 **/
const gchar*
thunar_vfs_check_version (guint required_major,
                          guint required_minor,
                          guint required_micro)
{
  return NULL;
}



#define __THUNAR_VFS_CONFIG_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
