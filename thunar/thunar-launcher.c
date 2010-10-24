/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-launcher-ui.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-sendto-model.h>
#include <thunar/thunar-stock.h>



typedef struct _ThunarLauncherMountData ThunarLauncherMountData;
typedef struct _ThunarLauncherPokeData ThunarLauncherPokeData;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_UI_MANAGER,
  PROP_WIDGET,
};



static void                    thunar_launcher_component_init             (ThunarComponentIface     *iface);
static void                    thunar_launcher_navigator_init             (ThunarNavigatorIface     *iface);
static void                    thunar_launcher_dispose                    (GObject                  *object);
static void                    thunar_launcher_finalize                   (GObject                  *object);
static void                    thunar_launcher_get_property               (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           GValue                   *value,
                                                                           GParamSpec               *pspec);
static void                    thunar_launcher_set_property               (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           const GValue             *value,
                                                                           GParamSpec               *pspec);
static ThunarFile             *thunar_launcher_get_current_directory      (ThunarNavigator          *navigator);
static void                    thunar_launcher_set_current_directory      (ThunarNavigator          *navigator,
                                                                           ThunarFile               *current_directory);
static GList                  *thunar_launcher_get_selected_files         (ThunarComponent          *component);
static void                    thunar_launcher_set_selected_files         (ThunarComponent          *component,
                                                                           GList                    *selected_files);
static GtkUIManager           *thunar_launcher_get_ui_manager             (ThunarComponent          *component);
static void                    thunar_launcher_set_ui_manager             (ThunarComponent          *component,
                                                                           GtkUIManager             *ui_manager);
static void                    thunar_launcher_execute_files              (ThunarLauncher           *launcher,
                                                                           GList                    *files);
static void                    thunar_launcher_open_files                 (ThunarLauncher           *launcher,
                                                                           GList                    *files);
static void                    thunar_launcher_open_paths                 (GAppInfo                 *app_info,
                                                                           GList                    *file_list,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_open_windows               (ThunarLauncher           *launcher,
                                                                           GList                    *directories);
static void                    thunar_launcher_update                     (ThunarLauncher           *launcher);
static void                    thunar_launcher_action_open                (GtkAction                *action,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_action_open_with_other     (GtkAction                *action,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_action_open_in_new_window  (GtkAction                *action,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_action_sendto_desktop      (GtkAction                *action,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_action_sendto_volume       (GtkAction                *action,
                                                                           ThunarLauncher           *launcher);
static void                    thunar_launcher_widget_destroyed           (ThunarLauncher           *launcher,
                                                                           GtkWidget                *widget);
static gboolean                thunar_launcher_sendto_idle                (gpointer                  user_data);
static void                    thunar_launcher_sendto_idle_destroy        (gpointer                  user_data);
static void                    thunar_launcher_mount_data_free            (ThunarLauncherMountData  *data);
static void                    thunar_launcher_poke_files                 (ThunarLauncher           *launcher,
                                                                           ThunarLauncherPokeData   *poke_data);
static void                    thunar_launcher_poke_files_finish          (ThunarBrowser            *browser,
                                                                           ThunarFile               *file,
                                                                           ThunarFile               *target_file,
                                                                           GError                   *error,
                                                                           gpointer                  user_data);
static ThunarLauncherPokeData *thunar_launcher_poke_data_new              (GList                    *files);
static void                    thunar_launcher_poke_data_free             (ThunarLauncherPokeData   *data);



struct _ThunarLauncherClass
{
  GObjectClass __parent__;
};

struct _ThunarLauncher
{
  GObject __parent__;

  ThunarFile             *current_directory;
  GList                  *selected_files;

  GtkIconFactory         *icon_factory;
  GtkActionGroup         *action_group;
  GtkUIManager           *ui_manager;
  guint                   ui_merge_id;
  guint                   ui_addons_merge_id;

  GtkAction              *action_open;
  GtkAction              *action_open_with_other;
  GtkAction              *action_open_in_new_window;
  GtkAction              *action_open_with_other_in_menu;

  GtkWidget              *widget;

  GVolumeMonitor         *volume_monitor;
  ThunarSendtoModel      *sendto_model;
  gint                    sendto_idle_id;
};

struct _ThunarLauncherMountData
{
  ThunarLauncher *launcher;
  GList          *files;
};

struct _ThunarLauncherPokeData
{
  GList *files;
  GList *resolved_files;
};



static const GtkActionEntry action_entries[] =
{
  { "open", GTK_STOCK_OPEN, N_ ("_Open"), "<control>O", NULL, G_CALLBACK (thunar_launcher_action_open), },
  { "open-in-new-window", NULL, N_ ("Open in New Window"), "<control><shift>O", N_ ("Open the selected directory in a new window"), G_CALLBACK (thunar_launcher_action_open_in_new_window), },
  { "open-with-other", NULL, N_ ("Open With Other _Application..."), NULL, N_ ("Choose another application with which to open the selected file"), G_CALLBACK (thunar_launcher_action_open_with_other), },
  { "open-with-menu", NULL, N_ ("Open With"), NULL, NULL, NULL, },
  { "open-with-other-in-menu", NULL, N_ ("Open With Other _Application..."), NULL, N_ ("Choose another application with which to open the selected file"), G_CALLBACK (thunar_launcher_action_open_with_other), },
  { "sendto-desktop", THUNAR_STOCK_DESKTOP, "", NULL, NULL, G_CALLBACK (thunar_launcher_action_sendto_desktop), },
};

static GQuark thunar_launcher_handler_quark;



G_DEFINE_TYPE_WITH_CODE (ThunarLauncher, thunar_launcher, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_launcher_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_launcher_component_init))



static void
thunar_launcher_class_init (ThunarLauncherClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the "thunar-launcher-handler" quark */
  thunar_launcher_handler_quark = g_quark_from_static_string ("thunar-launcher-handler");
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_launcher_dispose;
  gobject_class->finalize = thunar_launcher_finalize;
  gobject_class->get_property = thunar_launcher_get_property;
  gobject_class->set_property = thunar_launcher_set_property;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* Override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_UI_MANAGER, "ui-manager");

  /**
   * ThunarLauncher:widget:
   *
   * The #GtkWidget with which this launcher is associated.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        "widget",
                                                        "widget",
                                                        GTK_TYPE_WIDGET,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_launcher_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_launcher_get_selected_files;
  iface->set_selected_files = thunar_launcher_set_selected_files;
  iface->get_ui_manager = thunar_launcher_get_ui_manager;
  iface->set_ui_manager = thunar_launcher_set_ui_manager;
}



static void
thunar_launcher_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_launcher_get_current_directory;
  iface->set_current_directory = thunar_launcher_set_current_directory;
}



static void
thunar_launcher_init (ThunarLauncher *launcher)
{
  /* setup the action group for the launcher actions */
  launcher->action_group = gtk_action_group_new ("ThunarLauncher");
  gtk_action_group_set_translation_domain (launcher->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (launcher->action_group, action_entries, G_N_ELEMENTS (action_entries), launcher);

  /* determine references to our actions */
  launcher->action_open = gtk_action_group_get_action (launcher->action_group, "open");
  launcher->action_open_with_other = gtk_action_group_get_action (launcher->action_group, "open-with-other");
  launcher->action_open_in_new_window = gtk_action_group_get_action (launcher->action_group, "open-in-new-window");
  launcher->action_open_with_other_in_menu = gtk_action_group_get_action (launcher->action_group, "open-with-other-in-menu");

  /* initialize and add our custom icon factory for the application/action icons */
  launcher->icon_factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (launcher->icon_factory);

  /* setup the "Send To" support */
  launcher->sendto_model = thunar_sendto_model_get_default ();

  /* the "Send To" menu also displays removable devices from the volume monitor */
  launcher->volume_monitor = g_volume_monitor_get ();
  g_signal_connect_swapped (launcher->volume_monitor, "volume-added", G_CALLBACK (thunar_launcher_update), launcher);
  g_signal_connect_swapped (launcher->volume_monitor, "volume-removed", G_CALLBACK (thunar_launcher_update), launcher);
}



static void
thunar_launcher_dispose (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  /* reset our properties */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (launcher), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (launcher), NULL);
  thunar_launcher_set_widget (THUNAR_LAUNCHER (launcher), NULL);

  /* disconnect from the currently selected files */
  thunar_file_list_free (launcher->selected_files);
  launcher->selected_files = NULL;

  (*G_OBJECT_CLASS (thunar_launcher_parent_class)->dispose) (object);
}



static void
thunar_launcher_finalize (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  /* be sure to cancel the sendto idle source */
  if (G_UNLIKELY (launcher->sendto_idle_id != 0))
    g_source_remove (launcher->sendto_idle_id);

  /* drop our custom icon factory for the application/action icons */
  gtk_icon_factory_remove_default (launcher->icon_factory);
  g_object_unref (launcher->icon_factory);

  /* release the reference on the action group */
  g_object_unref (launcher->action_group);

  /* disconnect from the volume monitor used for the "Send To" menu */
  g_signal_handlers_disconnect_by_func (launcher->volume_monitor, thunar_launcher_update, launcher);
  g_object_unref (launcher->volume_monitor);

  /* release the reference on the sendto model */
  g_object_unref (launcher->sendto_model);

  (*G_OBJECT_CLASS (thunar_launcher_parent_class)->finalize) (object);
}



static void
thunar_launcher_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_component_get_ui_manager (THUNAR_COMPONENT (object)));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, thunar_launcher_get_widget (THUNAR_LAUNCHER (object)));
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
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object), g_value_get_object (value));
      break;

    case PROP_WIDGET:
      thunar_launcher_set_widget (THUNAR_LAUNCHER (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_launcher_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_LAUNCHER (navigator)->current_directory;
}



static void
thunar_launcher_set_current_directory (ThunarNavigator *navigator,
                                       ThunarFile      *current_directory)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (navigator);

  /* disconnect from the previous directory */
  if (G_LIKELY (launcher->current_directory != NULL))
    g_object_unref (G_OBJECT (launcher->current_directory));

  /* activate the new directory */
  launcher->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (current_directory));

  /* notify listeners */
  g_object_notify (G_OBJECT (launcher), "current-directory");
}



