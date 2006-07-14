/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdkkeysyms.h>

#include <exo/exo.h>

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-compact-view.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>



static void thunar_preferences_dialog_class_init (ThunarPreferencesDialogClass *klass);
static void thunar_preferences_dialog_init       (ThunarPreferencesDialog      *dialog);
static void thunar_preferences_dialog_finalize   (GObject                      *object);
static void thunar_preferences_dialog_response   (GtkDialog                    *dialog,
                                                  gint                          response);



struct _ThunarPreferencesDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarPreferencesDialog
{
  ThunarAbstractDialog __parent__;

  ThunarPreferences *preferences;
  GtkTooltips       *tooltips;
};



static GObjectClass *thunar_preferences_dialog_parent_class;



GType
thunar_preferences_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPreferencesDialogClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_preferences_dialog_class_init,
        NULL,
        NULL,
        sizeof (ThunarPreferencesDialog),
        0,
        (GInstanceInitFunc) thunar_preferences_dialog_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_TYPE_ABSTRACT_DIALOG, I_("ThunarPreferencesDialog"), &info, 0);
    }

  return type;
}



static gboolean
transform_icon_size_to_index (const GValue *src_value,
                              GValue       *dst_value,
                              gpointer      user_data)
{
  GEnumClass *klass;
  gint        n;

  klass = g_type_class_ref (THUNAR_TYPE_ICON_SIZE);
  for (n = 0; n < klass->n_values; ++n)
    if (klass->values[n].value == g_value_get_enum (src_value))
      g_value_set_int (dst_value, n);
  g_type_class_unref (klass);

  return TRUE;
}



static gboolean
transform_index_to_icon_size (const GValue *src_value,
                              GValue       *dst_value,
                              gpointer      user_data)
{
  GEnumClass *klass;

  klass = g_type_class_ref (THUNAR_TYPE_ICON_SIZE);
  g_value_set_enum (dst_value, klass->values[g_value_get_int (src_value)].value);
  g_type_class_unref (klass);

  return TRUE;
}



static gboolean
transform_view_string_to_index (const GValue *src_value,
                                GValue       *dst_value,
                                gpointer      user_data)
{
  GType type;

  type = g_type_from_name (g_value_get_string (src_value));
  if (type == THUNAR_TYPE_ICON_VIEW)
    g_value_set_int (dst_value, 0);
  else if (type == THUNAR_TYPE_DETAILS_VIEW)
    g_value_set_int (dst_value, 1);
  else if (type == THUNAR_TYPE_COMPACT_VIEW)
    g_value_set_int (dst_value, 2);
  else
    g_value_set_int (dst_value, 3);

  return TRUE;
}



static gboolean
transform_view_index_to_string (const GValue *src_value,
                                GValue       *dst_value,
                                gpointer      user_data)
{
  switch (g_value_get_int (src_value))
    {
    case 0:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_ICON_VIEW));
      break;

    case 1:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_DETAILS_VIEW));
      break;

    case 2:
      g_value_set_static_string (dst_value, g_type_name (THUNAR_TYPE_COMPACT_VIEW));
      break;

    default:
      g_value_set_static_string (dst_value, g_type_name (G_TYPE_NONE));
      break;
    }

  return TRUE;
}



static void
thunar_preferences_dialog_class_init (ThunarPreferencesDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_preferences_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_preferences_dialog_response;
}



