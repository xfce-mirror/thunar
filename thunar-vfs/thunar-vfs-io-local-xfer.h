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

#ifndef __THUNAR_VFS_IO_LOCAL_XFER_H__
#define __THUNAR_VFS_IO_LOCAL_XFER_H__

#include <thunar-vfs/thunar-vfs-io-ops.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsIOLocalXferMode:
 * @THUNAR_VFS_IO_LOCAL_XFER_COPY : generate path for copy.
 * @THUNAR_VFS_IO_LOCAL_XFER_LINK : generate path for link.
 *
 * Modi for _thunar_vfs_io_local_xfer_next_path().
 **/
typedef enum /*< skip >*/
{
  THUNAR_VFS_IO_LOCAL_XFER_COPY,
  THUNAR_VFS_IO_LOCAL_XFER_LINK,
} ThunarVfsIOLocalXferMode;

ThunarVfsPath *_thunar_vfs_io_local_xfer_next_path (const ThunarVfsPath           *source_path,
                                                    ThunarVfsPath                 *target_directory_path,
                                                    guint                          n,
                                                    ThunarVfsIOLocalXferMode       mode,
                                                    GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean       _thunar_vfs_io_local_xfer_copy      (const ThunarVfsPath           *source_path,
                                                    ThunarVfsPath                 *target_path,
                                                    ThunarVfsIOOpsProgressCallback callback,
                                                    gpointer                       callback_data,
                                                    gboolean                       merge_directories,
                                                    GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gboolean       _thunar_vfs_io_local_xfer_link      (const ThunarVfsPath           *source_path,
                                                    ThunarVfsPath                 *target_path,
                                                    GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;


G_END_DECLS;

#endif /* !__THUNAR_VFS_IO_LOCAL_XFER_H__ */
