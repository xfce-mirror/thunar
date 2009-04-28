/* $Id$ */
/*-
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>



#define THUNAR_JOB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_JOB, ThunarJobPrivate))



/* Signal identifiers */
enum
{
  ASK,
  ASK_REPLACE,
  ERROR,
  FINISHED,
  INFO_MESSAGE,
  FILES_READY,
  NEW_FILES,
  PERCENT,
  LAST_SIGNAL,
};



typedef struct _ThunarJobSyncSignalData  ThunarJobSyncSignalData;



static void              thunar_job_class_init          (ThunarJobClass     *klass);
static void              thunar_job_init                (ThunarJob          *job);
static void              thunar_job_finalize            (GObject            *object);
static ThunarJobResponse thunar_job_real_ask            (ThunarJob          *job,
                                                         const gchar        *message,
                                                         ThunarJobResponse   choices);
static ThunarJobResponse thunar_job_real_ask_replace    (ThunarJob          *job,
                                                         ThunarFile         *source_file,
                                                         ThunarFile         *target_file);
static void              _thunar_job_error              (ThunarJob          *job,
                                                         GError             *error);
static void              _thunar_job_finished           (ThunarJob          *job);
static gboolean          _thunar_job_finish             (ThunarJob          *job,
                                                         GSimpleAsyncResult *result,
                                                         GError            **error);
static void              _thunar_job_async_ready        (GObject            *object,
                                                         GAsyncResult       *result);
static gboolean          _thunar_job_scheduler_job_func (GIOSchedulerJob    *scheduler_job,
                                                         GCancellable       *cancellable,
                                                         gpointer            user_data);



struct _ThunarJobPrivate
{
  ThunarJobResponse earlier_ask_create_response;
  ThunarJobResponse earlier_ask_overwrite_response;
  ThunarJobResponse earlier_ask_skip_response;
  GIOSchedulerJob  *scheduler_job;
  GCancellable     *cancellable;
  GList            *total_files;
  guint             running : 1;
};

struct _ThunarJobSyncSignalData
{
  gpointer instance;
  GQuark   signal_detail;
  guint    signal_id;
  va_list  var_args;
};



static GObjectClass *thunar_job_parent_class;
static guint         job_signals[LAST_SIGNAL];



GType
thunar_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            "ThunarJob",
                                            sizeof (ThunarJobClass),
                                            (GClassInitFunc) thunar_job_class_init,
                                            sizeof (ThunarJob),
                                            (GInstanceInitFunc) thunar_job_init,
                                            G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static gboolean
_thunar_job_ask_accumulator (GSignalInvocationHint *ihint,
                             GValue                *return_accu,
                             const GValue          *handler_return,
                             gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  return FALSE;
}



