/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-progress-dialog.h>



enum
{
  PROP_0,
  PROP_JOB,
};



static void              thunar_progress_dialog_dispose      (GObject                    *object);
static void              thunar_progress_dialog_get_property (GObject                    *object,
                                                              guint                       prop_id,
                                                              GValue                     *value,
                                                              GParamSpec                 *pspec);
static void              thunar_progress_dialog_set_property (GObject                    *object,
                                                              guint                       prop_id,
                                                              const GValue               *value,
                                                              GParamSpec                 *pspec);
static void              thunar_progress_dialog_response     (GtkDialog                  *dialog,
                                                              gint                        response);
static ThunarJobResponse thunar_progress_dialog_ask          (ThunarProgressDialog       *dialog,
                                                              const gchar                *message,
                                                              ThunarJobResponse           choices,
                                                              ThunarJob                  *job);
static ThunarJobResponse thunar_progress_dialog_ask_replace  (ThunarProgressDialog       *dialog,
                                                              ThunarFile                 *src_file,
                                                              ThunarFile                 *dst_file,
                                                              ThunarJob                  *job);
static void              thunar_progress_dialog_error        (ThunarProgressDialog       *dialog,
                                                              GError                     *error,
                                                              ExoJob                     *job);
static void              thunar_progress_dialog_finished     (ThunarProgressDialog       *dialog,
                                                              ExoJob                     *job);
static void              thunar_progress_dialog_info_message (ThunarProgressDialog       *dialog,
                                                              const gchar                *message,
                                                              ExoJob                     *job);
static void              thunar_progress_dialog_percent      (ThunarProgressDialog       *dialog,
                                                              gdouble                     percent,
                                                              ExoJob                     *job);



struct _ThunarProgressDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarProgressDialog
{
  GtkDialog  __parent__;

  ThunarJob *job;

  GTimeVal   start_time;
  GTimeVal   last_update_time;

  GtkWidget *progress_bar;
  GtkWidget *progress_label;
};



G_DEFINE_TYPE (ThunarProgressDialog, thunar_progress_dialog, GTK_TYPE_DIALOG)



static void
thunar_progress_dialog_class_init (ThunarProgressDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_progress_dialog_dispose;
  gobject_class->get_property = thunar_progress_dialog_get_property;
  gobject_class->set_property = thunar_progress_dialog_set_property;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_progress_dialog_response;

  /**
   * ThunarProgressDialog:job:
   *
   * The #ThunarJob, whose progress is displayed by this dialog, or 
   * %NULL if no job is set.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_JOB,
                                   g_param_spec_object ("job", "job", "job",
                                                        THUNAR_TYPE_JOB,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_progress_dialog_init (ThunarProgressDialog *dialog)
{
  GtkWidget *table;
  GtkWidget *image;
  GtkWidget *label;

  /* remember the current time as start time */
  g_get_current_time (&dialog->start_time);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, -1);
  gtk_window_set_title (GTK_WINDOW (dialog), "");

  table = g_object_new (GTK_TYPE_TABLE,
                        "border-width", 6,
                        "n-columns", 3,
                        "n-rows", 3,
                        "row-spacing", 6,
                        "column-spacing", 5,
                        NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  image = g_object_new (GTK_TYPE_IMAGE, "icon-size", GTK_ICON_SIZE_BUTTON, NULL);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 1, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);
  gtk_widget_show (image);

  label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big ());
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);
  gtk_widget_show (label);

  dialog->progress_label = g_object_new (GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
  gtk_table_attach (GTK_TABLE (table), dialog->progress_label, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->progress_label);

  dialog->progress_bar = g_object_new (GTK_TYPE_PROGRESS_BAR, "text", " ", NULL);
  gtk_table_attach (GTK_TABLE (table), dialog->progress_bar, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->progress_bar);

  /* connect the window icon name to the action image */
  exo_binding_new (G_OBJECT (dialog), "icon-name",
                   G_OBJECT (image), "icon-name");

  /* connect the window title to the action label */
  exo_binding_new (G_OBJECT (dialog), "title",
                   G_OBJECT (label), "label");
}



