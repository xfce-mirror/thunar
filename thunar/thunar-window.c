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
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-favourites-pane.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-location-dialog.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-window-ui.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_UI_MANAGER,
};



static void     thunar_window_class_init                  (ThunarWindowClass  *klass);
static void     thunar_window_init                        (ThunarWindow       *window);
static void     thunar_window_finalize                    (GObject            *object);
static void     thunar_window_get_property                (GObject            *object,
                                                           guint               prop_id,
                                                           GValue             *value,
                                                           GParamSpec         *pspec);
static void     thunar_window_set_property                (GObject            *object,
                                                           guint               prop_id,
                                                           const GValue       *value,
                                                           GParamSpec         *pspec);
static void     thunar_window_action_open_new_window      (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_close_all_windows    (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_close                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_location_bar_changed (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_side_pane_changed    (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_view_changed         (GtkRadioAction     *action,
                                                           GtkRadioAction     *current,
                                                           ThunarWindow       *window);
static void     thunar_window_action_go_up                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_location             (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_action_about                (GtkAction          *action,
                                                           ThunarWindow       *window);
static void     thunar_window_notify_loading              (ThunarView         *view,
                                                           GParamSpec         *pspec,
                                                           ThunarWindow       *window);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  ThunarIconFactory *icon_factory;

  GtkActionGroup  *action_group;
  GtkUIManager    *ui_manager;

  GtkWidget       *paned;
  GtkWidget       *location_bar;
  GtkWidget       *view_container;
  GtkWidget       *view;
  GtkWidget       *statusbar;

  ThunarFile      *current_directory;
};



static const GtkActionEntry action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "open-new-window", NULL, N_ ("Open New _Window"), "<control>N", N_ ("Open a new Thunar window for the displayed location"), G_CALLBACK (thunar_window_action_open_new_window), },
  { "close-all-windows", NULL, N_ ("Close _All Windows"), "<control><shift>W", N_ ("Close all Thunar windows"), G_CALLBACK (thunar_window_action_close_all_windows), },
  { "close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close this window"), G_CALLBACK (thunar_window_action_close), },
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "view-location-bar-menu", NULL, N_ ("_Location Bar"), NULL, },
  { "view-side-pane-menu", NULL, N_ ("_Side Pane"), NULL, },
  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "open-parent", GTK_STOCK_GO_UP, N_ ("Open _Parent"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (thunar_window_action_go_up), },
  { "open-location", NULL, N_ ("Open _Location..."), "<control>L", N_ ("Specify a location to open"), G_CALLBACK (thunar_window_action_location), },
  { "help-menu", NULL, N_ ("_Help"), NULL, },
#if GTK_CHECK_VERSION(2,6,0)
  { "about", GTK_STOCK_ABOUT, N_ ("_About"), NULL, N_ ("Display information about Thunar"), G_CALLBACK (thunar_window_action_about), },
#else
  { "about", GTK_STOCK_DIALOG_INFO, N_ ("_About"), NULL, N_ ("Display information about Thunar"), G_CALLBACK (thunar_window_action_about), },
#endif
};



