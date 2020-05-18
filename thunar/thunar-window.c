/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-compact-view.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-menu.h>
#include <thunar/thunar-menu-util.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-trash-action.h>
#include <thunar/thunar-tree-pane.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-window-ui.h>
#include <thunar/thunar-device-monitor.h>

#include <glib.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_UI_MANAGER,
  PROP_ZOOM_LEVEL,
};

/* Signal identifiers */
enum
{
  BACK,
  RELOAD,
  TOGGLE_SIDEPANE,
  TOGGLE_MENUBAR,
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_RESET,
  TAB_CHANGE,
  LAST_SIGNAL,
};



static void      thunar_window_dispose                    (GObject                *object);
static void      thunar_window_finalize                   (GObject                *object);
static gboolean  thunar_window_delete                     (GtkWidget              *widget,
                                                           GdkEvent               *event,
                                                           gpointer                data);
static void      thunar_window_get_property               (GObject                *object,
                                                           guint                   prop_id,
                                                           GValue                 *value,
                                                           GParamSpec             *pspec);
static void      thunar_window_set_property               (GObject                *object,
                                                           guint                   prop_id,
                                                           const GValue           *value,
                                                           GParamSpec             *pspec);
static gboolean thunar_window_back                        (ThunarWindow           *window);
static gboolean thunar_window_reload                      (ThunarWindow           *window,
                                                           gboolean                reload_info);
static gboolean  thunar_window_toggle_sidepane            (ThunarWindow           *window);
static gboolean  thunar_window_zoom_in                    (ThunarWindow           *window);
static gboolean  thunar_window_zoom_out                   (ThunarWindow           *window);
static gboolean  thunar_window_zoom_reset                 (ThunarWindow           *window);
static gboolean  thunar_window_tab_change                 (ThunarWindow           *window,
                                                           gint                    nth);
static void      thunar_window_realize                    (GtkWidget              *widget);
static void      thunar_window_unrealize                  (GtkWidget              *widget);
static gboolean  thunar_window_configure_event            (GtkWidget              *widget,
                                                           GdkEventConfigure      *event);
