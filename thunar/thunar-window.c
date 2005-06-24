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
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-window.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void thunar_window_class_init      (ThunarWindowClass  *klass);
static void thunar_window_init            (ThunarWindow       *window);
static void thunar_window_dispose         (GObject            *object);
static void thunar_window_finalize        (GObject            *object);
static void thunar_window_get_property    (GObject            *object,
                                           guint               prop_id,
                                           GValue             *value,
                                           GParamSpec         *pspec);
static void thunar_window_set_property    (GObject            *object,
                                           guint               prop_id,
                                           const GValue       *value,
                                           GParamSpec         *pspec);
static void thunar_window_action_close    (GtkAction          *action,
                                           ThunarWindow       *window);
static void thunar_window_action_go_up    (GtkAction          *action,
                                           ThunarWindow       *window);
static void thunar_window_file_activated  (ThunarView         *view,
                                           ThunarFile         *file,
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
  { "go-up", GTK_STOCK_GO_UP, N_ ("_Up"), "<alt>Up", N_ ("Open the parent folder"), G_CALLBACK (thunar_window_action_go_up), },
  { "help-menu", NULL, N_ ("_Help"), NULL, },
};

static const gchar ui_description[] =
  "<ui>"
  "  <menubar name='main-menu'>"
  "    <menu action='file-menu'>"
  "      <menuitem action='close' />"
  "    </menu>"
  "    <menu action='edit-menu' />"
  "    <menu action='view-menu' />"
  "    <menu action='go-menu'>"
  "      <menuitem action='go-up' />"
  "    </menu>"
  "    <menu action='help-menu' />"
  "  </menubar>"
  "</ui>";

G_DEFINE_TYPE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW);



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_window_dispose;
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
                                                        G_PARAM_READWRITE));
}



static void
thunar_window_init (ThunarWindow *window)
{
  ThunarListModel *model;
  GtkAccelGroup   *accel_group;
  GtkWidget       *vbox;
  GtkWidget       *menubar;
  GtkWidget       *paned;
  GtkWidget       *swin;
  GtkWidget       *box;

  window->action_group = gtk_action_group_new ("thunar-window");
  gtk_action_group_add_actions (window->action_group, action_entries,
                                G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (window));
  
  window->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (window->ui_manager, window->action_group, 0);
  gtk_ui_manager_add_ui_from_string (window->ui_manager, ui_description, -1, NULL);

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

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (box), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  window->view = thunar_details_view_new ();
  g_signal_connect (G_OBJECT (window->view), "file-activated",
                    G_CALLBACK (thunar_window_file_activated), window);
  gtk_container_add (GTK_CONTAINER (swin), window->view);
  gtk_widget_grab_focus (window->view);
  gtk_widget_show (window->view);

  model = thunar_list_model_new ();
  thunar_view_set_model (THUNAR_VIEW (window->view), model);
  g_object_unref (G_OBJECT (model));

  window->statusbar = g_object_new (THUNAR_TYPE_STATUSBAR,
                                    "view", window->view,
                                    NULL);
  gtk_box_pack_start (GTK_BOX (vbox), window->statusbar, FALSE, FALSE, 0);
  gtk_widget_show (window->statusbar);
}



static void
thunar_window_dispose (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);
  thunar_window_set_current_directory (window, NULL);
  G_OBJECT_CLASS (thunar_window_parent_class)->dispose (object);
}



static void
thunar_window_finalize (GObject *object)
{
  ThunarWindow *window = THUNAR_WINDOW (object);

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
  gtk_main_quit ();
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
thunar_window_file_activated (ThunarView   *view,
                              ThunarFile   *file,
                              ThunarWindow *window)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_WINDOW (window));

  /* change directory if the user activated a file
   * that refers to a directory.
   */
  if (thunar_file_is_directory (file))
    thunar_window_set_current_directory (window, file);
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
  ThunarListModel *model;
  ThunarFolder    *folder;
  GtkAction       *action;
  GtkWidget       *dialog;
  GdkPixbuf       *icon;
  GError          *error = NULL;

  g_return_if_fail (THUNAR_IS_WINDOW (window));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  // TODO: We need a better error-recovery here!

  /* check if we already display the requests directory */
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
      action = gtk_action_group_get_action (window->action_group, "go-up");
      g_object_set (G_OBJECT (action),
                    "sensitive", thunar_file_has_parent (current_directory),
                    NULL);
    }

  /* setup the folder for the view, we use a simple but very effective
   * trick here to speed up the folder change: we completely disconnect
   * the model from the view, load the folder into the model and afterwards
   * reconnect the model with the view. This way we avoid having to fire
   * and process thousands of row_removed() and row_inserted() signals.
   * Instead the view can process the complete file list in the model
   * ONCE.
   */
  model = thunar_view_get_model (THUNAR_VIEW (window->view));
  g_object_ref (G_OBJECT (model));
  thunar_view_set_model (THUNAR_VIEW (window->view), NULL);
  if (G_LIKELY (current_directory != NULL))
    {
      /* try to open the directory */
      folder = thunar_file_open_as_folder (current_directory, &error);
      if (G_LIKELY (folder != NULL))
        {
          thunar_list_model_set_folder (model, folder);
          g_object_unref (G_OBJECT (folder));
        }
      else
        {
          /* error condition, reset the folder */
          thunar_list_model_set_folder (model, NULL);

          /* make sure the window is shown */
          gtk_widget_show_now (GTK_WIDGET (window));

          /* display an error dialog */
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to open directory `%s': %s",
                                           thunar_file_get_display_name (current_directory),
                                           error->message);
          gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);

          /* free the error details */
          g_error_free (error);
        }
    }
  else
    {
      /* just reset the folder, so nothing is displayed */
      thunar_list_model_set_folder (model, NULL);
    }
  thunar_view_set_model (THUNAR_VIEW (window->view), model);
  g_object_unref (G_OBJECT (model));

  /* tell everybody that we have a new "current-directory" */
  g_object_notify (G_OBJECT (window), "current-directory");
}



