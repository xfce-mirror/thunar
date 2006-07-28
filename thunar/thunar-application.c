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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-progress-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-util.h>



/* Prototype for the Thunar-VFS job launchers */
typedef ThunarVfsJob *(*Launcher) (GList   *source_path_list,
                                   GList   *target_path_list,
                                   GError **error);



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DAEMON,
};



static void       thunar_application_class_init             (ThunarApplicationClass *klass);
static void       thunar_application_init                   (ThunarApplication      *application);
static void       thunar_application_finalize               (GObject                *object);
static void       thunar_application_get_property           (GObject                *object,
                                                             guint                   prop_id,
                                                             GValue                 *value,
                                                             GParamSpec             *pspec);
static void       thunar_application_set_property           (GObject                *object,
                                                             guint                   prop_id,
                                                             const GValue           *value,
                                                             GParamSpec             *pspec);
static void       thunar_application_collect_and_launch     (ThunarApplication      *application,
                                                             gpointer                parent,
                                                             const gchar            *icon_name,
                                                             const gchar            *title,
                                                             Launcher                launcher,
                                                             GList                  *source_path_list,
                                                             ThunarVfsPath          *target_path,
                                                             GClosure               *new_files_closure);
static void       thunar_application_launch                 (ThunarApplication      *application,
                                                             gpointer                parent,
                                                             const gchar            *icon_name,
                                                             const gchar            *title,
                                                             Launcher                launcher,
                                                             GList                  *source_path_list,
                                                             GList                  *target_path_list,
                                                             GClosure               *new_files_closure);
static GtkWidget *thunar_application_open_window_with_role  (ThunarApplication      *application,
                                                             const gchar            *role,
                                                             ThunarFile             *directory,
                                                             GdkScreen              *screen);
static void       thunar_application_window_destroyed       (GtkWidget              *window,
                                                             ThunarApplication      *application);
static gboolean   thunar_application_show_dialogs           (gpointer                user_data);
static void       thunar_application_show_dialogs_destroy   (gpointer                user_data);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject __parent__;

  ThunarPreferences *preferences;
  GList             *windows;

  gboolean           daemon;

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
  gobject_class->get_property = thunar_application_get_property;
  gobject_class->set_property = thunar_application_set_property;

  /**
   * ThunarApplication:daemon:
   *
   * %TRUE if the application should be run in daemon mode,
   * in which case it will never terminate. %FALSE if the
   * application should terminate once the last window is
   * closed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DAEMON,
                                   g_param_spec_boolean ("daemon",
                                                         "daemon",
                                                         "daemon",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_application_init (ThunarApplication *application)
{
  gchar *path;

  /* initialize the application */
  application->preferences = thunar_preferences_get ();
  application->show_dialogs_timer_id = -1;

  /* check if we have a saved accel map */
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "Thunar/accels.scm");
  if (G_LIKELY (path != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (path);
      g_free (path);
    }
}



