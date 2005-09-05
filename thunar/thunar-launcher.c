/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-open-with-action.h>



enum
{
  PROP_0,
  PROP_ACTION_GROUP,
  PROP_SELECTED_FILES,
  PROP_WIDGET,
};

enum
{
  OPEN_DIRECTORY,
  LAST_SIGNAL,
};



static void thunar_launcher_class_init                (ThunarLauncherClass      *klass);
static void thunar_launcher_init                      (ThunarLauncher           *launcher);
static void thunar_launcher_dispose                   (GObject                  *object);
static void thunar_launcher_finalize                  (GObject                  *object);
static void thunar_launcher_get_property              (GObject                  *object,
                                                       guint                     prop_id,
                                                       GValue                   *value,
                                                       GParamSpec               *pspec);
static void thunar_launcher_set_property              (GObject                  *object,
                                                       guint                     prop_id,
                                                       const GValue             *value,
                                                       GParamSpec               *pspec);
static void thunar_launcher_execute_files             (ThunarLauncher           *launcher,
                                                       GList                    *files);
static void thunar_launcher_open_uris                 (ThunarVfsMimeApplication *application,
                                                       GList                    *uri_list,
                                                       ThunarLauncher           *launcher);
static void thunar_launcher_open_files                (ThunarLauncher           *launcher,
                                                       GList                    *files);
static void thunar_launcher_open_new_windows          (ThunarLauncher           *launcher,
                                                       GList                    *directories);
static void thunar_launcher_update                    (ThunarLauncher           *launcher);
static void thunar_launcher_action_open               (GtkAction                *action,
                                                       ThunarLauncher           *launcher);
static void thunar_launcher_action_open_application   (GtkAction                *action,
                                                       ThunarVfsMimeApplication *application,
                                                       GList                    *uri_list,
                                                       ThunarLauncher           *launcher);
static void thunar_launcher_action_open_in_new_window (GtkAction                *action,
                                                       ThunarLauncher           *launcher);



struct _ThunarLauncherClass
{
  GObjectClass __parent__;

  void (*open_directory) (ThunarLauncher *launcher,
                          ThunarFile     *directory);
};

struct _ThunarLauncher
{
  GObject __parent__;

  GtkActionGroup *action_group;
  GList          *selected_files;
  GtkWidget      *widget;

  GtkAction      *action_open;
  GtkAction      *action_open_in_new_window;
  GtkAction      *action_open_with;
};



static guint launcher_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarLauncher, thunar_launcher, G_TYPE_OBJECT);



static void
thunar_launcher_class_init (ThunarLauncherClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_launcher_dispose;
  gobject_class->finalize = thunar_launcher_finalize;
  gobject_class->get_property = thunar_launcher_get_property;
  gobject_class->set_property = thunar_launcher_set_property;

  /**
   * ThunarLauncher:action-group:
   *
   * The #GtkActionGroup to which this launcher should
   * add its #GtkActions.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACTION_GROUP,
                                   g_param_spec_object ("action-group",
                                                        _("Action group"),
                                                        _("Action group"),
                                                        GTK_TYPE_ACTION_GROUP,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarLauncher:selected-files:
   *
   * The list of selected #ThunarFile<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTED_FILES,
                                   g_param_spec_pointer ("selected-files",
                                                         _("Selected files"),
                                                         _("Selected files"),
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarLauncher:widget:
   *
   * The #GtkWidget with which this launcher is
   * associated.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        _("Widget"),
                                                        _("Widget"),
                                                        GTK_TYPE_WIDGET,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarLauncher::open-directory:
   * @launcher  : a #ThunarLauncher.
   * @directory : a #ThunarFile referring to the new directory.
   *
   * Invoked whenever the user requests to open a new @directory
   * based on the @launcher in the current window (if possible).
   **/
  launcher_signals[OPEN_DIRECTORY] =
    g_signal_new ("open-directory",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarLauncherClass, open_directory),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_TYPE_FILE);
}



