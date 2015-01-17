/* vi:set et ai sw=2 sts=2 ts=2: */
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

#include <exo/exo.h>

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-transfer-job.h>
#include <thunar/thunar-progress-view.h>



enum
{
  PROP_0,
  PROP_JOB,
  PROP_ICON_NAME,
  PROP_TITLE,
};



static void              thunar_progress_view_finalize     (GObject            *object);
static void              thunar_progress_view_dispose      (GObject            *object);
static void              thunar_progress_view_get_property (GObject            *object,
                                                            guint               prop_id,
                                                            GValue             *value,
                                                            GParamSpec         *pspec);
static void              thunar_progress_view_set_property (GObject            *object,
                                                            guint               prop_id,
                                                            const GValue       *value,
                                                            GParamSpec         *pspec);
static void              thunar_progress_view_cancel_job   (ThunarProgressView *view);
static ThunarJobResponse thunar_progress_view_ask          (ThunarProgressView *view,
                                                            const gchar        *message,
                                                            ThunarJobResponse   choices,
                                                            ThunarJob          *job);
static ThunarJobResponse thunar_progress_view_ask_replace  (ThunarProgressView *view,
                                                            ThunarFile         *src_file,
                                                            ThunarFile         *dst_file,
                                                            ThunarJob          *job);
static void              thunar_progress_view_error        (ThunarProgressView *view,
                                                            GError             *error,
                                                            ExoJob             *job);
static void              thunar_progress_view_finished     (ThunarProgressView *view,
                                                            ExoJob             *job);
static void              thunar_progress_view_info_message (ThunarProgressView *view,
                                                            const gchar        *message,
                                                            ExoJob             *job);
static void              thunar_progress_view_percent      (ThunarProgressView *view,
                                                            gdouble             percent,
                                                            ExoJob             *job);
static ThunarJob        *thunar_progress_view_get_job      (ThunarProgressView *view);
static void              thunar_progress_view_set_job      (ThunarProgressView *view,
                                                            ThunarJob          *job);



struct _ThunarProgressViewClass
{
  GtkVBoxClass __parent__;
};

struct _ThunarProgressView
{
  GtkVBox  __parent__;

  ThunarJob *job;

  GtkWidget *progress_bar;
  GtkWidget *progress_label;
  GtkWidget *message_label;

  gchar     *icon_name;
  gchar     *title;
};



G_DEFINE_TYPE (ThunarProgressView, thunar_progress_view, GTK_TYPE_VBOX)



static void
thunar_progress_view_class_init (ThunarProgressViewClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_progress_view_finalize;
  gobject_class->dispose = thunar_progress_view_dispose;
  gobject_class->get_property = thunar_progress_view_get_property;
  gobject_class->set_property = thunar_progress_view_set_property;

  /**
   * ThunarProgressView:job:
   *
   * The #ThunarJob, whose progress is displayed by this view, or
   * %NULL if no job is set.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_JOB,
                                   g_param_spec_object ("job", "job", "job",
                                                        THUNAR_TYPE_JOB,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "icon-name",
                                                        "icon-name",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "title",
                                                        "title",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_signal_new ("need-attention",
                THUNAR_TYPE_PROGRESS_VIEW,
                G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  g_signal_new ("finished",
                THUNAR_TYPE_PROGRESS_VIEW,
                G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);
}



static void
thunar_progress_view_init (ThunarProgressView *view)
{
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *vbox3;
  GtkWidget *hbox;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (view), vbox);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  image = g_object_new (GTK_TYPE_IMAGE, "icon-size", GTK_ICON_SIZE_BUTTON, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
  exo_binding_new (G_OBJECT (view), "icon-name", G_OBJECT (image), "icon-name");
  gtk_widget_show (image);

  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
  gtk_widget_show (vbox2);

  label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big_bold ());
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  view->message_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_ellipsize (GTK_LABEL (view->message_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_pack_start (GTK_BOX (vbox2), view->message_label, TRUE, TRUE, 0);
  gtk_widget_show (view->message_label);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  vbox3 = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox), vbox3, TRUE, TRUE, 0);
  gtk_widget_show (vbox3);

  view->progress_bar = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox3), view->progress_bar, TRUE, TRUE, 0);
  gtk_widget_show (view->progress_bar);

  view->progress_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  gtk_label_set_ellipsize (GTK_LABEL (view->progress_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_attributes (GTK_LABEL (view->progress_label), thunar_pango_attr_list_small ());
  gtk_box_pack_start (GTK_BOX (vbox3), view->progress_label, FALSE, TRUE, 0);
  gtk_widget_show (view->progress_label);

  button = gtk_button_new ();
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (thunar_progress_view_cancel_job), view);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_widget_set_can_focus (button, FALSE);
  gtk_widget_show (button);

  image = gtk_image_new_from_stock (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  /* connect the view title to the action label */
  exo_binding_new (G_OBJECT (view), "title", G_OBJECT (label), "label");
}