static void
thunar_application_finalize (GObject *object)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);
  gchar             *path;
  GList             *lp;

  /* save the current accel map */
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/accels.scm", TRUE);
  if (G_LIKELY (path != NULL))
    {
      /* save the accel map */
      gtk_accel_map_save (path);
      g_free (path);
    }

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
thunar_application_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);
  
  switch (prop_id)
    {
    case PROP_DAEMON:
      g_value_set_boolean (value, thunar_application_get_daemon (application));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_application_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);
  
  switch (prop_id)
    {
    case PROP_DAEMON:
      thunar_application_set_daemon (application, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_application_collect_and_launch (ThunarApplication *application,
                                       gpointer           parent,
                                       const gchar       *icon_name,
                                       const gchar       *title,
                                       Launcher           launcher,
                                       GList             *source_path_list,
                                       ThunarVfsPath     *target_path,
                                       GClosure          *new_files_closure)
{
  ThunarVfsInfo *info;
  ThunarVfsPath *path;
  GError        *err = NULL;
  GList         *target_path_list = NULL;
  GList         *lp;
  gchar         *original_path;
  gchar         *original_name;

  /* check if we have anything to operate on */
  if (G_UNLIKELY (source_path_list == NULL))
    return;

  /* generate the target path list */
  for (lp = g_list_last (source_path_list); err == NULL && lp != NULL; lp = lp->prev)
    {
      /* reset the path */
      path = NULL;

      /* verify that we're not trying to collect a root node */
      if (G_UNLIKELY (thunar_vfs_path_is_root (lp->data)))
        {
          /* tell the user that we cannot perform the requested operation */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL, g_strerror (EINVAL));
        }
      else
        {
          /* check if we're copying from a location in the trash */
          if (G_UNLIKELY (thunar_vfs_path_get_scheme (lp->data) == THUNAR_VFS_PATH_SCHEME_TRASH))
            {
              /* determine the info for the trashed resource */
              info = thunar_vfs_info_new_for_path (lp->data, NULL);
              if (G_LIKELY (info != NULL))
                {
                  /* try to use the basename of the original path */
                  original_path = thunar_vfs_info_get_metadata (info, THUNAR_VFS_INFO_METADATA_TRASH_ORIGINAL_PATH, NULL);
                  if (G_LIKELY (original_path != NULL))
                    {
                      /* g_path_get_basename() may return '.' or '/' */
                      original_name = g_path_get_basename (original_path);
                      if (strcmp (original_name, ".") != 0 && strchr (original_name, G_DIR_SEPARATOR) == NULL)
                        path = thunar_vfs_path_relative (target_path, original_name);
                      g_free (original_name);
                      g_free (original_path);
                    }

                  /* release the info */
                  thunar_vfs_info_unref (info);
                }
            }

          /* fallback to the path's basename */
          if (G_LIKELY (path == NULL))
            path = thunar_vfs_path_relative (target_path, thunar_vfs_path_get_name (lp->data));

          /* add to the target path list */
          target_path_list = g_list_prepend (target_path_list, path);
        }
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (parent, err, _("Failed to launch operation"));

      /* release the error */
      g_error_free (err);
    }
  else
    {
      /* launch the operation */
      thunar_application_launch (application, parent, icon_name, title, launcher,
                                 source_path_list, target_path_list, new_files_closure);
    }

  /* release the target path list */
  thunar_vfs_path_list_free (target_path_list);
}



static void
thunar_application_launch (ThunarApplication *application,
                           gpointer           parent,
                           const gchar       *icon_name,
                           const gchar       *title,
                           Launcher           launcher,
                           GList             *source_path_list,
                           GList             *target_path_list,
                           GClosure          *new_files_closure)
{
  ThunarVfsJob *job;
  GtkWindow    *window;
  GtkWidget    *dialog;
  GdkScreen    *screen;
  GError       *error = NULL;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, &window);

  /* try to allocate a new job for the operation */
  job = (*launcher) (source_path_list, target_path_list, &error);
  if (G_UNLIKELY (job == NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (parent, error, _("Failed to launch operation"));

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
                             "screen", screen,
                             NULL);

      /* connect to the parent (if any) */
      if (G_LIKELY (window != NULL))
        gtk_window_set_transient_for (GTK_WINDOW (dialog), window);

      /* be sure to destroy the dialog when the job is done */
      g_signal_connect_after (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), dialog);

      /* hook up the dialog window */
      thunar_application_take_window (application, GTK_WINDOW (dialog));

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



static GtkWidget*
thunar_application_open_window_with_role (ThunarApplication *application,
                                          const gchar       *role,
                                          ThunarFile        *directory,
                                          GdkScreen         *screen)
{
  GtkWidget *window;

  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* allocate the window */
  window = g_object_new (THUNAR_TYPE_WINDOW,
                         "role", role,
                         "screen", screen,
                         NULL);

  /* hook up the window */
  thunar_application_take_window (application, GTK_WINDOW (window));

  /* show the new window */
  gtk_widget_show (window);

  /* change the directory */
  thunar_window_set_current_directory (THUNAR_WINDOW (window), directory);

  return window;
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
   * windows and we are not in daemon mode.
   */
  if (G_UNLIKELY (application->windows == NULL && !application->daemon))
    gtk_main_quit ();
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
 * thunar_application_get_daemon:
 * @application : a #ThunarApplication.
 *
 * Returns %TRUE if @application is in daemon mode.
 *
 * Return value: %TRUE if @application is in daemon mode.
 **/
gboolean
thunar_application_get_daemon (ThunarApplication *application)
{
  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  return application->daemon;
}



/**
 * thunar_application_set_daemon:
 * @application : a #ThunarApplication.
 * @daemon      : %TRUE to set @application into daemon mode.
 *
 * If @daemon is %TRUE, @application will be set into daemon mode.
 **/
void
thunar_application_set_daemon (ThunarApplication *application,
                               gboolean           daemon)
{
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  if (application->daemon != daemon)
    {
      application->daemon = daemon;
      g_object_notify (G_OBJECT (application), "daemon");
    }
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
 * thunar_application_has_windows:
 * @application : a #ThunarApplication.
 *
 * Returns %TRUE if @application controls atleast one window.
 *
 * Return value: %TRUE if @application controls atleast one window.
 **/
gboolean
thunar_application_has_windows (ThunarApplication *application)
{
  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  return (application->windows != NULL);
}



/**
 * thunar_application_take_window:
 * @application : a #ThunarApplication.
 * @window      : a #GtkWindow.
 *
 * Lets @application take over control of the specified @window.
 * @application will not exit until the last controlled #GtkWindow
 * is closed by the user.
 **/
void
thunar_application_take_window (ThunarApplication *application,
                                GtkWindow         *window)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (g_list_find (application->windows, window) == NULL);

  /* connect to the "destroy" signal */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (thunar_application_window_destroyed), application);

  /* add the window to our internal list */
  application->windows = g_list_prepend (application->windows, window);
}



/**
 * thunar_application_open_window:
 * @application    : a #ThunarApplication.
 * @directory      : the directory to open.
 * @screen         : the #GdkScreen on which to open the window or %NULL
 *                   to open on the default screen.
 *
 * Opens a new #ThunarWindow for @application, displaying the
 * given @directory.
 *
 * Return value: the newly allocated #ThunarWindow.
 **/
GtkWidget*
thunar_application_open_window (ThunarApplication *application,
                                ThunarFile        *directory,
                                GdkScreen         *screen)
{
  GtkWidget *window;
  gchar     *role;

  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (THUNAR_IS_FILE (directory), NULL);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), NULL);

  /* generate a unique role for the new window (for session management) */
  role = g_strdup_printf ("Thunar-%u-%u", (guint) time (NULL), (guint) g_random_int ());
  window = thunar_application_open_window_with_role (application, role, directory, screen);
  g_free (role);

  return window;
}