static GList*
thunar_launcher_get_selected_files (ThunarComponent *component)
{
  return THUNAR_LAUNCHER (component)->selected_files;
}



static void
thunar_launcher_set_selected_files (ThunarComponent *component,
                                    GList           *selected_files)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (component);
  GList          *np;
  GList          *op;

  /* compare the old and the new list of selected files */
  for (np = selected_files, op = launcher->selected_files; np != NULL && op != NULL; np = np->next, op = op->next)
    if (G_UNLIKELY (np->data != op->data))
      break;

  /* check if the list of selected files really changed */
  if (G_UNLIKELY (np != NULL || op != NULL))
    {
      /* disconnect from the previously selected files */
      thunar_file_list_free (launcher->selected_files);

      /* connect to the new selected files list */
      launcher->selected_files = thunar_file_list_copy (selected_files);

      /* update the launcher actions */
      thunar_launcher_update (launcher);

      /* notify listeners */
      g_object_notify (G_OBJECT (launcher), "selected-files");
    }
}



static GtkUIManager*
thunar_launcher_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_LAUNCHER (component)->ui_manager;
}



static void
thunar_launcher_set_ui_manager (ThunarComponent *component,
                                GtkUIManager    *ui_manager)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (component);
  GError         *error = NULL;

  /* disconnect from the previous UI manager */
  if (G_UNLIKELY (launcher->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (launcher->ui_manager, launcher->action_group);

      /* unmerge our addons ui controls from the previous UI manager */
      if (G_LIKELY (launcher->ui_addons_merge_id != 0))
        {
          gtk_ui_manager_remove_ui (launcher->ui_manager, launcher->ui_addons_merge_id);
          launcher->ui_addons_merge_id = 0;
        }

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (launcher->ui_manager, launcher->ui_merge_id);

      /* drop the reference on the previous UI manager */
      g_object_unref (G_OBJECT (launcher->ui_manager));
    }

  /* activate the new UI manager */
  launcher->ui_manager = ui_manager;

  /* connect to the new UI manager */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, launcher->action_group, -1);

      /* merge our UI control items with the new manager */
      launcher->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_launcher_ui, thunar_launcher_ui_length, &error);
      if (G_UNLIKELY (launcher->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarLauncher menus: %s", error->message);
          g_error_free (error);
        }

      /* update the user interface */
      thunar_launcher_update (launcher);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (launcher), "ui-manager");
}



