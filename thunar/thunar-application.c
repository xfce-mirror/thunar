/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
 * Copyright (c) 2009      Jannis Pohlmann <jannis@xfce.org>
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
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-progress-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-util.h>



/* Prototype for the Thunar job launchers */
typedef ThunarJob *(*Launcher) (GList  *source_path_list,
                                GList  *target_path_list);



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
                                                             GList                  *source_file_list,
                                                             GFile                  *target_file,
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
static void       thunar_application_volman_device_added    (ThunarVfsVolumeManager *volume_manager,
                                                             const gchar            *udi,
                                                             ThunarApplication      *application);
static void       thunar_application_volman_device_removed  (ThunarVfsVolumeManager *volume_manager,
                                                             const gchar            *udi,
                                                             ThunarApplication      *application);
static void       thunar_application_volman_device_eject    (ThunarVfsVolumeManager *volume_manager,
                                                             const gchar            *udi,
                                                             ThunarApplication      *application);
static gboolean   thunar_application_volman_idle            (gpointer                user_data);
static void       thunar_application_volman_idle_destroy    (gpointer                user_data);
static void       thunar_application_volman_watch           (GPid                    pid,
                                                             gint                    status,
                                                             gpointer                user_data);
static void       thunar_application_volman_watch_destroy   (gpointer                user_data);
static gboolean   thunar_application_show_dialogs           (gpointer                user_data);
static void       thunar_application_show_dialogs_destroy   (gpointer                user_data);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject                __parent__;

  ThunarPreferences     *preferences;
  GList                 *windows;

  gboolean               daemon;

  guint                  show_dialogs_timer_id;

  /* the volume manager support */
  ThunarVfsVolumeManager *volman;
  GSList                 *volman_udis;
  guint                   volman_idle_id;
  guint                   volman_watch_id;
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

  /* check if we have a saved accel map */
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "Thunar/accels.scm");
  if (G_LIKELY (path != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (path);
      g_free (path);
    }

  /* connect to the volume manager */
  application->volman = thunar_vfs_volume_manager_get_default ();

  /* check if the volume manager supports HAL */
  if (g_signal_lookup ("device-added", G_OBJECT_TYPE (application->volman)) != 0)
    {
      /* connect the volume manager support callbacks (used to spawn thunar-volman appropriately) */
      g_signal_connect (G_OBJECT (application->volman), "device-added", G_CALLBACK (thunar_application_volman_device_added), application);
      g_signal_connect (G_OBJECT (application->volman), "device-removed", G_CALLBACK (thunar_application_volman_device_removed), application);
      g_signal_connect (G_OBJECT (application->volman), "device-eject", G_CALLBACK (thunar_application_volman_device_eject), application);
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

  /* cancel any pending volman watch source */
  if (G_UNLIKELY (application->volman_watch_id != 0))
    g_source_remove (application->volman_watch_id);
  
  /* cancel any pending volman idle source */
  if (G_UNLIKELY (application->volman_idle_id != 0))
    g_source_remove (application->volman_idle_id);

  /* drop all pending volume manager UDIs */
  g_slist_foreach (application->volman_udis, (GFunc) g_free, NULL);
  g_slist_free (application->volman_udis);

  /* disconnect from the volume manager */
  g_object_unref (G_OBJECT (application->volman));

  /* drop any running "show dialogs" timer */
  if (G_UNLIKELY (application->show_dialogs_timer_id != 0))
    g_source_remove (application->show_dialogs_timer_id);

  /* drop the open windows */
  for (lp = application->windows; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), G_CALLBACK (thunar_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (lp->data));
    }
  g_list_free (application->windows);

  /* disconnect from the preferences */
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
                                       GList             *source_file_list,
                                       GFile             *target_file,
                                       GClosure          *new_files_closure)
{
  GFile  *file;
  GError *err = NULL;
  GList  *target_file_list = NULL;
  GList  *lp;
  gchar  *basename;

  /* check if we have anything to operate on */
  if (G_UNLIKELY (source_file_list == NULL))
    return;

  /* generate the target path list */
  for (lp = g_list_last (source_file_list); err == NULL && lp != NULL; lp = lp->prev)
    {
      /* verify that we're not trying to collect a root node */
      if (G_UNLIKELY (g_file_is_root (lp->data)))
        {
          /* tell the user that we cannot perform the requested operation */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", g_strerror (EINVAL));
        }
      else
        {
          basename = g_file_get_basename (lp->data);
          file = g_file_resolve_relative_path (target_file, basename);
          g_free (basename);

          /* add to the target file list */
          target_file_list = g_file_list_prepend (target_file_list, file);
          g_object_unref (file);
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
                                 source_file_list, target_file_list, new_files_closure);
    }

  /* release the target path list */
  g_file_list_free (target_file_list);
}



