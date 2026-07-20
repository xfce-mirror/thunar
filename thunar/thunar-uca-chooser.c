/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2026 The Xfce Development Team
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

#include "thunar/thunar-uca-chooser.h"

#include "thunar/thunar-order-editor.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-uca-editor.h"
#include "thunar/thunar-uca-model.h"

#include <libxfce4ui/libxfce4ui.h>



struct _ThunarUcaChooserClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarUcaChooser
{
  ThunarOrderEditor __parent__;

  ThunarUcaModel *uca_model;
};



static void
thunar_uca_chooser_finalize (GObject *object);



THUNARX_DEFINE_TYPE (ThunarUcaChooser, thunar_uca_chooser, THUNAR_TYPE_ORDER_EDITOR);



static void
thunar_uca_chooser_class_init (ThunarUcaChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_uca_chooser_finalize;
}



static gboolean
thunar_uca_chooser_add (ThunarUcaChooser *chooser)
{
  if (thunar_uca_editor_show (GTK_WINDOW (chooser), NULL, NULL))
    return FALSE;

  /* failed to add element, stop event processing */
  return TRUE;
}



static gboolean
thunar_uca_chooser_remove (ThunarUcaChooser *chooser,
                           gint             *indexes,
                           gint              n_items)
{
  GtkWidget *dialog;
  gint       response;

  /* create the question dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (chooser),
                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                   | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Are you sure you want to delete the selected actions?"));

  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete action"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("If you delete a custom action, it is permanently lost."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Delete"), GTK_RESPONSE_YES, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_YES)
    {
      /* remove the rows from the model */
      for (gint i = n_items - 1; i >= 0; --i)
        xfce_item_list_model_remove (XFCE_ITEM_LIST_MODEL (chooser->uca_model), indexes[i]);

      /* sync the model to persistent storage */
      thunar_uca_editor_save_persistently (GTK_WINDOW (chooser), chooser->uca_model);
    }

  /* cancelled, stop event processing */
  return TRUE;
}



static gboolean
thunar_uca_chooser_edit (ThunarUcaChooser *chooser,
                         gint              index)
{
  GValue   value;
  gboolean status;

  xfce_item_list_model_get_item_value (XFCE_ITEM_LIST_MODEL (chooser->uca_model), index, THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID, &value);
  status = thunar_uca_editor_show (GTK_WINDOW (chooser), g_value_get_string (&value), NULL);
  g_value_reset (&value);

  if (!status)
    return TRUE;

  return FALSE;
}



static void
thunar_uca_chooser_init (ThunarUcaChooser *chooser)
{
  GtkWidget         *label;
  GtkWidget         *tree_view;
  GtkTreeSelection  *selection;
  XfceItemListView  *item_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *cell_renderer;

  chooser->uca_model = thunar_uca_model_get_default ();

  /* dialog */
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("Configure custom actions that will appear in the file manager's context menus and/or toolbar.\n"
                           "These actions are applicable to folders or certain kinds of files.\n"
                           "\n"
                           "Check the <a href=\"https://docs.xfce.org/xfce/thunar/custom-actions\">documentation</a> for a collection of custom action examples."));

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_container_add (thunar_order_editor_get_description_area (THUNAR_ORDER_EDITOR (chooser)), label);
  gtk_widget_show (label);

  g_object_set (chooser, "title", _("Custom Actions"), NULL);

  /* item view */
  item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (chooser));
  xfce_item_list_view_set_label_visibility (item_view, FALSE);
  xfce_item_list_view_set_model (item_view, XFCE_ITEM_LIST_MODEL (chooser->uca_model));

  g_signal_connect_swapped (item_view, "add-item", G_CALLBACK (thunar_uca_chooser_add), chooser);
  g_signal_connect_swapped (item_view, "edit-item", G_CALLBACK (thunar_uca_chooser_edit), chooser);
  g_signal_connect_swapped (item_view, "remove-items", G_CALLBACK (thunar_uca_chooser_remove), chooser);

  /* tree view */
  tree_view = xfce_item_list_view_get_tree_view (item_view);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), XFCE_ITEM_LIST_VIEW_COLUMN_ICON);
  cell_renderer = g_object_get_data (G_OBJECT (column), "renderer");
  g_object_set (cell_renderer,
                "stock-size", GTK_ICON_SIZE_DND,
                "xpad", 2,
                "ypad", 2,
                NULL);
}



static void
thunar_uca_chooser_finalize (GObject *object)
{
  ThunarUcaChooser *chooser = THUNAR_UCA_CHOOSER (object);

  g_object_unref (chooser->uca_model);

  G_OBJECT_CLASS (thunar_uca_chooser_parent_class)->finalize (object);
}



void
thunar_uca_chooser_show (GtkWindow *window)
{
  ThunarUcaChooser *chooser;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  chooser = g_object_new (THUNAR_UCA_TYPE_CHOOSER, NULL);
  thunar_order_editor_show (THUNAR_ORDER_EDITOR (chooser), GTK_WIDGET (window));
}