static void
thunar_launcher_execute_files (ThunarLauncher *launcher,
                               GList          *files)
{
  GdkScreen *screen;
  GError    *error = NULL;
  GFile     *working_directory;
  GList     *lp;

  /* determine the screen on which to run the file(s) */
  screen = (launcher->widget != NULL) ? gtk_widget_get_screen (launcher->widget) : NULL;

  /* execute all selected files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      working_directory = thunar_file_get_file (launcher->current_directory);

      if (!thunar_file_execute (lp->data, working_directory, screen, NULL, &error))
        {
          /* display an error message to the user */
          thunar_dialogs_show_error (launcher->widget, error, _("Failed to execute file \"%s\""), thunar_file_get_display_name (lp->data));
          g_error_free (error);
          break;
        }
    }
}



static void
thunar_launcher_open_files (ThunarLauncher *launcher,
                            GList          *files)
{
  GAppInfo   *app_info;
  GHashTable *applications;
  GList      *file_list;
  GList      *lp;

  /* allocate a hash table to associate applications to URIs */
  applications = g_hash_table_new_full (g_direct_hash,
                                        (GEqualFunc) g_app_info_equal,
                                        (GDestroyNotify) g_object_unref,
                                        (GDestroyNotify) thunar_g_file_list_free);

  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* determine the default application for the MIME type */
      app_info = thunar_file_get_default_handler (lp->data);

      /* check if we have an application here */
      if (G_LIKELY (app_info != NULL))
        {
          /* check if we have that application already */
          file_list = g_hash_table_lookup (applications, app_info);
          if (G_LIKELY (file_list != NULL))
            {
              /* take a copy of the list as the old one will be dropped by the insert */
              file_list = thunar_g_file_list_copy (file_list);
            }

          /* append our new URI to the list */
          file_list = thunar_g_file_list_append (file_list, thunar_file_get_file (lp->data));

          /* (re)insert the URI list for the application */
          g_hash_table_insert (applications, app_info, file_list);
        }
      else
        {
          /* display a chooser dialog for the file and stop */
          thunar_show_chooser_dialog (launcher->widget, lp->data, TRUE);
          break;
        }
    }

  /* run all collected applications */
  g_hash_table_foreach (applications, (GHFunc) thunar_launcher_open_paths, launcher);

  /* drop the applications hash table */
  g_hash_table_destroy (applications);
}



static void
thunar_launcher_open_paths (GAppInfo       *app_info,
                            GList          *path_list,
                            ThunarLauncher *launcher)
{
  GdkAppLaunchContext *context;
  GdkScreen           *screen;
  GError              *error = NULL;
  GFile               *working_directory = NULL;
  gchar               *message;
  gchar               *name;
  guint                n;

  /* determine the screen on which to launch the application */
  screen = (launcher->widget != NULL) ? gtk_widget_get_screen (launcher->widget) : NULL;

  /* create launch context */
  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_screen (context, screen);

  /* determine the working directory */
  if (launcher->current_directory != NULL)
    working_directory = thunar_file_get_file (launcher->current_directory);

  /* try to execute the application with the given URIs */
  if (!thunar_g_app_info_launch (app_info, working_directory, path_list, G_APP_LAUNCH_CONTEXT (context), &error))
    {
      /* figure out the appropriate error message */
      n = g_list_length (path_list);
      if (G_LIKELY (n == 1))
        {
          /* we can give a precise error message here */
          name = g_filename_display_name (g_file_get_basename (path_list->data));
          message = g_strdup_printf (_("Failed to open file \"%s\""), name);
          g_free (name);
        }
      else
        {
          /* we can just tell that n files failed to open */
          message = g_strdup_printf (ngettext ("Failed to open %d file", "Failed to open %d files", n), n);
        }

      /* display an error dialog to the user */
      thunar_dialogs_show_error (launcher->widget, error, "%s", message);
      g_error_free (error);
      g_free (message);
    }

  /* destroy the launch context */
  g_object_unref (context);
}



static void
thunar_launcher_open_windows (ThunarLauncher *launcher,
                              GList          *directories)
{
  ThunarApplication *application;
  GtkWidget         *dialog;
  GtkWidget         *window;
  GdkScreen         *screen;
  gchar             *label;
  GList             *lp;
  gint               response = GTK_RESPONSE_YES;
  gint               n;

  /* ask the user if we would open more than one new window */
  n = g_list_length (directories);
  if (G_UNLIKELY (n > 1))
    {
      /* open a message dialog */
      window = (launcher->widget != NULL) ? gtk_widget_get_toplevel (launcher->widget) : NULL;
      dialog = gtk_message_dialog_new ((GtkWindow *) window,
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       _("Are you sure you want to open all folders?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                ngettext ("This will open %d separate file manager window.",
                                                          "This will open %d separate file manager windows.",
                                                          n),
                                                n);
      label = g_strdup_printf (ngettext ("Open %d New Window", "Open %d New Windows", n), n);
      gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (GTK_DIALOG (dialog), label, GTK_RESPONSE_YES);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (label);
    }

  /* open n new windows if the user approved it */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* query the application object */
      application = thunar_application_get ();

      /* determine the screen on which to open the new windows */
      screen = (launcher->widget != NULL) ? gtk_widget_get_screen (launcher->widget) : NULL;

      /* open all requested windows */
      for (lp = directories; lp != NULL; lp = lp->next)
        thunar_application_open_window (application, lp->data, screen, NULL);

      /* release the application object */
      g_object_unref (G_OBJECT (application));
    }
}



