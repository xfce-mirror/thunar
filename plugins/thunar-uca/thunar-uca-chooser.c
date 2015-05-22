/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <gdk/gdkkeysyms.h>

#include <thunar-uca/thunar-uca-chooser.h>
#include <thunar-uca/thunar-uca-editor.h>
#include <thunar-uca/thunar-uca-model.h>



static gboolean thunar_uca_chooser_key_press_event    (GtkWidget              *widget,
                                                       GdkEventKey            *event);
static void     thunar_uca_chooser_response           (GtkDialog              *dialog,
                                                       gint                    response);
static void     thunar_uca_chooser_exchange           (ThunarUcaChooser       *uca_chooser,
                                                       GtkTreeSelection       *selection,
                                                       GtkTreeModel           *model,
                                                       GtkTreeIter            *iter_a,
                                                       GtkTreeIter            *iter_b);
static void     thunar_uca_chooser_open_editor        (ThunarUcaChooser       *uca_chooser,
                                                       gboolean                edit);
static void     thunar_uca_chooser_save               (ThunarUcaChooser       *uca_chooser,
                                                       ThunarUcaModel         *uca_model);
static void     thunar_uca_chooser_selection_changed  (ThunarUcaChooser       *uca_chooser,
                                                       GtkTreeSelection       *selection);
static void     thunar_uca_chooser_add_clicked        (ThunarUcaChooser       *uca_chooser);
static void     thunar_uca_chooser_edit_clicked       (ThunarUcaChooser       *uca_chooser);
static void     thunar_uca_chooser_delete_clicked     (ThunarUcaChooser       *uca_chooser);
static void     thunar_uca_chooser_up_clicked         (ThunarUcaChooser       *uca_chooser);
static void     thunar_uca_chooser_down_clicked       (ThunarUcaChooser       *uca_chooser);



struct _ThunarUcaChooserClass
{
  GtkDialogClass __parent__;
};

struct _ThunarUcaChooser
{
  GtkDialog __parent__;

  GtkWidget   *treeview;
  GtkWidget   *add_button;
  GtkWidget   *edit_button;
  GtkWidget   *delete_button;
  GtkWidget   *up_button;
  GtkWidget   *down_button;
};



THUNARX_DEFINE_TYPE (ThunarUcaChooser, thunar_uca_chooser, GTK_TYPE_DIALOG);



static void
thunar_uca_chooser_class_init (ThunarUcaChooserClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->key_press_event = thunar_uca_chooser_key_press_event;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_uca_chooser_response;
}



static void
thunar_uca_chooser_init (ThunarUcaChooser *uca_chooser)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  ThunarUcaModel    *uca_model;
  GtkWidget         *image;
  GtkWidget         *label;
  GtkWidget         *hbox;
  GtkWidget         *swin;
  GtkWidget         *vbox;

  /* configure the dialog window */
  gtk_dialog_add_button (GTK_DIALOG (uca_chooser), GTK_STOCK_HELP, GTK_RESPONSE_HELP);
  gtk_dialog_add_button (GTK_DIALOG (uca_chooser), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (uca_chooser), GTK_RESPONSE_CLOSE);
  gtk_dialog_set_has_separator (GTK_DIALOG (uca_chooser), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (uca_chooser), 500, 350);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (uca_chooser), TRUE);
  gtk_window_set_title (GTK_WINDOW (uca_chooser), _("Custom Actions"));

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (uca_chooser)->vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DND);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("You can configure custom actions that will appear in the\n"
                           "file managers context menus for certain kinds of files."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (uca_chooser)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (hbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  uca_chooser->treeview = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (uca_chooser->treeview), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (uca_chooser->treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (swin), uca_chooser->treeview);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->treeview), "row-activated", G_CALLBACK (thunar_uca_chooser_edit_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->treeview);

  uca_model = thunar_uca_model_get_default ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (uca_chooser->treeview), GTK_TREE_MODEL (uca_model));
  g_object_unref (G_OBJECT (uca_model));

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_resizable (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uca_chooser->treeview), column);

  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF, "stock-size", GTK_ICON_SIZE_DND, "xpad", 2, "ypad", 2, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer, "gicon", THUNAR_UCA_MODEL_COLUMN_GICON, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "markup", THUNAR_UCA_MODEL_COLUMN_STOCK_LABEL, NULL);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  uca_chooser->add_button = gtk_button_new ();
  gtk_widget_set_tooltip_text (uca_chooser->add_button, _("Add a new custom action."));
  gtk_box_pack_start (GTK_BOX (vbox), uca_chooser->add_button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->add_button), "clicked", G_CALLBACK (thunar_uca_chooser_add_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->add_button);

  image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (uca_chooser->add_button), image);
  gtk_widget_show (image);

  uca_chooser->edit_button = gtk_button_new ();
  gtk_widget_set_tooltip_text (uca_chooser->edit_button, _("Edit the currently selected action."));
  gtk_box_pack_start (GTK_BOX (vbox), uca_chooser->edit_button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->edit_button), "clicked", G_CALLBACK (thunar_uca_chooser_edit_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->edit_button);

  image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (uca_chooser->edit_button), image);
  gtk_widget_show (image);

  uca_chooser->delete_button = gtk_button_new ();
  gtk_widget_set_tooltip_text (uca_chooser->delete_button, _("Delete the currently selected action."));
  gtk_box_pack_start (GTK_BOX (vbox), uca_chooser->delete_button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->delete_button), "clicked", G_CALLBACK (thunar_uca_chooser_delete_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->delete_button);

  image = gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (uca_chooser->delete_button), image);
  gtk_widget_show (image);

  uca_chooser->up_button = gtk_button_new ();
  gtk_widget_set_tooltip_text (uca_chooser->up_button, _("Move the currently selected action up by one row."));
  gtk_box_pack_start (GTK_BOX (vbox), uca_chooser->up_button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->up_button), "clicked", G_CALLBACK (thunar_uca_chooser_up_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->up_button);

  image = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (uca_chooser->up_button), image);
  gtk_widget_show (image);

  uca_chooser->down_button = gtk_button_new ();
  gtk_widget_set_tooltip_text (uca_chooser->down_button, _("Move the currently selected action down by one row."));
  gtk_box_pack_start (GTK_BOX (vbox), uca_chooser->down_button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (uca_chooser->down_button), "clicked", G_CALLBACK (thunar_uca_chooser_down_clicked), uca_chooser);
  gtk_widget_show (uca_chooser->down_button);

  image = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (uca_chooser->down_button), image);
  gtk_widget_show (image);

  /* configure the tree view selection after the buttons have been created */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (uca_chooser->treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_uca_chooser_selection_changed), uca_chooser);
  thunar_uca_chooser_selection_changed (uca_chooser, selection);
}



