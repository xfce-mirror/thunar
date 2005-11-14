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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-marshal.h>
#include <thunar-vfs/thunar-vfs-alias.h>



enum
{
  ASK,
  INFO_MESSAGE,
  NEW_FILES,
  PERCENT,
  LAST_SIGNAL,
};



static void                            thunar_vfs_interactive_job_class_init (ThunarVfsInteractiveJobClass   *klass);
static void                            thunar_vfs_interactive_job_init       (ThunarVfsInteractiveJob        *interactive_job);
static void                            thunar_vfs_interactive_job_finalize   (GObject                        *object);
static ThunarVfsInteractiveJobResponse thunar_vfs_interactive_job_real_ask   (ThunarVfsInteractiveJob        *interactive_job,
                                                                              const gchar                    *message,
                                                                              ThunarVfsInteractiveJobResponse choices);
static ThunarVfsInteractiveJobResponse thunar_vfs_interactive_job_ask        (ThunarVfsInteractiveJob        *interactive_job,
                                                                              const gchar                    *message,
                                                                              ThunarVfsInteractiveJobResponse choices);



static guint interactive_signals[LAST_SIGNAL];



G_DEFINE_ABSTRACT_TYPE (ThunarVfsInteractiveJob, thunar_vfs_interactive_job, THUNAR_VFS_TYPE_JOB);



static gboolean
ask_accumulator (GSignalInvocationHint *ihint,
                 GValue                *return_accu,
                 const GValue          *handler_return,
                 gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  return FALSE;
}



