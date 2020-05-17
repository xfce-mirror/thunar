/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-launcher-ui.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-sendto-model.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-window.h>

#include <libxfce4ui/libxfce4ui.h>



typedef struct _ThunarLauncherPokeData ThunarLauncherPokeData;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_UI_MANAGER,
  PROP_WIDGET,
  N_PROPERTIES
};



static void                    thunar_launcher_component_init             (ThunarComponentIface           *iface);
static void                    thunar_launcher_navigator_init             (ThunarNavigatorIface           *iface);
static void                    thunar_launcher_dispose                    (GObject                        *object);
static void                    thunar_launcher_finalize                   (GObject                        *object);
static void                    thunar_launcher_get_property               (GObject                        *object,
                                                                           guint                           prop_id,
                                                                           GValue                         *value,
                                                                           GParamSpec                     *pspec);
static void                    thunar_launcher_set_property               (GObject                        *object,
                                                                           guint                           prop_id,
                                                                           const GValue                   *value,
                                                                           GParamSpec                     *pspec);
static ThunarFile             *thunar_launcher_get_current_directory      (ThunarNavigator                *navigator);
static void                    thunar_launcher_set_current_directory      (ThunarNavigator                *navigator,
                                                                           ThunarFile                     *current_directory);
static GList                  *thunar_launcher_get_selected_files         (ThunarComponent                *component);
static void                    thunar_launcher_set_selected_files         (ThunarComponent                *component,
                                                                           GList                          *selected_files);
static GtkUIManager           *thunar_launcher_get_ui_manager             (ThunarComponent          *component);
static void                    thunar_launcher_set_ui_manager             (ThunarComponent          *component,
                                                                           GtkUIManager             *ui_manager);
static void                    thunar_launcher_execute_files              (ThunarLauncher                 *launcher,
                                                                           GList                          *files);
static void                    thunar_launcher_open_file                  (ThunarLauncher                 *launcher,
                                                                           ThunarFile                     *file,
                                                                           GAppInfo                       *application_to_use);
static void                    thunar_launcher_open_files                 (ThunarLauncher                 *launcher,
                                                                           GList                          *files,
                                                                           GAppInfo                       *application_to_use);
static void                    thunar_launcher_open_paths                 (GAppInfo                       *app_info,
                                                                           GList                          *file_list,
                                                                           ThunarLauncher                 *launcher);
static void                    thunar_launcher_open_windows               (ThunarLauncher                 *launcher,
                                                                           GList                          *directories);
static void                    thunar_launcher_poke_files                 (ThunarLauncher                 *launcher,
                                                                           GAppInfo                       *application_to_use,
                                                                           ThunarLauncherFolderOpenAction  folder_open_action);
static void                    thunar_launcher_poke_files_finish          (ThunarBrowser                  *browser,
                                                                           ThunarFile                     *file,
                                                                           ThunarFile                     *target_file,
                                                                           GError                         *error,
                                                                           gpointer                        user_data);
static ThunarLauncherPokeData *thunar_launcher_poke_data_new              (GList                          *files_to_poke,
                                                                           GAppInfo                       *application_to_use,
                                                                           ThunarLauncherFolderOpenAction  folder_open_action);
static void                    thunar_launcher_poke_data_free             (ThunarLauncherPokeData         *data);
static void                    thunar_launcher_widget_destroyed           (ThunarLauncher                 *launcher,
                                                                           GtkWidget                      *widget);
