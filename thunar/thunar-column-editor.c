/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-column-editor.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>



static void thunar_column_editor_finalize           (GObject                  *object);
static void thunar_column_editor_help_clicked       (GtkWidget                *button,
                                                     ThunarColumnEditor       *column_editor);
static void thunar_column_editor_move_down          (GtkWidget                *button,
                                                     ThunarColumnEditor       *column_editor);
static void thunar_column_editor_move_up            (GtkWidget                *button,
                                                     ThunarColumnEditor       *column_editor);
static void thunar_column_editor_toggled            (GtkCellRendererToggle    *cell_renderer,
                                                     const gchar              *path_string,
                                                     ThunarColumnEditor       *column_editor);
static void thunar_column_editor_toggle_visibility  (GtkWidget                *button,
                                                     ThunarColumnEditor       *column_editor);
static void thunar_column_editor_update_buttons     (ThunarColumnEditor       *column_editor);
static void thunar_column_editor_use_defaults       (GtkWidget                *button,
                                                     ThunarColumnEditor       *column_editor);



struct _ThunarColumnEditorClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarColumnEditor
{
  ThunarAbstractDialog __parent__;

  ThunarPreferences *preferences;

  ThunarColumnModel *column_model;

  GtkWidget         *tree_view;
  GtkWidget         *up_button;
  GtkWidget         *down_button;
  GtkWidget         *show_button;
  GtkWidget         *hide_button;
};



G_DEFINE_TYPE (ThunarColumnEditor, thunar_column_editor, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_column_editor_class_init (ThunarColumnEditorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_column_editor_finalize;
}



static void
thunar_column_editor_init (ThunarColumnEditor *column_editor)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkTreeIter        iter;
  GtkWidget         *separator;
  GtkWidget         *button;
  GtkWidget         *frame;
  GtkWidget         *image;
  GtkWidget         *label;
  GtkWidget         *table;
  GtkWidget         *vbox;
  GtkWidget         *swin;

  /* grab a reference on the preferences */
  column_editor->preferences = thunar_preferences_get ();

  /* grab a reference on the shared column model */
  column_editor->column_model = thunar_column_model_get_default ();
  g_signal_connect_data (G_OBJECT (column_editor->column_model), "row-changed", G_CALLBACK (thunar_column_editor_update_buttons),
                         column_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_data (G_OBJECT (column_editor->column_model), "rows-reordered", G_CALLBACK (thunar_column_editor_update_buttons),
                         column_editor, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  /* setup the dialog */
  gtk_dialog_add_button (GTK_DIALOG (column_editor), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (column_editor), GTK_RESPONSE_CLOSE);
  gtk_window_set_resizable (GTK_WINDOW (column_editor), FALSE);
  gtk_window_set_title (GTK_WINDOW (column_editor), _("Configure Columns in the Detailed List View"));

  /* add the "Help" button */
  button = gtk_button_new_from_stock (GTK_STOCK_HELP);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_column_editor_help_clicked), column_editor);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (column_editor)->action_area), button, FALSE, FALSE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (column_editor)->action_area), button, TRUE);
  gtk_widget_show (button);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (column_editor)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Visible Columns"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (8, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  /* create the top label for the column editor dialog */
  label = gtk_label_new (_("Choose the order of information to appear in the\ndetailed list view."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table), swin, 0, 1, 1, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  column_editor->tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (column_editor->tree_view), FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (column_editor->tree_view), GTK_TREE_MODEL (column_editor->column_model));
  gtk_container_add (GTK_CONTAINER (swin), column_editor->tree_view);
  gtk_widget_show (column_editor->tree_view);

  /* append the toggle column */
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (thunar_column_editor_toggled), column_editor);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "active", THUNAR_COLUMN_MODEL_COLUMN_VISIBLE,
                                       "activatable", THUNAR_COLUMN_MODEL_COLUMN_MUTABLE,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (column_editor->tree_view), column);

  /* append the name column */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_COLUMN_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (column_editor->tree_view), column);

  /* create the "Move Up" button */
  column_editor->up_button = gtk_button_new_with_mnemonic (_("Move _Up"));
  g_signal_connect (G_OBJECT (column_editor->up_button), "clicked", G_CALLBACK (thunar_column_editor_move_up), column_editor);
  gtk_table_attach (GTK_TABLE (table), column_editor->up_button, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (column_editor->up_button);

  image = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (column_editor->up_button), image);
  gtk_widget_show (image);

  /* create the "Move Down" button */
  column_editor->down_button = gtk_button_new_with_mnemonic (_("Move Dow_n"));
  g_signal_connect (G_OBJECT (column_editor->down_button), "clicked", G_CALLBACK (thunar_column_editor_move_down), column_editor);
  gtk_table_attach (GTK_TABLE (table), column_editor->down_button, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_widget_show (column_editor->down_button);

  image = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (column_editor->down_button), image);
  gtk_widget_show (image);

  /* create the "Show" button */
  column_editor->show_button = gtk_button_new_with_mnemonic (_("_Show"));
  g_signal_connect (G_OBJECT (column_editor->show_button), "clicked", G_CALLBACK (thunar_column_editor_toggle_visibility), column_editor);
  gtk_table_attach (GTK_TABLE (table), column_editor->show_button, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);
  gtk_widget_show (column_editor->show_button);

  /* create the "Hide" button */
  column_editor->hide_button = gtk_button_new_with_mnemonic (_("Hi_de"));
  g_signal_connect (G_OBJECT (column_editor->hide_button), "clicked", G_CALLBACK (thunar_column_editor_toggle_visibility), column_editor);
  gtk_table_attach (GTK_TABLE (table), column_editor->hide_button, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);
  gtk_widget_show (column_editor->hide_button);

  /* create the horiz separator */
  separator = gtk_hseparator_new ();
  gtk_table_attach (GTK_TABLE (table), separator, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);
  gtk_widget_show (separator);

  /* create the "Use Default" button */
  button = gtk_button_new_with_mnemonic (_("Use De_fault"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_column_editor_use_defaults), column_editor);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 6, 7, GTK_FILL, 0, 0, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Column Sizing"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (2, 1, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  /* create the label that explains the column sizing option */
  label = gtk_label_new (_("By default columns will be automatically expanded if\n"
                           "needed to ensure the text is fully visible. If you dis-\n"
                           "able this behavior below the file manager will always\n"
                           "use the user defined column widths."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* create the "Automatically expand columns as needed" button */
  button = gtk_check_button_new_with_mnemonic (_("Automatically _expand columns as needed"));
  exo_mutual_binding_new_with_negation (G_OBJECT (column_editor->preferences), "last-details-view-fixed-columns", G_OBJECT (button), "active");
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), button);
  gtk_widget_show (button);

  /* setup the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_column_editor_update_buttons), column_editor);

  /* select the first item */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (column_editor->column_model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  /* grab focus to the tree view */
  gtk_widget_grab_focus (column_editor->tree_view);
}