static void
thunar_vfs_interactive_job_class_init (ThunarVfsInteractiveJobClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_interactive_job_finalize;

  klass->ask = thunar_vfs_interactive_job_real_ask;

  /**
   * ThunarVfsInteractiveJob::ask:
   * @job     : a #ThunarVfsJob.
   * @message : question to display to the user.
   * @choices : a combination of #ThunarVfsInteractiveJobResponse<!---->s.
   *
   * The @message is garantied to contain valid UTF-8.
   *
   * Return value: the selected choice.
   **/
  interactive_signals[ASK] =
    g_signal_new ("ask",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsInteractiveJobClass, ask),
                  ask_accumulator, NULL,
                  _thunar_vfs_marshal_FLAGS__STRING_FLAGS,
                  THUNAR_VFS_TYPE_VFS_INTERACTIVE_JOB_RESPONSE,
                  2, G_TYPE_STRING,
                  THUNAR_VFS_TYPE_VFS_INTERACTIVE_JOB_RESPONSE);

  /**
   * ThunarVfsInteractiveJob::new-files:
   * @job       : a #ThunarVfsJob.
   * @path_list : a list of #ThunarVfsPath<!---->s that were created by @job.
   *
   * This signal is emitted by the @job right before the @job is terminated
   * and informs the application about the list of created files in @path_list.
   * @path_list contains only the toplevel path items, that were specified by
   * the application on creation of the @job.
   **/
  interactive_signals[NEW_FILES] =
    g_signal_new ("new-files",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsInteractiveJob::info-message:
   * @job     : a #ThunarVfsJob.
   * @message : information to be displayed about @job.
   *
   * This signal is emitted to display information about the
   * @job. Examples of messages are "Preparing..." or
   * "Cleaning up...".
   *
   * The @message is garantied to contain valid UTF-8, so
   * it can be displayed by #GtkWidget<!---->s out of the
   * box.
   **/
  interactive_signals[INFO_MESSAGE] =
    g_signal_new ("info-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * ThunarVfsInteractiveJob::percent:
   * @job     : a #ThunarVfsJob.
   * @percent : the percentage of completeness.
   *
   * This signal is emitted to present the state
   * of the overall progress.
   **/
  interactive_signals[PERCENT] =
    g_signal_new ("percent",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}



static void
thunar_vfs_interactive_job_init (ThunarVfsInteractiveJob *interactive_job)
{
  /* grab a reference on the default vfs monitor */
  interactive_job->monitor = thunar_vfs_monitor_get_default ();
}



static void
thunar_vfs_interactive_job_finalize (GObject *object)
{
  ThunarVfsInteractiveJob *interactive_job = THUNAR_VFS_INTERACTIVE_JOB (object);

  /* release the reference on the default vfs monitor */
  g_object_unref (G_OBJECT (interactive_job->monitor));

  (*G_OBJECT_CLASS (thunar_vfs_interactive_job_parent_class)->finalize) (object);
}



static ThunarVfsInteractiveJobResponse
thunar_vfs_interactive_job_real_ask (ThunarVfsInteractiveJob        *interactive_job,
                                     const gchar                    *message,
                                     ThunarVfsInteractiveJobResponse choices)
{
  return THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL;
}



static ThunarVfsInteractiveJobResponse
thunar_vfs_interactive_job_ask (ThunarVfsInteractiveJob        *interactive_job,
                                const gchar                    *message,
                                ThunarVfsInteractiveJobResponse choices)
{
  ThunarVfsInteractiveJobResponse response;
  thunar_vfs_job_emit (THUNAR_VFS_JOB (interactive_job), interactive_signals[ASK], 0, message, choices, &response);
  return response;
}



/**
 * thunar_vfs_interactive_job_info_message:
 * @interactive_job : a #ThunarVfsInteractiveJob.
 * @message         : an informational message about @interactive_job.
 *
 * Emits the ::info-message signal on @interactive_job with @message.
 *
 * The text contained in @message must be valid UTF-8.
 **/
void
thunar_vfs_interactive_job_info_message (ThunarVfsInteractiveJob *interactive_job,
                                         const gchar             *message)
{
  g_return_if_fail (THUNAR_VFS_IS_INTERACTIVE_JOB (interactive_job));
  g_return_if_fail (g_utf8_validate (message, -1, NULL));

  thunar_vfs_job_emit (THUNAR_VFS_JOB (interactive_job), interactive_signals[INFO_MESSAGE], 0, message);
}



/**
 * thunar_vfs_interactive_job_percent:
 * @interactive_job : a #ThunarVfsInteractiveJob.
 * @percent         : the percentage of completeness (in the range of 0.0 to 100.0).
 *
 * Emits the ::percent signal on @interactive_job with @percent.
 **/
void
thunar_vfs_interactive_job_percent (ThunarVfsInteractiveJob *interactive_job,
                                    gdouble                  percent)
{
  struct timeval tv;
  guint64        time;

  g_return_if_fail (THUNAR_VFS_IS_INTERACTIVE_JOB (interactive_job));
  g_return_if_fail (percent >= 0.0 && percent <= 100.0);

  /* determine the current time in msecs */
  gettimeofday (&tv, NULL);
  time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

  /* don't fire the "percent" signal more than ten times per second
   * to avoid unneccessary load on the main thread.
   */
  if (G_UNLIKELY (time - interactive_job->last_percent_time > 100))
    {
      thunar_vfs_job_emit (THUNAR_VFS_JOB (interactive_job), interactive_signals[PERCENT], 0, percent);
      interactive_job->last_percent_time = time;
    }
}



/**
 * thunar_vfs_interactive_job_new_files:
 * @interactive_job : a #ThunarVfsInteractiveJob.
 * @path_list       : the #ThunarVfsPath<!---->s that were created by @interactive_job.
 *
 * Emits the ::created signal on @interactive_job with @info_list.
 **/
void
thunar_vfs_interactive_job_new_files (ThunarVfsInteractiveJob *interactive_job,
                                      const GList             *path_list)
{
  g_return_if_fail (THUNAR_VFS_IS_INTERACTIVE_JOB (interactive_job));

  /* wait for the monitor to process all pending events */
  thunar_vfs_monitor_wait (interactive_job->monitor);

  /* emit the new-files signal */
  thunar_vfs_job_emit (THUNAR_VFS_JOB (interactive_job), interactive_signals[NEW_FILES], 0, path_list);
}



/**
 * thunar_vfs_interactive_job_overwrite:
 * @interactive_job : a #ThunarVfsInteractiveJob.
 * @message         : the message to be displayed in valid UTF-8.
 *
 * Asks the user whether to overwrite a certain file as described by
 * @message.
 *
 * The return value may be %TRUE to perform the overwrite, or %FALSE,
 * which means to either skip the file or cancel the @interactive_job.
 * The caller must check using thunar_vfs_job_cancelled() if %FALSE
 * is returned.
 *
 * Return value: %TRUE to overwrite or %FALSE to leave it or cancel
 *               the @interactive_job.
 **/
gboolean
thunar_vfs_interactive_job_overwrite (ThunarVfsInteractiveJob *interactive_job,
                                      const gchar             *message)
{
  ThunarVfsInteractiveJobResponse response;

  g_return_val_if_fail (THUNAR_VFS_IS_INTERACTIVE_JOB (interactive_job), FALSE);
  g_return_val_if_fail (g_utf8_validate (message, -1, NULL), FALSE);

  /* check if the user cancelled the job already */
  if (thunar_vfs_job_cancelled (THUNAR_VFS_JOB (interactive_job)))
    return FALSE;

  /* check if the user said "overwrite all" earlier */
  if (interactive_job->overwrite_all)
    return TRUE;

  /* ask the user */
  response = thunar_vfs_interactive_job_ask (interactive_job, message,
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES |
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL |
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO |
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL);

  /* evaluate the response */
  switch (response)
    {
    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL:
      interactive_job->overwrite_all = TRUE;
      /* FALL-THROUGH */

    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES:
      return TRUE;

    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL:
      thunar_vfs_job_cancel (THUNAR_VFS_JOB (interactive_job));
      /* FALL-THROUGH */

    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_NO:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}



/**
 * thunar_vfs_interactive_job_skip:
 * @interactive_job : a #ThunarVfsInteractiveJob.
 * @message         : the message to be displayed as valid UTF-8.
 *
 * Asks the user whether to skip a certain file as described by
 * @message.
 *
 * The return value may be %TRUE to perform the skip, or %FALSE,
 * which means to cancel the @interactive_job.
 *
 * Return value: %TRUE to overwrite or %FALSE to cancel
 *               the @interactive_job.
 **/
gboolean
thunar_vfs_interactive_job_skip (ThunarVfsInteractiveJob *interactive_job,
                                 const gchar             *message)
{
  ThunarVfsInteractiveJobResponse response;

  g_return_val_if_fail (THUNAR_VFS_IS_INTERACTIVE_JOB (interactive_job), FALSE);
  g_return_val_if_fail (g_utf8_validate (message, -1, NULL), FALSE);

  /* check if the user cancelled the job already */
  if (thunar_vfs_job_cancelled (THUNAR_VFS_JOB (interactive_job)))
    return FALSE;

  /* check if the user said "skip all" earlier */
  if (interactive_job->skip_all)
    return TRUE;

  /* ask the user */
  response = thunar_vfs_interactive_job_ask (interactive_job, message,
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES |
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL |
                                             THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL);

  /* evaluate the response */
  switch (response)
    {
    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES_ALL:
      interactive_job->skip_all = TRUE;
      /* FALL-THROUGH */

    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_YES:
      return TRUE;

    case THUNAR_VFS_INTERACTIVE_JOB_RESPONSE_CANCEL:
      thunar_vfs_job_cancel (THUNAR_VFS_JOB (interactive_job));
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}



#define __THUNAR_VFS_INTERACTIVE_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