static void                    thunar_launcher_action_open                (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_open_in_new_tabs    (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_open_in_new_windows (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_open_with_other     (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_sendto_desktop      (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_properties          (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_sendto_device       (ThunarLauncher                 *launcher,
                                                                           GObject                        *object);
static void                    thunar_launcher_action_add_shortcuts       (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_make_link           (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_duplicate           (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_rename              (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_restore             (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_move_to_trash       (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_delete              (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_trash_delete        (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_empty_trash         (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_cut                 (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_copy                (ThunarLauncher                 *launcher);
static void                    thunar_launcher_action_paste_into_folder   (ThunarLauncher                 *launcher);
static void                    thunar_launcher_sendto_device              (ThunarLauncher                 *launcher,
                                                                           ThunarDevice                   *device);
static void                    thunar_launcher_sendto_mount_finish        (ThunarDevice                   *device,
                                                                           const GError                   *error,
                                                                           gpointer                        user_data);
static GtkWidget              *thunar_launcher_build_sendto_submenu       (ThunarLauncher                 *launcher);
static void                    thunar_launcher_menu_item_activated        (ThunarLauncher                 *launcher,
                                                                           GtkWidget                      *menu_item);
static void                    thunar_launcher_action_create_folder       (ThunarLauncher                 *launcher);
static GtkWidget              *thunar_launcher_append_paste_item          (ThunarLauncher                 *launcher,
                                                                           GtkMenuShell                   *menu,
                                                                           gboolean                        force);

struct _ThunarLauncherClass
{
  GObjectClass __parent__;
};

struct _ThunarLauncher
{
  GObject __parent__;

  ThunarFile             *current_directory;
  GList                  *selected_files;

  GtkActionGroup         *action_group;
  GtkUIManager           *ui_manager;
  guint                   ui_merge_id;
  guint                   ui_addons_merge_id;

  GtkAction              *action_open;
  GtkAction              *action_open_with_other;
  GtkAction              *action_open_in_new_window;
  GtkAction              *action_open_in_new_tab;
  GtkAction              *action_open_with_other_in_menu;

  gint                    n_selected_files;
  gint                    n_selected_directories;
  gint                    n_selected_executables;
  gint                    n_selected_regulars;
  gboolean                selection_trashable;
  gboolean                current_directory_selected;
  gboolean                single_folder_selected;

  ThunarFile             *single_folder;
  ThunarFile             *parent_folder;


  ThunarPreferences      *preferences;

  /* Parent widget which holds the instance of the launcher */
  GtkWidget              *widget;
};

static GQuark thunar_launcher_appinfo_quark;
static GQuark thunar_launcher_device_quark;
static GQuark thunar_launcher_file_quark;

struct _ThunarLauncherPokeData
{
  GList                          *files_to_poke;
  GList                          *files_poked;
  GAppInfo                       *application_to_use;
  ThunarLauncherFolderOpenAction  folder_open_action;
};



static const GtkActionEntry action_entries[] =
{
  { "open", "document-open", N_ ("_Open"), "<control>O", NULL, G_CALLBACK (NULL), },
  { "open-in-new-tab", NULL, N_ ("Open in New _Tab"), "<control><shift>P", NULL, G_CALLBACK (NULL), },
  { "open-in-new-window", NULL, N_ ("Open in New _Window"), "<control><shift>O", NULL, G_CALLBACK (NULL), },
  { "open-with-other", NULL, N_ ("Open With Other _Application..."), NULL, N_ ("Choose another application with which to open the selected file"), G_CALLBACK (NULL), },
  { "open-with-menu", NULL, N_ ("Open With"), NULL, NULL, NULL, },
  { "open-with-other-in-menu", NULL, N_ ("Open With Other _Application..."), NULL, N_ ("Choose another application with which to open the selected file"), G_CALLBACK (NULL), },
  { "sendto-desktop", "user-desktop", "", NULL, NULL, G_CALLBACK (NULL), },
};

static GParamSpec *launcher_props[N_PROPERTIES] = { NULL, };

static XfceGtkActionEntry thunar_launcher_action_entries[] =
{
    { THUNAR_LAUNCHER_ACTION_OPEN,             "<Actions>/ThunarLauncher/open",                    "<Primary>O",        XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            "document-open",        G_CALLBACK (thunar_launcher_action_open),                },
    { THUNAR_LAUNCHER_ACTION_EXECUTE,          "<Actions>/ThunarLauncher/execute",                 "",                  XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            "system-run",           G_CALLBACK (thunar_launcher_action_open),                },
    { THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB,      "<Actions>/ThunarLauncher/open-in-new-tab",         "<Primary><shift>P", XFCE_GTK_MENU_ITEM,       NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_open_in_new_tabs),    },
    { THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW,   "<Actions>/ThunarLauncher/open-in-new-window",      "<Primary><shift>O", XFCE_GTK_MENU_ITEM,       NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_open_in_new_windows), },
    { THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER,  "<Actions>/ThunarLauncher/open-with-other",         "",                  XFCE_GTK_MENU_ITEM,       N_ ("Open With Other _Application..."), N_ ("Choose another application with which to open the selected file"),                          NULL,                   G_CALLBACK (thunar_launcher_action_open_with_other),     },

    /* For backward compatibility the old accel paths are re-used. Currently not possible to automatically migrate to new accel paths. */
    /* Waiting for https://gitlab.gnome.org/GNOME/gtk/issues/2375 to be able to fix that */
    { THUNAR_LAUNCHER_ACTION_SENDTO_MENU,      "<Actions>/ThunarWindow/sendto-menu",               "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Send To"),                        NULL,                                                                                            NULL,                   NULL,                                                    },
    { THUNAR_LAUNCHER_ACTION_SENDTO_SHORTCUTS, "<Actions>/ThunarShortcutsPane/sendto-shortcuts",   "",                  XFCE_GTK_MENU_ITEM,       NULL,                                   NULL,                                                                                            "bookmark-new",         G_CALLBACK (thunar_launcher_action_add_shortcuts),       },
    { THUNAR_LAUNCHER_ACTION_SENDTO_DESKTOP,   "<Actions>/ThunarLauncher/sendto-desktop",          "",                  XFCE_GTK_MENU_ITEM,       NULL,                                   NULL,                                                                                            "user-desktop",         G_CALLBACK (thunar_launcher_action_sendto_desktop),      },
    { THUNAR_LAUNCHER_ACTION_PROPERTIES,       "<Actions>/ThunarStandardView/properties",          "<Alt>Return",       XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Properties..."),                  N_ ("View the properties of the selected file"),                                                 "document-properties",  G_CALLBACK (thunar_launcher_action_properties),          },
    { THUNAR_LAUNCHER_ACTION_MAKE_LINK,        "<Actions>/ThunarStandardView/make-link",           "",                  XFCE_GTK_MENU_ITEM,       N_ ("Ma_ke Link"),                      NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_make_link),           },
    { THUNAR_LAUNCHER_ACTION_DUPLICATE,        "<Actions>/ThunarStandardView/duplicate",           "",                  XFCE_GTK_MENU_ITEM,       N_ ("Du_plicate"),                      NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_duplicate),           },
    { THUNAR_LAUNCHER_ACTION_RENAME,           "<Actions>/ThunarStandardView/rename",              "F2",                XFCE_GTK_MENU_ITEM,       N_ ("_Rename..."),                      NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_rename),              },
    { THUNAR_LAUNCHER_ACTION_EMPTY_TRASH,      "<Actions>/ThunarWindow/empty-trash",               "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Empty Trash"),                    N_ ("Delete all files and folders in the Trash"),                                                NULL,                   G_CALLBACK (thunar_launcher_action_empty_trash),         },
    { THUNAR_LAUNCHER_ACTION_CREATE_FOLDER,    "<Actions>/ThunarStandardView/create-folder",       "<Primary><shift>N", XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Create _Folder..."),               N_ ("Create an empty folder within the current folder"),                                         "folder-new",           G_CALLBACK (thunar_launcher_action_create_folder),       },

    { THUNAR_LAUNCHER_ACTION_RESTORE,          "<Actions>/ThunarLauncher/restore",                 "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Restore"),                        NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_restore),             },
    { THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH,    "<Actions>/ThunarLauncher/move-to-trash",           "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Mo_ve to Trash"),                  NULL,                                                                                            "user-trash",           G_CALLBACK (thunar_launcher_action_move_to_trash),       },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete",                  "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Delete"),                         NULL,                                                                                            "edit-delete",          G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete-2",                "<Shift>Delete",     XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete-3",                "<Shift>KP_Delete",  XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_TRASH_DELETE,     "<Actions>/ThunarLauncher/trash-delete",            "Delete",            XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_trash_delete),        },
    { THUNAR_LAUNCHER_ACTION_TRASH_DELETE,     "<Actions>/ThunarLauncher/trash-delete-2",          "KP_Delete",         XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_trash_delete),        },
    { THUNAR_LAUNCHER_ACTION_PASTE,            "<Actions>/ThunarLauncher/paste",                   "<Primary>V",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Paste"),                          N_ ("Move or copy files previously selected by a Cut or Copy command"),                          "edit-paste",           G_CALLBACK (thunar_launcher_action_paste_into_folder),   },
    { THUNAR_LAUNCHER_ACTION_PASTE_ALT,        NULL,                                                 "",                XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Paste Into Folder"),              N_ ("Move or copy files previously selected by a Cut or Copy command into the selected folder"), "edit-paste",           G_CALLBACK (thunar_launcher_action_paste_into_folder),   },
    { THUNAR_LAUNCHER_ACTION_COPY,             "<Actions>/ThunarLauncher/copy",                    "<Primary>C",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Copy"),                           N_ ("Prepare the selected files to be copied with a Paste command"),                             "edit-copy",            G_CALLBACK (thunar_launcher_action_copy),                },
    { THUNAR_LAUNCHER_ACTION_CUT,              "<Actions>/ThunarLauncher/cut",                     "<Primary>X",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Cu_t"),                            N_ ("Prepare the selected files to be moved with a Paste command"),                              "edit-cut",             G_CALLBACK (thunar_launcher_action_cut),                 },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_launcher_action_entries,G_N_ELEMENTS(thunar_launcher_action_entries),id)


G_DEFINE_TYPE_WITH_CODE (ThunarLauncher, thunar_launcher, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_launcher_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_launcher_component_init))



static void
thunar_launcher_class_init (ThunarLauncherClass *klass)
{
  GObjectClass *gobject_class;
  gpointer      g_iface;

  /* determine all used quarks */
  thunar_launcher_appinfo_quark = g_quark_from_static_string ("thunar-launcher-appinfo");
  thunar_launcher_device_quark = g_quark_from_static_string ("thunar-launcher-device");
  thunar_launcher_file_quark = g_quark_from_static_string ("thunar-launcher-file");

  xfce_gtk_translate_action_entries (thunar_launcher_action_entries, G_N_ELEMENTS (thunar_launcher_action_entries));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_launcher_dispose;
  gobject_class->finalize = thunar_launcher_finalize;
  gobject_class->get_property = thunar_launcher_get_property;
  gobject_class->set_property = thunar_launcher_set_property;

  /**
   * ThunarLauncher:widget:
   *
   * The #GtkWidget with which this launcher is associated.
   **/
  launcher_props[PROP_WIDGET] =
      g_param_spec_object ("widget",
                           "widget",
                           "widget",
                           GTK_TYPE_WIDGET,
                           EXO_PARAM_WRITABLE);

  /* Override ThunarNavigator's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_NAVIGATOR);
  launcher_props[PROP_CURRENT_DIRECTORY] =
      g_param_spec_override ("current-directory",
                             g_object_interface_find_property (g_iface, "current-directory"));

  /* Override ThunarComponent's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_COMPONENT);
  launcher_props[PROP_SELECTED_FILES] =
      g_param_spec_override ("selected-files",
                             g_object_interface_find_property (g_iface, "selected-files"));

  launcher_props[PROP_UI_MANAGER] =
      g_param_spec_override ("ui-manager",
                             g_object_interface_find_property (g_iface, "ui-manager"));

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, launcher_props);
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
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* setup the action group for the launcher actions */
  launcher->action_group = gtk_action_group_new ("ThunarLauncher");
  gtk_action_group_set_translation_domain (launcher->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (launcher->action_group, action_entries, G_N_ELEMENTS (action_entries), launcher);

  /* determine references to our actions */
  launcher->action_open = gtk_action_group_get_action (launcher->action_group, "open");
  launcher->action_open_with_other = gtk_action_group_get_action (launcher->action_group, "open-with-other");
  launcher->action_open_in_new_window = gtk_action_group_get_action (launcher->action_group, "open-in-new-window");
  launcher->action_open_in_new_tab = gtk_action_group_get_action (launcher->action_group, "open-in-new-tab");
  launcher->action_open_with_other_in_menu = gtk_action_group_get_action (launcher->action_group, "open-with-other-in-menu");
G_GNUC_END_IGNORE_DEPRECATIONS

  launcher->selected_files = NULL;

  /* grab a reference on the preferences */
  launcher->preferences = thunar_preferences_get ();
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
  thunar_g_file_list_free (launcher->selected_files);
  launcher->selected_files = NULL;

  /* unref parent, if any */
  if (launcher->parent_folder != NULL)
      g_object_unref (launcher->parent_folder);

  (*G_OBJECT_CLASS (thunar_launcher_parent_class)->dispose) (object);
}



static void
thunar_launcher_finalize (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  /* release the preferences reference */
  g_object_unref (launcher->preferences);

  /* release the reference on the action group */
  g_object_unref (launcher->action_group);

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
      thunar_launcher_set_widget (launcher, g_value_get_object (value));
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
  GList          *selected_files = NULL;

  /* disconnect from the previous directory */
  if (G_LIKELY (launcher->current_directory != NULL))
    g_object_unref (G_OBJECT (launcher->current_directory));

  /* activate the new directory */
  launcher->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      g_object_ref (G_OBJECT (current_directory));

      /* set selected files with current directory, if not initialized yet*/
      if (launcher->selected_files == NULL)
        {
          selected_files = g_list_append (selected_files, current_directory);
          thunar_launcher_set_selected_files (THUNAR_COMPONENT (navigator), selected_files);
          g_list_free (selected_files);
        }
    }

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_CURRENT_DIRECTORY]);
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
  GList          *lp;

  /* disconnect from the previously selected files */
  thunar_g_file_list_free (launcher->selected_files);

  /* connect to the new selected files list */
  launcher->selected_files = thunar_g_file_list_copy (selected_files);

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_SELECTED_FILES]);

  /* unref previous parent, if any */
  if (launcher->parent_folder != NULL)
    {
      g_object_unref (launcher->parent_folder);
      launcher->parent_folder = NULL;
    }

  /* if nothing is selected, the current directory is considered to be the selection */
  if (selected_files == NULL || g_list_length (selected_files) == 0)
    {
      if (launcher->current_directory != NULL)
        launcher->selected_files = g_list_append (launcher->selected_files, launcher->current_directory);
    }
  else
    {
      launcher->selected_files = thunar_g_file_list_copy (selected_files);
    }

  launcher->selection_trashable    = TRUE;
  launcher->n_selected_files       = 0;
  launcher->n_selected_directories = 0;
  launcher->n_selected_executables = 0;
  launcher->n_selected_regulars    = 0;

  /* determine the number of files/directories/executables */
  for (lp = launcher->selected_files; lp != NULL; lp = lp->next, ++launcher->n_selected_files)
    {
      /* Keep a reference on all selected files */
      g_object_ref (lp->data);

      if (thunar_file_is_directory (lp->data)
          || thunar_file_is_shortcut (lp->data)
          || thunar_file_is_mountable (lp->data))
        {
          ++launcher->n_selected_directories;
        }
      else
        {
          if (thunar_file_is_executable (lp->data))
            ++launcher->n_selected_executables;
          ++launcher->n_selected_regulars;
        }

      if (!thunar_file_can_be_trashed (lp->data))
        launcher->selection_trashable = FALSE;
    }

  launcher->single_folder_selected = (launcher->n_selected_directories == 1 && launcher->n_selected_files == 1);
  if (launcher->single_folder_selected)
    {
      /* grab the folder of the first selected item */
      launcher->single_folder = THUNAR_FILE (launcher->selected_files->data);
      if (launcher->current_directory != NULL)
        launcher->current_directory_selected = g_file_equal (thunar_file_get_file (THUNAR_FILE (launcher->selected_files->data)), thunar_file_get_file (launcher->current_directory));
    }
  else
    {
      launcher->single_folder = NULL;
      launcher->current_directory_selected = FALSE;
    }

  if (launcher->selected_files != NULL)
    {
      /* just grab the folder of the first selected item */
      launcher->parent_folder = thunar_file_get_parent (THUNAR_FILE (launcher->selected_files->data), NULL);
    }
  else
    {
      launcher->parent_folder = NULL;
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
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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
G_GNUC_END_IGNORE_DEPRECATIONS

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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, launcher->action_group, -1);

      /* merge our UI control items with the new manager */
      launcher->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_launcher_ui, thunar_launcher_ui_length, &error);
G_GNUC_END_IGNORE_DEPRECATIONS
      if (G_UNLIKELY (launcher->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarLauncher menus: %s", error->message);
          g_error_free (error);
        }

      /* update the user interface */
      //thunar_launcher_update (launcher);
    }

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_UI_MANAGER]);
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

  launcher->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    {
      g_object_ref (G_OBJECT (widget));
      g_signal_connect_swapped (G_OBJECT (widget), "destroy", G_CALLBACK (thunar_launcher_widget_destroyed), launcher);
    }

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_WIDGET]);
}



static void
thunar_launcher_menu_item_activated (ThunarLauncher *launcher,
                                     GtkWidget      *menu_item)
{
  GAppInfo *app_info;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->selected_files == NULL))
    return;

  /* if we have a mime handler associated with the menu_item, we pass it to the launcher (g_object_get_qdata will return NULL otherwise)*/
  app_info = g_object_get_qdata (G_OBJECT (menu_item), thunar_launcher_appinfo_quark);
  thunar_launcher_activate_selected_files (launcher, THUNAR_LAUNCHER_CHANGE_DIRECTORY, app_info);
}



