/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
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
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-progress-dialog.h>



/* Prototype for the Thunar-VFS job launchers */
typedef ThunarVfsJob *(*Launcher) (GList   *source_path_list,
                                   GList   *target_path_list,
                                   GError **error);



static void     thunar_application_class_init           (ThunarApplicationClass *klass);
static void     thunar_application_init                 (ThunarApplication      *application);
static void     thunar_application_finalize             (GObject                *object);
static void     thunar_application_collect_and_launch   (ThunarApplication      *application,
                                                         GtkWidget              *widget,
                                                         const gchar            *icon_name,
                                                         const gchar            *title,
                                                         Launcher                launcher,
                                                         GList                  *source_path_list,
                                                         ThunarVfsPath          *target_path,
                                                         GClosure               *new_files_closure);
static void     thunar_application_launch               (ThunarApplication      *application,
                                                         GtkWidget              *widget,
                                                         const gchar            *icon_name,
                                                         const gchar            *title,
                                                         Launcher                launcher,
                                                         GList                  *source_path_list,
                                                         GList                  *target_path_list,
                                                         GClosure               *new_files_closure);
static void     thunar_application_window_destroyed     (GtkWidget              *window,
                                                         ThunarApplication      *application);
static gboolean thunar_application_show_dialogs         (gpointer                user_data);
static void     thunar_application_show_dialogs_destroy (gpointer                user_data);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject __parent__;

  ThunarPreferences *preferences;
  GList             *windows;

  gint               show_dialogs_timer_id;
};



static GObjectClass *thunar_application_parent_class;



GType
thunar_application_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarApplicationClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_application_class_init,
        NULL,
        NULL,
        sizeof (ThunarApplication),
        0,
        (GInstanceInitFunc) thunar_application_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarApplication"), &info, 0);
    }

  return type;
}



static void
thunar_application_class_init (ThunarApplicationClass *klass)
{
  GObjectClass *gobject_class;
 
  /* determine the parent type class */
  thunar_application_parent_class = g_type_class_peek_parent (klass);

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

  /* drop the open windows */
  for (lp = application->windows; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), G_CALLBACK (thunar_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (lp->data));
    }
  g_list_free (application->windows);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (application->preferences));
  
  (*G_OBJECT_CLASS (thunar_application_parent_class)->finalize) (object);
}



static void
thunar_application_collect_and_launch (ThunarApplication *application,
                                       GtkWidget         *widget,
                                       const gchar       *icon_name,
                                       const gchar       *title,
                                       Launcher           launcher,
                                       GList             *source_path_list,
                                       ThunarVfsPath     *target_path,
                                       GClosure          *new_files_closure)
{
  ThunarVfsPath *path;
  GList         *target_path_list = NULL;
  GList         *lp;

  /* check if we have anything to operate on */
  if (G_UNLIKELY (source_path_list == NULL))
    return;

  /* generate the target path list */
  for (lp = g_list_last (source_path_list); lp != NULL; lp = lp->prev)
    {
      path = thunar_vfs_path_relative (target_path, thunar_vfs_path_get_name (lp->data));
      target_path_list = g_list_prepend (target_path_list, path);
    }

  /* launch the operation */
  thunar_application_launch (application, widget, icon_name, title, launcher,
                             source_path_list, target_path_list, new_files_closure);

  /* release the target path list */
  thunar_vfs_path_list_free (target_path_list);
}



static void
thunar_application_launch (ThunarApplication *application,
                           GtkWidget         *widget,
                           const gchar       *icon_name,
                           const gchar       *title,
                           Launcher           launcher,
                           GList             *source_path_list,
                           GList             *target_path_list,
                           GClosure          *new_files_closure)
{
  ThunarVfsJob *job;
  GtkWidget    *dialog;
  GtkWindow    *window;
  GError       *error = NULL;

  /* determine the toplevel window for the widget */
  window = (widget != NULL) ? (GtkWindow *) gtk_widget_get_toplevel (widget) : NULL;

  /* try to allocate a new job for the operation */
  job = (*launcher) (source_path_list, target_path_list, &error);
  if (G_UNLIKELY (job == NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (widget, error, _("Failed to launch operation"));

      /* release the error */
      g_error_free (error);
    }
  else
    {
      /* connect the "new-files" closure (if any) */
      if (G_LIKELY (new_files_closure != NULL))
        g_signal_connect_closure (G_OBJECT (job), "new-files", new_files_closure, FALSE);

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

      /* drop our reference on the job */
      g_object_unref (G_OBJECT (job));
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
        /*&& application->desktop_view == NULL*/))
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
 * for the caller, so you must call g_object_unref()
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
 * must be freed using g_list_free().
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
 * thunar_application_copy_to:
 * @application       : a #ThunarApplication.
 * @widget            : the associated widget or %NULL.
 * @source_path_list  : the list of #ThunarVfsPath<!---->s that should be copied.
 * @target_path_list  : the list of #ThunarVfsPath<!---->s where files should be copied to.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files from @source_path_list to their locations specified in
 * @target_path_list.
 *
 * @source_path_list and @target_path_list must be of the same length. 
 **/
void
thunar_application_copy_to (ThunarApplication *application,
                            GtkWidget         *widget,
                            GList             *source_path_list,
                            GList             *target_path_list,
                            GClosure          *new_files_closure)
{
  g_return_if_fail (g_list_length (source_path_list) == g_list_length (target_path_list));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, widget, "stock_folder-copy",
                             _("Copying files..."), thunar_vfs_copy_files,
                             source_path_list, target_path_list, new_files_closure);
}