static void      thunar_window_notebook_switch_page       (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void      thunar_window_notebook_page_added        (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void      thunar_window_notebook_page_removed      (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_notebook_button_press_event(GtkWidget              *notebook,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean  thunar_window_notebook_popup_menu        (GtkWidget              *notebook,
                                                           ThunarWindow           *window);
static gpointer  thunar_window_notebook_create_window     (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           gint                    x,
                                                           gint                    y,
                                                           ThunarWindow           *window);
static void     thunar_window_merge_custom_preferences    (ThunarWindow           *window);
static gboolean thunar_window_bookmark_merge              (gpointer                user_data);
static void      thunar_window_update_location_bar_visible(ThunarWindow           *window);
static void      thunar_window_handle_reload_request      (ThunarWindow           *window);
static void      thunar_window_install_sidepane           (ThunarWindow           *window,
                                                           GType                   type);
static void      thunar_window_start_open_location        (ThunarWindow           *window,
                                                           const gchar            *initial_text);
static void      thunar_window_action_open_new_tab        (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_open_new_window     (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_detach_tab          (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_close_all_windows   (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_close_tab           (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_close_window        (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_preferences         (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_reload              (ThunarWindow           *window,
                                                           GtkWidget              *menu_item);
static void      thunar_window_action_switch_next_tab     (ThunarWindow           *window);
static void      thunar_window_action_switch_previous_tab (ThunarWindow           *window);
static void      thunar_window_action_pathbar_changed     (ThunarWindow           *window);
static void      thunar_window_action_toolbar_changed     (ThunarWindow           *window);
static void      thunar_window_action_shortcuts_changed   (ThunarWindow           *window);
static void      thunar_window_action_tree_changed        (ThunarWindow           *window);
static void      thunar_window_action_statusbar_changed   (ThunarWindow           *window);
static void      thunar_window_action_menubar_changed     (ThunarWindow           *window);
static void      thunar_window_action_detailed_view       (ThunarWindow           *window);
static void      thunar_window_action_icon_view           (ThunarWindow           *window);
static void      thunar_window_action_compact_view        (ThunarWindow           *window);
static void      thunar_window_action_view_changed        (ThunarWindow           *window,
                                                           GType                   view_type);
static void      thunar_window_action_go_up               (ThunarWindow           *window);
static void      thunar_window_action_open_home           (ThunarWindow           *window);
static void      thunar_window_action_open_desktop        (ThunarWindow           *window);
static void      thunar_window_action_open_computer       (ThunarWindow           *window);
static void      thunar_window_action_open_templates      (ThunarWindow           *window);
static void      thunar_window_action_open_file_system    (ThunarWindow           *window);
static void      thunar_window_action_open_trash          (ThunarWindow           *window);
static void      thunar_window_action_open_network        (ThunarWindow           *window);
static void      thunar_window_action_open_bookmark       (GtkWidget              *menu_item);
static void      thunar_window_action_open_location       (ThunarWindow           *window);
static void      thunar_window_action_contents            (ThunarWindow           *window);
static void      thunar_window_action_about               (ThunarWindow           *window);
static void      thunar_window_action_show_hidden         (ThunarWindow           *window);
static gboolean  thunar_window_propagate_key_event        (GtkWindow              *window,
                                                           GdkEvent               *key_event,
                                                           gpointer                user_data);
static void      thunar_window_action_open_file_menu      (ThunarWindow           *window);
static void      thunar_window_current_directory_changed  (ThunarFile             *current_directory,
                                                           ThunarWindow           *window);
static void     thunar_window_connect_proxy               (GtkUIManager           *manager,
                                                           GtkAction              *action,
                                                           GtkWidget              *proxy,
                                                           ThunarWindow           *window);
static void     thunar_window_disconnect_proxy            (GtkUIManager           *manager,
                                                           GtkAction              *action,
                                                           GtkWidget              *proxy,
                                                           ThunarWindow           *window);
static void     thunar_window_menu_item_selected          (GtkWidget              *menu_item,
                                                           ThunarWindow           *window);
static void     thunar_window_menu_item_deselected        (GtkWidget              *menu_item,
                                                           ThunarWindow           *window);
static void     thunar_window_update_custom_actions       (ThunarView             *view,
                                                           GParamSpec             *pspec,
                                                           ThunarWindow           *window);
static void      thunar_window_notify_loading             (ThunarView             *view,
                                                           GParamSpec             *pspec,
                                                           ThunarWindow           *window);
static void      thunar_window_device_pre_unmount         (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           GFile                  *root_file,
                                                           ThunarWindow           *window);
static void      thunar_window_device_changed             (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           ThunarWindow           *window);
static gboolean thunar_window_merge_idle                  (gpointer                user_data);
static void     thunar_window_merge_idle_destroy          (gpointer                user_data);
static gboolean  thunar_window_save_paned                 (ThunarWindow           *window);
static gboolean  thunar_window_save_geometry_timer        (gpointer                user_data);
static void      thunar_window_save_geometry_timer_destroy(gpointer                user_data);
static void      thunar_window_set_zoom_level             (ThunarWindow           *window,
                                                           ThunarZoomLevel         zoom_level);
static void      thunar_window_update_window_icon         (ThunarWindow           *window);
static gboolean  thunar_window_create_file_menu           (ThunarWindow           *window,
                                                           GdkEventCrossing       *event,
                                                           GtkWidget              *menu);
static gboolean  thunar_window_create_edit_menu           (ThunarWindow           *window,
                                                           GdkEventCrossing       *event,
                                                           GtkWidget              *menu);
static gboolean  thunar_window_create_view_menu           (ThunarWindow           *window,
                                                           GdkEventCrossing        *event,
                                                           GtkWidget              *menu);
static gboolean  thunar_window_create_go_menu             (ThunarWindow           *window,
                                                           GdkEventCrossing        *event,
                                                           GtkWidget              *menu);
static gboolean  thunar_window_create_help_menu           (ThunarWindow           *window,
                                                           GdkEventCrossing        *event,
                                                           GtkWidget              *menu);
static void      thunar_window_select_files               (ThunarWindow           *window,
                                                           GList                  *path_list);
static void      thunar_window_binding_create             (ThunarWindow           *window,
                                                           gpointer                src_object,
                                                           const gchar            *src_prop,
                                                           gpointer                dst_object,
                                                           const                   gchar *dst_prop,
                                                           GBindingFlags           flags);
static gboolean  thunar_window_menu_item_hovered          (ThunarWindow           *window,
                                                           GdkEventCrossing       *event,
                                                           GtkWidget              *menu);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;

  /* internal action signals */
  gboolean (*back)            (ThunarWindow *window);
  gboolean (*reload)          (ThunarWindow *window,
                               gboolean      reload_info);
  gboolean (*zoom_in)         (ThunarWindow *window);
  gboolean (*zoom_out)        (ThunarWindow *window);
  gboolean (*zoom_reset)      (ThunarWindow *window);
  gboolean (*tab_change)      (ThunarWindow *window,
                               gint          idx);
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  /* support for custom preferences actions */
  ThunarxProviderFactory *provider_factory;
  guint                   custom_preferences_merge_id;

  /* UI manager merge ID for go menu actions */
  guint                   go_items_actions_merge_id;

  /* UI manager merge ID for the bookmark actions */
  guint                   bookmark_items_actions_merge_id;
  GtkActionGroup         *bookmark_action_group;
  GFile                  *bookmark_file;
  GFileMonitor           *bookmark_monitor;
  guint                   bookmark_reload_idle_id;

  ThunarClipboardManager *clipboard;

  ThunarPreferences      *preferences;

  ThunarIconFactory      *icon_factory;

  GtkActionGroup         *action_group;
  GtkUIManager           *ui_manager;

  /* to be able to change folder on "device-pre-unmount" if required */
  ThunarDeviceMonitor    *device_monitor;

  /* closures for the menu_item_selected()/menu_item_deselected() callbacks */
  GClosure               *menu_item_selected_closure;
  GClosure               *menu_item_deselected_closure;

  /* custom menu actions for the file menu */
  GtkActionGroup         *custom_actions;
  guint                   custom_merge_id;

  GtkWidget              *grid;
  GtkWidget              *menubar;
  GtkWidget              *spinner;
  GtkWidget              *paned;
  GtkWidget              *sidepane;
  GtkWidget              *view_box;
  GtkWidget              *notebook;
  GtkWidget              *view;
  GtkWidget              *statusbar;

  GType                   view_type;
  GSList                 *view_bindings;

  /* support for two different styles of location bars */
  GtkWidget              *location_bar;
  GtkWidget              *location_toolbar;

  ThunarLauncher         *launcher;

  ThunarFile             *current_directory;
  GtkAccelGroup          *accel_group;

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* menu merge idle source */
  guint                   merge_idle_id;

  gboolean                show_hidden;

  /* support to remember window geometry */
  guint                   save_geometry_timer_id;

  /* support to toggle side pane using F9,
   * see the toggle_sidepane() function.
   */
  GType                   toggle_sidepane_type;

  /* Takes care to select a file after e.g. rename/create */
  GClosure               *select_files_closure;
};



static GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "new-tab", "tab-new", N_ ("New _Tab"), "<control>T", N_ ("Open a new tab for the displayed location"), G_CALLBACK (NULL), },
  { "new-window", "window-new", N_ ("New _Window"), "<control>N", N_ ("Open a new Thunar window for the displayed location"), G_CALLBACK (NULL), },
  { "sendto-menu", NULL, N_ ("_Send To"), NULL, },
  { "empty-trash", NULL, N_ ("_Empty Trash"), NULL, N_ ("Delete all files and folders in the Trash"), G_CALLBACK (NULL), },
  { "detach-tab", NULL, N_ ("Detac_h Tab"), NULL, N_ ("Open current folder in a new window"), G_CALLBACK (NULL), },
  { "switch-previous-tab", "go-previous", N_ ("_Previous Tab"), "<control>Page_Up", N_ ("Switch to Previous Tab"), G_CALLBACK (NULL), },
  { "switch-next-tab", "go-next", N_ ("_Next Tab"), "<control>Page_Down", N_ ("Switch to Next Tab"), G_CALLBACK (NULL), },
  { "close-all-windows", NULL, N_ ("Close _All Windows"), "<control><shift>W", N_ ("Close all Thunar windows"), G_CALLBACK (NULL), },
  { "close-tab", "window-close", N_ ("C_lose Tab"), "<control>W", N_ ("Close this folder"), G_CALLBACK (NULL), },
  { "close-window", "application-exit", N_ ("_Close Window"), "<control>Q", N_ ("Close this window"), G_CALLBACK (NULL), },
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "preferences", "preferences-system", N_ ("Pr_eferences..."), NULL, N_ ("Edit Thunars Preferences"), G_CALLBACK (NULL), },
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "reload", "view-refresh-symbolic", N_ ("_Reload"), "<control>R", N_ ("Reload the current folder"), G_CALLBACK (NULL), },
  { "view-location-selector-menu", NULL, N_ ("_Location Selector"), NULL, },
  { "view-side-pane-menu", NULL, N_ ("_Side Pane"), NULL, },
  { "zoom-in", "zoom-in-symbolic", N_ ("Zoom I_n"), "<control>plus", N_ ("Show the contents in more detail"), G_CALLBACK (NULL), },
  { "zoom-in-alt", NULL, "zoom-in-alt", "<control>equal", NULL, G_CALLBACK (NULL), },
  { "zoom-out", "zoom-out-symbolic", N_ ("Zoom _Out"), "<control>minus", N_ ("Show the contents in less detail"), G_CALLBACK (NULL), },
  { "zoom-reset", "zoom-original-symbolic", N_ ("Normal Si_ze"), "<control>0", N_ ("Show the contents at the normal size"), G_CALLBACK (NULL), },
  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "open-parent", "go-up-symbolic", N_ ("Open _Parent"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (NULL), },
  { "open-home", "go-home-symbolic", N_ ("_Home"), "<alt>Home", N_ ("Go to the home folder"), G_CALLBACK (NULL), },
  { "open-desktop", "user-desktop", N_ ("Desktop"), NULL, N_ ("Go to the desktop folder"), G_CALLBACK (NULL), },
  { "open-computer", "computer", N_ ("Computer"), NULL, N_ ("Browse all local and remote disks and folders accessible from this computer"), G_CALLBACK (NULL), },
  { "open-file-system", "drive-harddisk", N_ ("File System"), NULL, N_ ("Browse the file system"), G_CALLBACK (NULL), },
  { "open-network", "network-workgroup", N_("B_rowse Network"), NULL, N_ ("Browse local network connections"), G_CALLBACK (NULL), },
  { "open-templates", "text-x-generic-template", N_("T_emplates"), NULL, N_ ("Go to the templates folder"), G_CALLBACK (NULL), },
  { "open-location", NULL, N_ ("_Open Location..."), "<control>L", N_ ("Specify a location to open"), G_CALLBACK (NULL), },
  { "open-location-alt", NULL, "open-location-alt", "<alt>D", NULL, G_CALLBACK (NULL), },
  { "help-menu", NULL, N_ ("_Help"), NULL, },
  { "contents", "help-browser", N_ ("_Contents"), "F1", N_ ("Display Thunar user manual"), G_CALLBACK (NULL), },
  { "about", "help-about", N_ ("_About"), NULL, N_ ("Display information about Thunar"), G_CALLBACK (NULL), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "show-hidden", NULL, N_ ("Show _Hidden Files"), "<control>H", N_ ("Toggles the display of hidden files in the current window"), G_CALLBACK (NULL), FALSE, },
  { "view-location-selector-pathbar", NULL, N_ ("_Pathbar Style"), NULL, N_ ("Modern approach with buttons that correspond to folders"), G_CALLBACK (NULL), FALSE, },
  { "view-location-selector-toolbar", NULL, N_ ("_Toolbar Style"), NULL, N_ ("Traditional approach with location bar and navigation buttons"), G_CALLBACK (NULL), FALSE, },
  { "view-side-pane-shortcuts", NULL, N_ ("_Shortcuts"), "<control>B", N_ ("Toggles the visibility of the shortcuts pane"), G_CALLBACK (NULL), FALSE, },
  { "view-side-pane-tree", NULL, N_ ("_Tree"), "<control>E", N_ ("Toggles the visibility of the tree pane"), G_CALLBACK (NULL), FALSE, },
  { "view-statusbar", NULL, N_ ("St_atusbar"), NULL, N_ ("Change the visibility of this window's statusbar"), G_CALLBACK (NULL), FALSE, },
  { "view-menubar", NULL, N_ ("_Menubar"), "<control>M", N_ ("Change the visibility of this window's menubar"), G_CALLBACK (NULL), TRUE, },
};


static XfceGtkActionEntry thunar_window_action_entries[] =
{
    { THUNAR_WINDOW_ACTION_FILE_MENU,                      "<Actions>/ThunarWindow/file-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_File"),                  NULL, NULL, NULL,},
    { THUNAR_WINDOW_ACTION_NEW_TAB,                        "<Actions>/ThunarWindow/new-tab",                         "<Primary>t",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("New _Tab"),               N_ ("Open a new tab for the displayed location"),                                    "tab-new",                 G_CALLBACK (thunar_window_action_open_new_tab),       },
    { THUNAR_WINDOW_ACTION_NEW_WINDOW,                     "<Actions>/ThunarWindow/new-window",                      "<Primary>n",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("New _Window"),            N_ ("Open a new Thunar window for the displayed location"),                          "window-new",              G_CALLBACK (thunar_window_action_open_new_window),    },
    { THUNAR_WINDOW_ACTION_DETACH_TAB,                     "<Actions>/ThunarWindow/detach-tab",                      "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Detac_h Tab"),            N_ ("Open current folder in a new window"),                                          NULL,                      G_CALLBACK (thunar_window_action_detach_tab),         },
    { THUNAR_WINDOW_ACTION_CLOSE_TAB,                      "<Actions>/ThunarWindow/close-tab",                       "<Primary>w",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("C_lose Tab"),             N_ ("Close this folder"),                                                            "window-close",            G_CALLBACK (thunar_window_action_close_tab),          },
    { THUNAR_WINDOW_ACTION_CLOSE_WINDOW,                   "<Actions>/ThunarWindow/close-window",                    "<Primary>q",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Close Window"),          N_ ("Close this window"),                                                            "application-exit",        G_CALLBACK (thunar_window_action_close_window),       },
    { THUNAR_WINDOW_ACTION_CLOSE_ALL_WINDOWS,              "<Actions>/ThunarWindow/close-all-windows",               "<Primary><Shift>w",    XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Close _All Windows"),     N_ ("Close all Thunar windows"),                                                     NULL,                      G_CALLBACK (thunar_window_action_close_all_windows),  },

    { THUNAR_WINDOW_ACTION_EDIT_MENU,                      "<Actions>/ThunarWindow/edit-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Edit"),                  NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_PREFERENCES,                    "<Actions>/ThunarWindow/preferences",                     "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Pr_eferences..."),        N_ ("Edit Thunars Preferences"),                                                     "preferences-system",      G_CALLBACK (thunar_window_action_preferences),        },

    { THUNAR_WINDOW_ACTION_VIEW_MENU,                      "<Actions>/ThunarWindow/view-menu",                       "",                     XFCE_GTK_MENU_ITEM,       N_ ("_View"),                  NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_RELOAD,                         "<Actions>/ThunarWindow/reload",                          "<Primary>r",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Reload"),                N_ ("Reload the current folder"),                                                    "view-refresh-symbolic",   G_CALLBACK (thunar_window_action_reload),             },
    { THUNAR_WINDOW_ACTION_RELOAD_ALT,                     "<Actions>/ThunarWindow/reload-alt",                      "F5",                   XFCE_GTK_IMAGE_MENU_ITEM, NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_reload),             },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_MENU,    "<Actions>/ThunarWindow/view-location-selector-menu",     "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Location Selector"),     NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_PATHBAR, "<Actions>/ThunarWindow/view-location-selector-pathbar",  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Pathbar Style"),         N_ ("Modern approach with buttons that correspond to folders"),                      NULL,                      G_CALLBACK (thunar_window_action_pathbar_changed),    },
    { THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_TOOLBAR, "<Actions>/ThunarWindow/view-location-selector-toolbar",  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Toolbar Style"),         N_ ("Traditional approach with location bar and navigation buttons"),                NULL,                      G_CALLBACK (thunar_window_action_toolbar_changed),    },
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_MENU,            "<Actions>/ThunarWindow/view-side-pane-menu",             "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Side Pane"),             NULL,                                                                                NULL,                      NULL,                                                 },
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_SHORTCUTS,       "<Actions>/ThunarWindow/view-side-pane-shortcuts",        "<Primary>b",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Shortcuts"),             N_ ("Toggles the visibility of the shortcuts pane"),                                 NULL,                      G_CALLBACK (thunar_window_action_shortcuts_changed),  },
    { THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_TREE,            "<Actions>/ThunarWindow/view-side-pane-tree",             "<Primary>e",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Tree"),                  N_ ("Toggles the visibility of the tree pane"),                                      NULL,                      G_CALLBACK (thunar_window_action_tree_changed),       },
    { THUNAR_WINDOW_ACTION_TOGGLE_SIDE_PANE,               "<Actions>/ThunarWindow/toggle-side-pane",                "F9",                   XFCE_GTK_MENU_ITEM,       NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_toggle_sidepane),           },
    { THUNAR_WINDOW_ACTION_VIEW_STATUSBAR,                 "<Actions>/ThunarWindow/view-statusbar",                  "",                     XFCE_GTK_CHECK_MENU_ITEM, N_ ("St_atusbar"),             N_ ("Change the visibility of this window's statusbar"),                             NULL,                      G_CALLBACK (thunar_window_action_statusbar_changed),  },
    { THUNAR_WINDOW_ACTION_VIEW_MENUBAR,                   "<Actions>/ThunarWindow/view-menubar",                    "<Primary>m",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("_Menubar"),               N_ ("Change the visibility of this window's menubar"),                               NULL,                      G_CALLBACK (thunar_window_action_menubar_changed),    },
    { THUNAR_WINDOW_ACTION_SHOW_HIDDEN,                    "<Actions>/ThunarWindow/show-hidden",                     "<Primary>h",           XFCE_GTK_CHECK_MENU_ITEM, N_ ("Show _Hidden Files"),     N_ ("Toggles the display of hidden files in the current window"),                    NULL,                      G_CALLBACK (thunar_window_action_show_hidden),        },
    { THUNAR_WINDOW_ACTION_ZOOM_IN,                        "<Actions>/ThunarWindow/zoom-in",                         "<Primary>KP_Add",      XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Zoom I_n"),               N_ ("Show the contents in more detail"),                                             "zoom-in-symbolic",        G_CALLBACK (thunar_window_zoom_in),                   },
    { THUNAR_WINDOW_ACTION_ZOOM_OUT,                       "<Actions>/ThunarWindow/zoom-out",                        "<Primary>KP_Subtract", XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Zoom _Out"),              N_ ("Show the contents in less detail"),                                             "zoom-out-symbolic",       G_CALLBACK (thunar_window_zoom_out),                  },
    { THUNAR_WINDOW_ACTION_ZOOM_RESET,                     "<Actions>/ThunarWindow/zoom-reset",                      "<Primary>KP_0",        XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Normal Si_ze"),           N_ ("Show the contents at the normal size"),                                         "zoom-original-symbolic",  G_CALLBACK (thunar_window_zoom_reset),                },
    { THUNAR_WINDOW_ACTION_VIEW_AS_ICONS,                  "<Actions>/ThunarWindow/view-as-icons",                   "<Primary>1",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("View as _Icons"),         N_("Display folder content in an icon view"),                                        NULL,                      G_CALLBACK (thunar_window_action_icon_view),          },
    { THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST,          "<Actions>/ThunarWindow/view-as-detailed-list",           "<Primary>2",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("View as _Detailed List"), N_("Display folder content in a detailed list view"),                                NULL,                      G_CALLBACK (thunar_window_action_detailed_view),      },
    { THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST,           "<Actions>/ThunarWindow/view-as-compact-list",            "<Primary>3",           XFCE_GTK_RADIO_MENU_ITEM, N_ ("View as _Compact List"),  N_("Display folder content in a compact list view"),                                 NULL,                      G_CALLBACK (thunar_window_action_compact_view),       },

    { THUNAR_WINDOW_ACTION_GO_MENU,                        "<Actions>/ThunarWindow/go-menu",                         "",                     XFCE_GTK_MENU_ITEM,       N_ ("_Go"),                    NULL,                                                                                NULL,                      NULL                                                  },
    { THUNAR_WINDOW_ACTION_OPEN_FILE_SYSTEM,               "<Actions>/ThunarWindow/open-file-system",                "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("File System"),            N_ ("Browse the file system"),                                                       "drive-harddisk",          G_CALLBACK (thunar_window_action_open_file_system),   },
    { THUNAR_WINDOW_ACTION_OPEN_COMPUTER,                  "<Actions>/ThunarWindow/open-computer",                    "",                    XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Computer"),               N_ ("Go to the computer folder"),                                                    "computer",                G_CALLBACK (thunar_window_action_open_computer),      },
    { THUNAR_WINDOW_ACTION_OPEN_HOME,                      "<Actions>/ThunarWindow/open-home",                       "<Alt>Home",            XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Home"),                  N_ ("Go to the home folder"),                                                        "go-home-symbolic",        G_CALLBACK (thunar_window_action_open_home),          },
    { THUNAR_WINDOW_ACTION_OPEN_DESKTOP,                   "<Actions>/ThunarWindow/open-desktop",                    "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Desktop"),                N_ ("Go to the desktop folder"),                                                     "user-desktop",            G_CALLBACK (thunar_window_action_open_desktop),       },
    { THUNAR_WINDOW_ACTION_OPEN_COMPUTER,                  "<Actions>/ThunarWindow/open-computer",                   "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Computer"),               N_ ("Browse all local and remote disks and folders accessible from this computer"),  "computer",                G_CALLBACK (thunar_window_action_open_computer),      },
    { THUNAR_WINDOW_ACTION_OPEN_TRASH,                     "<Actions>/ThunarWindow/open-trash",                      "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("T_rash"),                 N_ ("Display the contents of the trash can"),                                        NULL,                      G_CALLBACK (thunar_window_action_open_trash),         },
    { THUNAR_WINDOW_ACTION_OPEN_PARENT,                    "<Actions>/ThunarWindow/open-parent",                     "<Alt>Up",              XFCE_GTK_IMAGE_MENU_ITEM, N_ ("Open _Parent"),           N_ ("Open the parent folder"),                                                       "go-up-symbolic",          G_CALLBACK (thunar_window_action_go_up),              },
    { THUNAR_WINDOW_ACTION_OPEN_LOCATION,                  "<Actions>/ThunarWindow/open-location",                   "<Primary>l",           XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Open Location..."),      N_ ("Specify a location to open"),                                                   NULL,                      G_CALLBACK (thunar_window_action_open_location),      },
    { THUNAR_WINDOW_ACTION_OPEN_LOCATION_ALT,              "<Actions>/ThunarWindow/open-location-alt",               "<Alt>d",               XFCE_GTK_MENU_ITEM,       "open-location-alt",           NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_open_location),      },
    { THUNAR_WINDOW_ACTION_OPEN_TEMPLATES,                 "<Actions>/ThunarWindow/open-templates",                  "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_("T_emplates"),              N_ ("Go to the templates folder"),                                                   "text-x-generic-template", G_CALLBACK (thunar_window_action_open_templates),     },
    { THUNAR_WINDOW_ACTION_OPEN_NETWORK,                   "<Actions>/ThunarWindow/open-network",                    "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_("B_rowse Network"),         N_ ("Browse local network connections"),                                             "network-workgroup",       G_CALLBACK (thunar_window_action_open_network),       },

    { THUNAR_WINDOW_ACTION_HELP_MENU,                      "<Actions>/ThunarWindow/contents/help-menu",              "",                     XFCE_GTK_MENU_ITEM      , N_ ("_Help"),                  NULL, NULL, NULL},
    { THUNAR_WINDOW_ACTION_CONTENTS,                       "<Actions>/ThunarWindow/contents",                        "F1",                   XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Contents"),              N_ ("Display Thunar user manual"),                                                   "help-browser",            G_CALLBACK (thunar_window_action_contents),            },
    { THUNAR_WINDOW_ACTION_ABOUT,                          "<Actions>/ThunarWindow/about",                           "",                     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_About"),                 N_ ("Display information about Thunar"),                                             "help-about",              G_CALLBACK (thunar_window_action_about),               },
    { THUNAR_WINDOW_ACTION_SWITCH_PREV_TAB,                "<Actions>/ThunarWindow/switch-previous-tab",             "<Primary>Page_Up",     XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Previous Tab"),          N_ ("Switch to Previous Tab"),                                                       "go-previous",             G_CALLBACK (thunar_window_action_switch_previous_tab), },
    { THUNAR_WINDOW_ACTION_SWITCH_NEXT_TAB,                "<Actions>/ThunarWindow/switch-next-tab",                 "<Primary>Page_Down",   XFCE_GTK_IMAGE_MENU_ITEM, N_ ("_Next Tab"),              N_ ("Switch to Next Tab"),                                                           "go-next",                 G_CALLBACK (thunar_window_action_switch_next_tab),     },
    { 0,                                                   "<Actions>/ThunarWindow/open-file-menu",                  "F10",                  0,                        NULL,                          NULL,                                                                                NULL,                      G_CALLBACK (thunar_window_action_open_file_menu),      },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_window_action_entries,G_N_ELEMENTS(thunar_window_action_entries),id)



static guint window_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;
  guint           i;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_window_dispose;
  gobject_class->finalize = thunar_window_finalize;
  gobject_class->get_property = thunar_window_get_property;
  gobject_class->set_property = thunar_window_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_window_realize;
  gtkwidget_class->unrealize = thunar_window_unrealize;
  gtkwidget_class->configure_event = thunar_window_configure_event;

  klass->back = thunar_window_back;
  klass->reload = thunar_window_reload;
  klass->zoom_in = thunar_window_zoom_in;
  klass->zoom_out = thunar_window_zoom_out;
  klass->zoom_reset = thunar_window_zoom_reset;
  klass->tab_change = thunar_window_tab_change;

  xfce_gtk_translate_action_entries (thunar_window_action_entries, G_N_ELEMENTS (thunar_window_action_entries));

  /**
   * ThunarWindow:current-directory:
   *
   * The directory currently displayed within this #ThunarWindow
   * or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_DIRECTORY,
                                   g_param_spec_object ("current-directory",
                                                        "current-directory",
                                                        "current-directory",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /**
   * ThunarWindow:ui-manager:
   *
   * The #GtkUIManager used for this #ThunarWindow. This property
   * can only be read and is garantied to always contain a valid
   * #GtkUIManager instance (thus it's never %NULL).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_UI_MANAGER,
                                   g_param_spec_object ("ui-manager",
                                                        "ui-manager",
                                                        "ui-manager",
                                                        GTK_TYPE_UI_MANAGER,
                                                        EXO_PARAM_READABLE));
G_GNUC_END_IGNORE_DEPRECATIONS

  /**
   * ThunarWindow:zoom-level:
   *
   * The #ThunarZoomLevel applied to the #ThunarView currently
   * shown within this window.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ZOOM_LEVEL,
                                   g_param_spec_enum ("zoom-level",
                                                      "zoom-level",
                                                      "zoom-level",
                                                      THUNAR_TYPE_ZOOM_LEVEL,
                                                      THUNAR_ZOOM_LEVEL_100_PERCENT,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarWindow::back:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to go to the
   * previous visited folder. This is an internal
   * signal used to bind the action to keys.
   **/
  window_signals[BACK] =
    g_signal_new (I_("back"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, back),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::reload:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to reload the contents
   * of the currently displayed folder.
   **/
  window_signals[RELOAD] =
    g_signal_new (I_("reload"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, reload),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);

  /**
   * ThunarWindow::zoom-in:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to zoom in. This
   * is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_IN] =
    g_signal_new (I_("zoom-in"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_in),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::zoom-out:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests to zoom out. This
   * is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_OUT] =
    g_signal_new (I_("zoom-out"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_out),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::zoom-reset:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user requests reset the zoom level.
   * This is an internal signal used to bind the action to keys.
   **/
  window_signals[ZOOM_RESET] =
    g_signal_new (I_("zoom-reset"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, zoom_reset),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::tab-change:
   * @window : a #ThunarWindow instance.
   * @idx    : tab index,
   *
   * Emitted whenever the user uses a Alt+N combination to
   * switch tabs.
   **/
  window_signals[TAB_CHANGE] =
    g_signal_new (I_("tab-change"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, tab_change),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);

  /* setup the key bindings for the windows */
  binding_set = gtk_binding_set_by_class (klass);

  /* setup the key bindings for Alt+N */
  for (i = 0; i < 10; i++)
    {
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_0 + i, GDK_MOD1_MASK,
                                    "tab-change", 1, G_TYPE_UINT, i - 1);
    }
}



static inline gint
view_type2index (GType type)
{
  /* this necessary for platforms where sizeof(GType) != sizeof(gint),
   * see https://bugzilla.xfce.org/show_bug.cgi?id=2726 for details.
   */
  if (sizeof (GType) == sizeof (gint))
    {
      /* no need to map anything */
      return (gint) type;
    }
  else
    {
      /* map from types to unique indices */
      if (G_LIKELY (type == THUNAR_TYPE_COMPACT_VIEW))
        return 0;
      else if (type == THUNAR_TYPE_DETAILS_VIEW)
        return 1;
      else
        return 2;
    }
}



static inline GType
view_index2type (gint idx)
{
  /* this necessary for platforms where sizeof(GType) != sizeof(gint),
   * see https://bugzilla.xfce.org/show_bug.cgi?id=2726 for details.
   */
  if (sizeof (GType) == sizeof (gint))
    {
      /* no need to map anything */
      return (GType) idx;
    }
  else
    {
      /* map from indices to unique types */
      switch (idx)
        {
        case 0:  return THUNAR_TYPE_COMPACT_VIEW;
        case 1:  return THUNAR_TYPE_DETAILS_VIEW;
        default: return THUNAR_TYPE_ICON_VIEW;
        }
    }
}



static void
thunar_window_init (ThunarWindow *window)
{
  GtkRadioAction  *radio_action;
  GtkAccelGroup   *accel_group;
  GtkWidget       *label;
  GtkWidget       *infobar;
  GtkWidget       *item;
  GtkAction       *action;
  gboolean         last_menubar_visible;
  GSList          *group;
  gchar           *last_location_bar;
  gchar           *last_side_pane;
  gchar           *last_view;
  GType            type;
  gint             last_separator_position;
  gint             last_window_width;
  gint             last_window_height;
  gboolean         last_window_maximized;
  gboolean         last_statusbar_visible;
  GtkToolItem     *tool_item;
  gboolean         small_icons;
  GtkStyleContext *context;

  /* unset the view type */
  window->view_type = G_TYPE_NONE;

  /* grab a reference on the provider factory */
  window->provider_factory = thunarx_provider_factory_get_default ();

  /* grab a reference on the preferences */
  window->preferences = thunar_preferences_get ();

  window->accel_group = gtk_accel_group_new ();
  xfce_gtk_accel_map_add_entries (thunar_window_action_entries, G_N_ELEMENTS (thunar_window_action_entries));
  xfce_gtk_accel_group_connect_action_entries (window->accel_group,
                                               thunar_window_action_entries,
                                               G_N_ELEMENTS (thunar_window_action_entries),
                                               window);

  gtk_window_add_accel_group (GTK_WINDOW (window), window->accel_group);

  /* get all properties for init */
  g_object_get (G_OBJECT (window->preferences),
                "last-show-hidden", &window->show_hidden,
                "last-window-width", &last_window_width,
                "last-window-height", &last_window_height,
                "last-window-maximized", &last_window_maximized,
                "last-menubar-visible", &last_menubar_visible,
                "last-separator-position", &last_separator_position,
                "last-location-bar", &last_location_bar,
                "last-side-pane", &last_side_pane,
                "last-statusbar-visible", &last_statusbar_visible,
                "misc-small-toolbar-icons", &small_icons,
                NULL);

  /* set up a handler to confirm exit when there are multiple tabs open  */
  g_signal_connect (window, "delete-event", G_CALLBACK (thunar_window_delete), NULL);

  /* connect to the volume monitor */
  window->device_monitor = thunar_device_monitor_get ();
  g_signal_connect (window->device_monitor, "device-pre-unmount", G_CALLBACK (thunar_window_device_pre_unmount), window);
  g_signal_connect (window->device_monitor, "device-removed", G_CALLBACK (thunar_window_device_changed), window);
  g_signal_connect (window->device_monitor, "device-changed", G_CALLBACK (thunar_window_device_changed), window);

  /* allocate a closure for the menu_item_selected() callback */
  window->menu_item_selected_closure = g_cclosure_new_object (G_CALLBACK (thunar_window_menu_item_selected), G_OBJECT (window));
  g_closure_ref (window->menu_item_selected_closure);
  g_closure_sink (window->menu_item_selected_closure);

  /* allocate a closure for the menu_item_deselected() callback */
  window->menu_item_deselected_closure = g_cclosure_new_object (G_CALLBACK (thunar_window_menu_item_deselected), G_OBJECT (window));
  g_closure_ref (window->menu_item_deselected_closure);
  g_closure_sink (window->menu_item_deselected_closure);
  window->icon_factory = thunar_icon_factory_get_default ();

  /* Catch key events before accelerators get processed */
  g_signal_connect (window, "key-press-event", G_CALLBACK (thunar_window_propagate_key_event), NULL);
  g_signal_connect (window, "key-release-event", G_CALLBACK (thunar_window_propagate_key_event), NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* setup the action group for this window */
  window->action_group = gtk_action_group_new ("ThunarWindow");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* initialize the "show-hidden" action using the last value from the preferences */
  //action = gtk_action_group_get_action (window->action_group, "show-hidden");
  //gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), last_show_hidden);

  /*
   * add view options
   */
  radio_action = gtk_radio_action_new ("view-as-icons", _("View as _Icons"), _("Display folder content in an icon view"),
                                       NULL, view_type2index (THUNAR_TYPE_ICON_VIEW));
  gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (radio_action), "<control>1");
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-as-detailed-list", _("View as _Detailed List"), _("Display folder content in a detailed list view"),
                                       NULL, view_type2index (THUNAR_TYPE_DETAILS_VIEW));
  gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (radio_action), "<control>2");
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-as-compact-list", _("View as _Compact List"), _("Display folder content in a compact list view"),
                                       NULL, view_type2index (THUNAR_TYPE_COMPACT_VIEW));
  gtk_action_group_add_action_with_accel (window->action_group, GTK_ACTION (radio_action), "<control>3");
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  window->ui_manager = gtk_ui_manager_new ();
  g_signal_connect (G_OBJECT (window->ui_manager), "connect-proxy", G_CALLBACK (thunar_window_connect_proxy), window);
  g_signal_connect (G_OBJECT (window->ui_manager), "disconnect-proxy", G_CALLBACK (thunar_window_disconnect_proxy), window);
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, thunar_window_ui, thunar_window_ui_length, NULL);

  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
G_GNUC_END_IGNORE_DEPRECATIONS

  window->select_files_closure = g_cclosure_new_swap (G_CALLBACK (thunar_window_select_files), window, NULL);
  g_closure_ref (window->select_files_closure);
  g_closure_sink (window->select_files_closure);
  window->launcher = g_object_new (THUNAR_TYPE_LAUNCHER, "widget", GTK_WIDGET (window),
                                  "select-files-closure",  window->select_files_closure, NULL);

  exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->launcher), "current-directory");
  g_signal_connect_swapped (G_OBJECT (window->launcher), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (window->launcher), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);
  thunar_launcher_append_accelerators (window->launcher, window->accel_group);

  /* determine the default window size from the preferences */
  gtk_window_set_default_size (GTK_WINDOW (window), last_window_width, last_window_height);

  /* restore the maxized state of the window */
  if (G_UNLIKELY (last_window_maximized))
    gtk_window_maximize (GTK_WINDOW (window));

  /* add thunar style class for easier theming */
  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_style_context_add_class (context, "thunar");

  window->grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), window->grid);
  gtk_widget_show (window->grid);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  window->menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
G_GNUC_END_IGNORE_DEPRECATIONS

  /* build the menubar */
  window->menubar = gtk_menu_bar_new ();
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_FILE_MENU), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));
  g_signal_connect_swapped (G_OBJECT (item), "button-press-event", G_CALLBACK (thunar_window_create_file_menu), window);
  g_signal_connect_swapped (G_OBJECT (item), "enter-notify-event", G_CALLBACK (thunar_window_menu_item_hovered), window);
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_EDIT_MENU), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));
  g_signal_connect_swapped (G_OBJECT (item), "button-press-event", G_CALLBACK (thunar_window_create_edit_menu), window);
  g_signal_connect_swapped (G_OBJECT (item), "enter-notify-event", G_CALLBACK (thunar_window_menu_item_hovered), window);
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_MENU), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));
  g_signal_connect_swapped (G_OBJECT (item), "button-press-event", G_CALLBACK (thunar_window_create_view_menu), window);
  g_signal_connect_swapped (G_OBJECT (item), "enter-notify-event", G_CALLBACK (thunar_window_menu_item_hovered), window);
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_GO_MENU), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));
  g_signal_connect_swapped (G_OBJECT (item), "button-press-event", G_CALLBACK (thunar_window_create_go_menu), window);
  g_signal_connect_swapped (G_OBJECT (item), "enter-notify-event", G_CALLBACK (thunar_window_menu_item_hovered), window);
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_HELP_MENU), G_OBJECT (window), GTK_MENU_SHELL (window->menubar));
  g_signal_connect_swapped (G_OBJECT (item), "button-press-event", G_CALLBACK (thunar_window_create_help_menu), window);
  g_signal_connect_swapped (G_OBJECT (item), "enter-notify-event", G_CALLBACK (thunar_window_menu_item_hovered), window);
  gtk_widget_show_all (window->menubar);

  if (last_menubar_visible == FALSE)
    gtk_widget_hide (window->menubar);
  gtk_widget_set_hexpand (window->menubar, TRUE);
  gtk_grid_attach (GTK_GRID (window->grid), window->menubar, 0, 0, 1, 1);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* update menubar visibiliy */
  action = gtk_action_group_get_action (window->action_group, "view-menubar");
  g_signal_connect (G_OBJECT (window->menubar), "deactivate", G_CALLBACK (NULL), window);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), last_menubar_visible);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* append the menu item for the spinner */
  item = gtk_menu_item_new ();
  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_menu_item_set_right_justified (GTK_MENU_ITEM (item), TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_menu_shell_append (GTK_MENU_SHELL (window->menubar), item);
  gtk_widget_show (item);

  /* place the spinner into the menu item */
  window->spinner = gtk_spinner_new ();
  gtk_container_add (GTK_CONTAINER (item), window->spinner);
  exo_binding_new (G_OBJECT (window->spinner), "active",
                   G_OBJECT (window->spinner), "visible");

  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      /* add the bar for the root warning */
      infobar = gtk_info_bar_new ();
      gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_WARNING);
      gtk_widget_set_hexpand (infobar, TRUE);
      gtk_grid_attach (GTK_GRID (window->grid), infobar, 0, 2, 1, 1);
      gtk_widget_show (infobar);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning: you are using the root account. You may harm your system."));
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), label);
      gtk_widget_show (label);
    }

  window->paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_set_border_width (GTK_CONTAINER (window->paned), 0);
  gtk_widget_set_hexpand (window->paned, TRUE);
  gtk_widget_set_vexpand (window->paned, TRUE);
  gtk_grid_attach (GTK_GRID (window->grid), window->paned, 0, 4, 1, 1);
  gtk_widget_show (window->paned);

  /* determine the last separator position and apply it to the paned view */
  gtk_paned_set_position (GTK_PANED (window->paned), last_separator_position);
  g_signal_connect_swapped (window->paned, "accept-position", G_CALLBACK (thunar_window_save_paned), window);
  g_signal_connect_swapped (window->paned, "button-release-event", G_CALLBACK (thunar_window_save_paned), window);

  window->view_box = gtk_grid_new ();
  gtk_paned_pack2 (GTK_PANED (window->paned), window->view_box, TRUE, FALSE);
  gtk_widget_show (window->view_box);

  /* tabs */
  window->notebook = gtk_notebook_new ();
  gtk_widget_set_hexpand (window->notebook, TRUE);
  gtk_widget_set_vexpand (window->notebook, TRUE);
  gtk_grid_attach (GTK_GRID (window->view_box), window->notebook, 0, 1, 1, 1);
  g_signal_connect (G_OBJECT (window->notebook), "switch-page", G_CALLBACK (thunar_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added", G_CALLBACK (thunar_window_notebook_page_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed", G_CALLBACK (thunar_window_notebook_page_removed), window);
  g_signal_connect_after (G_OBJECT (window->notebook), "button-press-event", G_CALLBACK (thunar_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "popup-menu", G_CALLBACK (thunar_window_notebook_popup_menu), window);
  g_signal_connect (G_OBJECT (window->notebook), "create-window", G_CALLBACK (thunar_window_notebook_create_window), window);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (window->notebook), FALSE);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (window->notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (window->notebook), 0);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), "thunar-tabs");
  gtk_widget_show (window->notebook);

  /* allocate the new location bar widget */
  window->location_bar = thunar_location_bar_new ();
  g_object_bind_property (G_OBJECT (window), "current-directory", G_OBJECT (window->location_bar), "current-directory", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "reload-requested", G_CALLBACK (thunar_window_handle_reload_request), window);
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "entry-done", G_CALLBACK (thunar_window_update_location_bar_visible), window);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* setup the toolbar for the location bar */
  window->location_toolbar = gtk_ui_manager_get_widget (window->ui_manager, "/location-toolbar");
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_toolbar_set_style (GTK_TOOLBAR (window->location_toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->location_toolbar),
                              small_icons ? GTK_ICON_SIZE_SMALL_TOOLBAR : GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_hexpand (window->location_toolbar, TRUE);
  gtk_grid_attach (GTK_GRID (window->grid), window->location_toolbar, 0, 1, 1, 1);

  /* add the location bar tool item */
  tool_item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (tool_item, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), tool_item, -1);
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (window->location_toolbar), FALSE);
  gtk_widget_show (GTK_WIDGET (tool_item));

  /* add the location bar itself */
  gtk_container_add (GTK_CONTAINER (tool_item), window->location_bar);

  /* display the new location bar widget */
  gtk_widget_show (window->location_bar);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* activate the selected location selector */
  action = gtk_action_group_get_action (window->action_group, "view-location-selector-pathbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !strcmp(last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)));
  action = gtk_action_group_get_action (window->action_group, "view-location-selector-toolbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !strcmp(last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)));
G_GNUC_END_IGNORE_DEPRECATIONS

  g_free (last_location_bar);

  /* setup setting the location bar visibility on-demand */
  g_signal_connect_object (G_OBJECT (window->preferences), "notify::last-location-bar", G_CALLBACK (thunar_window_update_location_bar_visible), window, G_CONNECT_SWAPPED);
  thunar_window_update_location_bar_visible (window);

  /* update window icon whenever preferences change */
  g_signal_connect_object (G_OBJECT (window->preferences), "notify::misc-change-window-icon", G_CALLBACK (thunar_window_update_window_icon), window, G_CONNECT_SWAPPED);

  /* determine the selected side pane */
  if (exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_SHORTCUTS_PANE)))
    type = THUNAR_TYPE_SHORTCUTS_PANE;
  else if (exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_TREE_PANE)))
    type = THUNAR_TYPE_TREE_PANE;
  else
    type = G_TYPE_NONE;
  thunar_window_install_sidepane (window, type);
  g_free (last_side_pane);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* activate the selected side pane */
  action = gtk_action_group_get_action (window->action_group, "view-side-pane-shortcuts");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (type == THUNAR_TYPE_SHORTCUTS_PANE));
  action = gtk_action_group_get_action (window->action_group, "view-side-pane-tree");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (type == THUNAR_TYPE_TREE_PANE));

  /* check if we should display the statusbar by default */
  action = gtk_action_group_get_action (window->action_group, "view-statusbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), last_statusbar_visible);

  /* connect signal */
  action = gtk_action_group_get_action (window->action_group, "view-as-icons");
G_GNUC_END_IGNORE_DEPRECATIONS
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_view_changed), window);

  /* ensure that all the view types are registered */
  g_type_ensure (THUNAR_TYPE_ICON_VIEW);
  g_type_ensure (THUNAR_TYPE_DETAILS_VIEW);
  g_type_ensure (THUNAR_TYPE_COMPACT_VIEW);

  /* schedule asynchronous menu action merging */
  window->merge_idle_id = g_idle_add_full (G_PRIORITY_LOW + 20, thunar_window_merge_idle, window, thunar_window_merge_idle_destroy);

  /* same is done for view in thunar_window_action_view_changed */
  thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (window->sidepane), window->show_hidden);
}



