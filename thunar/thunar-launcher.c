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
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-sendto-model.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-tree-view.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-window.h>

#include <libxfce4ui/libxfce4ui.h>



/**
 * SECTION:thunar-launcher
 * @Short_description: Manages creation and execution of menu-item
 * @Title: ThunarLauncher
 *
 * The #ThunarLauncher class manages the creation and execution of menu-item which are used by multiple menus.
 * The management is done in a central way to prevent code duplication on various places.
 * XfceGtkActionEntry is used in order to define a list of the managed items and ease the setup of single items.
 *
 * #ThunarLauncher implements the #ThunarNavigator interface in order to use the "open in new tab" and "change directory" service.
 * It as well tracks the current directory via #ThunarNavigator.
 *
 * #ThunarLauncher implements the #ThunarComponent interface in order to track the currently selected files.
 * Based on to the current selection (and some other criteria), some menu items will not be shown, or will be insensitive.
 *
 * Files which are opened via #ThunarLauncher are poked first in order to e.g do missing mount operations.
 *
 * As well menu-item related services, like activation of selected files and opening tabs/new windows,
 * are provided by #ThunarLauncher.
 *
 * It is required to keep an instance of #ThunarLauncher open, in order to listen to accellerators which target
 * menu-items managed by #ThunarLauncher.
 * Typically a single instance of #ThunarLauncher is provided by each #ThunarWindow.
 */



typedef struct _ThunarLauncherPokeData ThunarLauncherPokeData;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_WIDGET,
  PROP_SELECT_FILES_CLOSURE,
  PROP_SELECTED_DEVICE,
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
static void                    thunar_launcher_set_selected_files         (ThunarComponent                *component,
                                                                           GList                          *selected_files);
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
static void                    thunar_launcher_poke                       (ThunarLauncher                 *launcher,
                                                                           GAppInfo                       *application_to_use,
                                                                           ThunarLauncherFolderOpenAction  folder_open_action);
static void                    thunar_launcher_poke_device_finish         (ThunarBrowser                  *browser,
                                                                           ThunarDevice                   *volume,
                                                                           ThunarFile                     *mount_point,
                                                                           GError                         *error,
                                                                           gpointer                        user_data,
                                                                           gboolean                        cancelled);
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
static void                    thunar_launcher_action_paste               (ThunarLauncher                 *launcher);
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
static void                    thunar_launcher_action_create_document     (ThunarLauncher                 *launcher,
                                                                           GtkWidget                      *menu_item);
static GtkWidget              *thunar_launcher_create_document_submenu_new(ThunarLauncher                 *launcher);



struct _ThunarLauncherClass
{
  GObjectClass __parent__;
};

struct _ThunarLauncher
{
  GObject __parent__;

  ThunarFile             *current_directory;
  GList                  *files_to_process;
  ThunarDevice           *device_to_process;

  gint                    n_files_to_process;
  gint                    n_directories_to_process;
  gint                    n_regulars_to_process;
  gboolean                files_to_process_trashable;
  gboolean                files_are_selected;
  gboolean                files_are_all_executable;
  gboolean                single_directory_to_process;

  ThunarFile             *single_folder;
  ThunarFile             *parent_folder;

  GClosure               *select_files_closure;

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
    { THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT,  "<Actions>/ThunarStandardView/create-document",     "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Create _Document"),                N_ ("Create a new document from a template"),                                                    "document-new",         G_CALLBACK (NULL),                                       },

    { THUNAR_LAUNCHER_ACTION_RESTORE,          "<Actions>/ThunarLauncher/restore",                 "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Restore"),                        NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_restore),             },
    { THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH,    "<Actions>/ThunarLauncher/move-to-trash",           "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Mo_ve to Trash"),                  NULL,                                                                                            "user-trash",           G_CALLBACK (thunar_launcher_action_trash_delete),       },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete",                  "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Delete"),                         NULL,                                                                                            "edit-delete",          G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete-2",                "<Shift>Delete",     XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_DELETE,           "<Actions>/ThunarLauncher/delete-3",                "<Shift>KP_Delete",  XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_delete),              },
    { THUNAR_LAUNCHER_ACTION_TRASH_DELETE,     "<Actions>/ThunarLauncher/trash-delete",            "Delete",            XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_trash_delete),        },
    { THUNAR_LAUNCHER_ACTION_TRASH_DELETE,     "<Actions>/ThunarLauncher/trash-delete-2",          "KP_Delete",         XFCE_GTK_IMAGE_MENU_ITEM, NULL,                                   NULL,                                                                                            NULL,                   G_CALLBACK (thunar_launcher_action_trash_delete),        },
    { THUNAR_LAUNCHER_ACTION_PASTE,            "<Actions>/ThunarLauncher/paste",                   "<Primary>V",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Paste"),                          N_ ("Move or copy files previously selected by a Cut or Copy command"),                          "edit-paste",           G_CALLBACK (thunar_launcher_action_paste),               },
    { THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER,NULL,                                               "",                  XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Paste Into Folder"),              N_ ("Move or copy files previously selected by a Cut or Copy command into the selected folder"), "edit-paste",           G_CALLBACK (thunar_launcher_action_paste_into_folder),   },
    { THUNAR_LAUNCHER_ACTION_COPY,             "<Actions>/ThunarLauncher/copy",                    "<Primary>C",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Copy"),                           N_ ("Prepare the selected files to be copied with a Paste command"),                             "edit-copy",            G_CALLBACK (thunar_launcher_action_copy),                },
    { THUNAR_LAUNCHER_ACTION_CUT,              "<Actions>/ThunarLauncher/cut",                     "<Primary>X",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Cu_t"),                            N_ ("Prepare the selected files to be moved with a Paste command"),                              "edit-cut",             G_CALLBACK (thunar_launcher_action_cut),                 },

    { THUNAR_LAUNCHER_ACTION_MOUNT,            NULL,                                               "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Mount"),                          N_ ("Mount the selected device"),                                                                NULL,                   G_CALLBACK (thunar_launcher_action_open),                },
    { THUNAR_LAUNCHER_ACTION_UNMOUNT,          NULL,                                               "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Unmount"),                        N_ ("Unmount the selected device"),                                                              NULL,                   G_CALLBACK (thunar_launcher_action_unmount),             },
    { THUNAR_LAUNCHER_ACTION_EJECT,            NULL,                                               "",                  XFCE_GTK_MENU_ITEM,       N_ ("_Eject"),                          N_ ("Eject the selected device"),                                                                NULL,                   G_CALLBACK (thunar_launcher_action_eject),               },
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

  /**
   * ThunarLauncher:select-files-closure:
   *
   * The #GClosure which will be called if the selected file should be updated after a launcher operation
   **/
  launcher_props[PROP_SELECT_FILES_CLOSURE] =
     g_param_spec_pointer ("select-files-closure",
                           "select-files-closure",
                           "select-files-closure",
                           G_PARAM_WRITABLE
                           | G_PARAM_CONSTRUCT_ONLY);

  /**
   * ThunarLauncher:select-device:
   *
   * The #ThunarDevice which currently is selected (or NULL if no #ThunarDevice is selected)
   **/
  launcher_props[PROP_SELECTED_DEVICE] =
     g_param_spec_pointer ("selected-device",
                           "selected-device",
                           "selected-device",
                           G_PARAM_WRITABLE);

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

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, launcher_props);
}



