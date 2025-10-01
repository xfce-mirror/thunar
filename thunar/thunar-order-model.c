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

#include "thunar/thunar-order-model.h"

static void
thunar_order_model_tree_model_interface_init (GtkTreeModelIface *iface);

static void
thunar_order_model_tree_drag_source_interface_init (GtkTreeDragSourceIface *iface);

static void
thunar_order_model_tree_drag_dest_interface_init (GtkTreeDragDestIface *iface);

static gboolean
thunar_order_model_drag_data_delete (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path);

static gboolean
thunar_order_model_drag_data_get (GtkTreeDragSource *drag_source,
                                  GtkTreePath       *path,
                                  GtkSelectionData  *selection_data);

static gboolean
thunar_order_model_row_draggable (GtkTreeDragSource *drag_source,
                                  GtkTreePath       *path);

static gboolean
thunar_order_model_drag_data_received (GtkTreeDragDest  *drag_dest,
                                       GtkTreePath      *dest,
                                       GtkSelectionData *selection_data);

static gboolean
thunar_order_model_row_drop_possible (GtkTreeDragDest  *drag_dest,
                                      GtkTreePath      *dest_path,
                                      GtkSelectionData *selection_data);

static GdkAtom
thunar_order_model_get_dnd_atom (ThunarOrderModel *order_model);

static gint
thunar_order_model_get_dnd_format (ThunarOrderModel *order_model);

static gint
thunar_order_model_get_n_items (ThunarOrderModel *order_model);

static void
thunar_order_model_init_iter (GtkTreeIter *iter,
                              gint         position);

static gint
thunar_order_model_get_iter_position (GtkTreeIter *iter);

static gint
thunar_order_model_get_n_columns (GtkTreeModel *tree_model);

static GType
thunar_order_model_get_column_type (GtkTreeModel *tree_model,
                                    gint          column);

static GtkTreeModelFlags
thunar_order_model_get_flags (GtkTreeModel *tree_model);

static gboolean
thunar_order_model_get_iter (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             GtkTreePath  *path);

static GtkTreePath *
thunar_order_model_get_path (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter);

static gint
thunar_order_model_iter_n_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter);

static gboolean
thunar_order_model_iter_children (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent);

static gboolean
thunar_order_model_iter_nth_child (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *parent,
                                   gint          n);

static gboolean
thunar_order_model_iter_next (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter);

static gboolean
thunar_order_model_iter_previous (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter);

static void
thunar_order_model_get_value (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              gint          column,
                              GValue       *value);

G_DEFINE_TYPE_WITH_CODE (ThunarOrderModel, thunar_order_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_order_model_tree_model_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, thunar_order_model_tree_drag_source_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_order_model_tree_drag_dest_interface_init))

static void
thunar_order_model_class_init (ThunarOrderModelClass *klass)
{
}

static void
thunar_order_model_init (ThunarOrderModel *order_model)
{
}

static void
thunar_order_model_tree_model_interface_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = thunar_order_model_get_n_columns;
  iface->get_column_type = thunar_order_model_get_column_type;
  iface->get_flags = thunar_order_model_get_flags;
  iface->get_iter = thunar_order_model_get_iter;
  iface->get_path = thunar_order_model_get_path;
  iface->iter_n_children = thunar_order_model_iter_n_children;
  iface->iter_children = thunar_order_model_iter_children;
  iface->iter_nth_child = thunar_order_model_iter_nth_child;
  iface->iter_next = thunar_order_model_iter_next;
  iface->iter_previous = thunar_order_model_iter_previous;
  iface->get_value = thunar_order_model_get_value;
}

static void
thunar_order_model_tree_drag_source_interface_init (GtkTreeDragSourceIface *iface)
{
  iface->drag_data_delete = thunar_order_model_drag_data_delete;
  iface->drag_data_get = thunar_order_model_drag_data_get;
  iface->row_draggable = thunar_order_model_row_draggable;
}

static void
thunar_order_model_tree_drag_dest_interface_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = thunar_order_model_drag_data_received;
  iface->row_drop_possible = thunar_order_model_row_drop_possible;
}

static gboolean
thunar_order_model_drag_data_delete (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path)
{
  return TRUE;
}

static gboolean
thunar_order_model_drag_data_get (GtkTreeDragSource *drag_source,
                                  GtkTreePath       *path,
                                  GtkSelectionData  *selection_data)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (drag_source);
  gchar            *path_string = gtk_tree_path_to_string (path);
  guint             path_length = strlen (path_string);

  gtk_selection_data_set (selection_data,
                          thunar_order_model_get_dnd_atom (order_model),
                          thunar_order_model_get_dnd_format (order_model),
                          (const guchar *) path_string,
                          path_length + 1);

  g_free (path_string);

  return TRUE;
}