/**
 * thunar_window_select_files:
 * @window            : a #ThunarWindow instance.
 * @files_to_selected : a list of #GFile<!---->s
 *
 * Visually selects the files, given by the list
 **/
static void
thunar_window_select_files (ThunarWindow *window,
                            GList        *files_to_selected)
{
  GList *thunarFiles = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  for (GList *lp = files_to_selected; lp != NULL; lp = lp->next)
      thunarFiles = g_list_append (thunarFiles, thunar_file_get (G_FILE (files_to_selected->data), NULL));
  thunar_view_set_selected_files (THUNAR_VIEW (window->view), thunarFiles);
  g_list_free_full (thunarFiles, g_object_unref);
}



static gboolean
thunar_window_menu_is_open (ThunarWindow *window)
{
  GList     *lp;
  GtkWidget *submenu;

  g_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  for(lp = gtk_container_get_children (GTK_CONTAINER (window->menubar)); lp != NULL; lp = lp->next)
    {
      submenu = gtk_menu_item_get_submenu(lp->data);
      if (submenu != NULL && gtk_widget_get_visible (submenu))
        return TRUE;
    }
  return FALSE;
}



static gboolean
thunar_window_menu_item_hovered (ThunarWindow     *window,
                                 GdkEventCrossing *event,
                                 GtkWidget        *menu)
{
  gboolean ret;

  g_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (thunar_window_menu_is_open(window))
    g_signal_emit_by_name (menu, "button-press-event", NULL, &ret);
  return FALSE;
}



