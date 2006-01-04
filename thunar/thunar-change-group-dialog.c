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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar/thunar-change-group-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-stock.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};

/* Column identifiers for the combo box */
typedef enum
{
  THUNAR_CHANGE_GROUP_STORE_COLUMN_STOCK_ID,
  THUNAR_CHANGE_GROUP_STORE_COLUMN_NAME,
  THUNAR_CHANGE_GROUP_STORE_COLUMN_GROUP,
  THUNAR_CHANGE_GROUP_STORE_N_COLUMNS,
} ThunarChangeGroupStoreColumn;



static void thunar_change_group_dialog_class_init   (ThunarChangeGroupDialogClass *klass);
static void thunar_change_group_dialog_init         (ThunarChangeGroupDialog      *change_group_dialog);
static void thunar_change_group_dialog_finalize     (GObject                      *object);
static void thunar_change_group_dialog_get_property (GObject                      *object,
                                                     guint                         prop_id,
                                                     GValue                       *value,
                                                     GParamSpec                   *pspec);
static void thunar_change_group_dialog_set_property (GObject                      *object,
                                                     guint                         prop_id,
                                                     const GValue                 *value,
                                                     GParamSpec                   *pspec);
static void thunar_change_group_dialog_response     (GtkDialog                    *dialog,
                                                     gint                          response);
static void thunar_change_group_dialog_file_changed (ThunarChangeGroupDialog      *change_group_dialog,
                                                     ThunarFile                   *file);



struct _ThunarChangeGroupDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarChangeGroupDialog
{
  GtkDialog __parent__;

  ThunarFile *file;

  GtkWidget  *combo;
};



static GObjectClass *thunar_change_group_dialog_parent_class;



GType
thunar_change_group_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarChangeGroupDialogClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_change_group_dialog_class_init,
        NULL,
        NULL,
        sizeof (ThunarChangeGroupDialog),
        0,
        (GInstanceInitFunc) thunar_change_group_dialog_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_DIALOG, I_("ThunarChangeGroupDialogClass"), &info, 0);
    }

  return type;
}



static void
thunar_change_group_dialog_class_init (ThunarChangeGroupDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_change_group_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_change_group_dialog_finalize;
  gobject_class->get_property = thunar_change_group_dialog_get_property;
  gobject_class->set_property = thunar_change_group_dialog_set_property;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_change_group_dialog_response;

  /**
   * ThunarChangeGroupDialog:file:
   *
   * The #ThunarFile whose group should be edited.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static gboolean
row_separator_func (GtkTreeModel *model,
                    GtkTreeIter  *iter,
                    gpointer      data)
{
  GObject *object;

  /* determine the value of the "group" column */
  gtk_tree_model_get (model, iter, THUNAR_CHANGE_GROUP_STORE_COLUMN_GROUP, &object, -1);
  if (G_LIKELY (object != NULL))
    {
      g_object_unref (object);
      return FALSE;
    }

  return TRUE;
}