static gboolean
thunar_order_model_row_draggable (GtkTreeDragSource *drag_source,
                                  GtkTreePath       *path)
{
  return TRUE;
}

static gboolean
thunar_order_model_drag_data_received (GtkTreeDragDest  *drag_dest,
                                       GtkTreePath      *dest,
                                       GtkSelectionData *selection_data)
{
  const gchar *data = (const gchar *) gtk_selection_data_get_data (selection_data);
  GtkTreePath *source = gtk_tree_path_new_from_string (data);
  GtkTreeIter  a_iter;
  GtkTreeIter  b_iter;

  thunar_order_model_get_iter (GTK_TREE_MODEL (drag_dest), &a_iter, source);
  thunar_order_model_get_iter (GTK_TREE_MODEL (drag_dest), &b_iter, dest);
  thunar_order_model_swap_items (THUNAR_ORDER_MODEL (drag_dest), &a_iter, &b_iter);
  gtk_tree_path_free (source);

  return TRUE;
}

static gboolean
thunar_order_model_row_drop_possible (GtkTreeDragDest  *drag_dest,
                                      GtkTreePath      *dest_path,
                                      GtkSelectionData *selection_data)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (drag_dest);
  GdkAtom           atom = gtk_selection_data_get_data_type (selection_data);
  gint              format = gtk_selection_data_get_format (selection_data);

  return atom == thunar_order_model_get_dnd_atom (order_model)
         && format == thunar_order_model_get_dnd_format (order_model);
}

static GdkAtom
thunar_order_model_get_dnd_atom (ThunarOrderModel *order_model)
{
  return gdk_atom_intern_static_string (G_OBJECT_TYPE_NAME (order_model));
}

static gint
thunar_order_model_get_dnd_format (ThunarOrderModel *order_model)
{
  /* Each object has its own format */
  return (gint) (uintptr_t) order_model;
}

static gint
thunar_order_model_get_n_items (ThunarOrderModel *order_model)
{
  ThunarOrderModelClass *order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);

  g_return_val_if_fail (order_model_class->get_n_items != NULL, 0);
  return order_model_class->get_n_items (order_model);
}

static void
thunar_order_model_init_iter (GtkTreeIter *iter,
                              gint         position)
{
  memset (iter, 0, sizeof (GtkTreeIter));
  iter->user_data = (gpointer) (uintptr_t) (position);
}

static gint
thunar_order_model_get_iter_position (GtkTreeIter *iter)
{
  return (gint) (uintptr_t) iter->user_data;
}

static gint
thunar_order_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_ORDER_MODEL_N_COLUMNS;
}

static GType
thunar_order_model_get_column_type (GtkTreeModel *tree_model,
                                    gint          column)
{
  switch (column)
    {
    case THUNAR_ORDER_MODEL_COLUMN_ACTIVE:
      return G_TYPE_BOOLEAN;

    case THUNAR_ORDER_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_ORDER_MODEL_COLUMN_ICON:
      return G_TYPE_STRING;

    case THUNAR_ORDER_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_ORDER_MODEL_COLUMN_TOOLTIP:
      return G_TYPE_STRING;

    default:
      g_return_val_if_reached (G_TYPE_NONE);
    }
}

static GtkTreeModelFlags
thunar_order_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_LIST_ONLY;
}

static gboolean
thunar_order_model_get_iter (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             GtkTreePath  *path)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);
  gint              n_items = thunar_order_model_get_n_items (order_model);
  gint              position = *gtk_tree_path_get_indices (path);

  if (position < 0 || position >= n_items)
    return FALSE;

  thunar_order_model_init_iter (iter, position);
  return TRUE;
}

static GtkTreePath *
thunar_order_model_get_path (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter)
{
  gint position = thunar_order_model_get_iter_position (iter);

  return gtk_tree_path_new_from_indices (position, -1);
}

static gint
thunar_order_model_iter_n_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);

  if (iter == NULL)
    return thunar_order_model_get_n_items (order_model);

  return 0;
}

static gboolean
thunar_order_model_iter_children (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);

  if (thunar_order_model_get_n_items (order_model) <= 0)
    return FALSE;

  thunar_order_model_init_iter (iter, 0);
  return TRUE;
}

