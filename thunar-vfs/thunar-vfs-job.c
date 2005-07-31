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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gobject/gvaluecollector.h>

#include <thunar-vfs/thunar-vfs-job.h>



enum
{
  ERROR,
  FINISHED,
  LAST_SIGNAL,
};

typedef struct
{
  ThunarVfsJob     *job;
  guint             signal_id;
  GQuark            signal_detail;
  va_list           var_args;
  volatile gboolean pending;
} ThunarVfsJobEmitDetails;



static void     thunar_vfs_job_register_type      (GType             *type);
static void     thunar_vfs_job_class_init         (ThunarVfsJobClass *klass);
static void     thunar_vfs_job_init               (ThunarVfsJob      *job);
static void     thunar_vfs_job_finalize           (ThunarVfsJob      *job);
static void     thunar_vfs_job_execute            (gpointer           data,
                                                   gpointer           user_data);
static gboolean thunar_vfs_job_idle               (gpointer           user_data);
static void     thunar_vfs_job_value_init         (GValue            *value);
static void     thunar_vfs_job_value_free         (GValue            *value);
static void     thunar_vfs_job_value_copy         (const GValue      *src_value,
                                                   GValue            *dst_value);
static gpointer thunar_vfs_job_value_peek_pointer (const GValue      *value);
static gchar*   thunar_vfs_job_value_collect      (GValue            *value,
                                                   guint              n_collect_values,
                                                   GTypeCValue       *collect_values,
                                                   guint              collect_flags);
static gchar*   thunar_vfs_job_value_lcopy        (const GValue      *value,
                                                   guint              n_collect_values,
                                                   GTypeCValue       *collect_values,
                                                   guint              collect_flags);



static guint        job_signals[LAST_SIGNAL];
static GThreadPool *job_pool = NULL;



GType
thunar_vfs_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_job_register_type, &type);

  return type;
}



static void
thunar_vfs_job_register_type (GType *type)
{
  static const GTypeFundamentalInfo finfo =
  {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE | G_TYPE_FLAG_DERIVABLE | G_TYPE_FLAG_DEEP_DERIVABLE,
  };

  static const GTypeValueTable value_table =
  {
    thunar_vfs_job_value_init,
    thunar_vfs_job_value_free,
    thunar_vfs_job_value_copy,
    thunar_vfs_job_value_peek_pointer,
    "p",
    thunar_vfs_job_value_collect,
    "p",
    thunar_vfs_job_value_lcopy,
  };

  static const GTypeInfo info =
  {
    sizeof (ThunarVfsJobClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_job_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsJob),
    0,
    (GInstanceInitFunc) thunar_vfs_job_init,
    &value_table,
  };

  *type = g_type_register_fundamental (g_type_fundamental_next (), "ThunarVfsJob", &info, &finfo, G_TYPE_FLAG_ABSTRACT);
}



static void
thunar_vfs_job_class_init (ThunarVfsJobClass *klass)
{
  klass->finalize = thunar_vfs_job_finalize;

  /**
   * ThunarVfsJob::error:
   * @job   : a #ThunarVfsJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while executing the
   * @job.
   **/
  job_signals[ERROR] =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsJob::finished:
   * @job : a #ThunarVfsJob.
   *
   * This signal will be automatically emitted once the
   * @job finishes its execution, no matter whether @job
   * completed successfully or was cancelled by the
   * user.
   **/
  job_signals[FINISHED] =
    g_signal_new ("finished",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_vfs_job_init (ThunarVfsJob *job)
{
  job->ref_count = 1;
  job->cond = g_cond_new ();
  job->mutex = g_mutex_new ();
}



static void
thunar_vfs_job_finalize (ThunarVfsJob *job)
{
  /* destroy the synchronization entities */
  g_mutex_free (job->mutex);
  g_cond_free (job->cond);

  /* disconnect all handlers for this instance */
  g_signal_handlers_destroy (job);
}



static void
thunar_vfs_job_execute (gpointer data,
                        gpointer user_data)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB (data);

  /* perform the real work */
  (*THUNAR_VFS_JOB_GET_CLASS (job)->execute) (job);

  /* emit the "finished" signal (if not cancelled!) */
  thunar_vfs_job_emit (job, job_signals[FINISHED], 0);

  /* cleanup */
  thunar_vfs_job_unref (job);
}



static gboolean
thunar_vfs_job_idle (gpointer user_data)
{
  ThunarVfsJobEmitDetails *details = user_data;
  ThunarVfsJob            *job = details->job;

  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);

  g_mutex_lock (job->mutex);

  /* emit the signal */
  GDK_THREADS_ENTER ();
  g_signal_emit_valist (job, details->signal_id, details->signal_detail, details->var_args);
  GDK_THREADS_LEAVE ();

  /* tell the other thread, that we're done */
  details->pending = FALSE;
  g_cond_signal (job->cond);
  g_mutex_unlock (job->mutex);

  /* remove the idle source */
  return FALSE;
}