static void
thunar_job_class_init (ThunarJobClass *klass)
{
  GObjectClass *gobject_class;

  /* add our private data for this class */
  g_type_class_add_private (klass, sizeof (ThunarJobPrivate));

  /* determine the parent class */
  thunar_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_job_finalize;

  klass->execute = NULL;
  klass->ask = thunar_job_real_ask;
  klass->ask_replace = thunar_job_real_ask_replace;

  /**
   * ThunarJob::ask:
   * @job     : a #ThunarJob.
   * @message : question to display to the user.
   * @choices : a combination of #ThunarJobResponse<!---->s.
   *
   * The @message is garantied to contain valid UTF-8.
   *
   * Return value: the selected choice.
   **/
  job_signals[ASK] =
    g_signal_new (I_("ask"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarJobClass, ask),
                  _thunar_job_ask_accumulator, NULL,
                  _thunar_marshal_FLAGS__STRING_FLAGS,
                  THUNAR_TYPE_JOB_RESPONSE,
                  2, G_TYPE_STRING,
                  THUNAR_TYPE_JOB_RESPONSE);

  /**
   * ThunarJob::ask-replace:
   * @job      : a #ThunarJob.
   * @src_file : the #ThunarFile of the source file.
   * @dst_file : the #ThunarFile of the destination file, that
   *             may be replaced with the source file.
   *
   * Emitted to ask the user whether the destination file should
   * be replaced by the source file.
   *
   * Return value: the selected choice.
   **/
  job_signals[ASK_REPLACE] =
    g_signal_new (I_("ask-replace"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarJobClass, ask_replace),
                  _thunar_job_ask_accumulator, NULL,
                  _thunar_marshal_FLAGS__OBJECT_OBJECT,
                  THUNAR_TYPE_JOB_RESPONSE, 
                  2, THUNAR_TYPE_FILE, THUNAR_TYPE_FILE);

  /**
   * ThunarJob::error:
   * @job   : a #ThunarJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while executing the @job.
   **/
  job_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarJob::files-ready:
   * @job       : a #ThunarJob.
   * @file_list : a list of #ThunarFile<!---->s.
   *
   * This signal is used by #ThunarJob<!---->s returned by
   * the thunar_io_jobs_list_directory() function whenever 
   * there's a bunch of #ThunarFile<!---->s ready. This signal 
   * is garantied to be never emitted with an @file_list 
   * parameter of %NULL.
   *
   * To allow some further optimizations on the handler-side,
   * the handler is allowed to take over ownership of the
   * @file_list, i.e. it can reuse the @infos list and just replace
   * the data elements with it's own objects based on the
   * #ThunarFile<!---->s contained within the @file_list (and
   * of course properly unreffing the previously contained infos).
   * If a handler takes over ownership of @file_list it must return
   * %TRUE here, and no further handlers will be run. Else, if
   * the handler doesn't want to take over ownership of @infos,
   * it must return %FALSE, and other handlers will be run. Use
   * this feature with care, and only if you can be sure that
   * you are the only handler connected to this signal for a
   * given job!
   *
   * Return value: %TRUE if the handler took over ownership of
   *               @file_list, else %FALSE.
   **/
  job_signals[FILES_READY] =
    g_signal_new (I_("files-ready"),
                  G_TYPE_FROM_CLASS (klass), G_SIGNAL_NO_HOOKS,
                  0, g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__POINTER,
                  G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);

  /**
   * ThunarJob::finished:
   * @job : a #ThunarJob.
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
   * ThunarJob::info-message:
   * @job     : a #ThunarJob.
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
   * ThunarJob::new-files:
   * @job       : a #ThunarJob.
   * @file_list : a list of #GFile<!---->s that were created by @job.
   *
   * This signal is emitted by the @job right before the @job is terminated
   * and informs the application about the list of created files in @file_list.
   * @file_list contains only the toplevel file items, that were specified by
   * the application on creation of the @job.
   **/
  job_signals[NEW_FILES] =
    g_signal_new (I_("new-files"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarJob::percent:
   * @job     : a #ThunarJob.
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
thunar_job_init (ThunarJob *job)
{
  job->priv = THUNAR_JOB_GET_PRIVATE (job);
  job->priv->cancellable = g_cancellable_new ();
  job->priv->earlier_ask_create_response = 0;
  job->priv->earlier_ask_overwrite_response = 0;
  job->priv->earlier_ask_skip_response = 0;
  job->priv->running = FALSE;
  job->priv->scheduler_job = NULL;
}



static void
thunar_job_finalize (GObject *object)
{
  ThunarJob *job = THUNAR_JOB (object);

  thunar_job_cancel (job);
  g_object_unref (job->priv->cancellable);

  (*G_OBJECT_CLASS (thunar_job_parent_class)->finalize) (object);
}



static ThunarJobResponse 
thunar_job_real_ask (ThunarJob        *job,
                     const gchar      *message,
                     ThunarJobResponse choices)
{
  ThunarJobResponse response;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  g_signal_emit (job, job_signals[ASK], 0, message, choices, &response);
  return response;
}



static ThunarJobResponse 
thunar_job_real_ask_replace (ThunarJob  *job,
                             ThunarFile *source_file,
                             ThunarFile *target_file)
{
  ThunarJobResponse response;
  gchar            *message;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (source_file), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (target_file), THUNAR_JOB_RESPONSE_CANCEL);

  message = g_strdup_printf (_("The file \"%s\" already exists. Would you like to replace it?\n\n"
                               "If you replace an existing file, its contents will be overwritten."),
                             thunar_file_get_display_name (source_file));

  g_signal_emit (job, job_signals[ASK], 0, message,
                 THUNAR_JOB_RESPONSE_YES
                 | THUNAR_JOB_RESPONSE_YES_ALL
                 | THUNAR_JOB_RESPONSE_NO
                 | THUNAR_JOB_RESPONSE_NO_ALL
                 | THUNAR_JOB_RESPONSE_CANCEL,
                 &response);

  /* clean up */
  g_free (message);

  return response;
}



static gboolean
_thunar_job_finish (ThunarJob          *job,
                    GSimpleAsyncResult *result,
                    GError            **error)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return !g_simple_async_result_propagate_error (result, error);
}