/**
 * thunar_launcher_activate_selected_files:
 * @launcher : a #ThunarLauncher instance
 * @action   : the #ThunarLauncherFolderOpenAction to use, if there are folders among the selected files
 * @app_info : a #GAppInfo instance
 *
 * Will try to open all selected files with the provided #GAppInfo
 **/
void
thunar_launcher_activate_selected_files (ThunarLauncher                 *launcher,
                                         ThunarLauncherFolderOpenAction  action,
                                         GAppInfo                       *app_info)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  thunar_launcher_poke_files (launcher, app_info, action);
}



static void
thunar_launcher_execute_files (ThunarLauncher *launcher,
                               GList          *files)
{
  GError *error = NULL;
  GFile  *working_directory;
  GList  *lp;

  /* execute all selected files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      working_directory = thunar_file_get_file (launcher->current_directory);

      if (!thunar_file_execute (lp->data, working_directory, launcher->widget, NULL, NULL, &error))
        {
          /* display an error message to the user */
          thunar_dialogs_show_error (launcher->widget, error, _("Failed to execute file \"%s\""), thunar_file_get_display_name (lp->data));
          g_error_free (error);
          break;
        }
    }
}



static guint
thunar_launcher_g_app_info_hash (gconstpointer app_info)
{
  return 0;
}



