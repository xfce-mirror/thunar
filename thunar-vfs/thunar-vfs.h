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

#ifndef __THUNAR_VFS_H__
#define __THUNAR_VFS_H__

#define THUNAR_VFS_INSIDE_THUNAR_VFS_H

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-job.h>
#include <thunar-vfs/thunar-vfs-mime-application.h>
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-mime-info.h>
#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-path.h>
#include <thunar-vfs/thunar-vfs-thumb.h>
#include <thunar-vfs/thunar-vfs-user.h>
#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-volume.h>

#undef THUNAR_VFS_INSIDE_THUNAR_VFS_H

G_BEGIN_DECLS;

void          thunar_vfs_init             (void);
void          thunar_vfs_shutdown         (void);

ThunarVfsJob *thunar_vfs_listdir          (ThunarVfsPath *path,
                                           GError       **error) G_GNUC_MALLOC;

ThunarVfsJob *thunar_vfs_copy_file        (ThunarVfsPath *source_path,
                                           ThunarVfsPath *target_path,
                                           GError       **error) G_GNUC_MALLOC;
ThunarVfsJob *thunar_vfs_copy_files       (GList         *source_path_list,
                                           GList         *target_path_list,
                                           GError       **error) G_GNUC_MALLOC;

ThunarVfsJob *thunar_vfs_link_file        (ThunarVfsPath *source_path,
                                           ThunarVfsPath *target_path,
                                           GError       **error) G_GNUC_MALLOC;
ThunarVfsJob *thunar_vfs_link_files       (GList         *source_path_list,
                                           GList         *target_path_list,
                                           GError       **error) G_GNUC_MALLOC;

ThunarVfsJob *thunar_vfs_move_file        (ThunarVfsPath *source_path,
                                           ThunarVfsPath *target_path,
                                           GError       **error) G_GNUC_MALLOC;
ThunarVfsJob *thunar_vfs_move_files       (GList         *source_path_list,
                                           GList         *target_path_list,
                                           GError       **error) G_GNUC_MALLOC;

ThunarVfsJob *thunar_vfs_unlink_file      (ThunarVfsPath *path,
                                           GError       **error) G_GNUC_MALLOC;
ThunarVfsJob *thunar_vfs_unlink_files     (GList         *path_list,
                                           GError       **error) G_GNUC_MALLOC;

ThunarVfsJob *thunar_vfs_make_directory   (ThunarVfsPath *path,
                                           GError       **error) G_GNUC_MALLOC;
ThunarVfsJob *thunar_vfs_make_directories (GList         *path_list,
                                           GError       **error) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_VFS_H__ */