static void
thunar_launcher_update (ThunarLauncher *launcher)
{
  const gchar  *context_menu_path;
  const gchar  *file_menu_path;
  GtkWidget    *menu_item;
  GtkWidget    *image;
  GtkAction    *action;
  gboolean      default_is_open_with_other = FALSE;
  GList        *applications;
  GList        *actions;
  GList        *lp;
  gchar        *tooltip;
  gchar        *label;
  gchar        *name;
  gchar        *ui_path;
  gint          n_directories = 0;
  gint          n_executables = 0;
  gint          n_regulars = 0;
  gint          n_selected_files = 0;
  gint          n;

  /* verify that we're connected to an UI manager */
  if (G_UNLIKELY (launcher->ui_manager == NULL))
    return;

  /* drop the previous addons ui controls from the UI manager */
  if (G_LIKELY (launcher->ui_addons_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (launcher->ui_manager, launcher->ui_addons_merge_id);
      gtk_ui_manager_ensure_update (launcher->ui_manager);
      launcher->ui_addons_merge_id = 0;
    }

  /* reset the application set for the "Open" action */
  g_object_set_qdata (G_OBJECT (launcher->action_open), thunar_launcher_handler_quark, NULL);

  /* determine the number of files/directories/executables */
  for (lp = launcher->selected_files; lp != NULL; lp = lp->next, ++n_selected_files)
    {
      if (thunar_file_is_directory (lp->data) 
          || thunar_file_is_shortcut (lp->data) 
          || thunar_file_is_mountable (lp->data))
        {
          ++n_directories;
        }
      else
        {
          if (thunar_file_is_executable (lp->data))
            ++n_executables;
          ++n_regulars;
        }
    }

  /* update the user interface depending on the current selection */
  if (G_LIKELY (n_selected_files == 0 || n_directories > 0))
    {
      /** CASE 1: nothing selected or atleast one directory in the selection
       **
       ** - "Open" and "Open in n New Windows" actions
       **/

      /* the "Open" action is "Open in n New Windows" if we have two or more directories */
      if (G_UNLIKELY (n_selected_files == n_directories && n_directories > 1))
        {
          /* turn "Open" into "Open in n New Windows" */
          label = g_strdup_printf (ngettext ("Open in %d New Window", "Open in %d New Windows", n_directories), n_directories);
          tooltip = g_strdup_printf (ngettext ("Open the selected directory in %d new window",
                                               "Open the selected directories in %d new windows",
                                               n_directories), n_directories);
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", label,
                        "sensitive", TRUE,
                        "tooltip", tooltip,
                        NULL);
          g_free (tooltip);
          g_free (label);
        }
      else
        {
          /* the "Open" action is sensitive if we have atleast one selected file,
           * the label is set to "Open in New Window" if we're not in a regular
           * view (i.e. current_directory is not set) and have only one directory
           * selected to reflect that this action will open a new window.
           */
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", (launcher->current_directory == NULL && n_directories == n_selected_files && n_directories == 1)
                                 ? _("_Open in New Window")
                                 : _("_Open"),
                        "sensitive", (n_selected_files > 0),
                        "tooltip", ngettext ("Open the selected file", "Open the selected files", n_selected_files),
                        NULL);
        }

      /* the "Open in New Window" action is visible if we have exactly one directory */
      g_object_set (G_OBJECT (launcher->action_open_in_new_window),
                    "sensitive", (n_directories == 1),
                    "visible", (n_directories == n_selected_files && n_selected_files <= 1 && launcher->current_directory != NULL),
                    NULL);

      /* hide the "Open With Other Application" actions */
      gtk_action_set_visible (launcher->action_open_with_other, FALSE);
      gtk_action_set_visible (launcher->action_open_with_other_in_menu, FALSE);
    }
  else
    {
      /** CASE 2: one or more file in the selection
       **
       ** - "Execute" action if all selected files are executable
       ** - No "Open in n New Windows" action
       **/

      /* drop all previous addon actions from the action group */
      actions = gtk_action_group_list_actions (launcher->action_group);
      for (lp = actions; lp != NULL; lp = lp->next)
        if (strncmp (gtk_action_get_name (lp->data), "thunar-launcher-addon-", 22) == 0)
          gtk_action_group_remove_action (launcher->action_group, lp->data);
      g_list_free (actions);

      /* allocate a new merge id from the UI manager */
      launcher->ui_addons_merge_id = gtk_ui_manager_new_merge_id (launcher->ui_manager);

      /* make the "Open" action sensitive */
      gtk_action_set_sensitive (launcher->action_open, TRUE);

      /* hide the "Open in n New Windows" action */
      gtk_action_set_visible (launcher->action_open_in_new_window, FALSE);

      /* determine the set of applications that work for all selected files */
      applications = thunar_file_list_get_applications (launcher->selected_files);

      /* reset the desktop actions list */
      actions = NULL;

      /* check if we have only executable files in the selection */
      if (G_UNLIKELY (n_executables == n_selected_files))
        {
          /* turn the "Open" action into "Execute" */
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", _("_Execute"),
                        "tooltip", ngettext ("Execute the selected file", "Execute the selected files", n_selected_files),
                        NULL);
        }
      else if (G_LIKELY (applications != NULL))
        {
          /* turn the "Open" action into "Open With DEFAULT" */
          label = g_strdup_printf (_("_Open With \"%s\""), g_app_info_get_name (applications->data));
          tooltip = g_strdup_printf (ngettext ("Use \"%s\" to open the selected file",
                                               "Use \"%s\" to open the selected files",
                                               n_selected_files), g_app_info_get_name (applications->data));
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", label,
                        "tooltip", tooltip,
                        NULL);
          g_free (tooltip);
          g_free (label);

          /* remember the default application for the "Open" action */
          g_object_set_qdata_full (G_OBJECT (launcher->action_open), thunar_launcher_handler_quark, applications->data, g_object_unref);

          /* FIXME Add the desktop actions for this application. 
           * Unfortunately this is not supported by GIO directly */

          /* drop the default application from the list */
          applications = g_list_remove (applications, applications->data);
        }
      else if (G_UNLIKELY (n_selected_files == 1))
        {
          /* turn the "Open" action into "Open With Other Application" */
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", _("_Open With Other Application..."),
                        "tooltip", _("Choose another application with which to open the selected file"),
                        NULL);
          default_is_open_with_other = TRUE;
        }
      else
        {
          /* we can only show a generic "Open" action */
          g_object_set (G_OBJECT (launcher->action_open),
                        "label", _("_Open With Default Applications"),
                        "tooltip", ngettext ("Open the selected file with the default application",
                                             "Open the selected files with the default applications", n_selected_files),
                        NULL);
        }

      /* place the other applications in the "Open With" submenu if we have more than 2 other applications, or the
       * default action for the file is "Execute", in which case the "Open With" actions aren't that relevant either
       */
      if (G_UNLIKELY (g_list_length (applications) > 2 || n_executables == n_selected_files))
        {
          /* determine the base paths for the actions */
          file_menu_path = "/main-menu/file-menu/placeholder-launcher/open-with-menu/placeholder-applications";
          context_menu_path = "/file-context-menu/placeholder-launcher/open-with-menu/placeholder-applications";

          /* show the "Open With Other Application" in the submenu and hide the toplevel one */
          gtk_action_set_visible (launcher->action_open_with_other, FALSE);
          gtk_action_set_visible (launcher->action_open_with_other_in_menu, (n_selected_files == 1));
        }
      else
        {
          /* determine the base paths for the actions */
          file_menu_path = "/main-menu/file-menu/placeholder-launcher/placeholder-applications";
          context_menu_path = "/file-context-menu/placeholder-launcher/placeholder-applications";

          /* add a separator if we have more than one additional application */
          if (G_LIKELY (applications != NULL))
            {
              /* add separator after the DEFAULT/execute action */
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     file_menu_path, "separator", NULL,
                                     GTK_UI_MANAGER_SEPARATOR, FALSE);
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     context_menu_path, "separator", NULL,
                                     GTK_UI_MANAGER_SEPARATOR, FALSE);
            }

          /* show the toplevel "Open With Other Application" (if not already done by the "Open" action) */
          gtk_action_set_visible (launcher->action_open_with_other, !default_is_open_with_other && (n_selected_files == 1));
          gtk_action_set_visible (launcher->action_open_with_other_in_menu, FALSE);
        }

      /* add actions for all remaining applications */
      if (G_LIKELY (applications != NULL))
        {
          /* process all applications and determine the desktop actions */
          for (lp = applications, n = 0; lp != NULL; lp = lp->next, ++n)
            {
              /* FIXME Determine the desktop actions for this application. 
               * Unfortunately this is not supported by GIO directly. */

              /* generate a unique label, unique id and tooltip for the application's action */
              name = g_strdup_printf ("thunar-launcher-addon-application%d-%p", n, launcher);
              label = g_strdup_printf (_("Open With \"%s\""), g_app_info_get_name (lp->data));
              tooltip = g_strdup_printf (ngettext ("Use \"%s\" to open the selected file",
                                                   "Use \"%s\" to open the selected files",
                                                   n_selected_files), g_app_info_get_name (lp->data));

              /* allocate a new action for the application */
              action = gtk_action_new (name, label, tooltip, NULL);
              gtk_action_group_add_action (launcher->action_group, action);
              g_object_set_qdata_full (G_OBJECT (action), thunar_launcher_handler_quark, lp->data, g_object_unref);
              g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_launcher_action_open), launcher);
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     file_menu_path, name, name,
                                     GTK_UI_MANAGER_MENUITEM, FALSE);
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     context_menu_path, name, name,
                                     GTK_UI_MANAGER_MENUITEM, FALSE);
              g_object_unref (G_OBJECT (action));

              /* FIXME There's no API for creating GtkActions using GIcon in GTK+ 2.14. A "gicon" property
               * has been added to GtkAction in GTK+ 2.16 though. For now, this hack will have to do: */

              ui_path = g_strconcat (file_menu_path, "/", name, NULL);
              menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
              image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
              gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
              g_free (ui_path);
              	
              ui_path = g_strconcat (context_menu_path, "/", name, NULL);
              menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
              image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
              gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
              g_free (ui_path);

              /* FIXME End of the hack */

              /* cleanup */
              g_free (tooltip);
              g_free (label);
              g_free (name);
            }

          /* cleanup */
          g_list_free (applications);
        }

      /* FIXME Add desktop actions here. Unfortunately they are not supported by 
       * GIO, so we'll have to roll our own thing here */
    }

  /* schedule an update of the "Send To" menu */
  if (G_LIKELY (launcher->sendto_idle_id == 0))
    {
      launcher->sendto_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_launcher_sendto_idle,
                                                  launcher, thunar_launcher_sendto_idle_destroy);
    }
}



