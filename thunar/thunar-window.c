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
#include <thunar/thunar-location-dialog.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-stock.h>
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
  PROP_SHOW_HIDDEN,
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



static void     thunar_window_dispose                     (GObject                *object);
static void     thunar_window_finalize                    (GObject                *object);
static void     thunar_window_get_property                (GObject                *object,
                                                           guint                   prop_id,
                                                           GValue                 *value,
                                                           GParamSpec             *pspec);
static void     thunar_window_set_property                (GObject                *object,
                                                           guint                   prop_id,
                                                           const GValue           *value,
                                                           GParamSpec             *pspec);
static gboolean thunar_window_back                        (ThunarWindow           *window);
static gboolean thunar_window_reload                      (ThunarWindow           *window,
                                                           gboolean                reload_info);
static gboolean thunar_window_toggle_sidepane             (ThunarWindow           *window);
static gboolean thunar_window_toggle_menubar              (ThunarWindow           *window);
static void     thunar_window_toggle_menubar_deactivate   (GtkWidget              *menubar,
                                                           ThunarWindow           *window);
static gboolean thunar_window_zoom_in                     (ThunarWindow           *window);
static gboolean thunar_window_zoom_out                    (ThunarWindow           *window);
static gboolean thunar_window_zoom_reset                  (ThunarWindow           *window);
static gboolean thunar_window_tab_change                  (ThunarWindow           *window,
                                                           gint                    nth);
static void     thunar_window_realize                     (GtkWidget              *widget);
static void     thunar_window_unrealize                   (GtkWidget              *widget);
static gboolean thunar_window_configure_event             (GtkWidget              *widget,
                                                           GdkEventConfigure      *event);
