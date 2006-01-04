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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-change-group-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-permissions-model.h>
#include <thunar/thunar-stock.h>



#define INDEX_TO_MODE_MASK(index) ((1 << (3 - ((index) % 4))) << ((2 - (index / 4)) * 3))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_MUTABLE,
};



static void               thunar_permissions_model_class_init       (ThunarPermissionsModelClass  *klass);
static void               thunar_permissions_model_tree_model_init  (GtkTreeModelIface            *iface);
static void               thunar_permissions_model_init             (ThunarPermissionsModel       *model);
static void               thunar_permissions_model_finalize         (GObject                      *object);
static void               thunar_permissions_model_get_property     (GObject                      *object,
                                                                     guint                         prop_id,
                                                                     GValue                       *value,
                                                                     GParamSpec                   *pspec);
static void               thunar_permissions_model_set_property     (GObject                      *object,
                                                                     guint                         prop_id,
                                                                     const GValue                 *value,
                                                                     GParamSpec                   *pspec);
static GtkTreeModelFlags  thunar_permissions_model_get_flags        (GtkTreeModel                 *tree_model);
static gint               thunar_permissions_model_get_n_columns    (GtkTreeModel                 *tree_model);
static GType              thunar_permissions_model_get_column_type  (GtkTreeModel                 *tree_model,
                                                                     gint                          column);
static gboolean           thunar_permissions_model_get_iter         (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter,
                                                                     GtkTreePath                  *path);
static GtkTreePath       *thunar_permissions_model_get_path         (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter);
static void               thunar_permissions_model_get_value        (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter,
                                                                     gint                          column,
                                                                     GValue                       *value);
static gboolean           thunar_permissions_model_iter_next        (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter);
static gboolean           thunar_permissions_model_iter_children    (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter,
                                                                     GtkTreeIter                  *parent);
static gboolean           thunar_permissions_model_iter_has_child   (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter);
static gint               thunar_permissions_model_iter_n_children  (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter);
static gboolean           thunar_permissions_model_iter_nth_child   (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter,
                                                                     GtkTreeIter                  *parent,
                                                                     gint                          n);
static gboolean           thunar_permissions_model_iter_parent      (GtkTreeModel                 *tree_model,
                                                                     GtkTreeIter                  *iter,
                                                                     GtkTreeIter                  *child);
static void               thunar_permissions_model_file_changed     (ThunarFile                   *file,
                                                                     ThunarPermissionsModel       *model);



struct _ThunarPermissionsModelClass
{
  GObjectClass __parent__;
};

struct _ThunarPermissionsModel
{
  GObject __parent__;

  gint        stamp;
  ThunarFile *file;
};



static const gchar *THUNAR_PERMISSIONS_STOCK_IDS[] =
{
  THUNAR_STOCK_PERMISSIONS_USER,
  THUNAR_STOCK_PERMISSIONS_GROUP,
  THUNAR_STOCK_PERMISSIONS_OTHER,
};



static GObjectClass *thunar_permissions_model_parent_class;



GType
thunar_permissions_model_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPermissionsModelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_permissions_model_class_init,
        NULL,
        NULL,
        sizeof (ThunarPermissionsModel),
        0,
        (GInstanceInitFunc) thunar_permissions_model_init,
        NULL,
      };

      static const GInterfaceInfo tree_model_info =
      {
        (GInterfaceInitFunc) thunar_permissions_model_tree_model_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarPermissionsModel"), &info, 0);
      g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL, &tree_model_info);
    }

  return type;
}



