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

#ifndef __THUNAR_VFS_JOB_H__
#define __THUNAR_VFS_JOB_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsJobPrivate ThunarVfsJobPrivate;
typedef struct _ThunarVfsJobClass   ThunarVfsJobClass;
typedef struct _ThunarVfsJob        ThunarVfsJob;

#define THUNAR_VFS_TYPE_JOB             (thunar_vfs_job_get_type ())
#define THUNAR_VFS_JOB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_JOB, ThunarVfsJob))
#define THUNAR_VFS_JOB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_JOB, ThunarVfsJobClass))
#define THUNAR_VFS_IS_JOB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_JOB))
#define THUNAR_VFS_IS_JOB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_JOB))
#define THUNAR_VFS_JOB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_JOB, ThunarVfsJobClass))

struct _ThunarVfsJobClass
{
  GObjectClass __parent__;

  /* virtual methods */
  void (*execute)  (ThunarVfsJob *job);

  /* signals */
  void (*finished) (ThunarVfsJob *job);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
};

struct _ThunarVfsJob
{
  GObject __parent__;

  /*< private >*/
  volatile gboolean    cancelled;
  ThunarVfsJobPrivate *priv;
};

GType                  thunar_vfs_job_get_type     (void) G_GNUC_CONST;

/* public API */
ThunarVfsJob          *thunar_vfs_job_launch       (ThunarVfsJob       *job);
void                   thunar_vfs_job_cancel       (ThunarVfsJob       *job);
G_INLINE_FUNC gboolean thunar_vfs_job_cancelled    (const ThunarVfsJob *job);



/* module API */
void                   thunar_vfs_job_emit_valist  (ThunarVfsJob       *job,
                                                    guint               signal_id,
                                                    GQuark              signal_detail,
                                                    va_list             var_args) G_GNUC_INTERNAL;
void                   thunar_vfs_job_emit         (ThunarVfsJob       *job,
                                                    guint               signal_id,
                                                    GQuark              signal_detail,
                                                    ...) G_GNUC_INTERNAL;
void                   thunar_vfs_job_error        (ThunarVfsJob       *job,
                                                    GError             *error) G_GNUC_INTERNAL;


/* inline function implementations */
#if defined(G_CAN_INLINE) || defined(__THUNAR_VFS_JOB_C__)
/**
 * thunar_vfs_job_cancelled:
 * @job : a #ThunarVfsJob.
 *
 * Checks whether @job was previously cancelled
 * by a call to thunar_vfs_job_cancel().
 *
 * Return value: %TRUE if @job is cancelled.
 **/
G_INLINE_FUNC gboolean
thunar_vfs_job_cancelled (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  return job->cancelled;
}
#endif /* G_CAN_INLINE || __THUNAR_VFS_JOB_C__ */


#if defined(THUNAR_VFS_COMPILATION)
void _thunar_vfs_job_init     (void) G_GNUC_INTERNAL;
void _thunar_vfs_job_shutdown (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__THUNAR_VFS_JOB_H__ */