/**
 * thunar_application_bulk_rename:
 * @application       : a #ThunarApplication.
 * @working_directory : the default working directory for the bulk rename dialog.
 * @filenames         : the list of file names that should be renamed or the empty
 *                      list to start with an empty rename dialog. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory.
 * @standalone        : %TRUE to display the bulk rename dialog like a standalone
 *                      application.
 * @screen            : the #GdkScreen on which to rename the @filenames or %NULL
 *                      to use the default #GdkScreen.
 * @error             : return location for errors or %NULL.
 *
 * Tries to popup the bulk rename dialog.
 *
 * Return value: %TRUE if the dialog was opened successfully, otherwise %FALSE.
 **/
gboolean
thunar_application_bulk_rename (ThunarApplication *application,
                                const gchar       *working_directory,
                                gchar            **filenames,
                                gboolean           standalone,
                                GdkScreen         *screen,
                                GError           **error)
{
  ThunarFile *current_directory = NULL;
  ThunarFile *file;
  gboolean    result = FALSE;
  GList      *file_list = NULL;
  gchar      *filename;
  gint        n;

  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (working_directory != NULL, FALSE);

  /* determine the file for the working directory */
  current_directory = thunar_file_get_for_uri (working_directory, error);
  if (G_UNLIKELY (current_directory == NULL))
    return FALSE;

  /* check if we should use the default screen */
  if (G_LIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* try to process all filenames and convert them to the appropriate file objects */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename is an absolute path or looks like an URI */
      if (g_path_is_absolute (filenames[n]) || thunar_util_looks_like_an_uri (filenames[n]))
        {
          /* determine the file for the filename directly */
          file = thunar_file_get_for_uri (filenames[n], error);
        }
      else
        {
          /* translate the filename into an absolute path first */
          filename = g_build_filename (working_directory, filenames[n], NULL);
          file = thunar_file_get_for_uri (filename, error);
          g_free (filename);
        }

      /* verify that we have a valid file */
      if (G_LIKELY (file != NULL))
        file_list = g_list_append (file_list, file);
      else
        break;
    }

  /* check if the filenames where resolved successfully */
  if (G_LIKELY (filenames[n] == NULL))
    {
      /* popup the bulk rename dialog */
      thunar_show_renamer_dialog (screen, current_directory, file_list, standalone);

      /* we succeed */
      result = TRUE;
    }

  /* cleanup */
  g_object_unref (G_OBJECT (current_directory));
  thunar_file_list_free (file_list);

  return result;
}