static void
thunar_change_group_dialog_init (ThunarChangeGroupDialog *change_group_dialog)
{
  GtkCellRenderer *renderer;
  AtkRelationSet  *relations;
  AtkRelation     *relation;
  AtkObject       *object;
  GtkWidget       *label;
  GtkWidget       *hbox;

  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (change_group_dialog)->vbox), 2);
  gtk_container_set_border_width (GTK_CONTAINER (change_group_dialog), 5);
  gtk_dialog_add_button (GTK_DIALOG (change_group_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (change_group_dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (change_group_dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_has_separator (GTK_DIALOG (change_group_dialog), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (change_group_dialog), 300, -1);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (change_group_dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (change_group_dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (change_group_dialog), _("Change Group"));

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (change_group_dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Group:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  change_group_dialog->combo = gtk_combo_box_new ();
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (change_group_dialog->combo), row_separator_func, NULL, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), change_group_dialog->combo);
  gtk_box_pack_start (GTK_BOX (hbox), change_group_dialog->combo, TRUE, TRUE, 0);
  gtk_widget_show (change_group_dialog->combo);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (change_group_dialog->combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  /* append the icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (change_group_dialog->combo), renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (change_group_dialog->combo), renderer, "stock-id", THUNAR_CHANGE_GROUP_STORE_COLUMN_STOCK_ID);

  /* append the text renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (change_group_dialog->combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (change_group_dialog->combo), renderer, "text", THUNAR_CHANGE_GROUP_STORE_COLUMN_NAME);
}



static void
thunar_change_group_dialog_finalize (GObject *object)
{
  ThunarChangeGroupDialog *change_group_dialog = THUNAR_CHANGE_GROUP_DIALOG (object);

  /* reset the file */
  thunar_change_group_dialog_set_file (change_group_dialog, NULL);

  (*G_OBJECT_CLASS (thunar_change_group_dialog_parent_class)->finalize) (object);
}



static void
thunar_change_group_dialog_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ThunarChangeGroupDialog *change_group_dialog = THUNAR_CHANGE_GROUP_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_change_group_dialog_get_file (change_group_dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_change_group_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ThunarChangeGroupDialog *change_group_dialog = THUNAR_CHANGE_GROUP_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_change_group_dialog_set_file (change_group_dialog, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_change_group_dialog_response (GtkDialog *dialog,
                                     gint       response)
{
  ThunarChangeGroupDialog *change_group_dialog = THUNAR_CHANGE_GROUP_DIALOG (dialog);
  ThunarVfsGroup          *group;
  GtkTreeModel            *model;
  GtkTreeIter              iter;
  GError                  *error = NULL;

  /* check if the user pressed "Ok" and determine the selected iterator */
  if (response == GTK_RESPONSE_ACCEPT && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (change_group_dialog->combo), &iter))
    {
      /* determine the selected group */
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (change_group_dialog->combo));
      gtk_tree_model_get (model, &iter, THUNAR_CHANGE_GROUP_STORE_COLUMN_GROUP, &group, -1);
      if (G_LIKELY (group != NULL))
        {
          /* try to change the group of the file */
          if (!thunar_file_chgrp (change_group_dialog->file, group, &error))
            {
              /* display an error message */
              thunar_dialogs_show_error (GTK_WIDGET (change_group_dialog), error, _("Failed to change the group to which `%s' belongs"),
                                         thunar_file_get_display_name (change_group_dialog->file));
              g_error_free (error);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (group));
        }
    }

  /* close the dialog */
  gtk_widget_destroy (GTK_WIDGET (dialog));
}



static gint
group_compare (gconstpointer group_a,
               gconstpointer group_b,
               gpointer      group_primary)
{
  ThunarVfsGroupId group_primary_id = thunar_vfs_group_get_id (THUNAR_VFS_GROUP (group_primary));
  ThunarVfsGroupId group_a_id = thunar_vfs_group_get_id (THUNAR_VFS_GROUP (group_a));
  ThunarVfsGroupId group_b_id = thunar_vfs_group_get_id (THUNAR_VFS_GROUP (group_b));

  /* check if the groups are equal */
  if (group_a_id == group_b_id)
    return 0;

  /* the primary group is always sorted first */
  if (group_a_id == group_primary_id)
    return -1;
  else if (group_b_id == group_primary_id)
    return 1;

  /* system groups (< 100) are always sorted last */
  if (group_a_id < 100 && group_b_id >= 100)
    return 1;
  else if (group_b_id < 100 && group_a_id >= 100)
    return -1;

  /* otherwise just sort by name */
  return g_ascii_strcasecmp (thunar_vfs_group_get_name (THUNAR_VFS_GROUP (group_a)), thunar_vfs_group_get_name (THUNAR_VFS_GROUP (group_b)));
}



static void
thunar_change_group_dialog_file_changed (ThunarChangeGroupDialog *change_group_dialog,
                                         ThunarFile              *file)
{
  ThunarVfsUserManager *user_manager;
  ThunarVfsGroup       *group;
  ThunarVfsUser        *user;
  GtkListStore         *store;
  GtkTreeIter           iter;
  GList                *groups;
  GList                *lp;
  gint                  state = 0;

  g_return_if_fail (THUNAR_IS_CHANGE_GROUP_DIALOG (change_group_dialog));
  g_return_if_fail (change_group_dialog->file == file);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* check if we are allowed to change the group */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (change_group_dialog), GTK_RESPONSE_ACCEPT, thunar_file_is_chgrpable (file));
  gtk_widget_set_sensitive (change_group_dialog->combo, thunar_file_is_chgrpable (file));

  /* allocate a new store for the combo box */
  store = gtk_list_store_new (THUNAR_CHANGE_GROUP_STORE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT);

  /* setup the new store for the combo box */
  gtk_combo_box_set_model (GTK_COMBO_BOX (change_group_dialog->combo), GTK_TREE_MODEL (store));

  /* determine the user for the the new file */
  user = thunar_file_get_user (file);
  if (G_LIKELY (user != NULL))
    {
      /* determine the group of the new file */
      group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          /* check if we have superuser privilieges */
          if (G_UNLIKELY (geteuid () == 0))
            {
              /* determine all groups in the system */
              user_manager = thunar_vfs_user_manager_get_default ();
              groups = thunar_vfs_user_manager_get_all_groups (user_manager);
              g_object_unref (G_OBJECT (user_manager));
            }
          else
            {
              /* determine the groups for the user and take a copy */
              groups = g_list_copy (thunar_vfs_user_get_groups (user));
              g_list_foreach (groups, (GFunc) g_object_ref, NULL);
            }

          /* sort the groups according to group_compare() */
          groups = g_list_sort_with_data (groups, group_compare, group);

          /* add the groups to the store */
          for (lp = groups; lp != NULL; lp = lp->next)
            {
              /* append a separator after the primary group and after the user-groups (not system groups) */
              if (thunar_vfs_group_get_id (groups->data) == thunar_vfs_group_get_id (group) && lp != groups && state == 0)
                {
                  gtk_list_store_append (store, &iter);
                  ++state;
                }
              else if (lp != groups && thunar_vfs_group_get_id (lp->data) < 100 && state == 1)
                {
                  gtk_list_store_append (store, &iter);
                  ++state;
                }

              /* append a new item for the group */
              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter,
                                  THUNAR_CHANGE_GROUP_STORE_COLUMN_STOCK_ID, THUNAR_STOCK_PERMISSIONS_GROUP,
                                  THUNAR_CHANGE_GROUP_STORE_COLUMN_NAME, thunar_vfs_group_get_name (lp->data),
                                  THUNAR_CHANGE_GROUP_STORE_COLUMN_GROUP, lp->data,
                                  -1);

              /* set the active iter for the combo box if this group is the primary group */
              if (G_UNLIKELY (lp->data == group))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (change_group_dialog->combo), &iter);
            }

          /* cleanup */
          g_list_foreach (groups, (GFunc) g_object_unref, NULL);
          g_object_unref (G_OBJECT (group));
          g_list_free (groups);
        }

      /* cleanup */
      g_object_unref (G_OBJECT (user));
    }

  /* cleanup */
  g_object_unref (G_OBJECT (store));
}



