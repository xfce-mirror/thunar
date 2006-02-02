/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-location-dialog.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-throbber.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-window-ui.h>



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
  RELOAD,
  ZOOM_IN,
  ZOOM_OUT,
  LAST_SIGNAL,
};



static void     thunar_window_class_init                  (ThunarWindowClass  *klass);
static void     thunar_window_init                        (ThunarWindow       *window);
static void     thunar_window_dispose                     (GObject            *object);
static void     thunar_window_finalize                    (GObject            *object);
static void     thunar_window_get_property                (GObject            *object,
                                                           guint               prop_id,
                                                           GValue             *value,
                                                           GParamSpec         *pspec);
static void     thunar_window_set_property                (GObject            *object,
                                                           guint               prop_id,
                                                           const GValue       *value,
                                                           GParamSpec         *pspec);
static gboolean thunar_window_reload                      (ThunarWindow       *window);
static gboolean thunar_window_zoom_in                     (ThunarWindow       *window);
static gboolean thunar_window_zoom_out                    (ThunarWindow       *window);
static void     thunar_window_realize                     (GtkWidget          *widget);
static void     thunar_window_unrealize                   (GtkWidget          *widget);
static gboolean thunar_window_configure_event             (GtkWidget          *widget,
                                                           GdkEventConfigure  *event);
