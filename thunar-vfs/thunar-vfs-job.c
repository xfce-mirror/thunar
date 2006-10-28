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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* implement thunar-vfs-job's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __THUNAR_VFS_JOB_C__
#include <thunar-vfs/thunar-vfs-job.h>

#include <gobject/gvaluecollector.h>

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-job-private.h>
#include <thunar-vfs/thunar-vfs-marshal.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



#define THUNAR_VFS_JOB_SOURCE(source)   ((ThunarVfsJobSource *) (source))
#define THUNAR_VFS_JOB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_VFS_TYPE_JOB, ThunarVfsJobPrivate))



/* Signal identifiers */
enum
{
  ASK,
  ERROR,
  FINISHED,
  INFO_MESSAGE,
  INFOS_READY,
  NEW_FILES,
  PERCENT,
  LAST_SIGNAL,
};



typedef struct _ThunarVfsJobEmitAsync ThunarVfsJobEmitAsync;
typedef struct _ThunarVfsJobEmitSync  ThunarVfsJobEmitSync;
typedef struct _ThunarVfsJobSource    ThunarVfsJobSource;



static void                 thunar_vfs_job_class_init       (ThunarVfsJobClass    *klass);
static void                 thunar_vfs_job_init             (ThunarVfsJob         *job);
static ThunarVfsJobResponse thunar_vfs_job_real_ask         (ThunarVfsJob         *job,
                                                             const gchar          *message,
                                                             ThunarVfsJobResponse  choices);
static void                 thunar_vfs_job_execute          (gpointer              data,
                                                             gpointer              user_data);
static gboolean             thunar_vfs_job_source_prepare   (GSource              *source,
                                                             gint                 *timeout);
static gboolean             thunar_vfs_job_source_check     (GSource              *source);
static gboolean             thunar_vfs_job_source_dispatch  (GSource              *source,
                                                             GSourceFunc           callback,
                                                             gpointer              user_data);
static void                 thunar_vfs_job_source_finalize  (GSource              *source);



struct _ThunarVfsJobPrivate
{
  ThunarVfsJobEmitAsync *emit_async;
  ThunarVfsJobEmitSync  *emit_sync;
  gboolean               running;

  /* for ask_overwrite()/ask_skip() */
  guint                  ask_overwrite_all : 1;
  guint                  ask_overwrite_none : 1;
  guint                  ask_skip_all : 1;

  /* for total_paths()/process_path() */
  GList                 *total_paths;
};

struct _ThunarVfsJobEmitAsync /* an asychronous notification */
{
  ThunarVfsJobEmitAsync *next;
  guint                  signal_id;
  GQuark                 signal_detail;
  guint                  n_values;
  GValue                *values;
};

struct _ThunarVfsJobEmitSync  /* a synchronous signal */
{
  guint             signal_id;
  GQuark            signal_detail;
  va_list           var_args;
  volatile gboolean pending;
};

struct _ThunarVfsJobSource
{
  GSource       source;
  ThunarVfsJob *job;
};



static GSourceFuncs thunar_vfs_job_source_funcs =
{
  thunar_vfs_job_source_prepare,
  thunar_vfs_job_source_check,
  thunar_vfs_job_source_dispatch,
  thunar_vfs_job_source_finalize,
  NULL,
};

static GObjectClass  *thunar_vfs_job_parent_class;
static guint          job_signals[LAST_SIGNAL];
static GThreadPool   *job_pool = NULL;
static GCond         *job_cond = NULL;
static GMutex        *job_mutex = NULL;
static volatile guint jobs_running = 0;



GType
thunar_vfs_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsJob",
                                                 sizeof (ThunarVfsJobClass),
                                                 thunar_vfs_job_class_init,
                                                 sizeof (ThunarVfsJob),
                                                 thunar_vfs_job_init,
                                                 G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static gboolean
_thunar_vfs_job_ask_accumulator (GSignalInvocationHint *ihint,
                                 GValue                *return_accu,
                                 const GValue          *handler_return,
                                 gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  return FALSE;
}



