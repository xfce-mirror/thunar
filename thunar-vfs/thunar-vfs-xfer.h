/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_XFER_H__
#define __THUNAR_VFS_XFER_H__

#include <thunar-vfs/thunar-vfs-path.h>
#include <thunar-vfs/thunar-vfs-types.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsXferCallback:
 * @total_size     : the total size of the file system entity.
 * @completed_size : the completed size of the file system entity.
 * @user_data      : the user data specified when thunar_vfs_xfer_copy()
 *                   was called.
 *
 * Return value: %FALSE to cancel the transfer operation,
 *               %TRUE to continue.
 **/
typedef gboolean (*ThunarVfsXferCallback) (ThunarVfsFileSize total_size,
                                           ThunarVfsFileSize completed_size,
                                           gpointer          user_data);

gboolean thunar_vfs_xfer_copy (const ThunarVfsPath  *source_path,
                               ThunarVfsPath        *target_path,
                               ThunarVfsPath       **target_path_return,
                               ThunarVfsXferCallback callback,
                               gpointer              user_data,
                               GError              **error) G_GNUC_INTERNAL;

gboolean thunar_vfs_xfer_link (const ThunarVfsPath  *source_path,
                               ThunarVfsPath        *target_path,
                               ThunarVfsPath       **target_path_return,
                               GError              **error) G_GNUC_INTERNAL;


#if defined(THUNAR_VFS_COMPILATION)
void _thunar_vfs_xfer_init     (void) G_GNUC_INTERNAL;
void _thunar_vfs_xfer_shutdown (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__THUNAR_VFS_XFER_H__ */
