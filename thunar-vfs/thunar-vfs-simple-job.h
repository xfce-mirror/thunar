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

#ifndef __THUNAR_VFS_SIMPLE_JOB_H__
#define __THUNAR_VFS_SIMPLE_JOB_H__

#include <thunar-vfs/thunar-vfs-job-private.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsSimpleJobFunc:
 * @job            : a #ThunarVfsJob.
 * @param_values   : the #GValue<!---->s passed to thunar_vfs_simple_job_launch().
 * @n_param_values : the number of #GValue<!---->s passed in @param_values.
 * @error          : return location for errors.
 *
 * Used by the #ThunarVfsSimpleJob to process the @job. See thunar_vfs_simple_job_launch()
 * for further details.
 *
 * Return value: %TRUE on success, %FALSE in case of an error.
 **/
typedef gboolean (*ThunarVfsSimpleJobFunc) (ThunarVfsJob *job,
                                            const GValue *param_values,
                                            guint         n_param_values,
                                            GError      **error);

typedef struct _ThunarVfsSimpleJobClass ThunarVfsSimpleJobClass;
typedef struct _ThunarVfsSimpleJob      ThunarVfsSimpleJob;

#define THUNAR_VFS_TYPE_SIMPLE_JOB            (thunar_vfs_simple_job_get_type ())
#define THUNAR_VFS_SIMPLE_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_SIMPLE_JOB, ThunarVfsSimpleJob))
#define THUNAR_VFS_SIMPLE_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_SIMPLE_JOB, ThunarVfsSimpleJobClass))
#define THUNAR_VFS_IS_SIMPLE_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_SIMPLE_JOB))
#define THUNAR_VFS_IS_SIMPLE_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_SIMPLE_JOB))
#define THUNAR_VFS_SIMPLE_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_SIMPLE_JOB, ThunarVfsSimpleJobClass))

GType         thunar_vfs_simple_job_get_type  (void) G_GNUC_CONST G_GNUC_INTERNAL;

ThunarVfsJob *thunar_vfs_simple_job_launch    (ThunarVfsSimpleJobFunc func,
                                               guint                  n_param_values,
                                               ...) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_SIMPLE_JOB_H__ */