static void
thunar_progress_dialog_dispose (GObject *object)
{
  ThunarProgressDialog *dialog = THUNAR_PROGRESS_DIALOG (object);

  /* disconnect from the job (if any) */
  thunar_progress_dialog_set_job (dialog, NULL);

  (*G_OBJECT_CLASS (thunar_progress_dialog_parent_class)->dispose) (object);
}



static void
thunar_progress_dialog_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarProgressDialog *dialog = THUNAR_PROGRESS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_JOB:
      g_value_set_object (value, thunar_progress_dialog_get_job (dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_progress_dialog_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarProgressDialog *dialog = THUNAR_PROGRESS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_JOB:
      thunar_progress_dialog_set_job (dialog, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarJobResponse
thunar_progress_dialog_ask (ThunarProgressDialog *dialog,
                            const gchar          *message,
                            ThunarJobResponse     choices,
                            ThunarJob            *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (message, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (dialog->job == job, THUNAR_JOB_RESPONSE_CANCEL);

  /* be sure to display the progress dialog prior to opening the question dialog */
  gtk_widget_show_now (GTK_WIDGET (dialog));

  /* display the question dialog */
  return thunar_dialogs_show_job_ask (GTK_WINDOW (dialog), message, choices);
}



static ThunarJobResponse
thunar_progress_dialog_ask_replace (ThunarProgressDialog *dialog,
                                    ThunarFile           *src_file,
                                    ThunarFile           *dst_file,
                                    ThunarJob            *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (dialog->job == job, THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (src_file), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (dst_file), THUNAR_JOB_RESPONSE_CANCEL);

  /* be sure to display the progress dialog prior to opening the question dialog */
  gtk_widget_show_now (GTK_WIDGET (dialog));

  /* display the question dialog */
  return thunar_dialogs_show_job_ask_replace (GTK_WINDOW (dialog), src_file, dst_file);
}



static void
thunar_progress_dialog_error (ThunarProgressDialog *dialog,
                              GError               *error,
                              ExoJob               *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (error != NULL && error->message != NULL);
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (dialog->job == THUNAR_JOB (job));

  /* be sure to display the progress dialog prior to opening the error dialog */
  gtk_widget_show_now (GTK_WIDGET (dialog));

  /* display the error message */
  thunar_dialogs_show_job_error (GTK_WINDOW (dialog), error);
}



static void
thunar_progress_dialog_finished (ThunarProgressDialog *dialog,
                                 ExoJob               *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (dialog->job == THUNAR_JOB (job));

  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}



static void
thunar_progress_dialog_info_message (ThunarProgressDialog *dialog,
                                     const gchar          *message,
                                     ExoJob               *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (g_utf8_validate (message, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (dialog->job == THUNAR_JOB (job));

  gtk_label_set_text (GTK_LABEL (dialog->progress_label), message);
}



static inline guint64
time_diff (const GTimeVal *now,
           const GTimeVal *last)
{
  return ((guint64) now->tv_sec - last->tv_sec) * G_USEC_PER_SEC
       + ((guint64) last->tv_usec - last->tv_usec);
}



static void
thunar_progress_dialog_percent (ThunarProgressDialog *dialog,
                                gdouble               percent,
                                ExoJob               *job)
{
  GTimeVal current_time;
  gulong   remaining_time;
  gulong   elapsed_time;
  gchar    text[512];

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (percent >= 0.0 && percent <= 100.0);
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (dialog->job == THUNAR_JOB (job));

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progress_bar), percent / 100.0);

  /* check if we should update the time display (every 400ms) */
  g_get_current_time (&current_time);
  if (time_diff (&current_time, &dialog->last_update_time) > 400 * 1000)
    {
      /* calculate the remaining time (in seconds) */
      elapsed_time = time_diff (&current_time, &dialog->start_time) / 1000;
      remaining_time = ((100 * elapsed_time) / percent - elapsed_time) / 1000;

      /* setup the time label */
      if (G_LIKELY (remaining_time > 0))
        {
          /* format the time text */
          if (remaining_time > 60 * 60)
            {
              remaining_time = (gulong) (remaining_time / (60 * 60));
              g_snprintf (text, sizeof (text), ngettext ("(%lu hour remaining)", "(%lu hours remaining)", remaining_time), remaining_time);
            }
          else if (remaining_time > 60)
            {
              remaining_time = (gulong) (remaining_time / 60);
              g_snprintf (text, sizeof (text), ngettext ("(%lu minute remaining)", "(%lu minutes remaining)", remaining_time), remaining_time);
            }
          else
            {
              remaining_time = remaining_time;
              g_snprintf (text, sizeof (text), ngettext ("(%lu second remaining)", "(%lu seconds remaining)", remaining_time), remaining_time);
            }

          /* apply the time text */
          gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progress_bar), text);
        }
      else
        {
          /* display an empty label */
          gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progress_bar), " ");
        }

      /* remember the current time as last update time */
      dialog->last_update_time = current_time;
    }
}