/**
 * thunar_application_process_filenames:
 * @application       : a #ThunarApplication.
 * @working_directory : the working directory relative to which the @filenames should
 *                      be interpreted.
 * @filenames         : a list of supported URIs or filenames. If a filename is specified
 *                      it can be either an absolute path or a path relative to the
 *                      @working_directory.
 * @screen            : the #GdkScreen on which to process the @filenames, or %NULL to
 *                      use the default screen.
 * @error             : return location for errors or %NULL.
 *
 * Tells @application to process the given @filenames and launch them appropriately.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
thunar_application_process_filenames (ThunarApplication *application,
                                      const gchar       *working_directory,
                                      gchar            **filenames,
                                      GdkScreen         *screen,
                                      GError           **error)
{
  ThunarFile *file;
  GError     *derror = NULL;
  gchar      *filename;
  GList      *file_list = NULL;
  GList      *lp;
  gint        n;

  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (working_directory != NULL, FALSE);
  g_return_val_if_fail (filenames != NULL, FALSE);
  g_return_val_if_fail (*filenames != NULL, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to process all filenames and convert them to the appropriate file objects */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename is an absolute path or looks like an URI */
      if (g_path_is_absolute (filenames[n]) || thunar_util_looks_like_an_uri (filenames[n]))
        {
          /* determine the file for the filename directly */
          file = thunar_file_get_for_uri (filenames[n], &derror);
        }
      else
        {
          /* translate the filename into an absolute path first */
          filename = g_build_filename (working_directory, filenames[n], NULL);
          file = thunar_file_get_for_uri (filename, &derror);
          g_free (filename);
        }

      /* verify that we have a valid file */
      if (G_LIKELY (file != NULL))
        file_list = g_list_append (file_list, file);
      else
        goto failure;
    }

  /* ok, let's try to launch the given files then */
  for (lp = file_list; lp != NULL; lp = lp->next)
    {
      /* try to launch this file, display an error dialog if that fails */
      if (!thunar_file_launch (lp->data, screen, &derror))
        {
          /* tell the user that we were unable to launch the file specified on the cmdline */
          thunar_dialogs_show_error (screen, derror, _("Failed to open \"%s\""), thunar_file_get_display_name (lp->data));
          g_error_free (derror);
          break;
        }
    }

  /* release all files */
  thunar_file_list_free (file_list);

  return TRUE;