static void
thunar_permissions_model_class_init (ThunarPermissionsModelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_permissions_model_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_permissions_model_finalize;
  gobject_class->get_property = thunar_permissions_model_get_property;
  gobject_class->set_property = thunar_permissions_model_set_property;

  /**
   * ThunarPermissionsModel:file:
   *
   * The #ThunarFile whose permissions are managed by this model.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPermissionsModel:mutable.
   *
   * Whether the mode of the #ThunarFile whose permissions are managed
   * by this model can be changed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MUTABLE,
                                   g_param_spec_boolean ("mutable",
                                                         "mutable",
                                                         "mutable",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));
}



static void
thunar_permissions_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags        = thunar_permissions_model_get_flags;
  iface->get_n_columns    = thunar_permissions_model_get_n_columns;
  iface->get_column_type  = thunar_permissions_model_get_column_type;
  iface->get_iter         = thunar_permissions_model_get_iter;
  iface->get_path         = thunar_permissions_model_get_path;
  iface->get_value        = thunar_permissions_model_get_value;
  iface->iter_next        = thunar_permissions_model_iter_next;
  iface->iter_children    = thunar_permissions_model_iter_children;
  iface->iter_has_child   = thunar_permissions_model_iter_has_child;
  iface->iter_n_children  = thunar_permissions_model_iter_n_children;
  iface->iter_nth_child   = thunar_permissions_model_iter_nth_child;
  iface->iter_parent      = thunar_permissions_model_iter_parent;
}



static void
thunar_permissions_model_init (ThunarPermissionsModel *model)
{
  model->stamp = g_random_int ();
}



static void
thunar_permissions_model_finalize (GObject *object)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (object);
  thunar_permissions_model_set_file (model, NULL);
  (*G_OBJECT_CLASS (thunar_permissions_model_parent_class)->finalize) (object);
}



static void
thunar_permissions_model_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_permissions_model_get_file (model));
      break;

    case PROP_MUTABLE:
      g_value_set_boolean (value, thunar_permissions_model_is_mutable (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_permissions_model_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_permissions_model_set_file (model, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_permissions_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}



static gint
thunar_permissions_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_PERMISSIONS_MODEL_N_COLUMNS;
}



static GType
thunar_permissions_model_get_column_type (GtkTreeModel *tree_model,
                                          gint          column)
{
  switch (column)
    {
    case THUNAR_PERMISSIONS_MODEL_COLUMN_TITLE:
      return G_TYPE_STRING;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_ICON:
      return G_TYPE_STRING;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_ICON_VISIBLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_STATE:
      return G_TYPE_BOOLEAN;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_STATE_VISIBLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    default:
      g_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}



static gboolean
thunar_permissions_model_get_iter (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   GtkTreePath  *path)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  guint                   index;
  gint                   *indices;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the tree path indices */
  indices = gtk_tree_path_get_indices (path);

  /* determine the base index */
  if (indices[0] < 0 || indices[0] > 2)
    return FALSE;
  index = indices[0] * 4;

  /* determine the sub index */
  if (gtk_tree_path_get_depth (path) > 1)
    {
      if (indices[1] < 0 || indices[1] > 2)
        return FALSE;
      index += (indices[1] + 1);
    }

  /* setup the tree iter */
  iter->stamp = model->stamp;
  iter->user_data = GUINT_TO_POINTER (index);

  return TRUE;
}



static GtkTreePath*
thunar_permissions_model_get_path (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  GtkTreePath            *path;
  guint                   index;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);

  index = GPOINTER_TO_UINT (iter->user_data);
  if (G_UNLIKELY (index >= 3 * 4))
    return NULL;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, index / 4);
  if ((index % 4) != 0)
    gtk_tree_path_append_index (path, (index % 4) - 1);

  return path;
}



