/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <thunar-vfs/thunar-vfs.h>



int
main (int argc, char **argv)
{
  ThunarVfsURI *a;
  ThunarVfsURI *b;

  g_type_init ();

  /* perform simple validation on "file:///" */
  a = thunar_vfs_uri_new_for_path ("/");
  g_assert (thunar_vfs_uri_is_root (a));
  g_assert (thunar_vfs_uri_is_local (a));
  g_assert (exo_str_is_equal (thunar_vfs_uri_get_name (a), "/"));
  g_assert (exo_str_is_equal (thunar_vfs_uri_get_path (a), "/"));
  g_assert (thunar_vfs_uri_parent (a) == NULL);
  g_assert (exo_str_is_equal (thunar_vfs_uri_to_string (a, THUNAR_VFS_URI_HIDE_HOST), "file:///"));

  /* verify that URI parsing works */
  a = thunar_vfs_uri_new_for_path ("/tmp");
  b = thunar_vfs_uri_new ("file:///tmp", NULL);
  g_assert (thunar_vfs_uri_equal (a, b));
  g_assert (thunar_vfs_uri_hash (a) == thunar_vfs_uri_hash (b));

  /* verify thunar_vfs_uri_relative */
  a = thunar_vfs_uri_new_for_path ("/usr");
  b = thunar_vfs_uri_relative (a, "bin");
  g_assert (!thunar_vfs_uri_equal (a, b));
  g_assert (thunar_vfs_uri_equal (a, thunar_vfs_uri_parent (b)));
  g_assert (exo_str_is_equal (thunar_vfs_uri_to_string (a, THUNAR_VFS_URI_HIDE_HOST), "file:///usr"));
  g_assert (exo_str_is_equal (thunar_vfs_uri_to_string (b, THUNAR_VFS_URI_HIDE_HOST), "file:///usr/bin"));

  /* verify that trash:// uris work */
  a = thunar_vfs_uri_new ("trash:", NULL);
  b = thunar_vfs_uri_new ("trash://", NULL);
  g_assert (thunar_vfs_uri_equal (a, b));
  a = thunar_vfs_uri_new ("trash:///", NULL);
  g_assert (thunar_vfs_uri_equal (a, b));
  b = thunar_vfs_uri_new ("trash:///file", NULL);
  g_assert (!thunar_vfs_uri_equal (a, b));
  b = thunar_vfs_uri_parent (b);
  g_assert (thunar_vfs_uri_equal (a, b));
  g_assert (exo_str_is_equal (thunar_vfs_uri_to_string (a, 0), "trash:///"));

  return EXIT_SUCCESS;
}