failure:
  g_set_error (error, derror->domain, derror->code, _("Failed to open \"%s\": %s"), filenames[n], derror->message);
  thunar_file_list_free (file_list);
  g_error_free (derror);
  return FALSE;
}



/**
 * thunar_application_copy_to:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                            gpointer           parent,
                            GList             *source_path_list,
                            GList             *target_path_list,
                            GClosure          *new_files_closure)
{
  g_return_if_fail (g_list_length (source_path_list) == g_list_length (target_path_list));
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "stock_folder-copy",
                             _("Copying files..."), thunar_vfs_copy_files,
                             source_path_list, target_path_list, new_files_closure);
}



/**
 * thunar_application_copy_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                              gpointer           parent,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (target_path != NULL);

  /* collect the target paths and launch the job */
  thunar_application_collect_and_launch (application, parent, "stock_folder-copy",
                                         _("Copying files..."), thunar_vfs_copy_files,
                                         source_path_list, target_path, new_files_closure);
}



/**
 * thunar_application_link_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                              gpointer           parent,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (target_path != NULL);

  /* collect the target paths and launch the job */
  thunar_application_collect_and_launch (application, parent, "stock_link",
                                         _("Creating symbolic links..."),
                                         thunar_vfs_link_files, source_path_list,
                                         target_path, new_files_closure);
}



/**
 * thunar_application_move_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                              gpointer           parent,
                              GList             *source_path_list,
                              ThunarVfsPath     *target_path,
                              GClosure          *new_files_closure)
{
  const gchar *icon;
  const gchar *text;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (target_path != NULL);

  /* determine the appropriate message text and the icon based on the target_path */
  if (thunar_vfs_path_get_scheme (target_path) == THUNAR_VFS_PATH_SCHEME_TRASH)
    {
      icon = "gnome-fs-trash-full";
      text = _("Moving files into the trash...");
    }
  else
    {
      icon = "stock_folder-move";
      text = _("Moving files...");
    }

  /* launch the operation */
  thunar_application_collect_and_launch (application, parent, icon, text,
                                         thunar_vfs_move_files, source_path_list,
                                         target_path, new_files_closure);
}



static ThunarVfsJob*
unlink_stub (GList   *source_path_list,
             GList   *target_path_list,
             GError **error)
{
  return thunar_vfs_unlink_files (source_path_list, error);
}



/**
 * thunar_application_unlink_files:
 * @application : a #ThunarApplication.
 * @parent      : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list   : the list of #ThunarFile<!---->s that should be deleted.
 *
 * Deletes all files in the @file_list and takes care of all user interaction.
 *
 * If the user pressed the shift key while triggering the delete action,
 * the files will be deleted permanently (after confirming the action),
 * otherwise the files will be moved to the trash.
 **/