static void     thunar_window_merge_custom_preferences    (ThunarWindow       *window);
static void     thunar_window_action_open_new_window      (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_close_all_windows    (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_close                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_preferences          (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_reload               (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_location_bar_changed (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_side_pane_changed    (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_statusbar_changed    (GtkToggleAction    *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_zoom_in              (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_zoom_out             (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_zoom_reset           (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_view_changed         (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_go_up                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_open_home            (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_open_templates       (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_open_location        (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_about                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_show_hidden          (GtkToggleAction    *action,
                                                           ThunarWindow       *window);
static void     thunar_window_current_directory_changed   (ThunarFile         *current_directory,
                                                           ThunarWindow       *window);
static void     thunar_window_connect_proxy               (GtkUIManager       *manager,
                                                           GtkAction          *action,
                                                           GtkWidget          *proxy,
                                                           ThunarWindow       *window);
static void     thunar_window_disconnect_proxy            (GtkUIManager       *manager,
                                                           GtkAction          *action,
                                                           GtkWidget          *proxy,
                                                           ThunarWindow       *window);
static void     thunar_window_menu_item_selected          (GtkWidget          *menu_item,
                                                           ThunarWindow       *window);
static void     thunar_window_menu_item_deselected        (GtkWidget          *menu_item,
                                                           ThunarWindow       *window);
static void     thunar_window_notify_loading              (ThunarView         *view,
                                                           GParamSpec         *pspec,
                                                           ThunarWindow       *window);
static gboolean thunar_window_merge_idle                  (gpointer            user_data);
static void     thunar_window_merge_idle_destroy          (gpointer            user_data);
static gboolean thunar_window_save_geometry_timer         (gpointer            user_data);
static void     thunar_window_save_geometry_timer_destroy (gpointer            user_data);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;

  /* internal action signals */
  gboolean (*reload)   (ThunarWindow *window);
  gboolean (*zoom_in)  (ThunarWindow *window);
  gboolean (*zoom_out) (ThunarWindow *window);
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  /* support for custom preferences actions */
  ThunarxProviderFactory *provider_factory;
  gint                    custom_preferences_merge_id;

  ThunarClipboardManager *clipboard;

  ThunarPreferences      *preferences;

  ThunarIconFactory      *icon_factory;

  GtkActionGroup         *action_group;
  GtkUIManager           *ui_manager;

  /* closures for the menu_item_selected()/menu_item_deselected() callbacks */
  GClosure               *menu_item_selected_closure;
  GClosure               *menu_item_deselected_closure;

  GtkWidget              *table;
  GtkWidget              *throbber;
  GtkWidget              *paned;
  GtkWidget              *view_container;
  GtkWidget              *view;
  GtkWidget              *statusbar;

  /* support for two different styles of location bars (standalone/toolbar'ed) */
  GtkWidget              *location_standalone_box;
  GtkWidget              *location_toolbar_box;
  GtkWidget              *location_bar;

  ThunarHistory          *history;

  ThunarFile             *current_directory;

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* menu merge idle source */
  gint                    merge_idle_id;

  /* support to remember window geometry */
  gint                    save_geometry_timer_id;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "open-new-window", NULL, N_ ("Open New _Window"), "<control>N", N_ ("Open a new Thunar window for the displayed location"), G_CALLBACK (thunar_window_action_open_new_window), },
  { "close-all-windows", NULL, N_ ("Close _All Windows"), "<control><shift>W", N_ ("Close all Thunar windows"), G_CALLBACK (thunar_window_action_close_all_windows), },
  { "close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close this window"), G_CALLBACK (thunar_window_action_close), },
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "preferences", GTK_STOCK_PREFERENCES, N_ ("Pr_eferences..."), NULL, N_ ("Edit Thunars Preferences"), G_CALLBACK (thunar_window_action_preferences), },
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "reload", GTK_STOCK_REFRESH, N_ ("_Reload"), "<control>R", N_ ("Reload the current folder"), G_CALLBACK (thunar_window_action_reload), },
  { "view-location-selector-menu", NULL, N_ ("_Location Selector"), NULL, },
  { "view-side-pane-menu", NULL, N_ ("_Side Pane"), NULL, },
  { "zoom-in", GTK_STOCK_ZOOM_IN, N_ ("Zoom _In"), "<control>plus", N_ ("Show the contents in more detail"), G_CALLBACK (thunar_window_action_zoom_in), },
  { "zoom-out", GTK_STOCK_ZOOM_OUT, N_ ("Zoom _Out"), "<control>minus", N_ ("Show the contents in less detail"), G_CALLBACK (thunar_window_action_zoom_out), },
  { "zoom-reset", GTK_STOCK_ZOOM_100, N_ ("Normal Si_ze"), "<control>0", N_ ("Show the contents at the normal size"), G_CALLBACK (thunar_window_action_zoom_reset), },
  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "open-parent", GTK_STOCK_GO_UP, N_ ("Open _Parent"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (thunar_window_action_go_up), },
  { "open-home", GTK_STOCK_HOME, N_ ("_Home"), "<alt>Home", N_ ("Go to the home folder"), G_CALLBACK (thunar_window_action_open_home), },
  { "open-templates", THUNAR_STOCK_TEMPLATES, N_ ("T_emplates"), NULL, N_ ("Go to the templates folder"), G_CALLBACK (thunar_window_action_open_templates), },
  { "open-location", NULL, N_ ("Open _Location..."), "<control>L", N_ ("Specify a location to open"), G_CALLBACK (thunar_window_action_open_location), },
  { "help-menu", NULL, N_ ("_Help"), NULL, },
  { "about", GTK_STOCK_ABOUT, N_ ("_About"), NULL, N_ ("Display information about Thunar"), G_CALLBACK (thunar_window_action_about), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "show-hidden", NULL, N_ ("Show _Hidden Files"), "<control>H", N_ ("Toggles the display of hidden files in the current window"), G_CALLBACK (thunar_window_action_show_hidden), FALSE, },
  { "view-statusbar", NULL, N_ ("St_atusbar"), NULL, N_ ("Change the visibility of this window's statusbar"), G_CALLBACK (thunar_window_action_statusbar_changed), FALSE, },
};



static GObjectClass *thunar_window_parent_class;
static guint         window_signals[LAST_SIGNAL];



GType
thunar_window_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarWindowClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_window_class_init,
        NULL,
        NULL,
        sizeof (ThunarWindow),
        0,
        (GInstanceInitFunc) thunar_window_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_WINDOW, I_("ThunarWindow"), &info, 0);
    }

  return type;
}



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_window_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_window_dispose;
  gobject_class->finalize = thunar_window_finalize;
  gobject_class->get_property = thunar_window_get_property;
  gobject_class->set_property = thunar_window_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_window_realize;
  gtkwidget_class->unrealize = thunar_window_unrealize;
  gtkwidget_class->configure_event = thunar_window_configure_event;

  klass->reload = thunar_window_reload;
  klass->zoom_in = thunar_window_zoom_in;
  klass->zoom_out = thunar_window_zoom_out;

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

  /* setup the key bindings for the windows */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_F5, 0, "reload", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, GDK_CONTROL_MASK, "zoom-in", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, GDK_CONTROL_MASK, "zoom-out", 0);
}



static void
thunar_window_init (ThunarWindow *window)
{
  GtkRadioAction *radio_action;
  GtkAccelGroup  *accel_group;
  GtkAction      *action;
  GtkWidget      *menubar;
  GtkWidget      *vbox;
  GtkWidget      *item;
  gboolean        show_hidden;
  gboolean        statusbar_visible;
  GSList         *group;
  gchar          *type_name;
  GType           type;
  gint            width;
  gint            height;

  /* grab a reference on the provider factory */
  window->provider_factory = thunarx_provider_factory_get_default ();

  /* grab a reference on the preferences */
  window->preferences = thunar_preferences_get ();

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
  window->action_group = gtk_action_group_new ("thunar-window");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
  gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));

  /* initialize the "show-hidden" action using the last value from the preferences */
  action = gtk_action_group_get_action (window->action_group, "show-hidden");
  g_object_get (G_OBJECT (window->preferences), "last-show-hidden", &show_hidden, NULL);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_hidden);

  /* synchronize the "show-hidden" state with the "last-show-hidden" preference */
  exo_binding_new (G_OBJECT (window), "show-hidden", G_OBJECT (window->preferences), "last-show-hidden");

  /* setup the history support */
  window->history = g_object_new (THUNAR_TYPE_HISTORY, "action-group", window->action_group, NULL);
  g_signal_connect_swapped (G_OBJECT (window->history), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
  exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->history), "current-directory");

  /*
   * add the side pane options
   */
  radio_action = gtk_radio_action_new ("view-side-pane-shortcuts", _("_Shortcuts"), NULL, NULL, THUNAR_TYPE_SHORTCUTS_PANE);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-side-pane-hidden", _("_Hidden"), NULL, NULL, G_TYPE_NONE);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  /*
   * add the location selector options
   */
  radio_action = gtk_radio_action_new ("view-location-selector-pathbar", _("_Pathbar Style"),
                                       _("Modern approach with buttons that correspond to folders"), NULL, THUNAR_TYPE_LOCATION_BUTTONS);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-location-selector-toolbar", _("_Toolbar Style"),
                                       _("Traditional approach with location bar and navigation buttons"), NULL, THUNAR_TYPE_LOCATION_ENTRY);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-location-selector-hidden", _("_Hidden"), _("Don't display any location selector"), NULL, G_TYPE_NONE);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  /*
   * add view options
   */
  radio_action = gtk_radio_action_new ("view-as-icons", _("View as _Icons"), _("Display folder content in an icon view"),
                                       NULL, THUNAR_TYPE_ICON_VIEW);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-as-detailed-list", _("View as _Detailed List"), _("Display folder content in a detailed list view"),
                                       NULL, THUNAR_TYPE_DETAILS_VIEW);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
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

  /* determine the default window size from the preferences */
  g_object_get (G_OBJECT (window->preferences), "last-window-width", &width, "last-window-height", &height, NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);

  window->table = gtk_table_new (4, 1, FALSE);
  gtk_container_add (GTK_CONTAINER (window), window->table);
  gtk_widget_show (window->table);

  menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_table_attach (GTK_TABLE (window->table), menubar, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (menubar);

  /* append the menu item for the throbber */
  item = gtk_menu_item_new ();
  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
  gtk_menu_item_set_right_justified (GTK_MENU_ITEM (item), TRUE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
  gtk_widget_show (item);

  /* place the throbber into the menu item */
  window->throbber = thunar_throbber_new ();
  gtk_container_add (GTK_CONTAINER (item), window->throbber);
  gtk_widget_show (window->throbber);

  window->location_toolbar_box = gtk_alignment_new (0.0f, 0.5f, 1.0f, 1.0f);
  gtk_table_attach (GTK_TABLE (window->table), window->location_toolbar_box, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (window->location_toolbar_box);

  window->paned = g_object_new (GTK_TYPE_HPANED, "border-width", 6, NULL);
  gtk_table_attach (GTK_TABLE (window->table), window->paned, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (window->paned);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_paned_pack2 (GTK_PANED (window->paned), vbox, TRUE, FALSE);
  gtk_widget_show (vbox);

  window->location_standalone_box = gtk_alignment_new (0.0f, 0.5f, 1.0f, 1.0f);
  gtk_box_pack_start (GTK_BOX (vbox), window->location_standalone_box, FALSE, FALSE, 0);
  gtk_widget_show (window->location_standalone_box);

  window->view_container = gtk_alignment_new (0.5f, 0.5f, 1.0f, 1.0f);
  gtk_box_pack_start (GTK_BOX (vbox), window->view_container, TRUE, TRUE, 0);
  gtk_widget_show (window->view_container);

  /* determine the selected location bar */
  g_object_get (G_OBJECT (window->preferences), "last-location-bar", &type_name, NULL);
  type = g_type_from_name (type_name);
  g_free (type_name);

  /* activate the selected location bar */
  action = gtk_action_group_get_action (window->action_group, "view-location-selector-pathbar");
  exo_gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), g_type_is_a (type, THUNAR_TYPE_LOCATION_BAR) ? type : G_TYPE_NONE);
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_location_bar_changed), window);
  thunar_window_action_location_bar_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);

  /* determine the selected side pane */
  g_object_get (G_OBJECT (window->preferences), "last-side-pane", &type_name, NULL);
  type = g_type_from_name (type_name);
  g_free (type_name);

  /* activate the selected side pane */
  action = gtk_action_group_get_action (window->action_group, "view-side-pane-shortcuts");
  exo_gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), g_type_is_a (type, THUNAR_TYPE_SIDE_PANE) ? type : G_TYPE_NONE);
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_side_pane_changed), window);
  thunar_window_action_side_pane_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);

  /* determine the default view */
  g_object_get (G_OBJECT (window->preferences), "default-view", &type_name, NULL);
  type = g_type_from_name (type_name);
  g_free (type_name);

  /* check if we should display the statusbar by default */
  g_object_get (G_OBJECT (window->preferences), "last-statusbar-visible", &statusbar_visible, NULL);
  action = gtk_action_group_get_action (window->action_group, "view-statusbar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), statusbar_visible);

  /* determine the last selected view if we don't have a valid default */
  if (!g_type_is_a (type, THUNAR_TYPE_VIEW))
    {
      g_object_get (G_OBJECT (window->preferences), "last-view", &type_name, NULL);
      type = g_type_from_name (type_name);
      g_free (type_name);
    }

  /* activate the selected view */
  action = gtk_action_group_get_action (window->action_group, "view-as-icons");
  exo_gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), g_type_is_a (type, THUNAR_TYPE_VIEW) ? type : THUNAR_TYPE_ICON_VIEW);
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_view_changed), window);
  thunar_window_action_view_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);

  /* schedule asynchronous menu action merging */
  window->merge_idle_id = g_idle_add_full (G_PRIORITY_LOW + 20, thunar_window_merge_idle, window, thunar_window_merge_idle_destroy);
}



