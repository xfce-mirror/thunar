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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-desktop-model.h>



GType
thunar_desktop_position_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static ("ThunarDesktopPosition",
                                           (GBoxedCopyFunc) thunar_desktop_position_dup,
                                           (GBoxedFreeFunc) thunar_desktop_position_free);
    }

  return type;
}



/**
 * thunar_desktop_position_dup:
 * @position : a #ThunarDesktopPosition.
 *
 * Returns a duplicate of @position. The caller
 * is responsible to call #thunar_desktop_position_free()
 * on the returned object.
 *
 * Return value: a duplicate of @position.
 **/
ThunarDesktopPosition*
thunar_desktop_position_dup (const ThunarDesktopPosition *position)
{
  g_return_val_if_fail (position != NULL, NULL);
  return g_memdup (position, sizeof (*position));
}



/**
 * thunar_desktop_position_free:
 * @position : a #ThunarDesktopPosition.
 *
 * Frees the resources allocated for @position.
 **/
void
thunar_desktop_position_free (ThunarDesktopPosition *position)
{
  g_return_if_fail (position != NULL);

#ifndef G_DISABLE_CHECKS
  memset (position, 0xaa, sizeof (*position));
#endif

  g_free (position);
}




typedef struct _ThunarDesktopModelItem ThunarDesktopModelItem;



static void               thunar_desktop_model_class_init       (ThunarDesktopModelClass  *klass);
static void               thunar_desktop_model_tree_model_init  (GtkTreeModelIface        *iface);
static void               thunar_desktop_model_init             (ThunarDesktopModel       *model);
static void               thunar_desktop_model_finalize         (GObject                  *object);
static GtkTreeModelFlags  thunar_desktop_model_get_flags        (GtkTreeModel             *tree_model);
static gint               thunar_desktop_model_get_n_columns    (GtkTreeModel             *tree_model);
static GType              thunar_desktop_model_get_column_type  (GtkTreeModel             *tree_model,
                                                                 gint                      index);
static gboolean           thunar_desktop_model_get_iter         (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter,
                                                                 GtkTreePath              *path);
static GtkTreePath       *thunar_desktop_model_get_path         (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter);
static void               thunar_desktop_model_get_value        (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter,
                                                                 gint                      column,
                                                                 GValue                   *value);
static gboolean           thunar_desktop_model_iter_next        (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter);
static gboolean           thunar_desktop_model_iter_children    (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter,
                                                                 GtkTreeIter              *parent);
static gboolean           thunar_desktop_model_iter_has_child   (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter);
static gint               thunar_desktop_model_iter_n_children  (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter);
static gboolean           thunar_desktop_model_iter_nth_child   (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter,
                                                                 GtkTreeIter              *parent,
                                                                 gint                      n);
static gboolean           thunar_desktop_model_iter_parent      (GtkTreeModel             *tree_model,
                                                                 GtkTreeIter              *iter,
                                                                 GtkTreeIter              *child);
static void               thunar_desktop_model_item_free        (ThunarDesktopModelItem   *item);



struct _ThunarDesktopModelClass
{
  GObjectClass __parent__;
};

struct _ThunarDesktopModel
{
  GObject __parent__;

  guint   stamp;
  GList  *items;
  gint    n_items;
};

struct _ThunarDesktopModelItem
{
  ThunarFile           *file;
  ThunarDesktopPosition position;
};



G_DEFINE_TYPE_WITH_CODE (ThunarDesktopModel,
                         thunar_desktop_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                thunar_desktop_model_tree_model_init));



static void
thunar_desktop_model_class_init (ThunarDesktopModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_desktop_model_finalize;
}



static void
thunar_desktop_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_desktop_model_get_flags;
  iface->get_n_columns = thunar_desktop_model_get_n_columns;
  iface->get_column_type = thunar_desktop_model_get_column_type;
  iface->get_iter = thunar_desktop_model_get_iter;
  iface->get_path = thunar_desktop_model_get_path;
  iface->get_value = thunar_desktop_model_get_value;
  iface->iter_next = thunar_desktop_model_iter_next;
  iface->iter_children = thunar_desktop_model_iter_children;
  iface->iter_has_child = thunar_desktop_model_iter_has_child;
  iface->iter_n_children = thunar_desktop_model_iter_n_children;
  iface->iter_nth_child = thunar_desktop_model_iter_nth_child;
  iface->iter_parent = thunar_desktop_model_iter_parent;
}