static gboolean
thunar_order_model_iter_nth_child (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *parent,
                                   gint          n)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);

  if (n < 0 || n >= thunar_order_model_get_n_items (order_model))
    return FALSE;

  thunar_order_model_init_iter (iter, n);
  return TRUE;
}

static gboolean
thunar_order_model_iter_next (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);
  gint              n_items = thunar_order_model_get_n_items (order_model);
  gint              position = thunar_order_model_get_iter_position (iter);
  gboolean          status = (position + 1) < n_items;

  if (status)
    thunar_order_model_init_iter (iter, position + 1);

  return status;
}

static gboolean
thunar_order_model_iter_previous (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  ThunarOrderModel *order_model = THUNAR_ORDER_MODEL (tree_model);
  gint              n_items = thunar_order_model_get_n_items (order_model);
  gint              position = thunar_order_model_get_iter_position (iter);
  gboolean          status = (position - 1) >= 0 && (position - 1) < n_items;

  if (status)
    thunar_order_model_init_iter (iter, position - 1);

  return status;
}

static void
thunar_order_model_get_value (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              gint          column,
                              GValue       *value)
{
  ThunarOrderModel      *order_model = THUNAR_ORDER_MODEL (tree_model);
  ThunarOrderModelClass *order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);
  gint                   position = thunar_order_model_get_iter_position (iter);

  g_return_if_fail (order_model_class->get_value != NULL);
  g_value_init (value, thunar_order_model_get_column_type (tree_model, column));
  order_model_class->get_value (order_model, position, column, value);
}

void
thunar_order_model_set_activity (ThunarOrderModel *order_model,
                                 GtkTreeIter      *iter,
                                 gboolean          activity)
{
  ThunarOrderModelClass *order_model_class;
  gint                   position;
  GtkTreePath           *path;

  g_return_if_fail (THUNAR_IS_ORDER_MODEL (order_model));
  order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);
  g_return_if_fail (order_model_class->set_activity != NULL);

  position = thunar_order_model_get_iter_position (iter);
  order_model_class->set_activity (order_model, position, activity);

  path = thunar_order_model_get_path (GTK_TREE_MODEL (order_model), iter);
  g_signal_emit_by_name (order_model, "row-changed", path, iter);
  gtk_tree_path_free (path);
}

void
thunar_order_model_swap_items (ThunarOrderModel *order_model,
                               GtkTreeIter      *a_iter,
                               GtkTreeIter      *b_iter)
{
  ThunarOrderModelClass *order_model_class;
  gint                   a_position;
  gint                   b_position;
  GtkTreePath           *a_path;
  GtkTreePath           *b_path;

  g_return_if_fail (THUNAR_IS_ORDER_MODEL (order_model));
  order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);
  g_return_if_fail (order_model_class->swap_items != NULL);

  a_position = thunar_order_model_get_iter_position (a_iter);
  b_position = thunar_order_model_get_iter_position (b_iter);
  order_model_class->swap_items (order_model, a_position, b_position);

  a_path = thunar_order_model_get_path (GTK_TREE_MODEL (order_model), a_iter);
  b_path = thunar_order_model_get_path (GTK_TREE_MODEL (order_model), b_iter);

  g_signal_emit_by_name (order_model, "row-changed", a_path, a_iter);
  g_signal_emit_by_name (order_model, "row-changed", b_path, b_iter);

  gtk_tree_path_free (a_path);
  gtk_tree_path_free (b_path);
}

void
thunar_order_model_reset (ThunarOrderModel *order_model)
{
  ThunarOrderModelClass *order_model_class;

  g_return_if_fail (THUNAR_IS_ORDER_MODEL (order_model));
  order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);
  g_return_if_fail (order_model_class->reset != NULL);
  order_model_class->reset (order_model);
}

void
thunar_order_model_reload (ThunarOrderModel *order_model)
{
  gint         n_items;
  gint         i;
  GtkTreeIter  iter;
  GtkTreePath *path;

  g_return_if_fail (THUNAR_IS_ORDER_MODEL (order_model));
  n_items = thunar_order_model_get_n_items (order_model);

  thunar_order_model_init_iter (&iter, 0);
  path = gtk_tree_path_new_from_indices (0, -1);
  for (i = 0; i < n_items; ++i)
    g_signal_emit_by_name (order_model, "row-deleted", path);
  for (i = 0; i < n_items; ++i)
    g_signal_emit_by_name (order_model, "row-inserted", path, &iter);
  gtk_tree_path_free (path);
}