static void
thunar_launcher_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = (gpointer) exo_noop_null;
  iface->set_selected_files = thunar_launcher_set_selected_files;
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
  launcher->files_to_process = NULL;
  launcher->select_files_closure = NULL;
  launcher->device_to_process = NULL;

  /* grab a reference on the preferences */
  launcher->preferences = thunar_preferences_get ();
}



static void
thunar_launcher_dispose (GObject *object)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (object);

  /* reset our properties */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (launcher), NULL);
  thunar_launcher_set_widget (THUNAR_LAUNCHER (launcher), NULL);

  /* disconnect from the currently selected files */
  thunar_g_file_list_free (launcher->files_to_process);
  launcher->files_to_process = NULL;

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

    case PROP_WIDGET:
      thunar_launcher_set_widget (launcher, g_value_get_object (value));
      break;

    case PROP_SELECT_FILES_CLOSURE:
      launcher->select_files_closure = g_value_get_pointer (value);
      break;

    case PROP_SELECTED_DEVICE:
      launcher->device_to_process = g_value_get_pointer (value);
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
    {
      g_object_ref (G_OBJECT (current_directory));

      /* update files_to_process if not initialized yet */
      if (launcher->files_to_process == NULL)
        thunar_launcher_set_selected_files (THUNAR_COMPONENT (navigator), NULL);
    }

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_CURRENT_DIRECTORY]);
}



static void
thunar_launcher_set_selected_files (ThunarComponent *component,
                                    GList           *selected_files)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (component);
  GList          *lp;

  /* That happens at startup for some reason */
  if (launcher->current_directory == NULL)
    return;

  /* disconnect from the previous files to process */
  if (launcher->files_to_process != NULL)
    thunar_g_file_list_free (launcher->files_to_process);
  launcher->files_to_process = NULL;

  /* notify listeners */
  g_object_notify_by_pspec (G_OBJECT (launcher), launcher_props[PROP_SELECTED_FILES]);

  /* unref previous parent, if any */
  if (launcher->parent_folder != NULL)
    g_object_unref (launcher->parent_folder);
  launcher->parent_folder = NULL;

  launcher->files_are_selected = TRUE;
  if (selected_files == NULL || g_list_length (selected_files) == 0)
    launcher->files_are_selected = FALSE;

  launcher->files_to_process_trashable = TRUE;
  launcher->n_files_to_process         = 0;
  launcher->n_directories_to_process   = 0;
  launcher->n_regulars_to_process      = 0;
  launcher->files_are_all_executable   = TRUE;
  launcher->single_directory_to_process = FALSE;
  launcher->single_folder = NULL;
  launcher->parent_folder = NULL;

  /* if nothing is selected, the current directory is the folder to use for all menus */
  if (launcher->files_are_selected)
    launcher->files_to_process = thunar_g_file_list_copy (selected_files);
  else
    launcher->files_to_process = g_list_append (launcher->files_to_process, launcher->current_directory);

  /* determine the number of files/directories/executables */
  for (lp = launcher->files_to_process; lp != NULL; lp = lp->next, ++launcher->n_files_to_process)
    {
      /* Keep a reference on all selected files */
      g_object_ref (lp->data);

      if (thunar_file_is_directory (lp->data)
          || thunar_file_is_shortcut (lp->data)
          || thunar_file_is_mountable (lp->data))
        {
          ++launcher->n_directories_to_process;
        }
      else
        {
          if (launcher->files_are_all_executable && !thunar_file_is_executable (lp->data))
            launcher->files_are_all_executable = FALSE;
          ++launcher->n_regulars_to_process;
        }

      if (!thunar_file_can_be_trashed (lp->data))
        launcher->files_to_process_trashable = FALSE;
    }

  launcher->single_directory_to_process = (launcher->n_directories_to_process == 1 && launcher->n_files_to_process == 1);
  if (launcher->single_directory_to_process)
    {
      /* grab the folder of the first selected item */
      launcher->single_folder = THUNAR_FILE (launcher->files_to_process->data);
    }

  if (launcher->files_to_process != NULL)
    {
      /* just grab the folder of the first selected item */
      launcher->parent_folder = thunar_file_get_parent (THUNAR_FILE (launcher->files_to_process->data), NULL);
    }
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

  if (G_UNLIKELY (launcher->files_to_process == NULL))
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

  thunar_launcher_poke (launcher, app_info, action);
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

          /* Foreign data needs to be set explicitly to the new app_info */
          if (g_object_get_data (G_OBJECT (application_to_use), "skip-app-info-update") != NULL)
            g_object_set_data (G_OBJECT (app_info), "skip-app-info-update", GUINT_TO_POINTER (1));
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
thunar_launcher_poke (ThunarLauncher                 *launcher,
                      GAppInfo                       *application_to_use,
                      ThunarLauncherFolderOpenAction  folder_open_action)
{
  ThunarLauncherPokeData *poke_data;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

   if (launcher->files_to_process == NULL)
     {
       g_warning("No files to process. thunar_launcher_poke aborted.");
       return;
     }

   poke_data = thunar_launcher_poke_data_new (launcher->files_to_process, application_to_use, folder_open_action);

   if (launcher->device_to_process != NULL)
     {
       thunar_browser_poke_device (THUNAR_BROWSER (launcher), launcher->device_to_process,
                                   launcher->widget, thunar_launcher_poke_device_finish,
                                  poke_data);
     }
   else
     {
      // We will only poke one file at a time, in order to dont use all available CPU's
      // TODO: Check if that could cause slowness
      thunar_browser_poke_file (THUNAR_BROWSER (launcher), poke_data->files_to_poke->data,
                                launcher->widget, thunar_launcher_poke_files_finish,
                                poke_data);
     }
}



