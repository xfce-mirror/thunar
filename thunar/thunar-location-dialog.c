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

#include <thunar/thunar-location-dialog.h>
#include <thunar/thunar-path-entry.h>



static void thunar_location_dialog_class_init (ThunarLocationDialogClass *klass);
static void thunar_location_dialog_init       (ThunarLocationDialog      *location_dialog);



struct _ThunarLocationDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarLocationDialog
{
  GtkDialog __parent__;

  GtkWidget *entry;
};



G_DEFINE_TYPE (ThunarLocationDialog, thunar_location_dialog, GTK_TYPE_DIALOG);



static void
thunar_location_dialog_class_init (ThunarLocationDialogClass *klass)
{
}



static gboolean
transform_object_to_boolean (const GValue *src_value,
                             GValue       *dst_value,
                             gpointer      user_data)
{
  g_value_set_boolean (dst_value, (g_value_get_object (src_value) != NULL));
  return TRUE;
}



static void
thunar_location_dialog_init (ThunarLocationDialog *location_dialog)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *cancel_button;
  GtkWidget      *open_button;
  GtkWidget      *hbox;
  GtkWidget      *label;

  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (location_dialog)->vbox), 2);
  gtk_container_set_border_width (GTK_CONTAINER (location_dialog), 5);
  gtk_dialog_set_has_separator (GTK_DIALOG (location_dialog), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (location_dialog), 350, -1);
  gtk_window_set_title (GTK_WINDOW (location_dialog), _("Open Location"));

  cancel_button = gtk_dialog_add_button (GTK_DIALOG (location_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  open_button = gtk_dialog_add_button (GTK_DIALOG (location_dialog), GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT);
  gtk_window_set_default (GTK_WINDOW (location_dialog), open_button);

  hbox = g_object_new (GTK_TYPE_HBOX,
                       "border-width", 5,
                       "spacing", 12,
                       NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (location_dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Location:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  location_dialog->entry = thunar_path_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), location_dialog->entry);
  gtk_entry_set_activates_default (GTK_ENTRY (location_dialog->entry), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), location_dialog->entry, TRUE, TRUE, 0);
  gtk_widget_show (location_dialog->entry);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (location_dialog->entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  /* the "Open" button is only sensitive if a valid file is entered */
  exo_binding_new_full (G_OBJECT (location_dialog->entry), "current-file",
                        G_OBJECT (open_button), "sensitive",
                        transform_object_to_boolean, NULL, NULL);
}



/**
 * thunar_location_dialog_new:
 * 
 * Allocates a new #ThunarLocationDialog instance.
 *
 * Return value: the newly allocated #ThunarLocationDialog.
 **/
GtkWidget*
thunar_location_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_LOCATION_DIALOG, NULL);
}



/**
 * thunar_location_dialog_get_selected_file:
 * @location_dialog : a #ThunarLocationDialog.
 *
 * Returns the file selected for the @dialog or
 * %NULL if the file entered is not valid.
 *
 * Return value: the selected #ThunarFile or %NULL.
 **/
ThunarFile*
thunar_location_dialog_get_selected_file (ThunarLocationDialog *location_dialog)
{
  g_return_val_if_fail (THUNAR_IS_LOCATION_DIALOG (location_dialog), NULL);
  return thunar_path_entry_get_current_file (THUNAR_PATH_ENTRY (location_dialog->entry));
}



/**
 * thunar_location_dialog_set_selected_file:
 * @location_dialog : a #ThunarLocationDialog.
 * @selected_file   : a #ThunarFile or %NULL.
 *
 * Sets the file for @location_dialog to @selected_file.
 **/
void
thunar_location_dialog_set_selected_file (ThunarLocationDialog *location_dialog,
                                          ThunarFile           *selected_file)
{
  g_return_if_fail (THUNAR_IS_LOCATION_DIALOG (location_dialog));
  g_return_if_fail (selected_file == NULL || THUNAR_IS_FILE (selected_file));
  thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (location_dialog->entry), selected_file);
}