static void
thunar_window_dispose (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  /* destroy the save geometry timer source */
  if (G_UNLIKELY (window->save_geometry_timer_id > 0))
    g_source_remove (window->save_geometry_timer_id);

  /* destroy the merge idle source */
  if (G_UNLIKELY (window->merge_idle_id > 0))
    g_source_remove (window->merge_idle_id);

  /* un-merge the custom preferences */
  if (G_LIKELY (window->custom_preferences_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (window->ui_manager, window->custom_preferences_merge_id);
      window->custom_preferences_merge_id = 0;
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

  /* disconnect from the ui manager */
  g_signal_handlers_disconnect_matched (G_OBJECT (window->ui_manager), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
  g_object_unref (G_OBJECT (window->ui_manager));

  g_object_unref (G_OBJECT (window->action_group));
  g_object_unref (G_OBJECT (window->icon_factory));
  g_object_unref (G_OBJECT (window->history));

  /* release our reference on the provider factory */
  g_object_unref (G_OBJECT (window->provider_factory));

  /* release the preferences reference */
  g_object_unref (G_OBJECT (window->preferences));

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
thunar_window_reload (ThunarWindow *window)
{
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* force the view to reload */
  if (G_LIKELY (window->view != NULL))
    {
      thunar_view_reload (THUNAR_VIEW (window->view));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_window_zoom_in (ThunarWindow *window)
{
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

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
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), FALSE);

  /* check if we can still zoom out */
  if (G_LIKELY (window->zoom_level > 0))
    {
      thunar_window_set_zoom_level (window, window->zoom_level - 1);
      return TRUE;
    }

  return FALSE;
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
      if (window->save_geometry_timer_id > 0)
        g_source_remove (window->save_geometry_timer_id);

      /* check if we should schedule another save timer */
      if (GTK_WIDGET_VISIBLE (widget))
        {
          /* save the geometry one second after the last configure event */
          window->save_geometry_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, thunar_window_save_geometry_timer,
                                                               window, thunar_window_save_geometry_timer_destroy);
        }
    }

  /* let Gtk+ handle the configure event */
  return (*GTK_WIDGET_CLASS (thunar_window_parent_class)->configure_event) (widget, event);
}



static void
thunar_window_merge_custom_preferences (ThunarWindow *window)
{
  GList *providers;
  GList *actions;
  GList *ap, *pp;

  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (window->custom_preferences_merge_id == 0);

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
thunar_window_action_open_new_window (GtkAction    *action,
                                      ThunarWindow *window)
{
  ThunarApplication *application;

  application = thunar_application_get ();
  thunar_application_open_window (application, window->current_directory,
                                  gtk_widget_get_screen (GTK_WIDGET (window)));
  g_object_unref (G_OBJECT (application));
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
  g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
  g_list_free (windows);
}



static void
thunar_window_action_close (GtkAction    *action,
                            ThunarWindow *window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}



static void
thunar_window_action_preferences (GtkAction    *action,
                                  ThunarWindow *window)
{
  GtkWidget *dialog;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* allocate and display a preferences dialog */
  dialog = thunar_preferences_dialog_new (GTK_WINDOW (window));
  gtk_widget_show (dialog);
}



static void
thunar_window_action_reload (GtkAction    *action,
                             ThunarWindow *window)
{
  gboolean result;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* force the view to reload */
  g_signal_emit (G_OBJECT (window), window_signals[RELOAD], 0, &result);
}



static void
thunar_window_action_location_bar_changed (GtkRadioAction *action,
                                           GtkRadioAction *current,
                                           ThunarWindow   *window)
{
  GtkToolItem *toolitem;
  GtkWidget   *toolbar;
  GType        type;

  /* drop the previous location bar (if any) */
  if (G_UNLIKELY (window->location_bar != NULL))
    {
      gtk_widget_destroy (window->location_bar);
      window->location_bar = NULL;
    }

  /* hide the location bar containers */
  gtk_widget_hide (window->location_standalone_box);
  gtk_widget_hide (window->location_toolbar_box);

  /* determine the new type of location bar */
  type = gtk_radio_action_get_current_value (action);

  /* activate the new location bar */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* initialize the new location bar widget */
      window->location_bar = g_object_new (type, NULL);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->location_bar), "current-directory");
      g_signal_connect_swapped (G_OBJECT (window->location_bar), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);

      /* check if the location bar should be placed into a toolbar */
      if (!thunar_location_bar_is_standalone (THUNAR_LOCATION_BAR (window->location_bar)))
        {
          /* be sure to drop any previous toolbar */
          toolbar = gtk_bin_get_child (GTK_BIN (window->location_toolbar_box));
          if (G_LIKELY (toolbar != NULL))
            gtk_widget_destroy (toolbar);

          /* allocate the new toolbar */
          toolbar = gtk_toolbar_new ();
          gtk_container_add (GTK_CONTAINER (window->location_toolbar_box), toolbar);
          gtk_widget_show (toolbar);

          /* add the "back" action */
          toolitem = thunar_gtk_action_group_create_tool_item (window->action_group, "back");
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add the "forward" action */
          toolitem = thunar_gtk_action_group_create_tool_item (window->action_group, "forward");
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add the "open-parent" action */
          toolitem = thunar_gtk_action_group_create_tool_item (window->action_group, "open-parent");
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add a separator */
          toolitem = gtk_separator_tool_item_new ();
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add the "open-home" action */
          toolitem = thunar_gtk_action_group_create_tool_item (window->action_group, "open-home");
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add a separator */
          toolitem = gtk_separator_tool_item_new ();
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* add the toolitem with the location bar */
          toolitem = gtk_tool_item_new ();
          gtk_tool_item_set_expand (toolitem, TRUE);
          gtk_container_add (GTK_CONTAINER (toolitem), window->location_bar);
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
          gtk_widget_show (GTK_WIDGET (toolitem));

          /* make the location toolbar container visible */
          gtk_widget_show (window->location_toolbar_box);
        }
      else
        {
          /* it's a standalone location bar, just place it in the standalone container */
          gtk_container_add (GTK_CONTAINER (window->location_standalone_box), window->location_bar);
          gtk_widget_show (window->location_standalone_box);
        }

      /* display the new location bar widget */
      gtk_widget_show (window->location_bar);
    }

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "last-location-bar", g_type_name (type), NULL);
}