static void
_thunar_job_async_ready (GObject      *object,
                         GAsyncResult *result)
{
  ThunarJob *job = THUNAR_JOB (object);
  GError    *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result));

  if (!_thunar_job_finish (job, G_SIMPLE_ASYNC_RESULT (result), &error))
    {
      g_assert (error != NULL);

      /* don't treat cancellation as an error for now */
      if (error->code != G_IO_ERROR_CANCELLED)
        _thunar_job_error (job, error);

      g_error_free (error);
    }

  _thunar_job_finished (job);
}



static gboolean
_thunar_job_scheduler_job_func (GIOSchedulerJob *scheduler_job,
                                GCancellable    *cancellable,
                                gpointer         user_data)
{
  GSimpleAsyncResult *result = user_data;
  ThunarJob          *job;
  GError             *error = NULL;
  gboolean            success;

  job = g_simple_async_result_get_op_res_gpointer (result);
  job->priv->scheduler_job = scheduler_job;

  success = (*THUNAR_JOB_GET_CLASS (job)->execute) (job, &error);

  /* TODO why was this necessary again? */
  g_io_scheduler_job_send_to_mainloop (scheduler_job, (GSourceFunc) gtk_false, 
                                       NULL, NULL);

  if (!success)
    {
      g_simple_async_result_set_from_error (result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete_in_idle (result);

  return FALSE;
}



/**
 * thunar_job_launch:
 * @job : a #ThunarJob.
 *
 * This functions schedules @job to be run as soon
 * as possible, in a separate thread.
 *
 * Return value: a pointer to @job.
 **/
ThunarJob*
thunar_job_launch (ThunarJob *job)
{
  GSimpleAsyncResult *result;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), NULL);
  _thunar_return_val_if_fail (!job->priv->running, NULL);
  _thunar_return_val_if_fail (THUNAR_JOB_GET_CLASS (job)->execute != NULL, NULL);

  /* mark the job as running */
  job->priv->running = TRUE;

  result = g_simple_async_result_new (G_OBJECT (job),
                                      (GAsyncReadyCallback) _thunar_job_async_ready,
                                      NULL,
                                      thunar_job_launch);

  g_simple_async_result_set_op_res_gpointer (result,
                                             g_object_ref (job),
                                             (GDestroyNotify) g_object_unref);

  g_io_scheduler_push_job (_thunar_job_scheduler_job_func,
                           result,
                           (GDestroyNotify) g_object_unref,
                           G_PRIORITY_HIGH,
                           job->priv->cancellable);

  return job;
}



/**
 * thunar_job_cancel:
 * @job : a #ThunarJob.
 *
 * Attempts to cancel the operation currently
 * performed by @job. Even after the cancellation
 * of @job, it may still emit signals, so you
 * must take care of disconnecting all handlers
 * appropriately if you cannot handle signals
 * after cancellation.
 **/
void
thunar_job_cancel (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  g_cancellable_cancel (job->priv->cancellable);
}



/**
 * thunar_job_is_cancelled:
 * @job : a #ThunarJob.
 *
 * Checks whether @job was previously cancelled
 * by a call to thunar_job_cancel().
 *
 * Return value: %TRUE if @job is cancelled.
 **/
gboolean
thunar_job_is_cancelled (const ThunarJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  return g_cancellable_is_cancelled (job->priv->cancellable);
}



GCancellable *
thunar_job_get_cancellable (const ThunarJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), NULL);
  return job->priv->cancellable;
}



gboolean
thunar_job_set_error_if_cancelled (ThunarJob  *job,
                                   GError    **error)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  return g_cancellable_set_error_if_cancelled (job->priv->cancellable, error);
}



static gboolean
_thunar_job_emit_valist_in_mainloop (gpointer user_data)
{
  ThunarJobSyncSignalData *data = user_data;

  g_signal_emit_valist (data->instance, 
                        data->signal_id, 
                        data->signal_detail, 
                        data->var_args);

  return FALSE;
}



