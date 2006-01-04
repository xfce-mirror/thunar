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

#include <thunar/thunar-advanced-permissions-dialog.h>
#include <thunar/thunar-dialogs.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void thunar_advanced_permissions_dialog_class_init   (ThunarAdvancedPermissionsDialogClass *klass);
static void thunar_advanced_permissions_dialog_init         (ThunarAdvancedPermissionsDialog      *advanced_permissions_dialog);
static void thunar_advanced_permissions_dialog_finalize     (GObject                              *object);
static void thunar_advanced_permissions_dialog_get_property (GObject                              *object,
                                                             guint                                 prop_id,
                                                             GValue                               *value,
                                                             GParamSpec                           *pspec);
static void thunar_advanced_permissions_dialog_set_property (GObject                              *object,
                                                             guint                                 prop_id,
                                                             const GValue                         *value,
                                                             GParamSpec                           *pspec);
static void thunar_advanced_permissions_dialog_response     (GtkDialog                            *dialog,
                                                             gint                                  response);
static void thunar_advanced_permissions_dialog_file_changed (ThunarAdvancedPermissionsDialog      *advanced_permissions_dialog,
                                                             ThunarFile                           *file);



struct _ThunarAdvancedPermissionsDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarAdvancedPermissionsDialog
{
  GtkDialog __parent__;

  ThunarFile *file;

  GtkWidget  *suid_button;
  GtkWidget  *sgid_button;
  GtkWidget  *sticky_button;
};



static GObjectClass *thunar_advanced_permissions_dialog_parent_class;



GType
thunar_advanced_permissions_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarAdvancedPermissionsDialogClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_advanced_permissions_dialog_class_init,
        NULL,
        NULL,
        sizeof (ThunarAdvancedPermissionsDialog),
        0,
        (GInstanceInitFunc) thunar_advanced_permissions_dialog_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_DIALOG, I_("ThunarAdvancedPermissionsDialog"), &info, 0);
    }

  return type;
}



