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

#ifndef __THUNAR_VFS_INTERACTIVE_JOB_H__
#define __THUNAR_VFS_INTERACTIVE_JOB_H__

#include <thunar-vfs/thunar-vfs-job.h>

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
 *
 * Deprecated: 0.3.3: Use #ThunarVfsJobResponse instead.
 **/
typedef enum /*< skip >*/
{
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES     = 1 << 0,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL = 1 << 1,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO      = 1 << 2,
  THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL  = 1 << 3,
} ThunarVfsInteractiveJobResponse;

#define THUNAR_VFS_TYPE_VFS_INTERACTIVE_JOB_RESPONSE (thunar_vfs_interactive_job_response_get_type ())
GType thunar_vfs_interactive_job_response_get_type (void) G_GNUC_CONST;

struct _ThunarVfsInteractiveJobClass
{
  /*< private >*/
  ThunarVfsJobClass __parent__;
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
};

struct _ThunarVfsInteractiveJob
{
  /*< private >*/
  ThunarVfsJob __parent__;
  guint64      reserved0; /* backward ABI compatibility */
  gpointer     reserved1;
  guint        reserved2 : 1;
  guint        reserved3 : 1;
};

GType thunar_vfs_interactive_job_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_VFS_INTERACTIVE_JOB_H__ */