static void
thunar_vfs_job_class_init (ThunarVfsJobClass *klass)
{
  /* add our private data for this class */
  g_type_class_add_private (klass, sizeof (ThunarVfsJobPrivate));

  /* determine the parent class */
  thunar_vfs_job_parent_class = g_type_class_peek_parent (klass);

  klass->ask = thunar_vfs_job_real_ask;

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
  job_signals[ASK] =
    g_signal_new (I_("ask"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsJobClass, ask),
                  _thunar_vfs_job_ask_accumulator, NULL,
                  _thunar_vfs_marshal_FLAGS__STRING_FLAGS,
                  THUNAR_VFS_TYPE_VFS_JOB_RESPONSE,
                  2, G_TYPE_STRING,
                  THUNAR_VFS_TYPE_VFS_JOB_RESPONSE);

  /**
   * ThunarVfsJob::error:
   * @job   : a #ThunarVfsJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while executing the
   * @job.
   **/
  job_signals[ERROR] =
    g_signal_new (I_("error"),
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
    g_signal_new (I_("finished"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarVfsJob::info-message:
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
  job_signals[INFO_MESSAGE] =
    g_signal_new (I_("info-message"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * ThunarVfsJob::infos-ready:
   * @job       : a #ThunarVfsJob.
   * @info_list : a list of #ThunarVfsInfo<!---->s.
   *
   * This signal is used by #ThunarVfsJob<!---->s returned by
   * the thunar_vfs_listdir() function whenever there's a bunch
   * of #ThunarVfsInfo<!---->s ready. This signal is garantied
   * to be never emitted with an @info_list parameter of %NULL.
   *
   * To allow some further optimizations on the handler-side,
   * the handler is allowed to take over ownership of the
   * @info_list, i.e. it can reuse the @infos list and just replace
   * the data elements with it's own objects based on the
   * #ThunarVfsInfo<!---->s contained within the @info_list (and
   * of course properly unreffing the previously contained infos).
   * If a handler takes over ownership of @info_list it must return
   * %TRUE here, and no further handlers will be run. Else, if
   * the handler doesn't want to take over ownership of @infos,
   * it must return %FALSE, and other handlers will be run. Use
   * this feature with care, and only if you can be sure that
   * you are the only handler connected to this signal for a
   * given job!
   *
   * Return value: %TRUE if the handler took over ownership of
   *               @info_list, else %FALSE.
   **/
  job_signals[INFOS_READY] =
    g_signal_new (I_("infos-ready"),
                  G_TYPE_FROM_CLASS (klass), G_SIGNAL_NO_HOOKS,
                  0, g_signal_accumulator_true_handled, NULL,
                  _thunar_vfs_marshal_BOOLEAN__POINTER,
                  G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsJob::new-files:
   * @job       : a #ThunarVfsJob.
   * @path_list : a list of #ThunarVfsPath<!---->s that were created by @job.
   *
   * This signal is emitted by the @job right before the @job is terminated
   * and informs the application about the list of created files in @path_list.
   * @path_list contains only the toplevel path items, that were specified by
   * the application on creation of the @job.
   **/
  job_signals[NEW_FILES] =
    g_signal_new (I_("new-files"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsJob::percent:
   * @job     : a #ThunarVfsJob.
   * @percent : the percentage of completeness.
   *
   * This signal is emitted to present the state
   * of the overall progress.
   *
   * The @percent value is garantied to be in the
   * range 0.0 to 100.0.
   **/
  job_signals[PERCENT] =
    g_signal_new (I_("percent"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}



static void
thunar_vfs_job_init (ThunarVfsJob *job)
{
  job->priv = THUNAR_VFS_JOB_GET_PRIVATE (job);
}



static ThunarVfsJobResponse
thunar_vfs_job_real_ask (ThunarVfsJob        *job,
                         const gchar         *message,
                         ThunarVfsJobResponse choices)
{
  return THUNAR_VFS_JOB_RESPONSE_CANCEL;
}



static void
thunar_vfs_job_execute (gpointer data,
                        gpointer user_data)
{
  ThunarVfsJobEmitAsync *emit_async;
  ThunarVfsJob          *job = THUNAR_VFS_JOB (data);

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->priv->running);

  /* perform the real work */
  (*THUNAR_VFS_JOB_GET_CLASS (job)->execute) (job);

  /* mark the job as done */
  job->priv->running = FALSE;

  /* release pending asynchronous emissions */
  while (job->priv->emit_async)
    {
      /* drop the first asynchronous emission */
      emit_async = job->priv->emit_async;
      job->priv->emit_async = emit_async->next;
      _thunar_vfs_g_value_array_free (emit_async->values, emit_async->n_values);
      _thunar_vfs_slice_free (ThunarVfsJobEmitAsync, emit_async);
    }

  /* wake up the main event loop */
  g_main_context_wakeup (NULL);
}



static gboolean
thunar_vfs_job_source_prepare (GSource *source,
                               gint    *timeout)
{
  if (thunar_vfs_job_source_check (source))
    {
      *timeout = 0;
      return TRUE;
    }
  else
    {
      /* need to check for async emissions */
      *timeout = 200;
      return FALSE;
    }
}



static gboolean
thunar_vfs_job_source_check (GSource *source)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  /* check if the job is done or has a pending async or sync emission */
  return (!job->priv->running
       || job->priv->emit_async != NULL
       || job->priv->emit_sync != NULL);
}



static gboolean
thunar_vfs_job_source_dispatch (GSource    *source,
                                GSourceFunc callback,
                                gpointer    user_data)
{
  ThunarVfsJobEmitAsync *emit_async;
  ThunarVfsJobEmitSync  *emit_sync;
  ThunarVfsJob          *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  /* process all pending async signal emissions */
  while (job->priv->emit_async != NULL)
    {
      /* remove the first async emission from the list */
      g_mutex_lock (job_mutex);
      emit_async = job->priv->emit_async;
      job->priv->emit_async = emit_async->next;
      g_mutex_unlock (job_mutex);

      /* emit the asynchronous signal */
      GDK_THREADS_ENTER ();
      g_signal_emitv (emit_async->values, emit_async->signal_id, emit_async->signal_detail, NULL);
      GDK_THREADS_LEAVE ();

      /* release the async signal descriptor */
      _thunar_vfs_g_value_array_free (emit_async->values, emit_async->n_values);
      _thunar_vfs_slice_free (ThunarVfsJobEmitAsync, emit_async);
    }

  /* check if the job is done */
  if (!job->priv->running)
    {
      /* emit the "finished" signal */
      GDK_THREADS_ENTER ();
      g_signal_emit (G_OBJECT (job), job_signals[FINISHED], 0);
      GDK_THREADS_LEAVE ();

      /* drop the source */
      return FALSE;
    }

  /* check if the job has a pending synchronous emission */
  if (G_LIKELY (job->priv->emit_sync != NULL))
    {
      /* determine and reset the emission details */
      g_mutex_lock (job_mutex);
      emit_sync = job->priv->emit_sync;
      job->priv->emit_sync = NULL;
      g_mutex_unlock (job_mutex);

      /* emit the signal */
      GDK_THREADS_ENTER ();
      g_signal_emit_valist (job, emit_sync->signal_id, emit_sync->signal_detail, emit_sync->var_args);
      GDK_THREADS_LEAVE ();

      /* tell the other thread, that we're done */
      g_mutex_lock (job_mutex);
      emit_sync->pending = FALSE;
      g_cond_broadcast (job_cond);
      g_mutex_unlock (job_mutex);
    }

  /* keep the source alive */
  return TRUE;
}



static void
thunar_vfs_job_source_finalize (GSource *source)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB_SOURCE (source)->job;

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* decrement the number of running jobs */
  jobs_running -= 1;

  /* release the job reference */
  GDK_THREADS_ENTER ();
  g_object_unref (G_OBJECT (job));
  GDK_THREADS_LEAVE ();
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
  GSource *source;

  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (!job->priv->running, NULL);
  g_return_val_if_fail (job_pool != NULL, NULL);

  /* allocate and initialize the watch source */
  source = g_source_new (&thunar_vfs_job_source_funcs, sizeof (ThunarVfsJobSource));
  g_source_set_priority (source, G_PRIORITY_HIGH);

  /* connect the source to the job */
  THUNAR_VFS_JOB_SOURCE (source)->job = g_object_ref (G_OBJECT (job));

  /* increment the number of running jobs */
  jobs_running += 1;

  /* mark the job as running */
  job->priv->running = TRUE;

  /* schedule a thread to handle the job */
  g_thread_pool_push (job_pool, job, NULL);

  /* attach the source to the main context */
  g_source_attach (source, NULL);

  /* the main context now keeps the reference */
  g_source_unref (source);

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
  job->cancelled = TRUE;
}



/**
 * _thunar_vfs_job_emit_valist:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on @job.
 * @signal_detail : the detail.
 * @var_args      : a list of paramters to be passed to the signal,
 *                  folled by a location for the return value. If
 *                  the return type of the signals is %G_TYPE_NONE,
 *                  the return value location can be omitted.
 *
 * Emits the signal identified by @signal_id on @job in
 * the context of the main thread. This method doesn't
 * return until the signal was emitted.
 **/
void
_thunar_vfs_job_emit_valist (ThunarVfsJob *job,
                             guint         signal_id,
                             GQuark        signal_detail,
                             va_list       var_args)
{
  ThunarVfsJobEmitSync emit_sync;

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_JOB (job));
  _thunar_vfs_return_if_fail (job->priv->emit_sync == NULL);
  _thunar_vfs_return_if_fail (job->priv->running);

  /* prepare the synchronous emission */
  emit_sync.signal_id = signal_id;
  emit_sync.signal_detail = signal_detail;
  emit_sync.pending = TRUE;

  /* copy the variable argument list (portable) */
  G_VA_COPY (emit_sync.var_args, var_args);

  /* emit the signal using our source */
  g_mutex_lock (job_mutex);
  job->priv->emit_sync = &emit_sync;
  g_main_context_wakeup (NULL);
  while (G_UNLIKELY (emit_sync.pending))
    g_cond_wait (job_cond, job_mutex);
  g_mutex_unlock (job_mutex);
}



/**
 * _thunar_vfs_job_emit:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on qjob.
 * @signal_detail : the signal detail.
 * @...           : a list of parameters to be passed to the signal.
 *
 * Convenience wrapper for thunar_vfs_job_emit_valist().
 **/
void
_thunar_vfs_job_emit (ThunarVfsJob *job,
                      guint         signal_id,
                      GQuark        signal_detail,
                      ...)
{
  va_list var_args;

  va_start (var_args, signal_detail);
  _thunar_vfs_job_emit_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



/**
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on @job.
 * @signal_detail : the detail.
 * @var_args      : a list of paramters to be passed to the signal.
 *
 * Emits the signal identified by @signal_id on @job in
 * the context of the main thread. This method returns
 * immediately, and does not wait for the signal to be
 * emitted.
 **/
void
_thunar_vfs_job_notify_valist (ThunarVfsJob *job,
                               guint         signal_id,
                               GQuark        signal_detail,
                               va_list       var_args)
{
  ThunarVfsJobEmitAsync *emit_async;
  GSignalQuery           signal_query;
  gchar                 *error_message;
  guint                  n;

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_JOB (job));
  _thunar_vfs_return_if_fail (job->priv->running);

  /* acquire the job subsystem lock */
  g_mutex_lock (job_mutex);

  /* check if we already have an async emission for the signal */
  for (emit_async = job->priv->emit_async; emit_async != NULL; emit_async = emit_async->next)
    if (emit_async->signal_id == signal_id && emit_async->signal_detail == signal_detail)
      break;

  /* override an existing emission */
  if (G_UNLIKELY (emit_async != NULL))
    {
      /* release the previous values */
      _thunar_vfs_g_value_array_free (emit_async->values, emit_async->n_values);
    }
  else
    {
      /* allocate a new asynchronous emission */
      emit_async = _thunar_vfs_slice_new (ThunarVfsJobEmitAsync);
      emit_async->signal_id = signal_id;
      emit_async->signal_detail = signal_detail;

      /* add to the list */
      emit_async->next = job->priv->emit_async;
      job->priv->emit_async = emit_async;
    }

  /* query the signal information */
  g_signal_query (signal_id, &signal_query);

  /* allocate the parameter values */
  emit_async->n_values = signal_query.n_params + 1;
  emit_async->values = g_new0 (GValue, emit_async->n_values);

  /* initialize the job parameter */
  g_value_init (emit_async->values, THUNAR_VFS_TYPE_JOB);
  g_value_set_object (emit_async->values, job);

  /* add the remaining parameters */
  for (n = 0; n < signal_query.n_params; ++n)
    {
      /* collect the value from the stack */
      g_value_init (emit_async->values + n + 1, signal_query.param_types[n]);
      G_VALUE_COLLECT (emit_async->values + n + 1, var_args, 0, &error_message);

      /* check if an error occurred */
      if (G_UNLIKELY (error_message != NULL))
        {
          g_error ("%s: %s", G_STRLOC, error_message);
          g_free (error_message);
        }
    }

  /* release the job subsystem lock */
  g_mutex_unlock (job_mutex);
}



/**
 * _thunar_vfs_job_emit:
 * @job           : a #ThunarVfsJob.
 * @signal_id     : the id of the signal to emit on qjob.
 * @signal_detail : the signal detail.
 * @...           : a list of parameters to be passed to the signal.
 *
 * Convenience wrapper for thunar_vfs_job_notify_valist().
 **/
void
_thunar_vfs_job_notify (ThunarVfsJob *job,
                        guint         signal_id,
                        GQuark        signal_detail,
                        ...)
{
  va_list var_args;

  va_start (var_args, signal_detail);
  _thunar_vfs_job_notify_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



/**
 * _thunar_vfs_job_ask_valist:
 * @job      : a #ThunarVfsJob.
 * @format   : a printf(3)-style format string.
 * @var_args : argument list for the @format.
 * @question : the question text to append or %NULL.
 * @choices  : the possible choices.
 *
 * Sends the formatted question to the @job owner and awaits
 * its answer.
 *
 * Return value: the response from the @job owner.
 **/
ThunarVfsJobResponse
_thunar_vfs_job_ask_valist (ThunarVfsJob        *job,
                            const gchar         *format,
                            va_list              var_args,
                            const gchar         *question,
                            ThunarVfsJobResponse choices)
{
  ThunarVfsJobResponse response;
  gchar               *message;
  gchar               *text;

  _thunar_vfs_return_val_if_fail (g_utf8_validate (format, -1, NULL), THUNAR_VFS_JOB_RESPONSE_CANCEL);

  /* send the question and wait for the answer */
  text = g_strdup_vprintf (format, var_args);
  message = (question != NULL) ? g_strconcat (text, ".\n\n", question, NULL) : g_strconcat (text, ".", NULL);
  _thunar_vfs_job_emit (job, job_signals[ASK], 0, message, choices, &response);
  g_free (message);
  g_free (text);

  /* cancel the job as per users request */
  if (G_UNLIKELY (response == THUNAR_VFS_JOB_RESPONSE_CANCEL))
    thunar_vfs_job_cancel (job);

  return response;
}



/**
 * _thunar_vfs_job_ask_overwrite:
 * @job    : a #ThunarVfsJob.
 * @format : a printf(3)-style format string.
 * @...    : arguments for the @format.
 *
 * Asks the user whether to overwrite a certain file as described by
 * the specified @format string.
 *
 * The return value may be %THUNAR_VFS_JOB_RESPONSE_CANCEL if the user
 * cancelled the @job, or %THUNAR_VFS_JOB_RESPONSE_YES if the user
 * wants to overwrite or %THUNAR_VFS_JOB_RESPONSE_NO if the user wants
 * to keep the file, which does not necessarily mean to cancel the
 * @job (whether the @job will be cancelled depends on the semantics
 * of the @job).
 *
 * Return value: the response of the user.
 **/
ThunarVfsJobResponse
_thunar_vfs_job_ask_overwrite (ThunarVfsJob *job,
                               const gchar  *format,
                               ...)
{
  ThunarVfsJobResponse response;
  va_list              var_args;

  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), THUNAR_VFS_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (job->cancelled))
    return THUNAR_VFS_JOB_RESPONSE_CANCEL;

  /* check if the user said "Overwrite All" earlier */
  if (G_UNLIKELY (job->priv->ask_overwrite_all))
    return THUNAR_VFS_JOB_RESPONSE_YES;
  
  /* check if the user said "Overwrite None" earlier */
  if (G_UNLIKELY (job->priv->ask_overwrite_none))
    return THUNAR_VFS_JOB_RESPONSE_NO;

  /* ask the user using the provided format string */
  va_start (var_args, format);
  response = _thunar_vfs_job_ask_valist (job, format, var_args,
                                         _("Do you want to overwrite it?"),
                                         THUNAR_VFS_JOB_RESPONSE_YES
                                         | THUNAR_VFS_JOB_RESPONSE_YES_ALL
                                         | THUNAR_VFS_JOB_RESPONSE_NO
                                         | THUNAR_VFS_JOB_RESPONSE_NO_ALL
                                         | THUNAR_VFS_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  /* translate the response */
  switch (response)
    {
    case THUNAR_VFS_JOB_RESPONSE_YES_ALL:
      job->priv->ask_overwrite_all = TRUE;
      response = THUNAR_VFS_JOB_RESPONSE_YES;
      break;

    case THUNAR_VFS_JOB_RESPONSE_NO_ALL:
      job->priv->ask_overwrite_none = TRUE;
      response = THUNAR_VFS_JOB_RESPONSE_NO;
      break;

    default:
      break;
    }

  return response;
}



/**
 * _thunar_vfs_job_ask_skip:
 * @job    : a #ThunarVfsJob.
 * @format : a printf(3)-style format string.
 * @...    : arguments for the @format.
 *
 * Asks the user whether to skip a certain file as described by
 * @format.
 *
 * The return value may be %TRUE to perform the skip, or %FALSE,
 * which means to cancel the @job.
 *
 * Return value: %TRUE to skip or %FALSE to cancel the @job.
 **/
gboolean
_thunar_vfs_job_ask_skip (ThunarVfsJob *job,
                          const gchar  *format,
                          ...)
{
  ThunarVfsJobResponse response;
  va_list              var_args;

  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (job->cancelled))
    return FALSE;

  /* check if the user said "Skip All" earlier */
  if (G_UNLIKELY (job->priv->ask_skip_all))
    return TRUE;

  /* ask the user using the provided format string */
  va_start (var_args, format);
  response = _thunar_vfs_job_ask_valist (job, format, var_args,
                                         _("Do you want to skip it?"),
                                         THUNAR_VFS_JOB_RESPONSE_YES
                                         | THUNAR_VFS_JOB_RESPONSE_YES_ALL
                                         | THUNAR_VFS_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  /* evaluate the response */
  switch (response)
    {
    case THUNAR_VFS_JOB_RESPONSE_YES_ALL:
      job->priv->ask_skip_all = TRUE;
      /* FALL-THROUGH */

    case THUNAR_VFS_JOB_RESPONSE_YES:
      return TRUE;

    case THUNAR_VFS_JOB_RESPONSE_CANCEL:
      return FALSE;

    default:
      _thunar_vfs_assert_not_reached ();
      return FALSE;
    }
}



/**
 * _thunar_vfs_job_error:
 * @job   : a #ThunarVfsJob.
 * @error : a #GError describing the error cause.
 *
 * Emits the ::error signal on @job with the given @error. Whether
 * or not the @job continues after emitting the error depends on
 * the particular implementation of @job, but most jobs will
 * terminate instantly after emitting an error.
 **/
void
_thunar_vfs_job_error (ThunarVfsJob *job,
                       GError       *error)
{
  _thunar_vfs_return_if_fail (error != NULL && g_utf8_validate (error->message, -1, NULL));
  _thunar_vfs_job_emit (job, job_signals[ERROR], 0, error);
}



/**
 * _thunar_vfs_job_info_message:
 * @job     : a #ThunarVfsJob.
 * @message : the info message.
 *
 * Emits the ::info-message signal on @job with the info @message.
 **/
void
_thunar_vfs_job_info_message (ThunarVfsJob *job,
                              const gchar  *message)
{
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_JOB (job));
  _thunar_vfs_return_if_fail (g_utf8_validate (message, -1, NULL));

  _thunar_vfs_job_notify (job, job_signals[INFO_MESSAGE], 0, message);
}



/**
 * _thunar_vfs_job_infos_ready:
 * @job       : a #ThunarVfsJob.
 * @info_list : a #GList of #ThunarVfsInfo<!---->s.
 *
 * Emits the ::infos-ready signal on @job with the given @info_list.
 *
 * Return value: %TRUE if any of the signal handlers took over
 *               ownership of the @info_list, %FALSE if the caller
 *               is responsible to free the @info_list.
 **/
gboolean
_thunar_vfs_job_infos_ready (ThunarVfsJob *job,
                             GList        *info_list)
{
  gboolean handled = FALSE;
  _thunar_vfs_return_val_if_fail (info_list != NULL, FALSE);
  _thunar_vfs_job_emit (job, job_signals[INFOS_READY], 0, info_list, &handled);
  return handled;
}



/**
 * thunar_vfs_job_new_files:
 * @job       : a #ThunarVfsJob.
 * @path_list : the #ThunarVfsPath<!---->s that were created by the @job.
 *
 * Emits the ::new-files signal on @job with the @path_list.
 **/
void
_thunar_vfs_job_new_files (ThunarVfsJob *job,
                           const GList  *path_list)
{
  /* check if any paths were supplied */
  if (G_LIKELY (path_list != NULL))
    {
      /* wait for the monitor to process all pending events */
      thunar_vfs_monitor_wait (_thunar_vfs_monitor);

      /* emit the new-files signal */
      _thunar_vfs_job_emit (job, job_signals[NEW_FILES], 0, path_list);
    }
}



/**
 * _thunar_vfs_job_percent:
 * @job     : a #ThunarVfsJob.
 * @percent : the percentage of completion (in the range of 0.0 to 100.0).
 *
 * Emits the ::percent signal on @job with @percent.
 **/
void
_thunar_vfs_job_percent (ThunarVfsJob *job,
                         gdouble       percent)
{
  /* clamp the value to the range 0.0 to 100.0 */
  if (G_UNLIKELY (percent < 0.0))
    percent = 0.0;
  else if (G_UNLIKELY (percent > 100.0))
    percent = 100.0;

  /* notify about the new percentage */
  _thunar_vfs_job_notify (job, job_signals[PERCENT], 0, percent);
}



/**
 * _thunar_vfs_job_total_paths:
 * @job         : a #ThunarVfsJob.
 * @total_paths : the total #GList of #ThunarVfsPath<!---->s to be processed
 *                by the @job.
 *
 * Use this method for jobs that work based on #ThunarVfsPath<!---->s
 * to initialize the total paths and item count.
 *
 * Afterwards call _thunar_vfs_job_process_path() for every path that
 * you begin to process.
 *
 * The @total_paths list must be valid for all invocations of the
 * _thunar_vfs_job_process_path() method, otherwise the behaviour
 * will be undefined.
 **/
void
_thunar_vfs_job_total_paths (ThunarVfsJob *job,
                             GList        *total_paths)
{
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_JOB (job));
  job->priv->total_paths = total_paths;
}




/**
 * _thunar_vfs_job_process_path:
 * @job            : a #ThunarVfsJob.
 * @path_list_item : the next #GList item in the list of
 *                   #ThunarVfsPath<!---->s previously passed
 *                   to _thunar_vfs_job_total_paths().
 *
 * Use this method after setting the path list using
 * _thunar_vfs_job_total_paths() to update both the
 * info message and the percent for the @job.
 *
 * @path_list_item must be a list item in the path list previously
 * set via _thunar_vfs_job_total_paths().
 **/
void
_thunar_vfs_job_process_path (ThunarVfsJob *job,
                              GList        *path_list_item)
{
  GList *lp;
  gchar *display_name;
  guint  n_processed;
  guint  n_total;

  _thunar_vfs_return_if_fail (g_list_position (job->priv->total_paths, path_list_item) >= 0);
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* update the info message to the path name */
  display_name = _thunar_vfs_path_dup_display_name (path_list_item->data);
  _thunar_vfs_job_info_message (job, display_name);
  g_free (display_name);

  /* verify that we have total_paths set */
  if (G_LIKELY (job->priv->total_paths != NULL))
    {
      /* determine the number of processed paths */
      for (lp = job->priv->total_paths, n_processed = 0; lp != path_list_item; lp = lp->next, ++n_processed)
        ;

      /* emit only if n_processed is a multiple of 8 */
      if ((n_processed % 8) == 0)
        {
          /* determine the total number of paths */
          for (n_total = n_processed; lp != NULL; lp = lp->next, ++n_total)
            ;

          /* update the progress status */
          _thunar_vfs_job_percent (job, (n_processed * 100.0) / n_total);
        }
    }
}



/**
 * _thunar_vfs_job_init:
 *
 * Initializes the jobs module of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_job_init (void)
{
  _thunar_vfs_return_if_fail (job_pool == NULL);

  /* allocate the synchronization entities */
  job_cond = g_cond_new ();
  job_mutex = g_mutex_new ();

  /* allocate the shared thread pool */
  job_pool = g_thread_pool_new (thunar_vfs_job_execute, NULL, 8, FALSE, NULL);
}



/**
 * _thunar_vfs_job_shutdown:
 *
 * Shuts down the jobs module of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_job_shutdown (void)
{
  _thunar_vfs_return_if_fail (job_pool != NULL);

  /* wait for all jobs to terminate */
  while (G_UNLIKELY (jobs_running > 0))
    g_main_context_iteration (NULL, TRUE);

  /* release the thread pool */
  g_thread_pool_free (job_pool, FALSE, TRUE);
  job_pool = NULL;

  /* release the synchronization entities */
  g_mutex_free (job_mutex);
  g_cond_free (job_cond);
}



#define __THUNAR_VFS_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
