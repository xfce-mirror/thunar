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

#include <thunar/thunar-details-view.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-preferences.h>



static void     thunar_preferences_dialog_class_init      (ThunarPreferencesDialogClass *klass);
static void     thunar_preferences_dialog_init            (ThunarPreferencesDialog      *dialog);
static void     thunar_preferences_dialog_finalize        (GObject                      *object);
static gboolean thunar_preferences_dialog_key_press_event (GtkWidget                    *widget,
                                                           GdkEventKey                  *event);
static void     thunar_preferences_dialog_response        (GtkDialog                    *dialog,
                                                           gint                          response);



struct _ThunarPreferencesDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarPreferencesDialog
{
  GtkDialog __parent__;

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

      type = g_type_register_static (GTK_TYPE_DIALOG, I_("ThunarPreferencesDialog"), &info, 0);
    }

  return type;
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
  else
    g_value_set_int (dst_value, 2);

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
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_preferences_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_dialog_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->key_press_event = thunar_preferences_dialog_key_press_event;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_preferences_dialog_response;
}



static void
thunar_preferences_dialog_init (ThunarPreferencesDialog *dialog)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *notebook;
  GtkWidget      *button;
  GtkWidget      *combo;
  GtkWidget      *frame;
  GtkWidget      *label;
  GtkWidget      *table;
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

  button = gtk_check_button_new_with_mnemonic (_("Show hidden and _backup files"));
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "default-show-hidden", G_OBJECT (button), "active");
  gtk_tooltips_set_tip (dialog->tooltips, button, _("Select this option to show hidden and backup files in new windows. "
                                                    "The first character in a hidden filename is a period (.). The last "
                                                    "character in a backup filename is a tilde (~)."), NULL);
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

  label = gtk_label_new (_("Miscellaneous"));
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_widget_show (label);

  table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Apply permissions _recursively:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Ask everytime"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Always"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Never"));
#if !GTK_CHECK_VERSION(2,9,0)
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (g_object_notify), "active");
#endif
  exo_mutual_binding_new (G_OBJECT (dialog->preferences), "misc-recursive-permissions", G_OBJECT (combo), "active");
  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
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



static gboolean
thunar_preferences_dialog_key_press_event (GtkWidget   *widget,
                                           GdkEventKey *event)
{
  /* close the dialog on Esc */
  if (G_UNLIKELY (event->keyval == GDK_Escape))
    {
      gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_CLOSE);
      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_preferences_dialog_parent_class)->key_press_event) (widget, event);
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

