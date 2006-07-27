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

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



GType
thunar_vfs_interactive_job_response_get_type (void)
{
  return THUNAR_VFS_TYPE_VFS_JOB_RESPONSE;
}



GType
thunar_vfs_interactive_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_JOB,
                                                 "ThunarVfsInteractiveJob",
                                                 sizeof (ThunarVfsInteractiveJobClass),
                                                 NULL,
                                                 sizeof (ThunarVfsInteractiveJob),
                                                 NULL,
                                                 G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



#define __THUNAR_VFS_INTERACTIVE_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
