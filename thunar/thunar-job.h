/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_JOB_H__
#define __THUNAR_JOB_H__

#include <gio/gio.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-file.h>

G_BEGIN_DECLS

typedef struct _ThunarJobPrivate ThunarJobPrivate;
typedef struct _ThunarJobClass   ThunarJobClass;
typedef struct _ThunarJob        ThunarJob;

#define THUNAR_TYPE_JOB            (thunar_job_get_type ())
#define THUNAR_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_JOB, ThunarJob))
#define THUNAR_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_JOB, ThunarJobClass))
#define THUNAR_IS_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_JOB))
#define THUNAR_IS_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_JOB))
#define THUNAR_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_JOB, ThunarJobClass))

struct _ThunarJobClass
{
  /*< private >*/
  GObjectClass __parent__;

  /*< public >*/

  /* virtual methods */
  gboolean          (*execute) (ThunarJob        *job,
                                GError          **error);

  /* signals */
  ThunarJobResponse (*ask)         (ThunarJob        *job,
                                    const gchar      *message,
                                    ThunarJobResponse choices);
  ThunarJobResponse (*ask_replace) (ThunarJob        *job,
                                    ThunarFile       *source_file,
                                    ThunarFile       *target_file);
};

struct _ThunarJob
{
  /*< private >*/
  GObject           __parent__;
  ThunarJobPrivate *priv;
};

GType             thunar_job_get_type               (void) G_GNUC_CONST;
ThunarJob        *thunar_job_launch                 (ThunarJob       *job);
void              thunar_job_cancel                 (ThunarJob       *job);
gboolean          thunar_job_is_cancelled           (const ThunarJob *job);

GCancellable     *thunar_job_get_cancellable        (const ThunarJob *job);
gboolean          thunar_job_set_error_if_cancelled (ThunarJob       *job,
                                                     GError         **error);

void              thunar_job_set_total_files        (ThunarJob       *job,
                                                     GList           *total_files);
void              thunar_job_processing_file        (ThunarJob       *job,
                                                     GList           *current_file);

void              thunar_job_emit                   (ThunarJob       *job,
                                                     guint            signal_id,
                                                     GQuark           signal_detail,
                                                     ...);
ThunarJobResponse thunar_job_ask_create             (ThunarJob       *job,
                                                     const gchar     *format,
                                                     ...);
ThunarJobResponse thunar_job_ask_overwrite          (ThunarJob       *job,
                                                     const gchar     *format,
                                                     ...);
ThunarJobResponse thunar_job_ask_replace            (ThunarJob       *job,
                                                     GFile           *source_path,
                                                     GFile           *target_path,
                                                     GError         **error);
ThunarJobResponse thunar_job_ask_skip               (ThunarJob       *job,
                                                     const gchar     *format,
                                                     ...);
void              thunar_job_info_message           (ThunarJob       *job,
                                                     const gchar     *message);
void              thunar_job_new_files              (ThunarJob       *job,
                                                     const GList     *file_list);
void              thunar_job_percent                (ThunarJob       *job,
                                                     gdouble          percent);

G_END_DECLS

#endif /* !__THUNAR_JOB_H__ */