static void
thunar_column_editor_finalize (GObject *object)
{
  ThunarColumnEditor *column_editor = THUNAR_COLUMN_EDITOR (object);

  /* release our reference on the shared column model */
  g_signal_handlers_disconnect_matched (G_OBJECT (column_editor->column_model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, column_editor);
  g_object_unref (G_OBJECT (column_editor->column_model));

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (column_editor->preferences));

  (*G_OBJECT_CLASS (thunar_column_editor_parent_class)->finalize) (object);
}



static void
thunar_column_editor_help_clicked (GtkWidget          *button,
                                   ThunarColumnEditor *column_editor)
{
  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* open the user manual */
  xfce_dialog_show_help (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                         "thunar",
                         "the-file-manager-window",
                         "customizing_the_appearance");
}



static void
thunar_column_editor_move_down (GtkWidget          *button,
                                ThunarColumnEditor *column_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter1))
    {
      /* copy the iterator */
      iter2 = iter1;

      /* determine the next iterator and exchange the rows */
      if (gtk_tree_model_iter_next (model, &iter2))
        thunar_column_model_exchange (THUNAR_COLUMN_MODEL (model), &iter1, &iter2);
    }
}



static void
thunar_column_editor_move_up (GtkWidget          *button,
                              ThunarColumnEditor *column_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter1;
  GtkTreeIter       iter2;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter1))
    {
      /* determine the path for the iterator */
      path = gtk_tree_model_get_path (model, &iter1);
      if (G_LIKELY (path != NULL))
        {
          /* advance to the prev path */
          if (gtk_tree_path_prev (path) && gtk_tree_model_get_iter (model, &iter2, path))
            {
              /* exchange the rows */
              thunar_column_model_exchange (THUNAR_COLUMN_MODEL (model), &iter1, &iter2);
            }

          /* release the path */
          gtk_tree_path_free (path);
        }
    }
}