static void thunar_launcher_poke_device_finish (ThunarBrowser *browser,
                                                ThunarDevice  *volume,
                                                ThunarFile    *mount_point,
                                                GError        *error,
                                                gpointer       user_data,
                                                gboolean       cancelled)
{
  ThunarLauncherPokeData *poke_data = user_data;
  gchar                  *device_name;

  if (error != NULL)
    {
      device_name = thunar_device_get_name (volume);
      thunar_dialogs_show_error (GTK_WIDGET (THUNAR_LAUNCHER (browser)->widget), error, _("Failed to mount \"%s\""), device_name);
      g_free (device_name);
    }

  if (cancelled == TRUE || error != NULL || mount_point == NULL)
    {
      thunar_launcher_poke_data_free (poke_data);
      return;
    }

  if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_TAB)
    {
      thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (browser), mount_point);
    }
  else if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW)
    {
      GList *directories = NULL;
      directories = g_list_append (directories, mount_point);
      thunar_launcher_open_windows (THUNAR_LAUNCHER (browser), directories);
      g_list_free (directories);
    }
  else if (poke_data->folder_open_action == THUNAR_LAUNCHER_CHANGE_DIRECTORY)
    {
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (browser), mount_point);
    }

  thunar_launcher_poke_data_free (poke_data);
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
          else if (poke_data->folder_open_action == THUNAR_LAUNCHER_NO_ACTION)
            {
              // nothing to do
            }
          else
              g_warning("'folder_open_action' was not defined");
          g_list_free (directories);
        }

      /* check if we have any files to process */
      if (G_LIKELY (files != NULL))
        {
          /* if all files are executable, we just run them here */
          if (G_UNLIKELY (executable) && poke_data->application_to_use == NULL)
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
 * thunar_launcher_open_selected_folders:
 * @launcher : a #ThunarLauncher instance
 * @open_in_tabs : TRUE to open each folder in a new tab, FALSE to open each folder in a new window
 *
 * Will open each selected folder in a new tab/window
 **/
void thunar_launcher_open_selected_folders (ThunarLauncher *launcher,
                                            gboolean        open_in_tabs)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    _thunar_return_if_fail (thunar_file_is_directory (THUNAR_FILE (lp->data)));

  if (open_in_tabs)
    thunar_launcher_poke (launcher, NULL, THUNAR_LAUNCHER_OPEN_AS_NEW_TAB);
  else
    thunar_launcher_poke (launcher, NULL, THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW);
}



static void
thunar_launcher_action_open (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->files_to_process == NULL))
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

  if (G_UNLIKELY (launcher->files_to_process == NULL))
    return;

  thunar_launcher_open_selected_folders (launcher, TRUE);
}



static void
thunar_launcher_action_open_in_new_windows (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->files_to_process == NULL))
    return;

  thunar_launcher_open_selected_folders (launcher, FALSE);
}



static void
thunar_launcher_action_open_with_other (ThunarLauncher *launcher)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->n_files_to_process == 1)
    thunar_show_chooser_dialog (launcher->widget, launcher->files_to_process->data, TRUE);
}



/**
 * thunar_launcher_append_accelerators:
 * @launcher    : a #ThunarLauncher.
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 **/
void thunar_launcher_append_accelerators (ThunarLauncher *launcher,
                                          GtkAccelGroup  *accel_group)
{
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  xfce_gtk_accel_map_add_entries (thunar_launcher_action_entries, G_N_ELEMENTS (thunar_launcher_action_entries));
  xfce_gtk_accel_group_connect_action_entries (accel_group,
                                               thunar_launcher_action_entries,
                                               G_N_ELEMENTS (thunar_launcher_action_entries),
                                               launcher);
}



