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
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-progress-dialog.h>



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



static void     thunar_application_class_init             (ThunarApplicationClass *klass);
static void     thunar_application_init                   (ThunarApplication      *application);
static void     thunar_application_finalize               (GObject                *object);
static void     thunar_application_get_property           (GObject                *object,
                                                           guint                   prop_id,
                                                           GValue                 *value,
                                                           GParamSpec             *pspec);
static void     thunar_application_set_property           (GObject                *object,
                                                           guint                   prop_id,
                                                           const GValue           *value,
                                                           GParamSpec             *pspec);
static void     thunar_application_collect_and_launch     (ThunarApplication      *application,
                                                           GtkWidget              *widget,
                                                           const gchar            *icon_name,
                                                           const gchar            *title,
                                                           Launcher                launcher,
                                                           GList                  *source_path_list,
                                                           ThunarVfsPath          *target_path,
                                                           GClosure               *new_files_closure);
static void     thunar_application_launch                 (ThunarApplication      *application,
                                                           GtkWidget              *widget,
                                                           const gchar            *icon_name,
                                                           const gchar            *title,
                                                           Launcher                launcher,
                                                           GList                  *source_path_list,
                                                           GList                  *target_path_list,
                                                           GClosure               *new_files_closure);
static void     thunar_application_open_window_with_role  (ThunarApplication      *application,
                                                           const gchar            *role,
                                                           ThunarFile             *directory,
                                                           GdkScreen              *screen);
static void     thunar_application_save_yourself          (ExoXsessionClient      *xsession_client,
                                                           ThunarApplication      *application);
static void     thunar_application_window_destroyed       (GtkWidget              *window,
                                                           ThunarApplication      *application);
static gboolean thunar_application_show_dialogs           (gpointer                user_data);
static void     thunar_application_show_dialogs_destroy   (gpointer                user_data);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject __parent__;

  ThunarPreferences *preferences;
  GList             *windows;

  /* session management support */
  ExoXsessionClient *xsession_client;

  /* daemon mode */
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

  /* release our reference on the X session client */
  if (G_LIKELY (application->xsession_client != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (application->xsession_client), thunar_application_save_yourself, application);
      g_object_unref (G_OBJECT (application->xsession_client));
    }

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



static void
thunar_application_open_window_with_role (ThunarApplication *application,
                                          const gchar       *role,
                                          ThunarFile        *directory,
                                          GdkScreen         *screen)
{
  GtkWidget *window;
  GdkWindow *leader;

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

  /* register with session manager on first display
   * opened by Thunar. This is to ensure that we
   * get restarted by only _one_ session manager (the
   * one running on the first display).
   */
  if (G_UNLIKELY (application->xsession_client == NULL))
    {
      leader = gdk_display_get_default_group (gdk_screen_get_display (screen));
      application->xsession_client = exo_xsession_client_new_with_group (leader);
      g_signal_connect (G_OBJECT (application->xsession_client), "save-yourself",
                        G_CALLBACK (thunar_application_save_yourself), application);
    }
}



static void
thunar_application_save_yourself (ExoXsessionClient *xsession_client,
                                  ThunarApplication *application)
{
  ThunarFile *current_directory;
  GString    *session_data;
  gchar      *display_name;
  gchar     **oargv;
  gchar     **argv;
  gchar      *uri;
  GList      *lp;
  gint        argc = 1;

  g_return_if_fail (EXO_IS_XSESSION_CLIENT (xsession_client));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* generate the session data from the ThunarWindow's */
  session_data = g_string_sized_new (1024);
  for (lp = application->windows; lp != NULL; lp = lp->next)
    {
      /* ignore everything except ThunarWindows */
      if (!THUNAR_IS_WINDOW (lp->data))
        continue;

      /* determine the current directory for the window */
      current_directory = thunar_window_get_current_directory (THUNAR_WINDOW (lp->data));
      if (G_UNLIKELY (current_directory == NULL))
        continue;

      /* prepend the separator if not first */
      if (*session_data->str != '\0')
        g_string_append_c (session_data, '|');

      /* append the display name for the window */
      display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (lp->data)));
      g_string_append_printf (session_data, "%s;", display_name);
      g_free (display_name);

      /* append the role for the window */
      g_string_append_printf (session_data, "%s;", gtk_window_get_role (GTK_WINDOW (lp->data)));

      /* append the URI for the window */
      uri = thunar_vfs_path_dup_uri (thunar_file_get_path (current_directory));
      g_string_append (session_data, uri);
      g_free (uri);
    }

  /* allocate the argument vector */
  argv = g_new (gchar *, 5);

  /* determine the binary from the previous restart command */
  if (exo_xsession_client_get_restart_command (xsession_client, &oargv, NULL))
    {
      argv[0] = g_strdup (oargv[0]);
      g_strfreev (oargv);
    }
  else
    {
      argv[0] = g_strdup ("Thunar");
    }

  /* append --daemon option if we're in daemon mode */
  if (thunar_application_get_daemon (application))
    argv[argc++] = g_strdup ("--daemon");

  /* append --restore-session if we have any session data */
  if (G_LIKELY (*session_data->str != '\0'))
    {
      argv[argc++] = g_strdup ("--restore-session");
      argv[argc++] = g_strdup_printf ("[%s]", session_data->str);
    }

  /* NULL-terminate the argument vector */
  argv[argc] = NULL;

  /* setup the new restart command */
  exo_xsession_client_set_restart_command (xsession_client, argv, argc);

  /* cleanup */
  g_string_free (session_data, TRUE);
  g_strfreev (argv);
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
  gchar *role;

  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (THUNAR_IS_FILE (directory));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  /* generate a unique role for the new window (for session management) */
  role = g_strdup_printf ("Thunar-%x-%x", (guint) time (NULL), (guint) g_random_int ());
  thunar_application_open_window_with_role (application, role, directory, screen);
  g_free (role);
}



