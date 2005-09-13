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
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  ThunarVfsVolumeManager *manager;
  ThunarVfsVolume        *volume;
  GList                  *volumes;

  g_type_init ();

  manager = thunar_vfs_volume_manager_get_default ();
  g_assert (THUNAR_VFS_IS_VOLUME_MANAGER (manager));

  volumes = thunar_vfs_volume_manager_get_volumes (manager);
  g_assert (g_list_length (volumes) == 3);

  volume = g_list_nth_data (volumes, 0);
  g_assert (THUNAR_VFS_IS_VOLUME (volume));
  g_assert (!thunar_vfs_volume_is_mounted (volume));
  g_assert (thunar_vfs_volume_get_mount_point (volume) != NULL);

  volume = g_list_nth_data (volumes, 1);
  g_assert (THUNAR_VFS_IS_VOLUME (volume));
  g_assert (!thunar_vfs_volume_is_mounted (volume));
  g_assert (thunar_vfs_volume_get_mount_point (volume) != NULL);

  volume = g_list_nth_data (volumes, 2);
  g_assert (THUNAR_VFS_IS_VOLUME (volume));
  g_assert (!thunar_vfs_volume_is_mounted (volume));
  g_assert (thunar_vfs_volume_get_mount_point (volume) != NULL);

  g_object_unref (G_OBJECT (manager));

  return EXIT_SUCCESS;
#else
  /* only BSD systems will pass this test */
  return 77;
#endif
}