static void
thunar_vfs_job_value_init (GValue *value)
{
  /* nothing to do here, as value is already zero-filled! */
}



static void
thunar_vfs_job_value_free (GValue *value)
{
  if (G_LIKELY (value->data[0].v_pointer != NULL))
    thunar_vfs_job_unref (value->data[0].v_pointer);
}



static void
thunar_vfs_job_value_copy (const GValue *src_value,
                           GValue       *dst_value)
{
  if (G_LIKELY (src_value->data[0].v_pointer != NULL))
    dst_value->data[0].v_pointer = thunar_vfs_job_ref (src_value->data[0].v_pointer);
}



static gpointer
thunar_vfs_job_value_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}



static gchar*
thunar_vfs_job_value_collect (GValue      *value,
                              guint        n_collect_values,
                              GTypeCValue *collect_values,
                              guint        collect_flags)
{
  if (G_LIKELY (collect_values[0].v_pointer != NULL))
    value->data[0].v_pointer = thunar_vfs_job_ref (collect_values[0].v_pointer);

  return NULL;
}



static gchar*
thunar_vfs_job_value_lcopy (const GValue *value,
                            guint         n_collect_values,
                            GTypeCValue  *collect_values,
                            guint         collect_flags)
{
  ThunarVfsJob **job_p = collect_values[0].v_pointer;

  g_return_val_if_fail (job_p != NULL, NULL);

  if (value->data[0].v_pointer == NULL)
    *job_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *job_p = value->data[0].v_pointer;
  else
    *job_p = thunar_vfs_job_ref (value->data[0].v_pointer);

  return NULL;
}



/**
 * thunar_vfs_job_ref:
 * @job : a #ThunarVfsJob.
 *
 * Increments the reference count on @job by
 * 1 and returns a pointer to @job.
 *
 * Return value: a pointer to @job.
 **/
ThunarVfsJob*
thunar_vfs_job_ref (ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (job->ref_count > 0, NULL);

  g_atomic_int_inc (&job->ref_count);

  return job;
}



/**
 * thunar_vfs_job_unref:
 * @job : a #ThunarVfsJob.
 *
 * Decrements the reference count on @job by
 * 1. If the reference count drops to zero,
 * the resources allocated to @job will be
 * freed.
 **/
void
thunar_vfs_job_unref (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  if (g_atomic_int_dec_and_test (&job->ref_count))
    {
      /* call the finalize method (if any) */
      if (THUNAR_VFS_JOB_GET_CLASS (job)->finalize != NULL)
        (*THUNAR_VFS_JOB_GET_CLASS (job)->finalize) (job);

      /* free the instance resources */
      g_type_free_instance ((GTypeInstance *) job);
    }
}



/**
 * thunar_vfs_job_launch:
 * @job : a #ThunarVfsJob.
 *
 * This functions schedules @job to be run as soon
 * as possible, in a separate thread.
 *
 * Return value: a pointer to @job.
 **/