/**
 * thunar_change_group_dialog_new:
 *
 * Allocates a new #ThunarChangeGroupDialog instance.
 *
 * Return value: the newly allocated #ThunarChangeGroupDialog.
 **/
GtkWidget*
thunar_change_group_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_CHANGE_GROUP_DIALOG, NULL);
}



/**
 * thunar_change_group_dialog_get_file:
 * @change_group_dialog : a #ThunarChangeGroupDialog instance.
 *
 * Returns the #ThunarFile associated with the specified @change_group_dialog.
 *
 * Return value: the #ThunarFile associated with @change_group_dialog.
 **/
ThunarFile*
thunar_change_group_dialog_get_file (ThunarChangeGroupDialog *change_group_dialog)
{
  g_return_val_if_fail (THUNAR_IS_CHANGE_GROUP_DIALOG (change_group_dialog), NULL);
  return change_group_dialog->file;
}



/**
 * thunar_change_group_dialog_set_file:
 * @change_group_dialog : a #ThunarChangeGroupDialog instance.
 * @file                : a #ThunarFile instance or %NULL.
 *
 * Associates @change_group_dialog with the specified @file, and thereby
 * lets @change_group_dialog edit the group of the new @file.
 **/
void
thunar_change_group_dialog_set_file (ThunarChangeGroupDialog *change_group_dialog,
                                     ThunarFile              *file)
{
  g_return_if_fail (THUNAR_IS_CHANGE_GROUP_DIALOG (change_group_dialog));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (change_group_dialog->file == file))
    return;

  /* disconnect from the previously set file */
  if (G_LIKELY (change_group_dialog->file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (change_group_dialog->file), thunar_change_group_dialog_file_changed, change_group_dialog);
      g_object_unref (G_OBJECT (change_group_dialog->file));
    }

  /* activate the new file */
  change_group_dialog->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the file */
      g_object_ref (G_OBJECT (file));

      /* stay informed of changes to the file */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_change_group_dialog_file_changed), change_group_dialog);

      /* setup the GUI for the new file */
      thunar_change_group_dialog_file_changed (change_group_dialog, file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (change_group_dialog), "file");
}