static gboolean
thunar_launcher_show_trash (ThunarLauncher *launcher)
{
  if (launcher->parent_folder == NULL)
    return FALSE;

  /* If the folder is read only, always show trash insensitive */
  /* If we are outside waste basket, the selection is trashable and we support trash, show trash */
  return !thunar_file_is_writable (launcher->parent_folder) || ( !thunar_file_is_trashed (launcher->parent_folder) && launcher->files_to_process_trashable && thunar_g_vfs_is_uri_scheme_supported ("trash"));
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
  GtkWidget                *focused_widget;
  gchar                    *label_text;
  gchar                    *tooltip_text;
  const XfceGtkActionEntry *action_entry = get_action_entry (action);
  gboolean                  show_delete_item;
  gboolean                  show_item;
  ThunarClipboardManager   *clipboard;
  ThunarFile               *parent;
  gint                      n;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);
  _thunar_return_val_if_fail (action_entry != NULL, NULL);

  /* This may occur when the thunar-window is build */
  if (G_UNLIKELY (launcher->files_to_process == NULL) && launcher->device_to_process == NULL)
    return NULL;

  switch (action)
    {
      case THUNAR_LAUNCHER_ACTION_OPEN: /* aka "activate" */
        return xfce_gtk_image_menu_item_new_from_icon_name (_("_Open"), ngettext ("Open the selected file", "Open the selected files", launcher->n_files_to_process),
                                           action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);

      case THUNAR_LAUNCHER_ACTION_EXECUTE:
        return xfce_gtk_image_menu_item_new_from_icon_name (_("_Execute"), ngettext ("Execute the selected file", "Execute the selected files", launcher->n_files_to_process),
                                           action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);

      case THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB:
        n = launcher->n_files_to_process > 0 ? launcher->n_files_to_process : 1;
        label_text = g_strdup_printf (ngettext ("Open in New _Tab", "Open in %d New _Tabs", n), n);
        tooltip_text = g_strdup_printf (ngettext ("Open the selected directory in new tab",
                                                  "Open the selected directories in %d new tabs", n), n);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        g_free (tooltip_text);
        g_free (label_text);
        return item;

      case THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW:
        n = launcher->n_files_to_process > 0 ? launcher->n_files_to_process : 1;
        label_text = g_strdup_printf (ngettext ("Open in New _Window", "Open in %d New _Windows", n), n);
        tooltip_text = g_strdup_printf (ngettext ("Open the selected directory in new window",
                                                  "Open the selected directories in %d new windows",n), n);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        g_free (tooltip_text);
        g_free (label_text);
        return item;

      case THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER:
        return xfce_gtk_menu_item_new (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);

      case THUNAR_LAUNCHER_ACTION_SENDTO_MENU:
        if (launcher->files_are_selected == FALSE)
          return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        submenu = thunar_launcher_build_sendto_submenu (launcher);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        return item;

      case THUNAR_LAUNCHER_ACTION_MAKE_LINK:
        show_item = thunar_file_is_writable (launcher->current_directory) &&
                    launcher->files_are_selected &&
                    thunar_file_is_trashed (launcher->current_directory) == FALSE;
        if (!show_item && !force)
          return NULL;

        label_text = ngettext ("Ma_ke Link", "Ma_ke Links", launcher->n_files_to_process);
        tooltip_text = ngettext ("Create a symbolic link for the selected file",
                                 "Create a symbolic link for each selected file", launcher->n_files_to_process);
        item = xfce_gtk_menu_item_new (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                       G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
        return item;

      case THUNAR_LAUNCHER_ACTION_DUPLICATE:
        show_item = thunar_file_is_writable (launcher->current_directory) &&
                    launcher->files_are_selected &&
                    thunar_file_is_trashed (launcher->current_directory) == FALSE;
        if (!show_item && !force)
          return NULL;
        item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
        return item;

      case THUNAR_LAUNCHER_ACTION_RENAME:
        show_item = thunar_file_is_writable (launcher->current_directory) &&
                    launcher->files_are_selected &&
                    thunar_file_is_trashed (launcher->current_directory) == FALSE;
        if (!show_item && !force)
          return NULL;
        tooltip_text = ngettext ("Rename the selected file",
                                 "Rename the selected files", launcher->n_files_to_process);
        item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                       action_entry->callback, G_OBJECT (launcher), menu);
        gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
        return item;

      case THUNAR_LAUNCHER_ACTION_RESTORE:
        if (launcher->files_are_selected && thunar_file_is_trashed (launcher->current_directory))
          {
            tooltip_text = ngettext ("Restore the selected file to its original location",
                                     "Restore the selected files to its original location", launcher->n_files_to_process);
            item = xfce_gtk_menu_item_new (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                           action_entry->callback, G_OBJECT (launcher), menu);
            gtk_widget_set_sensitive (item, thunar_file_is_writable (launcher->current_directory));
            return item;
          }
        return NULL;

      case THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH:
        if (!thunar_launcher_show_trash (launcher))
          return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
          return NULL;

        tooltip_text = ngettext ("Move the selected file to the Trash",
                                 "Move the selected files to the Trash", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
        return item;


      case THUNAR_LAUNCHER_ACTION_DELETE:
        g_object_get (G_OBJECT (launcher->preferences), "misc-show-delete-action", &show_delete_item, NULL);
        if (thunar_launcher_show_trash (launcher) && !show_delete_item)
          return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
          return NULL;

        tooltip_text = ngettext ("Permanently delete the selected file",
                                 "Permanently delete the selected files", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                            action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
        return item;

      case THUNAR_LAUNCHER_ACTION_EMPTY_TRASH:
        if (launcher->single_directory_to_process == TRUE)
          {
            if (thunar_file_is_root (launcher->single_folder) && thunar_file_is_trashed (launcher->single_folder))
              {
                item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text, action_entry->accel_path,
                                                                    action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
                gtk_widget_set_sensitive (item, thunar_file_get_item_count (launcher->single_folder) > 0);
                return item;
              }
          }
        return NULL;

      case THUNAR_LAUNCHER_ACTION_CREATE_FOLDER:
        if (THUNAR_IS_TREE_VIEW (launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
          parent = launcher->single_folder;
        else
          parent = launcher->current_directory;
        if (thunar_file_is_trashed (parent))
          return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        gtk_widget_set_sensitive (item, thunar_file_is_writable (parent));
        return item;

      case THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT:
        if (THUNAR_IS_TREE_VIEW (launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
          parent = launcher->single_folder;
        else
          parent = launcher->current_directory;
        if (thunar_file_is_trashed (parent))
          return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        submenu = thunar_launcher_create_document_submenu_new (launcher);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_widget_set_sensitive (item, thunar_file_is_writable (parent));
        return item;

      case THUNAR_LAUNCHER_ACTION_CUT:
        focused_widget = thunar_gtk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE (focused_widget))
          {
            item = xfce_gtk_image_menu_item_new_from_icon_name (
                          action_entry->menu_item_label_text,
                          N_ ("Cut the selection"),
                          action_entry->accel_path, G_CALLBACK (gtk_editable_cut_clipboard),
                          G_OBJECT (focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive (item, thunar_gtk_editable_can_cut (GTK_EDITABLE (focused_widget)));
          }
        else
          {
            show_item = launcher->files_are_selected;
            if (!show_item && !force)
              return NULL;
            tooltip_text = ngettext ("Prepare the selected file to be moved with a Paste command",
                                     "Prepare the selected files to be moved with a Paste command", launcher->n_files_to_process);
            item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                                action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive (item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable (launcher->parent_folder));
          }
        return item;

      case THUNAR_LAUNCHER_ACTION_COPY:
        focused_widget = thunar_gtk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE (focused_widget))
          {
            item = xfce_gtk_image_menu_item_new_from_icon_name (
                          action_entry->menu_item_label_text,
                          N_ ("Copy the selection"),
                          action_entry->accel_path,G_CALLBACK (gtk_editable_copy_clipboard),
                          G_OBJECT (focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive (item, thunar_gtk_editable_can_copy (GTK_EDITABLE (focused_widget)));
          }
        else
          {
            show_item = launcher->files_are_selected;
            if (!show_item && !force)
              return NULL;
            tooltip_text = ngettext ("Prepare the selected file to be copied with a Paste command",
                                    "Prepare the selected files to be copied with a Paste command", launcher->n_files_to_process);
            item = xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                                                action_entry->callback, G_OBJECT (launcher), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive (item, show_item);
          }
        return item;

      case THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER:
        if (!launcher->single_directory_to_process)
          return NULL;
        clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        gtk_widget_set_sensitive (item, thunar_clipboard_manager_get_can_paste (clipboard) && thunar_file_is_writable (launcher->single_folder));
        g_object_unref (clipboard);
        return item;

      case THUNAR_LAUNCHER_ACTION_PASTE:
        focused_widget = thunar_gtk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE (focused_widget))
          {
            item = xfce_gtk_image_menu_item_new_from_icon_name (
                          action_entry->menu_item_label_text,
                          N_ ("Paste the clipboard"),
                          action_entry->accel_path,G_CALLBACK (gtk_editable_paste_clipboard),
                          G_OBJECT (focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive (item, thunar_gtk_editable_can_paste (GTK_EDITABLE (focused_widget)));
          }
        else
          {
            if (launcher->single_directory_to_process && launcher->files_are_selected)
                return thunar_launcher_append_menu_item (launcher, menu, THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER, force);
            clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
            item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
            gtk_widget_set_sensitive (item, thunar_clipboard_manager_get_can_paste (clipboard) && thunar_file_is_writable (launcher->current_directory));
            g_object_unref (clipboard);
          }
        return item;

      case THUNAR_LAUNCHER_ACTION_MOUNT:
        if (launcher->device_to_process == NULL || thunar_device_is_mounted (launcher->device_to_process) == TRUE)
          return NULL;
        return xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));

      case THUNAR_LAUNCHER_ACTION_UNMOUNT:
        if (launcher->device_to_process == NULL || thunar_device_is_mounted (launcher->device_to_process) == FALSE)
          return NULL;
        return xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));

      case THUNAR_LAUNCHER_ACTION_EJECT:
        if (launcher->device_to_process == NULL || thunar_device_get_kind (launcher->device_to_process) != THUNAR_DEVICE_KIND_VOLUME)
          return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry (action_entry, G_OBJECT (launcher), GTK_MENU_SHELL (menu));
        gtk_widget_set_sensitive (item, thunar_device_can_eject (launcher->device_to_process));
        return item;

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
  files = thunar_file_list_to_thunar_g_file_list (launcher->files_to_process);
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
  files = thunar_file_list_to_thunar_g_file_list (launcher->files_to_process);
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
      for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
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
  if (launcher->n_directories_to_process > 0 && launcher->n_directories_to_process == launcher->n_files_to_process)
    {
      /* determine the toplevel window we belong to */
      window = gtk_widget_get_toplevel (launcher->widget);
      if (THUNAR_IS_WINDOW (window) && thunar_window_has_shortcut_sidepane (THUNAR_WINDOW (window)))
        {
          action_entry = get_action_entry (THUNAR_LAUNCHER_ACTION_SENDTO_SHORTCUTS);
          if (action_entry != NULL)
            {
              label_text   = ngettext ("Side Pane (Create Shortcut)", "Side Pane (Create Shortcuts)", launcher->n_files_to_process);
              tooltip_text = ngettext ("Add the selected folder to the shortcuts side pane",
                                       "Add the selected folders to the shortcuts side pane", launcher->n_files_to_process);
              item = xfce_gtk_image_menu_item_new_from_icon_name (label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                                                  G_OBJECT (launcher), action_entry->menu_item_icon_name, GTK_MENU_SHELL (submenu));
            }
        }
    }

  /* Check whether at least one files is located in the trash (to en-/disable the "sendto-desktop" action). */
  for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    {
      if (G_UNLIKELY (thunar_file_is_trashed (lp->data)))
        linkable = FALSE;
    }
  if (linkable)
    {
      action_entry = get_action_entry (THUNAR_LAUNCHER_ACTION_SENDTO_DESKTOP);
      if (action_entry != NULL)
        {
          label_text   = ngettext ("Desktop (Create Link)", "Desktop (Create Links)", launcher->n_files_to_process);
          tooltip_text = ngettext ("Create a link to the selected file on the desktop",
                                   "Create links to the selected files on the desktop", launcher->n_files_to_process);
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
                                                "Send the selected files to \"%s\"", launcher->n_files_to_process), label_text);
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
  appinfo_list = thunar_sendto_model_get_matching (sendto_model, launcher->files_to_process);
  g_object_unref (sendto_model);

  if (G_LIKELY (appinfo_list != NULL))
    {
      /* add all handlers to the user interface */
      for (lp = appinfo_list; lp != NULL; lp = lp->next)
        {
          /* generate a unique name and tooltip for the handler */
          label_text = g_strdup (g_app_info_get_name (lp->data));
          tooltip_text = g_strdup_printf (ngettext ("Send the selected file to \"%s\"",
                                                    "Send the selected files to \"%s\"", launcher->n_files_to_process), label_text);

          icon = g_app_info_get_icon (lp->data);
          image = NULL;
          if (G_LIKELY (icon != NULL))
              image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);

          item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated),
                                               G_OBJECT (launcher), image, GTK_MENU_SHELL (submenu));
          g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_appinfo_quark, g_object_ref (lp->data), g_object_unref);
          g_object_set_data (G_OBJECT (lp->data), "skip-app-info-update", GUINT_TO_POINTER (1));

          /* cleanup */
          g_free (label_text);
          g_free (tooltip_text);
        }

      /* release the appinfo list */
      g_list_free_full (appinfo_list, g_object_unref);
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
      if (launcher->files_to_process == NULL)
        {
          /* if we don't have any files selected, we just popup the properties dialog for the current folder. */
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), launcher->current_directory);
        }
      else
        {
          /* popup the properties dialog for all file(s) */
          thunar_properties_dialog_set_files (THUNAR_PROPERTIES_DIALOG (dialog), launcher->files_to_process);
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
  if (launcher->files_are_selected == FALSE || thunar_file_is_trashed (launcher->current_directory))
    return;

  for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    {
      g_files = g_list_append (g_files, thunar_file_get_file (lp->data));
    }
  /* link the selected files into the current directory, which effectively
   * creates new unique links for the files.
   */
  application = thunar_application_get ();
  thunar_application_link_into (application, launcher->widget, g_files,
                                thunar_file_get_file (launcher->current_directory), launcher->select_files_closure);
  g_object_unref (G_OBJECT (application));
  g_list_free (g_files);
}



static void
thunar_launcher_action_duplicate (ThunarLauncher *launcher)
{
  ThunarApplication *application;
  GList             *files_to_process;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_UNLIKELY (launcher->current_directory == NULL))
    return;
  if (launcher->files_are_selected == FALSE || thunar_file_is_trashed (launcher->current_directory))
    return;

  /* determine the selected files for the view */
  files_to_process = thunar_file_list_to_thunar_g_file_list (launcher->files_to_process);
  if (G_LIKELY (files_to_process != NULL))
    {
      /* copy the selected files into the current directory, which effectively
       * creates duplicates of the files.
       */
      application = thunar_application_get ();
      thunar_application_copy_into (application, launcher->widget, files_to_process,
                                    thunar_file_get_file (launcher->current_directory), launcher->select_files_closure);
      g_object_unref (G_OBJECT (application));

      /* clean up */
      thunar_g_file_list_free (files_to_process);
    }
}