static gboolean
thunar_window_create_file_menu (ThunarWindow     *window,
                                GdkEventCrossing *event,
                                GtkWidget        *menu)
{
  ThunarMenu *submenu;
  GtkWidget  *item;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);

  submenu = g_object_new (THUNAR_TYPE_MENU, "menu-type", THUNAR_MENU_TYPE_WINDOW,
                                            "launcher", window->launcher, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_TAB), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_WINDOW), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  thunar_menu_add_sections (submenu, THUNAR_MENU_SECTION_OPEN
                                   | THUNAR_MENU_SECTION_SENDTO
                                   | THUNAR_MENU_SECTION_CREATE_NEW_FILES
                                   | THUNAR_MENU_SECTION_EMPTY_TRASH
                                   | THUNAR_MENU_SECTION_CUSTOM_ACTIONS
                                   | THUNAR_MENU_SECTION_PROPERTIES);
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_DETACH_TAB), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  gtk_widget_set_sensitive (item, gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) > 1);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_ALL_WINDOWS), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_TAB), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_WINDOW), G_OBJECT (window), GTK_MENU_SHELL (submenu));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), GTK_WIDGET (submenu));
  gtk_widget_show_all (GTK_WIDGET (submenu));

  return FALSE;
}



static gboolean
thunar_window_create_edit_menu (ThunarWindow     *window,
                                GdkEventCrossing *event,
                                GtkWidget        *menu)
{
  ThunarMenu      *submenu;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);

  submenu = g_object_new (THUNAR_TYPE_MENU, "launcher", window->launcher, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  thunar_menu_add_sections (submenu, THUNAR_MENU_SECTION_CUT
                                   | THUNAR_MENU_SECTION_COPY_PASTE
                                   | THUNAR_MENU_SECTION_TRASH_DELETE);
  thunar_menu_add_sections (submenu, THUNAR_MENU_SECTION_DUPLICATE
                                   | THUNAR_MENU_SECTION_MAKELINK
                                   | THUNAR_MENU_SECTION_RENAME
                                   | THUNAR_MENU_SECTION_RESTORE);

  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_PREFERENCES), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), GTK_WIDGET (submenu));
  gtk_widget_show_all (GTK_WIDGET (submenu));

  return FALSE;
}



static gboolean
thunar_window_create_view_menu (ThunarWindow     *window,
                                GdkEventCrossing *event,
                                GtkWidget        *menu)
{
  ThunarMenu *submenu;
  GtkWidget  *item;
  GtkWidget  *sub_items;
  gchar      *last_location_bar;
  gchar      *last_side_pane;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);

  submenu = g_object_new (THUNAR_TYPE_MENU, "launcher", window->launcher, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_RELOAD), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_MENU), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  sub_items =  gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (sub_items), window->accel_group);
  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_PATHBAR), G_OBJECT (window),
                                                   exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)), GTK_MENU_SHELL (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_LOCATION_SELECTOR_TOOLBAR), G_OBJECT (window),
                                                   exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)), GTK_MENU_SHELL (sub_items));
  g_free (last_location_bar);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (sub_items));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_MENU), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  sub_items =  gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (sub_items), window->accel_group);
  g_object_get (window->preferences, "last-side-pane", &last_side_pane, NULL);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_SHORTCUTS), G_OBJECT (window),
                                                   exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_SHORTCUTS_PANE)), GTK_MENU_SHELL (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_SIDE_PANE_TREE), G_OBJECT (window),
                                                   exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_TREE_PANE)), GTK_MENU_SHELL (sub_items));
  g_free (last_side_pane);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (sub_items));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_STATUSBAR), G_OBJECT (window),
                                                   gtk_widget_get_visible (window->statusbar), GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_MENUBAR), G_OBJECT (window),
                                                   gtk_widget_get_visible (window->menubar), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SHOW_HIDDEN), G_OBJECT (window),
                                                   window->show_hidden, GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (submenu), THUNAR_WINDOW_ACTION_ZOOM_IN);
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (submenu), THUNAR_WINDOW_ACTION_ZOOM_OUT);
  thunar_window_append_menu_item (window, GTK_MENU_SHELL (submenu), THUNAR_WINDOW_ACTION_ZOOM_RESET);
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));

  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_ICONS),
                                                 G_OBJECT (window), window->view_type == THUNAR_TYPE_ICON_VIEW, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_DETAILED_LIST),
                                                 G_OBJECT (window), window->view_type == THUNAR_TYPE_DETAILS_VIEW, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_VIEW_AS_COMPACT_LIST),
                                                 G_OBJECT (window), window->view_type == THUNAR_TYPE_COMPACT_VIEW, GTK_MENU_SHELL (submenu));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), GTK_WIDGET (submenu));
  gtk_widget_show_all (GTK_WIDGET (submenu));

  return FALSE;
}



static gboolean
thunar_window_create_go_menu (ThunarWindow     *window,
                              GdkEventCrossing *event,
                              GtkWidget        *menu)
{
  ThunarMenu               *submenu;
  gchar                    *icon_name;
  const XfceGtkActionEntry *action_entry;
  GtkWidget                *item;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);

  submenu = g_object_new (THUNAR_TYPE_MENU, "launcher",              window->launcher, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), GTK_WIDGET (submenu));
  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_PARENT), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  gtk_widget_set_sensitive (item, !thunar_g_file_is_root (thunar_file_get_file (window->current_directory)));

  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_COMPUTER), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_HOME), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_DESKTOP), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      GFile      *gfile;
      ThunarFile *trash_folder;

      /* try to connect to the trash bin */
      gfile = thunar_g_file_new_for_trash ();
      if (gfile != NULL)
        {
          trash_folder = thunar_file_get (gfile, NULL);
          if (trash_folder != NULL)
            {
              action_entry = get_action_entry (THUNAR_WINDOW_ACTION_OPEN_TRASH);
              if (action_entry != NULL)
                {
                  if (thunar_file_get_item_count (trash_folder) > 0)
                    icon_name = "user-trash-full";
                  else
                    icon_name = "user-trash";
                  xfce_gtk_image_menu_item_new_from_icon_name (action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                                               action_entry->accel_path, action_entry->callback, G_OBJECT (window), icon_name, GTK_MENU_SHELL (submenu));
                  g_object_unref (trash_folder);
                }
            }
          g_object_unref (gfile);
        }
    }
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_TEMPLATES), G_OBJECT (window), GTK_MENU_SHELL (submenu));

  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_FILE_SYSTEM), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_NETWORK), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_OPEN_LOCATION), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  gtk_widget_show_all (GTK_WIDGET (submenu));

  return FALSE;
}



static gboolean
thunar_window_create_help_menu (ThunarWindow     *window,
                                GdkEventCrossing *event,
                                GtkWidget        *menu)
{
  ThunarMenu *submenu;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);

  submenu = g_object_new (THUNAR_TYPE_MENU, "launcher", window->launcher, NULL);
  gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CONTENTS), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_ABOUT), G_OBJECT (window), GTK_MENU_SHELL (submenu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), GTK_WIDGET (submenu));
  gtk_widget_show_all (GTK_WIDGET (submenu));

  return FALSE;
}



static void
thunar_window_dispose (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  /* destroy the merge idle source */
  if (G_UNLIKELY (window->merge_idle_id != 0))
    g_source_remove (window->merge_idle_id);

  /* un-merge the custom preferences */
  if (G_LIKELY (window->custom_preferences_merge_id != 0))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_ui_manager_remove_ui (window->ui_manager, window->custom_preferences_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS
      window->custom_preferences_merge_id = 0;
    }

  /* un-merge the go menu actions */
  if (G_LIKELY (window->go_items_actions_merge_id != 0))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_ui_manager_remove_ui (window->ui_manager, window->go_items_actions_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS
      window->go_items_actions_merge_id = 0;
    }

  /* un-merge the bookmark actions */
  if (G_LIKELY (window->bookmark_items_actions_merge_id != 0))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_ui_manager_remove_ui (window->ui_manager, window->bookmark_items_actions_merge_id);
G_GNUC_END_IGNORE_DEPRECATIONS
      window->bookmark_items_actions_merge_id = 0;
    }

  if (window->bookmark_reload_idle_id != 0)
    {
      g_source_remove (window->bookmark_reload_idle_id);
      window->bookmark_reload_idle_id = 0;
    }

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id != 0))
    g_source_remove (window->save_geometry_timer_id);

  /* disconnect from the current-directory */
  thunar_window_set_current_directory (window, NULL);

  (*G_OBJECT_CLASS (thunar_window_parent_class)->dispose) (object);
}



static void
thunar_window_finalize (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  /* drop our references on the menu_item_selected()/menu_item_deselected() closures */
  g_closure_unref (window->menu_item_deselected_closure);
  g_closure_unref (window->menu_item_selected_closure);

  /* disconnect from the volume monitor */
  g_signal_handlers_disconnect_matched (window->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (window->device_monitor);

  /* disconnect from the ui manager */
  g_signal_handlers_disconnect_matched (window->ui_manager, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (window->ui_manager);

  /* release the custom actions */
  if (window->custom_actions != NULL)
    g_object_unref (window->custom_actions);

  g_object_unref (window->action_group);
  g_object_unref (window->icon_factory);
  g_object_unref (window->launcher);

  if (window->bookmark_action_group != NULL)
    g_object_unref (window->bookmark_action_group);

  if (window->bookmark_file != NULL)
    g_object_unref (window->bookmark_file);

  if (window->bookmark_monitor != NULL)
    {
      g_file_monitor_cancel (window->bookmark_monitor);
      g_object_unref (window->bookmark_monitor);
    }

  /* release our reference on the provider factory */
  g_object_unref (window->provider_factory);

  /* release the preferences reference */
  g_object_unref (window->preferences);

  g_closure_invalidate (window->select_files_closure);
  g_closure_unref (window->select_files_closure);

  (*G_OBJECT_CLASS (thunar_window_parent_class)->finalize) (object);
}



static gboolean thunar_window_delete (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   data )
{
  GtkNotebook *notebook;
  gboolean confirm_close_multiple_tabs, do_not_ask_again;
  gint response, n_tabs;

   _thunar_return_val_if_fail (THUNAR_IS_WINDOW (widget),FALSE);

  /* if we don't have muliple tabs then just exit */
  notebook  = GTK_NOTEBOOK (THUNAR_WINDOW (widget)->notebook);
  n_tabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (THUNAR_WINDOW (widget)->notebook));
  if (n_tabs < 2)
    return FALSE;

  /* check if the user has disabled confirmation of closing multiple tabs, and just exit if so */
  g_object_get (G_OBJECT (THUNAR_WINDOW (widget)->preferences),
                "misc-confirm-close-multiple-tabs", &confirm_close_multiple_tabs,
                NULL);
  if(!confirm_close_multiple_tabs)
    return FALSE;

  /* ask the user for confirmation */
  do_not_ask_again = FALSE;
  response = xfce_dialog_confirm_close_tabs (GTK_WINDOW (widget), n_tabs, TRUE, &do_not_ask_again);

  /* if the user requested not to be asked again, store this preference */
  if (response != GTK_RESPONSE_CANCEL && do_not_ask_again)
    g_object_set (G_OBJECT (THUNAR_WINDOW (widget)->preferences),
                  "misc-confirm-close-multiple-tabs", FALSE, NULL);

  if(response == GTK_RESPONSE_YES)
    return FALSE;
  if(response == GTK_RESPONSE_CLOSE)
    gtk_notebook_remove_page (notebook,  gtk_notebook_get_current_page(notebook));
  return TRUE;
}



static void
thunar_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_window_get_current_directory (window));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, window->ui_manager);
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_enum (value, window->zoom_level);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_window_set_property (GObject            *object,
                            guint               prop_id,
                            const GValue       *value,
                            GParamSpec         *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_window_set_current_directory (window, g_value_get_object (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_window_set_zoom_level (window, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_window_back (ThunarWindow *window)
{
  GtkAction   *action;
  GdkEvent    *event;
  const gchar *accel_path;
  GtkAccelKey  key;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check source event */
  event = gtk_get_current_event ();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (event != NULL
      && event->type == GDK_KEY_PRESS)
    {
      action = thunar_gtk_ui_manager_get_action_by_name (window->ui_manager, "open-parent");
      if (G_LIKELY (action != NULL))
        {
          /* check if the current event (back) is different then the open-parent
           * accelerator. this way a user can override the default backspace action
           * of back in open-parent, without backspace resulting in a back action
           * if open-parent is insensitive in the menu */
          accel_path = gtk_action_get_accel_path (action);
          if (accel_path != NULL
              && gtk_accel_map_lookup_entry (accel_path, &key)
              && key.accel_key == ((GdkEventKey *) event)->keyval
              && key.accel_mods == 0)
            return FALSE;
        }
    }

  /* activate the "back" action */
  action = thunar_gtk_ui_manager_get_action_by_name (window->ui_manager, "back");
  if (G_LIKELY (action != NULL))
    {
      gtk_action_activate (action);
      return TRUE;
    }
G_GNUC_END_IGNORE_DEPRECATIONS

  return FALSE;
}



static gboolean
thunar_window_reload (ThunarWindow *window,
                      gboolean      reload_info)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* force the view to reload */
  if (G_LIKELY (window->view != NULL))
    {
      thunar_view_reload (THUNAR_VIEW (window->view), reload_info);
      return TRUE;
    }

  return FALSE;
}



/**
 * thunar_window_has_shortcut_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: True, if this window is running a shortcut sidepane
 **/
gboolean
thunar_window_has_shortcut_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    {
      return G_OBJECT_TYPE (window->sidepane) == THUNAR_TYPE_SHORTCUTS_PANE;
    }
  return FALSE;
}



/**
 * thunar_window_get_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: (transfer none): The #ThunarSidePane of this window, or NULL if not available
 **/
GtkWidget* thunar_window_get_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  return GTK_WIDGET (window->sidepane);
}



static gboolean
thunar_window_toggle_sidepane (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    {
      /* determine the currently active side pane type */
      window->toggle_sidepane_type = G_OBJECT_TYPE (window->sidepane);
      thunar_window_install_sidepane (window, G_TYPE_NONE);
    }
  else
    {
      /* check if we have a previously remembered toggle type */
      if (window->toggle_sidepane_type == THUNAR_TYPE_TREE_PANE || window->toggle_sidepane_type == THUNAR_TYPE_SHORTCUTS_PANE)
          thunar_window_install_sidepane (window, window->toggle_sidepane_type);
    }

  return TRUE;
}



static gboolean
thunar_window_zoom_in (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if we can still zoom in */
  if (G_LIKELY (window->zoom_level < THUNAR_ZOOM_N_LEVELS - 1))
    {
      thunar_window_set_zoom_level (window, window->zoom_level + 1);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_zoom_out (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if we can still zoom out */
  if (G_LIKELY (window->zoom_level > 0))
    {
      thunar_window_set_zoom_level (window, window->zoom_level - 1);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_zoom_reset (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* tell the view to reset it's zoom level */
  if (G_LIKELY (window->view != NULL))
    {
      thunar_view_reset_zoom_level (THUNAR_VIEW (window->view));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_tab_change (ThunarWindow *window,
                          gint          nth)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* Alt+0 is 10th tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook),
                                 nth == -1 ? 9 : nth);

  return TRUE;
}



static void
thunar_window_action_switch_next_tab (ThunarWindow *window)
{
  gint current_page;
  gint new_page;
  gint pages;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  new_page = (current_page + 1) % pages;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), new_page);
}



static void
thunar_window_action_switch_previous_tab (ThunarWindow *window)
{
  gint current_page;
  gint new_page;
  gint pages;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  new_page = (current_page - 1) % pages;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), new_page);
}