void
thunar_application_unlink_files (ThunarApplication *application,
                                 gpointer           parent,
                                 GList             *file_list)
{
  GdkModifierType state;
  ThunarVfsPath  *path;
  GtkWidget      *dialog;
  GtkWindow      *window;
  GdkScreen      *screen;
  gboolean        permanently;
  GList          *path_list = NULL;
  GList          *lp;
  gchar          *message;
  guint           n_path_list = 0;
  gint            response;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* check if we should permanently delete the files (user holds shift) */
  permanently = (gtk_get_current_event_state (&state) && (state & GDK_SHIFT_MASK) != 0);

  /* determine the paths for the files */
  for (lp = g_list_last (file_list); lp != NULL; lp = lp->prev, ++n_path_list)
    {
      /* prepend the path to the path list */
      path_list = thunar_vfs_path_list_prepend (path_list, thunar_file_get_path (lp->data));

      /* check if the file is in the trash already */
      if (thunar_file_is_trashed (lp->data))
        permanently = TRUE;
    }

  /* nothing to do if we don't have any paths */
  if (G_UNLIKELY (n_path_list == 0))
    return;

  /* ask the user to confirm if deleting permanently */
  if (G_UNLIKELY (permanently))
    {
      /* parse the parent pointer */
      screen = thunar_util_parse_parent (parent, &window);

      /* generate the question to confirm the delete operation */
      if (G_LIKELY (n_path_list == 1))
        {
          message = g_strdup_printf (_("Are you sure that you want to\npermanently delete \"%s\"?"),
                                     thunar_file_get_display_name (THUNAR_FILE (file_list->data)));
        }
      else
        {
          message = g_strdup_printf (ngettext ("Are you sure that you want to permanently\ndelete the selected file?",
                                               "Are you sure that you want to permanently\ndelete the %u selected files?",
                                               n_path_list),
                                     n_path_list);
        }

      /* ask the user to confirm the delete operation */
      dialog = gtk_message_dialog_new (window,
                                       GTK_DIALOG_MODAL
                                       | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       "%s", message);
      if (G_UNLIKELY (window == NULL && screen != NULL))
        gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_DELETE, GTK_RESPONSE_YES,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("If you delete a file, it is permanently lost."));
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (message);

      /* perform the delete operation */
      if (G_LIKELY (response == GTK_RESPONSE_YES))
        {
          /* launch the "Delete" operation */
          thunar_application_launch (application, parent, "stock_delete",
                                      _("Deleting files..."), unlink_stub,
                                      path_list, path_list, NULL);
        }
    }
  else
    {
      /* launch the "Move to Trash" operation */
      path = thunar_vfs_path_get_for_trash ();
      thunar_application_move_into (application, parent, path_list, path, NULL);
      thunar_vfs_path_unref (path);
    }

  /* release the path list */
  thunar_vfs_path_list_free (path_list);
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
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                          gpointer           parent,
                          GList             *path_list,
                          GClosure          *new_files_closure)
{
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "stock_new",
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
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
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
                          gpointer           parent,
                          GList             *path_list,
                          GClosure          *new_files_closure)
{
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "stock_folder",
                             _("Creating directories..."), mkdir_stub,
                             path_list, path_list, new_files_closure);
}



/**
 * thunar_application_empty_trash:
 * @application : a #ThunarApplication.
 * @parent      : the parent, which can either be the associated
 *                #GtkWidget, the screen on which display the
 *                progress and the confirmation, or %NULL.
 *
 * Deletes all files and folders in the Trash after asking
 * the user to confirm the operation.
 **/
void
thunar_application_empty_trash (ThunarApplication *application,
                                gpointer           parent)
{
  GtkWidget *dialog;
  GtkWindow *window;
  GdkScreen *screen;
  GList      path_list;
  gint       response;

  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, &window);

  /* ask the user to confirm the operation */
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL
                                   | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Remove all files and folders from the Trash?"));
  if (G_UNLIKELY (window == NULL && screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          _("_Empty Trash"), GTK_RESPONSE_YES,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you choose to empty the Trash, all items in it will be permanently lost. "
                                              "Please note that you can also delete them separately."));
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* check if the user confirmed */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* fake a path list with only the trash root (the root
       * folder itself will never be unlinked, so this is safe)
       */
      path_list.data = thunar_vfs_path_get_for_trash ();
      path_list.next = NULL;
      path_list.prev = NULL;

      /* launch the operation */
      thunar_application_launch (application, parent, "gnome-fs-trash-empty",
                                  _("Emptying the Trash..."),
                                  unlink_stub, &path_list, NULL, NULL);

      /* cleanup */
      thunar_vfs_path_unref (path_list.data);
    }
}



/**
 * thunar_application_restore_files:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @trash_file_list   : a #GList of #ThunarFile<!---->s in the trash.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Restores all #ThunarFile<!---->s in the @trash_file_list to their original
 * location.
 **/