void
thunar_job_emit_valist (ThunarJob *job,
                        guint      signal_id,
                        GQuark     signal_detail,
                        va_list    var_args)
{
  ThunarJobSyncSignalData data;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (job->priv->scheduler_job != NULL);

  data.instance = job;
  data.signal_id = signal_id;
  data.signal_detail = signal_detail;
  
  /* copy the variable argument list */
  G_VA_COPY (data.var_args, var_args);

  /* emit the signal in the main loop */
  g_io_scheduler_job_send_to_mainloop (job->priv->scheduler_job,
                                       _thunar_job_emit_valist_in_mainloop,
                                       &data,
                                       NULL);
}



void
thunar_job_emit (ThunarJob *job,
                 guint      signal_id,
                 GQuark     signal_detail,
                 ...)
{
  va_list var_args;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  va_start (var_args, signal_detail);
  thunar_job_emit_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



static ThunarJobResponse
_thunar_job_ask_valist (ThunarJob        *job,
                        const gchar      *format,
                        va_list           var_args,
                        const gchar      *question,
                        ThunarJobResponse choices)
{
  ThunarJobResponse response;
  gchar            *text;
  gchar            *message;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (format, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
  
  /* generate the dialog message */
  text = g_strdup_vprintf (format, var_args);
  message = (question != NULL) 
            ? g_strconcat (text, ".\n\n", question, NULL) 
            : g_strconcat (text, ".", NULL);
  g_free (text);  

  /* send the question and wait for the answer */
  thunar_job_emit (job, job_signals[ASK], 0, message, choices, &response);
  g_free (message);

  /* cancel the job as per users request */
  if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_CANCEL))
    thunar_job_cancel (job);

  return response;
}



ThunarJobResponse
thunar_job_ask_overwrite (ThunarJob   *job,
                          const gchar *format,
                          ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (thunar_job_is_cancelled (job)))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Overwrite All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* check if the user said "Overwrite None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_NO_ALL))
    return THUNAR_JOB_RESPONSE_NO;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to overwrite it?"),
                                     THUNAR_JOB_RESPONSE_YES 
                                     | THUNAR_JOB_RESPONSE_YES_ALL
                                     | THUNAR_JOB_RESPONSE_NO
                                     | THUNAR_JOB_RESPONSE_NO_ALL
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  /* remember response for later */
  job->priv->earlier_ask_overwrite_response = response;

  /* translate response */
  switch (response)
    {
    case THUNAR_JOB_RESPONSE_YES_ALL:
      response = THUNAR_JOB_RESPONSE_YES;
      break;

    case THUNAR_JOB_RESPONSE_NO_ALL:
      response = THUNAR_JOB_RESPONSE_NO;
      break;

    default:
      break;
    }

  return response;
}



ThunarJobResponse
thunar_job_ask_create (ThunarJob   *job,
                       const gchar *format,
                       ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);

  if (G_UNLIKELY (thunar_job_is_cancelled (job)))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Create All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_create_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* check if the user said "Create None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_create_response == THUNAR_JOB_RESPONSE_NO_ALL))
    return THUNAR_JOB_RESPONSE_NO;

  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to create it?"),
                                     THUNAR_JOB_RESPONSE_YES 
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  job->priv->earlier_ask_create_response = response;

  /* translate the response */
  if (response == THUNAR_JOB_RESPONSE_YES_ALL)
    response = THUNAR_JOB_RESPONSE_YES;
  else if (response == THUNAR_JOB_RESPONSE_NO_ALL)
    response = THUNAR_JOB_RESPONSE_NO;
  else if (response == THUNAR_JOB_RESPONSE_CANCEL)
    thunar_job_cancel (job);

  return response;
}