static void
thunar_progress_view_finalize (GObject *object)
{
  ThunarProgressView *view = THUNAR_PROGRESS_VIEW (object);

  g_free (view->icon_name);
  g_free (view->title);

  (*G_OBJECT_CLASS (thunar_progress_view_parent_class)->finalize) (object);
}



static void
thunar_progress_view_dispose (GObject *object)
{
  ThunarProgressView *view = THUNAR_PROGRESS_VIEW (object);

  /* disconnect from the job (if any) */
  if (view->job != NULL)
    {
      exo_job_cancel (EXO_JOB (view->job));
      thunar_progress_view_set_job (view, NULL);
    }

  (*G_OBJECT_CLASS (thunar_progress_view_parent_class)->dispose) (object);
}



static void
thunar_progress_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarProgressView *view = THUNAR_PROGRESS_VIEW (object);

  switch (prop_id)
    {
    case PROP_JOB:
      g_value_set_object (value, thunar_progress_view_get_job (view));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, view->icon_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, view->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_progress_view_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarProgressView *view = THUNAR_PROGRESS_VIEW (object);

  switch (prop_id)
    {
    case PROP_JOB:
      thunar_progress_view_set_job (view, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      thunar_progress_view_set_icon_name (view, g_value_get_string (value));
      break;

    case PROP_TITLE:
      thunar_progress_view_set_title (view, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_progress_view_cancel_job (ThunarProgressView *view)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_JOB (view->job));

  if (view->job != NULL)
    {
      /* cancel the job */
      exo_job_cancel (EXO_JOB (view->job));

      /* don't listen to percentage updates any more */
      g_signal_handlers_disconnect_matched (view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                            thunar_progress_view_percent, NULL);

      /* don't listen to info messages any more */
      g_signal_handlers_disconnect_matched (view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                            thunar_progress_view_info_message, NULL);

      /* update the status text */
      gtk_label_set_text (GTK_LABEL (view->progress_label), _("Cancelling..."));
    }
}



static ThunarJobResponse
thunar_progress_view_ask (ThunarProgressView *view,
                          const gchar        *message,
                          ThunarJobResponse   choices,
                          ThunarJob          *job)
{
  GtkWidget *window;

  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_VIEW (view), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (message, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (view->job == job, THUNAR_JOB_RESPONSE_CANCEL);

  /* be sure to display the corresponding dialog prior to opening the question view */
  g_signal_emit_by_name (view, "need-attention");

  /* determine the toplevel window of the view */
  window = gtk_widget_get_toplevel (GTK_WIDGET (view));

  /* display the question view */
  return thunar_dialogs_show_job_ask (window != NULL ? GTK_WINDOW (window) : NULL,
                                      message, choices);
}



static ThunarJobResponse
thunar_progress_view_ask_replace (ThunarProgressView *view,
                                  ThunarFile         *src_file,
                                  ThunarFile         *dst_file,
                                  ThunarJob          *job)
{
  GtkWidget *window;

  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_VIEW (view), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (view->job == job, THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (src_file), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (dst_file), THUNAR_JOB_RESPONSE_CANCEL);

  /* be sure to display the corresponding dialog prior to opening the question view */
  g_signal_emit_by_name (view, "need-attention");

  /* determine the toplevel window of the view */
  window = gtk_widget_get_toplevel (GTK_WIDGET (view));

  /* display the question view */
  return thunar_dialogs_show_job_ask_replace (window != NULL ? GTK_WINDOW (window) : NULL,
                                              src_file, dst_file);
}



static void
thunar_progress_view_error (ThunarProgressView *view,
                            GError             *error,
                            ExoJob             *job)
{
  GtkWidget *window;

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (error != NULL && error->message != NULL);
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (view->job == THUNAR_JOB (job));

  /* be sure to display the corresponding dialog prior to opening the question view */
  g_signal_emit_by_name (view, "need-attention");

  /* determine the toplevel window of the view */
  window = gtk_widget_get_toplevel (GTK_WIDGET (view));

  /* display the error message */
  thunar_dialogs_show_job_error (window != NULL ? GTK_WINDOW (window) : NULL, error);
}



static void
thunar_progress_view_finished (ThunarProgressView *view,
                               ExoJob             *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (view->job == THUNAR_JOB (job));

  /* emit finished signal to notify others that the job is finished */
  g_signal_emit_by_name (view, "finished");
}



static void
thunar_progress_view_info_message (ThunarProgressView *view,
                                   const gchar        *message,
                                   ExoJob             *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (g_utf8_validate (message, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (view->job == THUNAR_JOB (job));

  gtk_label_set_text (GTK_LABEL (view->message_label), message);
}



static void
thunar_progress_view_percent (ThunarProgressView *view,
                              gdouble             percent,
                              ExoJob             *job)
{
  gchar *text;

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (percent >= 0.0 && percent <= 100.0);
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (view->job == THUNAR_JOB (job));

  /* update progressbar */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (view->progress_bar), percent / 100.0);

  /* set progress text */
  if (THUNAR_IS_TRANSFER_JOB (job))
    {
      text = thunar_transfer_job_get_status (THUNAR_TRANSFER_JOB (job));
      gtk_label_set_text (GTK_LABEL (view->progress_label), text);
      g_free (text);
    }
}



/**
 * thunar_progress_view_new_with_job:
 * @job : a #ThunarJob or %NULL.
 *
 * Allocates a new #ThunarProgressView and associates it with the @job.
 *
 * Return value: the newly allocated #ThunarProgressView.
 **/
GtkWidget*
thunar_progress_view_new_with_job (ThunarJob *job)
{
  _thunar_return_val_if_fail (job == NULL || THUNAR_IS_JOB (job), NULL);
  return g_object_new (THUNAR_TYPE_PROGRESS_VIEW, "job", job, NULL);
}



/**
 * thunar_progress_view_get_job:
 * @view : a #ThunarProgressView.
 *
 * Returns the #ThunarJob associated with @view
 * or %NULL if no job is currently associated with @view.
 *
 * Return value: the job associated with @view or %NULL.
 **/
static ThunarJob *
thunar_progress_view_get_job (ThunarProgressView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_VIEW (view), NULL);
  return view->job;
}



/**
 * thunar_progress_view_set_job:
 * @view : a #ThunarProgressView.
 * @job    : a #ThunarJob or %NULL.
 *
 * Associates @job with @view.
 **/
static void
thunar_progress_view_set_job (ThunarProgressView *view,
                              ThunarJob          *job)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));
  _thunar_return_if_fail (job == NULL || THUNAR_IS_JOB (job));

  /* check if we're already on that job */
  if (G_UNLIKELY (view->job == job))
    return;

  /* disconnect from the previous job */
  if (G_LIKELY (view->job != NULL))
    {
      g_signal_handlers_disconnect_matched (view->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
      g_object_unref (G_OBJECT (view->job));
    }

  /* activate the new job */
  view->job = job;

  /* connect to the new job */
  if (G_LIKELY (job != NULL))
    {
      g_object_ref (job);

      g_signal_connect_swapped (job, "ask", G_CALLBACK (thunar_progress_view_ask), view);
      g_signal_connect_swapped (job, "ask-replace", G_CALLBACK (thunar_progress_view_ask_replace), view);
      g_signal_connect_swapped (job, "error", G_CALLBACK (thunar_progress_view_error), view);
      g_signal_connect_swapped (job, "finished", G_CALLBACK (thunar_progress_view_finished), view);
      g_signal_connect_swapped (job, "info-message", G_CALLBACK (thunar_progress_view_info_message), view);
      g_signal_connect_swapped (job, "percent", G_CALLBACK (thunar_progress_view_percent), view);
    }

  g_object_notify (G_OBJECT (view), "job");
}



void
thunar_progress_view_set_icon_name (ThunarProgressView *view,
                                    const gchar        *icon_name)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));

  if (exo_str_is_equal (view->icon_name, icon_name))
    return;

  g_free (view->icon_name);
  view->icon_name = g_strdup (icon_name);

  g_object_notify (G_OBJECT (view), "icon-name");
}



void
thunar_progress_view_set_title (ThunarProgressView *view,
                                const gchar        *title)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));

  if (exo_str_is_equal (view->title, title))
    return;

  g_free (view->title);
  view->title = g_strdup (title);

  g_object_notify (G_OBJECT (view), "title");
}
