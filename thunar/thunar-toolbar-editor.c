/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2021 Sergios - Anestis Kefalidis <sergioskefalidis@gmail.com>
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

#include <exo/exo.h>

#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-toolbar-editor.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-application.h>



static void thunar_toolbar_editor_finalize           (GObject                  *object);
static void thunar_toolbar_editor_help_clicked       (GtkWidget                *button,
                                                      ThunarToolbarEditor      *toolbar_editor);
static void thunar_toolbar_editor_move_down          (GtkWidget                *button,
                                                      ThunarToolbarEditor      *toolbar_editor);
static void thunar_toolbar_editor_move_up            (GtkWidget                *button,
                                                      ThunarToolbarEditor      *toolbar_editor);
static void thunar_toolbar_editor_toggled            (GtkCellRendererToggle    *cell_renderer,
                                                      const gchar              *path_string,
                                                      ThunarToolbarEditor      *toolbar_editor);
static void thunar_toolbar_editor_update_buttons     (ThunarToolbarEditor      *toolbar_editor);
static void thunar_toolbar_editor_use_defaults       (GtkWidget                *button,
                                                      ThunarToolbarEditor      *toolbar_editor);



struct _ThunarToolbarEditorClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarToolbarEditor
{
  ThunarAbstractDialog __parent__;

  ThunarPreferences *preferences;

  GtkListStore      *model;

  GtkWidget         *tree_view;
  GtkWidget         *up_button;
  GtkWidget         *down_button;
};