static void
thunar_launcher_open_file (ThunarLauncher *launcher,
                           ThunarFile     *file,
                           GAppInfo       *application_to_use)
{
  GList *files = NULL;

  files = g_list_append (files, file);
  thunar_launcher_open_files (launcher, files, application_to_use);
  g_list_free (files);
}



static void
thunar_launcher_open_files (ThunarLauncher *launcher,
                            GList          *files,
                            GAppInfo       *application_to_use)
{
  GHashTable *applications;
  GAppInfo   *app_info;
  GList      *file_list;
  GList      *lp;

  /* allocate a hash table to associate applications to URIs. since GIO allocates
   * new GAppInfo objects every time, g_direct_hash does not work. we therefore use
   * a fake hash function to always hit the collision list of the hash table and
   * avoid storing multiple equal GAppInfos by means of g_app_info_equal(). */
  applications = g_hash_table_new_full (thunar_launcher_g_app_info_hash,
                                        (GEqualFunc) g_app_info_equal,
                                        (GDestroyNotify) g_object_unref,
                                        (GDestroyNotify) thunar_g_file_list_free);

  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* Because we created the hash_table with g_hash_table_new_full
       * g_object_unref on each hash_table key and value will be called by g_hash_table_destroy */
      if (application_to_use)
        {
          app_info = g_app_info_dup (application_to_use);
        }
      else
        {
          /* determine the default application for the MIME type */
          app_info = thunar_file_get_default_handler (lp->data);
        }

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
  screen = gtk_widget_get_screen (launcher->widget);

  /* create launch context */
  context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
  gdk_app_launch_context_set_screen (context, screen);
  gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());
  gdk_app_launch_context_set_icon (context, g_app_info_get_icon (app_info));

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
      window = gtk_widget_get_toplevel (launcher->widget);
      dialog = gtk_message_dialog_new ((GtkWindow *) window,
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       _("Are you sure you want to open all folders?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                ngettext ("This will open %d separate file manager window.",
                                                          "This will open %d separate file manager windows.",
                                                          n), n);
      label = g_strdup_printf (ngettext ("Open %d New Window", "Open %d New Windows", n), n);
      gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
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
      screen = gtk_widget_get_screen (launcher->widget);

      /* open all requested windows */
      for (lp = directories; lp != NULL; lp = lp->next)
        thunar_application_open_window (application, lp->data, screen, NULL, TRUE);

      /* release the application object */
      g_object_unref (G_OBJECT (application));
    }
}



static void
thunar_launcher_poke_files (ThunarLauncher                 *launcher,
                            GAppInfo                       *application_to_use,
                            ThunarLauncherFolderOpenAction  folder_open_action)
{
  ThunarLauncherPokeData *poke_data;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

   poke_data = thunar_launcher_poke_data_new (launcher->selected_files, application_to_use, folder_open_action);

  // We will only poke one file at a time, in order to dont use all available CPU's
  // TODO: Check if that could cause slowness
  thunar_browser_poke_file (THUNAR_BROWSER (launcher), poke_data->files_to_poke->data,
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
  gboolean                open_new_window_as_tab = TRUE;
  GList                  *directories = NULL;
  GList                  *files = NULL;
  GList                  *lp;

  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (poke_data != NULL);
  _thunar_return_if_fail (poke_data->files_to_poke != NULL);

  /* check if poking succeeded */
  if (error == NULL)
    {
      /* add the resolved file to the list of file to be opened/executed later */
      poke_data->files_poked = g_list_prepend (poke_data->files_poked,g_object_ref (target_file));
    }

  /* release and remove the just poked file from the list */
  g_object_unref (poke_data->files_to_poke->data);
  poke_data->files_to_poke = g_list_delete_link (poke_data->files_to_poke, poke_data->files_to_poke);

  if (poke_data->files_to_poke == NULL)
    {
      /* separate files and directories in the selected files list */
      for (lp = poke_data->files_poked; lp != NULL; lp = lp->next)
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
          if (poke_data->application_to_use != NULL)
            {
              /* open them separately, using some specific application */
              for (lp = directories; lp != NULL; lp = lp->next)
                thunar_launcher_open_file (THUNAR_LAUNCHER (browser), lp->data, poke_data->application_to_use);
            }
          else if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_TAB)
            {
              for (lp = directories; lp != NULL; lp = lp->next)
                thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (browser), lp->data);
            }
          else if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW)
            {
              thunar_launcher_open_windows (THUNAR_LAUNCHER (browser), directories);
            }
          else if (poke_data->folder_open_action == THUNAR_LAUNCHER_CHANGE_DIRECTORY)
            {
              /* If multiple directories are passed, we assume that we should open them all */
              if (directories->next == NULL)
                thunar_navigator_change_directory (THUNAR_NAVIGATOR (browser), directories->data);
              else
                {
                  g_object_get (G_OBJECT (THUNAR_LAUNCHER (browser)->preferences), "misc-open-new-window-as-tab", &open_new_window_as_tab, NULL);
                  if (open_new_window_as_tab)
                    {
                      for (lp = directories; lp != NULL; lp = lp->next)
                        thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (browser), lp->data);
                    }
                  else
                    thunar_launcher_open_windows (THUNAR_LAUNCHER (browser), directories);
                }
            }
          else
              g_warning("'folder_open_action' was not defined");
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
              /* try to open all files */
              thunar_launcher_open_files (THUNAR_LAUNCHER (browser), files, poke_data->application_to_use);
            }

          /* cleanup */
          g_list_free (files);
        }

      /* free the poke data */
      thunar_launcher_poke_data_free (poke_data);
    }
  else
    {
      /* we need to continue this until all files have been resolved */
      // We will only poke one file at a time, in order to dont use all available CPU's
      // TODO: Check if that could cause slowness
      thunar_browser_poke_file (browser, poke_data->files_to_poke->data,
                                (THUNAR_LAUNCHER (browser))->widget, thunar_launcher_poke_files_finish, poke_data);
    }
}



static ThunarLauncherPokeData *
thunar_launcher_poke_data_new (GList                          *files_to_poke,
                               GAppInfo                       *application_to_use,
                               ThunarLauncherFolderOpenAction  folder_open_action)
{
  ThunarLauncherPokeData *data;

  data = g_slice_new0 (ThunarLauncherPokeData);
  data->files_to_poke = thunar_g_file_list_copy (files_to_poke);
  data->files_poked = NULL;
  data->application_to_use = application_to_use;

  /* keep a reference on the appdata */
  if (application_to_use != NULL)
    g_object_ref (application_to_use);
  data->folder_open_action = folder_open_action;

  return data;
}



static void
thunar_launcher_poke_data_free (ThunarLauncherPokeData *data)
{
  _thunar_return_if_fail (data != NULL);

  thunar_g_file_list_free (data->files_to_poke);
  thunar_g_file_list_free (data->files_poked);

  if (data->application_to_use != NULL)
    g_object_unref (data->application_to_use);
  g_slice_free (ThunarLauncherPokeData, data);
}



void thunar_launcher_open_selected_folders_in_new_tabs (ThunarLauncher *launcher)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
    _thunar_return_if_fail (thunar_file_is_directory (THUNAR_FILE (lp->data)));

  thunar_launcher_poke_files (launcher, NULL, THUNAR_LAUNCHER_OPEN_AS_NEW_TAB);
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



/**
 * thunar_launcher_open_selected_folders_in_new_windows:
 * @launcher : a #ThunarLauncher instance
 *
 * Will open each selected folder in a new window
 **/
void thunar_launcher_open_selected_folders_in_new_windows (ThunarLauncher *launcher)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
    _thunar_return_if_fail (thunar_file_is_directory (THUNAR_FILE (lp->data)));

  thunar_launcher_poke_files (launcher, NULL, THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW);
}



