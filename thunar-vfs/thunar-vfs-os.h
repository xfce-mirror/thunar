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

#if !defined(THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __THUNAR_VFS_OS_H__
#define __THUNAR_VFS_OS_H__

#include <thunar-vfs/thunar-vfs-path-private.h>

G_BEGIN_DECLS;

gboolean  _thunar_vfs_os_is_dir_empty (const gchar   *absolute_path) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

GList    *_thunar_vfs_os_scandir      (ThunarVfsPath *path,
                                       const gchar   *absolute_path,
                                       gboolean       follow_links,
                                       GList        **directories_return,
                                       GError       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_OS_H__ */