/**
 * thunar_launcher_append_custom_actions:
 * @launcher : a #ThunarLauncher instance
 * @menu : #GtkMenuShell on which the custom actions should be appended
 *
 * Will append all custom actions which match the file-type to the provided #GtkMenuShell
 *
 * Return value: TRUE if any custom action was added
 **/
gboolean
thunar_launcher_append_custom_actions (ThunarLauncher *launcher,
                                       GtkMenuShell   *menu)
{
  gboolean                uca_added = FALSE;
  GtkWidget              *window;
  GtkWidget              *gtk_menu_item;
  ThunarxProviderFactory *provider_factory;
  GList                  *providers;
  GList                  *thunarx_menu_items = NULL;
  GList                  *lp_provider;
  GList                  *lp_item;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  /* determine the toplevel window we belong to */
  window = gtk_widget_get_toplevel (launcher->widget);

  /* load the menu providers from the provider factory */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  g_object_unref (provider_factory);

  if (G_UNLIKELY (providers == NULL))
    return FALSE;

  /* This may occur when the thunar-window is build */
  if (G_UNLIKELY (launcher->files_to_process == NULL))
    return FALSE;

  /* load the menu items offered by the menu providers */
  for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
      if (launcher->files_are_selected == FALSE)
        thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items (lp_provider->data, window, THUNARX_FILE_INFO (launcher->current_directory));
      else
        thunarx_menu_items = thunarx_menu_provider_get_file_menu_items (lp_provider->data, window, launcher->files_to_process);

      for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
          gtk_menu_item = thunar_gtk_menu_thunarx_menu_item_new (lp_item->data, menu);

          /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item*/
          g_signal_connect_swapped (G_OBJECT (gtk_menu_item), "destroy", G_CALLBACK (g_object_unref), lp_item->data);
          uca_added = TRUE;
        }
      g_list_free (thunarx_menu_items);
    }
  g_list_free_full (providers, g_object_unref);
  return uca_added;
}