static void
thunar_application_launch (ThunarApplication *application,
                           gpointer           parent,
                           const gchar       *icon_name,
                           const gchar       *title,
                           Launcher           launcher,
                           GList             *source_file_list,
                           GList             *target_file_list,
                           GClosure          *new_files_closure)
{
  ThunarJob *job;
  GtkWindow *window;
  GtkWidget *dialog;
  GdkScreen *screen;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, &window);

  /* try to allocate a new job for the operation */
  job = (*launcher) (source_file_list, target_file_list);
    
  /* connect the "new-files" closure (if any) */
  if (G_LIKELY (new_files_closure != NULL))
    g_signal_connect_closure (job, "new-files", new_files_closure, FALSE);

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
  if (G_LIKELY (application->show_dialogs_timer_id == 0))
    {
      application->show_dialogs_timer_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 750, thunar_application_show_dialogs,
                                                               application, thunar_application_show_dialogs_destroy);
    }

  /* drop our reference on the job */
  g_object_unref (job);
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
  _thunar_return_if_fail (GTK_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (g_list_find (application->windows, window) != NULL);

  application->windows = g_list_remove (application->windows, window);

  /* terminate the application if we don't have any more
   * windows and we are not in daemon mode.
   */
  if (G_UNLIKELY (application->windows == NULL && !application->daemon))
    gtk_main_quit ();
}



static void
thunar_application_volman_device_added (ThunarVfsVolumeManager *volume_manager,
                                        const gchar            *udi,
                                        ThunarApplication      *application)
{
  _thunar_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (volume_manager));
  _thunar_return_if_fail (application->volman == volume_manager);
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* schedule the UDI for processing (last come, first served) */
  application->volman_udis = g_slist_prepend (application->volman_udis, g_strdup (udi));

  /* check if there's currently no active or scheduled handler */
  if (G_LIKELY (application->volman_idle_id == 0 && application->volman_watch_id == 0))
    {
      /* schedule a new handler using the idle source, which invokes the handler */
      application->volman_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_application_volman_idle,
                                                     application, thunar_application_volman_idle_destroy);
    }
}



static void
thunar_application_volman_device_removed (ThunarVfsVolumeManager *volume_manager,
                                          const gchar            *udi,
                                          ThunarApplication      *application)
{
  GSList *lp;

  _thunar_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (volume_manager));
  _thunar_return_if_fail (application->volman == volume_manager);
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* check if this is one of the pending UDIs */
  for (lp = application->volman_udis; lp != NULL; lp = lp->next)
    if (G_UNLIKELY (strcmp (lp->data, udi) == 0))
      {
        /* release the UDI */
        g_free (lp->data);

        /* drop the UDI from the list of pending device UDIs */
        application->volman_udis = g_slist_delete_link (application->volman_udis, lp);
        break;
      }
}



static void
thunar_application_volman_device_eject (ThunarVfsVolumeManager *volume_manager,
                                        const gchar            *udi,
                                        ThunarApplication      *application)
{
  GdkScreen *screen;
  GError    *err = NULL;
  gchar     *argv[4];

  _thunar_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (volume_manager));
  _thunar_return_if_fail (application->volman == volume_manager);
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* generate the argument list for exo-eject */
  argv[0] = (gchar *) "exo-eject";
  argv[1] = (gchar *) "-h";
  argv[2] = (gchar *) udi;
  argv[3] = NULL;

  /* locate the currently active screen (the one with the pointer) */
  screen = thunar_gdk_screen_get_active ();

  /* try to spawn the volman on the active screen */
  if (!gdk_spawn_on_screen (screen, NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err))
    {
      /* failed to launch exo-eject, inform the user about this */
      thunar_dialogs_show_error (screen, err, _("Failed to execute \"%s\""), "exo-eject");
      g_error_free (err);
    }
  else
    {
      /* we most probably removed the device */
      thunar_application_volman_device_removed (volume_manager, udi, application);
    }
}



