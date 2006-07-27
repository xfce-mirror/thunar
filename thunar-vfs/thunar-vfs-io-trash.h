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

#if !defined(THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __THUNAR_VFS_IO_TRASH_H__
#define __THUNAR_VFS_IO_TRASH_H__

#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-io-ops.h>

G_BEGIN_DECLS;

void           _thunar_vfs_io_trash_scan            (void) G_GNUC_INTERNAL;
gboolean       _thunar_vfs_io_trash_rescan          (void) G_GNUC_INTERNAL;

gchar         *_thunar_vfs_io_trash_get_top_dir     (guint                          trash_id,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_io_trash_get_trash_dir   (guint                          trash_id,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_trash_get_trash_info  (const ThunarVfsPath           *path,
                                                     gchar                        **original_path_return,
                                                     gchar                        **deletion_date_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gboolean       _thunar_vfs_io_trash_new_trash_info  (const ThunarVfsPath           *original_path,
                                                     guint                         *trash_id_return,
                                                     gchar                        **file_id_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

ThunarVfsPath *_thunar_vfs_io_trash_path_new        (guint                          trash_id,
                                                     const gchar                   *file_id,
                                                     const gchar                   *relative_path) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_trash_path_parse      (const ThunarVfsPath           *path,
                                                     guint                         *trash_id_return,
                                                     gchar                        **file_id_return,
                                                     gchar                        **relative_path_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_io_trash_path_resolve    (const ThunarVfsPath           *path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

ThunarVfsInfo *_thunar_vfs_io_trash_get_info        (ThunarVfsPath                 *path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_io_trash_get_metadata    (ThunarVfsPath                 *path,
                                                     ThunarVfsInfoMetadata          metadata,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GList         *_thunar_vfs_io_trash_listdir         (ThunarVfsPath                 *path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_trash_copy_file       (ThunarVfsPath                 *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     ThunarVfsIOOpsProgressCallback callback,
                                                     gpointer                       callback_data,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_trash_move_file       (ThunarVfsPath                 *source_path,
                                                     ThunarVfsPath                 *target_path,
                                                     ThunarVfsPath                **target_path_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gboolean       _thunar_vfs_io_trash_remove          (ThunarVfsPath                 *path,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
GList         *_thunar_vfs_io_trash_scandir         (ThunarVfsPath                 *path,
                                                     gboolean                       follow_links,
                                                     GList                        **directories_return,
                                                     GError                       **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void           _thunar_vfs_io_trash_init            (void) G_GNUC_INTERNAL;
void           _thunar_vfs_io_trash_shutdown        (void) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_IO_TRASH_H__ */
