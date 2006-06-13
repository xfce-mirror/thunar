/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_PRIVATE_H__
#define __THUNAR_VFS_PRIVATE_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

GType _thunar_vfs_g_type_register_simple (GType        type_parent,
                                          const gchar *type_name_static,
                                          guint        class_size,
                                          gpointer     class_init,
                                          guint        instance_size,
                                          gpointer     instance_init,
                                          GTypeFlags   flags) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_PRIVATE_H__ */