static void
thunar_permissions_model_get_value (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    gint          column,
                                    GValue       *value)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  ThunarVfsGroup         *group;
  ThunarVfsUser          *user;
  const gchar            *real_name;
  const gchar            *user_name;
  guint                   index;

  g_return_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model));

  /* initialize the value with the proper type */
  g_value_init (value, gtk_tree_model_get_column_type (tree_model, column));

  /* verify that we have a valid file */
  if (G_UNLIKELY (model->file == NULL))
    return;

  /* verify that the index is valid */
  index = GPOINTER_TO_UINT (iter->user_data);
  if (index >= 3 * 4)
    return;

  switch (column)
    {
    case THUNAR_PERMISSIONS_MODEL_COLUMN_TITLE:
      if ((index % 4) == 0)
        {
          switch (index / 4)
            {
            case 0:
              user = thunar_file_get_user (model->file);
              if (G_LIKELY (user != NULL))
                {
                  user_name = thunar_vfs_user_get_name (user);
                  real_name = thunar_vfs_user_get_real_name (user);
                  if (G_LIKELY (strcmp (user_name, real_name) != 0))
                    g_value_take_string (value, g_strdup_printf (_("%s (%s)"), real_name, user_name));
                  else
                    g_value_set_string (value, user_name);
                  g_object_unref (G_OBJECT (user));
                }
              else
                {
                  g_value_set_static_string (value, _("Unknown file owner"));
                }
              break;

            case 1:
              group = thunar_file_get_group (model->file);
              if (G_LIKELY (group != NULL))
                {
                  g_value_set_string (value, thunar_vfs_group_get_name (group));
                  g_object_unref (G_OBJECT (group));
                }
              else
                {
                  g_value_set_static_string (value, _("Unknown file group"));
                }
              break;

            case 2:
              g_value_set_static_string (value, _("All other users"));
              break;
            }
        }
      else
        {
          switch ((index % 4) - 1)
            {
            case 0:
              g_value_set_static_string (value, _("Read"));
              break;

            case 1:
              g_value_set_static_string (value, _("Write"));
              break;

            case 2:
              g_value_set_static_string (value, thunar_file_is_directory (model->file) ? _("List folder contents") : _("Execute"));
              break;
            }
        }
      break;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_ICON:
      g_value_set_static_string (value, THUNAR_PERMISSIONS_STOCK_IDS[index / 4]);
      break;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_ICON_VISIBLE:
      g_value_set_boolean (value, (index % 4) == 0);
      break;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_STATE:
      g_value_set_boolean (value, thunar_file_get_mode (model->file) & INDEX_TO_MODE_MASK (index));
      break;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_STATE_VISIBLE:
      g_value_set_boolean (value, (index % 4) != 0);
      break;

    case THUNAR_PERMISSIONS_MODEL_COLUMN_MUTABLE:
      g_value_set_boolean (value, thunar_file_is_chmodable (model->file));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_permissions_model_iter_next (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  guint                   index = GPOINTER_TO_UINT (iter->user_data);

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

  if (index == 0 || index == 4)
    index += 4;
  else if (((index % 4) == 1 || (index % 4) == 2) && index < 3 * 4)
    index += 1;
  else
    return FALSE;

  iter->user_data = GUINT_TO_POINTER (index);

  return TRUE;
}



static gboolean
thunar_permissions_model_iter_children (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *parent)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  guint                   index = GPOINTER_TO_UINT (parent->user_data);

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (parent->stamp == model->stamp, FALSE);

  if ((index % 4) == 0 && index < 3 * 4)
    {
      iter->stamp = model->stamp;
      iter->user_data = GUINT_TO_POINTER (index + 1);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_permissions_model_iter_has_child (GtkTreeModel *tree_model,
                                         GtkTreeIter  *iter)
{
  return (gtk_tree_model_iter_n_children (tree_model, iter) > 0);
}



static gint
thunar_permissions_model_iter_n_children (GtkTreeModel *tree_model,
                                          GtkTreeIter  *iter)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), 0);
  g_return_val_if_fail (iter == NULL || iter->stamp == model->stamp, 0);

  if (iter == NULL || (GPOINTER_TO_UINT (iter->user_data) % 4) == 0)
    return 3;

  return 0;
}



static gboolean
thunar_permissions_model_iter_nth_child (GtkTreeModel *tree_model,
                                         GtkTreeIter  *iter,
                                         GtkTreeIter  *parent,
                                         gint          n)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  guint                   index;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  if (G_LIKELY (n >= 0 && n < 3))
    {
      if (parent == NULL)
        {
          iter->stamp = model->stamp;
          iter->user_data = GUINT_TO_POINTER (n * 4);
          return TRUE;
        }
      else
        {
          index = GPOINTER_TO_UINT (parent->user_data);
          if (index == 0 || index == 4 || index == 8)
            {
              iter->stamp = model->stamp;
              iter->user_data = GUINT_TO_POINTER (index + n + 1);
              return TRUE;
            }
        }
    }

  return FALSE;
}



static gboolean
thunar_permissions_model_iter_parent (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *child)
{
  ThunarPermissionsModel *model = THUNAR_PERMISSIONS_MODEL (tree_model);
  guint                   index = GPOINTER_TO_UINT (child->user_data);

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (child->stamp == model->stamp, FALSE);

  if (index < 3 * 4 && (index % 4) != 0)
    {
      iter->stamp = model->stamp;
      iter->user_data = GUINT_TO_POINTER ((index / 4) * 4);
      return TRUE;
    }

  return FALSE;
}



static void
thunar_permissions_model_file_changed (ThunarFile             *file,
                                       ThunarPermissionsModel *model)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  gint         index;

  g_return_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model));
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (file == model->file);

  /* emit "row-changed" for all (virtual) tree items */
  for (index = 0; index < 3 * 4; ++index)
    {
      /* setup the tree iter */
      iter.stamp = model->stamp;
      iter.user_data = GINT_TO_POINTER (index);

      /* notify the view */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);
    }

  /* notify listeners of "mutable" */
  g_object_notify (G_OBJECT (model), "mutable");
}



/**
 * thunar_permissions_model_new:
 *
 * Allocates a new #ThunarPermissionsModel.
 *
 * Return value: the newly allocated #ThunarPermissionsModel.
 **/
