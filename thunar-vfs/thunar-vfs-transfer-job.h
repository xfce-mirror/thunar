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

#ifndef __THUNAR_VFS_TRANSFER_JOB_H__
#define __THUNAR_VFS_TRANSFER_JOB_H__

#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-path.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsTransferJobClass ThunarVfsTransferJobClass;
typedef struct _ThunarVfsTransferJob      ThunarVfsTransferJob;

#define THUNAR_VFS_TYPE_TRANSFER_JOB            (thunar_vfs_transfer_job_get_type ())
#define THUNAR_VFS_TRANSFER_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_TRANSFER_JOB, ThunarVfsTransferJob))
#define THUNAR_VFS_TRANSFER_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_TRANSFER_JOB, ThunarVfsTransferJobClass))
#define THUNAR_VFS_IS_TRANSFER_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_TRANSFER_JOB))
#define THUNAR_VFS_IS_TRANSFER_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_TRANSFER_JOB))
#define THUNAR_VFS_TRANSFER_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_TRANSFER_JOB, ThunarVfsTransferJobClass))

GType         thunar_vfs_transfer_job_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

ThunarVfsJob *thunar_vfs_transfer_job_new      (GList   *source_path_list,
                                                GList   *target_path_list,
                                                gboolean move,
                                                GError **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_TRANSFER_JOB_H__ */