static void
thunar_desktop_model_init (ThunarDesktopModel *model)
{
  ThunarDesktopModelItem *item;
  ThunarVfsURI           *uri;

  model->stamp = g_random_int ();

  uri = thunar_vfs_uri_new_for_path (xfce_get_homedir ());
  item = g_new (ThunarDesktopModelItem, 1);
  item->file = thunar_file_get_for_uri (uri, NULL);
  item->position.row = 0;
  item->position.column = 0;
  item->position.screen_number = 0;
  model->items = g_list_append (model->items, item);
  ++model->n_items;
  thunar_vfs_uri_unref (uri);

  uri = thunar_vfs_uri_new ("trash:", NULL);
  item = g_new (ThunarDesktopModelItem, 1);
  item->file = thunar_file_get_for_uri (uri, NULL);
  item->position.row = 4;
  item->position.column = 1;
  item->position.screen_number = 0;
  model->items = g_list_append (model->items, item);
  ++model->n_items;
  thunar_vfs_uri_unref (uri);
}



static void
thunar_desktop_model_finalize (GObject *object)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (object);

  /* free the desktop items */
  g_list_foreach (model->items, (GFunc) thunar_desktop_model_item_free, NULL);
  g_list_free (model->items);

  G_OBJECT_CLASS (thunar_desktop_model_parent_class)->finalize (object);
}



static GtkTreeModelFlags
thunar_desktop_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_desktop_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_DESKTOP_MODEL_N_COLUMNS;
}



static GType
thunar_desktop_model_get_column_type (GtkTreeModel *tree_model,
                                      gint          index)
{
  switch (index)
    {
    case THUNAR_DESKTOP_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_DESKTOP_MODEL_COLUMN_POSITION:
      return THUNAR_TYPE_DESKTOP_POSITION;
    }

  g_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_desktop_model_get_iter (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreePath  *path)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (tree_model);
  gint                index;

  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  index = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (index >= model->n_items))
    return FALSE;

  iter->stamp = model->stamp;
  iter->user_data = g_list_nth (model->items, index);

  return TRUE;
}



static GtkTreePath*
thunar_desktop_model_get_path (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (model), NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);

  return gtk_tree_path_new_from_indices (g_list_position (model->items, iter->user_data), -1);
}



static void
thunar_desktop_model_get_value (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                gint          column,
                                GValue       *value)
{
  ThunarDesktopModelItem *item = ((GList *) iter->user_data)->data;
  ThunarDesktopModel     *model = THUNAR_DESKTOP_MODEL (tree_model);

  g_return_if_fail (THUNAR_IS_DESKTOP_MODEL (model));
  g_return_if_fail (model->stamp == iter->stamp);

  switch (column)
    {
    case THUNAR_DESKTOP_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, item->file);
      break;

    case THUNAR_DESKTOP_MODEL_COLUMN_POSITION:
      g_value_init (value, THUNAR_TYPE_DESKTOP_POSITION);
      g_value_set_boxed (value, &item->position);
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
thunar_desktop_model_iter_next (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter->stamp == THUNAR_DESKTOP_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = ((GList *) iter->user_data)->next;
  return (iter->user_data != NULL);
}



static gboolean
thunar_desktop_model_iter_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (model), FALSE);
  g_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  if (G_LIKELY (parent == NULL && model->items != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = model->items;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_desktop_model_iter_has_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_desktop_model_iter_n_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (model), 0);
  g_return_val_if_fail (iter == NULL || iter->stamp == model->stamp, 0);

  return (iter == NULL) ? model->n_items : 0;
}



static gboolean
thunar_desktop_model_iter_nth_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent,
                                     gint          n)
{
  ThunarDesktopModel *model = THUNAR_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_DESKTOP_MODEL (model), FALSE);
  g_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  if (G_LIKELY (parent == NULL && n < model->n_items))
    {
      iter->stamp = model->stamp;
      iter->user_data = g_list_nth (model->items, n);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_desktop_model_iter_parent (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *child)
{
  return FALSE;
}



static void
thunar_desktop_model_item_free (ThunarDesktopModelItem *item)
{
  g_object_unref (G_OBJECT (item->file));
  g_free (item);
}



/**
 * thunar_desktop_model_get_default:
 *
 * Returns the default #ThunarDesktopModel instance,
 * which is shared by all #ThunarDesktopView<!---->s.
 *
 * The caller is responsible to free the returned
 * object using #g_object_unref().
 *
 * Return value: the default #ThunarDesktopModel.
 **/
ThunarDesktopModel*
thunar_desktop_model_get_default (void)
{
  static ThunarDesktopModel *model = NULL;

  if (G_LIKELY (model == NULL))
    {
      model = g_object_new (THUNAR_TYPE_DESKTOP_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);
    }
  else
    {
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