/**
 * thunar_application_copy_into:
 * @application       : a #ThunarApplication.
 * @widget            : the associated widget or %NULL.
 * @source_path_list  : the list of #ThunarVfsPath<!---->s that should be copied.
 * @target_path       : the #ThunarVfsPath to the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files referenced by the @source_path_list to the directory
 * referenced by @target_path. This method takes care of all user interaction.
 **/
void
thunar_application_copy_into (ThunarApplication *application,
                              GtkWidget         *widget,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (target_path != NULL);

  /* collect the target paths and launch the job */
  thunar_application_collect_and_launch (application, widget, "stock_folder-copy",
                                         _("Copying files..."), thunar_vfs_copy_files,
                                         source_path_list, target_path, new_files_closure);
}



/**
 * thunar_application_link_into:
 * @application       : a #ThunarApplication.
 * @widget            : the associated #GtkWidget or %NULL.
 * @source_path_list  : the list of #ThunarVfsPath<!---->s that should be symlinked.
 * @target_path       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Symlinks all files referenced by the @source_path_list to the directory
 * referenced by @target_path. This method takes care of all user
 * interaction.
 **/
void
thunar_application_link_into (ThunarApplication *application,
                              GtkWidget         *widget,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (target_path != NULL);

  /* collect the target paths and launch the job */
  thunar_application_collect_and_launch (application, widget, "stock_link",
                                         _("Creating symbolic links..."),
                                         thunar_vfs_link_files, source_path_list,
                                         target_path, new_files_closure);
}



/**
 * thunar_application_move_into:
 * @application       : a #ThunarApplication.
 * @widget            : the associated #GtkWidget or %NULL.
 * @source_path_list  : the list of #ThunarVfsPath<!---->s that should be moved.
 * @target_path       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Moves all files referenced by the @source_path_list to the directory
 * referenced by @target_path. This method takes care of all user
 * interaction.
 **/
void
thunar_application_move_into (ThunarApplication *application,
                              GtkWidget         *widget,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (target_path != NULL);

  /* launch the operation */
  thunar_application_collect_and_launch (application, widget, "stock_folder-move",
                                         _("Moving files..."), thunar_vfs_move_files,
                                         source_path_list, target_path, new_files_closure);
}



static ThunarVfsJob*
unlink_stub (GList   *source_path_list,
             GList   *target_path_list,
             GError **error)
{
  return thunar_vfs_unlink_files (source_path_list, error);
}



/**
 * thunar_application_unlink:
 * @application : a #ThunarApplication.
 * @widget      : the associated #GtkWidget or %NULL.
 * @path_list   : the list of #ThunarVfsPath<!---->s that should be deleted.
 *
 * Deletes all files referenced by the @path_list and takes care of all user
 * interaction.
 **/
void
thunar_application_unlink (ThunarApplication *application,
                           GtkWidget         *widget,
                           GList             *path_list)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  /* launch the operation */
  thunar_application_launch (application, widget, "stock_delete",
                              _("Deleting files..."), unlink_stub,
                              path_list, path_list, NULL);
}



static ThunarVfsJob*
creat_stub (GList   *source_path_list,
            GList   *target_path_list,
            GError **error)
{
  return thunar_vfs_create_files (source_path_list, error);
}



/**
 * thunar_application_creat:
 * @application       : a #ThunarApplication.
 * @widget            : the associated #GtkWidget or %NULL.
 * @path_list         : the list of files to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates empty files for all #ThunarVfsPath<!---->s listed in @path_list. This
 * method takes care of all user interaction.
 **/
void
thunar_application_creat (ThunarApplication *application,
                          GtkWidget         *widget,
                          GList             *path_list,
                          GClosure          *new_files_closure)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  /* launch the operation */
  thunar_application_launch (application, widget, "stock_new",
                             _("Creating files..."), creat_stub,
                             path_list, path_list, new_files_closure);
}



static ThunarVfsJob*
mkdir_stub (GList   *source_path_list,
            GList   *target_path_list,
            GError **error)
{
  return thunar_vfs_make_directories (source_path_list, error);
}



/**
 * thunar_application_mkdir:
 * @application       : a #ThunarApplication.
 * @widget            : the associated #GtkWidget or %NULL.
 * @path_list         : the list of directories to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates all directories referenced by the @path_list. This method takes care of all user
 * interaction.
 **/
void
thunar_application_mkdir (ThunarApplication *application,
                          GtkWidget         *widget,
                          GList             *path_list,
                          GClosure          *new_files_closure)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  /* launch the operation */
  thunar_application_launch (application, widget, "stock_folder",
                             _("Creating directories..."), mkdir_stub,
                             path_list, path_list, new_files_closure);
}