static void
thunar_window_realize (GtkWidget *widget)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* let the GtkWidget class perform the realize operation */
  (*GTK_WIDGET_CLASS (thunar_window_parent_class)->realize) (widget);

  /* connect to the clipboard manager of the new display and be sure to redraw the window
   * whenever the clipboard contents change to make sure we always display up2date state.
   */
  window->clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (widget));
  g_signal_connect_swapped (G_OBJECT (window->clipboard), "changed",
                            G_CALLBACK (gtk_widget_queue_draw), widget);
}



static void
thunar_window_unrealize (GtkWidget *widget)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* disconnect from the clipboard manager */
  g_signal_handlers_disconnect_by_func (G_OBJECT (window->clipboard), gtk_widget_queue_draw, widget);

  /* let the GtkWidget class unrealize the window */
  (*GTK_WIDGET_CLASS (thunar_window_parent_class)->unrealize) (widget);

  /* drop the reference on the clipboard manager, we do this after letting the GtkWidget class
   * unrealise the window to prevent the clipboard being disposed during the unrealize  */
  g_object_unref (G_OBJECT (window->clipboard));
}



static gboolean
thunar_window_configure_event (GtkWidget         *widget,
                               GdkEventConfigure *event)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  /* check if we have a new dimension here */
  if (widget_allocation.width != event->width || widget_allocation.height != event->height)
    {
      /* drop any previous timer source */
      if (window->save_geometry_timer_id != 0)
        g_source_remove (window->save_geometry_timer_id);

      /* check if we should schedule another save timer */
      if (gtk_widget_get_visible (widget))
        {
          /* save the geometry one second after the last configure event */
          window->save_geometry_timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 1, thunar_window_save_geometry_timer,
                                                                       window, thunar_window_save_geometry_timer_destroy);
        }
    }

  /* let Gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (thunar_window_parent_class)->configure_event) (widget, event);
}



static void
thunar_window_binding_destroyed (gpointer data,
                                 GObject  *binding)
{
  ThunarWindow *window = THUNAR_WINDOW (data);

  if (window->view_bindings != NULL)
    window->view_bindings = g_slist_remove (window->view_bindings, binding);
}



static void
thunar_window_binding_create (ThunarWindow *window,
                              gpointer src_object,
                              const gchar *src_prop,
                              gpointer dst_object,
                              const gchar *dst_prop,
                              GBindingFlags flags)
{
  GBinding *binding;

  _thunar_return_if_fail (G_IS_OBJECT (src_object));
  _thunar_return_if_fail (G_IS_OBJECT (dst_object));

  binding = g_object_bind_property (G_OBJECT (src_object), src_prop,
                                    G_OBJECT (dst_object), dst_prop,
                                    flags);

  g_object_weak_ref (G_OBJECT (binding), thunar_window_binding_destroyed, window);
  window->view_bindings = g_slist_prepend (window->view_bindings, binding);
}



static void
thunar_window_notebook_switch_page (GtkWidget    *notebook,
                                    GtkWidget    *page,
                                    guint         page_num,
                                    ThunarWindow *window)
{
  GSList        *view_bindings;
  ThunarFile    *current_directory;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));
  _thunar_return_if_fail (window->notebook == notebook);

  /* leave if nothing changed */
  if (window->view == page)
    return;

  if (G_LIKELY (window->view != NULL))
    {
      /* unregisters the actions from the ui */
      thunar_component_set_ui_manager (THUNAR_COMPONENT (window->view), NULL);

      /* unset view during switch */
      window->view = NULL;
    }

  /* disconnect existing bindings */
  view_bindings = window->view_bindings;
  window->view_bindings = NULL;
  g_slist_free_full (view_bindings, g_object_unref);

  /* update the directory of the current window */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (page));
  thunar_window_set_current_directory (window, current_directory);

  /* add stock bindings */
  thunar_window_binding_create (window, window, "current-directory", page, "current-directory", G_BINDING_DEFAULT);
  thunar_window_binding_create (window, page, "loading", window->spinner, "active", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "selected-files", window->launcher, "selected-files", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "zoom-level", window, "zoom-level", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  /* connect to the sidepane (if any) */
  if (G_LIKELY (window->sidepane != NULL))
    {
      thunar_window_binding_create (window, page, "selected-files",
                                    window->sidepane, "selected-files",
                                    G_BINDING_SYNC_CREATE);
    }

  /* connect to the statusbar (if any) */
  if (G_LIKELY (window->statusbar != NULL))
    {
      thunar_window_binding_create (window, page, "statusbar-text",
                                    window->statusbar, "text",
                                    G_BINDING_SYNC_CREATE);
    }

  /* activate new view */
  window->view = page;
  window->view_type = G_TYPE_FROM_INSTANCE (page);

  if (window->view_type != G_TYPE_NONE)
    g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (window->view_type), NULL);

  /* integrate the standard view action in the ui */
  thunar_component_set_ui_manager (THUNAR_COMPONENT (page), window->ui_manager);

  /* update the actions */
  thunar_standard_view_selection_changed (THUNAR_STANDARD_VIEW (page));

  gtk_widget_grab_focus (page);
}



static void
thunar_window_notebook_show_tabs (ThunarWindow *window)
{
  gint       n_pages;
  gboolean   show_tabs = TRUE;

  /* check if tabs should be visible */
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (n_pages < 2)
    {
      g_object_get (G_OBJECT (window->preferences),
                    "misc-always-show-tabs", &show_tabs, NULL);
    }

  /* update visibility */
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), show_tabs);
}



static void
thunar_window_notebook_page_added (GtkWidget    *notebook,
                                   GtkWidget    *page,
                                   guint         page_num,
                                   ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));
  _thunar_return_if_fail (window->notebook == notebook);

  /* connect signals */
  g_signal_connect (G_OBJECT (page), "notify::loading", G_CALLBACK (thunar_window_notify_loading), window);
  g_signal_connect_swapped (G_OBJECT (page), "start-open-location", G_CALLBACK (thunar_window_start_open_location), window);
  g_signal_connect_swapped (G_OBJECT (page), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (page), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);

  /* update tab visibility */
  thunar_window_notebook_show_tabs (window);

  /* set default type if not set yet */
  if (window->view_type == G_TYPE_NONE)
    window->view_type = G_OBJECT_TYPE (page);
}



static void
thunar_window_notebook_page_removed (GtkWidget    *notebook,
                                     GtkWidget    *page,
                                     guint         page_num,
                                     ThunarWindow *window)
{
  gint n_pages;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  _thunar_return_if_fail (THUNAR_IS_VIEW (page));
  _thunar_return_if_fail (window->notebook == notebook);

  /* drop connected signals */
  g_signal_handlers_disconnect_matched (page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);

  /* remove from the ui */
  thunar_component_set_ui_manager (THUNAR_COMPONENT (page), NULL);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
  if (n_pages == 0)
    {
      /* destroy the window */
      gtk_widget_destroy (GTK_WIDGET (window));
    }
  else
    {
      /* update tab visibility */
      thunar_window_notebook_show_tabs (window);
    }
}



static gboolean
thunar_window_notebook_button_press_event (GtkWidget      *notebook,
                                           GdkEventButton *event,
                                           ThunarWindow   *window)
{
  gint           page_num = 0;
  GtkWidget     *page;
  GtkWidget     *label_box;
  GtkAllocation  alloc;
  gint           x, y;
  gboolean       close_tab;

  if ((event->button == 2 || event->button == 3)
      && event->type == GDK_BUTTON_PRESS)
    {
      /* get real window coordinates */
      gdk_window_get_position (event->window, &x, &y);
      x += event->x;
      y += event->y;

      /* lookup the clicked tab */
      while ((page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page_num)) != NULL)
        {
          label_box = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), page);
          gtk_widget_get_allocation (label_box, &alloc);

          if (x >= alloc.x && x < alloc.x + alloc.width
              && y >= alloc.y && y < alloc.y + alloc.height)
            break;

          page_num++;
        }

      /* leave if no tab could be found */
      if (page == NULL)
        return FALSE;

      if (event->button == 2)
        {
          /* check if we should close the tab */
          g_object_get (window->preferences, "misc-tab-close-middle-click", &close_tab, NULL);
          if (close_tab)
            gtk_widget_destroy (page);
        }
      else if (event->button == 3)
        {
          /* update the current tab before we show the menu */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);

          /* show the tab menu */
          thunar_window_notebook_popup_menu (notebook, window);
        }

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_notebook_popup_menu (GtkWidget    *notebook,
                                   ThunarWindow *window)
{
  GtkWidget *menu;

  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu), window->accel_group);
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_NEW_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_DETACH_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SWITCH_PREV_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_SWITCH_NEXT_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_WINDOW_ACTION_CLOSE_TAB), G_OBJECT (window), GTK_MENU_SHELL (menu));
  gtk_widget_show_all (menu);
  thunar_gtk_menu_run (GTK_MENU (menu));
  return TRUE;
}



static gpointer
thunar_window_notebook_create_window (GtkWidget    *notebook,
                                      GtkWidget    *page,
                                      gint          x,
                                      gint          y,
                                      ThunarWindow *window)
{
  GtkWidget         *new_window;
  ThunarApplication *application;
  gint               width, height;
  GdkMonitor        *monitor;
  GdkScreen         *screen;
  GdkRectangle       geo;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  _thunar_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  _thunar_return_val_if_fail (window->notebook == notebook, NULL);
  _thunar_return_val_if_fail (THUNAR_IS_VIEW (page), NULL);

  /* do nothing if this window has only 1 tab */
  if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) < 2)
    return NULL;

  /* create new window */
  application = thunar_application_get ();
  screen = gtk_window_get_screen (GTK_WINDOW (window));
  new_window = thunar_application_open_window (application, NULL, screen, NULL, TRUE);
  g_object_unref (application);

  /* make sure the new window has the same size */
  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  gtk_window_resize (GTK_WINDOW (new_window), width, height);

  /* move the window to the drop position */
  if (x >= 0 && y >= 0)
    {
      /* get the monitor geometry */
      monitor = gdk_display_get_monitor_at_point (gdk_display_get_default (), x, y);
      gdk_monitor_get_geometry (monitor, &geo);

      /* calculate window position, but keep it on the current monitor */
      x = CLAMP (x - width / 2, geo.x, geo.x + geo.width - width);
      y = CLAMP (y - height / 2, geo.y, geo.y + geo.height - height);

      /* move the window */
      gtk_window_move (GTK_WINDOW (new_window), MAX (0, x), MAX (0, y));
    }

  /* insert page in new notebook */
  return THUNAR_WINDOW (new_window)->notebook;
}



void
thunar_window_notebook_insert (ThunarWindow *window,
                               ThunarFile   *directory)
{
  ThunarHistory  *history = NULL;
  GtkWidget      *view;
  gint            page_num;
  GtkWidget      *label;
  GtkWidget      *label_box;
  GtkWidget      *button;
  GtkWidget      *icon;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (directory));
  _thunar_return_if_fail (window->view_type != G_TYPE_NONE);

  /* leave if no directory is set */
  if (directory == NULL)
    return;

  /* save the history of the origin view */
  if (THUNAR_IS_STANDARD_VIEW (window->view))
    history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (window->view));

  /* allocate and setup a new view */
  view = g_object_new (window->view_type, "current-directory", directory, "accel-group", window->accel_group, NULL);
  thunar_view_set_show_hidden (THUNAR_VIEW (view), window->show_hidden);
  gtk_widget_show (view);

  /* use the history of the origin view if available */
  if (history != NULL)
    thunar_standard_view_set_history (THUNAR_STANDARD_VIEW (view), history);

  label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  label = gtk_label_new (NULL);
  exo_binding_new (G_OBJECT (view), "display-name", G_OBJECT (label), "label");
  exo_binding_new (G_OBJECT (view), "tooltip-text", G_OBJECT (label), "tooltip-text");
  gtk_widget_set_has_tooltip (label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_margin_start (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_end (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_top (GTK_WIDGET(label), 3);
  gtk_widget_set_margin_bottom (GTK_WIDGET(label), 3);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (label_box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (label_box), button, FALSE, FALSE, 0);
  gtk_widget_set_can_default (button, FALSE);
  gtk_widget_set_focus_on_click (button, FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, _("Close tab"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (gtk_widget_destroy), view);
  gtk_widget_show (button);

  icon = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), icon);
  gtk_widget_show (icon);

  /* insert the new page */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  page_num = gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook), view, label_box, page_num + 1);

  /* switch to the new tab*/
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (window->notebook), view, "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook), view, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->notebook), view, TRUE);

  /* take focus on the view */
  gtk_widget_grab_focus (view);
}



void
thunar_window_update_directories (ThunarWindow *window,
                                  ThunarFile   *old_directory,
                                  ThunarFile   *new_directory)
{
  GtkWidget  *view;
  ThunarFile *directory;
  gint        n;
  gint        n_pages;
  gint        active_page;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (old_directory));
  _thunar_return_if_fail (THUNAR_IS_FILE (new_directory));

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (G_UNLIKELY (n_pages == 0))
    return;

  active_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  for (n = 0; n < n_pages; n++)
    {
      /* get the view */
      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);
      if (! THUNAR_IS_NAVIGATOR (view))
        continue;

      /* get the directory of the view */
      directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (view));
      if (! THUNAR_IS_FILE (directory))
        continue;

      /* if it matches the old directory, change to the new one */
      if (directory == old_directory)
        {
          if (n == active_page)
            thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), new_directory);
          else
            thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view), new_directory);
        }
    }
}



static void
thunar_window_update_location_bar_visible (ThunarWindow *window)
{
  gchar *last_location_bar = NULL;

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);

  if (exo_str_is_equal (last_location_bar, g_type_name (G_TYPE_NONE)))
    gtk_widget_hide (window->location_toolbar);
  else
    gtk_widget_show (window->location_toolbar);

  g_free (last_location_bar);
}



static void
thunar_window_update_window_icon (ThunarWindow *window)
{
  gboolean      change_window_icon;
  GtkIconTheme *icon_theme;
  const gchar  *icon_name = "folder";

  g_object_get (window->preferences, "misc-change-window-icon", &change_window_icon, NULL);

  if (change_window_icon)
    {
      icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
      icon_name = thunar_file_get_icon_name (window->current_directory,
                                             THUNAR_FILE_ICON_STATE_DEFAULT,
                                             icon_theme);
    }

  gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
}