static void
thunar_launcher_open_file (ThunarLauncher *launcher,
                           ThunarFile     *file)
{
  GList files;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
            
  files.data = file;
  files.next = NULL;
  files.prev = NULL;

  if (thunar_file_is_directory (file))
    {
      /* check if we're in a regular view (i.e. current_directory is set) */
      if (G_LIKELY (launcher->current_directory != NULL))
        {
          /* we want to open one directory, so just emit "change-directory" here */
          thunar_navigator_change_directory (THUNAR_NAVIGATOR (launcher), file);
        }
      else
        {
          /* open the selected directory in a new window */
          thunar_launcher_open_windows (launcher, &files);
        }
    }
  else
    {
      if (thunar_file_is_executable (file))
        {
          /* try to execute the file */
          thunar_launcher_execute_files (launcher, &files);
        }
      else
        {
          /* try to open the file using its default application */
          thunar_launcher_open_files (launcher, &files);
        }
    }
}



static void
thunar_launcher_poke_file_finish (ThunarBrowser *browser,
                                  ThunarFile    *file,
                                  ThunarFile    *target_file,
                                  GError        *error,
                                  gpointer       ignored)
{
  if (error == NULL)
    {
      thunar_launcher_open_file (THUNAR_LAUNCHER (browser), target_file);
    }
  else
    {
      thunar_dialogs_show_error (THUNAR_LAUNCHER (browser)->widget, error,
                                 _("Failed to open \"%s\""), 
                                 thunar_file_get_display_name (file));
    }
}