static void
thunar_window_action_side_pane_changed (GtkRadioAction *action,
                                        GtkRadioAction *current,
                                        ThunarWindow   *window)
{
  GtkWidget *widget;
  GType      type;

  /* drop the previous side pane (if any) */
  widget = gtk_paned_get_child1 (GTK_PANED (window->paned));
  if (G_LIKELY (widget != NULL))
    gtk_widget_destroy (widget);

  /* determine the new type of side pane */
  type = gtk_radio_action_get_current_value (action);
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      widget = g_object_new (type, NULL);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (widget), "current-directory");
      g_signal_connect_swapped (G_OBJECT (widget), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      gtk_paned_pack1 (GTK_PANED (window->paned), widget, FALSE, FALSE);
      gtk_widget_show (widget);
    }

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "last-side-pane", g_type_name (type), NULL);
}



static void
thunar_window_action_statusbar_changed (GtkToggleAction *action,
                                        ThunarWindow    *window)
{
  gboolean active;

  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

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
      gtk_table_attach (GTK_TABLE (window->table), window->statusbar, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (window->statusbar);

      /* connect to the view (if any) */
      if (G_LIKELY (window->view != NULL))
        exo_binding_new (G_OBJECT (window->view), "statusbar-text", G_OBJECT (window->statusbar), "text");
    }

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "last-statusbar-visible", active, NULL);
}