static gboolean
thunar_application_volman_idle (gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);
  GdkScreen         *screen;
  gboolean           misc_volume_management;
  GError            *err = NULL;
  gchar            **argv;
  GPid               pid;

  GDK_THREADS_ENTER ();

  /* check if volume management is enabled (otherwise, we don't spawn anything, but clear the list here) */
  g_object_get (G_OBJECT (application->preferences), "misc-volume-management", &misc_volume_management, NULL);
  if (G_LIKELY (misc_volume_management))
    {
      /* check if we don't already have a handler, and we have a pending UDI */
      if (application->volman_watch_id == 0 && application->volman_udis != NULL)
        {
          /* generate the argument list for the volman */
          argv = g_new (gchar *, 4);
          argv[0] = g_strdup ("thunar-volman");
          argv[1] = g_strdup ("--device-added");
          argv[2] = application->volman_udis->data;
          argv[3] = NULL;

          /* remove the first list item from the pending list */
          application->volman_udis = g_slist_delete_link (application->volman_udis, application->volman_udis);

          /* locate the currently active screen (the one with the pointer) */
          screen = thunar_gdk_screen_get_active ();

          /* try to spawn the volman on the active screen */
          if (gdk_spawn_on_screen (screen, NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, &err))
            {
              /* add a child watch for the volman handler */
              application->volman_watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid, thunar_application_volman_watch,
                                                                     application, thunar_application_volman_watch_destroy);
            }
          else
            {
              /* failed to spawn, tell the user, giving a hint to install the thunar-volman package */
              g_warning ("Failed to launch the volume manager (%s), make sure you have the \"thunar-volman\" package installed.", err->message);
              g_error_free (err);
            }

          /* cleanup */
          g_strfreev (argv);
        }

    }
  else
    {
      /* drop all pending HAL device UDIs */
      g_slist_foreach (application->volman_udis, (GFunc) g_free, NULL);
      g_slist_free (application->volman_udis);
      application->volman_udis = NULL;
    }

  GDK_THREADS_LEAVE ();

  /* keep the idle source alive as long as no handler is
   * active and we have pending UDIs that must be handled
   */
  return (application->volman_watch_id == 0
       && application->volman_udis != NULL);
}



static void
thunar_application_volman_idle_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->volman_idle_id = 0;
}



static void
thunar_application_volman_watch (GPid     pid,
                                 gint     status,
                                 gpointer user_data)
{
  ThunarApplication *application = THUNAR_APPLICATION (user_data);

  GDK_THREADS_ENTER ();

  /* check if the idle source isn't active, but we have pending UDIs */
  if (application->volman_idle_id == 0 && application->volman_udis != NULL)
    {
      /* schedule a new handler using the idle source, which invokes the handler */
      application->volman_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_application_volman_idle,
                                                     application, thunar_application_volman_idle_destroy);
    }

  /* be sure to close the pid handle */
  g_spawn_close_pid (pid);

  GDK_THREADS_LEAVE ();
}



static void
thunar_application_volman_watch_destroy (gpointer user_data)
{
  THUNAR_APPLICATION (user_data)->volman_watch_id = 0;
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
  THUNAR_APPLICATION (user_data)->show_dialogs_timer_id = 0;
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
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
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
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

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

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);

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
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
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
 * 
 * If the @window has no transient window, it will also create a
 * new #GtkWindowGroup for this window. This will make different
 * windows work independant (think gtk_dialog_run).
 **/