static gboolean
thunar_uca_chooser_key_press_event (GtkWidget   *widget,
                                    GdkEventKey *event)
{
  /* close chooser window on Esc key press */
  if (G_UNLIKELY (event->keyval == GDK_Escape))
    {
      gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_CLOSE);
      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_uca_chooser_parent_class)->key_press_event) (widget, event);
}



static void
thunar_uca_chooser_response (GtkDialog *dialog,
                             gint       response)
{
  if (response == GTK_RESPONSE_CLOSE)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else if (GTK_DIALOG_CLASS (thunar_uca_chooser_parent_class)->response != NULL)
    {
      (*GTK_DIALOG_CLASS (thunar_uca_chooser_parent_class)->response) (dialog, response);
    }
}



static void
thunar_uca_chooser_selection_changed (ThunarUcaChooser *uca_chooser,
                                      GtkTreeSelection *selection)
{
  GtkTreeModel *model;
  GtkTreePath  *path = NULL;
  GtkTreeIter   iter;
  gboolean      selected;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  /* check if we have currently selected an item */
  selected = gtk_tree_selection_get_selected (selection, &model, &iter);
  if (G_LIKELY (selected))
    {
      /* determine the path for the selected iter */
      path = gtk_tree_model_get_path (model, &iter);
    }

  /* change sensitivity of "Edit" and "Delete" appropritately */
  gtk_widget_set_sensitive (uca_chooser->edit_button, selected);
  gtk_widget_set_sensitive (uca_chooser->delete_button, selected);

  /* change sensitivity of "Move Up" and "Move Down" appropritately */
  gtk_widget_set_sensitive (uca_chooser->up_button, selected && gtk_tree_path_get_indices (path)[0] > 0);
  gtk_widget_set_sensitive (uca_chooser->down_button, selected && gtk_tree_path_get_indices (path)[0] < gtk_tree_model_iter_n_children (model, NULL) - 1);

  /* release path (if any) */
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);
}



static void
thunar_uca_chooser_exchange (ThunarUcaChooser *uca_chooser,
                             GtkTreeSelection *selection,
                             GtkTreeModel     *model,
                             GtkTreeIter      *iter_a,
                             GtkTreeIter      *iter_b)
{
  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));
  g_return_if_fail (iter_a != NULL);
  g_return_if_fail (iter_b != NULL);

  /* perform the move */
  thunar_uca_model_exchange (THUNAR_UCA_MODEL (model), iter_a, iter_b);

  /* tell the chooser that the selection may have changed */
  thunar_uca_chooser_selection_changed (uca_chooser, selection);

  /* sync the model to persistent storage */
  thunar_uca_chooser_save (uca_chooser, THUNAR_UCA_MODEL (model));
}