static void
thunar_window_handle_reload_request (ThunarWindow *window)
{
  gboolean result;

  /* force the view to reload */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, TRUE, &result);
}



static void
thunar_window_install_sidepane (ThunarWindow *window,
                                GType         type)
{
  GtkStyleContext *context;

  _thunar_return_if_fail (type == G_TYPE_NONE || g_type_is_a (type, THUNAR_TYPE_SIDE_PANE));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* drop the previous side pane (if any) */
  if (G_UNLIKELY (window->sidepane != NULL))
    {
      gtk_widget_destroy (window->sidepane);
      window->sidepane = NULL;
    }

  /* check if we have a new sidepane widget */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* allocate the new side pane widget */
      window->sidepane = g_object_new (type, NULL);
      gtk_widget_set_size_request (window->sidepane, 0, -1);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->sidepane), "current-directory");
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);
      context = gtk_widget_get_style_context (window->sidepane);
      gtk_style_context_add_class (context, "sidebar");
      gtk_paned_pack1 (GTK_PANED (window->paned), window->sidepane, FALSE, FALSE);
      gtk_widget_show (window->sidepane);

      /* connect the side pane widget to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        thunar_window_binding_create (window, window->view, "selected-files", window->sidepane, "selected-files", G_BINDING_SYNC_CREATE);
    }

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-side-pane", g_type_name (type), NULL);
}



static void
thunar_window_merge_custom_preferences (ThunarWindow *window)
{
  GList           *providers;
  GList           *items;
  GList           *pp;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (window->custom_preferences_merge_id == 0);

  /* determine the available preferences providers */
  providers = thunarx_provider_factory_list_providers (window->provider_factory, THUNARX_TYPE_PREFERENCES_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* allocate a new merge id from the UI manager */
      window->custom_preferences_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);
G_GNUC_END_IGNORE_DEPRECATIONS

      /* add menu items from all providers */
      for (pp = providers; pp != NULL; pp = pp->next)
        {
          /* determine the available menu items for the provider */
          items = thunarx_preferences_provider_get_menu_items (THUNARX_PREFERENCES_PROVIDER (pp->data), GTK_WIDGET (window));

          thunar_menu_util_add_items_to_ui_manager (window->ui_manager,
                                                    window->action_group,
                                                    window->custom_preferences_merge_id,
                                                    "/main-menu/edit-menu/placeholder-custom-preferences",
                                                    items);

          /* release the reference on the provider */
          g_object_unref (G_OBJECT (pp->data));

          /* release the action list */
          g_list_free (items);
        }

      /* release the provider list */
      g_list_free (providers);
    }
}



static void
thunar_window_bookmark_changed (ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (window->bookmark_reload_idle_id == 0)
    window->bookmark_reload_idle_id = g_idle_add (thunar_window_bookmark_merge, window);
}



static void
thunar_window_bookmark_release_file (gpointer data)
{
  ThunarFile *file = THUNAR_FILE (data);

  /* stop watching */
  thunar_file_unwatch (file);

  /* disconnect changed and destroy signals */
  g_signal_handlers_disconnect_matched (file,
                                        G_SIGNAL_MATCH_FUNC, 0,
                                        0, NULL,
                                        G_CALLBACK (thunar_window_bookmark_changed),
                                        NULL);

  g_object_unref (file);
}



static void
thunar_window_bookmark_merge_line (GFile       *file_path,
                                   const gchar *name,
                                   gint         line_num,
                                   gpointer     user_data)
{
  ThunarWindow *window = THUNAR_WINDOW (user_data);
  GtkAction    *action = NULL;
  GChecksum    *checksum;
  gchar        *uri;
  ThunarFile   *file;
  gchar        *parse_name;
  gchar        *tooltip;
  gchar        *remote_name = NULL;
  const gchar  *unique_name;
  const gchar  *path;
  GtkIconTheme *icon_theme;
  const gchar  *icon_name;

  _thunar_return_if_fail (G_IS_FILE (file_path));
  _thunar_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* create unique id based on the uri */
  uri = g_file_get_uri (file_path);
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));
  unique_name = g_checksum_get_string (checksum);
  g_free (uri);

  parse_name = g_file_get_parse_name (file_path);
  tooltip = g_strdup_printf (_("Open the location \"%s\""), parse_name);
  g_free (parse_name);

  icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));

  if (g_file_has_uri_scheme (file_path, "file"))
    {
      /* try to open the file corresponding to the uri */
      file = thunar_file_get (file_path, NULL);
      if (G_UNLIKELY (file == NULL))
        return;

      /* make sure the file refers to a directory */
      if (G_UNLIKELY (thunar_file_is_directory (file)))
        {
          if (name == NULL)
            name = thunar_file_get_display_name (file);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          action = gtk_action_new (unique_name, name, tooltip, NULL);
          icon_name = thunar_file_get_icon_name (file, THUNAR_FILE_ICON_STATE_DEFAULT, icon_theme);
          gtk_action_set_icon_name (action, icon_name);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), file,
                                  thunar_window_bookmark_release_file);

          /* watch the file */
          thunar_file_watch (file);

          g_signal_connect_swapped (G_OBJECT (file), "destroy",
                                    G_CALLBACK (thunar_window_bookmark_changed), window);
          g_signal_connect_swapped (G_OBJECT (file), "changed",
                                    G_CALLBACK (thunar_window_bookmark_changed), window);
        }
      else
        {
          g_object_unref (file);
        }

      /* add to the local bookmarks */
      path = "/main-menu/go-menu/placeholder-go-local-actions";
    }
  else
    {
      if (name == NULL)
        {
          remote_name = thunar_g_file_get_display_name_remote (file_path);
          name = remote_name;
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      action = gtk_action_new (unique_name, name, tooltip, NULL);
      gtk_action_set_icon_name (action, "folder-remote");
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_set_data_full (G_OBJECT (action), I_("location-file"),
                              g_object_ref (file_path), g_object_unref);

      g_free (remote_name);

      /* add to the remote bookmarks */
      path = "/main-menu/go-menu/placeholder-go-remote-actions";
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (G_LIKELY (action != NULL))
    {
      if (gtk_action_group_get_action (window->bookmark_action_group, unique_name) == NULL)
        {
          /* connect action */
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_window_action_open_bookmark), window);

          /* insert the bookmark in the group */
          gtk_action_group_add_action_with_accel (window->bookmark_action_group, action, NULL);

          /* add the action to the UI manager */
          gtk_ui_manager_add_ui (window->ui_manager,
                                 window->bookmark_items_actions_merge_id,
                                 path,
                                 unique_name, unique_name,
                                 GTK_UI_MANAGER_MENUITEM, FALSE);
        }

      g_object_unref (action);
    }
G_GNUC_END_IGNORE_DEPRECATIONS

  g_checksum_free (checksum);
  g_free (tooltip);
}



static gboolean
thunar_window_bookmark_merge (gpointer user_data)
{
  ThunarWindow *window = THUNAR_WINDOW (user_data);

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

THUNAR_THREADS_ENTER

  /* remove old actions */
  if (window->bookmark_items_actions_merge_id != 0)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_ui_manager_remove_ui (window->ui_manager, window->bookmark_items_actions_merge_id);
      gtk_ui_manager_ensure_update (window->ui_manager);
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  /* drop old bookmarks action group */
  if (window->bookmark_action_group != NULL)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_ui_manager_remove_action_group (window->ui_manager, window->bookmark_action_group);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (window->bookmark_action_group);
    }

  /* lazy initialize the bookmarks */
  if (window->bookmark_file == NULL)
    {
      window->bookmark_file = thunar_g_file_new_for_bookmarks ();
      window->bookmark_monitor = g_file_monitor_file (window->bookmark_file, G_FILE_MONITOR_NONE, NULL, NULL);
      if (G_LIKELY (window->bookmark_monitor != NULL))
        {
          g_signal_connect_swapped (window->bookmark_monitor, "changed",
                                    G_CALLBACK (thunar_window_bookmark_changed), window);
        }
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* generate a new merge id */
  window->bookmark_items_actions_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* create a new action group */
  window->bookmark_action_group = gtk_action_group_new ("ThunarBookmarks");
  gtk_ui_manager_insert_action_group (window->ui_manager, window->bookmark_action_group, -1);
G_GNUC_END_IGNORE_DEPRECATIONS

  /* collect bookmarks */
  thunar_util_load_bookmarks (window->bookmark_file,
                              thunar_window_bookmark_merge_line,
                              window);

  window->bookmark_reload_idle_id = 0;

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_window_open_or_launch (ThunarWindow *window,
                              ThunarFile   *file)
{
  GError *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (thunar_file_is_directory (file))
    {
      /* open the new directory */
      thunar_window_set_current_directory (window, file);
    }
  else
    {
      /* try to launch the selected file */
      if (!thunar_file_launch (file, window, NULL, &error))
        {
          thunar_dialogs_show_error (window, error, _("Failed to launch \"%s\""),
                                     thunar_file_get_display_name (file));
          g_error_free (error);
        }
    }
}



static void
thunar_window_poke_file_finish (ThunarBrowser *browser,
                                ThunarFile    *file,
                                ThunarFile    *target_file,
                                GError        *error,
                                gpointer       ignored)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (error == NULL)
    {
      thunar_window_open_or_launch (THUNAR_WINDOW (browser), target_file);
    }
  else
    {
      thunar_dialogs_show_error (GTK_WIDGET (browser), error,
                                 _("Failed to open \"%s\""),
                                 thunar_file_get_display_name (file));
    }
}



static void
thunar_window_start_open_location (ThunarWindow *window,
                                   const gchar  *initial_text)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* temporary show the location toolbar, even if it is normally hidden */
  gtk_widget_show (window->location_toolbar);
  thunar_location_bar_request_entry (THUNAR_LOCATION_BAR (window->location_bar), initial_text);
}



static void
thunar_window_action_open_new_tab (ThunarWindow *window,
                                   GtkWidget    *menu_item)
{
  /* insert new tab with current directory as default */
  thunar_window_notebook_insert (window, thunar_window_get_current_directory (window));
}



static void
thunar_window_action_open_new_window (ThunarWindow *window,
                                      GtkWidget    *menu_item)
{
  ThunarApplication *application;
  ThunarHistory     *history;
  ThunarWindow      *new_window;
  ThunarFile        *start_file;

  /* popup a new window */
  application = thunar_application_get ();
  new_window = THUNAR_WINDOW (thunar_application_open_window (application, window->current_directory,
                                                              gtk_widget_get_screen (GTK_WIDGET (window)), NULL, TRUE));
  g_object_unref (G_OBJECT (application));

  /* if we have no origin view we are done */
  if (window->view == NULL)
    return;

  /* let the view of the new window inherit the history of the origin view */
  history = thunar_standard_view_copy_history (THUNAR_STANDARD_VIEW (window->view));
  if (history != NULL)
    thunar_standard_view_set_history (THUNAR_STANDARD_VIEW (new_window->view), history);

  /* determine the first visible file in the current window */
  if (thunar_view_get_visible_range (THUNAR_VIEW (window->view), &start_file, NULL))
    {
      /* scroll the new window to the same file */
      thunar_window_scroll_to_file (new_window, start_file, FALSE, TRUE, 0.1f, 0.1f);

      /* release the file reference */
      g_object_unref (G_OBJECT (start_file));
    }
}



static void
thunar_window_action_detach_tab (ThunarWindow *window,
                                 GtkWidget    *menu_item)
{
  GtkWidget *notebook;
  GtkWidget *label;
  GtkWidget *view = window->view;

  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* create a new window */
  notebook = thunar_window_notebook_create_window (window->notebook, view, -1, -1, window);
  if (notebook == NULL)
    return;

  /* get the current label */
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook), view);
  _thunar_return_if_fail (GTK_IS_WIDGET (label));

  /* ref object so they don't destroy when removed from the container */
  g_object_ref (label);
  g_object_ref (view);

  /* remove view from the current notebook */
  gtk_container_remove (GTK_CONTAINER (window->notebook), view);

  /* insert in the new notebook */
  gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), view, label, 0);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (notebook), view, "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), view, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), view, TRUE);

  /* release */
  g_object_unref (label);
  g_object_unref (view);
}



static void
thunar_window_action_close_all_windows (ThunarWindow *window,
                                        GtkWidget    *menu_item)
{
  ThunarApplication *application;
  GList             *windows;

  /* query the list of currently open windows */
  application = thunar_application_get ();
  windows = thunar_application_get_windows (application);
  g_object_unref (G_OBJECT (application));

  /* destroy all open windows */
  g_list_free_full (windows, (GDestroyNotify) gtk_widget_destroy);
}



static void
thunar_window_action_close_tab (ThunarWindow *window,
                                GtkWidget    *menu_item)
{
  if (window->view != NULL)
    gtk_widget_destroy (window->view);
}



static void
thunar_window_action_close_window (ThunarWindow *window,
                                   GtkWidget    *menu_item)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}



static void
thunar_window_action_preferences (ThunarWindow *window,
                                  GtkWidget    *menu_item)
{
  GtkWidget         *dialog;
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* allocate and display a preferences dialog */;
  dialog = thunar_preferences_dialog_new (GTK_WINDOW (window));
  gtk_widget_show (dialog);

  /* ...and let the application take care of it */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));
}



static void
thunar_window_action_reload (ThunarWindow *window,
                             GtkWidget    *menu_item)
{
  gboolean result;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* force the view to reload */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, TRUE, &result);

  /* update the location bar to show the current directory */
  if (window->location_bar != NULL)
    g_object_notify (G_OBJECT (window->location_bar), "current-directory");
}



static void
thunar_window_action_pathbar_changed (ThunarWindow *window)
{
  gchar    *last_location_bar;
  gboolean  pathbar_checked;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  pathbar_checked = exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY));
  g_free (last_location_bar);

  if (pathbar_checked)
    g_object_set (window->preferences, "last-location-bar", g_type_name (G_TYPE_NONE), NULL);
  else
    g_object_set (window->preferences, "last-location-bar", g_type_name (THUNAR_TYPE_LOCATION_ENTRY), NULL);
}



static void
thunar_window_action_toolbar_changed (ThunarWindow *window)
{
  gchar    *last_location_bar;
  gboolean  toolbar_checked;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-location-bar", &last_location_bar, NULL);
  toolbar_checked = exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS));
  g_free (last_location_bar);

  if (toolbar_checked)
    g_object_set (window->preferences, "last-location-bar", g_type_name (G_TYPE_NONE), NULL);
  else
    g_object_set (window->preferences, "last-location-bar", g_type_name (THUNAR_TYPE_LOCATION_BUTTONS), NULL);
}



static void
thunar_window_action_shortcuts_changed (ThunarWindow *window)
{
  gchar    *last_side_pane;
  gboolean  shortcuts_checked;
  GType     type = G_TYPE_NONE;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-side-pane", &last_side_pane, NULL);
  shortcuts_checked = exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_SHORTCUTS_PANE));
  g_free (last_side_pane);

  if (shortcuts_checked)
    type = G_TYPE_NONE;
  else
    type = THUNAR_TYPE_SHORTCUTS_PANE;

  thunar_window_install_sidepane (window, type);
}



static void
thunar_window_action_tree_changed (ThunarWindow *window)
{
  gchar    *last_side_pane;
  gboolean  tree_view_checked;
  GType     type = G_TYPE_NONE;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-side-pane", &last_side_pane, NULL);
  tree_view_checked = exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_TREE_PANE));
  g_free (last_side_pane);

  if (tree_view_checked)
    type = G_TYPE_NONE;
  else
    type = THUNAR_TYPE_TREE_PANE;

  thunar_window_install_sidepane (window, type);
}