gboolean
thunar_launcher_check_uca_key_activation (ThunarLauncher *launcher,
                                          GdkEventKey    *key_event)
{
  GtkWidget              *window;
  ThunarxProviderFactory *provider_factory;
  GList                  *providers;
  GList                  *thunarx_menu_items = NULL;
  GList                  *lp_provider;
  GList                  *lp_item;
  GtkAccelKey             uca_key;
  gchar                  *name, *accel_path;
  gboolean                uca_activated = FALSE;

  /* determine the toplevel window we belong to */
  window = gtk_widget_get_toplevel (launcher->widget);

  /* load the menu providers from the provider factory */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  g_object_unref (provider_factory);

  if (G_UNLIKELY (providers == NULL))
    return uca_activated;

  /* load the menu items offered by the menu providers */
  for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
      if (launcher->files_are_selected == FALSE)
        thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items (lp_provider->data, window, THUNARX_FILE_INFO (launcher->current_directory));
      else
        thunarx_menu_items = thunarx_menu_provider_get_file_menu_items (lp_provider->data, window, launcher->files_to_process);
      for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
          g_object_get (G_OBJECT (lp_item->data), "name", &name, NULL);
          accel_path = g_strconcat ("<Actions>/ThunarActions/", name, NULL);
          if (gtk_accel_map_lookup_entry (accel_path, &uca_key) == TRUE)
            {
              if (g_ascii_tolower (key_event->keyval) == g_ascii_tolower (uca_key.accel_key))
                {
                  if ((key_event->state & gtk_accelerator_get_default_mod_mask ()) == uca_key.accel_mods)
                    {
                      thunarx_menu_item_activate (lp_item->data);
                      uca_activated = TRUE;
                    }
                }
            }
          g_free (name);
          g_free (accel_path);
          g_object_unref (lp_item->data);
        }
      g_list_free (thunarx_menu_items);
    }
  g_list_free_full (providers, g_object_unref);
  return uca_activated;
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

  if (launcher->files_to_process == NULL || g_list_length (launcher->files_to_process) == 0)
    return;
  if (launcher->files_are_selected == FALSE || thunar_file_is_trashed (launcher->current_directory))
    return;

  /* get the window */
  window = gtk_widget_get_toplevel (launcher->widget);

  /* start renaming if we have exactly one selected file */
  if (g_list_length (launcher->files_to_process) == 1)
    {
      /* run the rename dialog */
      job = thunar_dialogs_show_rename_file (GTK_WINDOW (window), THUNAR_FILE (launcher->files_to_process->data));
      if (G_LIKELY (job != NULL))
        {
          g_signal_connect (job, "error", G_CALLBACK (thunar_launcher_rename_error), launcher->widget);
          g_signal_connect (job, "finished", G_CALLBACK (thunar_launcher_rename_finished), launcher->widget);
        }
    }
  else
    {
      /* display the bulk rename dialog */
      thunar_show_renamer_dialog (GTK_WIDGET (window), launcher->current_directory, launcher->files_to_process, FALSE, NULL);
    }
}



static void
thunar_launcher_action_restore (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->files_are_selected == FALSE || !thunar_file_is_trashed (launcher->current_directory))
    return;

  /* restore the selected files */
  application = thunar_application_get ();
  thunar_application_restore_files (application, launcher->widget, launcher->files_to_process, NULL);
  g_object_unref (G_OBJECT (application));
}


static void
thunar_launcher_action_move_to_trash (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->parent_folder == NULL || launcher->files_are_selected == FALSE)
    return;

  application = thunar_application_get ();
  thunar_application_unlink_files (application, launcher->widget, launcher->files_to_process, FALSE);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_action_delete (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->parent_folder == NULL || launcher->files_are_selected == FALSE)
    return;

  application = thunar_application_get ();
  thunar_application_unlink_files (application, launcher->widget, launcher->files_to_process, TRUE);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_launcher_action_trash_delete (ThunarLauncher *launcher)
{
  GdkModifierType event_state;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* when shift modifier is pressed, we delete (as well via context menu) */
  if (gtk_get_current_event_state (&event_state) && (event_state & GDK_SHIFT_MASK) != 0)
    thunar_launcher_action_delete (launcher);
  else if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    thunar_launcher_action_move_to_trash (launcher);
  else
    thunar_launcher_action_delete (launcher);
}