static void
thunar_window_action_zoom_in (GtkAction    *action,
                              ThunarWindow *window)
{
  gboolean result;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* increase the zoom level */
  g_signal_emit (G_OBJECT (window), window_signals[ZOOM_IN], 0, &result);
}



static void
thunar_window_action_zoom_out (GtkAction    *action,
                               ThunarWindow *window)
{
  gboolean result;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* decrease the zoom level */
  g_signal_emit (G_OBJECT (window), window_signals[ZOOM_OUT], 0, &result);
}



static void
thunar_window_action_zoom_reset (GtkAction    *action,
                                 ThunarWindow *window)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* tell the view to reset it's zoom level */
  if (G_LIKELY (window->view != NULL))
    thunar_view_reset_zoom_level (THUNAR_VIEW (window->view));
}



static void
thunar_window_action_view_changed (GtkRadioAction *action,
                                   GtkRadioAction *current,
                                   ThunarWindow   *window)
{
  GType type;

  /* drop the previous view (if any) */
  if (G_LIKELY (window->view != NULL))
    {
      /* destroy and disconnect the previous view */
      gtk_widget_destroy (window->view);

      /* update the UI (else GtkUIManager will crash on merging) */
      gtk_ui_manager_ensure_update (window->ui_manager);
    }

  /* determine the new type of view */
  type = gtk_radio_action_get_current_value (action);

  /* allocate a new view of the requested type */
  if (G_LIKELY (type != G_TYPE_NONE))
    {
      /* allocate and setup a new view */
      window->view = g_object_new (type, "ui-manager", window->ui_manager, NULL);
      g_signal_connect (G_OBJECT (window->view), "notify::loading", G_CALLBACK (thunar_window_notify_loading), window);
      g_signal_connect_swapped (G_OBJECT (window->view), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->view), "current-directory");
      exo_binding_new (G_OBJECT (window), "show-hidden", G_OBJECT (window->view), "show-hidden");
      exo_binding_new (G_OBJECT (window->view), "loading", G_OBJECT (window->throbber), "animated");
      exo_mutual_binding_new (G_OBJECT (window->view), "zoom-level", G_OBJECT (window), "zoom-level");
      gtk_container_add (GTK_CONTAINER (window->view_container), window->view);
      gtk_widget_grab_focus (window->view);
      gtk_widget_show (window->view);

      /* connect to the statusbar (if any) */
      if (G_LIKELY (window->statusbar != NULL))
        exo_binding_new (G_OBJECT (window->view), "statusbar-text", G_OBJECT (window->statusbar), "text");
    }
  else
    {
      /* this should not happen under normal conditions */
      window->view = NULL;
    }

  /* remember the setting */
  g_object_set (G_OBJECT (window->preferences), "last-view", g_type_name (type), NULL);
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
  ThunarVfsPath *home_path;
  ThunarFile    *home_file;
  GError        *error = NULL;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the path to the home directory */
  home_path = thunar_vfs_path_get_for_home ();

  /* determine the file for the home directory */
  home_file = thunar_file_get_for_path (home_path, &error);
  if (G_UNLIKELY (home_file == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open home directory"));
      g_error_free (error);
    }
  else
    {
      /* open the home folder */
      thunar_window_set_current_directory (window, home_file);
      g_object_unref (G_OBJECT (home_file));
    }

  /* release our reference on the home path */
  thunar_vfs_path_unref (home_path);
}