static void
thunar_launcher_action_open (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->selected_files == NULL))
    return;

  thunar_launcher_activate_selected_files (launcher, THUNAR_LAUNCHER_CHANGE_DIRECTORY, NULL);
}



/**
 * thunar_launcher_action_open_in_new_tabs:
 * @launcher : a #ThunarLauncher instance
 *
 * Will open each selected folder in a new tab
 **/
static void
thunar_launcher_action_open_in_new_tabs (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->selected_files == NULL))
    return;

  thunar_launcher_open_selected_folders_in_new_tabs (launcher);
}



static void
thunar_launcher_action_open_in_new_windows (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->selected_files == NULL))
    return;

  thunar_launcher_open_selected_folders_in_new_windows (launcher);
}



static void
thunar_launcher_action_open_with_other (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->n_selected_files == 1)
    thunar_show_chooser_dialog (launcher->widget, launcher->selected_files->data, TRUE);
}



static gboolean
thunar_launcher_show_trash (ThunarLauncher *launcher)
{
  if (launcher->parent_folder == NULL)
    return FALSE;

  /* If the folder is read only, always show trash insensitive */
  /* If we are outside waste basket, the selection is trashable and we support trash, show trash */
  return !thunar_file_is_writable (launcher->parent_folder) || ( !thunar_file_is_trashed (launcher->parent_folder) && launcher->selection_trashable && thunar_g_vfs_is_uri_scheme_supported ("trash"));
}



/**
 * thunar_launcher_append_paste_item:
 * @launcher                 : a #ThunarLauncher instance
 * @menu                     : #GtkMenuShell on which the paste item should be appended
 * @force                    : Append a 'paste' #GtkMenuItem, even if multiple folders are selected
 *
 * Will append a 'paste' #GtkMenuItem to the provided #GtkMenuShell
 *
 * Return value: (transfer full): The new #GtkMenuItem, or NULL
 **/
static GtkWidget*
thunar_launcher_append_paste_item (ThunarLauncher *launcher,
                                   GtkMenuShell   *menu,
                                   gboolean        force)
{
  GtkWidget                *item = NULL;
  ThunarClipboardManager   *clipboard;
  const XfceGtkActionEntry *action_entry = get_action_entry (THUNAR_LAUNCHER_ACTION_PASTE);

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  _thunar_return_val_if_fail (action_entry != NULL, NULL);

  if (!force && !launcher->single_folder_selected)
    return NULL;

  /* grab a reference on the clipboard manager for this display */
  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));

  /* Some single folder is selected, but its not the current directory */
  if (launcher->single_folder_selected && !launcher->current_directory_selected)
    {
      item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
      gtk_widget_set_sensitive (item, thunar_clipboard_manager_get_can_paste (clipboard) && thunar_file_is_writable (launcher->single_folder));
    }
  else
    {
      item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
      gtk_widget_set_sensitive (item, thunar_clipboard_manager_get_can_paste (clipboard) && thunar_file_is_writable (launcher->current_directory));
    }
  g_object_unref (clipboard);
  return item;
}



/**
 * thunar_launcher_append_menu_item:
 * @launcher       : Instance of a  #ThunarLauncher
 * @menu           : #GtkMenuShell to which the item should be added
 * @action         : #ThunarLauncherAction to select which item should be added
 * @force          : force to generate the item. If it cannot be used, it will be shown as insensitive
 *
 * Adds the selected, widget specific #GtkMenuItem to the passed #GtkMenuShell
 *
 * Return value: (transfer none): The added #GtkMenuItem
 **/
GtkWidget*
thunar_launcher_append_menu_item (ThunarLauncher       *launcher,
                                  GtkMenuShell         *menu,
                                  ThunarLauncherAction  action,
                                  gboolean              force)
{
  GtkWidget                *item = NULL;
  GtkWidget                *submenu;
  gchar                    *label_text;
  gchar                    *tooltip_text;
  const XfceGtkActionEntry *action_entry = get_action_entry (action);
  gboolean                  show_delete_item;
  gboolean                  can_be_used;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  _thunar_return_val_if_fail (action_entry != NULL, NULL);

  /* This may occur when the thunar-window is build */
  if (G_UNLIKELY (launcher->selected_files == NULL))
    return NULL;

  switch (action)
    {
      case THUNAR_LAUNCHER_ACTION_OPEN: /* aka "activate" */
        return xfce_gtk_image_menu_item_new_from_icon_name (_("_Open"), ngettext ("Open the selected file", "Open the selected files", launcher->n_selected_files),
                                           action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);

      case THUNAR_LAUNCHER_ACTION_EXECUTE:
        return xfce_gtk_image_menu_item_new_from_icon_name (_("_Execute"), ngettext ("Execute the selected file", "Execute the selected files", launcher->n_selected_files),
                                           action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);

      case THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB:
        label_text = g_strdup_printf (ngettext ("Open in %d New _Tab", "Open in %d New _Tabs", launcher->n_selected_files), launcher->n_selected_files);
        tooltip_text = g_strdup_printf (ngettext ("Open the selected directory in %d new tab",
                                                  "Open the selected directories in %d new tabs", launcher->n_selected_files), launcher->n_selected_files);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        g_free (tooltip_text);
        g_free (label_text);
        return item;

      case THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW:
        label_text = g_strdup_printf (ngettext ("Open in %d New _Window", "Open in %d New _Windows", launcher->n_selected_files), launcher->n_selected_files);
        tooltip_text = g_strdup_printf (ngettext ("Open the selected directory in %d new window",
                                                  "Open the selected directories in %d new windows",launcher->n_selected_files), launcher->n_selected_files);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        g_free (tooltip_text);
        g_free (label_text);
        return item;

      case THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER:
        return xfce_gtk_menu_item_new (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);

      case THUNAR_LAUNCHER_ACTION_SENDTO_MENU:
        can_be_used = launcher->current_directory_selected == FALSE;
        if (!can_be_used && !force)
          return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        submenu = thunar_launcher_build_sendto_submenu (launcher);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        return item;

      case THUNAR_LAUNCHER_ACTION_MAKE_LINK:
        can_be_used = thunar_file_is_writable (launcher->current_directory) &&
                      launcher->current_directory_selected == FALSE &&
                      thunar_file_is_trashed (launcher->current_directory) == FALSE;;
        if (!can_be_used && !force)
          return NULL;

        label_text = ngettext ("Ma_ke Link", "Ma_ke Links", launcher->n_selected_files);
        tooltip_text = ngettext ("Create a symbolic link for the selected file",
                                 "Create a symbolic link for each selected file", launcher->n_selected_files);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                       G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_DUPLICATE:
        can_be_used = thunar_file_is_writable (launcher->current_directory) &&
                      launcher->current_directory_selected == FALSE &&
                      thunar_file_is_trashed (launcher->current_directory) == FALSE;
        if (!can_be_used && !force)
          return NULL;
        item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_RENAME:
        can_be_used = thunar_file_is_writable (launcher->current_directory) &&
                      launcher->current_directory_selected == FALSE &&
                      thunar_file_is_trashed (launcher->current_directory) == FALSE;
        if (!can_be_used && !force)
          return NULL;
        tooltip_text = ngettext ("Rename the selected file",
                                 "Rename the selected files", launcher->n_selected_files);
        item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                       action_entry->callback, G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_RESTORE:
        if (launcher->current_directory_selected == FALSE && thunar_file_is_trashed (launcher->current_directory))
          {
            tooltip_text = ngettext ("Restore the selected file to its original location",
                                     "Restore the selected files to its original location", launcher->n_selected_files);
            item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                           action_entry->callback, G_OBJECT (launcher), menu);
            gtk_widget_set_sensitive (item, thunar_file_is_writable (launcher->current_directory));
            return item;
          }
        return NULL;

      case THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH:
        if (!thunar_launcher_show_trash (launcher))
          return NULL;

        can_be_used = launcher->parent_folder != NULL &&
                      !launcher->current_directory_selected;
        if (!can_be_used && !force)
          return NULL;

        tooltip_text = ngettext ("Move the selected file to the Trash",
                                 "Move the selected files to the Trash", launcher->n_selected_files);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;


      case THUNAR_LAUNCHER_ACTION_DELETE:
        g_object_get (G_OBJECT (launcher->preferences), "misc-show-delete-action", &show_delete_item, NULL);
        if (thunar_launcher_show_trash (launcher) && !show_delete_item)
          return NULL;

        can_be_used = launcher->parent_folder != NULL &&
                      !launcher->current_directory_selected;
        if (!can_be_used && !force)
          return NULL;

        tooltip_text = ngettext ("Permanently delete the selected file",
                                 "Permanently delete the selected files", launcher->n_selected_files);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_EMPTY_TRASH:
        if (launcher->single_folder_selected == TRUE)
          {
            if (thunar_file_is_root (launcher->single_folder) && thunar_file_is_trashed (launcher->single_folder))
              {
                item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text, action_entry->accel_path,
                                                                    action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
                gtk_widget_set_sensitive (item, thunar_file_get_item_count (launcher->single_folder) > 0);
              }
          }
        return NULL;

      case THUNAR_LAUNCHER_ACTION_CREATE_FOLDER:
        if (thunar_file_is_trashed (launcher->current_directory))
          return NULL;
        return xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));


      case THUNAR_LAUNCHER_ACTION_CUT:
        can_be_used = launcher->current_directory_selected == FALSE &&
                      launcher->parent_folder != NULL;
        if (!can_be_used && !force)
          return NULL;
        tooltip_text = ngettext ("Prepare the selected file to be moved with a Paste command",
                                 "Prepare the selected files to be moved with a Paste command", launcher->n_selected_files);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_COPY:
        can_be_used = launcher->current_directory_selected == FALSE;
        if (!can_be_used && !force)
          return NULL;
        tooltip_text = ngettext ("Prepare the selected file to be copied with a Paste command",
                                 "Prepare the selected files to be copied with a Paste command", launcher->n_selected_files);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, can_be_used);
        return item;

      case THUNAR_LAUNCHER_ACTION_PASTE:
        return thunar_launcher_append_paste_item (launcher, menu, FALSE);

      default:
        return xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
    }
  return NULL;
}