static void
thunar_launcher_action_empty_trash (ThunarLauncher *launcher)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->single_directory_to_process == FALSE)
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

  if (thunar_file_is_trashed (launcher->current_directory))
    return;

  /* ask the user to enter a name for the new folder */
  name = thunar_dialogs_show_create (launcher->widget,
                                     "inode/directory",
                                     _("New Folder"),
                                     _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* fake the path list */
      if (THUNAR_IS_TREE_VIEW (launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
        path_list.data = g_file_resolve_relative_path (thunar_file_get_file (launcher->single_folder), name);
      else
        path_list.data = g_file_resolve_relative_path (thunar_file_get_file (launcher->current_directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      application = thunar_application_get ();
      thunar_application_mkdir (application, launcher->widget, &path_list, launcher->select_files_closure);
      g_object_unref (G_OBJECT (application));

      /* release the path */
      g_object_unref (path_list.data);

      /* release the file name */
      g_free (name);
    }
}



static void
thunar_launcher_action_create_document (ThunarLauncher *launcher,
                                        GtkWidget      *menu_item)
{
  ThunarApplication *application;
  GList              target_path_list;
  gchar             *name;
  gchar             *title;
  ThunarFile        *template_file;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (thunar_file_is_trashed (launcher->current_directory))
    return;

  template_file = g_object_get_qdata (G_OBJECT (menu_item), thunar_launcher_file_quark);

  if (template_file != NULL)
    {
      /* generate a title for the create dialog */
      title = g_strdup_printf (_("Create Document from template \"%s\""),
                               thunar_file_get_display_name (template_file));

      /* ask the user to enter a name for the new document */
      name = thunar_dialogs_show_create (launcher->widget,
                                         thunar_file_get_content_type (THUNAR_FILE (template_file)),
                                         thunar_file_get_display_name (template_file),
                                         title);
      /* cleanup */
      g_free (title);
    }
  else
    {
      /* ask the user to enter a name for the new empty file */
      name = thunar_dialogs_show_create (launcher->widget,
                                         "text/plain",
                                         _("New Empty File"),
                                         _("New Empty File..."));
    }

  if (G_LIKELY (name != NULL))
    {
      if (G_LIKELY (launcher->parent_folder != NULL))
        {
          /* fake the target path list */
          if (THUNAR_IS_TREE_VIEW (launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
            target_path_list.data = g_file_get_child (thunar_file_get_file (launcher->single_folder), name);
          else
            target_path_list.data = g_file_get_child (thunar_file_get_file (launcher->current_directory), name);
          target_path_list.next = NULL;
          target_path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_creat (application, launcher->widget, &target_path_list,
                                    template_file != NULL ? thunar_file_get_file (template_file) : NULL,
                                    launcher->select_files_closure);
          g_object_unref (G_OBJECT (application));

          /* release the target path */
          g_object_unref (target_path_list.data);
        }

      /* release the file name */
      g_free (name);
    }
}



/* helper method in order to find the parent menu for a menu item */
static GtkWidget *
thunar_launcher_create_document_submenu_templates_find_parent_menu (ThunarFile *file,
                                                                    GList      *dirs,
                                                                    GList      *items)
{
  GtkWidget *parent_menu = NULL;
  GFile     *parent;
  GList     *lp;
  GList     *ip;

  /* determine the parent of the file */
  parent = g_file_get_parent (thunar_file_get_file (file));

  /* check if the file has a parent at all */
  if (parent == NULL)
    return NULL;

  /* iterate over all dirs and menu items */
  for (lp = g_list_first (dirs), ip = g_list_first (items);
       parent_menu == NULL && lp != NULL && ip != NULL;
       lp = lp->next, ip = ip->next)
    {
      /* check if the current dir/item is the parent of our file */
      if (g_file_equal (parent, thunar_file_get_file (lp->data)))
        {
          /* we want to insert an item for the file in this menu */
          parent_menu = gtk_menu_item_get_submenu (ip->data);
        }
    }

  /* destroy the parent GFile */
  g_object_unref (parent);

  return parent_menu;
}



/* recursive helper method in order to create menu items for all available templates */
static gboolean
thunar_launcher_create_document_submenu_templates (ThunarLauncher *launcher,
                                                   GtkWidget      *create_file_submenu,
                                                   GList          *files)
{
  ThunarIconFactory *icon_factory;
  ThunarFile        *file;
  GdkPixbuf         *icon;
  GtkWidget         *parent_menu;
  GtkWidget         *submenu;
  GtkWidget         *image;
  GtkWidget         *item;
  GList             *lp;
  GList             *dirs = NULL;
  GList             *items = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), FALSE);

  /* do nothing if there is no menu */
  if (create_file_submenu == NULL)
    return FALSE;

  /* get the icon factory */
  icon_factory = thunar_icon_factory_get_default ();

  /* sort items so that directories come before files and ancestors come
   * before descendants */
  files = g_list_sort (files, (GCompareFunc) (void (*)(void)) thunar_file_compare_by_type);

  for (lp = g_list_first (files); lp != NULL; lp = lp->next)
    {
      file = lp->data;

      /* determine the parent menu for this file/directory */
      parent_menu = thunar_launcher_create_document_submenu_templates_find_parent_menu (file, dirs, items);
      if (parent_menu == NULL)
        parent_menu = create_file_submenu;

      /* determine the icon for this file/directory */
      icon = thunar_icon_factory_load_file_icon (icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 16);

      /* allocate an image based on the icon */
      image = gtk_image_new_from_pixbuf (icon);

      item = xfce_gtk_image_menu_item_new (thunar_file_get_display_name (file), NULL, NULL, NULL, NULL, image, GTK_MENU_SHELL (parent_menu));
      if (thunar_file_is_directory (file))
        {
          /* allocate a new submenu for the directory */
          submenu = gtk_menu_new ();

          /* allocate a new menu item for the directory */
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

          /* prepend the directory, its item and the parent menu it should
           * later be added to to the respective lists */
          dirs = g_list_prepend (dirs, file);
          items = g_list_prepend (items, item);
        }
      else
        {
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_launcher_action_create_document), launcher);
          g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_file_quark, g_object_ref (file), g_object_unref);
        }
      /* release the icon reference */
      g_object_unref (icon);
    }

  /* destroy lists */
  g_list_free (dirs);
  g_list_free (items);

  /* release the icon factory */
  g_object_unref (icon_factory);

  /* let the job destroy the file list */
  return FALSE;
}



/**
 * thunar_launcher_create_document_submenu_new:
 * @launcher : a #ThunarLauncher instance
 *
 * Will create a complete 'create_document* #GtkMenu and return it
 *
 * Return value: (transfer full): the created #GtkMenu
 **/
static GtkWidget*
thunar_launcher_create_document_submenu_new (ThunarLauncher *launcher)
{
  GList           *files = NULL;
  GFile           *home_dir;
  GFile           *templates_dir = NULL;
  const gchar     *path;
  gchar           *template_path;
  gchar           *label_text;
  GtkWidget       *submenu;
  GtkWidget       *item;

  _thunar_return_val_if_fail (THUNAR_IS_LAUNCHER (launcher), NULL);

  home_dir = thunar_g_file_new_for_home ();
  path = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);

  if (G_LIKELY (path != NULL))
    templates_dir = g_file_new_for_path (path);

  /* If G_USER_DIRECTORY_TEMPLATES not found, set "~/Templates" directory as default */
  if (G_UNLIKELY (path == NULL) || G_UNLIKELY (g_file_equal (templates_dir, home_dir)))
    {
      if (templates_dir != NULL)
        g_object_unref (templates_dir);
      templates_dir = g_file_resolve_relative_path (home_dir, "Templates");
    }

  if (G_LIKELY (templates_dir != NULL))
    {
      /* load the ThunarFiles */
      files = thunar_io_scan_directory (NULL, templates_dir, G_FILE_QUERY_INFO_NONE, TRUE, FALSE, TRUE, NULL);
    }

  submenu = gtk_menu_new();
  if (files == NULL)
    {
      template_path = g_file_get_path (templates_dir);
      label_text = g_strdup_printf (_("No templates installed in \"%s\""), template_path);
      item = xfce_gtk_image_menu_item_new (label_text, NULL, NULL, NULL, NULL, NULL, GTK_MENU_SHELL (submenu));
      gtk_widget_set_sensitive (item, FALSE);
      g_free (template_path);
      g_free (label_text);
    }
  else
    {
      thunar_launcher_create_document_submenu_templates (launcher, submenu, files);
      thunar_g_file_list_free (files);
    }

  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_image_menu_item_new_from_icon_name (_("_Empty File"), NULL, NULL, G_CALLBACK (thunar_launcher_action_create_document),
                                               G_OBJECT (launcher), "text-x-generic", GTK_MENU_SHELL (submenu));


  g_object_unref (templates_dir);
  g_object_unref (home_dir);

  return submenu;
}