ThunarVfsJob*
thunar_vfs_job_launch (ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (job->ref_count > 0, NULL);
  g_return_val_if_fail (!job->launched, NULL);
  g_return_val_if_fail (!job->cancelled, NULL);
  g_return_val_if_fail (job_pool != NULL, NULL);

  /* mark the job as launched */
  job->launched = TRUE;

  /* schedule the job on the thread pool */
  g_thread_pool_push (job_pool, thunar_vfs_job_ref (job), NULL);

  return job;
}



/**
 * thunar_vfs_job_cancel:
 * @job : a #ThunarVfsJob.
 *
 * Attempts to cancel the operation currently
 * performed by @job. Even after the cancellation
 * of @job, it may still emit signals, so you
 * must take care of disconnecting all handlers
 * appropriately if you cannot handle signals
 * after cancellation.
 **/
void
thunar_vfs_job_cancel (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  job->cancelled = TRUE;
}



/**
 * thunar_vfs_job_cancelled:
 * @job : a #ThunarVfsJob.
 *
 * Checks whether @job was previously cancelled
 * by a call to #thunar_vfs_job_cancel().
 *
 * Return value: %TRUE if @job is cancelled.
 **/
gboolean
thunar_vfs_job_cancelled (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  g_return_val_if_fail (job->ref_count > 0, FALSE);

  return job->cancelled;
}



/**
 * thunar_vfs_job_emit:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on @job.
 * @signal_detail : the detail.
 * @var_args      : a list of paramters to be passed to the signal,
 *                  folled by a location for the return value. If
 *                  the return type of the signals is #G_TYPE_NONE,
 *                  the return value location can be omitted.
 *
 * Emits the signal identified by @signal_id on @job in
 * the context of the main thread. This method doesn't
 * return until the signal was emitted.
 **/
void
thunar_vfs_job_emit_valist (ThunarVfsJob *job,
                            guint         signal_id,
                            GQuark        signal_detail,
                            va_list       var_args)
{
  ThunarVfsJobEmitDetails details;

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);
  g_return_if_fail (job->launched);

  details.job = job;
  details.signal_id = signal_id;
  details.signal_detail = signal_detail;
  details.var_args = var_args;
  details.pending = TRUE;

  g_mutex_lock (job->mutex);
  g_idle_add_full (G_PRIORITY_DEFAULT, thunar_vfs_job_idle, &details, NULL);
  while (G_UNLIKELY (details.pending))
    g_cond_wait (job->cond, job->mutex);
  g_mutex_unlock (job->mutex);
}



/**
 * thunar_vfs_job_emit:
 * @job           : a #ThunarVfsJob.
 * @signal_id     :
 * @signal_detail :
 * @...           :
 **/
void
thunar_vfs_job_emit (ThunarVfsJob *job,
                     guint         signal_id,
                     GQuark        signal_detail,
                     ...)
{
  va_list var_args;

  va_start (var_args, signal_detail);
  thunar_vfs_job_emit_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



/**
 * thunar_vfs_job_error:
 * @job   : a #ThunarVfsJob.
 * @error : a #GError describing the error cause.
 *
 * Emits the ::error signal on @job with the given @error. Whether
 * or not the @job continues after emitting the error depends on
 * the particular implementation of @job, but most jobs will
 * terminate instantly after emitting an error.
 **/
void
thunar_vfs_job_error (ThunarVfsJob *job,
                      GError       *error)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (error != NULL && error->message != NULL);

  thunar_vfs_job_emit (job, job_signals[ERROR], 0, error);
}




GType
thunar_vfs_param_spec_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (GParamSpecClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof (ThunarVfsParamSpecJob),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_PARAM,
                                     "ThunarVfsParamSpecJob",
                                     &info, 0);
    }

  return type;
}



/**
 * thunar_vfs_param_spec_job:
 * @name
 * @nick
 * @blurb
 * @job_type
 * @flags
 *
 * Return value:
 **/