static void
thunar_launcher_action_sendto_desktop (ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GFile             *desktop_file;
  GList             *files;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* determine the source paths */
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



static void
thunar_launcher_sendto_device (ThunarLauncher *launcher,
                               ThunarDevice   *device)
{
  ThunarApplication *application;
  GFile             *mount_point;
  GList             *files;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));

  if (!thunar_device_is_mounted (device))
    return;

  /* determine the source paths */
  files = thunar_file_list_to_thunar_g_file_list (launcher->selected_files);
  if (G_UNLIKELY (files == NULL))
    return;

  mount_point = thunar_device_get_root (device);
  if (mount_point != NULL)
    {
      /* copy the files onto the specified device */
      application = thunar_application_get ();
      thunar_application_copy_into (application, launcher->widget, files, mount_point, NULL);
      g_object_unref (application);
      g_object_unref (mount_point);
    }

  /* cleanup */
  thunar_g_file_list_free (files);
}



static void
thunar_launcher_sendto_mount_finish (ThunarDevice *device,
                                     const GError *error,
                                     gpointer      user_data)
{
  ThunarLauncher *launcher = user_data;
  gchar          *device_name;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (user_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (user_data));

  if (error != NULL)
    {
      /* tell the user that we were unable to mount the device, which is
       * required to send files to it */
      device_name = thunar_device_get_name (device);
      thunar_dialogs_show_error (launcher->widget, error, _("Failed to mount \"%s\""), device_name);
      g_free (device_name);
    }
  else
    {
      thunar_launcher_sendto_device (launcher, device);
    }
}



static void
thunar_launcher_action_sendto_device (ThunarLauncher *launcher,
                                      GObject        *object)
{
  GMountOperation *mount_operation;
  ThunarDevice    *device;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* determine the device to which to send */
  device = g_object_get_qdata (G_OBJECT (object), thunar_launcher_device_quark);
  if (G_UNLIKELY (device == NULL))
    return;

  /* make sure to mount the device first, if it's not already mounted */
  if (!thunar_device_is_mounted (device))
    {
      /* allocate a GTK+ mount operation */
      mount_operation = thunar_gtk_mount_operation_new (launcher->widget);

      /* try to mount the device and later start sending the files */
      thunar_device_mount (device,
                           mount_operation,
                           NULL,
                           thunar_launcher_sendto_mount_finish,
                           launcher);

      g_object_unref (mount_operation);
    }
  else
    {
      thunar_launcher_sendto_device (launcher, device);
    }
}


static void
thunar_launcher_action_add_shortcuts (ThunarLauncher *launcher)
{
  GList           *lp;
  GtkWidget       *window;
  const GtkWidget *sidepane;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* determine the toplevel window we belong to */
  window = gtk_widget_get_toplevel (launcher->widget);
  if (THUNAR_IS_WINDOW (window) == FALSE)
    return;
  if (thunar_window_has_shortcut_sidepane (THUNAR_WINDOW (window)) == FALSE)
    return;

  sidepane = thunar_window_get_sidepane (THUNAR_WINDOW (window));
  if (sidepane != NULL  && THUNAR_IS_SHORTCUTS_PANE (sidepane))
    {
      for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
        thunar_shortcuts_pane_add_shortcut (THUNAR_SHORTCUTS_PANE (sidepane), lp->data);
    }

}