void
thunar_application_take_window (ThunarApplication *application,
                                GtkWindow         *window)
{
  GtkWindowGroup *group;

  _thunar_return_if_fail (GTK_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (g_list_find (application->windows, window) == NULL);

  /* only windows without a parent get a new window group */
  if (gtk_window_get_transient_for (window) == NULL)
    {
      group = gtk_window_group_new ();
      gtk_window_group_add_window (group, window);
      g_object_weak_ref (G_OBJECT (window), (GWeakNotify) g_object_unref, group);
    }

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

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), NULL);
  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), NULL);

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

  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (working_directory != NULL, FALSE);

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

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);
  _thunar_return_val_if_fail (working_directory != NULL, FALSE);
  _thunar_return_val_if_fail (filenames != NULL, FALSE);
  _thunar_return_val_if_fail (*filenames != NULL, FALSE);
  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
  /* tell the user that we were unable to launch the file specified on the cmdline */
  thunar_dialogs_show_error (screen, derror, _("Failed to open \"%s\""), filenames[n]);

  g_set_error (error, derror->domain, derror->code, _("Failed to open \"%s\": %s"), filenames[n], derror->message);
  thunar_file_list_free (file_list);
  g_error_free (derror);
  return FALSE;
}



/**
 * thunar_application_copy_to:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be copied.
 * @target_file_list  : the list of #GFile<!---->s where files should be copied to.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files from @source_file_list to their locations specified in
 * @target_file_list.
 *
 * @source_file_list and @target_file_list must be of the same length. 
 **/
void
thunar_application_copy_to (ThunarApplication *application,
                            gpointer           parent,
                            GList             *source_file_list,
                            GList             *target_file_list,
                            GClosure          *new_files_closure)
{
  _thunar_return_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list));
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "stock_folder-copy",
                             _("Copying files..."), thunar_io_jobs_copy_files,
                             source_file_list, target_file_list, new_files_closure);
}



/**
 * thunar_application_copy_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be copied.
 * @target_file       : the #GFile to the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user interaction.
 **/
void
thunar_application_copy_into (ThunarApplication *application,
                              gpointer           parent,
                              GList             *source_file_list,
                              GFile             *target_file,
                              GClosure          *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (G_IS_FILE (target_file));

  /* collect the target files and launch the job */
  thunar_application_collect_and_launch (application, parent, "stock_folder-copy",
                                         _("Copying files..."), thunar_io_jobs_copy_files,
                                         source_file_list, target_file, new_files_closure);
}



/**
 * thunar_application_link_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be symlinked.
 * @target_file       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Symlinks all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void
thunar_application_link_into (ThunarApplication *application,
                              gpointer           parent,
                              GList             *source_file_list,
                              GFile     *target_file,
                              GClosure          *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (G_IS_FILE (target_file));

  /* collect the target files and launch the job */
  thunar_application_collect_and_launch (application, parent, "stock_link",
                                         _("Creating symbolic links..."),
                                         thunar_io_jobs_link_files, source_file_list,
                                         target_file, new_files_closure);
}



/**
 * thunar_application_move_into:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be moved.
 * @target_file       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #ThunarVfsPath<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Moves all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void
thunar_application_move_into (ThunarApplication *application,
                              gpointer           parent,
                              GList             *source_file_list,
                              GFile             *target_file,
                              GClosure          *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (target_file != NULL);
  
  /* launch the appropriate operation depending on the target file */
  if (g_file_is_trashed (target_file))
    {
      thunar_application_trash (application, parent, source_file_list);
    }
  else
    {
      thunar_application_collect_and_launch (application, parent, 
                                             "stock_folder-move", _("Moving files..."), 
                                             thunar_io_jobs_move_files, 
                                             source_file_list, target_file, 
                                             new_files_closure);
    }
}



static ThunarJob*
unlink_stub (GList *source_path_list,
             GList *target_path_list)
{
  return thunar_io_jobs_unlink_files (source_path_list);
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
  GtkWidget      *dialog;
  GtkWindow      *window;
  GdkScreen      *screen;
  gboolean        permanently;
  GList          *path_list = NULL;
  GList          *lp;
  gchar          *message;
  guint           n_path_list = 0;
  gint            response;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* check if we should permanently delete the files (user holds shift) */
  permanently = (gtk_get_current_event_state (&state) && (state & GDK_SHIFT_MASK) != 0);

  /* determine the paths for the files */
  for (lp = g_list_last (file_list); lp != NULL; lp = lp->prev, ++n_path_list)
    {
      /* prepend the path to the path list */
      path_list = g_file_list_prepend (path_list, thunar_file_get_file (lp->data));
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
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
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
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), 
                                                _("If you delete a file, it is permanently lost."));
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
      thunar_application_trash (application, parent, path_list);
    }

  /* release the path list */
  g_file_list_free (path_list);
}



