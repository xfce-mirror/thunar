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

#if !defined (THUNAR_VFS_INSIDE_THUNAR_VFS_H) && !defined (THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __THUNAR_VFS_MIME_ACTION_PRIVATE_H__
#define __THUNAR_VFS_MIME_ACTION_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-mime-action.h>
#include <thunar-vfs/thunar-vfs-mime-handler-private.h>

G_BEGIN_DECLS;

struct _ThunarVfsMimeActionClass
{
  ThunarVfsMimeHandlerClass __parent__;
};

struct _ThunarVfsMimeAction
{
  ThunarVfsMimeHandler __parent__;
};

ThunarVfsMimeAction *_thunar_vfs_mime_action_new (const gchar              *command,
                                                  const gchar              *name,
                                                  const gchar              *icon,
                                                  ThunarVfsMimeHandlerFlags flags) G_GNUC_INTERNAL G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_ACTION_PRIVATE_H__ */