static void
thunar_launcher_init (ThunarLauncher *launcher)
{
  /* the "Open" action */
  launcher->action_open = gtk_action_new ("open", _("_Open"), _("Open the selected items"), GTK_STOCK_OPEN);
  g_signal_connect (G_OBJECT (launcher->action_open), "activate", G_CALLBACK (thunar_launcher_action_open), launcher);

  /* the "Open in New Window" action */
  launcher->action_open_in_new_window = gtk_action_new ("open-in-new-window", _("Open in New Window"), _("Open the selected directories in new Thunar windows"), NULL);
  g_signal_connect (G_OBJECT (launcher->action_open_in_new_window), "activate", G_CALLBACK (thunar_launcher_action_open_in_new_window), launcher);

  /* the "Open with" action */
  launcher->action_open_with = thunar_open_with_action_new ("open-with", _("Open With"));
  g_signal_connect (G_OBJECT (launcher->action_open_with), "open-application", G_CALLBACK (thunar_launcher_action_open_application), launcher);
}



static void
thunar_launcher_dispose (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  thunar_launcher_set_action_group (launcher, NULL);
  thunar_launcher_set_selected_files (launcher, NULL);
  thunar_launcher_set_widget (launcher, NULL);

  (*G_OBJECT_CLASS (thunar_launcher_parent_class)->dispose) (object);
}



static void
thunar_launcher_finalize (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  /* drop all actions */
  g_object_unref (G_OBJECT (launcher->action_open));
  g_object_unref (G_OBJECT (launcher->action_open_in_new_window));
  g_object_unref (G_OBJECT (launcher->action_open_with));

  (*G_OBJECT_CLASS (thunar_launcher_parent_class)->finalize) (object);
}



