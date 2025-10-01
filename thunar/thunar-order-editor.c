/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 The Xfce Development Team
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

#include "thunar/thunar-order-editor.h"

#include "thunar/thunar-private.h"

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

typedef struct _ThunarOrderEditorPrivate ThunarOrderEditorPrivate;



struct _ThunarOrderEditorPrivate
{
  ThunarOrderModel *model;
  GtkWidget        *tree_view;
  GtkWidget        *description_area;
  GtkWidget        *settings_area;
  GtkWidget        *up_button;
  GtkWidget        *down_button;
  GtkWidget        *reset_button;
  GtkWidget        *help_button;
};



enum
{
  PROP_0,
  PROP_MODEL,
  PROP_HELP_ENABLED,
  PROP_RESET_ENABLED,
  N_PROPS,
};



enum
{
  HELP,
  N_SIGNALS,
};



static gint signals[N_SIGNALS];



static void
thunar_order_editor_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec);

static void
thunar_order_editor_help_clicked (ThunarOrderEditor *order_editor);

static void
thunar_order_editor_reset_clicked (ThunarOrderEditor *order_editor);

static void
thunar_order_editor_toggle_activity (ThunarOrderEditor *order_editor,
                                     const gchar       *path_string);

static void
thunar_order_editor_move_up (ThunarOrderEditor *order_editor);

static void
thunar_order_editor_move_down (ThunarOrderEditor *order_editor);

static void
thunar_order_editor_update_buttons (ThunarOrderEditor *order_editor);



G_DEFINE_TYPE_WITH_CODE (ThunarOrderEditor, thunar_order_editor, THUNAR_TYPE_ABSTRACT_DIALOG,
                         G_ADD_PRIVATE (ThunarOrderEditor))