static void
thunar_uca_chooser_open_editor (ThunarUcaChooser *uca_chooser,
                                gboolean          edit)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *editor;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));

  /* allocate the new editor */
  editor = g_object_new (THUNAR_UCA_TYPE_EDITOR, NULL);
  gtk_window_set_title (GTK_WINDOW (editor), edit ? _("Edit Action") : _("Create Action"));
  gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (uca_chooser));

  /* load the editor with the currently selected item (when editing) */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (uca_chooser->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter) && edit)
    thunar_uca_editor_load (THUNAR_UCA_EDITOR (editor), THUNAR_UCA_MODEL (model), &iter);

  /* run the editor */
  if (gtk_dialog_run (GTK_DIALOG (editor)) == GTK_RESPONSE_OK)
    {
      /* append a new iter (when not editing) */
      if (G_UNLIKELY (!edit))
        thunar_uca_model_append (THUNAR_UCA_MODEL (model), &iter);

      /* save the editor values to the model */
      thunar_uca_editor_save (THUNAR_UCA_EDITOR (editor), THUNAR_UCA_MODEL (model), &iter);

      /* hide the editor window */
      gtk_widget_hide (editor);

      /* sync the model to persistent storage */
      thunar_uca_chooser_save (uca_chooser, THUNAR_UCA_MODEL (model));
    }

  /* destroy the editor */
  gtk_widget_destroy (editor);
}



static void
thunar_uca_chooser_save (ThunarUcaChooser *uca_chooser,
                         ThunarUcaModel   *uca_model)
{
  GtkWidget *dialog;
  GError    *error = NULL;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));
  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));

  /* sync the model to persistent storage */
  if (!thunar_uca_model_save (uca_model, &error))
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (uca_chooser),
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to save actions to disk."));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
}



static void
thunar_uca_chooser_add_clicked (ThunarUcaChooser *uca_chooser)
{
  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));
  thunar_uca_chooser_open_editor (uca_chooser, FALSE);
}



static void
thunar_uca_chooser_edit_clicked (ThunarUcaChooser *uca_chooser)
{
  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));
  thunar_uca_chooser_open_editor (uca_chooser, TRUE);
}



static void
thunar_uca_chooser_delete_clicked (ThunarUcaChooser *uca_chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *name;
  GtkWidget        *dialog;
  gint              response;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));

  /* verify that we have an item selected and determine the iter for that item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (uca_chooser->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* create the question dialog */
      gtk_tree_model_get (model, &iter, THUNAR_UCA_MODEL_COLUMN_NAME, &name, -1);
      dialog = gtk_message_dialog_new (GTK_WINDOW (uca_chooser),
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_NONE,
                                       _("Are you sure that you want to delete\naction \"%s\"?"), name);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("If you delete a custom action, it is permanently lost."));
      gtk_dialog_add_buttons (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_DELETE, GTK_RESPONSE_YES, NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
      g_free (name);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_YES)
        {
          /* remove the row from the model */
          thunar_uca_model_remove (THUNAR_UCA_MODEL (model), &iter);

          /* sync the model to persistent storage */
          thunar_uca_chooser_save (uca_chooser, THUNAR_UCA_MODEL (model));
        }
    }
}



static void
thunar_uca_chooser_up_clicked (ThunarUcaChooser *uca_chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter_a;
  GtkTreeIter       iter_b;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));

  /* determine the currently selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (uca_chooser->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter_a))
    {
      /* determine the tree path to the iter */
      path = gtk_tree_model_get_path (model, &iter_a);
      if (gtk_tree_path_prev (path))
        {
          /* determine the iter for the previous item */
          gtk_tree_model_get_iter (model, &iter_b, path);

          /* perform the exchange operation */
          thunar_uca_chooser_exchange (uca_chooser, selection, model, &iter_a, &iter_b);
        }

      /* release the path */
      gtk_tree_path_free (path);
    }
}



static void
thunar_uca_chooser_down_clicked (ThunarUcaChooser *uca_chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter_a;
  GtkTreeIter       iter_b;

  g_return_if_fail (THUNAR_UCA_IS_CHOOSER (uca_chooser));

  /* determine the currently selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (uca_chooser->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter_a))
    {
      /* determine the iter to the next item */
      iter_b = iter_a;
      if (gtk_tree_model_iter_next (model, &iter_b))
        thunar_uca_chooser_exchange (uca_chooser, selection, model, &iter_a, &iter_b);
    }
}




