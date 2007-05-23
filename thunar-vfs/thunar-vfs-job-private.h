/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_JOB_PRIVATE_H__
#define __THUNAR_VFS_JOB_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-config.h>
#include <thunar-vfs/thunar-vfs-job.h>

G_BEGIN_DECLS;

/* generic routines for synchronous signal emission */
void                  _thunar_vfs_job_emit_valist     (ThunarVfsJob        *job,
                                                       guint                signal_id,
                                                       GQuark               signal_detail,
                                                       va_list              var_args) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_emit            (ThunarVfsJob        *job,
                                                       guint                signal_id,
                                                       GQuark               signal_detail,
                                                       ...) G_GNUC_INTERNAL;

/* generic routines for asynchronous signal emission */
void                  _thunar_vfs_job_notify_valist   (ThunarVfsJob        *job,
                                                       guint                signal_id,
                                                       GQuark               signal_detail,
                                                       va_list              var_args) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_notify          (ThunarVfsJob        *job,
                                                       guint                signal_id,
                                                       GQuark               signal_detail,
                                                       ...) G_GNUC_INTERNAL;

/* special routines for signal emission */
ThunarVfsJobResponse  _thunar_vfs_job_ask_valist      (ThunarVfsJob        *job,
                                                       const gchar         *format,
                                                       va_list              var_args,
                                                       const gchar         *question,
                                                       ThunarVfsJobResponse choices) G_GNUC_INTERNAL;
ThunarVfsJobResponse  _thunar_vfs_job_ask_overwrite   (ThunarVfsJob        *job,
                                                       const gchar         *format,
                                                       ...) G_GNUC_INTERNAL G_GNUC_PRINTF (2, 3);
ThunarVfsJobResponse  _thunar_vfs_job_ask_replace     (ThunarVfsJob        *job,
                                                       ThunarVfsPath       *src_path,
                                                       ThunarVfsPath       *dst_path) G_GNUC_INTERNAL;
ThunarVfsJobResponse  _thunar_vfs_job_ask_skip        (ThunarVfsJob        *job,
                                                       const gchar         *format,
                                                       ...) G_GNUC_INTERNAL G_GNUC_PRINTF (2, 3);
void                  _thunar_vfs_job_error           (ThunarVfsJob        *job,
                                                       GError              *error) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_info_message    (ThunarVfsJob        *job,
                                                       const gchar         *format) G_GNUC_INTERNAL;
gboolean              _thunar_vfs_job_infos_ready     (ThunarVfsJob        *job,
                                                       GList               *info_list) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
void                  _thunar_vfs_job_new_files       (ThunarVfsJob        *job,
                                                       const GList         *path_list) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_percent         (ThunarVfsJob        *job,
                                                       gdouble              percent) G_GNUC_INTERNAL;

/* special routines for path based jobs */
void                  _thunar_vfs_job_total_paths     (ThunarVfsJob        *job,
                                                       GList               *total_paths) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_process_path    (ThunarVfsJob        *job,
                                                       GList               *path_list_item) G_GNUC_INTERNAL;

/* initialization and shutdown routines */
void                  _thunar_vfs_job_init            (void) G_GNUC_INTERNAL;
void                  _thunar_vfs_job_shutdown        (void) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_JOB_PRIVATE_H__ */