static void
thunar_progress_dialog_response (GtkDialog *dialog,
                                 gint       response)
{
  /* cancel the job appropriately */
  switch (response)
    {
    case GTK_RESPONSE_NONE:
    case GTK_RESPONSE_REJECT:
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_NO:
      if (G_LIKELY (THUNAR_PROGRESS_DIALOG (dialog)->job != NULL))
        exo_job_cancel (EXO_JOB (THUNAR_PROGRESS_DIALOG (dialog)->job));
      break;
    }

  if (GTK_DIALOG_CLASS (thunar_progress_dialog_parent_class)->response != NULL)
    (*GTK_DIALOG_CLASS (thunar_progress_dialog_parent_class)->response) (dialog, response);
}



/**
 * thunar_progress_dialog_new:
 *
 * Allocates a new #ThunarProgressDialog.
 *
 * Return value: the newly allocated #ThunarProgressDialog.
 **/
GtkWidget*
thunar_progress_dialog_new (void)
{
  return thunar_progress_dialog_new_with_job (NULL);
}



/**
 * thunar_progress_dialog_new_with_job:
 * @job : a #ThunarJob or %NULL.
 *
 * Allocates a new #ThunarProgressDialog and associates it with the @job.
 *
 * Return value: the newly allocated #ThunarProgressDialog.
 **/
GtkWidget*
thunar_progress_dialog_new_with_job (ThunarJob *job)
{
  _thunar_return_val_if_fail (job == NULL || THUNAR_IS_JOB (job), NULL);
  return g_object_new (THUNAR_TYPE_PROGRESS_DIALOG, "job", job, NULL);
}



/**
 * thunar_progress_dialog_get_job:
 * @dialog : a #ThunarProgressDialog.
 *
 * Returns the #ThunarJob associated with @dialog
 * or %NULL if no job is currently associated with @dialog.
 *
 * Return value: the job associated with @dialog or %NULL.
 **/
ThunarJob *
thunar_progress_dialog_get_job (ThunarProgressDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), NULL);
  return dialog->job;
}



/**
 * thunar_progress_dialog_set_job:
 * @dialog : a #ThunarProgressDialog.
 * @job    : a #ThunarJob or %NULL.
 *
 * Associates @job with @dialog.
 **/
void
thunar_progress_dialog_set_job (ThunarProgressDialog *dialog,
                                ThunarJob            *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (job == NULL || THUNAR_IS_JOB (job));

  /* check if we're already on that job */
  if (G_UNLIKELY (dialog->job == job))
    return;

  /* disconnect from the previous job */
  if (G_LIKELY (dialog->job != NULL))
    {
      g_signal_handlers_disconnect_matched (dialog->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, dialog);
      g_object_unref (G_OBJECT (dialog->job));
    }

  /* activate the new job */
  dialog->job = job;

  /* connect to the new job */
  if (G_LIKELY (job != NULL))
    {
      g_object_ref (job);

      g_signal_connect_swapped (job, "ask", G_CALLBACK (thunar_progress_dialog_ask), dialog);
      g_signal_connect_swapped (job, "ask-replace", G_CALLBACK (thunar_progress_dialog_ask_replace), dialog);
      g_signal_connect_swapped (job, "error", G_CALLBACK (thunar_progress_dialog_error), dialog);
      g_signal_connect_swapped (job, "finished", G_CALLBACK (thunar_progress_dialog_finished), dialog);
      g_signal_connect_swapped (job, "info-message", G_CALLBACK (thunar_progress_dialog_info_message), dialog);
      g_signal_connect_swapped (job, "percent", G_CALLBACK (thunar_progress_dialog_percent), dialog);
    }

  g_object_notify (G_OBJECT (dialog), "job");
}