static void
thunar_window_action_statusbar_changed (ThunarWindow *window)
{
  gboolean last_statusbar_visible;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-statusbar-visible", &last_statusbar_visible, NULL);

  /* check if we should drop the statusbar */
  if (!last_statusbar_visible && window->statusbar != NULL)
    {
      /* just get rid of the statusbar */
      gtk_widget_destroy (window->statusbar);
      window->statusbar = NULL;
    }
  else if (last_statusbar_visible && window->statusbar == NULL)
    {
      /* setup a new statusbar */
      window->statusbar = thunar_statusbar_new ();
      gtk_widget_set_hexpand (window->statusbar, TRUE);
      gtk_grid_attach (GTK_GRID (window->view_box), window->statusbar, 0, 2, 1, 1);
      gtk_widget_show (window->statusbar);

      /* connect to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        thunar_window_binding_create (window, window->view, "statusbar-text", window->statusbar, "text", G_BINDING_SYNC_CREATE);
    }

  g_object_set (G_OBJECT (window->preferences), "last-statusbar-visible", !last_statusbar_visible, NULL);
}



static void
thunar_window_action_menubar_changed (ThunarWindow *window)
{
  gboolean last_menubar_visible;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  g_object_get (window->preferences, "last-menubar-visible", &last_menubar_visible, NULL);

  gtk_widget_set_visible (window->menubar, !last_menubar_visible);

  g_object_set (G_OBJECT (window->preferences), "last-menubar-visible", !last_menubar_visible, NULL);
}



static void
thunar_window_action_detailed_view (ThunarWindow *window)
{
  thunar_window_action_view_changed (window, THUNAR_TYPE_DETAILS_VIEW);
}



static void
thunar_window_action_icon_view (ThunarWindow *window)
{
  thunar_window_action_view_changed (window, THUNAR_TYPE_ICON_VIEW);
}



static void
thunar_window_action_compact_view (ThunarWindow *window)
{
  thunar_window_action_view_changed (window, THUNAR_TYPE_COMPACT_VIEW);
}



static void
thunar_window_action_view_changed (ThunarWindow *window,
                                   GType         view_type)
{
  ThunarFile     *file = NULL;
  ThunarFile     *current_directory = NULL;
  GtkWidget      *old_view;
  GList          *selected_files = NULL;

  /* drop the previous view (if any) */
  old_view = window->view;
  if (G_LIKELY (window->view != NULL))
    {
      /* get first visible file in the previous view */
      if (!thunar_view_get_visible_range (THUNAR_VIEW (window->view), &file, NULL))
        file = NULL;

      /* store the active directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (window->view));
      if (current_directory != NULL)
        g_object_ref (current_directory);

      /* remember the file selection */
      selected_files = thunar_g_file_list_copy (thunar_component_get_selected_files (THUNAR_COMPONENT (old_view)));
    }
  window->view_type = view_type;

  /* always open a new directory */
  if (current_directory == NULL && window->current_directory != NULL)
    current_directory = g_object_ref (window->current_directory);

  /* allocate a new view of the requested type */
  if (G_LIKELY (window->view_type != G_TYPE_NONE))
    {
      /* create new page */
      if (current_directory != NULL)
        thunar_window_notebook_insert (window, current_directory);

      /* scroll to the previously visible file in the old view */
      if (G_UNLIKELY (file != NULL))
        thunar_view_scroll_to_file (THUNAR_VIEW (window->view), file, FALSE, TRUE, 0.0f, 0.0f);
    }
  else
    {
      /* this should not happen under normal conditions */
      window->view = NULL;
    }

  /* destroy the old view */
  if (old_view != NULL)
    gtk_widget_destroy (old_view);

  /* restore the file selection */
  thunar_component_set_selected_files (THUNAR_COMPONENT (window->view), selected_files);
  thunar_g_file_list_free (selected_files);

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)) && window->view_type != G_TYPE_NONE)
    g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (window->view_type), NULL);

  /* release the file references */
  if (G_UNLIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (current_directory != NULL))
    g_object_unref (G_OBJECT (current_directory));
}



static void
thunar_window_action_go_up (ThunarWindow *window)
{
  ThunarFile *parent;
  GError     *error = NULL;

  parent = thunar_file_get_parent (window->current_directory, &error);
  if (G_LIKELY (parent != NULL))
    {
      thunar_window_set_current_directory (window, parent);
      g_object_unref (G_OBJECT (parent));
    }
  else
    {
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open parent folder"));
      g_error_free (error);
    }
}



static void
thunar_window_action_open_home (ThunarWindow *window)
{
  GFile         *home;
  ThunarFile    *home_file;
  GError        *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the path to the home directory */
  home = thunar_g_file_new_for_home ();

  /* determine the file for the home directory */
  home_file = thunar_file_get (home, &error);
  if (G_UNLIKELY (home_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open the home folder"));
      g_error_free (error);
    }
  else
    {
      /* open the home folder */
      thunar_window_set_current_directory (window, home_file);
      g_object_unref (G_OBJECT (home_file));
    }

  /* release our reference on the home path */
  g_object_unref (home);
}



static gboolean
thunar_window_open_user_folder (ThunarWindow   *window,
                                GUserDirectory  thunar_user_dir,
                                const gchar    *default_name)
{
  ThunarFile  *user_file = NULL;
  gboolean     result = FALSE;
  GError      *error = NULL;
  GFile       *home_dir;
  GFile       *user_dir;
  const gchar *path;
  gint         response;
  GtkWidget   *dialog;
  gchar       *parse_name;

  path = g_get_user_special_dir (thunar_user_dir);
  home_dir = thunar_g_file_new_for_home ();

  /* check if there is an entry in user-dirs.dirs */
  path = g_get_user_special_dir (thunar_user_dir);
  if (G_LIKELY (path != NULL))
    {
      user_dir = g_file_new_for_path (path);

      /* if equal to home, leave */
      if (g_file_equal (user_dir, home_dir))
        goto is_homedir;
    }
  else
    {
      /* build a name */
      user_dir = g_file_resolve_relative_path (home_dir, default_name);
    }

  /* try to load the user dir */
  user_file = thunar_file_get (user_dir, NULL);

  /* check if the directory exists */
  if (G_UNLIKELY (user_file == NULL || !thunar_file_exists (user_file)))
    {
      /* release the instance if it does not exist */
      if (user_file != NULL)
        {
          g_object_unref (user_file);
          user_file = NULL;
        }

      /* ask the user to create the directory */
      parse_name = g_file_get_parse_name (user_dir);
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_YES_NO,
                                       _("The directory \"%s\" does not exist. Do you want to create it?"),
                                       parse_name);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free (parse_name);

      if (response == GTK_RESPONSE_YES
          && g_file_make_directory_with_parents (user_dir, NULL, &error))
        {
          /* try again */
          user_file = thunar_file_get (user_dir, &error);
        }
    }

  if (G_LIKELY (user_file != NULL))
    {
      /* open the folder */
      thunar_window_set_current_directory (window, user_file);
      g_object_unref (G_OBJECT (user_file));
      result = TRUE;
    }
  else if (error != NULL)
    {
      parse_name = g_file_get_parse_name (user_dir);
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open directory \"%s\""), parse_name);
      g_free (parse_name);
      g_error_free (error);
    }

  is_homedir:

  g_object_unref (user_dir);
  g_object_unref (home_dir);

  return result;
}



static void
thunar_window_action_open_desktop (ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_window_open_user_folder (window, G_USER_DIRECTORY_DESKTOP, "Desktop");
}



static void
thunar_window_action_open_computer (ThunarWindow *window)
{
  GFile         *computer;
  ThunarFile    *computer_file;
  GError        *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the computer location */
  computer = g_file_new_for_uri ("computer://");

  /* determine the file for this location */
  computer_file = thunar_file_get (computer, &error);
  if (G_UNLIKELY (computer_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to browse the computer"));
      g_error_free (error);
    }
  else
    {
      /* open the computer location */
      thunar_window_set_current_directory (window, computer_file);
      g_object_unref (G_OBJECT (computer_file));
    }

  /* release our reference on the location itself */
  g_object_unref (computer);
}



static void
thunar_window_action_open_templates (ThunarWindow *window)
{
  GtkWidget     *dialog;
  GtkWidget     *button;
  GtkWidget     *label;
  GtkWidget     *image;
  GtkWidget     *hbox;
  GtkWidget     *vbox;
  gboolean       show_about_templates;
  gboolean       success;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  success = thunar_window_open_user_folder (window, G_USER_DIRECTORY_TEMPLATES, "Templates");

  /* check whether we should display the "About Templates" dialog */
  g_object_get (G_OBJECT (window->preferences),
                "misc-show-about-templates", &show_about_templates,
                NULL);

  if (G_UNLIKELY(show_about_templates && success))
    {
      /* display the "About Templates" dialog */
      dialog = gtk_dialog_new_with_buttons (_("About Templates"), GTK_WINDOW (window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            _("_OK"), GTK_RESPONSE_OK,
                                            NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
      gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (image, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new (_("All files in this folder will appear in the \"Create Document\" menu."));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big_bold ());
      gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      label = gtk_label_new (_("If you frequently create certain kinds "
                             " of documents, make a copy of one and put it in this "
                             "folder. Thunar will add an entry for this document in the"
                             " \"Create Document\" menu.\n\n"
                             "You can then select the entry from the \"Create Document\" "
                             "menu and a copy of the document will be created in the "
                             "directory you are viewing."));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_check_button_new_with_mnemonic (_("Do _not display this message again"));
      exo_mutual_binding_new_with_negation (G_OBJECT (window->preferences), "misc-show-about-templates", G_OBJECT (button), "active");
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      gtk_window_set_default_size (GTK_WINDOW (dialog), 600, -1);

      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }
}



static void
thunar_window_action_open_file_system (ThunarWindow *window)
{
  GFile         *root;
  ThunarFile    *root_file;
  GError        *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the path to the root directory */
  root = thunar_g_file_new_for_root ();

  /* determine the file for the root directory */
  root_file = thunar_file_get (root, &error);
  if (G_UNLIKELY (root_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open the file system root folder"));
      g_error_free (error);
    }
  else
    {
      /* open the root folder */
      thunar_window_set_current_directory (window, root_file);
      g_object_unref (G_OBJECT (root_file));
    }

  /* release our reference on the home path */
  g_object_unref (root);
}



static void
thunar_window_action_open_trash (ThunarWindow *window)
{
  GFile      *trash_bin;
  ThunarFile *trash_bin_file;
  GError     *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the path to the trash bin */
  trash_bin = thunar_g_file_new_for_trash ();

  /* determine the file for the trash bin */
  trash_bin_file = thunar_file_get (trash_bin, &error);
  if (G_UNLIKELY (trash_bin_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to display the contents of the trash can"));
      g_error_free (error);
    }
  else
    {
      /* open the trash folder */
      thunar_window_set_current_directory (window, trash_bin_file);
      g_object_unref (G_OBJECT (trash_bin_file));
    }

  /* release our reference on the trash bin path */
  g_object_unref (trash_bin);
}



static void
thunar_window_action_open_network (ThunarWindow *window)
{
  ThunarFile *network_file;
  GError     *error = NULL;
  GFile      *network;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the network root location */
  network = g_file_new_for_uri ("network://");

  /* determine the file for this location */
  network_file = thunar_file_get (network, &error);
  if (G_UNLIKELY (network_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to browse the network"));
      g_error_free (error);
    }
  else
    {
      /* open the network root location */
      thunar_window_set_current_directory (window, network_file);
      g_object_unref (G_OBJECT (network_file));
    }

  /* release our reference on the location itself */
  g_object_unref (network);
}



static gboolean
thunar_window_propagate_key_event (GtkWindow* window,
                                   GdkEvent  *key_event,
                                   gpointer   user_data)
{
  GtkWidget* focused_widget;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), GDK_EVENT_PROPAGATE);

  focused_widget = gtk_window_get_focus (window);

  /* Turn the accelerator priority around globally,
   * so that the focused widget always gets the accels first.
   * Implementing this cleanly while maintaining some wanted accels
   * (like Ctrl+N and exo accels) is a lot of work. So we resort to
   * only priorize GtkEditable, because that is the easiest way to
   * fix the right-ahead problem. */
  if (focused_widget != NULL && GTK_IS_EDITABLE (focused_widget))
    return gtk_window_propagate_key_event (window, (GdkEventKey *) key_event);

  return GDK_EVENT_PROPAGATE;
}



static void
thunar_window_poke_location_finish (ThunarBrowser *browser,
                                    GFile         *location,
                                    ThunarFile    *file,
                                    ThunarFile    *target_file,
                                    GError        *error,
                                    gpointer       ignored)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  thunar_window_poke_file_finish (browser, file, target_file, error, ignored);
}



static void
thunar_window_action_open_bookmark (GtkWidget *menu_item)
{
  ThunarFile *local_file;
  GFile      *remote_file;
  GtkWindow  *window;

  /* try to open the local file */
  local_file = g_object_get_data (G_OBJECT (menu_item), I_("thunar-file"));
  window = g_object_get_data (G_OBJECT (menu_item), I_("thunar-window"));
  if (local_file != NULL)
    {
      thunar_window_set_current_directory (THUNAR_WINDOW (window), local_file);
      g_object_unref (local_file);
      return;
    }

  /* try to poke remote files */
  remote_file = g_object_get_data (G_OBJECT (menu_item), I_("location-file"));
  if (remote_file != NULL)
    {
      thunar_browser_poke_location (THUNAR_BROWSER (window), remote_file, THUNAR_WINDOW (window),
                                    thunar_window_poke_location_finish, NULL);
      g_object_unref (remote_file);
    }
}



static void
thunar_window_action_open_location (ThunarWindow *window)
{
  /* just use the "start-open-location" callback */
  thunar_window_start_open_location (window, NULL);
}



static void
thunar_window_action_contents (ThunarWindow *window)
{
  /* display the documentation index */
  xfce_dialog_show_help (GTK_WINDOW (window), "thunar", NULL, NULL);
}



static void
thunar_window_action_about (ThunarWindow *window)
{
  /* just popup the about dialog */
  thunar_dialogs_show_about (GTK_WINDOW (window), PACKAGE_NAME,
                             _("Thunar is a fast and easy to use file manager\n"
                               "for the Xfce Desktop Environment."));
}



static void
thunar_window_action_show_hidden (ThunarWindow *window)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  window->show_hidden = !window->show_hidden;
  gtk_container_foreach (GTK_CONTAINER (window->notebook), (GtkCallback) (void (*)(void)) thunar_view_set_show_hidden, GINT_TO_POINTER (window->show_hidden));
  thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (window->sidepane), window->show_hidden);

  g_object_set (G_OBJECT (window->preferences), "last-show-hidden", window->show_hidden, NULL);

}



static void
thunar_window_action_open_file_menu (ThunarWindow *window)
{
  GtkWidget *file_menu;
  gboolean ret;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  file_menu = gtk_container_get_children (GTK_CONTAINER (window->menubar))->data;
  g_signal_emit_by_name (file_menu, "button-press-event", NULL, &ret);
  gtk_menu_shell_select_first (GTK_MENU_SHELL (window->menubar), TRUE);
}



static void
thunar_window_current_directory_changed (ThunarFile   *current_directory,
                                         ThunarWindow *window)
{
  gboolean      show_full_path;
  gchar        *parse_name = NULL;
  const gchar  *name;
  gboolean      ret;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (window->current_directory == current_directory);

  /* get name of directory or full path */
  g_object_get (G_OBJECT (window->preferences), "misc-full-path-in-title", &show_full_path, NULL);
  if (G_UNLIKELY (show_full_path))
    name = parse_name = g_file_get_parse_name (thunar_file_get_file (current_directory));
  else
    name = thunar_file_get_display_name (current_directory);

  /* set window title */
  gtk_window_set_title (GTK_WINDOW (window), name);
  g_free (parse_name);

  /* set window icon */
  thunar_window_update_window_icon (window);

  /* update the window menu. E.g. relevant for keyboard navigation in the window menu */
  for (GList *lp = gtk_container_get_children (GTK_CONTAINER (window->menubar)); lp != NULL; lp = lp->next)
      g_signal_emit_by_name (lp->data, "button-press-event", NULL, &ret);
}



static void
thunar_window_connect_proxy (GtkUIManager *manager,
                             GtkAction    *action,
                             GtkWidget    *proxy,
                             ThunarWindow *window)
{
  /* we want to get informed when the user hovers a menu item */
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_connect_closure (G_OBJECT (proxy), "select", window->menu_item_selected_closure, FALSE);
      g_signal_connect_closure (G_OBJECT (proxy), "deselect", window->menu_item_deselected_closure, FALSE);
    }
}



