/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <thunar/thunar-private.h>
#include <thunar/thunar-progress-dialog.h>
#include <thunar/thunar-progress-view.h>



#define SCROLLVIEW_THRESHOLD 5



static void     thunar_progress_dialog_dispose            (GObject              *object);
static void     thunar_progress_dialog_finalize           (GObject              *object);
static void     thunar_progress_dialog_shown              (ThunarProgressDialog *dialog);
static gboolean thunar_progress_dialog_closed             (ThunarProgressDialog *dialog);
static gboolean thunar_progress_dialog_toggled            (ThunarProgressDialog *dialog, 
                                                           GdkEventButton       *button,
                                                           GtkStatusIcon        *status_icon);
static void     thunar_progress_dialog_update_status_icon (ThunarProgressDialog *dialog);



struct _ThunarProgressDialogClass
{
  GtkWindowClass __parent__;
};

struct _ThunarProgressDialog
{
  GtkWindow      __parent__;

  GtkStatusIcon *status_icon;
  GtkWidget     *scrollwin;
  GtkWidget     *vbox;
  GtkWidget     *content_box;

  GList         *views;

  gint           x;
  gint           y;
};



G_DEFINE_TYPE (ThunarProgressDialog, thunar_progress_dialog, GTK_TYPE_WINDOW);



static void
thunar_progress_dialog_class_init (ThunarProgressDialogClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine parent type class */
  thunar_progress_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_progress_dialog_dispose;
  gobject_class->finalize = thunar_progress_dialog_finalize;
}



static void
thunar_progress_dialog_init (ThunarProgressDialog *dialog)
{
  dialog->views = NULL;

  gtk_window_set_title (GTK_WINDOW (dialog), _("File Operation Progress"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 450, 10);
  gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), NULL);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

  g_signal_connect_swapped (dialog, "show", 
                            G_CALLBACK (thunar_progress_dialog_shown), dialog);

  g_signal_connect (dialog, "delete-event", 
                    G_CALLBACK (thunar_progress_dialog_closed), dialog);

  dialog->vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dialog), dialog->vbox);
  gtk_widget_show (dialog->vbox);

  dialog->content_box = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (dialog->content_box), 8);
  gtk_container_add (GTK_CONTAINER (dialog->vbox), dialog->content_box);
  gtk_widget_show (dialog->content_box);
}



static void
thunar_progress_dialog_dispose (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_progress_dialog_parent_class)->dispose) (object);
}



static void
thunar_progress_dialog_finalize (GObject *object)
{
  ThunarProgressDialog *dialog = THUNAR_PROGRESS_DIALOG (object);

  /* destroy the status icon */
  if (dialog->status_icon != NULL)
    {
      gtk_status_icon_set_visible (dialog->status_icon, FALSE);
      g_object_unref (dialog->status_icon);
    }

  /* free the view list */
  g_list_free (dialog->views);

  (*G_OBJECT_CLASS (thunar_progress_dialog_parent_class)->finalize) (object);
}



static void
thunar_progress_dialog_shown (ThunarProgressDialog *dialog)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));

  /* show the status icon */
  if (dialog->status_icon == NULL)
    {
      dialog->status_icon = gtk_status_icon_new_from_icon_name ("stock_folder-copy");
      thunar_progress_dialog_update_status_icon (dialog);
      g_signal_connect_swapped (dialog->status_icon, "button-press-event",
                                G_CALLBACK (thunar_progress_dialog_toggled),
                                GTK_WIDGET (dialog));
    }
}



static gboolean
thunar_progress_dialog_closed (ThunarProgressDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), FALSE);

  /* remember the position of the dialog */
  gtk_window_get_position (GTK_WINDOW (dialog), &dialog->x, &dialog->y);

  /* hide the progress dialog */
  gtk_widget_hide (GTK_WIDGET (dialog));

  /* don't destroy the dialog */
  return TRUE;
}



static gboolean
thunar_progress_dialog_toggled (ThunarProgressDialog *dialog,
                                GdkEventButton       *event,
                                GtkStatusIcon        *status_icon)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), FALSE);
  _thunar_return_val_if_fail (GTK_IS_STATUS_ICON (status_icon), FALSE);

  /* check if the window is visible and has the focus */
  if (gtk_widget_get_visible (GTK_WIDGET (dialog)) 
      && gtk_window_is_active (GTK_WINDOW (dialog)))
    {
      /* remember the position of the dialog */
      gtk_window_get_position (GTK_WINDOW (dialog), &dialog->x, &dialog->y);

      /* it is, so hide it now */
      gtk_widget_hide (GTK_WIDGET (dialog));
    }
  else
    {
      /* check if the dialog is invisible */
      if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
        {
          /* restore its previous position before presenting it */
          gtk_window_move (GTK_WINDOW (dialog), dialog->x, dialog->y);
        }

      /* it's not, so we need to raise it above other windows */
      gtk_window_present_with_time (GTK_WINDOW (dialog), event->time);
    }

  return TRUE;
}