static void
thunar_column_editor_toggled (GtkCellRendererToggle *cell_renderer,
                              const gchar           *path_string,
                              ThunarColumnEditor    *column_editor)
{
  ThunarColumn column;
  GtkTreePath *path;
  GtkTreeIter  iter;
  gboolean     visible;

  _thunar_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (cell_renderer));
  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (path_string != NULL);

  /* determine the tree path for the string */
  path = gtk_tree_path_new_from_string (path_string);
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (column_editor->column_model), &iter, path))
    {
      /* determine the column for the iterator... */
      column = thunar_column_model_get_column_for_iter (column_editor->column_model, &iter);

      /* ...determine the existing visbility setting... */
      visible = thunar_column_model_get_column_visible (column_editor->column_model, column);

      /* ...and change the visibility of the column */
      thunar_column_model_set_column_visible (column_editor->column_model, column, !visible);
    }
  gtk_tree_path_free (path);
}



static void
thunar_column_editor_toggle_visibility (GtkWidget          *button,
                                        ThunarColumnEditor *column_editor)
{
  GtkTreeSelection *selection;
  ThunarColumn      column;
  GtkTreeIter       iter;
  gboolean          visible;

  _thunar_return_if_fail (THUNAR_IS_COLUMN_EDITOR (column_editor));
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* determine the selected tree iterator */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      /* determine the column for the iterator... */
      column = thunar_column_model_get_column_for_iter (column_editor->column_model, &iter);

      /* ...determine the existing visbility setting... */
      visible = thunar_column_model_get_column_visible (column_editor->column_model, column);

      /* ...and change the visibility of the column */
      thunar_column_model_set_column_visible (column_editor->column_model, column, !visible);
    }
}



static void
thunar_column_editor_update_buttons (ThunarColumnEditor *column_editor)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  gboolean          mutable;
  gboolean          visible;
  gint              idx;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the tree path for the iter */
      path = gtk_tree_model_get_path (model, &iter);
      if (G_UNLIKELY (path == NULL))
        return;

      /* update the "Move Up"/"Move Down" buttons */
      idx = gtk_tree_path_get_indices (path)[0];
      gtk_widget_set_sensitive (column_editor->up_button, (idx > 0));
      gtk_widget_set_sensitive (column_editor->down_button, (idx + 1 < gtk_tree_model_iter_n_children (model, NULL)));

      /* update the "Show"/"Hide" buttons */
      gtk_tree_model_get (model, &iter, THUNAR_COLUMN_MODEL_COLUMN_MUTABLE, &mutable, THUNAR_COLUMN_MODEL_COLUMN_VISIBLE, &visible, -1);
      gtk_widget_set_sensitive (column_editor->show_button, mutable && !visible);
      gtk_widget_set_sensitive (column_editor->hide_button, mutable && visible);

      /* release the path */
      gtk_tree_path_free (path);
    }
  else
    {
      /* just disable all buttons */
      gtk_widget_set_sensitive (column_editor->hide_button, FALSE);
      gtk_widget_set_sensitive (column_editor->show_button, FALSE);
      gtk_widget_set_sensitive (column_editor->down_button, FALSE);
      gtk_widget_set_sensitive (column_editor->up_button, FALSE);
    }
}



static void
thunar_column_editor_use_defaults (GtkWidget          *button,
                                   ThunarColumnEditor *column_editor)
{
  static const gchar *PROPERTY_NAMES[] =
  {
    "last-details-view-column-order",
    "last-details-view-visible-columns",
  };

  GtkTreeSelection *selection;
  GParamSpec       *pspec;
  GValue            value = { 0, };
  guint             n;

  /* reset the given properties to its default values */
  for (n = 0; n < G_N_ELEMENTS (PROPERTY_NAMES); ++n)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (column_editor->preferences), PROPERTY_NAMES[n]);
      g_value_init (&value, pspec->value_type);
      g_param_value_set_default (pspec, &value);
      g_object_set_property (G_OBJECT (column_editor->preferences), PROPERTY_NAMES[n], &value);
      g_value_unset (&value);
    }

  /* reset the tree view selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (column_editor->tree_view));
  gtk_tree_selection_unselect_all (selection);
}



/**
 * thunar_show_column_editor:
 * @parent : the #GtkWidget the #GdkScreen on which to open the
 *           column editor dialog. May also be %NULL in which case
 *           the default #GdkScreen will be used.
 *
 * Convenience function to display a #ThunarColumnEditor.
 *
 * If @parent is a #GtkWidget the editor dialog will be opened as
 * modal dialog above the @parent. Else if @parent is a screen (if
 * @parent is %NULL the default screen is used), the dialog won't
 * be modal and it will simply popup on the specified screen.
 **/
void
thunar_show_column_editor (gpointer parent)
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

  /* display the column editor */
  dialog = g_object_new (THUNAR_TYPE_COLUMN_EDITOR, NULL);

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