static void
thunar_order_editor_class_init (ThunarOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = thunar_order_editor_set_property;

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        NULL,
                                                        NULL,
                                                        THUNAR_TYPE_ORDER_MODEL,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_HELP_ENABLED,
                                   g_param_spec_boolean ("help-enabled",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_RESET_ENABLED,
                                   g_param_spec_boolean ("reset-enabled",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  signals[HELP] = g_signal_new ("help",
                                G_TYPE_FROM_CLASS (object_class),
                                0,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE,
                                0);
}



static void
thunar_order_editor_init (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);
  GtkCellRenderer          *renderer;
  GtkWidget                *action_area = xfce_gtk_dialog_get_action_area (GTK_DIALOG (order_editor));
  GtkWidget                *content_area = gtk_dialog_get_content_area (GTK_DIALOG (order_editor));
  GtkWidget                *vbox;
  GtkWidget                *hbox;
  GtkWidget                *swin;
  GtkWidget                *vbox_tree;
  GtkWidget                *vbox_right;
  GtkWidget                *vbox_buttons;
  GtkWidget                *image;
  GtkWidget                *button_box;
  GtkStyleContext          *style_context;

  /* create dialog buttons */
  gtk_dialog_add_button (GTK_DIALOG (order_editor), _("_Close"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (order_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_default_size (GTK_WINDOW (order_editor), 700, 500);
  gtk_window_set_resizable (GTK_WINDOW (order_editor), TRUE);

  priv->help_button = gtk_button_new_with_mnemonic (_("_Help"));
  g_signal_connect_swapped (G_OBJECT (priv->help_button), "clicked",
                            G_CALLBACK (thunar_order_editor_help_clicked), order_editor);
  gtk_box_pack_start (GTK_BOX (action_area), priv->help_button, FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area), priv->help_button, TRUE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* create description area */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  priv->description_area = gtk_widget_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), priv->description_area, FALSE, FALSE, 0);
  gtk_widget_show (priv->description_area);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);
  gtk_widget_show (hbox);

  /* create vbox_tree */
  vbox_tree = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox_tree, TRUE, TRUE, 0);
  gtk_widget_show (vbox_tree);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (swin, TRUE);
  gtk_widget_set_vexpand (swin, TRUE);
  gtk_widget_set_size_request (swin, 100, 200);
  gtk_box_pack_start (GTK_BOX (vbox_tree), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  priv->tree_view = gtk_tree_view_new ();
  g_signal_connect_swapped (priv->tree_view, "cursor-changed",
                            G_CALLBACK (thunar_order_editor_update_buttons), order_editor);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->tree_view), THUNAR_ORDER_MODEL_COLUMN_NAME);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (priv->tree_view), THUNAR_ORDER_MODEL_COLUMN_TOOLTIP);
  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (priv->tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (swin), priv->tree_view);
  gtk_widget_show (priv->tree_view);

  /* append the toggle visibility column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_swapped (G_OBJECT (renderer), "toggled",
                            G_CALLBACK (thunar_order_editor_toggle_activity), order_editor);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view),
                                               -1,
                                               "Active",
                                               renderer,
                                               "active", THUNAR_ORDER_MODEL_COLUMN_ACTIVE,
                                               "visible", THUNAR_ORDER_MODEL_COLUMN_MUTABLE,
                                               NULL);

  /* append the icon column */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view),
                                               -1,
                                               "Icon",
                                               renderer,
                                               "icon-name", THUNAR_ORDER_MODEL_COLUMN_ICON,
                                               NULL);

  /* append the name column */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view),
                                               -1,
                                               "Name",
                                               renderer,
                                               "markup", THUNAR_ORDER_MODEL_COLUMN_NAME,
                                               "sensitive", THUNAR_ORDER_MODEL_COLUMN_ACTIVE,
                                               NULL);

  /* right vbox */
  vbox_right = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox_right, FALSE, FALSE, 0);
  gtk_widget_set_vexpand (vbox_right, FALSE);
  gtk_widget_show (vbox_right);

  /* Create the buttons vbox container */
  vbox_buttons = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  style_context = gtk_widget_get_style_context (vbox_buttons);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);
  gtk_box_pack_start (GTK_BOX (vbox_right), vbox_buttons, FALSE, FALSE, 0);
  gtk_widget_set_vexpand (vbox_buttons, FALSE);
  gtk_widget_show (vbox_buttons);

  /* create the "Move Up" button */
  priv->up_button = gtk_button_new ();
  g_signal_connect_swapped (priv->up_button, "clicked", G_CALLBACK (thunar_order_editor_move_up), order_editor);
  gtk_widget_set_tooltip_text (priv->up_button, _("Move currently selected item up by one row"));
  gtk_box_pack_start (GTK_BOX (vbox_buttons), priv->up_button, FALSE, FALSE, 0);
  gtk_widget_show (priv->up_button);

  image = gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (priv->up_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (priv->up_button), image);
  gtk_widget_show (image);

  /* create the "Move Down" button */
  priv->down_button = gtk_button_new ();
  g_signal_connect_swapped (priv->down_button, "clicked", G_CALLBACK (thunar_order_editor_move_down), order_editor);
  gtk_widget_set_tooltip_text (priv->down_button, _("Move currently selected item down by one row"));
  gtk_box_pack_start (GTK_BOX (vbox_buttons), priv->down_button, FALSE, FALSE, 0);
  gtk_widget_show (priv->down_button);

  image = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_always_show_image (GTK_BUTTON (priv->down_button), TRUE);
  gtk_button_set_image (GTK_BUTTON (priv->down_button), image);
  gtk_widget_show (image);

  /* button box */
  button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_START);
  style_context = gtk_widget_get_style_context (button_box);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
  gtk_box_pack_start (GTK_BOX (vbox_tree), button_box, FALSE, FALSE, 0);
  gtk_widget_show (button_box);

  /* create the "Use Default" button */
  priv->reset_button = gtk_button_new_with_mnemonic (_("De_fault Order"));
  g_signal_connect_swapped (G_OBJECT (priv->reset_button), "clicked",
                            G_CALLBACK (thunar_order_editor_reset_clicked), order_editor);
  gtk_box_pack_start (GTK_BOX (button_box), priv->reset_button, FALSE, FALSE, 0);

  /* create settings area */
  priv->settings_area = gtk_widget_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), priv->settings_area, FALSE, FALSE, 0);
  gtk_widget_show (priv->settings_area);

  thunar_order_editor_update_buttons (order_editor);
}



static void
thunar_order_editor_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarOrderEditor        *order_editor = THUNAR_ORDER_EDITOR (object);
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_get_object (value);
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), GTK_TREE_MODEL (priv->model));
      thunar_order_editor_update_buttons (order_editor);
      break;

    case PROP_HELP_ENABLED:
      if (g_value_get_boolean (value))
        gtk_widget_show (priv->help_button);
      else
        gtk_widget_hide (priv->help_button);
      break;

    case PROP_RESET_ENABLED:
      if (g_value_get_boolean (value))
        gtk_widget_show (priv->reset_button);
      else
        gtk_widget_hide (priv->reset_button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}



static void
thunar_order_editor_help_clicked (ThunarOrderEditor *order_editor)
{
  g_signal_emit (order_editor, signals[HELP], 0);
}



static void
thunar_order_editor_reset_clicked (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);

  thunar_order_model_reset (THUNAR_ORDER_MODEL (priv->model));
  thunar_order_model_reload (THUNAR_ORDER_MODEL (priv->model));
  thunar_order_editor_update_buttons (order_editor);
}



