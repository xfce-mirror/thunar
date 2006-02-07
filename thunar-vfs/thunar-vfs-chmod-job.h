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

#ifndef __THUNAR_VFS_CHMOD_JOB_H__
#define __THUNAR_VFS_CHMOD_JOB_H__

#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-path.h>
#include <thunar-vfs/thunar-vfs-types.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsChmodJobClass ThunarVfsChmodJobClass;
typedef struct _ThunarVfsChmodJob      ThunarVfsChmodJob;

#define THUNAR_VFS_TYPE_CHMOD_JOB             (thunar_vfs_chmod_job_get_type ())
#define THUNAR_VFS_CHMOD_JOB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_CHMOD_JOB, ThunarVfsChmodJob))
#define THUNAR_VFS_CHMOD_JOB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_CHMOD_JOB, ThunarVfsChmodJobClass))
#define THUNAR_VFS_IS_CHMOD_JOB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_CHMOD_JOB))
#define THUNAR_VFS_IS_CHMOD_JOB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_CHMOD_JOB))
#define THUNAR_VFS_CHMOD_JOB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_CHMOD_JOB, ThunarVfsChmodJobClass))

GType         thunar_vfs_chmod_job_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

ThunarVfsJob *thunar_vfs_chmod_job_new      (ThunarVfsPath     *path,
                                             ThunarVfsFileMode  dir_mask,
                                             ThunarVfsFileMode  dir_mode,
                                             ThunarVfsFileMode  file_mask,
                                             ThunarVfsFileMode  file_mode,
                                             gboolean           recursive,
                                             GError           **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_CHMOD_JOB_H__ */
