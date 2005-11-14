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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs.h>



int
main (int argc, char **argv)
{
  ThunarVfsPath *a;
  ThunarVfsPath *b;
  gchar          buffer[THUNAR_VFS_PATH_MAXURILEN];
  gchar         *s;
  gint           n;

  /* initialize GThreads (required for Thunar-VFS initialization) */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* initialize Gtk+ (required for Thunar-VFS initialization) */
  gtk_init (&argc, &argv);

  /* initialize the thunar-vfs library */
  thunar_vfs_init ();

  /* verify the root path */
  a = thunar_vfs_path_new ("/", NULL);
  b = thunar_vfs_path_get_for_root ();
  s = thunar_vfs_path_dup_string (a);
  n = thunar_vfs_path_to_string (b, buffer, sizeof (buffer), NULL);
  g_assert (a == b);
  g_assert (thunar_vfs_path_equal (a, b));
  g_assert (thunar_vfs_path_get_parent (a) == NULL);
  g_assert (thunar_vfs_path_get_parent (b) == NULL);
  g_assert (thunar_vfs_path_hash (a) == thunar_vfs_path_hash (b));
  g_assert (strcmp (s, buffer) == 0);
  g_assert (strcmp (s, "/") == 0);
  g_assert (n == strlen (buffer) + 1);
  thunar_vfs_path_unref (b);
  thunar_vfs_path_unref (a);
  g_free (s);

  /* verify the home path */
  a = thunar_vfs_path_new (xfce_get_homedir (), NULL);
  b = thunar_vfs_path_get_for_home ();
  s = thunar_vfs_path_dup_uri (a);
  n = thunar_vfs_path_to_uri (b, buffer, sizeof (buffer), NULL);
  g_assert (a == b);
  g_assert (thunar_vfs_path_equal (a, b));
  g_assert (thunar_vfs_path_hash (a) == thunar_vfs_path_hash (b));
  g_assert (strcmp (s, buffer) == 0);
  g_assert (strncmp (s, "file:///", 8) == 0);
  g_assert (n == strlen (buffer) + 1);
  thunar_vfs_path_unref (b);
  thunar_vfs_path_unref (a);
  g_free (s);

  /* verify the URI handling */
  a = thunar_vfs_path_new ("/%test ", NULL);
  b = thunar_vfs_path_new ("file:///%25test%20//", NULL);
  s = thunar_vfs_path_dup_uri (a);
  n = thunar_vfs_path_to_string (b, buffer, sizeof (buffer), NULL);
  g_assert (thunar_vfs_path_equal (a, b));
  g_assert (thunar_vfs_path_hash (a) == thunar_vfs_path_hash (b));
  g_assert (strcmp (s, "file:///%25test%20") == 0);
  g_assert (strcmp (buffer, "/%test ") == 0);
  g_assert (n == strlen (buffer) + 1);
  thunar_vfs_path_unref (b);
  thunar_vfs_path_unref (a);
  g_free (s);

  /* verify the thunar_vfs_path_relative method */
  a = thunar_vfs_path_get_for_home ();
  b = thunar_vfs_path_get_parent (a);
  g_assert (b != NULL);
  thunar_vfs_path_ref (b);
  thunar_vfs_path_unref (a);
  s = g_path_get_basename (xfce_get_homedir ());
  a = thunar_vfs_path_relative (b, s);
  thunar_vfs_path_unref (b);
  g_free (s);
  b = thunar_vfs_path_get_for_home ();
  s = thunar_vfs_path_dup_string (a);
  g_assert (a == b);
  g_assert (thunar_vfs_path_equal (a, b));
  g_assert (thunar_vfs_path_hash (a) == thunar_vfs_path_hash (b));
  g_assert (strcmp (s, xfce_get_homedir ()) == 0);
  thunar_vfs_path_unref (b);
  thunar_vfs_path_unref (a);
  g_free (s);

  /* shutdown the Thunar-VFS library */
  thunar_vfs_shutdown ();

  return EXIT_SUCCESS;
}