static void     thunar_window_notebook_switch_page        (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void     thunar_window_notebook_page_added         (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static void     thunar_window_notebook_page_removed       (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           guint                   page_num,
                                                           ThunarWindow           *window);
static gboolean thunar_window_notebook_button_press_event (GtkWidget              *notebook,
                                                           GdkEventButton         *event,
                                                           ThunarWindow           *window);
static gboolean thunar_window_notebook_popup_menu         (GtkWidget              *notebook,
                                                           ThunarWindow           *window);
static gpointer thunar_window_notebook_create_window      (GtkWidget              *notebook,
                                                           GtkWidget              *page,
                                                           gint                    x,
                                                           gint                    y,
                                                           ThunarWindow           *window);
static void     thunar_window_notebook_insert             (ThunarWindow           *window,
                                                           ThunarFile             *directory);
static void     thunar_window_merge_custom_preferences    (ThunarWindow           *window);
static gboolean thunar_window_bookmark_merge              (gpointer                user_data);
static void     thunar_window_merge_go_actions            (ThunarWindow           *window);
static void     thunar_window_install_location_bar        (ThunarWindow           *window,
                                                           GType                   type);
static void     thunar_window_install_sidepane            (ThunarWindow           *window,
                                                           GType                   type);
static void     thunar_window_start_open_location         (ThunarWindow           *window,
                                                           const gchar            *initial_text);
static void     thunar_window_action_open_new_tab         (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_new_window      (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_empty_trash          (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_detach_tab           (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_close_all_windows    (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_close_tab            (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_close_window         (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_preferences          (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_reload               (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_pathbar_changed      (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_toolbar_changed      (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_shortcuts_changed    (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_tree_changed         (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_statusbar_changed    (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_menubar_changed      (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_zoom_in              (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_zoom_out             (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_zoom_reset           (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_view_changed         (GtkRadioAction         *action,
                                                           GtkRadioAction         *current,
                                                           ThunarWindow           *window);
static void     thunar_window_action_go_up                (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_home            (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_desktop         (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_templates       (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_file_system     (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_trash           (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_network         (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_bookmark        (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_open_location        (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_contents             (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_about                (GtkAction              *action,
                                                           ThunarWindow           *window);
static void     thunar_window_action_show_hidden          (GtkToggleAction        *action,
                                                           ThunarWindow           *window);
static void     thunar_window_current_directory_changed   (ThunarFile             *current_directory,
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
static void     thunar_window_notify_loading              (ThunarView             *view,
                                                           GParamSpec             *pspec,
                                                           ThunarWindow           *window);
static void     thunar_window_device_pre_unmount          (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           GFile                  *root_file,
                                                           ThunarWindow           *window);
static void     thunar_window_device_changed              (ThunarDeviceMonitor    *device_monitor,
                                                           ThunarDevice           *device,
                                                           ThunarWindow           *window);
static gboolean thunar_window_merge_idle                  (gpointer                user_data);
static void     thunar_window_merge_idle_destroy          (gpointer                user_data);
static gboolean thunar_window_save_paned                  (ThunarWindow           *window);
static gboolean thunar_window_save_geometry_timer         (gpointer                user_data);
static void     thunar_window_save_geometry_timer_destroy (gpointer                user_data);
static void     thunar_window_set_zoom_level              (ThunarWindow           *window,
                                                           ThunarZoomLevel         zoom_level);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;

  /* internal action signals */
  gboolean (*back)            (ThunarWindow *window);
  gboolean (*reload)          (ThunarWindow *window,
                               gboolean      reload_info);
  gboolean (*toggle_sidepane) (ThunarWindow *window);
  gboolean (*toggle_menubar)  (ThunarWindow *window);
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

  GtkWidget              *table;
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

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* menu merge idle source */
  guint                   merge_idle_id;

  /* support to remember window geometry */
  guint                   save_geometry_timer_id;

  /* support to toggle side pane using F9,
   * see the toggle_sidepane() function.
   */
  GType                   toggle_sidepane_type;
};



static GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "new-tab", "tab-new", N_ ("New _Tab"), "<control>T", N_ ("Open a new tab for the displayed location"), G_CALLBACK (thunar_window_action_open_new_tab), },
  { "new-window", "window-new", N_ ("New _Window"), "<control>N", N_ ("Open a new Thunar window for the displayed location"), G_CALLBACK (thunar_window_action_open_new_window), },
  { "sendto-menu", NULL, N_ ("_Send To"), NULL, },
  { "empty-trash", NULL, N_ ("_Empty Trash"), NULL, N_ ("Delete all files and folders in the Trash"), G_CALLBACK (thunar_window_action_empty_trash), },
  { "detach-tab", NULL, N_ ("Detac_h Tab"), NULL, N_ ("Open current folder in a new window"), G_CALLBACK (thunar_window_action_detach_tab), },
  { "close-all-windows", NULL, N_ ("Close _All Windows"), "<control><shift>W", N_ ("Close all Thunar windows"), G_CALLBACK (thunar_window_action_close_all_windows), },
  { "close-tab", "window-close", N_ ("C_lose Tab"), "<control>W", N_ ("Close this folder"), G_CALLBACK (thunar_window_action_close_tab), },
  { "close-window", "application-exit", N_ ("_Close Window"), "<control>Q", N_ ("Close this window"), G_CALLBACK (thunar_window_action_close_window), },
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "preferences", "preferences-system", N_ ("Pr_eferences..."), NULL, N_ ("Edit Thunars Preferences"), G_CALLBACK (thunar_window_action_preferences), },
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "reload", "view-refresh", N_ ("_Reload"), "<control>R", N_ ("Reload the current folder"), G_CALLBACK (thunar_window_action_reload), },
  { "view-location-selector-menu", NULL, N_ ("_Location Selector"), NULL, },
  { "view-side-pane-menu", NULL, N_ ("_Side Pane"), NULL, },
  { "zoom-in", "zoom-in", N_ ("Zoom I_n"), "<control>plus", N_ ("Show the contents in more detail"), G_CALLBACK (thunar_window_action_zoom_in), },
  { "zoom-out", "zoom-out", N_ ("Zoom _Out"), "<control>minus", N_ ("Show the contents in less detail"), G_CALLBACK (thunar_window_action_zoom_out), },
  { "zoom-reset", "zoom-original", N_ ("Normal Si_ze"), "<control>0", N_ ("Show the contents at the normal size"), G_CALLBACK (thunar_window_action_zoom_reset), },
  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "open-parent", "go-up", N_ ("Open _Parent"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (thunar_window_action_go_up), },
  { "open-home", "go-home", N_ ("_Home"), "<alt>Home", N_ ("Go to the home folder"), G_CALLBACK (thunar_window_action_open_home), },
  { "open-desktop", THUNAR_STOCK_DESKTOP, N_ ("Desktop"), NULL, N_ ("Go to the desktop folder"), G_CALLBACK (thunar_window_action_open_desktop), },
  { "open-file-system", "drive-harddisk", N_ ("File System"), NULL, N_ ("Browse the file system"), G_CALLBACK (thunar_window_action_open_file_system), },
  { "open-network", "network-workgroup", N_("B_rowse Network"), NULL, N_ ("Browse local network connections"), G_CALLBACK (thunar_window_action_open_network), },
  { "open-templates", THUNAR_STOCK_TEMPLATES, N_("T_emplates"), NULL, N_ ("Go to the templates folder"), G_CALLBACK (thunar_window_action_open_templates), },
  { "open-location", NULL, N_ ("_Open Location..."), "<control>L", N_ ("Specify a location to open"), G_CALLBACK (thunar_window_action_open_location), },
  { "help-menu", NULL, N_ ("_Help"), NULL, },
  { "contents", "help-browser", N_ ("_Contents"), "F1", N_ ("Display Thunar user manual"), G_CALLBACK (thunar_window_action_contents), },
  { "about", "help-about", N_ ("_About"), NULL, N_ ("Display information about Thunar"), G_CALLBACK (thunar_window_action_about), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "show-hidden", NULL, N_ ("Show _Hidden Files"), "<control>H", N_ ("Toggles the display of hidden files in the current window"), G_CALLBACK (thunar_window_action_show_hidden), FALSE, },
  { "view-location-selector-pathbar", NULL, N_ ("_Pathbar Style"), NULL, N_ ("Modern approach with buttons that correspond to folders"), G_CALLBACK (thunar_window_action_pathbar_changed), FALSE, },
  { "view-location-selector-toolbar", NULL, N_ ("_Toolbar Style"), NULL, N_ ("Traditional approach with location bar and navigation buttons"), G_CALLBACK (thunar_window_action_toolbar_changed), FALSE, },
  { "view-side-pane-shortcuts", NULL, N_ ("_Shortcuts"), "<control>B", N_ ("Toggles the visibility of the shortcuts pane"), G_CALLBACK (thunar_window_action_shortcuts_changed), FALSE, },
  { "view-side-pane-tree", NULL, N_ ("_Tree"), "<control>E", N_ ("Toggles the visibility of the tree pane"), G_CALLBACK (thunar_window_action_tree_changed), FALSE, },
  { "view-statusbar", NULL, N_ ("St_atusbar"), NULL, N_ ("Change the visibility of this window's statusbar"), G_CALLBACK (thunar_window_action_statusbar_changed), FALSE, },
  { "view-menubar", NULL, N_ ("_Menubar"), "<control>M", N_ ("Change the visibility of this window's menubar"), G_CALLBACK (thunar_window_action_menubar_changed), TRUE, },
};



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
  klass->toggle_sidepane = thunar_window_toggle_sidepane;
  klass->toggle_menubar = thunar_window_toggle_menubar;
  klass->zoom_in = thunar_window_zoom_in;
  klass->zoom_out = thunar_window_zoom_out;
  klass->zoom_reset = thunar_window_zoom_reset;
  klass->tab_change = thunar_window_tab_change;

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

  /**
   * ThunarWindow:show-hidden:
   *
   * Whether to show hidden files in the current window.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HIDDEN,
                                   g_param_spec_boolean ("show-hidden",
                                                         "show-hidden",
                                                         "show-hidden",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

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
                                                      THUNAR_ZOOM_LEVEL_NORMAL,
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
   * of the currently displayed folder. This is an internal
   * signal used to bind the action to keys.
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
   * ThunarWindow::toggle-sidepane:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user toggles the visibility of the
   * sidepane. This is an internal signal used to bind the
   * action to keys.
   **/
  window_signals[TOGGLE_SIDEPANE] =
    g_signal_new (I_("toggle-sidepane"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, toggle_sidepane),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * ThunarWindow::toggle-menubar:
   * @window : a #ThunarWindow instance.
   *
   * Emitted whenever the user toggles the visibility of the
   * menubar. This is an internal signal used to bind the
   * action to keys.
   **/
  window_signals[TOGGLE_MENUBAR] =
    g_signal_new (I_("toggle-menubar"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarWindowClass, toggle_menubar),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

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
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0, "back", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_F5, 0, "reload", 1, G_TYPE_BOOLEAN, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_F9, 0, "toggle-sidepane", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_F10, 0, "toggle-menubar", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_CONTROL_MASK, "zoom-in", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, GDK_CONTROL_MASK, "zoom-out", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_0, GDK_CONTROL_MASK, "zoom-reset", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Insert, GDK_CONTROL_MASK, "zoom-reset", 0);

  /* setup the key bindings for Alt+N */
  for (i = 0; i < 10; i++)
    {
      gtk_binding_entry_add_signal (binding_set, GDK_0 + i, GDK_MOD1_MASK,
                                    "tab-change", 1, G_TYPE_UINT, i - 1);
    }
}



static inline gint
view_type2index (GType type)
{
  /* this necessary for platforms where sizeof(GType) != sizeof(gint),
   * see http://bugzilla.xfce.org/show_bug.cgi?id=2726 for details.
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
   * see http://bugzilla.xfce.org/show_bug.cgi?id=2726 for details.
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
  GtkRadioAction *radio_action;
  GtkAccelGroup  *accel_group;
  GtkWidget      *label;
  GtkWidget      *infobar;
  GtkWidget      *item;
  GtkAction      *action;
  gboolean        last_show_hidden;
  gboolean        last_menubar_visible;
  GSList         *group;
  gchar          *last_location_bar;
  gchar          *last_side_pane;
  GType           type;
  gint            last_separator_position;
  gint            last_window_width;
  gint            last_window_height;
  gboolean        last_window_maximized;
  gboolean        last_statusbar_visible;
  GtkRcStyle     *style;

  /* unset the view type */
  window->view_type = G_TYPE_NONE;

  /* grab a reference on the provider factory */
  window->provider_factory = thunarx_provider_factory_get_default ();

  /* grab a reference on the preferences */
  window->preferences = thunar_preferences_get ();

  /* get all properties for init */
  g_object_get (G_OBJECT (window->preferences),
                "last-show-hidden", &last_show_hidden,
                "last-window-width", &last_window_width,
                "last-window-height", &last_window_height,
                "last-window-maximized", &last_window_maximized,
                "last-menubar-visible", &last_menubar_visible,
                "last-separator-position", &last_separator_position,
                "last-location-bar", &last_location_bar,
                "last-side-pane", &last_side_pane,
                "last-statusbar-visible", &last_statusbar_visible,
                NULL);

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

  /* setup the action group for this window */
  window->action_group = gtk_action_group_new ("ThunarWindow");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* initialize the "show-hidden" action using the last value from the preferences */
  action = gtk_action_group_get_action (window->action_group, "show-hidden");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), last_show_hidden);

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

  /* setup the launcher support */
  window->launcher = thunar_launcher_new ();
  thunar_launcher_set_widget (window->launcher, GTK_WIDGET (window));
  thunar_component_set_ui_manager (THUNAR_COMPONENT (window->launcher), window->ui_manager);
  exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->launcher), "current-directory");
  g_signal_connect_swapped (G_OBJECT (window->launcher), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  g_signal_connect_swapped (G_OBJECT (window->launcher), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);

  /* determine the default window size from the preferences */
  gtk_window_set_default_size (GTK_WINDOW (window), last_window_width, last_window_height);

  /* restore the maxized state of the window */
  if (G_UNLIKELY (last_window_maximized))
    gtk_window_maximize (GTK_WINDOW (window));

  window->table = gtk_table_new (6, 1, FALSE);
  gtk_container_add (GTK_CONTAINER (window), window->table);
  gtk_widget_show (window->table);

  window->menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_table_attach (GTK_TABLE (window->table), window->menubar, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  /* update menubar visibiliy */
  action = gtk_action_group_get_action (window->action_group, "view-menubar");
  g_signal_connect (G_OBJECT (window->menubar), "deactivate", G_CALLBACK (thunar_window_toggle_menubar_deactivate), window);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), last_menubar_visible);

  /* append the menu item for the spinner */
  item = gtk_menu_item_new ();
  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
  gtk_menu_item_set_right_justified (GTK_MENU_ITEM (item), TRUE);
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
      gtk_table_attach (GTK_TABLE (window->table), infobar, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (infobar);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning, you are using the root account, you may harm your system."));
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), label);
      gtk_widget_show (label);
    }

  window->paned = gtk_hpaned_new ();
  gtk_container_set_border_width (GTK_CONTAINER (window->paned), 0);
  gtk_table_attach (GTK_TABLE (window->table), window->paned, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (window->paned);

  /* determine the last separator position and apply it to the paned view */
  gtk_paned_set_position (GTK_PANED (window->paned), last_separator_position);
  g_signal_connect_swapped (window->paned, "accept-position", G_CALLBACK (thunar_window_save_paned), window);
  g_signal_connect_swapped (window->paned, "button-release-event", G_CALLBACK (thunar_window_save_paned), window);

  window->view_box = gtk_table_new (3, 1, FALSE);
  gtk_paned_pack2 (GTK_PANED (window->paned), window->view_box, TRUE, FALSE);
  gtk_widget_show (window->view_box);

  /* tabs */
  window->notebook = gtk_notebook_new ();
  gtk_table_attach (GTK_TABLE (window->view_box), window->notebook, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  g_signal_connect (G_OBJECT (window->notebook), "switch-page", G_CALLBACK (thunar_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added", G_CALLBACK (thunar_window_notebook_page_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed", G_CALLBACK (thunar_window_notebook_page_removed), window);
  g_signal_connect_after (G_OBJECT (window->notebook), "button-press-event", G_CALLBACK (thunar_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "popup-menu", G_CALLBACK (thunar_window_notebook_popup_menu), window);
  g_signal_connect (G_OBJECT (window->notebook), "create-window", G_CALLBACK (thunar_window_notebook_create_window), window);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (window->notebook), FALSE);
  gtk_notebook_set_homogeneous_tabs (GTK_NOTEBOOK (window->notebook), TRUE);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (window->notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (window->notebook), 0);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), "thunar-tabs");
  gtk_widget_show (window->notebook);

  /* drop the notebook borders */
  style = gtk_rc_style_new ();
  style->xthickness = style->ythickness = 0;
  gtk_widget_modify_style (window->notebook, style);
  g_object_unref (G_OBJECT (style));

  /* determine the selected location selector */
  if (exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)))
    type = THUNAR_TYPE_LOCATION_BUTTONS;
  else if (exo_str_is_equal (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)))
    type = THUNAR_TYPE_LOCATION_ENTRY;
  else
    type = G_TYPE_NONE;
  g_free (last_location_bar);

  /* activate the selected location selector */
  action = gtk_action_group_get_action (window->action_group, "view-location-selector-pathbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (type == THUNAR_TYPE_LOCATION_BUTTONS));
  action = gtk_action_group_get_action (window->action_group, "view-location-selector-toolbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (type == THUNAR_TYPE_LOCATION_ENTRY));

  /* determine the selected side pane (FIXME: Should probably be last-shortcuts-visible and last-tree-visible preferences) */
  if (exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_SHORTCUTS_PANE)))
    type = THUNAR_TYPE_SHORTCUTS_PANE;
  else if (exo_str_is_equal (last_side_pane, g_type_name (THUNAR_TYPE_TREE_PANE)))
    type = THUNAR_TYPE_TREE_PANE;
  else
    type = G_TYPE_NONE;
  g_free (last_side_pane);

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
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_view_changed), window);

  /* schedule asynchronous menu action merging */
  window->merge_idle_id = g_idle_add_full (G_PRIORITY_LOW + 20, thunar_window_merge_idle, window, thunar_window_merge_idle_destroy);
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
      gtk_ui_manager_remove_ui (window->ui_manager, window->custom_preferences_merge_id);
      window->custom_preferences_merge_id = 0;
    }

  /* un-merge the go menu actions */
  if (G_LIKELY (window->go_items_actions_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->go_items_actions_merge_id);
      window->go_items_actions_merge_id = 0;
    }

  /* un-merge the bookmark actions */
  if (G_LIKELY (window->bookmark_items_actions_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->bookmark_items_actions_merge_id);
      window->bookmark_items_actions_merge_id = 0;
    }

  if (window->bookmark_reload_idle_id != 0)
    {
      g_source_remove (window->bookmark_reload_idle_id);
      window->bookmark_reload_idle_id = 0;
    }

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

  (*G_OBJECT_CLASS (thunar_window_parent_class)->finalize) (object);
}



static void
thunar_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarWindow *window = THUNAR_WINDOW (object);
  GtkAction    *action;

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_window_get_current_directory (window));
      break;

    case PROP_SHOW_HIDDEN:
      action = gtk_action_group_get_action (window->action_group, "show-hidden");
      g_value_set_boolean (value, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
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



static gboolean
thunar_window_toggle_sidepane (ThunarWindow *window)
{
  GtkAction *action;
  gchar     *type_name;

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if a side pane is currently active */
  if (G_LIKELY (window->sidepane != NULL))
    {
      /* determine the currently active side pane type */
      window->toggle_sidepane_type = G_OBJECT_TYPE (window->sidepane);

      /* just reset both side pane actions */
      action = gtk_action_group_get_action (window->action_group, "view-side-pane-shortcuts");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
      action = gtk_action_group_get_action (window->action_group, "view-side-pane-tree");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
    }
  else
    {
      /* check if we have a previously remembered toggle type */
      if (G_UNLIKELY (window->toggle_sidepane_type == G_TYPE_INVALID))
        {
          /* guess type based on the last-side-pane preference, default to shortcuts */
          g_object_get (G_OBJECT (window->preferences), "last-side-pane", &type_name, NULL);
          if (exo_str_is_equal (type_name, g_type_name (THUNAR_TYPE_TREE_PANE)))
            window->toggle_sidepane_type = THUNAR_TYPE_TREE_PANE;
          else
            window->toggle_sidepane_type = THUNAR_TYPE_SHORTCUTS_PANE;
          g_free (type_name);
        }

      /* activate the given side pane */
      action = gtk_action_group_get_action (window->action_group, "view-side-pane-shortcuts");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (window->toggle_sidepane_type == THUNAR_TYPE_SHORTCUTS_PANE));
      action = gtk_action_group_get_action (window->action_group, "view-side-pane-tree");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), (window->toggle_sidepane_type == THUNAR_TYPE_TREE_PANE));
    }

  return TRUE;
}



static gboolean
thunar_window_toggle_menubar (ThunarWindow *window)
{
  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  if (!gtk_widget_get_visible (window->menubar))
    {
      /* temporarily show menu bar */
      gtk_widget_show (window->menubar);
      return TRUE;
    }

  return FALSE;
}



static void
thunar_window_toggle_menubar_deactivate (GtkWidget    *menubar,
                                         ThunarWindow *window)
{
  GtkAction *action;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (window->menubar == menubar);

  /* this was a temporarily show, hide the bar */
  action = gtk_action_group_get_action (window->action_group, "view-menubar");
  if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    gtk_widget_hide (menubar);
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
  g_object_unref (G_OBJECT (window->clipboard));

  /* let the GtkWidget class unrealize the window */
  (*GTK_WIDGET_CLASS (thunar_window_parent_class)->unrealize) (widget);
}



static gboolean
thunar_window_configure_event (GtkWidget         *widget,
                               GdkEventConfigure *event)
{
  ThunarWindow *window = THUNAR_WINDOW (widget);

  /* check if we have a new dimension here */
  if (widget->allocation.width != event->width || widget->allocation.height != event->height)
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
  GtkAction  *action;
  GSList     *view_bindings;
  ThunarFile *current_directory;

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

  /* activate the selected view */
  action = gtk_action_group_get_action (window->action_group, "view-as-icons");
  g_signal_handlers_block_by_func (action, thunar_window_action_view_changed, window);
  gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), view_type2index (G_OBJECT_TYPE (page)));
  g_signal_handlers_unblock_by_func (action, thunar_window_action_view_changed, window);

  /* add stock bindings */
  thunar_window_binding_create (window, window, "current-directory", page, "current-directory", G_BINDING_DEFAULT);
  thunar_window_binding_create (window, window, "show-hidden", page, "show-hidden", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "loading", window->spinner, "active", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "selected-files", window->launcher, "selected-files", G_BINDING_SYNC_CREATE);
  thunar_window_binding_create (window, page, "zoom-level", window, "zoom-level", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  /* connect to the location bar (if any) */
  if (G_LIKELY (window->location_bar != NULL))
    {
      thunar_window_binding_create (window, page, "selected-files",
                                    window->location_bar, "selected-files",
                                    G_BINDING_SYNC_CREATE);
    }

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
  GtkAction *action;

  /* check if tabs should be visible */
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  if (n_pages < 2)
    {
      g_object_get (G_OBJECT (window->preferences),
                    "misc-always-show-tabs", &show_tabs, NULL);
    }

  /* update visibility */
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook), show_tabs);

  /* visibility of the detach action */
  action = gtk_action_group_get_action (window->action_group, "detach-tab");
  gtk_action_set_visible (action, n_pages > 1);
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
  g_signal_connect (G_OBJECT (page), "notify::selected-files", G_CALLBACK (thunar_window_update_custom_actions), window);
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



static void
thunar_window_notebook_popup_menu_real (ThunarWindow *window,
                                        guint32       timestamp,
                                        gint          button)
{
  GtkWidget *menu;

  /* run the menu on the view's screen (figuring out whether to use the file or the folder context menu) */
  menu = gtk_ui_manager_get_widget (window->ui_manager, "/tab-context-menu");
  thunar_gtk_menu_run (GTK_MENU (menu), GTK_WIDGET (window),
                       NULL, NULL, button, timestamp);
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
          thunar_window_notebook_popup_menu_real (window, event->time, event->button);
        }

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_notebook_popup_menu (GtkWidget    *notebook,
                                   ThunarWindow *window)
{
  thunar_window_notebook_popup_menu_real (window, gtk_get_current_event_time (), 0);
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
  gint               monitor_num;
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
  new_window = thunar_application_open_window (application, NULL, screen, NULL);
  g_object_unref (application);

  /* make sure the new window has the same size */
  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  gtk_window_resize (GTK_WINDOW (new_window), width, height);

  /* move the window to the drop position */
  if (x >= 0 && y >= 0)
    {
      /* get the monitor geometry */
      monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);
      gdk_screen_get_monitor_geometry (screen, monitor_num, &geo);

      /* calculate window position, but keep it on the current monitor */
      x = CLAMP (x - width / 2, geo.x, geo.x + geo.width - width);
      y = CLAMP (y - height / 2, geo.y, geo.y + geo.height - height);

      /* move the window */
      gtk_window_move (GTK_WINDOW (new_window), MAX (0, x), MAX (0, y));
    }

  /* insert page in new notebook */
  return THUNAR_WINDOW (new_window)->notebook;
}



static void
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
  GtkRcStyle     *style;

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
  view = g_object_new (window->view_type, "current-directory", directory, NULL);
  gtk_widget_show (view);

  /* use the history of the origin view if available */
  if (history != NULL)
    thunar_standard_view_set_history (THUNAR_STANDARD_VIEW (view), history);

  label_box = gtk_hbox_new (FALSE, 0);

  label = gtk_label_new (NULL);
  exo_binding_new (G_OBJECT (view), "display-name", G_OBJECT (label), "label");
  exo_binding_new (G_OBJECT (view), "tooltip-text", G_OBJECT (label), "tooltip-text");
  gtk_widget_set_has_tooltip (label, TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_misc_set_padding (GTK_MISC (label), 3, 3);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (label_box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (label_box), button, FALSE, FALSE, 0);
  gtk_widget_set_can_default (button, FALSE);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, _("Close tab"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (gtk_widget_destroy), view);
  gtk_widget_show (button);

  /* make button a bit smaller */
  style = gtk_rc_style_new ();
  style->xthickness = style->ythickness = 0;
  gtk_widget_modify_style (button, style);
  g_object_unref (G_OBJECT (style));

  icon = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
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
thunar_window_install_location_bar (ThunarWindow *window,
                                    GType         type)
{
  GtkToolItem *item;
  gboolean     small_icons;

  _thunar_return_if_fail (type == G_TYPE_NONE || g_type_is_a (type, THUNAR_TYPE_LOCATION_BAR));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* drop the previous location bar (if any) */
  if (G_UNLIKELY (window->location_bar != NULL))
    {
      /* check if it was toolbar'ed (and thereby need to be disconnected from the toolbar) */
      if (!thunar_location_bar_is_standalone (THUNAR_LOCATION_BAR (window->location_bar)))
        {
          /* disconnect the toolbar from the window */
          gtk_container_remove (GTK_CONTAINER (window->table), window->location_toolbar);
          window->location_toolbar = NULL;
        }

      /* destroy the location bar */
      gtk_widget_destroy (window->location_bar);
      window->location_bar = NULL;
    }

  /* check if we have a new location bar */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* allocate the new location bar widget */
      window->location_bar = g_object_new (type, "ui-manager", window->ui_manager, NULL);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->location_bar), "current-directory");
      g_signal_connect_swapped (G_OBJECT (window->location_bar), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      g_signal_connect_swapped (G_OBJECT (window->location_bar), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);

      /* connect the location widget to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        thunar_window_binding_create (window, window->view, "selected-files", window->location_bar, "selected-files", G_BINDING_SYNC_CREATE);

      /* check if the location bar should be placed into a toolbar */
      if (!thunar_location_bar_is_standalone (THUNAR_LOCATION_BAR (window->location_bar)))
        {
          /* setup the toolbar for the location bar */
          window->location_toolbar = gtk_ui_manager_get_widget (window->ui_manager, "/location-toolbar");
          g_object_get (G_OBJECT (window->preferences), "misc-small-toolbar-icons", &small_icons, NULL);
          gtk_toolbar_set_style (GTK_TOOLBAR (window->location_toolbar), GTK_TOOLBAR_ICONS);
          gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->location_toolbar),
                                     small_icons ? GTK_ICON_SIZE_SMALL_TOOLBAR : GTK_ICON_SIZE_LARGE_TOOLBAR);
          gtk_table_attach (GTK_TABLE (window->table), window->location_toolbar, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
          gtk_widget_show (window->location_toolbar);

          /* add the location bar tool item (destroyed with the location bar) */
          item = gtk_tool_item_new ();
          gtk_tool_item_set_expand (item, TRUE);
          g_signal_connect_object (G_OBJECT (window->location_bar), "destroy", G_CALLBACK (gtk_widget_destroy), item, G_CONNECT_SWAPPED);
          gtk_toolbar_insert (GTK_TOOLBAR (window->location_toolbar), item, -1);
          gtk_widget_show (GTK_WIDGET (item));

          /* add the location bar itself */
          gtk_container_add (GTK_CONTAINER (item), window->location_bar);
        }
      else
        {
          /* it's a standalone location bar, just place it above the view */
          gtk_table_attach (GTK_TABLE (window->view_box), window->location_bar, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);
        }

      /* display the new location bar widget */
      gtk_widget_show (window->location_bar);
    }

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-location-bar", g_type_name (type), NULL);
}



static void
thunar_window_install_sidepane (ThunarWindow *window,
                                GType         type)
{
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
      thunar_component_set_ui_manager (THUNAR_COMPONENT (window->sidepane), window->ui_manager);
      exo_binding_new (G_OBJECT (window), "show-hidden", G_OBJECT (window->sidepane), "show-hidden");
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->sidepane), "current-directory");
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      g_signal_connect_swapped (G_OBJECT (window->sidepane), "open-new-tab", G_CALLBACK (thunar_window_notebook_insert), window);
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
  GList *providers;
  GList *actions;
  GList *ap, *pp;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (window->custom_preferences_merge_id == 0);

  /* determine the available preferences providers */
  providers = thunarx_provider_factory_list_providers (window->provider_factory, THUNARX_TYPE_PREFERENCES_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
      /* allocate a new merge id from the UI manager */
      window->custom_preferences_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

      /* add actions from all providers */
      for (pp = providers; pp != NULL; pp = pp->next)
        {
          /* determine the available actions for the provider */
          actions = thunarx_preferences_provider_get_actions (THUNARX_PREFERENCES_PROVIDER (pp->data), GTK_WIDGET (window));
          for (ap = actions; ap != NULL; ap = ap->next)
            {
              /* add the action to the action group */
              gtk_action_group_add_action (window->action_group, GTK_ACTION (ap->data));

              /* add the action to the UI manager */
              gtk_ui_manager_add_ui (window->ui_manager,
                                     window->custom_preferences_merge_id,
                                     "/main-menu/edit-menu/placeholder-custom-preferences",
                                     gtk_action_get_name (GTK_ACTION (ap->data)),
                                     gtk_action_get_name (GTK_ACTION (ap->data)),
                                     GTK_UI_MANAGER_MENUITEM, FALSE);

              /* release the reference on the action */
              g_object_unref (G_OBJECT (ap->data));
            }

          /* release the reference on the provider */
          g_object_unref (G_OBJECT (pp->data));

          /* release the action list */
          g_list_free (actions);
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

          action = gtk_action_new (unique_name, name, tooltip, NULL);
          icon_name = thunar_file_get_icon_name (file, THUNAR_FILE_ICON_STATE_DEFAULT, icon_theme);
          gtk_action_set_icon_name (action, icon_name);
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

      action = gtk_action_new (unique_name, name, tooltip, NULL);
      gtk_action_set_icon_name (action, "folder-remote");
      g_object_set_data_full (G_OBJECT (action), I_("location-file"),
                              g_object_ref (file_path), g_object_unref);

      g_free (remote_name);

      /* add to the remote bookmarks */
      path = "/main-menu/go-menu/placeholder-go-remote-actions";
    }

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

  g_checksum_free (checksum);
  g_free (tooltip);
}



static gboolean
thunar_window_bookmark_merge (gpointer user_data)
{
  ThunarWindow *window = THUNAR_WINDOW (user_data);

  _thunar_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  GDK_THREADS_ENTER ();

  /* remove old actions */
  if (window->bookmark_items_actions_merge_id != 0)
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->bookmark_items_actions_merge_id);
      gtk_ui_manager_ensure_update (window->ui_manager);
    }

  /* drop old bookmarks action group */
  if (window->bookmark_action_group != NULL)
    {
      gtk_ui_manager_remove_action_group (window->ui_manager, window->bookmark_action_group);
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

  /* generate a new merge id */
  window->bookmark_items_actions_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

  /* create a new action group */
  window->bookmark_action_group = gtk_action_group_new ("ThunarBookmarks");
  gtk_ui_manager_insert_action_group (window->ui_manager, window->bookmark_action_group, -1);

  /* collect bookmarks */
  thunar_util_load_bookmarks (window->bookmark_file,
                              thunar_window_bookmark_merge_line,
                              window);

  window->bookmark_reload_idle_id = 0;

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_window_merge_go_actions (ThunarWindow *window)
{
  GtkAction *action;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (window->go_items_actions_merge_id == 0);

  /* setup the "open-trash" action */
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      /* allocate a new merge id from the UI manager */
      window->go_items_actions_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

      /* add the trash action to the action group */
      action = thunar_trash_action_new ();
      g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_window_action_open_trash), window);
      gtk_action_group_add_action_with_accel (window->action_group, action, NULL);

      /* add the action to the UI manager */
      gtk_ui_manager_add_ui (window->ui_manager,
                             window->go_items_actions_merge_id,
                             "/main-menu/go-menu/placeholder-go-items-actions",
                             gtk_action_get_name (GTK_ACTION (action)),
                             gtk_action_get_name (GTK_ACTION (action)),
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      g_object_unref (action);
    }

  /* setup visibility of the "open-network" action */
  action = gtk_action_group_get_action (window->action_group, "open-network");
  gtk_action_set_visible (action, thunar_g_vfs_is_uri_scheme_supported ("network"));
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
  ThunarFile *selected_file;
  GtkWidget  *dialog;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* bring up the "Open Location"-dialog if the window has no location bar or the location bar
   * in the window does not support text entry by the user.
   */
  if (window->location_bar == NULL || !thunar_location_bar_accept_focus (THUNAR_LOCATION_BAR (window->location_bar), initial_text))
    {
      /* allocate the "Open Location" dialog */
      dialog = thunar_location_dialog_new ();
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
      thunar_location_dialog_set_working_directory (THUNAR_LOCATION_DIALOG (dialog),
                                                    thunar_window_get_current_directory (window));
      thunar_location_dialog_set_selected_file (THUNAR_LOCATION_DIALOG (dialog),
                                                thunar_window_get_current_directory (window));

      /* setup the initial text (if any) */
      if (G_UNLIKELY (initial_text != NULL))
        {
          /* show, grab focus, set text and move cursor to the end */
          gtk_widget_show_now (dialog);
          gtk_widget_grab_focus (THUNAR_LOCATION_DIALOG (dialog)->entry);
          gtk_entry_set_text (GTK_ENTRY (THUNAR_LOCATION_DIALOG (dialog)->entry), initial_text);
          gtk_editable_set_position (GTK_EDITABLE (THUNAR_LOCATION_DIALOG (dialog)->entry), -1);
        }

      /* run the dialog */
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        {
          /* be sure to hide the location dialog first */
          gtk_widget_hide (dialog);

          /* check if we have a new directory or a file to launch */
          selected_file = thunar_location_dialog_get_selected_file (THUNAR_LOCATION_DIALOG (dialog));
          if (selected_file != NULL)
            {
              thunar_browser_poke_file (THUNAR_BROWSER (window), selected_file, window,
                                        thunar_window_poke_file_finish, NULL);
            }
        }

      /* destroy the dialog */
      gtk_widget_destroy (dialog);
    }
}



static void
thunar_window_action_open_new_tab (GtkAction    *action,
                                   ThunarWindow *window)
{
  /* insert new tab with current directory as default */
  thunar_window_notebook_insert (window, thunar_window_get_current_directory (window));
}



static void
thunar_window_action_open_new_window (GtkAction    *action,
                                      ThunarWindow *window)
{
  ThunarApplication *application;
  ThunarHistory     *history;
  ThunarWindow      *new_window;
  ThunarFile        *start_file;

  /* popup a new window */
  application = thunar_application_get ();
  new_window = THUNAR_WINDOW (thunar_application_open_window (application, window->current_directory,
                                                              gtk_widget_get_screen (GTK_WIDGET (window)), NULL));
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
thunar_window_action_empty_trash (GtkAction    *action,
                                  ThunarWindow *window)
{
  ThunarApplication *application;

  /* launch the operation */
  application = thunar_application_get ();
  thunar_application_empty_trash (application, GTK_WIDGET (window), NULL);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_window_action_detach_tab (GtkAction    *action,
                                 ThunarWindow *window)
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
thunar_window_action_close_all_windows (GtkAction    *action,
                                        ThunarWindow *window)
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
thunar_window_action_close_tab (GtkAction    *action,
                                ThunarWindow *window)
{
  if (window->view != NULL)
    gtk_widget_destroy (window->view);
}



static void
thunar_window_action_close_window (GtkAction    *action,
                                   ThunarWindow *window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}



static void
thunar_window_action_preferences (GtkAction    *action,
                                  ThunarWindow *window)
{
  GtkWidget         *dialog;
  ThunarApplication *application;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
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
thunar_window_action_reload (GtkAction    *action,
                             ThunarWindow *window)
{
  gboolean result;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* force the view to reload */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, TRUE, &result);

  /* update the location bar to show the current directory */
  if (window->location_bar != NULL)
    g_object_notify (G_OBJECT (window->location_bar), "current-directory");
}



static void
thunar_window_action_pathbar_changed (GtkToggleAction *action,
                                      ThunarWindow    *window)
{
  GtkAction *other_action;
  GType      type;

  _thunar_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the new type of location bar */
  type = gtk_toggle_action_get_active (action) ? THUNAR_TYPE_LOCATION_BUTTONS : G_TYPE_NONE;

  /* install the new location bar */
  thunar_window_install_location_bar (window, type);

  /* check if we actually installed anything */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* reset the state of the toolbar action (without firing the handler) */
      other_action = gtk_action_group_get_action (window->action_group, "view-location-selector-toolbar");
      g_signal_handlers_block_by_func (G_OBJECT (other_action), thunar_window_action_toolbar_changed, window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (other_action), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (other_action), thunar_window_action_toolbar_changed, window);
    }
}



static void
thunar_window_action_toolbar_changed (GtkToggleAction *action,
                                      ThunarWindow    *window)
{
  GtkAction *other_action;
  GType      type;

  _thunar_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the new type of location bar */
  type = gtk_toggle_action_get_active (action) ? THUNAR_TYPE_LOCATION_ENTRY : G_TYPE_NONE;

  /* install the new location bar */
  thunar_window_install_location_bar (window, type);

  /* check if we actually installed anything */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* reset the state of the pathbar action (without firing the handler) */
      other_action = gtk_action_group_get_action (window->action_group, "view-location-selector-pathbar");
      g_signal_handlers_block_by_func (G_OBJECT (other_action), thunar_window_action_pathbar_changed, window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (other_action), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (other_action), thunar_window_action_pathbar_changed, window);
    }
}



static void
thunar_window_action_shortcuts_changed (GtkToggleAction *action,
                                        ThunarWindow    *window)
{
  GtkAction *other_action;
  GType      type;

  /* determine the new type of side pane */
  type = gtk_toggle_action_get_active (action) ? THUNAR_TYPE_SHORTCUTS_PANE : G_TYPE_NONE;

  /* install the new sidepane */
  thunar_window_install_sidepane (window, type);

  /* check if we actually installed anything */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* reset the state of the tree pane action (without firing the handler) */
      other_action = gtk_action_group_get_action (window->action_group, "view-side-pane-tree");
      g_signal_handlers_block_by_func (G_OBJECT (other_action), thunar_window_action_tree_changed, window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (other_action), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (other_action), thunar_window_action_tree_changed, window);
    }
}



static void
thunar_window_action_tree_changed (GtkToggleAction *action,
                                   ThunarWindow    *window)
{
  GtkAction *other_action;
  GType      type;

  /* determine the new type of side pane */
  type = gtk_toggle_action_get_active (action) ? THUNAR_TYPE_TREE_PANE : G_TYPE_NONE;

  /* install the new sidepane */
  thunar_window_install_sidepane (window, type);

  /* check if we actually installed anything */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* reset the state of the shortcuts pane action (without firing the handler) */
      other_action = gtk_action_group_get_action (window->action_group, "view-side-pane-shortcuts");
      g_signal_handlers_block_by_func (G_OBJECT (other_action), thunar_window_action_shortcuts_changed, window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (other_action), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (other_action), thunar_window_action_shortcuts_changed, window);
    }
}



static void
thunar_window_action_statusbar_changed (GtkToggleAction *action,
                                        ThunarWindow    *window)
{
  gboolean active;

  _thunar_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the new state of the action */
  active = gtk_toggle_action_get_active (action);

  /* check if we should drop the statusbar */
  if (!active && window->statusbar != NULL)
    {
      /* just get rid of the statusbar */
      gtk_widget_destroy (window->statusbar);
      window->statusbar = NULL;
    }
  else if (active && window->statusbar == NULL)
    {
      /* setup a new statusbar */
      window->statusbar = thunar_statusbar_new ();
      gtk_table_attach (GTK_TABLE (window->view_box), window->statusbar, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (window->statusbar);

      /* connect to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        thunar_window_binding_create (window, window->view, "statusbar-text", window->statusbar, "text", G_BINDING_SYNC_CREATE);
    }

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-statusbar-visible", active, NULL);
}



static void
thunar_window_action_menubar_changed (GtkToggleAction *action,
                                      ThunarWindow    *window)
{
  gboolean active;

  _thunar_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the new state of the action */
  active = gtk_toggle_action_get_active (action);

  /* show or hide the bar */
  gtk_widget_set_visible (window->menubar, active);

  /* remember the setting */
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-menubar-visible", active, NULL);
}



static void
thunar_window_action_zoom_in (GtkAction    *action,
                              ThunarWindow *window)
{
  gboolean result;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* increase the zoom level */
  g_signal_emit (G_OBJECT (window), window_signals[ZOOM_IN], 0, &result);
}



static void
thunar_window_action_zoom_out (GtkAction    *action,
                               ThunarWindow *window)
{
  gboolean result;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* decrease the zoom level */
  g_signal_emit (G_OBJECT (window), window_signals[ZOOM_OUT], 0, &result);
}



static void
thunar_window_action_zoom_reset (GtkAction    *action,
                                 ThunarWindow *window)
{
  gboolean result;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* reset zoom level */
  g_signal_emit (G_OBJECT (window), window_signals[ZOOM_RESET], 0, &result);
}



static void
thunar_window_action_view_changed (GtkRadioAction *action,
                                   GtkRadioAction *current,
                                   ThunarWindow   *window)
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

      /* update the UI (else GtkUIManager will crash on merging) */
      gtk_ui_manager_ensure_update (window->ui_manager);
    }

  /* determine the new type of view */
  window->view_type = view_index2type (gtk_radio_action_get_current_value (action));

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
  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (window->view_type), NULL);

  /* release the file references */
  if (G_UNLIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (current_directory != NULL))
    g_object_unref (G_OBJECT (current_directory));
}



static void
thunar_window_action_go_up (GtkAction    *action,
                            ThunarWindow *window)
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
thunar_window_action_open_home (GtkAction    *action,
                                ThunarWindow *window)
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
thunar_window_open_user_folder (GtkAction      *action,
                                ThunarWindow   *window,
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
thunar_window_action_open_desktop (GtkAction     *action,
                                   ThunarWindow  *window)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  thunar_window_open_user_folder (action, window,
                                  G_USER_DIRECTORY_DESKTOP,
                                  "Desktop");
}



static void
thunar_window_action_open_templates (GtkAction    *action,
                                     ThunarWindow *window)
{
  GtkWidget     *dialog;
  GtkWidget     *button;
  GtkWidget     *label;
  GtkWidget     *image;
  GtkWidget     *hbox;
  GtkWidget     *vbox;
  gboolean       show_about_templates;
  gboolean       success;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  success = thunar_window_open_user_folder (action,window,
                                            G_USER_DIRECTORY_TEMPLATES,
                                            "Templates");

  /* check whether we should display the "About Templates" dialog */
  g_object_get (G_OBJECT (window->preferences),
                "misc-show-about-templates", &show_about_templates,
                NULL);

  if (G_UNLIKELY(show_about_templates && success))
    {
      /* display the "About Templates" dialog */
      dialog = gtk_dialog_new_with_buttons (_("About Templates"), GTK_WINDOW (window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_NO_SEPARATOR,
                                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                                            NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      hbox = gtk_hbox_new (FALSE, 6);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
      gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      vbox = gtk_vbox_new (FALSE, 18);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      label = gtk_label_new (_("All files in this folder will appear in the \"Create Document\" menu."));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
      gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big_bold ());
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      label = gtk_label_new (_("If you frequently create certain kinds "
                             " of documents, make a copy of one and put it in this "
                             "folder. Thunar will add an entry for this document in the"
                             " \"Create Document\" menu.\n\n"
                             "You can then select the entry from the \"Create Document\" "
                             "menu and a copy of the document will be created in the "
                             "directory you are viewing."));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_check_button_new_with_mnemonic (_("Do _not display this message again"));
      exo_mutual_binding_new_with_negation (G_OBJECT (window->preferences), "misc-show-about-templates", G_OBJECT (button), "active");
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }
}



static void
thunar_window_action_open_file_system (GtkAction    *action,
                                       ThunarWindow *window)
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
thunar_window_action_open_trash (GtkAction    *action,
                                 ThunarWindow *window)
{
  GFile      *trash_bin;
  ThunarFile *trash_bin_file;
  GError     *error = NULL;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
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
thunar_window_action_open_network (GtkAction    *action,
                                   ThunarWindow *window)
{
  ThunarFile *network_file;
  GError     *error = NULL;
  GFile      *network;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
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
thunar_window_action_open_bookmark (GtkAction    *action,
                                    ThunarWindow *window)
{
  ThunarFile *local_file;
  GFile      *remote_file;

  /* try to open the local file */
  local_file = g_object_get_data (G_OBJECT (action), I_("thunar-file"));
  if (local_file != NULL)
    {
      thunar_window_set_current_directory (window, local_file);
      return;
    }

  /* try to the remote file */
  remote_file = g_object_get_data (G_OBJECT (action), I_("location-file"));
  if (remote_file != NULL)
    {
      thunar_browser_poke_location (THUNAR_BROWSER (window), remote_file, window,
                                    thunar_window_poke_location_finish, NULL);
    }
}



static void
thunar_window_action_open_location (GtkAction    *action,
                                    ThunarWindow *window)
{
  /* just use the "start-open-location" callback */
  thunar_window_start_open_location (window, NULL);
}



static void
thunar_window_action_contents (GtkAction    *action,
                               ThunarWindow *window)
{
  /* display the documentation index */
  xfce_dialog_show_help (GTK_WINDOW (window), "thunar", NULL, NULL);
}



static void
thunar_window_action_about (GtkAction    *action,
                            ThunarWindow *window)
{
  /* just popup the about dialog */
  thunar_dialogs_show_about (GTK_WINDOW (window), PACKAGE_NAME,
                             _("Thunar is a fast and easy to use file manager\n"
                               "for the Xfce Desktop Environment."));
}



static void
thunar_window_action_show_hidden (GtkToggleAction *action,
                                  ThunarWindow    *window)
{
  _thunar_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* just emit the "notify" signal for the "show-hidden"
   * signal and the view will automatically sync its state.
   */
  g_object_notify (G_OBJECT (window), "show-hidden");

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    g_object_set (G_OBJECT (window->preferences), "last-show-hidden",
                  gtk_toggle_action_get_active (action), NULL);
}



static void
thunar_window_current_directory_changed (ThunarFile   *current_directory,
                                         ThunarWindow *window)
{
  GtkIconTheme *icon_theme;
  GtkAction    *action;
  const gchar  *icon_name;
  gchar        *title;
  gboolean      show_full_path;
  gchar        *parse_name = NULL;
  const gchar  *name;

  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (window->current_directory == current_directory);

  /* update the "Empty Trash" action */
  action = gtk_action_group_get_action (window->action_group, "empty-trash");
  gtk_action_set_sensitive (action, (thunar_file_get_item_count (current_directory) > 0));
  gtk_action_set_visible (action, (thunar_file_is_root (current_directory) && thunar_file_is_trashed (current_directory)));

  /* get name of directory or full path */
  g_object_get (G_OBJECT (window->preferences), "misc-full-path-in-title", &show_full_path, NULL);
  if (G_UNLIKELY (show_full_path))
    name = parse_name = g_file_get_parse_name (thunar_file_get_file (current_directory));
  else
    name = thunar_file_get_display_name (current_directory);

  /* set window title */
  title = g_strdup_printf ("%s - %s", name, _("File Manager"));
  gtk_window_set_title (GTK_WINDOW (window), title);
  g_free (title);
  g_free (parse_name);

  /* set window icon */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
  icon_name = thunar_file_get_icon_name (current_directory,
                                         THUNAR_FILE_ICON_STATE_DEFAULT,
                                         icon_theme);
  gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
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
      /* determine the action for the menu item */
      action = gtk_widget_get_action (menu_item);
      if (G_UNLIKELY (action == NULL))
        return;

      /* determine the tooltip from the action */
      tooltip = gtk_action_get_tooltip (action);
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
  ThunarFile *folder;
  GList      *selected_files;
  GList      *actions = NULL;
  GList      *lp;
  GList      *providers;
  GList      *tmp;

  _thunar_return_if_fail (THUNAR_IS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* leave if the signal is emitted from a non-active tab */
  if (!gtk_widget_get_realized (GTK_WIDGET (window))
      || window->view != GTK_WIDGET (view))
    return;

  /* load the menu provides from the provider factory */
  providers = thunarx_provider_factory_list_providers (window->provider_factory,
                                                       THUNARX_TYPE_MENU_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
      /* grab a reference to the current directory of the window */
      folder = thunar_window_get_current_directory (window);

      /* get a list of selected files */
      selected_files = thunar_component_get_selected_files (THUNAR_COMPONENT (view));

      /* load the actions offered by the menu providers */
      for (lp = providers; lp != NULL; lp = lp->next)
        {
          if (G_LIKELY (selected_files != NULL))
            {
              tmp = thunarx_menu_provider_get_file_actions (lp->data,
                                                            GTK_WIDGET (window),
                                                            selected_files);
            }
          else if (G_LIKELY (folder != NULL))
            {
              tmp = thunarx_menu_provider_get_folder_actions (lp->data,
                                                              GTK_WIDGET (window),
                                                              THUNARX_FILE_INFO (folder));
            }
          else
            {
              tmp = NULL;
            }

          actions = g_list_concat (actions, tmp);
          g_object_unref (G_OBJECT (lp->data));
        }
      g_list_free (providers);
    }

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
  if (G_LIKELY (actions != NULL))
    {
      /* allocate the action group and the merge id for the custom actions */
      window->custom_actions = gtk_action_group_new ("ThunarActions");
      window->custom_merge_id = gtk_ui_manager_new_merge_id (window->ui_manager);

      /* insert the new action group and make sure the UI manager gets updated */
      gtk_ui_manager_insert_action_group (window->ui_manager, window->custom_actions, 0);
      gtk_ui_manager_ensure_update (window->ui_manager);

      /* add the actions to the UI manager */
      for (lp = actions; lp != NULL; lp = lp->next)
        {
          /* add the action to the action group */
          gtk_action_group_add_action_with_accel (window->custom_actions,
                                                  GTK_ACTION (lp->data),
                                                  NULL);

          /* add to the file context menu */
          gtk_ui_manager_add_ui (window->ui_manager,
                                 window->custom_merge_id,
                                 "/main-menu/file-menu/placeholder-custom-actions",
                                 gtk_action_get_name (GTK_ACTION (lp->data)),
                                 gtk_action_get_name (GTK_ACTION (lp->data)),
                                 GTK_UI_MANAGER_MENUITEM, FALSE);

          /* release the reference on the action */
          g_object_unref (G_OBJECT (lp->data));
        }

      /* cleanup */
      g_list_free (actions);
    }
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
          cursor = gdk_cursor_new (GDK_WATCH);
          gdk_window_set_cursor (GTK_WIDGET (window)->window, cursor);
          gdk_cursor_unref (cursor);
        }
      else
        {
          gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);
        }
    }
}



static void
thunar_window_device_pre_unmount (ThunarDeviceMonitor *device_monitor,
                                  ThunarDevice        *device,
                                  GFile               *root_file,
                                  ThunarWindow        *window)
{
  GtkAction *action;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (window->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_FILE (root_file));
  _thunar_return_if_fail (THUNAR_IS_WINDOW (window));

  /* nothing to do if we don't have a current directory */
  if (G_UNLIKELY (window->current_directory == NULL))
    return;

  /* check if the file is the current directory or an ancestor of the current directory */
  if (thunar_file_is_gfile_ancestor (window->current_directory, root_file))
    {
      /* change to the home folder */
      action = gtk_action_group_get_action (window->action_group, "open-home");
      if (G_LIKELY (action != NULL))
        gtk_action_activate (action);
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
  GDK_THREADS_ENTER ();
  thunar_window_merge_custom_preferences (window);
  thunar_window_merge_go_actions (window);
  GDK_THREADS_LEAVE ();

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

  GDK_THREADS_ENTER ();

  /* check if we should remember the window geometry */
  g_object_get (G_OBJECT (window->preferences), "misc-remember-geometry", &remember_geometry, NULL);
  if (G_LIKELY (remember_geometry))
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (GTK_WIDGET (window)->window);

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

  GDK_THREADS_LEAVE ();

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

  /* update the "Zoom In" and "Zoom Out" actions */
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "zoom-in", (zoom_level < THUNAR_ZOOM_N_LEVELS - 1));
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "zoom-out", (zoom_level > 0));
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
  GType       type;
  GtkAction  *action;
  gchar      *type_name;

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

          /* determine the last selected view if the last selected view preference is not selected */
          if (g_type_is_a (type, G_TYPE_NONE))
            {
              g_object_get (G_OBJECT (window->preferences), "last-view", &type_name, NULL);
              type = g_type_from_name (type_name);
            }

          /* activate the selected view */
          action = gtk_action_group_get_action (window->action_group, "view-as-icons");
          g_signal_handlers_block_by_func (action, thunar_window_action_view_changed, window);
          gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), view_type2index (g_type_is_a (type, THUNAR_TYPE_VIEW) ? type : THUNAR_TYPE_ICON_VIEW));
          thunar_window_action_view_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);
          g_signal_handlers_unblock_by_func (action, thunar_window_action_view_changed, window);
        }

      /* update window icon and title */
      thunar_window_current_directory_changed (current_directory, window);

      /* grab the focus to the main view */
      if (G_LIKELY (window->view != NULL))
        gtk_widget_grab_focus (window->view);
    }

  /* enable the 'Open new window' action if we have a valid directory */
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "new-window", (current_directory != NULL));
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "new-tab", (current_directory != NULL));

  /* enable the 'Up' action if possible for the new directory */
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "open-parent", (current_directory != NULL
                                                && thunar_file_has_parent (current_directory)));

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
