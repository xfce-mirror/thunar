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

#ifndef __THUNAR_VFS_IO_OPS_H__
#define __THUNAR_VFS_IO_OPS_H__

#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-types.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsIOOpsFlags:
 * @THUNAR_VFS_IO_OPS_NONE          : no special behaviour.
 * @THUNAR_VFS_IO_OPS_IGNORE_EEXIST : ignore EEXIST errors (for _thunar_vfs_io_ops_mkdir()).
 * @THUNAR_VFS_IO_OPS_IGNORE_ENOENT : ignore ENOENT errors (for _thunar_vfs_io_ops_remove()).
 *
 * Flags for I/O operations.
 **/
typedef enum /*< flags, skip >*/
{
  THUNAR_VFS_IO_OPS_NONE          = 0L,
  THUNAR_VFS_IO_OPS_IGNORE_EEXIST = (1L << 0),
  THUNAR_VFS_IO_OPS_IGNORE_ENOENT = (1L << 1),
} ThunarVfsIOOpsFlags;

/**
 * ThunarVfsIOOpsProgressCallback:
 * @chunk_size    : the size of the next completed chunk of copied data.
 * @callback_data : callback data specified for _thunar_vfs_io_ops_copy_file().
 *
 * Invoked during _thunar_vfs_io_ops_copy_file() to notify the caller
 * about the progress of the operation.
 *
 * Return value: %FALSE to cancel the operation, %TRUE to continue.
 **/
typedef gboolean (*ThunarVfsIOOpsProgressCallback) (ThunarVfsFileSize chunk_size,
                                                    gpointer          callback_data);

gboolean _thunar_vfs_io_ops_get_file_size_and_type  (ThunarVfsPath                 *path,
                                                     ThunarVfsFileSize             *size_return,
                                                     ThunarVfsFileType             *type_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gboolean _thunar_vfs_io_ops_copy_file               (ThunarVfsPath                 *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     ThunarVfsIOOpsProgressCallback callback,
                                                     gpointer                       callback_data,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean _thunar_vfs_io_ops_link_file               (ThunarVfsPath                 *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean _thunar_vfs_io_ops_move_file               (ThunarVfsPath                 *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gboolean _thunar_vfs_io_ops_mkdir                   (ThunarVfsPath                 *path,
                                                     ThunarVfsFileMode              mode,
                                                     ThunarVfsIOOpsFlags            flags,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gboolean _thunar_vfs_io_ops_remove                  (ThunarVfsPath                 *path,
                                                     ThunarVfsIOOpsFlags            flags,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_IO_OPS_H__ */