GtkTreeModel*
thunar_permissions_model_new (void)
{
  return g_object_new (THUNAR_TYPE_PERMISSIONS_MODEL, NULL);
}



/**
 * thunar_permissions_model_get_file:
 * @model : a #ThunarPermissionsModel instance.
 *
 * Returns the #ThunarFile associated with the @model.
 *
 * Return value: the #ThunarFile associated with @model.
 **/
ThunarFile*
thunar_permissions_model_get_file (ThunarPermissionsModel *model)
{
  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), NULL);
  return model->file;
}



/**
 * thunar_permissions_model_set_file:
 * @model : a #ThunarPermissionsModel instance.
 * @file  : a #ThunarFile instance or %NULL.
 *
 * Associates the @model with the specified @file.
 **/
void
thunar_permissions_model_set_file (ThunarPermissionsModel *model,
                                   ThunarFile             *file)
{
  g_return_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we have a new file here */
  if (G_UNLIKELY (model->file == file))
    return;

  /* disconnect from the previous file */
  if (G_LIKELY (model->file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (model->file), thunar_permissions_model_file_changed, model);
      g_object_unref (G_OBJECT (model->file));
    }

  /* activate the new file */
  model->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      g_object_ref (G_OBJECT (file));
      g_signal_connect (G_OBJECT (file), "changed", G_CALLBACK (thunar_permissions_model_file_changed), model);
      thunar_permissions_model_file_changed (file, model);
    }

  /* notify listeners */
  g_object_freeze_notify (G_OBJECT (model));
  g_object_notify (G_OBJECT (model), "file");
  g_object_notify (G_OBJECT (model), "mutable");
  g_object_thaw_notify (G_OBJECT (model));
}



/**
 * thunar_permissions_model_is_mutable:
 * @model : a #ThunarPermissionsModel instance.
 *
 * Returns %TRUE if the @model is mutable, that is, if the mode of the
 * #ThunarFile associated with the @model can be changed.
 *
 * Return value: %TRUE if @model is mutable.
 **/
gboolean
thunar_permissions_model_is_mutable (ThunarPermissionsModel *model)
{
  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  return (model->file != NULL && thunar_file_is_chmodable (model->file));
}



static void
change_group_activated (GtkAction  *action,
                        ThunarFile *file)
{
  GtkWidget *toplevel;
  GtkWidget *widget;
  GtkWidget *dialog;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the widget associated with the action */
  widget = g_object_get_data (G_OBJECT (action), "gtk-widget");
  if (G_UNLIKELY (widget == NULL))
    return;

  /* determine the toplevel item for the given widget */
  toplevel = gtk_widget_get_toplevel (widget);
  if (G_UNLIKELY (toplevel == NULL))
    return;

  /* popup the "Change Group" dialog */
  dialog = thunar_change_group_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  thunar_change_group_dialog_set_file (THUNAR_CHANGE_GROUP_DIALOG (dialog), file);
  gtk_widget_show (dialog);
}



static void
grant_or_deny_activated (GtkAction              *action,
                         ThunarPermissionsModel *model)
{
  GtkTreeIter *iter;
  GtkWidget   *widget;
  GError      *error = NULL;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model));

  /* determine the tree iterator associated with the action */
  iter = g_object_get_data (G_OBJECT (action), "gtk-tree-iter");
  if (G_UNLIKELY (iter == NULL))
    return;

  /* determine the widget associated with the action */
  widget = g_object_get_data (G_OBJECT (action), "gtk-widget");
  if (G_UNLIKELY (widget == NULL))
    return;

  /* try to toggle the item */
  if (!thunar_permissions_model_toggle (model, iter, &error))
    {
      /* display an error to the user (when we get here, we definitely have a valid file!) */
      thunar_dialogs_show_error (widget, error, _("Failed to change permissions of `%s'"), thunar_file_get_display_name (model->file));

      /* release the error */
      g_error_free (error);
    }
}



/**
 * thunar_permissions_model_get_actions:
 * @model  : a #ThunarPermissionsModel instance.
 * @widget : the #GtkWidget on which the actions will be invoked.
 * @iter   : a valid #GtkTreeIter within @model.
 *
 * Returns the list of #GtkAction<!---->s that can be performed on the
 * item referred to by @iter in @model.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkAction<!---->s that can be performed on
 *               @iter in @model.
 **/