/**
 * thunar_application_process_filenames:
 * @application       : a #ThunarApplication.
 * @working_directory : the working directory relative to which the @filenames should
 *                      be interpreted.
 * @filenames         : a list of file:-URIs or filenames. If a filename is specified
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
  ThunarVfsPath *path;
  ThunarFile    *file;
  GError        *derror = NULL;
  gchar         *filename;
  GList         *file_list = NULL;
  GList         *lp;
  gint           n;

  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (working_directory != NULL, FALSE);
  g_return_val_if_fail (filenames != NULL, FALSE);
  g_return_val_if_fail (*filenames != NULL, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to process all filenames and convert them to the appropriate file objects */
  for (n = 0; filenames[n] != NULL; ++n)
    {
      /* check if the filename is an absolute path or a file:-URI */
      if (g_path_is_absolute (filenames[n]) || g_str_has_prefix (filenames[n], "file:"))
        {
          /* determine the path for the filename directly */
          path = thunar_vfs_path_new (filenames[n], &derror);
        }
      else
        {
          /* translate the filename into an absolute path first */
          filename = g_build_filename (working_directory, filenames[n], NULL);
          path = thunar_vfs_path_new (filename, &derror);
          g_free (filename);
        }

      /* determine the file for the path */
      file = (path != NULL) ? thunar_file_get_for_path (path, &derror) : NULL;

      /* release the path (if any) */
      if (G_LIKELY (path != NULL))
        thunar_vfs_path_unref (path);

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
 * thunar_application_restore_session:
 * @application  : a #ThunarApplication.
 * @session_data : the session data in the private encoding.
 * @error        : return location for errors or %NULL.
 *
 * Tries to restore a previous session from the provided @session_data.
 * Returns %TRUE if the session was restored successfully, otherwise %FALSE
 * will be returned and @error will be set.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 **/
gboolean
thunar_application_restore_session (ThunarApplication *application,
                                    const gchar       *session_data,
                                    GError           **error)
{
  ThunarVfsPath *path;
  ThunarFile    *file;
  GdkScreen     *screen;
  gchar        **pairs = NULL;
  gchar        **data = NULL;
  gchar        *tmp;
  gint          n;

  g_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (session_data != NULL, FALSE);

  /* make sure the session data is enclosed in [...] */
  if (!g_str_has_prefix (session_data, "[") || !g_str_has_suffix (session_data, "]"))
    goto error0;

  /* decode the session data pairs */
  tmp = g_strdup (session_data + 1);
  tmp[strlen (tmp) - 1] = '\0';
  pairs = g_strsplit (tmp, "|", -1);
  g_free (tmp);

  /* process all pairs */
  for (n = 0; pairs[n] != NULL; ++n)
    {
      /* split the pair's components */
      data = g_strsplit (pairs[n], ";", -1);

      /* verify that we have exactly 3 components */
      if (data[0] == NULL || data[1] == NULL || data[2] == NULL || data[3] != NULL)
        goto error0;

      /* try to open the screen */
      screen = thunar_gdk_screen_open (data[0], error);
      if (G_UNLIKELY (screen == NULL))
        goto error1;

      /* try to parse the URI */
      path = thunar_vfs_path_new (data[2], error);
      if (G_UNLIKELY (path == NULL))
        {
          g_object_unref (G_OBJECT (screen));
          goto error1;
        }

      /* determine the file for the path */
      file = thunar_file_get_for_path (path, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* tell the application to popup a new window */
          thunar_application_open_window_with_role (application, data[1], file, screen);

          /* release the file */
          g_object_unref (G_OBJECT (file));
        }

      /* reset the data array */
      g_strfreev (data);
      data = NULL;

      /* cleanup screen and path */
      g_object_unref (G_OBJECT (screen));
      thunar_vfs_path_unref (path);
    }

  /* release the pairs */
  g_strfreev (pairs);
  return TRUE;

error0:
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid session data"));
error1:
  g_strfreev (pairs);
  g_strfreev (data);
  return FALSE;
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