static void
thunar_window_disconnect_proxy (GtkUIManager *manager,
                                GtkAction    *action,
                                GtkWidget    *proxy,
                                ThunarWindow *window)
{
  /* undo what we did in connect_proxy() */
  if (GTK_IS_MENU_ITEM (proxy))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (proxy), G_SIGNAL_MATCH_CLOSURE, 0, 0, window->menu_item_selected_closure, NULL, NULL);
      g_signal_handlers_disconnect_matched (G_OBJECT (proxy), G_SIGNAL_MATCH_CLOSURE, 0, 0, window->menu_item_deselected_closure, NULL, NULL);
    }
}



static void
thunar_window_menu_item_selected (GtkWidget    *menu_item,
                                  ThunarWindow *window)
{
  GtkAction   *action;
  const gchar *tooltip;
  gint         id;
  gchar       *short_tip = NULL;
  gchar       *p;

  /* we can only display tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* determine the action for the menu item */
      action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (menu_item));
      if (G_UNLIKELY (action == NULL))
        return;

      /* determine the tooltip from the action */
      tooltip = gtk_action_get_tooltip (action);
G_GNUC_END_IGNORE_DEPRECATIONS
      if (G_LIKELY (tooltip != NULL))
        {
          /* check if there is a new line in the tooltip */
          p = strchr (tooltip, '\n');
          if (p != NULL)
            {
              short_tip = g_strndup (tooltip, p - tooltip);
              tooltip = short_tip;
            }

          /* push to the statusbar */
          id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "Menu tooltip");
          gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), id, tooltip);
          g_free (short_tip);
        }
    }
}



static void
thunar_window_menu_item_deselected (GtkWidget    *menu_item,
                                    ThunarWindow *window)
{
  gint id;

  /* we can only undisplay tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* drop the last tooltip from the statusbar */
      id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "Menu tooltip");
      gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar), id);
    }
}



static void
thunar_window_update_custom_actions (ThunarView   *view,
                                     GParamSpec   *pspec,
                                     ThunarWindow *window)
{
  ThunarFile      *folder;
  GList           *selected_files;
  GList           *items = NULL;
  GList           *lp;
  GList           *providers;
  GList           *tmp;

  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* leave if the signal is emitted from a non-active tab */
  if (!gtk_widget_get_realized (GTK_WIDGET (window))
      || window->view != GTK_WIDGET (view))
    return;

  /* grab a reference to the current directory of the window */
  folder = thunar_window_get_current_directory (window);

  /* leave if current directory is invalid */
  if (folder != NULL &&
      !thunarx_file_info_is_directory (THUNARX_FILE_INFO (folder)))
      return;

  /* load the menu provides from the provider factory */
  providers = thunarx_provider_factory_list_providers (window->provider_factory,
                                                       THUNARX_TYPE_MENU_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
      /* get a list of selected files */
      selected_files = thunar_component_get_selected_files (THUNAR_COMPONENT (view));

      /* load the actions offered by the menu providers */
      for (lp = providers; lp != NULL; lp = lp->next)
        {
          if (G_LIKELY (selected_files != NULL))
            {
              tmp = thunarx_menu_provider_get_file_menu_items (lp->data,
                                                               GTK_WIDGET (window),
                                                               selected_files);
            }
          else if (G_LIKELY (folder != NULL))
            {
              tmp = thunarx_menu_provider_get_folder_menu_items (lp->data,
                                                                 GTK_WIDGET (window),
                                                                 THUNARX_FILE_INFO (folder));
            }
          else
            {
              tmp = NULL;
            }

          items = g_list_concat (items, tmp);
          g_object_unref (G_OBJECT (lp->data));
        }
      g_list_free (providers);
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* remove previously inserted menu actions from the UI manager */
  if (window->custom_merge_id != 0)
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->custom_merge_id);
      gtk_ui_manager_ensure_update (window->ui_manager);
      window->custom_merge_id = 0;
    }

  /* drop any previous custom action group */
  if (window->custom_actions != NULL)
    {
      gtk_ui_manager_remove_action_group (window->ui_manager, window->custom_actions);
      g_object_unref (window->custom_actions);
      window->custom_actions = NULL;
    }

  /* add the actions specified by the menu providers */
  if (G_LIKELY (items != NULL))
    {
      /* allocate the action group and the merge id for the custom actions */
      window->custom_actions = gtk_action_group_new ("ThunarActions");
      window->custom_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

      /* insert the new action group and make sure the UI manager gets updated */
      gtk_ui_manager_insert_action_group (window->ui_manager, window->custom_actions, 0);
      gtk_ui_manager_ensure_update (window->ui_manager);

      /* add the menu items to the UI manager */
      thunar_menu_util_add_items_to_ui_manager (window->ui_manager,
                                                window->custom_actions,
                                                window->custom_merge_id,
                                                "/main-menu/file-menu/placeholder-custom-actions",
                                                items);

      /* cleanup */
      g_list_free (items);
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}



static void
thunar_window_notify_loading (ThunarView   *view,
                              GParamSpec   *pspec,
                              ThunarWindow *window)
{
  GdkCursor *cursor;

  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (gtk_widget_get_realized (GTK_WIDGET (window))
      && window->view == GTK_WIDGET (view))
    {
      /* setup the proper cursor */
      if (thunar_view_get_loading (view))
        {
          cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (view)), GDK_WATCH);
          gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
          g_object_unref (cursor);
        }
      else
        {
          gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
        }
    }
}



static void
thunar_window_device_pre_unmount (ThunarDeviceMonitor *device_monitor,
                                  ThunarDevice        *device,
                                  GFile               *root_file,
                                  ThunarWindow        *window)
{
  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (window->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_FILE (root_file));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* nothing to do if we don't have a current directory */
  if (G_UNLIKELY (window->current_directory == NULL))
    return;

  /* check if the file is the current directory or an ancestor of the current directory */
  if (g_file_equal (thunar_file_get_file (window->current_directory), root_file)
      || thunar_file_is_gfile_ancestor (window->current_directory, root_file))
    {
      /* change to the home folder */
      thunar_window_action_open_home (window);
    }
}



static void
thunar_window_device_changed (ThunarDeviceMonitor *device_monitor,
                              ThunarDevice        *device,
                              ThunarWindow        *window)
{
  GFile *root_file;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (window->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  if (thunar_device_is_mounted (device))
    return;

  root_file = thunar_device_get_root (device);
  if (root_file != NULL)
    {
      thunar_window_device_pre_unmount (device_monitor, device, root_file, window);
      g_object_unref (root_file);
    }
}



static gboolean
thunar_window_merge_idle (gpointer user_data)
{
  ThunarWindow *window = THUNAR_WINDOW (user_data);

  /* merge custom preferences from the providers */
THUNAR_THREADS_ENTER
  thunar_window_merge_custom_preferences (window);
THUNAR_THREADS_LEAVE

  thunar_window_bookmark_merge (window);

  return FALSE;
}



static void
thunar_window_merge_idle_destroy (gpointer user_data)
{
  THUNAR_WINDOW (user_data)->merge_idle_id = 0;
}



static gboolean
thunar_window_save_paned (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  g_object_set (G_OBJECT (window->preferences), "last-separator-position",
                gtk_paned_get_position (GTK_PANED (window->paned)), NULL);

  /* for button release event */
  return FALSE;
}



static gboolean
thunar_window_save_geometry_timer (gpointer user_data)
{
  GdkWindowState state;
  ThunarWindow  *window = THUNAR_WINDOW (user_data);
  gboolean       remember_geometry;
  gint           width;
  gint           height;

THUNAR_THREADS_ENTER

  /* check if we should remember the window geometry */
  g_object_get (G_OBJECT (window->preferences), "misc-remember-geometry", &remember_geometry, NULL);
  if (G_LIKELY (remember_geometry))
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              /* determine the current width/height of the window... */
              gtk_window_get_size (GTK_WINDOW (window), &width, &height);

              /* ...and remember them as default for new windows */
              g_object_set (G_OBJECT (window->preferences), "last-window-width", width, "last-window-height", height,
                            "last-window-maximized", FALSE, NULL);
            }
          else
            {
              /* only store that the window is full screen */
              g_object_set (G_OBJECT (window->preferences), "last-window-maximized", TRUE, NULL);
            }
        }
    }

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_window_save_geometry_timer_destroy (gpointer user_data)
{
  THUNAR_WINDOW (user_data)->save_geometry_timer_id = 0;
}



/**
 * thunar_window_set_zoom_level:
 * @window     : a #ThunarWindow instance.
 * @zoom_level : the new zoom level for @window.
 *
 * Sets the zoom level for @window to @zoom_level.
 **/
void
thunar_window_set_zoom_level (ThunarWindow   *window,
                              ThunarZoomLevel zoom_level)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (zoom_level < THUNAR_ZOOM_N_LEVELS);

  /* check if we have a new zoom level */
  if (G_LIKELY (window->zoom_level != zoom_level))
    {
      /* remember the new zoom level */
      window->zoom_level = zoom_level;

      /* notify listeners */
      g_object_notify (G_OBJECT (window), "zoom-level");
    }
}



/**
 * thunar_window_get_current_directory:
 * @window : a #ThunarWindow instance.
 *
 * Queries the #ThunarFile instance, which represents the directory
 * currently displayed within @window. %NULL is returned if @window
 * is not currently associated with any directory.
 *
 * Return value: the directory currently displayed within @window or %NULL.
 **/
ThunarFile*
thunar_window_get_current_directory (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
  return window->current_directory;
}



/**
 * thunar_window_set_current_directory:
 * @window            : a #ThunarWindow instance.
 * @current_directory : the new directory or %NULL.
 **/
void
thunar_window_set_current_directory (ThunarWindow *window,
                                     ThunarFile   *current_directory)
{
  GType  type;
  gchar *type_name;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we already display the requested directory */
  if (G_UNLIKELY (window->current_directory == current_directory))
    return;

  /* disconnect from the previously active directory */
  if (G_LIKELY (window->current_directory != NULL))
    {
      /* disconnect signals and release reference */
      g_signal_handlers_disconnect_by_func (G_OBJECT (window->current_directory), thunar_window_current_directory_changed, window);
      g_object_unref (G_OBJECT (window->current_directory));
    }

  /* activate the new directory */
  window->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the file and connect the "changed"/"destroy" signals */
      g_signal_connect (G_OBJECT (current_directory), "changed", G_CALLBACK (thunar_window_current_directory_changed), window);
      g_object_ref (G_OBJECT (current_directory));

      /* create a new view if the window is new */
      if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) == 0)
        {
          /* determine the default view */
          g_object_get (G_OBJECT (window->preferences), "default-view", &type_name, NULL);
          type = g_type_from_name (type_name);
          g_free (type_name);

          /* determine the last selected view if the last selected view preference is not selected */
          if (g_type_is_a (type, G_TYPE_NONE) || g_type_is_a (type, G_TYPE_INVALID))
            {
              g_object_get (G_OBJECT (window->preferences), "last-view", &type_name, NULL);
              type = g_type_from_name (type_name);
              g_free (type_name);
            }

          /* fallback, in case nothing was set */
          if (g_type_is_a (type, G_TYPE_NONE) || g_type_is_a (type, G_TYPE_INVALID))
            type = THUNAR_TYPE_ICON_VIEW;

          thunar_window_action_view_changed (window, type);
        }

      /* update window icon and title */
      thunar_window_current_directory_changed (current_directory, window);

      if (G_LIKELY (window->view != NULL))
        {
          /* grab the focus to the main view */
          gtk_widget_grab_focus (window->view);
        }

  /* enable the 'Up' action if possible for the new directory */
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "open-parent", (current_directory != NULL
                                                && thunar_file_has_parent (current_directory)));
    }

  /* tell everybody that we have a new "current-directory",
   * we do this first so other widgets display the new
   * state already while the folder view is loading.
   */
  g_object_notify (G_OBJECT (window), "current-directory");
}



/**
 * thunar_window_scroll_to_file:
 * @window      : a #ThunarWindow instance.
 * @file        : a #ThunarFile.
 * @select_file : if %TRUE the @file will also be selected.
 * @use_align   : %TRUE to use the alignment arguments.
 * @row_align   : the vertical alignment.
 * @col_align   : the horizontal alignment.
 *
 * Tells the @window to scroll to the @file
 * in the current view.
 **/
void
thunar_window_scroll_to_file (ThunarWindow *window,
                              ThunarFile   *file,
                              gboolean      select_file,
                              gboolean      use_align,
                              gfloat        row_align,
                              gfloat        col_align)
{
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* verify that we have a valid view */
  if (G_LIKELY (window->view != NULL))
    thunar_view_scroll_to_file (THUNAR_VIEW (window->view), file, select_file, use_align, row_align, col_align);
}



gchar **
thunar_window_get_directories (ThunarWindow *window,
                               gint         *active_page)
{
  gint         n;
  gint         n_pages;
  gchar      **uris;
  GtkWidget   *view;
  ThunarFile  *directory;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (G_UNLIKELY (n_pages == 0))
    return NULL;

  /* create array of uris */
  uris = g_new0 (gchar *, n_pages + 1);
  for (n = 0; n < n_pages; n++)
    {
      /* get the view */
      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);
      _thunar_return_val_if_fail (THUNAR_IS_NAVIGATOR (view), FALSE);

      /* get the directory of the view */
      directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (view));
      _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), FALSE);

      /* add to array */
      uris[n] = thunar_file_dup_uri (directory);
    }

  /* selected tab */
  if (active_page != NULL)
    *active_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  return uris;
}



gboolean
thunar_window_set_directories (ThunarWindow   *window,
                               gchar         **uris,
                               gint            active_page)
{
  ThunarFile *directory;
  guint       n;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);
  _thunar_return_val_if_fail (uris != NULL, FALSE);

  for (n = 0; uris[n] != NULL; n++)
    {
      /* check if the string looks like an uri */
      if (!exo_str_looks_like_an_uri (uris[n]))
        continue;

      /* get the file for the uri */
      directory = thunar_file_get_for_uri (uris[n], NULL);
      if (G_UNLIKELY (directory == NULL))
        continue;

      /* open the directory in a new notebook */
      if (thunar_file_is_directory (directory))
        {
          if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) == 0)
            thunar_window_set_current_directory (window, directory);
          else
            thunar_window_notebook_insert (window, directory);
        }

      g_object_unref (G_OBJECT (directory));
    }

  /* select the page */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), active_page);

  /* we succeeded if new pages have been opened */
  return gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) > 0;
}



/**
 * thunar_window_get_action_entry:
 * @window  : Instance of a  #ThunarWindow
 * @action  : #ThunarWindowAction for which the #XfceGtkActionEntry is requested
 *
 * returns a reference to the requested #XfceGtkActionEntry
 *
 * Return value: (transfer none): The reference to the #XfceGtkActionEntry
 **/
const XfceGtkActionEntry*
thunar_window_get_action_entry (ThunarWindow       *window,
                                ThunarWindowAction  action)
{
  return get_action_entry (action);
}



/**
 * thunar_window_append_menu_item:
 * @window  : Instance of a  #ThunarWindow
 * @menu    : #GtkMenuShell to which the item should be added
 * @action  : #ThunarWindowAction to select which item should be added
 *
 * Adds the selected, widget specific #GtkMenuItem to the passed #GtkMenuShell
 *
 * Return value: (transfer none): The added #GtkMenuItem
 **/
void
thunar_window_append_menu_item (ThunarWindow       *window,
                                GtkMenuShell       *menu,
                                ThunarWindowAction  action)
{
  GtkWidget *item;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action), G_OBJECT (window), menu);

  if (action == THUNAR_WINDOW_ACTION_ZOOM_IN)
    gtk_widget_set_sensitive (item, G_LIKELY (window->zoom_level < THUNAR_ZOOM_N_LEVELS - 1));
  if (action == THUNAR_WINDOW_ACTION_ZOOM_OUT)
    gtk_widget_set_sensitive (item, G_LIKELY (window->zoom_level > 0));
}



/**
 * thunar_window_get_launcher:
 * @window : a #ThunarWindow instance.
 *
 * Return value: (transfer none): The single #ThunarLauncher of this #ThunarWindow
 **/
ThunarLauncher*
thunar_window_get_launcher (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);

  return window->launcher;
}