static void
thunar_preferences_dialog_init (ThunarPreferencesDialog *dialog)
{
  AtkRelationSet *relations;
  GtkAdjustment  *adjustment;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *notebook;
  GtkWidget      *button;
  GtkWidget      *align;
  GtkWidget      *combo;
  GtkWidget      *frame;
  GtkWidget      *label;
  GtkWidget      *range;
  GtkWidget      *table;
  GtkWidget      *hbox;
  GtkWidget      *ibox;
  GtkWidget      *vbox;

  /* grab a reference on the preferences */
  dialog->preferences = thunar_preferences_get ();

  /* allocate new tooltips for this dialog */
  dialog->tooltips = gtk_tooltips_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (dialog->tooltips));

  /* configure the dialog properties */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("File Manager Preferences"));

  /* add "Help" and "Close" buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                          NULL);

  notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);


  /*
     Views
   */
  label = gtk_label_new (_("Views"));
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 12, "spacing", 12, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Default View"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("View _new folders using:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Icon View"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Detailed List View"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Compact List View"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Last Active View"));
#if !GTK_CHECK_VERSION(2,9,0)
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
#endif
  exo_mutual_binding_new_full (G_OBJECT (dialog->preferences), "default-view", G_OBJECT (combo), "active",
                               transform_view_string_to_index, transform_view_index_to_string, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* set Atk label relation for the combo */
  object = gtk_widget_get_accessible (combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  button = gtk_check_button_new_with_mnemonic (_("Sort _folders before files"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-folders-first", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to list folders before files when you sort a folder."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  button = gtk_check_button_new_with_mnemonic (_("_Show thumbnails"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-show-thumbnails", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to display previewable files within a "
                                                    "folder as automatically generated thumbnail icons."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Icon View"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (1, 1, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  button = gtk_check_button_new_with_mnemonic (_("_Text beside icons"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-text-beside-icons", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to place the icon captions for items "
                                                    "beside the icon rather than below the icon."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);


  /*
     Side Pane
   */
  label = gtk_label_new (_("Side Pane"));
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 12, "spacing", 12, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Shortcuts Pane"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("_Icon Size:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Very Small"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Smaller"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Small"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Normal"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Large"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Larger"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Very Large"));
#if !GTK_CHECK_VERSION(2,9,0)
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
#endif
  exo_mutual_binding_new_full (G_OBJECT (dialog->preferences), "shortcuts-icon-size", G_OBJECT (combo), "active",
                               transform_icon_size_to_index, transform_index_to_icon_size, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* set Atk label relation for the combo */
  object = gtk_widget_get_accessible (combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  button = gtk_check_button_new_with_mnemonic (_("Show Icon _Emblems"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "shortcuts-icon-emblems", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to display icon emblems in the shortcuts pane for all folders "
                                                    "for which emblems have been defined in the folders properties dialog."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Tree Pane"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Icon _Size:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Very Small"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Smaller"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Small"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Normal"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Large"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Larger"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Very Large"));
#if !GTK_CHECK_VERSION(2,9,0)
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
#endif
  exo_mutual_binding_new_full (G_OBJECT (dialog->preferences), "tree-icon-size", G_OBJECT (combo), "active",
                               transform_icon_size_to_index, transform_index_to_icon_size, NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_widget_show (combo);

  /* set Atk label relation for the combo */
  object = gtk_widget_get_accessible (combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  button = gtk_check_button_new_with_mnemonic (_("Show Icon E_mblems"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "tree-icon-emblems", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to display icon emblems in the tree pane for all folders "
                                                    "for which emblems have been defined in the folders properties dialog."), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);


  /*
     Behavior
   */
  label = gtk_label_new (_("Behavior"));
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 12, "spacing", 12, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Navigation"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (3, 1, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  button = gtk_radio_button_new_with_mnemonic (NULL, _("_Single click to activate items"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-single-click", G_OBJECT (button), "active");
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (g_object_notify), "active");
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (button);

  align = gtk_alignment_new (0.0f, 0.0f, 1.0f, 1.0f);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 6, 18, 0);
  exo_binding_new (G_OBJECT (button), "active", G_OBJECT (align), "sensitive");
  gtk_table_attach (GTK_TABLE (table), align, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (align);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (align), hbox);
  gtk_widget_show (hbox);

  ibox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), ibox, FALSE, FALSE, 0);
  gtk_widget_show (ibox);

  label = gtk_label_new_with_mnemonic (_("Specify the d_elay before an item gets selected\n"
                                         "when the mouse pointer is paused over it:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.0f);
  gtk_box_pack_start (GTK_BOX (ibox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  align = g_object_new (GTK_TYPE_ALIGNMENT, "height-request", 6, NULL);
  gtk_box_pack_start (GTK_BOX (ibox), align, FALSE, FALSE, 0);
  gtk_widget_show (align);

  range = gtk_hscale_new_with_range (0.0, 2000.0, 100.0);
  gtk_scale_set_draw_value (GTK_SCALE (range), FALSE);
  gtk_tooltips_set_tip (dialog->tooltips, range, _("When single-click activation is enabled, pausing the mouse pointer over an item "
                                                   "will automatically select that item after the chosen delay. You can disable this "
                                                   "behavior by moving the slider to the left-most position. This behavior may be "
                                                   "useful when single clicks activate items, and you want only to select the item "
                                                   "without activating it."), NULL);
  gtk_box_pack_start (GTK_BOX (ibox), range, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), range);
  gtk_widget_show (range);

  /* connect the range's adjustment to the preferences */
  adjustment = gtk_range_get_adjustment (GTK_RANGE (range));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-single-click-timeout", G_OBJECT (adjustment), "value");

  /* set Atk label relation for the range */
  object = gtk_widget_get_accessible (range);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_box_pack_start (GTK_BOX (ibox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Disabled"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Medium"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.5f, 0.5f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Long"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_small_italic ());
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  button = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button), _("_Double click to activate items"));
  exo_mutual_binding_new_with_negation (G_OBJECT (dialog->preferences), "misc-single-click", G_OBJECT (button), "active");
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (g_object_notify), "active");
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (button);


  /*
     Advanced
   */
  label = gtk_label_new (_("Advanced"));
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 12, "spacing", 12, NULL);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  frame = g_object_new (GTK_TYPE_FRAME, "border-width", 0, "shadow-type", GTK_SHADOW_NONE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (_("Folder Permissions"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("When changing the permissions of a folder, you\n"
                                         "can also apply the changes to the contents of the\n"
                                         "folder. Select the default behavior below:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.0f);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Ask everytime"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Apply to Folder Only"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Apply to Folder and Contents"));
#if !GTK_CHECK_VERSION(2,9,0)
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
#endif
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-recursive-permissions", G_OBJECT (combo), "active");
  gtk_table_attach (GTK_TABLE (table), combo, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  /* set Atk label relation for the combo */
  object = gtk_widget_get_accessible (combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
}



static void
thunar_preferences_dialog_finalize (GObject *object)
{
  ThunarPreferencesDialog *dialog = THUNAR_PREFERENCES_DIALOG (object);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (dialog->preferences));

  /* release our reference on the tooltips */
  g_object_unref (G_OBJECT (dialog->tooltips));

  (*G_OBJECT_CLASS (thunar_preferences_dialog_parent_class)->finalize) (object);
}



static void
thunar_preferences_dialog_response (GtkDialog *dialog,
                                    gint       response)
{
  if (G_UNLIKELY (response == GTK_RESPONSE_HELP))
    {
      // FIXME: Bring up the documentation browser
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}



/**
 * thunar_preferences_dialog_new:
 * @parent : a #GtkWindow or %NULL.
 *
 * Allocates a new #ThunarPreferencesDialog widget.
 *
 * Return value: the newly allocated #ThunarPreferencesDialog.
 **/
GtkWidget*
thunar_preferences_dialog_new (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = g_object_new (THUNAR_TYPE_PREFERENCES_DIALOG, NULL);
  if (G_LIKELY (parent != NULL))
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return dialog;
}