static void
thunar_window_action_open_templates (GtkAction    *action,
                                     ThunarWindow *window)
{
  ThunarVfsPath *home_path;
  ThunarVfsPath *templates_path;
  ThunarFile    *templates_file = NULL;
  GtkWidget     *dialog;
  GtkWidget     *button;
  GtkWidget     *label;
  GtkWidget     *image;
  GtkWidget     *hbox;
  GtkWidget     *vbox;
  gboolean       show_about_templates;
  GError        *error = NULL;
  gchar         *absolute_path;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* determine the path to the ~/Templates folder */
  home_path = thunar_vfs_path_get_for_home ();
  templates_path = thunar_vfs_path_relative (home_path, "Templates");
  thunar_vfs_path_unref (home_path);

  /* make sure that the ~/Templates folder exists */
  absolute_path = thunar_vfs_path_dup_string (templates_path);
  xfce_mkdirhier (absolute_path, 0755, &error);
  g_free (absolute_path);

  /* determine the file for the ~/Templates path */
  if (G_LIKELY (error == NULL))
    templates_file = thunar_file_get_for_path (templates_path, &error);

  /* open the ~/Templates folder */
  if (G_LIKELY (templates_file != NULL))
    {
      /* go to the ~/Templates folder */
      thunar_window_set_current_directory (window, templates_file);
      g_object_unref (G_OBJECT (templates_file));

      /* check whether we should display the "About Templates" dialog */
      g_object_get (G_OBJECT (window->preferences), "misc-show-about-templates", &show_about_templates, NULL);
      if (G_UNLIKELY (show_about_templates))
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

          label = gtk_label_new (_("If you frequently create certain kinds of documents, "
                                   "make a copy of one and put it in this folder. Thunar "
                                   "will add an entry for this document in the \"Create "
                                   "Document\" menu.\n\n"
                                   "You can then select the entry from the \"Create "
                                   "Document\" menu and a copy of the document will "
                                   "be created in the directory you are viewing."));
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

  /* display an error dialog if something went wrong */
  if (G_UNLIKELY (error != NULL))
    {
      thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to open templates folder"));
      g_error_free (error);
    }

  /* release our reference on the ~/Templates path */
  thunar_vfs_path_unref (templates_path);
}



static void
thunar_window_action_open_location (GtkAction    *action,
                                    ThunarWindow *window)
{
  ThunarFile *selected_file;
  GtkWidget  *dialog;
  GError     *error = NULL;

  /* bring up the "Open Location"-dialog if the window has no location bar or the location bar
   * in the window does not support text entry by the user.
   */
  if (window->location_bar == NULL || !thunar_location_bar_accept_focus (THUNAR_LOCATION_BAR (window->location_bar)))
    {
      dialog = thunar_location_dialog_new ();
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
      thunar_location_dialog_set_selected_file (THUNAR_LOCATION_DIALOG (dialog), thunar_window_get_current_directory (window));
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        {
          /* check if we have a new directory or a file to launch */
          selected_file = thunar_location_dialog_get_selected_file (THUNAR_LOCATION_DIALOG (dialog));
          if (thunar_file_is_directory (selected_file))
            {
              /* open the new directory */
              thunar_window_set_current_directory (window, selected_file);
            }
          else
            {
              /* be sure to hide the location dialog first */
              gtk_widget_hide (dialog);

              /* try to launch the selected file */
              if (!thunar_file_launch (selected_file, GTK_WIDGET (window), &error))
                {
                  thunar_dialogs_show_error (GTK_WIDGET (window), error, _("Failed to launch `%s'"), thunar_file_get_display_name (selected_file));
                  g_error_free (error);
                }
            }
        }
      gtk_widget_destroy (dialog);
    }
}



static void
thunar_window_action_about (GtkAction    *action,
                            ThunarWindow *window)
{
  static const gchar *authors[] =
  {
    "Benedikt Meurer <benny@xfce.org>",
    NULL,
  };

  static const gchar license[] =
    "This program is free software; you can redistribute it and/or modify it\n"
    "under the terms of the GNU General Public License as published by the Free\n"
    "Software Foundation; either version 2 of the License, or (at your option)\n"
    "any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful, but WITHOUT\n"
    "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or\n"
    "FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for\n"
    "more details.\n"
    "\n"
    "You should have received a copy of the GNU General Public License along with\n"
    "this program; if not, write to the Free Software Foundation, Inc., 59 Temple\n"
    "Place, Suite 330, Boston, MA  02111-1307  USA.\n";

  GdkPixbuf *logo;
  
  /* try to load the about logo */
  logo = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/Thunar/Thunar-about-logo.png", NULL);

  /* open the about dialog */
  gtk_about_dialog_set_email_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_about_dialog_set_url_hook (exo_url_about_dialog_hook, NULL, NULL);
  gtk_show_about_dialog (GTK_WINDOW (window),
                         "authors", authors,
                         "comments", _("Thunar is a fast and easy to use file manager\nfor the Xfce Desktop Environment."),
                         "copyright", "Copyright \302\251 2004-2005 Benedikt Meurer",
                         "license", license,
                         "logo", logo,
                         "name", PACKAGE_NAME,
                         "translator-credits", _("translator-credits"),
                         "version", PACKAGE_VERSION,
                         "website", "http://thunar.xfce.org/",
                         NULL);

  /* release the about logo (if any) */
  if (G_LIKELY (logo != NULL))
    g_object_unref (G_OBJECT (logo));
}



static void
thunar_window_action_show_hidden (GtkToggleAction *action,
                                  ThunarWindow    *window)
{
  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* just emit the "notify" signal for the "show-hidden"
   * signal and the view will automatically sync its state.
   */
  g_object_notify (G_OBJECT (window), "show-hidden");
}



static void
thunar_window_current_directory_changed (ThunarFile   *current_directory,
                                         ThunarWindow *window)
{
  GdkPixbuf *icon;

  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (THUNAR_IS_FILE (current_directory));
  g_return_if_fail (window->current_directory == current_directory);

  /* set window title and icon */
  icon = thunar_icon_factory_load_file_icon (window->icon_factory, current_directory, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
  gtk_window_set_title (GTK_WINDOW (window), thunar_file_get_display_name (current_directory));
  gtk_window_set_icon (GTK_WINDOW (window), icon);
  g_object_unref (G_OBJECT (icon));
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
  GtkAction *action;
  gchar     *tooltip;
  gint       id;

  /* we can only display tooltips if we have a statusbar */
  if (G_LIKELY (window->statusbar != NULL))
    {
      /* determine the action for the menu item */
      action = g_object_get_data (G_OBJECT (menu_item), I_("gtk-action"));
      if (G_UNLIKELY (action == NULL))
        return;

      /* determine the tooltip from the action */
      g_object_get (G_OBJECT (action), "tooltip", &tooltip, NULL);
      if (G_LIKELY (tooltip != NULL))
        {
          id = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->statusbar), "Menu tooltip");
          gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), id, tooltip);
          g_free (tooltip);
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
thunar_window_notify_loading (ThunarView   *view,
                              GParamSpec   *pspec,
                              ThunarWindow *window)
{
  GdkCursor *cursor;

  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (THUNAR_VIEW (window->view) == view);

  if (GTK_WIDGET_REALIZED (window))
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
  


static gboolean
thunar_window_merge_idle (gpointer user_data)
{
  ThunarWindow *window = THUNAR_WINDOW (user_data);

  /* merge custom preferences from the providers */
  GDK_THREADS_ENTER ();
  thunar_window_merge_custom_preferences (window);
  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_window_merge_idle_destroy (gpointer user_data)
{
  THUNAR_WINDOW (user_data)->merge_idle_id = 0;
}



static gboolean
thunar_window_save_geometry_timer (gpointer user_data)
{
  GdkWindowState state;
  ThunarWindow  *window = THUNAR_WINDOW (user_data);
  gint           width;
  gint           height;

  GDK_THREADS_ENTER ();

  /* check if the window is still visible */
  if (GTK_WIDGET_VISIBLE (window))
    {
      /* determine the current state of the window */
      state = gdk_window_get_state (GTK_WIDGET (window)->window);

      /* don't save geometry for maximized or fullscreen windows */
      if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
        {
          /* determine the current width/height of the window... */
          gtk_window_get_size (GTK_WINDOW (window), &width, &height);

          /* ...and remember them as default for new windows */
          g_object_set (G_OBJECT (window->preferences), "last-window-width", width, "last-window-height", height, NULL);
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
 * thunar_window_new:
 *
 * Allocates a new #ThunarWindow instance, which isn't
 * associated with any directory.
 *
 * Return value: the newly allocated #ThunarWindow instance.
 **/
GtkWidget*
thunar_window_new (void)
{
  return g_object_new (THUNAR_TYPE_WINDOW, NULL);
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
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), NULL);
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
  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we already display the requested directory */
  if (G_UNLIKELY (window->current_directory == current_directory))
    return;

  /* disconnect from the previously active directory */
  if (G_LIKELY (window->current_directory != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (window->current_directory), thunar_window_current_directory_changed, window);
      g_object_unref (G_OBJECT (window->current_directory));
    }

  window->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the file and connect the "changed" signal */
      g_signal_connect (G_OBJECT (current_directory), "changed", G_CALLBACK (thunar_window_current_directory_changed), window);
      g_object_ref (G_OBJECT (current_directory));
    
      /* update window icon and title */
      thunar_window_current_directory_changed (current_directory, window);
    }

  /* enable the 'Open new window' action if we have a valid directory */
  thunar_gtk_action_group_set_action_sensitive (window->action_group, "open-new-window", (current_directory != NULL));

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
 * thunar_window_get_zoom_level:
 * @window : a #ThunarWindow instance.
 *
 * Returns the #ThunarZoomLevel for @window.
 *
 * Return value: the #ThunarZoomLevel for @indow.
 **/
ThunarZoomLevel
thunar_window_get_zoom_level (ThunarWindow *window)
{
  g_return_val_if_fail (THUNAR_IS_WINDOW (window), THUNAR_ZOOM_LEVEL_NORMAL);
  return window->zoom_level;
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
  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (zoom_level >= 0 && zoom_level < THUNAR_ZOOM_N_LEVELS);

  /* check if we have a new zoom level */
  if (G_LIKELY (window->zoom_level != zoom_level))
    {
      /* remember the new zoom level */
      window->zoom_level = zoom_level;

      /* update the "Zoom In" and "Zoom Out" actions */
      thunar_gtk_action_group_set_action_sensitive (window->action_group, "zoom-in", (zoom_level < THUNAR_ZOOM_N_LEVELS - 1));
      thunar_gtk_action_group_set_action_sensitive (window->action_group, "zoom-out", (zoom_level > 0));

      /* notify listeners */
      g_object_notify (G_OBJECT (window), "zoom-level");
    }
}