static void
thunar_advanced_permissions_dialog_class_init (ThunarAdvancedPermissionsDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_advanced_permissions_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_advanced_permissions_dialog_finalize;
  gobject_class->get_property = thunar_advanced_permissions_dialog_get_property;
  gobject_class->set_property = thunar_advanced_permissions_dialog_set_property;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_advanced_permissions_dialog_response;

  /**
   * ThunarAdvancedPermissionsDialog:file:
   *
   * The #ThunarFile whose advanced permissions should be edited
   * by this dialog.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_advanced_permissions_dialog_init (ThunarAdvancedPermissionsDialog *advanced_permissions_dialog)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *notebook;
  GtkWidget      *image;
  GtkWidget      *label;
  GtkWidget      *table;
  GtkWidget      *vbox;

  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (advanced_permissions_dialog)->vbox), 2);
  gtk_dialog_add_button (GTK_DIALOG (advanced_permissions_dialog), GTK_STOCK_HELP, GTK_RESPONSE_HELP);
  gtk_dialog_add_button (GTK_DIALOG (advanced_permissions_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (advanced_permissions_dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (advanced_permissions_dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_has_separator (GTK_DIALOG (advanced_permissions_dialog), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (advanced_permissions_dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (advanced_permissions_dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (advanced_permissions_dialog), _("Advanced Permissions"));

  notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (advanced_permissions_dialog)->vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);

  vbox = gtk_vbox_new (FALSE, 32);
  label = gtk_label_new (_("Special Bits"));
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
  gtk_widget_show (label);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 0);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  advanced_permissions_dialog->suid_button = gtk_check_button_new_with_mnemonic (_("Set _User ID (SUID)"));
  gtk_table_attach (GTK_TABLE (table), advanced_permissions_dialog->suid_button, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (advanced_permissions_dialog->suid_button);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_misc_set_padding (GTK_MISC (image), 3, 3);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("An executable file whose Set User ID (SUID) bit is\n"
                           "set will always be executed with the privileges of\n"
                           "the owner of the executable."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* set Atk label relation */
  object = gtk_widget_get_accessible (advanced_permissions_dialog->suid_button);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 0);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  advanced_permissions_dialog->sgid_button = gtk_check_button_new_with_mnemonic (_("Set _Group ID (SGID)"));
  gtk_table_attach (GTK_TABLE (table), advanced_permissions_dialog->sgid_button, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (advanced_permissions_dialog->sgid_button);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_misc_set_padding (GTK_MISC (image), 3, 3);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("An executable file whose Set Group ID (SGID) bit is\n"
                           "set will always be executed with the privileges of the\n"
                           "group of the executable."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* set Atk label relation */
  object = gtk_widget_get_accessible (advanced_permissions_dialog->suid_button);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 0);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  advanced_permissions_dialog->sticky_button = gtk_check_button_new_with_mnemonic (_("_Sticky"));
  gtk_table_attach (GTK_TABLE (table), advanced_permissions_dialog->sticky_button, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (advanced_permissions_dialog->sticky_button);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_misc_set_padding (GTK_MISC (image), 3, 3);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("A file in a sticky directory may only be removed or\n"
                           "renamed by a user if the user has write permission\n"
                           "for the directory and the user is the owner of the\n"
                           "file, the owner of the directory or the superuser."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* set Atk label relation */
  object = gtk_widget_get_accessible (advanced_permissions_dialog->suid_button);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
}



static void
thunar_advanced_permissions_dialog_finalize (GObject *object)
{
  ThunarAdvancedPermissionsDialog *advanced_permissions_dialog = THUNAR_ADVANCED_PERMISSIONS_DIALOG (object);

  /* reset the file property */
  thunar_advanced_permissions_dialog_set_file (advanced_permissions_dialog, NULL);

  (*G_OBJECT_CLASS (thunar_advanced_permissions_dialog_parent_class)->finalize) (object);
}



static void
thunar_advanced_permissions_dialog_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  ThunarAdvancedPermissionsDialog *advanced_permissions_dialog = THUNAR_ADVANCED_PERMISSIONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_advanced_permissions_dialog_get_file (advanced_permissions_dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_advanced_permissions_dialog_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  ThunarAdvancedPermissionsDialog *advanced_permissions_dialog = THUNAR_ADVANCED_PERMISSIONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_advanced_permissions_dialog_set_file (advanced_permissions_dialog, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_advanced_permissions_dialog_response (GtkDialog *dialog,
                                             gint       response)
{
  ThunarAdvancedPermissionsDialog *advanced_permissions_dialog = THUNAR_ADVANCED_PERMISSIONS_DIALOG (dialog);
  ThunarVfsFileMode                mode;
  GError                          *error = NULL;

  /* check if the user pressed "Ok" */
  if (response == GTK_RESPONSE_ACCEPT)
    {
      /* determine the previous file mode */
      mode = thunar_file_get_mode (advanced_permissions_dialog->file);

      /* drop the previous suid/sgid/sticky bits */
      mode &= ~(THUNAR_VFS_FILE_MODE_SUID | THUNAR_VFS_FILE_MODE_SGID | THUNAR_VFS_FILE_MODE_STICKY);

      /* apply the new bits */
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->suid_button)))
        mode |= THUNAR_VFS_FILE_MODE_SUID;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->sgid_button)))
        mode |= THUNAR_VFS_FILE_MODE_SGID;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->sticky_button)))
        mode |= THUNAR_VFS_FILE_MODE_STICKY;

      /* try to apply the new mode to the file */
      if (!thunar_file_chmod (advanced_permissions_dialog->file, mode, &error))
        {
          /* display an error message to the user */
          thunar_dialogs_show_error (GTK_WIDGET (dialog), error, _("Failed to change permissions of `%s'"),
                                     thunar_file_get_display_name (advanced_permissions_dialog->file));
          g_error_free (error);
        }
    }

  /* close the dialog window */
  gtk_widget_destroy (GTK_WIDGET (dialog));
}



