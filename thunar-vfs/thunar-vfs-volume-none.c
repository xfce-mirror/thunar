/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-vfs/thunar-vfs-volume-none.h>
#include <thunar-vfs/thunar-vfs-volume-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



struct _ThunarVfsVolumeManagerNoneClass
{
  ThunarVfsVolumeManagerClass __parent__;
};

struct _ThunarVfsVolumeManagerNone
{
  ThunarVfsVolumeClass __parent__;
};



GType
thunar_vfs_volume_manager_none_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeManagerNoneClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof (ThunarVfsVolumeManagerNone),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_VOLUME_MANAGER, I_("ThunarVfsVolumeManagerNone"), &info, 0);
    }

  return type;
}



#define __THUNAR_VFS_VOLUME_NONE_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