GList*
thunar_permissions_model_get_actions (ThunarPermissionsModel *model,
                                      GtkWidget              *widget,
                                      GtkTreeIter            *iter)
{
  ThunarVfsFileMode mode;
  GtkAction        *action;
  GList            *actions = NULL;
  gint              index;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  /* verify that we have a valid file */
  if (G_UNLIKELY (model->file == NULL))
    return NULL;

  /* determine the index for the iterator */
  index = GPOINTER_TO_UINT (iter->user_data);
  if (G_UNLIKELY (index >= 3 * 4))
    return NULL;

  /* add appropriate actions depending on the index */
  if (G_UNLIKELY (index == 4))
    {
      /* actions for the group item */
      action = gtk_action_new ("thunar-permissions-model-change-group", _("Change _Group..."), NULL, THUNAR_STOCK_PERMISSIONS_GROUP);
      g_object_set_data_full (G_OBJECT (action), I_("gtk-widget"), g_object_ref (G_OBJECT (widget)), g_object_unref);
      gtk_action_set_sensitive (action, thunar_file_is_chgrpable (model->file));
      g_signal_connect_data (G_OBJECT (action), "activate", G_CALLBACK (change_group_activated),
                             g_object_ref (G_OBJECT (model->file)), (GClosureNotify) g_object_unref, 0);
      actions = g_list_append (actions, action);
    }
  else if ((index % 4) != 0)
    {
      /* determine the file's mode */
      mode = thunar_file_get_mode (model->file);

      /* append the "Grant Permission" action */
      action = gtk_action_new ("thunar-permissions-model-grant-permission", _("_Grant Permission"), NULL, GTK_STOCK_YES);
      g_object_set_data_full (G_OBJECT (action), I_("gtk-tree-iter"), gtk_tree_iter_copy (iter), (GDestroyNotify) gtk_tree_iter_free);
      g_object_set_data_full (G_OBJECT (action), I_("gtk-widget"), g_object_ref (G_OBJECT (widget)), g_object_unref);
      gtk_action_set_sensitive (action, thunar_permissions_model_is_mutable (model) && (mode & INDEX_TO_MODE_MASK (index)) == 0);
      g_signal_connect_data (G_OBJECT (action), "activate", G_CALLBACK (grant_or_deny_activated),
                             g_object_ref (G_OBJECT (model)), (GClosureNotify) g_object_unref, 0);
      actions = g_list_append (actions, action);

      /* append the "Deny Permission" action */
      action = gtk_action_new ("thunar-permissions-model-deny-permission", _("_Deny Permission"), NULL, GTK_STOCK_NO);
      g_object_set_data_full (G_OBJECT (action), I_("gtk-tree-iter"), gtk_tree_iter_copy (iter), (GDestroyNotify) gtk_tree_iter_free);
      g_object_set_data_full (G_OBJECT (action), I_("gtk-widget"), g_object_ref (G_OBJECT (widget)), g_object_unref);
      gtk_action_set_sensitive (action, thunar_permissions_model_is_mutable (model) && (mode & INDEX_TO_MODE_MASK (index)) != 0);
      g_signal_connect_data (G_OBJECT (action), "activate", G_CALLBACK (grant_or_deny_activated),
                             g_object_ref (G_OBJECT (model)), (GClosureNotify) g_object_unref, 0);
      actions = g_list_append (actions, action);
    }

  return actions;
}



/**
 * thunar_permissions_model_toggle:
 * @model : a #ThunarPermissionsModel instance.
 * @iter  : a valid #GtkTreeIter for @model.
 * @error : return location for errors or %NULL.
 *
 * Toggles the permission flag identified by @iter for the
 * #ThunarFile associated with @model.
 *
 * Return value: %TRUE if the permission flag identified by
 *               @iter was successfully toggled.
 **/
gboolean
thunar_permissions_model_toggle (ThunarPermissionsModel *model,
                                 GtkTreeIter            *iter,
                                 GError                **error)
{
  guint index;
  guint mask;
  guint mode;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_MODEL (model), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

  /* determine the index for the iter */
  index = GPOINTER_TO_UINT (iter->user_data);

  /* check if we're associated with a file and have a valid index */
  if (G_LIKELY (model->file != NULL && (index % 4) != 0))
    {
      /* determine the mode and the mode mask */
      mask = INDEX_TO_MODE_MASK (index);
      mode = thunar_file_get_mode (model->file);

      /* toggle the bit set in the mode mask */
      if ((mode & mask) != 0)
        mode &= ~mask;
      else
        mode |= mask;

      /* perform the change mode operation */
      return thunar_file_chmod (model->file, mode, error);
    }

  return TRUE;
}