G_DEFINE_TYPE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW);



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_window_finalize;
  gobject_class->get_property = thunar_window_get_property;
  gobject_class->set_property = thunar_window_set_property;

  /**
   * ThunarWindow:current-directory:
   *
   * The directory currently displayed within this #ThunarWindow
   * or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_DIRECTORY,
                                   g_param_spec_object ("current-directory",
                                                        _("Current directory"),
                                                        _("The directory currently displayed within this window"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

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
                                                        _("UI manager"),
                                                        _("The UI manager used within this window"),
                                                        GTK_TYPE_UI_MANAGER,
                                                        EXO_PARAM_READABLE));
}



static void
thunar_window_init (ThunarWindow *window)
{
  GtkRadioAction *radio_action;
  GtkAccelGroup  *accel_group;
  GtkAction      *action;
  GtkWidget      *vbox;
  GtkWidget      *menubar;
  GtkWidget      *box;
  GSList         *group;

  window->icon_factory = thunar_icon_factory_get_default ();

  window->action_group = gtk_action_group_new ("thunar-window");
  gtk_action_group_set_translation_domain (window->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (window->action_group, action_entries,
                                G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (window));

  /*
   * add the side pane options
   */
  radio_action = gtk_radio_action_new ("view-side-pane-favourites", N_ ("_Favourites"), NULL, NULL, THUNAR_TYPE_FAVOURITES_PANE);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-side-pane-hidden", N_ ("_Hidden"), NULL, NULL, G_TYPE_INVALID);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  /*
   * add the location bar options
   */
  radio_action = gtk_radio_action_new ("view-location-bar-buttons", N_ ("_Button Style"), NULL, NULL, THUNAR_TYPE_LOCATION_BUTTONS);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-location-bar-entry", N_ ("_Traditional Style"), NULL, NULL, THUNAR_TYPE_LOCATION_ENTRY);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-location-bar-hidden", N_ ("_Hidden"), NULL, NULL, G_TYPE_INVALID);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  /*
   * add view options
   */
  radio_action = gtk_radio_action_new ("view-as-icons", N_ ("View as _Icons"), NULL, NULL, THUNAR_TYPE_ICON_VIEW);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, NULL);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  radio_action = gtk_radio_action_new ("view-as-detailed-list", N_ ("View as _Detailed List"), NULL, NULL, THUNAR_TYPE_DETAILS_VIEW);
  gtk_action_group_add_action (window->action_group, GTK_ACTION (radio_action));
  gtk_radio_action_set_group (radio_action, group);
  group = gtk_radio_action_get_group (radio_action);
  g_object_unref (G_OBJECT (radio_action));

  window->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, thunar_window_ui, thunar_window_ui_length, NULL);

  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), _("Thunar"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  menubar = gtk_ui_manager_get_widget (window->ui_manager, "/main-menu");
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);
  gtk_widget_show (menubar);

  window->paned = g_object_new (GTK_TYPE_HPANED, "border-width", 6, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), window->paned, TRUE, TRUE, 0);
  gtk_widget_show (window->paned);

  box = gtk_vbox_new (FALSE, 6);
  gtk_paned_pack2 (GTK_PANED (window->paned), box, TRUE, FALSE);
  gtk_widget_show (box);

  window->location_bar = gtk_alignment_new (0.0f, 0.5f, 1.0f, 1.0f);
  gtk_box_pack_start (GTK_BOX (box), window->location_bar, FALSE, FALSE, 0);
  gtk_widget_show (window->location_bar);

  window->view_container = gtk_alignment_new (0.5f, 0.5f, 1.0f, 1.0f);
  gtk_box_pack_start (GTK_BOX (box), window->view_container, TRUE, TRUE, 0);
  gtk_widget_show (window->view_container);

  window->statusbar = thunar_statusbar_new ();
  g_signal_connect_swapped (G_OBJECT (window->statusbar), "change-directory",
                            G_CALLBACK (thunar_window_set_current_directory), window);
  exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->statusbar), "current-directory");
  gtk_box_pack_start (GTK_BOX (vbox), window->statusbar, FALSE, FALSE, 0);
  gtk_widget_show (window->statusbar);

  /* activate the selected side pane */
  action = gtk_action_group_get_action (window->action_group, "view-side-pane-favourites");
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_side_pane_changed), window);
  thunar_window_action_side_pane_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);

  /* activate the selected location bar */
  action = gtk_action_group_get_action (window->action_group, "view-location-bar-buttons");
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_location_bar_changed), window);
  thunar_window_action_location_bar_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);

  /* activate the selected view */
  action = gtk_action_group_get_action (window->action_group, "view-as-icons");
  g_signal_connect (G_OBJECT (action), "changed", G_CALLBACK (thunar_window_action_view_changed), window);
  thunar_window_action_view_changed (GTK_RADIO_ACTION (action), GTK_RADIO_ACTION (action), window);
}