ThunarJobResponse 
thunar_job_ask_replace (ThunarJob *job,
                        GFile     *source_path,
                        GFile     *target_path,
                        GError   **error)
{
  ThunarJobResponse response;
  ThunarFile       *source_file;
  ThunarFile       *target_file;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (G_IS_FILE (source_path), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (G_IS_FILE (target_path), THUNAR_JOB_RESPONSE_CANCEL);

  if (G_UNLIKELY (thunar_job_set_error_if_cancelled (job, error)))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Overwrite All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* check if the user said "Overwrite None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_NO_ALL))
    return THUNAR_JOB_RESPONSE_NO;

  source_file = thunar_file_get (source_path, error);

  if (G_UNLIKELY (source_file == NULL))
    return THUNAR_JOB_RESPONSE_NO;

  target_file = thunar_file_get (target_path, error);

  if (G_UNLIKELY (target_file == NULL))
    {
      g_object_unref (source_file);
      return THUNAR_JOB_RESPONSE_NO;
    }

  thunar_job_emit (job, job_signals[ASK_REPLACE], 0, source_file, target_file, &response);

  g_object_unref (source_file);
  g_object_unref (target_file);

  /* remember the response for later */
  job->priv->earlier_ask_overwrite_response = response;

  /* translate the response */
  if (response == THUNAR_JOB_RESPONSE_YES_ALL)
    response = THUNAR_JOB_RESPONSE_YES;
  else if (response == THUNAR_JOB_RESPONSE_NO_ALL)
    response = THUNAR_JOB_RESPONSE_NO;
  else if (response == THUNAR_JOB_RESPONSE_CANCEL)
    thunar_job_cancel (job);

  return response;
}



ThunarJobResponse 
thunar_job_ask_skip (ThunarJob   *job,
                     const gchar *format,
                     ...)
{
  ThunarJobResponse response;
  va_list           var_args;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (thunar_job_is_cancelled (job)))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Skip All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_skip_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to skip it?"),
                                     THUNAR_JOB_RESPONSE_YES 
                                     | THUNAR_JOB_RESPONSE_YES_ALL
                                     | THUNAR_JOB_RESPONSE_CANCEL
                                     | THUNAR_JOB_RESPONSE_RETRY);
  va_end (var_args);

  /* remember the response */
  job->priv->earlier_ask_skip_response = response;

  /* translate the response */
  switch (response)
    {
    case THUNAR_JOB_RESPONSE_YES_ALL:
      response = THUNAR_JOB_RESPONSE_YES;
      break;

    case THUNAR_JOB_RESPONSE_YES:
    case THUNAR_JOB_RESPONSE_CANCEL:
    case THUNAR_JOB_RESPONSE_RETRY:
      break;

    default:
      _thunar_assert_not_reached ();
    }

  return response;
}



gboolean
thunar_job_files_ready (ThunarJob *job,
                        GList     *file_list)
{
  gboolean handled = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  thunar_job_emit (job, job_signals[FILES_READY], 0, file_list, &handled);
  return handled;
}



void
thunar_job_info_message (ThunarJob   *job,
                         const gchar *message)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (message != NULL);

  thunar_job_emit (job, job_signals[INFO_MESSAGE], 0, message);
}



void
thunar_job_new_files (ThunarJob   *job,
                      const GList *file_list)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  /* check if we have any files */
  if (G_LIKELY (file_list != NULL))
    {
      /* emit the "new-files" signal */
      thunar_job_emit (job, job_signals[NEW_FILES], 0, file_list);
    }
}



void
thunar_job_percent (ThunarJob *job,
                    gdouble    percent)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  percent = MAX (0.0, MIN (100.0, percent));
  thunar_job_emit (job, job_signals[PERCENT], 0, percent);
}



static void
_thunar_job_error (ThunarJob *job,
                   GError    *error)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (error != NULL);

  g_signal_emit (job, job_signals[ERROR], 0, error);
}



static void
_thunar_job_finished (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  g_signal_emit (job, job_signals[FINISHED], 0);
}



void
thunar_job_set_total_files (ThunarJob *job,
                            GList     *total_files)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (job->priv->total_files == NULL);
  _thunar_return_if_fail (total_files != NULL);

  job->priv->total_files = total_files;
}



void
thunar_job_processing_file (ThunarJob *job,
                            GList     *current_file)
{
  GList *lp;
  gchar *basename;
  gchar *display_name;
  guint  n_processed;
  guint  n_total;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (current_file != NULL);

  basename = g_file_get_basename (current_file->data);
  display_name = g_filename_display_name (basename);
  g_free (basename);

  thunar_job_info_message (job, display_name);
  g_free (display_name);

  /* verify that we have total files set */
  if (G_LIKELY (job->priv->total_files != NULL))
    {
      /* determine the number of files processed so far */
      for (lp = job->priv->total_files, n_processed = 0;
           lp != current_file;
           lp = lp->next);

      /* emit only if n_processed is a multiple of 8 */
      if ((n_processed % 8) == 0)
        {
          /* determine the total_number of files */
          n_total = g_list_length (job->priv->total_files);

          thunar_job_percent (job, (n_processed * 100.0) / n_total);
        }
    }
}
