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

#include "thunar/thunar-column-order-editor.h"

#include "thunar/thunar-column-order-model.h"
#include "thunar/thunar-enum-types.h"
#include "thunar/thunar-gtk-extensions.h"
#include "thunar/thunar-pango-extensions.h"
#include "thunar/thunar-preferences.h"

#include <libxfce4ui/libxfce4ui.h>

struct _ThunarColumnOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarColumnOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarPreferences *preferences;
};



static void
thunar_column_order_editor_finalize (GObject *object);

static void
thunar_column_order_editor_help (ThunarColumnOrderEditor *column_editor);



G_DEFINE_TYPE (ThunarColumnOrderEditor, thunar_column_order_editor, THUNAR_TYPE_ORDER_EDITOR)



static void
thunar_column_order_editor_class_init (ThunarColumnOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_column_order_editor_finalize;
}



static void
thunar_column_order_editor_init (ThunarColumnOrderEditor *column_editor)
{
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *button;
  GtkWidget *combo;
  GtkWidget *vbox;
  gint       row;

  column_editor->preferences = thunar_preferences_get ();

  /* description area */
  label = gtk_label_new (_("Choose the order of information to appear in the\ndetailed list view."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_container_add (thunar_order_editor_get_description_area (THUNAR_ORDER_EDITOR (column_editor)), label);
  gtk_widget_show (label);

  /* settings area */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_container_add (thunar_order_editor_get_settings_area (THUNAR_ORDER_EDITOR (column_editor)), vbox);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Column Sizing"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  /* new grid */
  row = 0;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* create the label that explains the column sizing option */
  label = gtk_label_new (_("By default columns will be automatically expanded if\n"
                           "needed to ensure the text is fully visible. If you dis-\n"
                           "able this behavior below the file manager will always\n"
                           "use the user defined column widths."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* next row */
  row++;

  /* create the "Automatically expand columns as needed" button */
  button = gtk_check_button_new_with_mnemonic (_("Automatically _expand columns as needed"));
  g_object_bind_property (G_OBJECT (column_editor->preferences),
                          "last-details-view-fixed-columns",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, row, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), button);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Size Column of Folders"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_container_add (GTK_CONTAINER (frame), grid);
  gtk_widget_show (grid);

  /* explain what it does */
  label = gtk_label_new (_("Show number of contained items:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  GEnumClass *enum_class = g_type_class_ref (THUNAR_TYPE_FOLDER_ITEM_COUNT);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_NEVER)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL)->value_nick));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(g_enum_get_value (enum_class, THUNAR_FOLDER_ITEM_COUNT_ALWAYS)->value_nick));
  g_type_class_unref (enum_class);

  g_object_bind_property_full (G_OBJECT (column_editor->preferences), "misc-folder-item-count",
                               G_OBJECT (combo), "active",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               transform_enum_value_to_index,
                               transform_index_to_enum_value,
                               (gpointer) thunar_folder_item_count_get_type, NULL);

  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row - 1, 1, 1);
  thunar_gtk_label_set_a11y_relation (GTK_LABEL (label), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* properties & signals */
  g_signal_connect (column_editor, "help", G_CALLBACK (thunar_column_order_editor_help), NULL);
  g_object_set (column_editor,
                "title", _("Configure Columns in the Detailed List View"),
                "help-enabled", TRUE,
                NULL);
}



static void
thunar_column_order_editor_finalize (GObject *object)
{
  ThunarColumnOrderEditor *column_editor = THUNAR_COLUMN_ORDER_EDITOR (object);

  g_clear_object (&column_editor->preferences);

  G_OBJECT_CLASS (thunar_column_order_editor_parent_class)->finalize (object);
}



static void
thunar_column_order_editor_help (ThunarColumnOrderEditor *column_editor)
{
  /* open the user manual */
  xfce_dialog_show_help (GTK_WINDOW (column_editor),
                         "thunar",
                         "the-file-manager-window",
                         "customizing_the_appearance");
}



void
thunar_column_order_editor_show (GtkWidget *window)
{
  XfceItemListModel       *model = thunar_column_order_model_new ();
  ThunarColumnOrderEditor *column_editor = g_object_new (THUNAR_TYPE_COLUMN_ORDER_EDITOR, "model", model, NULL);

  g_object_unref (model);
  thunar_order_editor_show (THUNAR_ORDER_EDITOR (column_editor), window);
}