static ThunarJob *
trash_stub (GList *source_file_list,
            GList *target_file_list)
{
  return thunar_io_jobs_trash_files (source_file_list);
}



void
thunar_application_trash (ThunarApplication *application,
                          gpointer           parent,
                          GList             *file_list)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (file_list != NULL);

  thunar_application_launch (application, parent, "gnome-fs-trash-full", 
                             _("Moving files into the trash..."), trash_stub,
                             file_list, NULL, NULL);
}



static ThunarJob*
creat_stub (GList *source_path_list,
            GList *target_path_list)
{
  return thunar_io_jobs_create_files (source_path_list);
}



/**
 * thunar_application_creat:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of files to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates empty files for all #GFile<!---->s listed in @file_list. This
 * method takes care of all user interaction.
 **/
void
thunar_application_creat (ThunarApplication *application,
                          gpointer           parent,
                          GList             *file_list,
                          GClosure          *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  
  /* launch the operation */
  thunar_application_launch (application, parent, "stock_new",
                             _("Creating files..."), creat_stub,
                             file_list, file_list, new_files_closure);
}



static ThunarJob*
mkdir_stub (GList *source_path_list,
            GList *target_path_list)
{
  return thunar_io_jobs_make_directories (source_path_list);
}



/**
 * thunar_application_mkdir:
 * @application       : a #ThunarApplication.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of directories to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates all directories referenced by the @file_list. This method takes care of all user
 * interaction.
 **/
void
thunar_application_mkdir (ThunarApplication *application,
                          gpointer           parent,
                          GList             *file_list,
                          GClosure          *new_files_closure)
{
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  /* launch the operation */
  thunar_application_launch (application, parent, "stock_folder",
                             _("Creating directories..."), mkdir_stub,
                             file_list, file_list, new_files_closure);
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
  GList      file_list;
  gint       response;

  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));
  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

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
      file_list.data = g_file_new_for_trash ();
      file_list.next = NULL;
      file_list.prev = NULL;

      /* launch the operation */
      thunar_application_launch (application, parent, "gnome-fs-trash-empty",
                                 _("Emptying the Trash..."),
                                 unlink_stub, &file_list, NULL, NULL);

      /* cleanup */
      g_object_unref (file_list.data);
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
  const gchar *original_uri;
  GError      *err = NULL;
  GFile       *target_path;
  GList       *source_path_list = NULL;
  GList       *target_path_list = NULL;
  GList       *lp;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));
  _thunar_return_if_fail (THUNAR_IS_APPLICATION (application));

  for (lp = trash_file_list; lp != NULL; lp = lp->next)
    {
      original_uri = thunar_file_get_original_path (lp->data);
      if (G_UNLIKELY (original_uri == NULL))
        {
          /* no OriginalPath, impossible to continue */
          g_set_error (&err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Failed to determine the original path for \"%s\""),
                       thunar_file_get_display_name (lp->data));
          break;
        }

      /* TODO we need to distinguish between URIs and paths here */
      target_path = g_file_new_for_commandline_arg (original_uri);

      source_path_list = g_file_list_append (source_path_list, thunar_file_get_file (lp->data));
      target_path_list = g_file_list_append (target_path_list, target_path);

      g_object_unref (target_path);
    }

  if (G_UNLIKELY (err != NULL))
    {
      /* display an error dialog */
      thunar_dialogs_show_error (parent, err, _("Could not restore \"%s\""), 
                                 thunar_file_get_display_name (lp->data));
      g_error_free (err);
    }
  else
    {
      /* launch the operation */
      thunar_application_launch (application, parent, "stock_folder-move",
                                 _("Restoring files..."), thunar_io_jobs_restore_files,
                                 source_path_list, target_path_list, new_files_closure);
    }

  /* free path lists */
  g_file_list_free (source_path_list);
  g_file_list_free (target_path_list);
}