G_DEFINE_TYPE (ThunarToolbarEditor, thunar_toolbar_editor, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_toolbar_editor_class_init (ThunarToolbarEditorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_toolbar_editor_finalize;
}



static void
thunar_toolbar_editor_init (ThunarToolbarEditor *toolbar_editor)
{
  GtkTreeSelection       *selection;
  GtkCellRenderer        *renderer;
  GtkTreeIter             iter;
  GtkWidget              *separator;
  GtkWidget              *button;
  GtkWidget              *frame;
  GtkWidget              *image;
  GtkWidget              *label;
  GtkWidget              *grid;
  GtkWidget              *vbox;
  GtkWidget              *swin;
  gint                    row = 0;
  /* Add custom actions */
  GList                  *windows;
  ThunarWindow           *window;
  ThunarxProviderFactory *provider_factory;
  GList                  *providers;
  GList                  *thunarx_menu_items = NULL;
  GList                  *lp_provider;
  GList                  *lp_item;

  /* grab a reference on the preferences */
  toolbar_editor->preferences = thunar_preferences_get ();

  /* grab a reference on the shared toolbar model */
  toolbar_editor->model = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);

  gtk_list_store_append (toolbar_editor->model, &iter);
  gtk_list_store_set (toolbar_editor->model, &iter,
                      0, TRUE,
                      1, "go-previous-symbolic",
                      2, "Previous",
                      -1);

  gtk_list_store_append (toolbar_editor->model, &iter);
  gtk_list_store_set (toolbar_editor->model, &iter,
                      0, TRUE,
                      1, "go-next-symbolic",
                      2, "Next",
                      -1);

  gtk_list_store_append (toolbar_editor->model, &iter);
  gtk_list_store_set (toolbar_editor->model, &iter,
                      0, TRUE,
                      1, "go-up-symbolic",
                      2, "Up",
                      -1);

  gtk_list_store_append (toolbar_editor->model, &iter);
  gtk_list_store_set (toolbar_editor->model, &iter,
                      0, TRUE,
                      1, "go-home-symbolic",
                      2, "Home",
                      -1);

  gtk_list_store_append (toolbar_editor->model, &iter);
  gtk_list_store_set (toolbar_editor->model, &iter,
                      0, TRUE,
                      1, "",
                      2, "Location Bar",
                      -1);

  /* Add Custom Actions */
  windows = thunar_application_get_windows (thunar_application_get ());
  window = windows->data;
  g_list_free (windows);

  /* load the menu providers from the provider factory */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  g_object_unref (provider_factory);

  if (G_UNLIKELY (providers != NULL))
    {
      /* load the menu items offered by the menu providers */
      for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
        {
          thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items (lp_provider->data, GTK_WIDGET (window), THUNARX_FILE_INFO (thunar_window_get_current_directory (window)));

          for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
            {
              gchar *name, *label_text, *icon_name;

              g_object_get (G_OBJECT (lp_item->data),
                            "name", &name,
                            "label", &label_text,
                            "icon", &icon_name,
                            NULL);
              
              if (g_str_has_prefix (name, "uca-action") == FALSE)
                {
                  g_free (name);
                  g_free (label_text);
                  g_free (icon_name);
                  break;
                }

              gtk_list_store_append (toolbar_editor->model, &iter);
              gtk_list_store_set (toolbar_editor->model, &iter,
                                  0, TRUE,
                                  1, icon_name,
                                  2, label_text,
                                  -1);

              g_free (name);
              g_free (label_text);
              g_free (icon_name);
            }

          g_list_free (thunarx_menu_items);
        }
      g_list_free_full (providers, g_object_unref);
    }

  /* connect after model has been fully initialized */
  g_signal_connect_data (G_OBJECT (toolbar_editor->model), "row-changed", G_CALLBACK (thunar_toolbar_editor_update_buttons),
                         toolbar_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_data (G_OBJECT (toolbar_editor->model), "rows-reordered", G_CALLBACK (thunar_toolbar_editor_update_buttons),
                         toolbar_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  /* setup the dialog */
  gtk_dialog_add_button (GTK_DIALOG (toolbar_editor), _("_Close"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (toolbar_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_resizable (GTK_WINDOW (toolbar_editor), FALSE);
  gtk_window_set_title (GTK_WINDOW (toolbar_editor), _("Configure the Toolbar"));

  /* add the "Help" button */
  button = gtk_button_new_with_mnemonic (_("_Help"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_toolbar_editor_help_clicked), toolbar_editor);
  gtk_box_pack_start (GTK_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (toolbar_editor))), button, FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (exo_gtk_dialog_get_action_area (GTK_DIALOG (toolbar_editor))), button, TRUE);
  gtk_widget_show (button);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (toolbar_editor))), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* create the top label for the toolbar editor dialog */
  label = gtk_label_new (_("Choose which items/actions appear in the toolbar and their order."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 2, 1);
  gtk_widget_show (label);

  /* next row */
  row++;

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (swin, TRUE);
  gtk_widget_set_vexpand (swin, TRUE);
  gtk_grid_attach (GTK_GRID (grid), swin, 0, row, 1, 6);
  gtk_widget_show (swin);

  /* create the tree view */
  toolbar_editor->tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (toolbar_editor->tree_view), FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (toolbar_editor->tree_view), GTK_TREE_MODEL (toolbar_editor->model));
  gtk_container_add (GTK_CONTAINER (swin), toolbar_editor->tree_view);
  gtk_widget_show (toolbar_editor->tree_view);

  /* append the toggle toolbar */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (thunar_toolbar_editor_toggled), toolbar_editor);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Active",
                                               renderer,
                                               "active", 0,
                                               NULL);

  /* append the icon toolbar */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Icon",
                                               renderer,
                                               "icon-name", 1,
                                               NULL);

  /* append the name toolbar */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (toolbar_editor->tree_view),
                                               -1,
                                               "Name",
                                               renderer,
                                               "text", 2,
                                               NULL);

  /* create the "Move Up" button */
  toolbar_editor->up_button = gtk_button_new_with_mnemonic (_("Move _Up"));
  g_signal_connect (G_OBJECT (toolbar_editor->up_button), "clicked", G_CALLBACK (thunar_toolbar_editor_move_up), toolbar_editor);
  gtk_grid_attach (GTK_GRID (grid), toolbar_editor->up_button, 1, row, 1, 1);
  gtk_widget_show (toolbar_editor->up_button);

  image = gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (toolbar_editor->up_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (toolbar_editor->up_button), image);
  gtk_widget_show (image);

  /* next row */
  row++;

  /* create the "Move Down" button */
  toolbar_editor->down_button = gtk_button_new_with_mnemonic (_("Move Dow_n"));
  g_signal_connect (G_OBJECT (toolbar_editor->down_button), "clicked", G_CALLBACK (thunar_toolbar_editor_move_down), toolbar_editor);
  gtk_grid_attach (GTK_GRID (grid), toolbar_editor->down_button, 1, row, 1, 1);
  gtk_widget_show (toolbar_editor->down_button);

  image = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (toolbar_editor->down_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (toolbar_editor->down_button), image);
  gtk_widget_show (image);

  /* next row */
  row++;

  /* create the horiz separator */
  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 1, row, 1, 1);
  gtk_widget_show (separator);

  /* next row */
  row++;

  /* create the "Use Default" button */
  button = gtk_button_new_with_mnemonic (_("Use De_fault"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_toolbar_editor_use_defaults), toolbar_editor);
  gtk_grid_attach (GTK_GRID (grid), button, 1, row, 1, 1);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  /* set up the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_toolbar_editor_update_buttons), toolbar_editor);

  /* select the first item */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (toolbar_editor->model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  /* grab focus to the tree view */
  gtk_widget_grab_focus (toolbar_editor->tree_view);
}