static void
thunar_launcher_poke_files (ThunarLauncher         *launcher,
                            ThunarLauncherPokeData *poke_data)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (poke_data != NULL);
  _thunar_return_if_fail (poke_data->files != NULL);

  thunar_browser_poke_file (THUNAR_BROWSER (launcher), poke_data->files->data,
                            launcher->widget, thunar_launcher_poke_files_finish, 
                            poke_data);
}



static void
thunar_launcher_poke_files_finish (ThunarBrowser *browser,
                                   ThunarFile    *file,
                                   ThunarFile    *target_file,
                                   GError        *error,
                                   gpointer       user_data)
{
  ThunarLauncherPokeData *poke_data = user_data;
  gboolean                executable = TRUE;
  GList                  *directories = NULL;
  GList                  *files = NULL;
  GList                  *lp;

  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (poke_data != NULL);
  _thunar_return_if_fail (poke_data->files != NULL);
  
  /* check if poking succeeded */
  if (error == NULL)
    {
      /* add the resolved file to the list of file to be opened/executed later */
      poke_data->resolved_files = g_list_prepend (poke_data->resolved_files, 
                                                  g_object_ref (target_file));
    }

  /* release and remove the just poked file from the list */
  g_object_unref (poke_data->files->data);
  poke_data->files = g_list_delete_link (poke_data->files, poke_data->files);

  if (poke_data->files == NULL)
    {
      /* separate files and directories in the selected files list */
      for (lp = poke_data->resolved_files; lp != NULL; lp = lp->next)
        {
          if (thunar_file_is_directory (lp->data))
            {
              /* add to our directory list */
              directories = g_list_prepend (directories, lp->data);
            }
          else
            {
              /* add to our file list */
              files = g_list_prepend (files, lp->data);

              /* check if the file is executable */
              executable = (executable && thunar_file_is_executable (lp->data));
            }
        }

      /* check if we have any directories to process */
      if (G_LIKELY (directories != NULL))
        {
          /* open new windows for all directories */
          thunar_launcher_open_windows (THUNAR_LAUNCHER (browser), directories);
          g_list_free (directories);
        }

      /* check if we have any files to process */
      if (G_LIKELY (files != NULL))
        {
          /* if all files are executable, we just run them here */
          if (G_UNLIKELY (executable))
            {
              /* try to execute all given files */
              thunar_launcher_execute_files (THUNAR_LAUNCHER (browser), files);
            }
          else
            {
              /* try to open all files using their default applications */
              thunar_launcher_open_files (THUNAR_LAUNCHER (browser), files);
            }

          /* cleanup */
          g_list_free (files);
        }

      /* free all files allocated for the poke data */
      thunar_launcher_poke_data_free (poke_data);
    }
  else
    {
      /* we need to continue this until all files have been resolved */
      thunar_launcher_poke_files (THUNAR_LAUNCHER (browser), poke_data);
    }
}



static void
thunar_launcher_action_open (GtkAction      *action,
                             ThunarLauncher *launcher)
{
  ThunarLauncherPokeData *poke_data;
  GAppInfo               *app_info;
  GList                  *selected_paths;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* check if we have a mime handler associated with the action */
  app_info = g_object_get_qdata (G_OBJECT (action), thunar_launcher_handler_quark);
  if (G_LIKELY (app_info != NULL))
    {
      /* try to open the selected files using the given application */
      selected_paths = thunar_file_list_to_thunar_g_file_list (launcher->selected_files);
      thunar_launcher_open_paths (app_info, selected_paths, launcher);
      thunar_g_file_list_free (selected_paths);
    }
  else if (g_list_length (launcher->selected_files) == 1)
    {
      thunar_browser_poke_file (THUNAR_BROWSER (launcher), 
                                launcher->selected_files->data, launcher->widget,
                                thunar_launcher_poke_file_finish, NULL);
    }
  else
    {
      /* resolve files one after another until none is left. Open/execute
       * the resolved files/directories when all this is done at a later 
       * stage */
      poke_data = thunar_launcher_poke_data_new (launcher->selected_files);
      thunar_launcher_poke_files (launcher, poke_data);
    }
}



static void
thunar_launcher_action_open_with_other (GtkAction      *action,
                                        ThunarLauncher *launcher)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* verify that we have atleast one selected file */
  if (G_LIKELY (launcher->selected_files != NULL))
    {
      /* popup the chooser dialog for the first selected file */
      thunar_show_chooser_dialog (launcher->widget, launcher->selected_files->data, TRUE);
    }
}



static void
thunar_launcher_action_open_in_new_window (GtkAction      *action,
                                           ThunarLauncher *launcher)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* open the selected directories in new windows */
  thunar_launcher_open_windows (launcher, launcher->selected_files);
}



static void
thunar_launcher_action_sendto_desktop (GtkAction      *action,
                                       ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GFile             *desktop_file;
  GList             *files;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* determine the source files */
  files = thunar_file_list_to_thunar_g_file_list (launcher->selected_files);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the file to the ~/Desktop folder */
  desktop_file = thunar_g_file_new_for_desktop ();

  /* launch the link job */
  application = thunar_application_get ();
  thunar_application_link_into (application, launcher->widget, files, desktop_file, NULL);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (desktop_file);
  thunar_g_file_list_free (files);
}



static ThunarLauncherMountData *
thunar_launcher_mount_data_new (ThunarLauncher *launcher,
                                GList          *files)
{
  ThunarLauncherMountData *data;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);

  data = g_slice_new0 (ThunarLauncherMountData);
  data->launcher = g_object_ref (launcher);
  data->files = thunar_g_file_list_copy (files);

  return data;
}



static void
thunar_launcher_mount_data_free (ThunarLauncherMountData *data)
{
  _thunar_return_if_fail (data != NULL);
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (data->launcher));

  g_object_unref (data->launcher);
  thunar_g_file_list_free (data->files);
  g_slice_free (ThunarLauncherMountData, data);
}



static ThunarLauncherPokeData *
thunar_launcher_poke_data_new (GList *files)
{
  ThunarLauncherPokeData *data;

  data = g_slice_new0 (ThunarLauncherPokeData);
  data->files = thunar_file_list_copy (files);
  data->resolved_files = NULL;

  return data;
}