static void
thunar_launcher_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      g_value_set_object (value, thunar_launcher_get_action_group (launcher));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_pointer (value, thunar_launcher_get_selected_files (launcher));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, thunar_launcher_get_widget (launcher));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_launcher_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      thunar_launcher_set_action_group (launcher, g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_launcher_set_selected_files (launcher, g_value_get_pointer (value));
      break;

    case PROP_WIDGET:
      thunar_launcher_set_widget (launcher, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_launcher_execute_files (ThunarLauncher *launcher,
                               GList          *files)
{
  ThunarFile *file;
  GtkWidget  *message;
  GtkWidget  *window;
  GdkScreen  *screen;
  GError     *error = NULL;
  GList      *lp;

  /* determine the screen on which to run the file(s) */
  screen = (launcher->widget != NULL)
         ? gtk_widget_get_screen (launcher->widget)
         : gdk_screen_get_default ();

  /* execute all selected files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);
      if (!thunar_file_execute (file, screen, NULL, &error))
        {
          window = (launcher->widget != NULL) ? gtk_widget_get_toplevel (launcher->widget) : NULL;
          message = gtk_message_dialog_new ((GtkWindow *) window,
                                            GTK_DIALOG_MODAL
                                            | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            _("Unable to execute file \"%s\"."),
                                            thunar_file_get_display_name (file));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
          break;
        }
    }
}



static void
thunar_launcher_open_uris (ThunarVfsMimeApplication *application,
                           GList                    *uri_list,
                           ThunarLauncher           *launcher)
{
  GdkScreen *screen;
  GtkWidget *window;
  GtkWidget *message;
  GError    *error = NULL;
  gchar     *name;
  gchar     *text;

  /* determine the screen on which to launch the application */
  screen = (launcher->widget != NULL) ? gtk_widget_get_screen (launcher->widget) : NULL;

  /* try to execute the application with the given URIs */
  if (!thunar_vfs_mime_application_exec (application, screen, uri_list, &error))
    {
      /* figure out the appropriate error message */
      if (g_list_length (uri_list) == 1)
        {
          name = thunar_vfs_uri_get_display_name (uri_list->data);
          text = g_strdup_printf (_("Unable to open \"%s\"."), name);
          g_free (name);
        }
      else
        {
          text = g_strdup_printf (_("Unable to open %d files."), g_list_length (uri_list));
        }

      /* display an error dialog */
      window = (launcher->widget != NULL) ? gtk_widget_get_toplevel (launcher->widget) : NULL;
      message = gtk_message_dialog_new ((GtkWindow *) window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", text);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
}



static void
thunar_launcher_open_files (ThunarLauncher *launcher,
                            GList          *files)
{
  ThunarVfsMimeApplication *application;
  ThunarVfsMimeDatabase    *database;
  ThunarVfsMimeInfo        *info;
  GHashTable               *applications;
  GtkWidget                *window;
  GtkWidget                *message;
  GList                    *uri_list;
  GList                    *lp;

  /* allocate a hash table to associate applications to URIs */
  applications = g_hash_table_new_full (thunar_vfs_mime_application_hash,
                                        thunar_vfs_mime_application_equal,
                                        thunar_vfs_mime_application_unref,
                                        (GDestroyNotify) thunar_vfs_uri_list_free);

  /* take a reference on the mime database */
  database = thunar_vfs_mime_database_get_default ();

  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* determine the MIME type for the file */
      info = thunar_file_get_mime_info (lp->data);

      /* determine the default application for the MIME type */
      application = thunar_vfs_mime_database_get_default_application (database, info);
      if (G_LIKELY (application != NULL))
        {
          /* check if we have that application already */
          uri_list = g_hash_table_lookup (applications, application);
          if (G_LIKELY (uri_list != NULL))
            {
              /* take a copy of the list as the old one will be dropped by the insert */
              uri_list = thunar_vfs_uri_list_copy (uri_list);
            }

          /* append our new URI to the list */
          uri_list = thunar_vfs_uri_list_append (uri_list, thunar_file_get_uri (lp->data));

          /* (re)insert the URI list for the application */
          g_hash_table_insert (applications, application, uri_list);
        }
      else
        {
          window = (launcher->widget != NULL) ? gtk_widget_get_toplevel (launcher->widget) : NULL;
          message = gtk_message_dialog_new ((GtkWindow *) window, 
                                            GTK_DIALOG_MODAL
                                            | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            _("No application available to open \"%s\".\n"
                                              "This will be fixed soon!"),
                                            thunar_file_get_display_name (lp->data));
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }

      /* cleanup */
      thunar_vfs_mime_info_unref (info);
    }

  /* run all collected applications */
  g_hash_table_foreach (applications, (GHFunc) thunar_launcher_open_uris, launcher);

  /* release the reference on the mime database */
  exo_object_unref (EXO_OBJECT (database));

  /* drop the applications hash table */
  g_hash_table_destroy (applications);
}



static void
thunar_launcher_open_new_windows (ThunarLauncher *launcher,
                                  GList          *directories)
{
  ThunarApplication *application;
  GtkWidget         *message;
  GtkWidget         *window;
  GdkScreen         *screen;
  GList             *lp;
  gint               response = GTK_RESPONSE_YES;
  gint               n;

  /* query the application object */
  application = thunar_application_get ();

  /* ask the user if we would open more than one new window */
  n = g_list_length (directories);
  if (G_UNLIKELY (n > 1))
    {
      /* open a message dialog */
      window = (launcher->widget != NULL) ? gtk_widget_get_toplevel (launcher->widget) : NULL;
      message = gtk_message_dialog_new ((GtkWindow *) window,
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_YES_NO,
                                        _("Are you sure you want to open all folders?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("This will open %d separate windows."), n);
      response = gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
    }

  /* open n new windows if the user approved it */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* determine the screen on which to open the new windows */
      screen = (launcher->widget != NULL) ? gtk_widget_get_screen (launcher->widget) : NULL;

      /* open all requested windows */
      for (lp = directories; lp != NULL; lp = lp->next)
        thunar_application_open_window (application, lp->data, screen);
    }

  /* release the application object */
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_update (ThunarLauncher *launcher)
{
  GList *items;
  GList *lp;
  gchar  text[256];
  gint   n_directories = 0;
  gint   n_executables = 0;
  gint   n_files = 0;
  gint   n_items;

  /* determine the number of selected items */
  items = launcher->selected_files;
  n_items = g_list_length (items);

  /* determine the number of files/directories */
  for (lp = items; lp != NULL; lp = lp->next)
    {
      if (thunar_file_is_directory (lp->data))
        {
          ++n_directories;
        }
      else
        {
          if (thunar_file_is_executable (lp->data))
            ++n_executables;
          ++n_files;
        }
    }

  if (G_UNLIKELY (n_directories == n_items))
    {
      /* we turn the "Open" label into "Open in n New Windows" if we have only directories */
      if (n_directories > 1)
        g_snprintf (text, sizeof (text), _("Open in %d New Windows"), n_directories);
      else
        g_strlcpy (text, _("_Open"), sizeof (text));

      g_object_set (G_OBJECT (launcher->action_open_in_new_window),
                    "sensitive", (n_directories > 0),
                    "visible", (n_directories <= 1),
                    NULL);

      g_object_set (G_OBJECT (launcher->action_open),
                    "label", text,
                    "sensitive", (n_directories > 0),
                    NULL);
    }
  else
    {
      /* we turn the "Open" label into "Execute" if we have only one executable file */
      if (G_UNLIKELY (n_executables == 1 && n_items == 1))
        g_object_set (G_OBJECT (launcher->action_open), "label", _("Execute"), NULL);
      else
        g_object_set (G_OBJECT (launcher->action_open), "label", _("Open"), NULL);

#if GTK_CHECK_VERSION(2,6,0)
      gtk_action_set_sensitive (launcher->action_open, (n_items > 0));
      gtk_action_set_visible (launcher->action_open_in_new_window, FALSE);
#else
      g_object_set (G_OBJECT (launcher->action_open), "sensitive", (n_items > 0), NULL);
      g_object_set (G_OBJECT (launcher->action_open_in_new_window), "visible", FALSE, NULL);
#endif
    }

  /* update the "Open With" action */
  thunar_open_with_action_set_file (THUNAR_OPEN_WITH_ACTION (launcher->action_open_with),
                                    (n_items == 1) ? THUNAR_FILE (items->data) : NULL);
}



static void
thunar_launcher_action_open (GtkAction      *action,
                             ThunarLauncher *launcher)
{
  GList *directories = NULL;
  GList *files = NULL;
  GList *lp;
  gint   n_selected_files;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* separate files and directories */
  n_selected_files = g_list_length (launcher->selected_files);
  for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
    {
      if (thunar_file_is_directory (lp->data))
        directories = g_list_append (directories, g_object_ref (lp->data));
      else
        files = g_list_append (files, g_object_ref (lp->data));
    }

  /* open the directories */
  if (directories != NULL)
    {
      /* open new windows if we have more than one selected item */
      if (G_UNLIKELY (n_selected_files == 1))
        g_signal_emit (G_OBJECT (launcher), launcher_signals[OPEN_DIRECTORY], 0, directories->data);
      else
        thunar_launcher_open_new_windows (launcher, directories);
      thunar_file_list_free (directories);
    }

  /* open the files */
  if (files != NULL)
    {
      if (g_list_length (files) == 1 && thunar_file_is_executable (files->data))
        thunar_launcher_execute_files (launcher, files);
      else
        thunar_launcher_open_files (launcher, files);
      thunar_file_list_free (files);
    }
}



static void
thunar_launcher_action_open_application (GtkAction                *action,
                                         ThunarVfsMimeApplication *application,
                                         GList                    *uri_list,
                                         ThunarLauncher           *launcher)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  g_return_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application));

  /* open the URIs (if any) using the given application */
  if (G_LIKELY (uri_list != NULL))
    thunar_launcher_open_uris (application, uri_list, launcher);
}



static void
thunar_launcher_action_open_in_new_window (GtkAction      *action,
                                           ThunarLauncher *launcher)
{
  GList *directories;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* take an additional reference on the launcher to make sure we don't get destroyed in the meantime */
  g_object_ref (G_OBJECT (launcher));

  /* open the directories */
  directories = thunar_file_list_copy (launcher->selected_files);
  thunar_launcher_open_new_windows (launcher, directories);
  thunar_file_list_free (directories);

  /* release the additional reference */
  g_object_unref (G_OBJECT (launcher));
}



/**
 * thunar_launcher_new:
 *
 * Allocates a new #ThunarLauncher instance and
 * returns it.
 *
 * Return value: the newly allocated #ThunarLauncher instance.
 **/
ThunarLauncher*
thunar_launcher_new (void)
{
  return g_object_new (THUNAR_TYPE_LAUNCHER, NULL);
}



/**
 * thunar_launcher_get_action_group:
 * @launcher : a #ThunarLauncher.
 *
 * Returns the #GtkActionGroup for the @launcher.
 *
 * Return value: the #GtkActionGroup for @launcher.
 **/
GtkActionGroup*
thunar_launcher_get_action_group (const ThunarLauncher *launcher)
{
  g_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  return launcher->action_group;
}



/**
 * thunar_launcher_set_action_group:
 * @launcher     : a #ThunarLauncher.
 * @action_group : a #GtkActionGroup or %NULL.
 *
 * Disconnects @launcher from any previously set
 * #GtkActionGroup. If @action_group is a valid
 * #GtkActionGroup, then @launcher will connect
 * its actions ("open", "open-in-new-window" and
 * "open-with") to @action_group.
 **/
void
thunar_launcher_set_action_group (ThunarLauncher *launcher,
                                  GtkActionGroup *action_group)
{
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  g_return_if_fail (action_group == NULL || GTK_IS_ACTION_GROUP (action_group));

  /* disconnect from the previous action group */
  if (G_UNLIKELY (launcher->action_group != NULL))
    {
      gtk_action_group_remove_action (launcher->action_group, launcher->action_open);
      gtk_action_group_remove_action (launcher->action_group, launcher->action_open_in_new_window);
      gtk_action_group_remove_action (launcher->action_group, launcher->action_open_with);
      g_object_unref (G_OBJECT (launcher->action_group));
    }

  /* activate the new action group */
  launcher->action_group = action_group;

  /* connect to the new action group */
  if (G_LIKELY (action_group != NULL))
    {
      g_object_ref (G_OBJECT (action_group));
      gtk_action_group_add_action_with_accel (action_group, launcher->action_open, "<control>O");
      gtk_action_group_add_action_with_accel (action_group, launcher->action_open_in_new_window, "<shift><control>O");
      gtk_action_group_add_action (action_group, launcher->action_open_with);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (launcher), "action-group");
}



/**
 * thunar_launcher_get_selected_files:
 * @launcher : a #ThunarLauncher.
 *
 * Returns the selected files currently set for @launcher.
 *
 * Return value: the list of selected #ThunarFile<!---->s
 *               for @launcher.
 **/
GList*
thunar_launcher_get_selected_files (const ThunarLauncher *launcher)
{
  g_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  return launcher->selected_files;
}



/**
 * thunar_launcher_set_selected_files:
 * @launcher       : a #ThunarLauncher.
 * @selected_files : a list of selected #ThunarFile<!---->s.
 *
 * Installs @selected_files for @launcher.
 **/
void
thunar_launcher_set_selected_files (ThunarLauncher *launcher,
                                    GList          *selected_files)
{
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* disconnect from the previously selected files */
  if (G_LIKELY (launcher->selected_files != NULL))
    thunar_file_list_free (launcher->selected_files);

  /* activate the new selected files list */
  launcher->selected_files = thunar_file_list_copy (selected_files);

  /* update the user interface */
  thunar_launcher_update (launcher);

  /* notify interested parties */
  g_object_notify (G_OBJECT (launcher), "selected-files");
}



/**
 * thunar_launcher_get_widget:
 * @launcher : a #ThunarLauncher.
 *
 * Returns the #GtkWidget with which @launcher is associated
 * currently or %NULL.
 *
 * Return value: the #GtkWidget associated with @launcher or %NULL.
 **/
GtkWidget*
thunar_launcher_get_widget (const ThunarLauncher *launcher)
{
  g_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  return launcher->widget;
}



/**
 * thunar_launcher_set_widget:
 * @launcher : a #ThunarLauncher.
 * @widget   : a #GtkWidget or %NULL.
 *
 * Tells @launcher to use @widget to determine the screen on
 * which to launch applications and such.
 **/
void
thunar_launcher_set_widget (ThunarLauncher *launcher,
                            GtkWidget      *widget)
{
  g_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  /* verify that the widgets differ */
  if (G_UNLIKELY (launcher->widget == widget))
    return;

  /* disconnect from the previously set widget */
  if (G_UNLIKELY (launcher->widget != NULL))
    g_object_unref (G_OBJECT (launcher->widget));

  /* activate the new widget */
  launcher->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    g_object_ref (G_OBJECT (widget));
  
  /* notify listeners */
  g_object_notify (G_OBJECT (launcher), "widget");
}