static void
thunar_advanced_permissions_dialog_file_changed (ThunarAdvancedPermissionsDialog *advanced_permissions_dialog,
                                                 ThunarFile                      *file)
{
  ThunarVfsFileMode mode;

  g_return_if_fail (THUNAR_IS_ADVANCED_PERMISSIONS_DIALOG (advanced_permissions_dialog));
  g_return_if_fail (advanced_permissions_dialog->file == file);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the file's mode */
  mode = thunar_file_get_mode (file);

  /* update the "SUID" check button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->suid_button), (mode & THUNAR_VFS_FILE_MODE_SUID));
  gtk_widget_set_sensitive (advanced_permissions_dialog->suid_button, thunar_file_is_chmodable (file));

  /* update the "SGID" check button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->sgid_button), (mode & THUNAR_VFS_FILE_MODE_SGID));
  gtk_widget_set_sensitive (advanced_permissions_dialog->sgid_button, thunar_file_is_chmodable (file));

  /* update the "Sticky" check button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_permissions_dialog->sticky_button), (mode & THUNAR_VFS_FILE_MODE_STICKY));
  gtk_widget_set_sensitive (advanced_permissions_dialog->sticky_button, thunar_file_is_chmodable (file));

  /* update the sensitivity of the "Ok" button */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (advanced_permissions_dialog), GTK_RESPONSE_ACCEPT, thunar_file_is_chmodable (file));
}



/**
 * thunar_advanced_permissions_dialog_new:
 *
 * Allocates a new #ThunarAdvancedPermissionsDialog instance.
 *
 * Return value: the newly allocated #ThunarAdvancedPermissionsDialog.
 **/
GtkWidget*
thunar_advanced_permissions_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_ADVANCED_PERMISSIONS_DIALOG, NULL);
}



/**
 * thunar_advanced_permissions_dialog_get_file:
 * @advanced_permissions_dialog : a #ThunarAdvancedPermissionsDialog instance.
 *
 * Returns the #ThunarFile currently associated with the @advanced_permissions_dialog.
 *
 * Return value: the file associated with @advanced_permissions_dialog.
 **/
ThunarFile*
thunar_advanced_permissions_dialog_get_file (ThunarAdvancedPermissionsDialog *advanced_permissions_dialog)
{
  g_return_val_if_fail (THUNAR_IS_ADVANCED_PERMISSIONS_DIALOG (advanced_permissions_dialog), NULL);
  return advanced_permissions_dialog->file;
}



/**
 * thunar_advanced_permissions_dialog_set_file:
 * @advanced_permissions_dialog : a #ThunarAdvancedPermissionsDialog instance.
 * @file                        : a #ThunarFile or %NULL.
 *
 * Associates the @advanced_permissions_dialog with the specified @file.
 **/
void
thunar_advanced_permissions_dialog_set_file (ThunarAdvancedPermissionsDialog *advanced_permissions_dialog,
                                             ThunarFile                      *file)
{
  g_return_if_fail (THUNAR_IS_ADVANCED_PERMISSIONS_DIALOG (advanced_permissions_dialog));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (advanced_permissions_dialog->file == file))
    return;

  /* disconnect from the previous file */
  if (G_LIKELY (advanced_permissions_dialog->file != NULL))
    {
      /* disconnect the changed signal handler */
      g_signal_handlers_disconnect_by_func (G_OBJECT (advanced_permissions_dialog->file), thunar_advanced_permissions_dialog_file_changed, advanced_permissions_dialog);

      /* release our reference */
      g_object_unref (G_OBJECT (advanced_permissions_dialog->file));
    }

  /* activate the new file */
  advanced_permissions_dialog->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the file */
      g_object_ref (G_OBJECT (file));

      /* stay informed about changes */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_advanced_permissions_dialog_file_changed), advanced_permissions_dialog);

      /* update the GUI */
      thunar_advanced_permissions_dialog_file_changed (advanced_permissions_dialog, file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (advanced_permissions_dialog), "file");
}
 
