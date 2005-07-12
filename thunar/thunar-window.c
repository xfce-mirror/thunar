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

#include <thunar/thunar-details-view.h>
#include <thunar/thunar-favourites-pane.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-window.h>
#include <thunar/thunar-window-ui.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_UI_MANAGER,
};



static void     thunar_window_class_init          (ThunarWindowClass  *klass);
static void     thunar_window_init                (ThunarWindow       *window);
static void     thunar_window_finalize            (GObject            *object);
static void     thunar_window_get_property        (GObject            *object,
                                                   guint               prop_id,
                                                   GValue             *value,
                                                   GParamSpec         *pspec);
static void     thunar_window_set_property        (GObject            *object,
                                                   guint               prop_id,
                                                   const GValue       *value,
                                                   GParamSpec         *pspec);
static void     thunar_window_action_close        (GtkAction          *action,
                                                   ThunarWindow       *window);
static void     thunar_window_action_go_up        (GtkAction          *action,
                                                   ThunarWindow       *window);
static void     thunar_window_action_about        (GtkAction          *action,
                                                   ThunarWindow       *window);
static void     thunar_window_notify_loading      (ThunarView         *view,
                                                   GParamSpec         *pspec,
                                                   ThunarWindow       *window);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  GtkActionGroup  *action_group;
  GtkUIManager    *ui_manager;

  GtkWidget       *side_pane;
  GtkWidget       *location_bar;
  GtkWidget       *view;
  GtkWidget       *statusbar;

  ThunarFile      *current_directory;
};



static const GtkActionEntry const action_entries[] =
{
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close this window"), G_CALLBACK (thunar_window_action_close), },
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "go-menu", NULL, N_ ("_Go"), NULL, },
  { "open-parent", GTK_STOCK_GO_UP, N_ ("Open _Parent"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (thunar_window_action_go_up), },
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
  GtkAccelGroup *accel_group;
  GtkWidget     *vbox;
  GtkWidget     *menubar;
  GtkWidget     *paned;
  GtkWidget     *box;

  window->action_group = gtk_action_group_new ("thunar-window");
  gtk_action_group_add_actions (window->action_group, action_entries,
                                G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (window));
  
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

  paned = g_object_new (GTK_TYPE_HPANED, "border-width", 6, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);
  gtk_widget_show (paned);

  window->side_pane = thunar_favourites_pane_new ();
  g_signal_connect_swapped (G_OBJECT (window->side_pane), "change-directory",
                            G_CALLBACK (thunar_window_set_current_directory), window);
  exo_binding_new (G_OBJECT (window), "current-directory",
                   G_OBJECT (window->side_pane), "current-directory");
  gtk_paned_pack1 (GTK_PANED (paned), window->side_pane, FALSE, FALSE);
  gtk_widget_show (window->side_pane);

  box = gtk_vbox_new (FALSE, 6);
  gtk_paned_pack2 (GTK_PANED (paned), box, TRUE, FALSE);
  gtk_widget_show (box);

  window->location_bar = thunar_location_buttons_new ();
  g_signal_connect_swapped (G_OBJECT (window->location_bar), "change-directory",
                            G_CALLBACK (thunar_window_set_current_directory), window);
  exo_binding_new (G_OBJECT (window), "current-directory",
                   G_OBJECT (window->location_bar), "current-directory");
  gtk_box_pack_start (GTK_BOX (box), window->location_bar, FALSE, FALSE, 0);
  gtk_widget_show (window->location_bar);

  window->view = thunar_details_view_new ();
  g_signal_connect (G_OBJECT (window->view), "notify::loading",
                    G_CALLBACK (thunar_window_notify_loading), window);
  g_signal_connect_swapped (G_OBJECT (window->view), "change-directory",
                            G_CALLBACK (thunar_window_set_current_directory), window);
  exo_binding_new (G_OBJECT (window), "current-directory", G_OBJECT (window->view), "current-directory");
  exo_binding_new (G_OBJECT (window), "ui-manager", G_OBJECT (window->view), "ui-manager");
  gtk_box_pack_start (GTK_BOX (box), window->view, TRUE, TRUE, 0);
  gtk_widget_grab_focus (window->view);
  gtk_widget_show (window->view);

  window->statusbar = thunar_statusbar_new ();
  exo_binding_new (G_OBJECT (window->view), "statusbar-text",
                   G_OBJECT (window->statusbar), "text");
  gtk_box_pack_start (GTK_BOX (vbox), window->statusbar, FALSE, FALSE, 0);
  gtk_widget_show (window->statusbar);
}



static void
thunar_window_finalize (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

  g_object_unref (G_OBJECT (window->current_directory));
  g_object_unref (G_OBJECT (window->action_group));
  g_object_unref (G_OBJECT (window->ui_manager));

  G_OBJECT_CLASS (thunar_window_parent_class)->finalize (object);
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
thunar_window_action_close (GtkAction    *action,
                            ThunarWindow *window)
{
  gtk_object_destroy (GTK_OBJECT (window));
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
thunar_window_action_about (GtkAction    *action,
                            ThunarWindow *window)
{
  XfceAboutInfo *info;
  GtkWidget     *dialog;

  info = xfce_about_info_new (PACKAGE_NAME, PACKAGE_VERSION, _("File Manager"),
                              XFCE_COPYRIGHT_TEXT ("2004-2005", "Benedikt Meurer"),
                              XFCE_LICENSE_GPL);
  xfce_about_info_set_homepage (info, "http://thunar.xfce.org/");
  xfce_about_info_add_credit (info, "Benedikt Meurer", "benny@xfce.org", _("Project leader"));

  dialog = xfce_about_dialog_new (GTK_WINDOW (window), info, NULL);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  xfce_about_info_free (info);
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
  GtkAction *action;
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
      icon = thunar_file_load_icon (current_directory, 48);
      gtk_window_set_icon (GTK_WINDOW (window), icon);
      gtk_window_set_title (GTK_WINDOW (window),
                            thunar_file_get_special_name (current_directory));
      g_object_unref (G_OBJECT (icon));

      /* enable the 'Up' action if possible for the new directory */
      action = gtk_action_group_get_action (window->action_group, "open-parent");
      g_object_set (G_OBJECT (action),
                    "sensitive", thunar_file_has_parent (current_directory),
                    NULL);
    }

  /* tell everybody that we have a new "current-directory",
   * we do this first so other widgets display the new
   * state already while the folder view is loading.
   */
  g_object_notify (G_OBJECT (window), "current-directory");
}