static void
thunar_window_finalize (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  g_object_unref (G_OBJECT (window->current_directory));
  g_object_unref (G_OBJECT (window->action_group));
  g_object_unref (G_OBJECT (window->icon_factory));
  g_object_unref (G_OBJECT (window->ui_manager));

  (*G_OBJECT_CLASS (thunar_window_parent_class)->finalize) (object);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
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
thunar_window_action_location_bar_changed (GtkRadioAction *action,
                                           GtkRadioAction *current,
                                           ThunarWindow   *window)
{
  GtkWidget *widget;
  GType      type;

  /* drop the previous location bar (if any) */
  widget = gtk_bin_get_child (GTK_BIN (window->location_bar));
  if (G_LIKELY (widget != NULL))
    gtk_widget_destroy (widget);

  /* determine the new type of location bar */
  type = gtk_radio_action_get_current_value (action);

  if (G_LIKELY (type != G_TYPE_INVALID))
    {
      /* initialize the new location bar widget */
      widget = g_object_new (type, NULL);
      g_signal_connect_swapped (G_OBJECT (widget), "change-directory",
                                G_CALLBACK (thunar_window_set_current_directory), window);
      exo_binding_new (G_OBJECT (window), "current-directory",
                       G_OBJECT (widget), "current-directory");
      gtk_container_add (GTK_CONTAINER (window->location_bar), widget);
      gtk_widget_show (widget);

      /* display the location bar container */
      gtk_widget_show (window->location_bar);
    }
  else
    {
      /* hide the location bar container */
      gtk_widget_hide (window->location_bar);
    }
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
  if (G_LIKELY (type != G_TYPE_INVALID))
    {
      widget = g_object_new (type, NULL);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (widget), "current-directory");
      g_signal_connect_swapped (G_OBJECT (widget), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      gtk_paned_pack1 (GTK_PANED (window->paned), widget, FALSE, FALSE);
      gtk_widget_show (widget);
    }
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
  if (G_LIKELY (type != G_TYPE_INVALID))
    {
      window->view = g_object_new (type, "ui-manager", window->ui_manager, NULL);
      g_signal_connect (G_OBJECT (window->view), "notify::loading", G_CALLBACK (thunar_window_notify_loading), window);
      g_signal_connect_swapped (G_OBJECT (window->view), "change-directory", G_CALLBACK (thunar_window_set_current_directory), window);
      exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->view), "current-directory");
      exo_binding_new (G_OBJECT (window->view), "loading", G_OBJECT (window->statusbar), "loading");
      exo_binding_new (G_OBJECT (window->view), "statusbar-text", G_OBJECT (window->statusbar), "text");
      gtk_container_add (GTK_CONTAINER (window->view_container), window->view);
      gtk_widget_grab_focus (window->view);
      gtk_widget_show (window->view);
    }
  else
    {
      /* this should not happen under normal conditions */
      window->view = NULL;
    }
}



static void
thunar_window_action_go_up (GtkAction    *action,
                            ThunarWindow *window)
{
  ThunarFile *parent;

  // FIXME: error handling
  parent = thunar_file_get_parent (window->current_directory, NULL);
  thunar_window_set_current_directory (window, parent);
  g_object_unref (G_OBJECT (parent));
}



static void
thunar_window_action_location (GtkAction    *action,
                               ThunarWindow *window)
{
  GtkWidget *dialog;
  GtkWidget *widget;

  /* bring up the "Open Location"-dialog if the window has no location bar or the location bar
   * in the window does not support text entry by the user.
   */
  widget = gtk_bin_get_child (GTK_BIN (window->location_bar));
  if (widget == NULL || !thunar_location_bar_accept_focus (THUNAR_LOCATION_BAR (widget)))
    {
      dialog = thunar_location_dialog_new ();
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
      thunar_location_dialog_set_selected_file (THUNAR_LOCATION_DIALOG (dialog), thunar_window_get_current_directory (window));
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        thunar_window_set_current_directory (window, thunar_location_dialog_get_selected_file (THUNAR_LOCATION_DIALOG (dialog)));
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
  gtk_about_dialog_set_email_hook ((gpointer) exo_noop, NULL, NULL);
  gtk_about_dialog_set_url_hook ((gpointer) exo_noop, NULL, NULL);
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
thunar_window_notify_loading (ThunarView   *view,
                              GParamSpec   *pspec,
                              ThunarWindow *window)
{
  GdkCursor *cursor;

  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (THUNAR_VIEW (window->view) == view);

  /* make sure that the window is shown */
  gtk_widget_show_now (GTK_WIDGET (window));

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

  /* be sure to sync the changes to the Xserver */
  gdk_display_flush (gtk_widget_get_display (GTK_WIDGET (window)));
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
  GdkPixbuf *icon;

  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we already display the requested directory */
  if (G_UNLIKELY (window->current_directory == current_directory))
    return;

  /* disconnect from the previously active directory */
  if (G_LIKELY (window->current_directory != NULL))
    g_object_unref (G_OBJECT (window->current_directory));

  window->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (window->current_directory));

  /* set window title/icon to the selected directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* set window title and icon */
      icon = thunar_file_load_icon (current_directory, THUNAR_FILE_ICON_STATE_DEFAULT, window->icon_factory, 48);
      gtk_window_set_icon (GTK_WINDOW (window), icon);
      gtk_window_set_title (GTK_WINDOW (window), thunar_file_get_special_name (current_directory));
      g_object_unref (G_OBJECT (icon));
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