static void
thunar_toolbar_editor_finalize (GObject *object)
{
  ThunarToolbarEditor *toolbar_editor = THUNAR_TOOLBAR_EDITOR (object);

  /* release our reference on the shared toolbar model */
  g_signal_handlers_disconnect_matched (G_OBJECT (toolbar_editor->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, toolbar_editor);
  g_object_unref (G_OBJECT (toolbar_editor->model));

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (toolbar_editor->preferences));

  (*G_OBJECT_CLASS (thunar_toolbar_editor_parent_class)->finalize) (object);
}



static void
thunar_toolbar_editor_help_clicked (GtkWidget          *button,
                                   ThunarToolbarEditor *toolbar_editor)
{
  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* open the user manual */
  xfce_dialog_show_help (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                         "thunar",
                         "the-file-manager-window",
                         "customizing_the_appearance");
}



static void
thunar_toolbar_editor_move_down (GtkWidget          *button,
                                ThunarToolbarEditor *toolbar_editor)
{
  GList            *windows;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreePath      *path1;
  GtkTreePath      *path2;

  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_next (model, &iter2) == FALSE)
    return;

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter1, &iter2);

  path1 = gtk_tree_model_get_path (model, &iter1);
  path2 = gtk_tree_model_get_path (model, &iter2);

  windows = thunar_application_get_windows (thunar_application_get ());
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_toolbar_exchange_items (window, gtk_tree_path_get_indices (path1)[0], gtk_tree_path_get_indices (path2)[0]);
    }

  gtk_tree_path_free (path1);
  gtk_tree_path_free (path2);
}



static void
thunar_toolbar_editor_move_up (GtkWidget          *button,
                              ThunarToolbarEditor *toolbar_editor)
{
  GList            *windows;
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;
  GtkTreePath      *path1;
  GtkTreePath      *path2;

  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_previous (model, &iter2) == FALSE)
    return;

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter1, &iter2);

  path1 = gtk_tree_model_get_path (model, &iter1);
  path2 = gtk_tree_model_get_path (model, &iter2);

  windows = thunar_application_get_windows (thunar_application_get ());
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_toolbar_exchange_items (window, gtk_tree_path_get_indices (path1)[0], gtk_tree_path_get_indices (path2)[0]);
    }

  gtk_tree_path_free (path1);
  gtk_tree_path_free (path2);
}