void
thunar_application_restore_files (ThunarApplication *application,
                                  gpointer           parent,
                                  GList             *trash_file_list,
                                  GClosure          *new_files_closure)
{
  ThunarVfsPath *target_path;
  GtkWidget     *dialog;
  GtkWindow     *window;
  GdkScreen     *screen;
  GError        *err = NULL;
  GList         *source_path_list = NULL;
  GList         *target_path_list = NULL;
  GList         *lp;
  gchar         *original_path;
  gchar         *original_dir;
  gchar         *display_name;
  gint           response = GTK_RESPONSE_YES;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* determine the target paths for all files */
  for (lp = trash_file_list; err == NULL && lp != NULL && response == GTK_RESPONSE_YES; lp = lp->next)
    {
      /* determine the original path for the file */
      original_path = thunar_file_get_original_path (lp->data);
      if (G_UNLIKELY (original_path == NULL))
        {
          /* no OriginalPath, impossible to continue */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Failed to determine the original path for \"%s\""),
                       thunar_file_get_display_name (lp->data));
          break;
        }

      /* determine the target path for the OriginalPath */
      target_path = thunar_vfs_path_new (original_path, &err);
      if (G_UNLIKELY (target_path == NULL))
        {
          /* invalid OriginalPath, cannot continue */
          g_free (original_path);
          break;
        }

      /* determine the directory of the original path */
      original_dir = g_path_get_dirname (original_path);
      if (!g_file_test (original_dir, G_FILE_TEST_IS_DIR))
        {
          /* parse the parent pointer */
          screen = thunar_util_parse_parent (parent, &window);

          /* ask the user whether to recreate the original dir */
          display_name = g_filename_display_name (original_dir);
          dialog = gtk_message_dialog_new (window,
                                           GTK_DIALOG_MODAL
                                           | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           _("Create the folder \"%s\"?"),
                                           display_name);
          gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  _("C_reate Folder"), GTK_RESPONSE_YES,
                                  NULL);
          if (G_UNLIKELY (window == NULL && screen != NULL))
            gtk_window_set_screen (GTK_WINDOW (dialog), screen);
          gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    _("The folder \"%s\" does not exist anymore, but it is required to restore "
                                                      "the file \"%s\" from the trash. Do you want to create the folder again?"),
                                                    display_name, thunar_file_get_display_name (lp->data));
          response = gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);
          g_free (display_name);

          /* check if the user wants to recreate the folder */
          if (G_LIKELY (response == GTK_RESPONSE_YES))
            {
              /* try to recreate the folder */
              xfce_mkdirhier (original_dir, 0755, &err);
            }
        }

      /* check if we succeed and aren't cancelled */
      if (G_LIKELY (err == NULL && response == GTK_RESPONSE_YES))
        {
          /* add the source/target pair to our lists */
          source_path_list = thunar_vfs_path_list_append (source_path_list, thunar_file_get_path (lp->data));
          target_path_list = g_list_append (target_path_list, target_path);
        }
      else
        {
          /* release the target path */
          thunar_vfs_path_unref (target_path);
        }

      /* cleanup */
      g_free (original_path);
      g_free (original_dir);
    }

  /* check if an error occurred or the user cancelled */
  if (G_UNLIKELY (err != NULL))
    {
      /* display an error dialog */
      thunar_dialogs_show_error (parent, err, _("Failed to restore \"%s\""), thunar_file_get_display_name (lp->data));
      g_error_free (err);
    }
  else if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* launch the operation */
      thunar_application_launch (application, parent, "stock_folder-move",
                                 _("Restoring files..."), thunar_vfs_move_files,
                                 source_path_list, target_path_list, new_files_closure);
    }

  /* cleanup */
  thunar_vfs_path_list_free (target_path_list);
  thunar_vfs_path_list_free (source_path_list);
}