GParamSpec*
thunar_vfs_param_spec_job (const gchar *name,
                           const gchar *nick,
                           const gchar *blurb,
                           GType        job_type,
                           GParamFlags  flags)
{
  GParamSpec *pspec;

  g_return_val_if_fail (g_type_is_a (job_type, THUNAR_VFS_TYPE_JOB), NULL);

  pspec = g_param_spec_internal (THUNAR_VFS_TYPE_PARAM_JOB, name, nick, blurb, flags);
  pspec->value_type = job_type;

  return pspec;
}



/**
 * thunar_vfs_value_set_job:
 * @value : a valid #GValue of type #THUNAR_VFS_TYPE_JOB or a derived type.
 * @job   : the #ThunarVfsJob to set or %NULL.
 *
 * Sets the contents of @value to @job.
 **/
void
thunar_vfs_value_set_job (GValue  *value,
                          gpointer job)
{
  g_return_if_fail (THUNAR_VFS_VALUE_HOLDS_JOB (value));
  g_return_if_fail (job == NULL || THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job == NULL || g_value_type_compatible (G_TYPE_FROM_INSTANCE (job), G_VALUE_TYPE (value)));

  if (value->data[0].v_pointer != NULL)
    thunar_vfs_job_unref (value->data[0].v_pointer);

  value->data[0].v_pointer = job;

  if (G_LIKELY (job != NULL))
    thunar_vfs_job_ref (job);
}



/**
 * thunar_vfs_value_take_job:
 * @value : a valid #GValue of type #THUNAR_VFS_TYPE_JOB or a derived type.
 * @job   : the #ThunarVfsJob to take over or %NULL.
 *
 * Sets the contents of @value to @job and takes over the ownership of
 * the callers reference to @job; the caller doesn't have to unref it
 * any more.
 **/
void
thunar_vfs_value_take_job (GValue  *value,
                           gpointer job)
{
  g_return_if_fail (THUNAR_VFS_VALUE_HOLDS_JOB (value));
  g_return_if_fail (job == NULL || THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job == NULL || g_value_type_compatible (G_TYPE_FROM_INSTANCE (job), G_VALUE_TYPE (value)));

  if (value->data[0].v_pointer != NULL)
    thunar_vfs_job_unref (value->data[0].v_pointer);

  value->data[0].v_pointer = job;
}



/**
 * thunar_vfs_value_get_job:
 * @value : a valid #GValue of type #THUNAR_VFS_TYPE_JOB or a derived type.
 *
 * Queries the #ThunarVfsJob stored within @value and returns it. The
 * stored value may be %NULL.
 *
 * Return value: the #ThunarVfsJob stored in @value.
 **/
gpointer
thunar_vfs_value_get_job (const GValue *value)
{
  g_return_val_if_fail (THUNAR_VFS_VALUE_HOLDS_JOB (value), NULL);
  return value->data[0].v_pointer;
}



/**
 * thunar_vfs_value_dup_job:
 * @value : a valid #GValue of type #THUNAR_VFS_TYPE_JOB or a derived type.
 *
 * Similar to #thunar_vfs_value_get_job(), but also takes a reference for
 * the caller if @value contains a valid #ThunarVfsJob.
 *
 * Return value: the #ThunarVfsJob stored in @value with an additional
 *               reference taken for the caller.
 **/
gpointer
thunar_vfs_value_dup_job (const GValue *value)
{
  g_return_val_if_fail (THUNAR_VFS_VALUE_HOLDS_JOB (value), NULL);
  return (value->data[0].v_pointer != NULL) ? thunar_vfs_job_ref (value->data[0].v_pointer) : NULL;
}




/**
 * _thunar_vfs_job_init:
 *
 * Initializes the jobs module of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_job_init (void)
{
  g_return_if_fail (job_pool == NULL);

  job_pool = g_thread_pool_new (thunar_vfs_job_execute, NULL, 8, FALSE, NULL);
}