static GtkWidget*
thunar_launcher_build_sendto_submenu (ThunarLauncher *launcher)
{
  GList                    *lp;
  gboolean                  linkable = TRUE;
  gchar                    *label_text;
  gchar                    *tooltip_text;
  GtkWidget                *image;
  GtkWidget                *item;
  GtkWidget                *submenu;
  GtkWidget                *window;
  GList                    *devices;
  GList                    *appinfo_list;
  GIcon                    *icon;
  ThunarDeviceMonitor      *device_monitor;
  ThunarSendtoModel        *sendto_model;
  const XfceGtkActionEntry *action_entry;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);

  submenu = gtk_menu_new();

  /* show "sent to shortcut" if only directories are selected */
  if (launcher->n_selected_directories > 0 && launcher->n_selected_directories == launcher->n_selected_files)
    {
      /* determine the toplevel window we belong to */
      window = gtk_widget_get_toplevel (launcher->widget);
      if (THUNAR_IS_WINDOW (window) && thunar_window_has_shortcut_sidepane (THUNAR_WINDOW (window)))
        {
          action_entry = get_action_entry (THUNAR_LAUNCHER_ACTION_SENDTO_SHORTCUTS);
          if (action_entry != NULL)
            {
              label_text   = ngettext ("Side Pane (Create Shortcut)", "Side Pane (Create Shortcuts)", launcher->n_selected_files);
              tooltip_text = ngettext ("Add the selected folder to the shortcuts side pane",
                                       "Add the selected folders to the shortcuts side pane", launcher->n_selected_files);
              item = xfce_gtk_image_menu_item_new_from_icon_name (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                                                  G_OBJECT (launcher), action_entry->menu_item_icon_name, GTK_MENU_SHELL (submenu));
            }
        }
    }

  /* Check whether at least one files is located in the trash (to en-/disable the "sendto-desktop" action). */
  for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
    {
      if (G_UNLIKELY (thunar_file_is_trashed (lp->data)))
        linkable = FALSE;
    }
  if (linkable)
    {
      action_entry = get_action_entry (THUNAR_LAUNCHER_ACTION_SENDTO_DESKTOP);
      if (action_entry != NULL)
        {
          label_text   = ngettext ("Desktop (Create Link)", "Desktop (Create Links)", launcher->n_selected_files);
          tooltip_text = ngettext ("Create a link to the selected file on the desktop",
                                   "Create links to the selected files on the desktop", launcher->n_selected_files);
          item = xfce_gtk_image_menu_item_new_from_icon_name (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                                              G_OBJECT (launcher), action_entry->menu_item_icon_name, GTK_MENU_SHELL (submenu));
        }
    }

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  /* determine the currently active devices */
  device_monitor = thunar_device_monitor_get ();
  devices = thunar_device_monitor_get_devices (device_monitor);
  g_object_unref (device_monitor);

  /* add removable (and writable) drives and media */
  for (lp = devices; lp != NULL; lp = lp->next)
    {
      /* generate a unique name and tooltip for the device */
      label_text = thunar_device_get_name (lp->data);
      tooltip_text = g_strdup_printf (ngettext ("Send the selected file to \"%s\"",
                                                "Send the selected files to \"%s\"", launcher->n_selected_files), label_text);
      icon = thunar_device_get_icon (lp->data);
      image = NULL;
      if (G_LIKELY (icon != NULL))
        {
          image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
          g_object_unref (icon);
        }
      item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_action_sendto_device),
                                           G_OBJECT (launcher), image, GTK_MENU_SHELL (submenu));
      g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_device_quark, lp->data, g_object_unref);
      g_object_set_data (G_OBJECT (lp->data), "skip-app-info-update", GUINT_TO_POINTER (1));

      /* cleanup */
      g_free (tooltip_text);
      g_free (label_text);
    }

  /* free the devices list */
  g_list_free (devices);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  /* determine the sendto appInfos for the selected files */
  sendto_model = thunar_sendto_model_get_default ();
  appinfo_list = thunar_sendto_model_get_matching (sendto_model, launcher->selected_files);
  g_object_unref (sendto_model);

  if (G_LIKELY (appinfo_list != NULL))
    {
      /* add all handlers to the user interface */
      for (lp = appinfo_list; lp != NULL; lp = lp->next)
        {
          /* generate a unique name and tooltip for the handler */
          label_text = (gchar*)g_app_info_get_name (lp->data);
          tooltip_text = g_strdup_printf (ngettext ("Send the selected file to \"%s\"",
                                                    "Send the selected files to \"%s\"", launcher->n_selected_files), label_text);

          icon = g_app_info_get_icon (lp->data);
          image = NULL;
          if (G_LIKELY (icon != NULL))
            {
              image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
              g_object_unref (icon);
            }
          item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated),
                                               G_OBJECT (launcher), image, GTK_MENU_SHELL (submenu));
          g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_appinfo_quark, lp->data, g_object_unref);

          /* cleanup */
          g_free (tooltip_text);
        }

      /* release the appinfo list */
      g_list_free (appinfo_list);
    }

  return submenu;
}



static void
thunar_launcher_action_properties (ThunarLauncher *launcher)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* popup the files dialog */
  toplevel = gtk_widget_get_toplevel (launcher->widget);
  if (G_LIKELY (toplevel != NULL))
    {
      dialog = thunar_properties_dialog_new (GTK_WINDOW (toplevel));

      /* check if no files are currently selected */
      if (launcher->selected_files == NULL)
        {
          /* if we don't have any files selected, we just popup the properties dialog for the current folder. */
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), launcher->current_directory);
        }
      else
        {
          /* popup the properties dialog for all file(s) */
          thunar_properties_dialog_set_files (THUNAR_PROPERTIES_DIALOG (dialog), launcher->selected_files);
        }
      gtk_widget_show (dialog);
    }
}



static void
thunar_launcher_action_make_link (ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GList             *g_files = NULL;
  GList             *lp;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->current_directory == NULL))
    return;
  if (launcher->current_directory_selected == TRUE || thunar_file_is_trashed (launcher->current_directory))
    return;

  for (lp = launcher->selected_files; lp != NULL; lp = lp->next)
    {
      g_files = g_list_append (g_files, thunar_file_get_file (lp->data));
    }
  /* link the selected files into the current directory, which effectively
   * creates new unique links for the files.
   */
  application = thunar_application_get ();
  thunar_application_link_into (application, launcher->widget, g_files,
                                thunar_file_get_file (launcher->current_directory), NULL);
  g_object_unref (G_OBJECT (application));
  g_list_free (g_files);
}



static void
thunar_launcher_action_duplicate (ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GList             *selected_files;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->current_directory == NULL))
    return;
  if (launcher->current_directory_selected == TRUE || thunar_file_is_trashed (launcher->current_directory))
    return;

  /* determine the selected files for the view */
  selected_files = thunar_file_list_to_thunar_g_file_list (launcher->selected_files);
  if (G_LIKELY (selected_files != NULL))
    {
      /* copy the selected files into the current directory, which effectively
       * creates duplicates of the files.
       */
      application = thunar_application_get ();
      thunar_application_copy_into (application, launcher->widget, selected_files,
                                    thunar_file_get_file (launcher->current_directory), NULL);
      g_object_unref (G_OBJECT (application));

      /* clean up */
      thunar_g_file_list_free (selected_files);
    }
}



static void
thunar_launcher_rename_error (ExoJob    *job,
                              GError    *error,
                              GtkWidget *widget)
{
  GArray     *param_values;
  ThunarFile *file;

  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (error != NULL);

  param_values = thunar_simple_job_get_param_values (THUNAR_SIMPLE_JOB (job));
  file = g_value_get_object (&g_array_index (param_values, GValue, 0));

  thunar_dialogs_show_error (GTK_WIDGET (widget), error,
                             _("Failed to rename \"%s\""),
                             thunar_file_get_display_name (file));
  g_object_unref (file);
}



static void
thunar_launcher_rename_finished (ExoJob    *job,
                                 GtkWidget *widget)
{
  _thunar_return_if_fail (EXO_IS_JOB (job));

  /* destroy the job */
  g_signal_handlers_disconnect_matched (job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, widget);
  g_object_unref (job);
}



static void
thunar_launcher_action_rename (ThunarLauncher *launcher)
{
  ThunarJob *job;
  GtkWidget *window;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->selected_files == NULL || g_list_length (launcher->selected_files) == 0)
    return;
  if (launcher->current_directory_selected == TRUE || thunar_file_is_trashed (launcher->current_directory))
    return;

  /* get the window */
  window = gtk_widget_get_toplevel (launcher->widget);

  /* start renaming if we have exactly one selected file */
  if (g_list_length (launcher->selected_files) == 1)
    {
      /* run the rename dialog */
      job = thunar_dialogs_show_rename_file (GTK_WINDOW (window), THUNAR_FILE (launcher->selected_files->data));
      if (G_LIKELY (job != NULL))
        {
          g_signal_connect (job, "error", G_CALLBACK (thunar_launcher_rename_error), launcher->widget);
          g_signal_connect (job, "finished", G_CALLBACK (thunar_launcher_rename_finished), launcher->widget);
        }
    }
  else
    {
      /* display the bulk rename dialog */
      thunar_show_renamer_dialog (GTK_WIDGET (window), launcher->current_directory, launcher->selected_files, FALSE, NULL);
    }
}



static void
thunar_launcher_action_restore (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->current_directory_selected == TRUE || !thunar_file_is_trashed (launcher->current_directory))
    return;

  /* restore the selected files */
  application = thunar_application_get ();
  thunar_application_restore_files (application, launcher->widget, launcher->selected_files, NULL);
  g_object_unref (G_OBJECT (application));
}


