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

#ifndef __THUNAR_VFS_INTERACTIVE_JOB_H__
#define __THUNAR_VFS_INTERACTIVE_JOB_H__

#include <thunar-vfs/thunar-vfs-job.h>
#include <thunar-vfs/thunar-vfs-monitor.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsInteractiveJobClass ThunarVfsInteractiveJobClass;
typedef struct _ThunarVfsInteractiveJob      ThunarVfsInteractiveJob;

#define THUNAR_VFS_TYPE_INTERACTIVE_JOB             (thunar_vfs_interactive_job_get_type ())
#define THUNAR_VFS_INTERACTIVE_JOB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_INTERACTIVE_JOB, ThunarVfsInteractiveJob))
#define THUNAR_VFS_INTERACTIVE_JOB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_INTERACTIVE_JOB, ThunarVfsInteractiveJobClass))
#define THUNAR_VFS_IS_INTERACTIVE_JOB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_INTERACTIVE_JOB))
#define THUNAR_VFS_IS_INTERACTIVE_JOB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_INTERACTIVE_JOB))
#define THUNAR_VFS_INTERACTIVE_JOB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_INTERACTIVE_JOB, ThunarVfsInteractiveJobClass))

/**
 * ThunarVfsInteractiveJobResponse:
 * @THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES     :
 * @THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL :
 * @THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO      :
 * @THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL  :
 **/
typedef enum /*< flags >*/
{
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES     = 1 << 0,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL = 1 << 1,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO      = 1 << 2,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL  = 1 << 3,
} ThunarVfsInteractiveJobResponse;

struct _ThunarVfsInteractiveJobClass
{
  ThunarVfsJobClass __parent__;

  /* signals */
  ThunarVfsInteractiveJobResponse (*ask) (ThunarVfsInteractiveJob        *interactive_job,
                                          const gchar                    *message,
                                          ThunarVfsInteractiveJobResponse choices);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
};

struct _ThunarVfsInteractiveJob
{
  ThunarVfsJob __parent__;

  /* the last time we fired the "percent" signal (in msecs) */
  guint64 last_percent_time;

  /* connection to the default ThunarVfsMonitor */
  ThunarVfsMonitor *monitor;

  guint overwrite_all : 1;
  guint skip_all : 1;
};

GType     thunar_vfs_interactive_job_get_type     (void) G_GNUC_CONST;

void      thunar_vfs_interactive_job_info_message (ThunarVfsInteractiveJob *interactive_job,
                                                   const gchar             *message) G_GNUC_INTERNAL;
void      thunar_vfs_interactive_job_percent      (ThunarVfsInteractiveJob *interactive_job,
                                                   gdouble                  percent) G_GNUC_INTERNAL;

void      thunar_vfs_interactive_job_new_files    (ThunarVfsInteractiveJob *interactive_job,
                                                   const GList             *path_list) G_GNUC_INTERNAL;

gboolean  thunar_vfs_interactive_job_overwrite    (ThunarVfsInteractiveJob *interactive_job,
                                                   const gchar             *message) G_GNUC_INTERNAL;
gboolean  thunar_vfs_interactive_job_skip         (ThunarVfsInteractiveJob *interactive_job,
                                                   const gchar             *message) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_INTERACTIVE_JOB_H__ */
