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

#include <libxfce4util/libxfce4util.h>



typedef struct _ThunarOrderEditorPrivate ThunarOrderEditorPrivate;

struct _ThunarOrderEditorPrivate
{
  XfceItemListModel *model;
  GtkWidget         *item_view;
  GtkWidget         *description_area;
  GtkWidget         *settings_area;
  GtkWidget         *help_button;
  GSimpleAction     *reset_action;
};

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_HELP_ENABLED,
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
                                                        XFCE_TYPE_ITEM_LIST_MODEL,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_HELP_ENABLED,
                                   g_param_spec_boolean ("help-enabled",
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
  GtkWidget                *action_area = xfce_gtk_dialog_get_action_area (GTK_DIALOG (order_editor));
  GtkWidget                *content_area = gtk_dialog_get_content_area (GTK_DIALOG (order_editor));
  GtkWidget                *vbox;
  GtkWidget                *hbox;
  GtkWidget                *image;

  /* create dialog buttons */
  gtk_dialog_add_button (GTK_DIALOG (order_editor), _("_Close"), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (order_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_default_size (GTK_WINDOW (order_editor), 600, 500);
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

  /* create item_view */
  priv->item_view = xfce_item_list_view_new (NULL);
  xfce_item_list_view_set_label_visibility (XFCE_ITEM_LIST_VIEW (priv->item_view), TRUE);
  gtk_widget_set_size_request (priv->item_view, 100, 250);
  gtk_box_pack_start (GTK_BOX (hbox), priv->item_view, TRUE, TRUE, 0);
  gtk_widget_show (priv->item_view);

  /* create settings area */
  priv->settings_area = gtk_widget_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), priv->settings_area, FALSE, FALSE, 0);
  gtk_widget_show (priv->settings_area);
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
      g_object_set (priv->item_view, "model", priv->model, NULL);
      break;

    case PROP_HELP_ENABLED:
      if (g_value_get_boolean (value))
        gtk_widget_show (priv->help_button);
      else
        gtk_widget_hide (priv->help_button);
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
                               XfceItemListModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_ORDER_EDITOR (order_editor));
  _thunar_return_if_fail (XFCE_IS_ITEM_LIST_MODEL (model));

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