static void
thunar_order_editor_toggle_activity (ThunarOrderEditor *order_editor,
                                     const gchar       *path_string)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);
  GtkTreeModel             *tree_model = GTK_TREE_MODEL (priv->model);
  GtkTreePath              *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter               iter;
  gboolean                  active;

  if (gtk_tree_model_get_iter (tree_model, &iter, path))
    {
      gtk_tree_model_get (tree_model, &iter, THUNAR_ORDER_MODEL_COLUMN_ACTIVE, &active, -1);
      thunar_order_model_set_activity (priv->model, &iter, !active);
      thunar_order_editor_update_buttons (order_editor);
    }
  gtk_tree_path_free (path);
}



static void
thunar_order_editor_move_up (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);
  GtkTreeSelection         *selection;
  GtkTreeModel             *model;
  GtkTreeIter               iter1;
  GtkTreeIter               iter2;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_previous (model, &iter2))
    {
      thunar_order_model_swap_items (priv->model, &iter1, &iter2);
      gtk_tree_selection_select_iter (selection, &iter2);
      thunar_order_editor_update_buttons (order_editor);
    }
}



static void
thunar_order_editor_move_down (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);
  GtkTreeSelection         *selection;
  GtkTreeModel             *model;
  GtkTreeIter               iter1;
  GtkTreeIter               iter2;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_get_selected (selection, &model, &iter1);
  iter2 = iter1;
  if (gtk_tree_model_iter_next (model, &iter2))
    {
      thunar_order_model_swap_items (priv->model, &iter1, &iter2);
      gtk_tree_selection_select_iter (selection, &iter2);
      thunar_order_editor_update_buttons (order_editor);
    }
}



static void
thunar_order_editor_update_buttons (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv = thunar_order_editor_get_instance_private (order_editor);
  GtkTreeSelection         *selection;
  GtkTreeModel             *tree_model;
  GtkTreeIter               iter1;
  GtkTreeIter              *iter2;
  gboolean                  is_active;

  gtk_widget_set_sensitive (priv->up_button, FALSE);
  gtk_widget_set_sensitive (priv->down_button, FALSE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  if (gtk_tree_selection_get_selected (selection, &tree_model, &iter1))
    {
      gtk_tree_model_get (tree_model, &iter1, THUNAR_ORDER_MODEL_COLUMN_ACTIVE, &is_active, -1);

      iter2 = gtk_tree_iter_copy (&iter1);
      if (gtk_tree_model_iter_previous (tree_model, iter2))
        gtk_widget_set_sensitive (priv->up_button, TRUE);
      gtk_tree_iter_free (iter2);

      iter2 = gtk_tree_iter_copy (&iter1);
      if (gtk_tree_model_iter_next (tree_model, iter2))
        gtk_widget_set_sensitive (priv->down_button, TRUE);
      gtk_tree_iter_free (iter2);
    }
}



GtkContainer *
thunar_order_editor_get_description_area (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv;

  _thunar_return_val_if_fail (THUNAR_IS_ORDER_EDITOR (order_editor), NULL);


  priv = thunar_order_editor_get_instance_private (order_editor);
  return GTK_CONTAINER (priv->description_area);
}



GtkContainer *
thunar_order_editor_get_settings_area (ThunarOrderEditor *order_editor)
{
  ThunarOrderEditorPrivate *priv;

  _thunar_return_val_if_fail (THUNAR_IS_ORDER_EDITOR (order_editor), NULL);

  priv = thunar_order_editor_get_instance_private (order_editor);
  return GTK_CONTAINER (priv->settings_area);
}



void
thunar_order_editor_set_model (ThunarOrderEditor *order_editor,
                               ThunarOrderModel  *model)
{
  _thunar_return_if_fail (THUNAR_IS_ORDER_EDITOR (order_editor));
  _thunar_return_if_fail (THUNAR_IS_ORDER_MODEL (model));

  g_object_set (order_editor, "model", model, NULL);
}



void
thunar_order_editor_show (ThunarOrderEditor *order_editor,
                          GtkWidget         *window)
{
  GdkScreen *screen = NULL;

  _thunar_return_if_fail (THUNAR_IS_ORDER_EDITOR (order_editor));
  _thunar_return_if_fail (GTK_IS_WIDGET (window));

  if (window == NULL)
    {
      screen = gdk_screen_get_default ();
    }
  else
    {
      screen = gtk_widget_get_screen (window);
      window = gtk_widget_get_toplevel (window);
    }

  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window and modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (order_editor), TRUE);
      gtk_window_set_modal (GTK_WINDOW (order_editor), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (order_editor), GTK_WINDOW (window));
    }

  /* set the screen for the window */
  if (screen != NULL && GDK_IS_SCREEN (screen))
    gtk_window_set_screen (GTK_WINDOW (order_editor), screen);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (order_editor));

  /* destroy the dialog */
  gtk_widget_destroy (GTK_WIDGET (order_editor));
}
