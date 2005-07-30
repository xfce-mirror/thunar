/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005 Jeff Franks <jcfranks@xfce.org>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-desktop-view.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-progress-dialog.h>



static void     thunar_application_class_init           (ThunarApplicationClass *klass);
static void     thunar_application_init                 (ThunarApplication      *application);
static void     thunar_application_finalize             (GObject                *object);
static void     thunar_application_handle_job           (ThunarApplication *application,
                                                         GtkWindow         *window,
                                                         ThunarVfsJob      *job,
                                                         const gchar       *icon_name,
                                                         const gchar       *title);
static void     thunar_application_window_destroyed     (GtkWidget         *window,
                                                         ThunarApplication *application);
static gboolean thunar_application_show_dialogs         (gpointer           user_data);
static void     thunar_application_show_dialogs_destroy (gpointer           user_data);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject __parent__;

  ThunarDesktopView *desktop_view;
  ThunarPreferences *preferences;
  GList             *windows;

  gint               show_dialogs_timer_id;
};



G_DEFINE_TYPE (ThunarApplication, thunar_application, G_TYPE_OBJECT);



static void
thunar_application_class_init (ThunarApplicationClass *klass)
{
  GObjectClass *gobject_class;
 
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_application_finalize;
}



static void
thunar_application_init (ThunarApplication *application)
{
  application->preferences = thunar_preferences_get ();
  application->show_dialogs_timer_id = -1;
}



static void
thunar_application_finalize (GObject *object)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);
  GList             *lp;

  /* drop any running "show dialogs" timer */
  if (G_UNLIKELY (application->show_dialogs_timer_id >= 0))
    g_source_remove (application->show_dialogs_timer_id);

  for (lp = application->windows; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), G_CALLBACK (thunar_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (lp->data));
    }
  g_list_free (application->windows);

  g_object_unref (G_OBJECT (application->preferences));
  
  if (G_LIKELY (application->desktop_view != NULL))
    g_object_unref (G_OBJECT (application->desktop_view));

  G_OBJECT_CLASS (thunar_application_parent_class)->finalize (object);
}



static void
thunar_application_handle_job (ThunarApplication *application,
                               GtkWindow         *window,
                               ThunarVfsJob      *job,
                               const gchar       *icon_name,
                               const gchar       *title)
{
  GtkWidget *dialog;

  /* allocate a progress dialog for the job */
  dialog = g_object_new (THUNAR_TYPE_PROGRESS_DIALOG,
                         "icon-name", icon_name,
                         "title", title,
                         "job", job,
                         NULL);

  /* connect to the parent (if any) */
  if (G_LIKELY (window != NULL))
    gtk_window_set_transient_for (GTK_WINDOW (dialog), window);

  /* be sure to destroy the dialog when the job is done */
  g_signal_connect_after (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), dialog);

  /* hook up the dialog window */
  g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (thunar_application_window_destroyed), application);
  application->windows = g_list_prepend (application->windows, dialog);

  /* Set up a timer to show the dialog, to make sure we don't
   * just popup and destroy a dialog for a very short job.
   */
  if (G_LIKELY (application->show_dialogs_timer_id < 0))
    {
      application->show_dialogs_timer_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 750, thunar_application_show_dialogs,
                                                               application, thunar_application_show_dialogs_destroy);
    }
}



static void
thunar_application_window_destroyed (GtkWidget         *window,
                                     ThunarApplication *application)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (g_list_find (application->windows, window) != NULL);

  application->windows = g_list_remove (application->windows, window);

  /* terminate the application if we don't have any more
   * windows and we don't manage the desktop.
   */
  if (G_UNLIKELY (application->windows == NULL
        && application->desktop_view == NULL))
    {
      gtk_main_quit ();
    }
}



static gboolean
thunar_application_show_dialogs (gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);
  GList             *lp;

  GDK_THREADS_ENTER ();

  /* show all progress dialogs */
  for (lp = application->windows; lp != NULL; lp = lp->next)
    if (THUNAR_IS_PROGRESS_DIALOG (lp->data))
      gtk_widget_show (GTK_WIDGET (lp->data));

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_application_show_dialogs_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->show_dialogs_timer_id = -1;
}



/**
 * thunar_application_get:
 *
 * Returns the global shared #ThunarApplication instance.
 * This method takes a reference on the global instance
 * for the caller, so you must call #g_object_unref()
 * on it when done.
 *
 * Return value: the shared #ThunarApplication instance.
 **/
ThunarApplication*
thunar_application_get (void)
{
  static ThunarApplication *application = NULL;

  if (G_UNLIKELY (application == NULL))
    {
      application = g_object_new (THUNAR_TYPE_APPLICATION, NULL);
      g_object_add_weak_pointer (G_OBJECT (application), (gpointer) &application);
    }
  else
    {
      g_object_ref (G_OBJECT (application));
    }

  return application;
}



/**
 * thunar_application_get_windows:
 * @application : a #ThunarApplication.
 *
 * Returns the list of regular #ThunarWindows currently handled by
 * @application. The returned list is owned by the caller and
 * must be freed using #g_list_free().
 *
 * Return value: the list of regular #ThunarWindows in @application.
 **/
GList*
thunar_application_get_windows (ThunarApplication *application)
{
  GList *windows = NULL;
  GList *lp;

  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);

  for (lp = application->windows; lp != NULL; lp = lp->next)
    if (G_LIKELY (THUNAR_IS_WINDOW (lp->data)))
      windows = g_list_prepend (windows, lp->data);

  return windows;
}



/**
 * thunar_application_open_window:
 * @application : a #ThunarApplication.
 * @directory   : the directory to open.
 * @screen      : the #GdkScreen on which to open the window or %NULL
 *                to open on the default screen.
 *
 * Opens a new #ThunarWindow for @application, displaying the
 * given @directory.
 **/
void
thunar_application_open_window (ThunarApplication *application,
                                ThunarFile        *directory,
                                GdkScreen         *screen)
{
  GtkWidget *window;

  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (THUNAR_IS_FILE (directory));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* allocate the window */
  window = g_object_new (THUNAR_TYPE_WINDOW,
                         "screen", screen,
                         NULL);

  /* hook up the window */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (thunar_application_window_destroyed), application);
  application->windows = g_list_prepend (application->windows, window);

  /* show the new window */
  gtk_widget_show (window);

  /* change the directory */
  thunar_window_set_current_directory (THUNAR_WINDOW (window), directory);
}



/**
 * thunar_application_unlink_files:
 * @application : a #ThunarApplication.
 * @window      : a parent #GtkWindow or %NULL.
 * @uri_list    : a list of #ThunarVfsURI<!---->s.
 *
 * Unlinks all files referenced by the @uri_list. This method takes
 * care of all user interaction.
 **/
void
thunar_application_unlink_files (ThunarApplication *application,
                                 GtkWindow         *window,
                                 GList             *uri_list)
{
  ThunarVfsJob *job;
  GtkWidget    *message;
  GError       *error = NULL;

  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (window == NULL || GTK_IS_WINDOW (window));
  
  /* allocate the job */
  job = thunar_vfs_unlink (uri_list, &error);
  if (G_UNLIKELY (job == NULL))
    {
      message = gtk_message_dialog_new (window, GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                        "%s", error->message);
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
  else
    {
      /* let the application take care of the dialog */
      thunar_application_handle_job (application, window, job, "stock_delete", _("Purging files..."));
      thunar_vfs_job_unref (job);
    }
}