static void
thunar_progress_dialog_view_needs_attention (ThunarProgressDialog *dialog,
                                             ThunarProgressView   *view)
{
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));

  /* TODO scroll to the view */

  /* raise the dialog */
  gtk_window_present (GTK_WINDOW (dialog));
}



static void
thunar_progress_dialog_job_finished (ThunarProgressDialog *dialog,
                                     ThunarProgressView   *view)
{
  guint n_views;

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (THUNAR_IS_PROGRESS_VIEW (view));

  /* remove the view from the list */
  dialog->views = g_list_remove (dialog->views, view);

  /* destroy the widget */
  gtk_widget_destroy (GTK_WIDGET (view));

  /* determine the number of views left */
  n_views = g_list_length (dialog->views);

  /* check if we've just removed the 4th view and are now left with
   * SCROLLVIEW_THRESHOLD-1 of them, in which case we drop the scroll window */
  if (n_views == SCROLLVIEW_THRESHOLD-1)
    {
      /* reparent the content box */
      gtk_widget_reparent (dialog->content_box, dialog->vbox);

      /* destroy the scroll win */
      gtk_widget_destroy (dialog->scrollwin);
    }

  /* check if we have less than SCROLLVIEW_THRESHOLD views 
   * and need to shrink the window */
  if (n_views < SCROLLVIEW_THRESHOLD)
    {
      /* try to shrink the window */
      gtk_window_resize (GTK_WINDOW (dialog), 450, 10);
    }

  /* check if we still have at least one view */
  if (dialog->views != NULL)
    {
      /* update the status icon */
      if (dialog->status_icon != NULL)
        thunar_progress_dialog_update_status_icon (dialog);
    }
  else
    {
      /* destroy the dialog as there are no views left */
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}



static void
thunar_progress_dialog_update_status_icon (ThunarProgressDialog *dialog)
{
  gchar *tooltip_text;
  guint  n_views;

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (GTK_IS_STATUS_ICON (dialog->status_icon));

  /* determine the number of views now being active */
  n_views = g_list_length (dialog->views);

  /* build the tooltip text */
  tooltip_text = g_strdup_printf (ngettext ("%d file operation running", 
                                            "%d file operations running",
                                            n_views), 
                                            n_views);

  /* update the tooltip */
  gtk_status_icon_set_tooltip_text (dialog->status_icon, tooltip_text);

  /* free the string */
  g_free (tooltip_text);
}






GtkWidget*
thunar_progress_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_PROGRESS_DIALOG, NULL);
}



void
thunar_progress_dialog_add_job (ThunarProgressDialog *dialog,
                                ThunarJob            *job,
                                const gchar          *icon_name,
                                const gchar          *title)
{
  GtkWidget *viewport;
  GtkWidget *view;

  _thunar_return_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (g_utf8_validate (title, -1, NULL));

  view = thunar_progress_view_new_with_job (job);
  thunar_progress_view_set_icon_name (THUNAR_PROGRESS_VIEW (view), icon_name);
  thunar_progress_view_set_title (THUNAR_PROGRESS_VIEW (view), title);
  gtk_box_pack_start (GTK_BOX (dialog->content_box), view, FALSE, TRUE, 0);
  gtk_widget_show (view);

  /* use the first job's icon-name for the dialog */
  if (dialog->views == NULL)
    gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

  /* add the view to the list of known views */
  dialog->views = g_list_prepend (dialog->views, view);

  /* check if we need to wrap the views in a scroll window (starting 
   * at SCROLLVIEW_THRESHOLD parallel operations */
  if (g_list_length (dialog->views) == SCROLLVIEW_THRESHOLD)
    {
      /* create a scrolled window and add it to the dialog */
      dialog->scrollwin = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->scrollwin), 
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (dialog->vbox), dialog->scrollwin);
      gtk_widget_show (dialog->scrollwin);

      /* create a viewport for the content box */
      viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (dialog->scrollwin)),
                                   gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (dialog->scrollwin)));
      gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (dialog->scrollwin), viewport);
      gtk_widget_show (viewport);

      /* move the content box into the viewport */
      gtk_widget_reparent (dialog->content_box, viewport);
    }

  g_signal_connect_swapped (view, "need-attention", 
                            G_CALLBACK (thunar_progress_dialog_view_needs_attention), dialog);

  g_signal_connect_swapped (view, "finished",
                            G_CALLBACK (thunar_progress_dialog_job_finished), dialog);

  if (dialog->status_icon != NULL)
    thunar_progress_dialog_update_status_icon (dialog);
}



gboolean
thunar_progress_dialog_has_jobs (ThunarProgressDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROGRESS_DIALOG (dialog), FALSE);
  return dialog->views != NULL;
}