static void
thunar_toolbar_editor_toggled (GtkCellRendererToggle *cell_renderer,
                              const gchar            *path_string,
                              ThunarToolbarEditor    *toolbar_editor)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  gboolean     visible;
  GList       *windows;

  _thunar_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (cell_renderer));
  _thunar_return_if_fail (THUNAR_IS_TOOLBAR_EDITOR (toolbar_editor));
  _thunar_return_if_fail (path_string != NULL);

  /* determine the tree path for the string */
  path = gtk_tree_path_new_from_string (path_string);
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (toolbar_editor->model), &iter, path))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (toolbar_editor->model), &iter, 0, &visible, -1);
      gtk_list_store_set (toolbar_editor->model, &iter, 0, !visible, -1);
    }

  windows = thunar_application_get_windows (thunar_application_get ());
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    {
      ThunarWindow *window = lp->data;
      thunar_window_toolbar_toggle_item (window, gtk_tree_path_get_indices (path)[0]);
    }
  gtk_tree_path_free (path);
}



static void
thunar_toolbar_editor_update_buttons (ThunarToolbarEditor *toolbar_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  gint              idx;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the tree path for the iter */
      path = gtk_tree_model_get_path (model, &iter);
      if (G_UNLIKELY (path == NULL))
        return;

      /* update the "Move Up"/"Move Down" buttons */
      idx = gtk_tree_path_get_indices (path)[0];
      gtk_widget_set_sensitive (toolbar_editor->up_button, (idx > 0));
      gtk_widget_set_sensitive (toolbar_editor->down_button, (idx + 1 < gtk_tree_model_iter_n_children (model, NULL)));

      /* release the path */
      gtk_tree_path_free (path);
    }
  else
    {
      /* just disable all buttons */
      gtk_widget_set_sensitive (toolbar_editor->down_button, FALSE);
      gtk_widget_set_sensitive (toolbar_editor->up_button, FALSE);
    }
}



static void
thunar_toolbar_editor_use_defaults (GtkWidget          *button,
                                   ThunarToolbarEditor *toolbar_editor)
{
  static const gchar *PROPERTY_NAMES[] =
  {
    "last-details-view-toolbar-order",
    "last-details-view-visible-toolbars",
  };

  GtkTreeSelection *selection;
  GParamSpec       *pspec;
  GValue            value = { 0, };
  guint             n;

  /* reset the given properties to its default values */
  for (n = 0; n < G_N_ELEMENTS (PROPERTY_NAMES); ++n)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (toolbar_editor->preferences), PROPERTY_NAMES[n]);
      g_value_init (&value, pspec->value_type);
      g_param_value_set_default (pspec, &value);
      g_object_set_property (G_OBJECT (toolbar_editor->preferences), PROPERTY_NAMES[n], &value);
      g_value_unset (&value);
    }

  /* reset the tree view selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (toolbar_editor->tree_view));
  gtk_tree_selection_unselect_all (selection);
}



/**
 * thunar_show_toolbar_editor:
 * @parent : the #GtkWidget the #GdkScreen on which to open the
 *           toolbar editor dialog. May also be %NULL in which case
 *           the default #GdkScreen will be used.
 *
 * Convenience function to display a #ThunarToolbarEditor.
 *
 * If @parent is a #GtkWidget the editor dialog will be opened as
 * modal dialog above the @parent. Else if @parent is a screen (if
 * @parent is %NULL the default screen is used), the dialog won't
 * be modal and it will simply popup on the specified screen.
 **/
void
thunar_show_toolbar_editor (gpointer parent)
{
  GtkWidget *window = NULL;
  GtkWidget *dialog;
  GdkScreen *screen = NULL;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the toolbar editor */
  dialog = g_object_new (THUNAR_TYPE_TOOLBAR_EDITOR, NULL);

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window and modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* set the screen for the window */
  if (screen != NULL && GDK_IS_SCREEN (screen))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);
}

