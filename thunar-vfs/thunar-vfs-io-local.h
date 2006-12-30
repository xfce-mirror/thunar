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

#ifndef __THUNAR_VFS_IO_LOCAL_H__
#define __THUNAR_VFS_IO_LOCAL_H__

#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-io-ops.h>

G_BEGIN_DECLS;

gboolean       _thunar_vfs_io_local_get_free_space  (const ThunarVfsPath           *path,
                                                     ThunarVfsFileSize             *free_space_return) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
ThunarVfsInfo *_thunar_vfs_io_local_get_info        (ThunarVfsPath                 *path,
                                                     const gchar                   *filename,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_io_local_get_metadata    (ThunarVfsPath                 *path,
                                                     ThunarVfsInfoMetadata          metadata,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GList         *_thunar_vfs_io_local_listdir         (ThunarVfsPath                 *path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_local_copy_file       (const ThunarVfsPath           *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     ThunarVfsIOOpsProgressCallback callback,
                                                     gpointer                       callback_data,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_local_link_file       (const ThunarVfsPath           *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_local_move_file       (const ThunarVfsPath           *source_path,
                                                     const ThunarVfsPath           *target_path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_local_rename          (ThunarVfsInfo                 *info,
                                                     const gchar                   *name,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_IO_LOCAL_H__ */