static void
thunar_launcher_poke_data_free (ThunarLauncherPokeData *data)
{
  _thunar_return_if_fail (data != NULL);

  thunar_file_list_free (data->files);
  thunar_file_list_free (data->resolved_files);
  g_slice_free (ThunarLauncherPokeData, data);
}



static void
thunar_launcher_sendto_volume (ThunarLauncher *launcher,
                               GVolume        *volume,
                               GList          *files)
{
  ThunarApplication *application;
  GMount            *mount;
  GFile             *mount_point;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (G_IS_VOLUME (volume));

  if (!thunar_g_volume_is_mounted (volume))
    return;
  
  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      mount_point = g_mount_get_root (mount);
      
      /* copy the files onto the specified volume */
      application = thunar_application_get ();
      thunar_application_copy_into (application, launcher->widget, files, mount_point, NULL);
      g_object_unref (application);

      g_object_unref (mount_point);
      g_object_unref (mount);
    }
}



static void
thunar_launcher_sendto_mount_finish (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  ThunarLauncherMountData *data = user_data;
  GVolume                 *volume = G_VOLUME (object);
  GError                  *error = NULL;
  gchar                   *volume_name;

  _thunar_return_if_fail (G_IS_VOLUME (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (user_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (data->launcher));

  if (!g_volume_mount_finish (volume, result, &error))
    {
      /* tell the user that we were unable to mount the volume, which is 
       * required to send files to it */
      volume_name = g_volume_get_name (volume);
      thunar_dialogs_show_error (data->launcher->widget, error, _("Failed to mount \"%s\""), 
                                 volume_name);
      g_free (volume_name);

      g_error_free (error);
    }
  else
    {
      thunar_launcher_sendto_volume (data->launcher, volume, data->files);
    }

  thunar_launcher_mount_data_free (data);
}



static void
thunar_launcher_action_sendto_volume (GtkAction      *action,
                                      ThunarLauncher *launcher)
{
  ThunarLauncherMountData *data;
  GMountOperation         *mount_operation;
  GtkWidget               *window;
  GVolume                 *volume;
  GList                   *files;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* determine the source paths */
  files = thunar_file_list_to_thunar_g_file_list (launcher->selected_files);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the volume to which to send */
  volume = g_object_get_qdata (G_OBJECT (action), thunar_launcher_handler_quark);
  if (G_UNLIKELY (volume == NULL))
    return;

  /* make sure to mount the volume first, if it's not already mounted */
  if (!thunar_g_volume_is_mounted (volume))
    {
      /* determine the toplevel window */
      window = gtk_widget_get_toplevel (launcher->widget);

      /* allocate mount data */
      data = thunar_launcher_mount_data_new (launcher, files);

      /* allocate a GTK+ mount operation */
      mount_operation = gtk_mount_operation_new (GTK_WINDOW (window));

      /* try to mount the volume and later start sending the files */
      g_volume_mount (volume, G_MOUNT_MOUNT_NONE, mount_operation, NULL,
                      thunar_launcher_sendto_mount_finish, data);
    }
  else
    {
      thunar_launcher_sendto_volume (launcher, volume, files);
    }

  /* cleanup */
  thunar_g_file_list_free (files);
}



static void
thunar_launcher_widget_destroyed (ThunarLauncher *launcher,
                                  GtkWidget      *widget)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (launcher->widget == widget);
  _thunar_return_if_fail (GTK_IS_WIDGET (widget));

  /* just reset the widget property for the launcher */
  thunar_launcher_set_widget (launcher, NULL);
}



static gboolean
thunar_launcher_sendto_idle (gpointer user_data)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (user_data);
  const gchar    *label;
  GtkAction      *action;
  GtkWidget      *image;
  GtkWidget      *menu_item;
  gboolean        linkable = TRUE;
  GIcon          *icon;
  GList          *handlers;
  GList          *volumes;
  GList          *lp;
  gchar          *name;
  gchar          *tooltip;
  gchar          *ui_path;
  gchar          *volume_name;
  gint            n_selected_files;
  gint            n = 0;

  /* verify that we have an UI manager */
  if (launcher->ui_manager == NULL)
    return FALSE;

  GDK_THREADS_ENTER ();

  /* determine the number of selected files and check whether atleast one of these
   * files is located in the trash (to en-/disable the "sendto-desktop" action).
   */
  for (lp = launcher->selected_files, n_selected_files = 0; lp != NULL; lp = lp->next, ++n_selected_files)
    {
      /* check if this file is in trash */
      if (G_UNLIKELY (linkable))
        linkable = !thunar_file_is_trashed (lp->data);
    }

  /* update the "Desktop (Create Link)" sendto action */
  action = gtk_action_group_get_action (launcher->action_group, "sendto-desktop");
  g_object_set (G_OBJECT (action),
                "label", ngettext ("Desktop (Create Link)", "Desktop (Create Links)", n_selected_files),
                "tooltip", ngettext ("Create a link to the selected file on the desktop",
                                     "Create links to the selected files on the desktop",
                                     n_selected_files),
                "visible", (linkable && n_selected_files > 0),
                NULL);

  /* re-add the content to "Send To" if we have any files */
  if (G_LIKELY (n_selected_files > 0))
    {
      /* drop all previous sendto actions from the action group */
      handlers = gtk_action_group_list_actions (launcher->action_group);
      for (lp = handlers; lp != NULL; lp = lp->next)
        if (strncmp (gtk_action_get_name (lp->data), "thunar-launcher-sendto", 22) == 0)
          gtk_action_group_remove_action (launcher->action_group, lp->data);
      g_list_free (handlers);

      /* allocate a new merge id from the UI manager (if not already done) */
      if (G_UNLIKELY (launcher->ui_addons_merge_id == 0))
        launcher->ui_addons_merge_id = gtk_ui_manager_new_merge_id (launcher->ui_manager);

      /* determine the currently active volumes */
      volumes = g_volume_monitor_get_volumes (launcher->volume_monitor);

      /* add removable (and writable) drives and media */
      for (lp = volumes; lp != NULL; lp = lp->next, ++n)
        {
          /* skip non-removable or disc media (CD-ROMs aren't writable by Thunar) */
          /* TODO skip non-writable volumes like CD-ROMs here */
          if (!thunar_g_volume_is_removable (lp->data))
            {
              g_object_unref (lp->data);
              continue;
            }

          /* generate a unique name and tooltip for the volume */
          volume_name = g_volume_get_name (lp->data);
          name = g_strdup_printf ("thunar-launcher-sendto%d-%p", n, launcher);
          tooltip = g_strdup_printf (ngettext ("Send the selected file to \"%s\"",
                                               "Send the selected files to \"%s\"",
                                               n_selected_files), volume_name);

          /* allocate a new action for the volume */
          action = gtk_action_new (name, volume_name, tooltip, NULL);
          g_object_set_qdata_full (G_OBJECT (action), thunar_launcher_handler_quark, lp->data, g_object_unref);
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_launcher_action_sendto_volume), launcher);
          gtk_action_group_add_action (launcher->action_group, action);
          gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                 "/main-menu/file-menu/sendto-menu/placeholder-sendto-actions",
                                 name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
          gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                 "/file-context-menu/sendto-menu/placeholder-sendto-actions",
                                 name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
          g_object_unref (action);

          /* FIXME There's no API for creating GtkActions using GIcon in GTK+ 2.14. A "gicon" property
           * has been added to GtkAction in GTK+ 2.16 though. For now, this hack will have to do: */

          ui_path = g_strconcat ("/main-menu/file-menu/sendto-menu/placeholder-sendto-actions/", name, NULL);
          menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
          image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
          icon = g_volume_get_icon (lp->data);
          gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
          g_object_unref (icon);
          g_free (ui_path);
          	
          ui_path = g_strconcat ("/file-context-menu/sendto-menu/placeholder-sendto-actions/", name, NULL);
          menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
          image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
          icon = g_volume_get_icon (lp->data);
          gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
          g_object_unref (icon);
          g_free (ui_path);

          /* FIXME End of the hack */

          /* cleanup */
          g_free (name);
          g_free (tooltip);
          g_free (volume_name);
        }

      /* free the volumes list */
      g_list_free (volumes);

      /* determine the sendto handlers for the selected files */
      handlers = thunar_sendto_model_get_matching (launcher->sendto_model, launcher->selected_files);
      if (G_LIKELY (handlers != NULL))
        {
          /* add all handlers to the user interface */
          for (lp = handlers; lp != NULL; lp = lp->next, ++n)
            {
              /* generate a unique name and tooltip for the handler */
              label = g_app_info_get_name (lp->data);
              name = g_strdup_printf ("thunar-launcher-sendto%d-%p", n, launcher);
              tooltip = g_strdup_printf (ngettext ("Send the selected file to \"%s\"",
                                                   "Send the selected files to \"%s\"",
                                                   n_selected_files), label);

              /* allocate a new action for the handler */
              action = gtk_action_new (name, label, tooltip, NULL);
              g_object_set_qdata_full (G_OBJECT (action), thunar_launcher_handler_quark, lp->data, g_object_unref);
              g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_launcher_action_open), launcher);
              gtk_action_group_add_action (launcher->action_group, action);
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     "/main-menu/file-menu/sendto-menu/placeholder-sendto-actions",
                                     name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
              gtk_ui_manager_add_ui (launcher->ui_manager, launcher->ui_addons_merge_id,
                                     "/file-context-menu/sendto-menu/placeholder-sendto-actions",
                                     name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
              g_object_unref (G_OBJECT (action));

              /* FIXME There's no API for creating GtkActions using GIcon in GTK+ 2.14. A "gicon" property
               * has been added to GtkAction in GTK+ 2.16 though. For now, this hack will have to do: */

              ui_path = g_strconcat ("/main-menu/file-menu/sendto-menu/placeholder-sendto-actions/", name, NULL);
              menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
              image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
              gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
              g_free (ui_path);
              	
              ui_path = g_strconcat ("/file-context-menu/sendto-menu/placeholder-sendto-actions/", name, NULL);
              menu_item = gtk_ui_manager_get_widget (launcher->ui_manager, ui_path);
              image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menu_item));
              gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
              g_free (ui_path);

              /* FIXME End of the hack */

              /* cleanup */
              g_free (tooltip);
              g_free (name);
            }

          /* release the handler list */
          g_list_free (handlers);
        }
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_launcher_sendto_idle_destroy (gpointer user_data)
{
  THUNAR_LAUNCHER (user_data)->sendto_idle_id = 0;
}