static void
thunar_launcher_action_cut (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->files_are_selected == FALSE || launcher->parent_folder == NULL)
    return;

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_cut_files (clipboard, launcher->files_to_process);
  g_object_unref (G_OBJECT (clipboard));
}



static void
thunar_launcher_action_copy (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (launcher->files_are_selected == FALSE)
    return;

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_copy_files (clipboard, launcher->files_to_process);
  g_object_unref (G_OBJECT (clipboard));
}



static void
thunar_launcher_action_paste (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_file (launcher->current_directory), launcher->widget, launcher->select_files_closure);
  g_object_unref (G_OBJECT (clipboard));
}



static void
thunar_launcher_action_paste_into_folder (ThunarLauncher *launcher)
{
  ThunarClipboardManager *clipboard;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (!launcher->single_directory_to_process)
    return;

  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (launcher->widget));
  thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_file (launcher->single_folder), launcher->widget, launcher->select_files_closure);
  g_object_unref (G_OBJECT (clipboard));
}


/**
 * thunar_launcher_action_mount:
 * @launcher : a #ThunarLauncher instance
*
 * Will mount the selected device, if any. The related folder will not be opened.
 **/
void
thunar_launcher_action_mount (ThunarLauncher *launcher)
{
  thunar_launcher_poke (launcher, NULL,THUNAR_LAUNCHER_NO_ACTION);
}



static void
thunar_launcher_action_eject_finish (ThunarDevice  *device,
                                      const GError *error,
                                      gpointer      user_data)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (user_data);
  gchar          *device_name;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* check if there was an error */
  if (error != NULL)
    {
      /* display an error dialog to inform the user */
      device_name = thunar_device_get_name (device);
      thunar_dialogs_show_error (GTK_WIDGET (launcher->widget), error, _("Failed to eject \"%s\""), device_name);
      g_free (device_name);
    }
  else
    launcher->device_to_process = NULL;

  g_object_unref (launcher);
}



/**
 * thunar_launcher_action_eject:
 * @launcher : a #ThunarLauncher instance
*
 * Will eject the selected device, if any
 **/
void
thunar_launcher_action_eject (ThunarLauncher *launcher)
{
  GMountOperation *mount_operation;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_LIKELY (launcher->device_to_process != NULL))
    {
      /* prepare a mount operation */
      mount_operation = thunar_gtk_mount_operation_new (GTK_WIDGET (launcher->widget));

      /* eject */
      thunar_device_eject (launcher->device_to_process,
                           mount_operation,
                           NULL,
                           thunar_launcher_action_eject_finish,
                           g_object_ref (launcher));

      g_object_unref (mount_operation);
    }
}



static void
thunar_launcher_action_unmount_finish (ThunarDevice *device,
                                       const GError *error,
                                       gpointer      user_data)
{
  ThunarLauncher *launcher = THUNAR_LAUNCHER (user_data);
  gchar          *device_name;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  /* check if there was an error */
  if (error != NULL)
    {
      /* display an error dialog to inform the user */
      device_name = thunar_device_get_name (device);
      thunar_dialogs_show_error (GTK_WIDGET (launcher->widget), error, _("Failed to unmount \"%s\""), device_name);
      g_free (device_name);
    }
  else
    launcher->device_to_process = NULL;

  g_object_unref (launcher);
}



/**
 * thunar_launcher_action_eject:
 * @launcher : a #ThunarLauncher instance
*
 * Will unmount the selected device, if any
 **/
void
thunar_launcher_action_unmount (ThunarLauncher *launcher)
{
  GMountOperation *mount_operation;

  _thunar_return_if_fail (THUNAR_IS_LAUNCHER (launcher));

  if (G_LIKELY (launcher->device_to_process != NULL))
    {
      /* prepare a mount operation */
      mount_operation = thunar_gtk_mount_operation_new (GTK_WIDGET (launcher->widget));

      /* eject */
      thunar_device_unmount (launcher->device_to_process,
                             mount_operation,
                             NULL,
                             thunar_launcher_action_unmount_finish,
                             g_object_ref (launcher));

      /* release the device */
      g_object_unref (mount_operation);
    }
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
                                           launcher->n_files_to_process), g_app_info_get_name (lp->data));
      image = gtk_image_new_from_gicon (g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
      item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated), G_OBJECT (launcher), image, GTK_MENU_SHELL (submenu));
      g_object_set_qdata_full (G_OBJECT (item), thunar_launcher_appinfo_quark, g_object_ref (lp->data), g_object_unref);
      g_free (tooltip_text);
      g_free (label_text);
    }

  if (launcher->n_files_to_process == 1)
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
  if (launcher->files_are_selected == FALSE && !force)
    return FALSE;

  /* determine the set of applications that work for all selected files */
  applications = thunar_file_list_get_applications (launcher->files_to_process);

  /* Execute OR Open OR OpenWith */
  if (G_UNLIKELY (launcher->files_are_all_executable))
    thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_EXECUTE, FALSE);
  else if (G_LIKELY (launcher->n_directories_to_process >= 1))
    {
      if (support_change_directory)
        thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN, FALSE);
    }
  else if (G_LIKELY (applications != NULL))
    {
      label_text = g_strdup_printf (_("_Open With \"%s\""), g_app_info_get_name (applications->data));
      tooltip_text = g_strdup_printf (ngettext ("Use \"%s\" to open the selected file",
                                                "Use \"%s\" to open the selected files",
                                                launcher->n_files_to_process), g_app_info_get_name (applications->data));

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
                                                "Open the selected files with the default applications", launcher->n_files_to_process));
      xfce_gtk_menu_item_new (label_text, tooltip_text, NULL, G_CALLBACK (thunar_launcher_menu_item_activated), G_OBJECT (launcher), menu);
      g_free (tooltip_text);
      g_free (label_text);
    }

  if (launcher->n_files_to_process == launcher->n_directories_to_process && launcher->n_directories_to_process >= 1)
    {
      if (support_tabs)
        thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB, FALSE);
      thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW, FALSE);
    }

  if (G_LIKELY (applications != NULL))
    {
      menu_item = xfce_gtk_menu_item_new (_("Open With"),
                                          _("Choose another application with which to open the selected file"),
                                          NULL, NULL, NULL, menu);
      submenu = thunar_launcher_build_application_submenu (launcher, applications);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);
    }
  else
    {
      if (launcher->n_files_to_process == 1)
        thunar_launcher_append_menu_item (launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
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