static void
thunar_launcher_action_move_to_trash (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->parent_folder == NULL || launcher->current_directory_selected)
    return;

  application = thunar_application_get ();
  thunar_application_unlink_files (application, launcher->widget, launcher->selected_files, FALSE);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_action_delete (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->parent_folder == NULL || launcher->current_directory_selected)
    return;

  application = thunar_application_get ();
  thunar_application_unlink_files (application, launcher->widget, launcher->selected_files, TRUE);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_action_trash_delete (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    thunar_launcher_action_move_to_trash (launcher);
  else
    thunar_launcher_action_delete (launcher);
}



static void
thunar_launcher_action_empty_trash (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->single_folder_selected == FALSE)
    return;
  if (!thunar_file_is_root (launcher->single_folder) || !thunar_file_is_trashed (launcher->single_folder))
    return;

  application = thunar_application_get ();
  thunar_application_empty_trash (application, launcher->widget, NULL);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_action_create_folder (ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GList              path_list;
  gchar             *name;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));
  _thunar_return_if_fail (launcher->single_folder_selected);
  _thunar_return_if_fail (launcher->single_folder != NULL);

  if (thunar_file_is_trashed (launcher->single_folder))
    return;

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (launcher->widget,
                                    "inode/directory",
                                    _("New Folder"),
                                    _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* fake the path list */
      path_list.data = g_file_resolve_relative_path (thunar_file_get_file (launcher->current_directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      application = thunar_application_get ();
      thunar_application_mkdir (application, launcher->widget, &path_list, NULL);
      g_object_unref (G_OBJECT (application));

      /* release the path */
      g_object_unref (path_list.data);

      /* release the file name */
      g_free (name);
    }
}



static void
thunar_launcher_action_cut (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->current_directory_selected || launcher->parent_folder == NULL)
    return;

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_cut_files (clipboard, launcher->selected_files);
  g_object_unref (G_OBJECT (clipboard));
}



static void
thunar_launcher_action_copy (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->current_directory_selected)
    return;

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_copy_files (clipboard, launcher->selected_files);
  g_object_unref (G_OBJECT (clipboard));
}



static void
thunar_launcher_action_paste_into_folder (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;
  ThunarFile             *folder_to_paste;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->current_directory_selected)
    folder_to_paste = launcher->current_directory;
  else if (launcher->single_folder_selected)
    folder_to_paste = launcher->single_folder;
  else
    folder_to_paste = launcher->current_directory; /* Fallback if e.g. non directory files, or many folders are selected */

  /* paste files from the clipboard to the folder */
  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_file (folder_to_paste), launcher->widget, NULL);
  g_object_unref (G_OBJECT (clipboard));
}



static GtkWidget*
thunar_launcher_build_application_submenu (ThunarLauncher *launcher,
                                           GList          *applications)
{
  GList     *lp;
  GtkWidget *submenu;
  GtkWidget *image;
  GtkWidget *item;
  gchar     *label_text;
  gchar     *tooltip_text;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);

  submenu =  gtk_menu_new();
  /* add open with subitem per application */
  for (lp = applications; lp != NULL; lp = lp->next)
    {
      label_text = g_strdup_printf (_("Open With \"%s\""), g_app_info_get_name (lp->data));
      tooltip_text = g_strdup_printf (ngettext ("Use \"%s\" to open the selected file",
                                           "Use \"%s\" to open the selected files",
                                           launcher->n_selected_files), g_app_info_get_name (lp->data));
      image = gtk_image_new_from_gicon (g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
      item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated), G_OBJECT (launcher), image, GTK_MENU_SHELL (submenu));
      g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_appinfo_quark, g_object_ref (lp->data), g_object_unref);
      g_free (tooltip_text);
      g_free (label_text);
    }

  if (launcher->n_selected_files == 1)
    {
      xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
      thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (submenu), THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
    }

  return submenu;
}



/**
 * thunar_launcher_append_open_section:
 * @launcher                 : a #ThunarLauncher instance
 * @menu                     : #GtkMenuShell on which the open section should be appended
 * @support_tabs             : Set to TRUE if 'open in new tab' should be shown
 * @support_change_directory : Set to TRUE if 'open' should be shown
 * @force                    : Append the open section, even if the selected folder is the current folder
 *
 * Will append the section "open/open in new window/open in new tab/open with" to the provided #GtkMenuShell
 *
 * Return value: TRUE if the section was added
 **/
gboolean
thunar_launcher_append_open_section (ThunarLauncher *launcher,
                                     GtkMenuShell   *menu,
                                     gboolean        support_tabs,
                                     gboolean        support_change_directory,
                                     gboolean        force)
{
  GList     *applications;
  gchar     *label_text;
  gchar     *tooltip_text;
  GtkWidget *image;
  GtkWidget *menu_item;
  GtkWidget *submenu;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), FALSE);

  /* Usually it is not required to open the current directory */
  if (launcher->current_directory_selected && !force)
    return FALSE;

  /* determine the set of applications that work for all selected files */
  applications = thunar_file_list_get_applications (launcher->selected_files);

  /* Execute OR Open OR OpenWith */
  if (G_UNLIKELY (launcher->n_selected_executables == launcher->n_selected_files))
    thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_EXECUTE, FALSE);
  else if (G_LIKELY (launcher->n_selected_directories >= 1))
    {
      if (support_change_directory)
        thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN, FALSE);
    }
  else if (G_LIKELY (applications != NULL))
    {
      label_text = g_strdup_printf (_("_Open With \"%s\""), g_app_info_get_name (applications->data));
      tooltip_text = g_strdup_printf (ngettext ("Use \"%s\" to open the selected file",
                                                "Use \"%s\" to open the selected files",
                                                launcher->n_selected_files), g_app_info_get_name (applications->data));

      image = gtk_image_new_from_gicon (g_app_info_get_icon (applications->data), GTK_ICON_SIZE_MENU);
      menu_item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated),
                                                G_OBJECT (launcher), image, menu);

      /* remember the default application for the "Open" action as quark */
      g_object_set_qdata_full (G_OBJECT (menu_item), thunar_launcher_appinfo_quark, applications->data, g_object_unref);
      g_free (tooltip_text);
      g_free (label_text);

      /* drop the default application from the list */
      applications = g_list_delete_link (applications, applications);
    }
  else
    {
      /* we can only show a generic "Open" action */
      label_text = g_strdup_printf (_("_Open With Default Applications"));
      tooltip_text = g_strdup_printf (ngettext ("Open the selected file with the default application",
                                                "Open the selected files with the default applications", launcher->n_selected_files));
      xfce_gtk_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated), G_OBJECT (launcher), menu);
      g_free (tooltip_text);
      g_free (label_text);
    }

  if (G_LIKELY (applications != NULL))
    {
      menu_item = xfce_gtk_menu_item_new (_("Open With"),
                                          _("Choose another application with which to open the selected file"),
                                          NULL, NULL, NULL, menu);
      submenu = thunar_launcher_build_application_submenu (launcher, applications);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);
    }

  if (launcher->n_selected_files == launcher->n_selected_directories && launcher->n_selected_directories >= 1)
    {
      if (support_tabs)
        thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB, FALSE);
      thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW, FALSE);
    }

  g_list_free_full (applications, g_object_unref);
  return TRUE;
}



/**
 * thunar_launcher_get_widget:
 * @launcher : a #ThunarLauncher instance
 *
 * Will return the parent widget of this #ThunarLauncher
 *
 * Return value: (transfer none): the parent widget of this #ThunarLauncher
 **/
GtkWidget*
thunar_launcher_get_widget (ThunarLauncher *launcher)
{
  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  return launcher->widget;
}