/**
 * thunar_launcher_new:
 *
 * Allocates a new #ThunarLauncher instance.
 *
 * Return value: the newly allocated #ThunarLauncher.
 **/
ThunarLauncher*
thunar_launcher_new (void)
{
  return g_object_new (THUNAR_TYPE_LAUNCHER, NULL);
}



/**
 * thunar_launcher_get_widget:
 * @launcher : a #ThunarLauncher.
 *
 * Returns the #GtkWidget currently associated with @launcher.
 *
 * Return value: the widget associated with @launcher.
 **/
GtkWidget*
thunar_launcher_get_widget (const ThunarLauncher *launcher)
{
  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  return launcher->widget;
}



/**
 * thunar_launcher_set_widget:
 * @launcher : a #ThunarLauncher.
 * @widget   : a #GtkWidget or %NULL.
 *
 * Associates @launcher with @widget.
 **/
void
thunar_launcher_set_widget (ThunarLauncher *launcher,
                            GtkWidget      *widget)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  /* disconnect from the previous widget */
  if (G_UNLIKELY (launcher->widget != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (launcher->widget), thunar_launcher_widget_destroyed, launcher);
      g_object_unref (G_OBJECT (launcher->widget));
    }

  /* activate the new widget */
  launcher->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    {
      g_object_ref (G_OBJECT (widget));
      g_signal_connect_swapped (G_OBJECT (widget), "destroy", G_CALLBACK (thunar_launcher_widget_destroyed), launcher);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (launcher), "widget");
}



