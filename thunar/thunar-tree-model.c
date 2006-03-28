/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-folder.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-tree-model.h>



/* convenience macros */
#define G_NODE(node)                 ((GNode *) (node))
#define THUNAR_TREE_MODEL_ITEM(item) ((ThunarTreeModelItem *) (item))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
};



typedef struct _ThunarTreeModelItem ThunarTreeModelItem;



static void                 thunar_tree_model_class_init              (ThunarTreeModelClass *klass);
static void                 thunar_tree_model_tree_model_init         (GtkTreeModelIface    *iface);
static void                 thunar_tree_model_init                    (ThunarTreeModel      *model);
static void                 thunar_tree_model_finalize                (GObject              *object);
static void                 thunar_tree_model_get_property            (GObject              *object,
                                                                       guint                 prop_id,
                                                                       GValue               *value,
                                                                       GParamSpec           *pspec);
static void                 thunar_tree_model_set_property            (GObject              *object,
                                                                       guint                 prop_id,
                                                                       const GValue         *value,
                                                                       GParamSpec           *pspec);
static GtkTreeModelFlags    thunar_tree_model_get_flags               (GtkTreeModel         *tree_model);
static gint                 thunar_tree_model_get_n_columns           (GtkTreeModel         *tree_model);
static GType                thunar_tree_model_get_column_type         (GtkTreeModel         *tree_model,
                                                                       gint                  column);
static gboolean             thunar_tree_model_get_iter                (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter,
                                                                       GtkTreePath          *path);
static GtkTreePath         *thunar_tree_model_get_path                (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static void                 thunar_tree_model_get_value               (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter,
                                                                       gint                  column,
                                                                       GValue               *value);
static gboolean             thunar_tree_model_iter_next               (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static gboolean             thunar_tree_model_iter_children           (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter,
                                                                       GtkTreeIter          *parent);
static gboolean             thunar_tree_model_iter_has_child          (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static gint                 thunar_tree_model_iter_n_children         (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static gboolean             thunar_tree_model_iter_nth_child          (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter,
                                                                       GtkTreeIter          *parent,
                                                                       gint                  n);
static gboolean             thunar_tree_model_iter_parent             (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter,
                                                                       GtkTreeIter          *child);
static void                 thunar_tree_model_ref_node                (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static void                 thunar_tree_model_unref_node              (GtkTreeModel         *tree_model,
                                                                       GtkTreeIter          *iter);
static gint                 thunar_tree_model_cmp_array               (gconstpointer         a,
                                                                       gconstpointer         b,
                                                                       gpointer              user_data);
static void                 thunar_tree_model_sort                    (ThunarTreeModel      *model,
                                                                       GNode                *node);
static void                 thunar_tree_model_file_changed            (ThunarFileMonitor    *file_monitor,
                                                                       ThunarFile           *file,
                                                                       ThunarTreeModel      *model);
static ThunarTreeModelItem *thunar_tree_model_item_new                (ThunarTreeModel      *model,
                                                                       ThunarFile           *file) G_GNUC_MALLOC;
static void                 thunar_tree_model_item_free               (ThunarTreeModelItem  *item);
static void                 thunar_tree_model_item_load_folder        (ThunarTreeModelItem  *item);
static void                 thunar_tree_model_item_unload_folder      (ThunarTreeModelItem  *item);
static void                 thunar_tree_model_item_files_added        (ThunarTreeModelItem  *item,
                                                                       GList                *files,
                                                                       ThunarFolder         *folder);
static void                 thunar_tree_model_item_files_removed      (ThunarTreeModelItem  *item,
                                                                       GList                *files,
                                                                       ThunarFolder         *folder);
static gboolean             thunar_tree_model_item_load_idle          (gpointer              user_data);
static void                 thunar_tree_model_item_load_idle_destroy  (gpointer              user_data);
static void                 thunar_tree_model_item_notify_loading     (ThunarTreeModelItem  *item,
                                                                       GParamSpec           *pspec,
                                                                       ThunarFolder         *folder);
static void                 thunar_tree_model_node_drop_dummy         (GNode                *node,
                                                                       ThunarTreeModel      *model);
static gboolean             thunar_tree_model_node_traverse_changed   (GNode                *node,
                                                                       gpointer              user_data);
static gboolean             thunar_tree_model_node_traverse_remove    (GNode                *node,
                                                                       gpointer              user_data);
static gboolean             thunar_tree_model_node_traverse_sort      (GNode                *node,
                                                                       gpointer              user_data);
static gboolean             thunar_tree_model_node_traverse_free      (GNode                *node,
                                                                       gpointer              user_data);



struct _ThunarTreeModelClass
{
  GObjectClass __parent__;
};

struct _ThunarTreeModel
{
  GObject __parent__;

  ThunarFileMonitor *file_monitor;

  gboolean           sort_case_sensitive;

  GNode             *root;
  guint              stamp;
};

struct _ThunarTreeModelItem
{
  gint             ref_count;
  gint             load_idle_id;
  ThunarFile      *file;
  ThunarFolder    *folder;
  ThunarTreeModel *model;
};

typedef struct
{
  gint   offset;
  GNode *node;
} SortTuple;



static GObjectClass *thunar_tree_model_parent_class;



GType
thunar_tree_model_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarTreeModelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_tree_model_class_init,
        NULL,
        NULL,
        sizeof (ThunarTreeModel),
        0,
        (GInstanceInitFunc) thunar_tree_model_init,
        NULL,
      };

      static const GInterfaceInfo tree_model_info =
      {
        (GInterfaceInitFunc) thunar_tree_model_tree_model_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarTreeModel"), &info, 0);
      g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL, &tree_model_info);
    }
  
  return type;
}



static void
thunar_tree_model_class_init (ThunarTreeModelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_tree_model_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tree_model_finalize;
  gobject_class->get_property = thunar_tree_model_get_property;
  gobject_class->set_property = thunar_tree_model_set_property;

  /**
   * ThunarTreeModel:case-sensitive:
   *
   * Whether the sorting of the folder items will be done
   * in a case-sensitive manner.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CASE_SENSITIVE,
                                   g_param_spec_boolean ("case-sensitive",
                                                         "case-sensitive",
                                                         "case-sensitive",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_tree_model_get_flags;
  iface->get_n_columns = thunar_tree_model_get_n_columns;
  iface->get_column_type = thunar_tree_model_get_column_type;
  iface->get_iter = thunar_tree_model_get_iter;
  iface->get_path = thunar_tree_model_get_path;
  iface->get_value = thunar_tree_model_get_value;
  iface->iter_next = thunar_tree_model_iter_next;
  iface->iter_children = thunar_tree_model_iter_children;
  iface->iter_has_child = thunar_tree_model_iter_has_child;
  iface->iter_n_children = thunar_tree_model_iter_n_children;
  iface->iter_nth_child = thunar_tree_model_iter_nth_child;
  iface->iter_parent = thunar_tree_model_iter_parent;
  iface->ref_node = thunar_tree_model_ref_node;
  iface->unref_node = thunar_tree_model_unref_node;
}



static void
thunar_tree_model_init (ThunarTreeModel *model)
{
  ThunarTreeModelItem *item;
  ThunarVfsPath       *path;
  ThunarFile          *file;
  GNode               *node;

  /* initialize the model data */
  model->sort_case_sensitive = TRUE;
  model->stamp = g_random_int ();

  /* connect to the file monitor */
  model->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (model->file_monitor), "file-changed", G_CALLBACK (thunar_tree_model_file_changed), model);

  /* allocate the "virtual root node" */
  model->root = g_node_new (NULL);

  /* allocate the home node */
  path = thunar_vfs_path_get_for_home ();
  file = thunar_file_get_for_path (path, NULL);
  if (G_LIKELY (file != NULL))
    {
      /* add the home node */
      item = thunar_tree_model_item_new (model, file);
      node = g_node_append_data (model->root, item);
      g_object_unref (G_OBJECT (file));

      /* add the dummy node */
      g_node_append_data (node, NULL);
    }
  thunar_vfs_path_unref (path);

  /* allocate the root node */
  path = thunar_vfs_path_get_for_root ();
  file = thunar_file_get_for_path (path, NULL);
  if (G_LIKELY (file != NULL))
    {
      /* add the root node */
      item = thunar_tree_model_item_new (model, file);
      node = g_node_append_data (model->root, item);
      g_object_unref (G_OBJECT (file));

      /* add the dummy node */
      g_node_append_data (node, NULL);
    }
  thunar_vfs_path_unref (path);
}



static void
thunar_tree_model_finalize (GObject *object)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (G_OBJECT (model->file_monitor), thunar_tree_model_file_changed, model);
  g_object_unref (G_OBJECT (model->file_monitor));

  /* release all resources allocated to the model */
  g_node_traverse (model->root, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_free, NULL);
  g_node_destroy (model->root);

  (*G_OBJECT_CLASS (thunar_tree_model_parent_class)->finalize) (object);
}



static void
thunar_tree_model_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_tree_model_get_case_sensitive (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tree_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      thunar_tree_model_set_case_sensitive (model, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_tree_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}



static gint
thunar_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_TREE_MODEL_N_COLUMNS;
}



static GType
thunar_tree_model_get_column_type (GtkTreeModel *tree_model,
                                   gint          column)
{
  switch (column)
    {
    case THUNAR_TREE_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_TREE_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_TREE_MODEL_COLUMN_ATTR:
      return PANGO_TYPE_ATTR_LIST;
    }

  g_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_tree_model_get_iter (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreePath  *path)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GtkTreeIter      parent;
  const gint      *indices;
  gint             depth;
  gint             n;

  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the path depth */
  depth = gtk_tree_path_get_depth (path);

  /* determine the path indices */
  indices = gtk_tree_path_get_indices (path);

  /* initialize the parent iterator with the root element */
  parent.stamp = model->stamp;
  parent.user_data = model->root;

  if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[0]))
    return FALSE;

  for (n = 1; n < depth; ++n)
    {
      parent = *iter;
      if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[n]))
        return FALSE;
    }

  return TRUE;
}



static GtkTreePath*
thunar_tree_model_get_path (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GtkTreePath     *path;
  GtkTreeIter      tmp_iter;
  GNode           *tmp_node;
  GNode           *node;
  gint             n;

  g_return_val_if_fail (iter->user_data != NULL, NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* determine the node for the iterator */
  node = iter->user_data;

  /* check if the iter refers to the "virtual root node" */
  if (node->parent == NULL && node == model->root)
    return gtk_tree_path_new ();
  else if (G_UNLIKELY (node->parent == NULL))
    return NULL;

  if (node->parent == model->root)
    {
      path = gtk_tree_path_new ();
      tmp_node = g_node_first_child (model->root);
    }
  else
    {
      /* determine the iterator for the parent node */
      tmp_iter.stamp = model->stamp;
      tmp_iter.user_data = node->parent;

      /* determine the path for the parent node */
      path = gtk_tree_model_get_path (tree_model, &tmp_iter);

      /* and the node for the parent's children */
      tmp_node = g_node_first_child (node->parent);
    }

  /* check if we have a valid path */
  if (G_LIKELY (path != NULL))
    {
      /* lookup our index in the child list */
      for (n = 0; tmp_node != NULL; ++n, tmp_node = tmp_node->next)
        if (tmp_node == node)
          break;

      /* check if we have found the node */
      if (G_UNLIKELY (tmp_node == NULL))
        {
          gtk_tree_path_free (path);
          return NULL;
        }

      /* append the index to the parent path */
      gtk_tree_path_append_index (path, n);
    }

  return path;
}



static void
thunar_tree_model_get_value (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node = iter->user_data;

  g_return_if_fail (iter->user_data != NULL);
  g_return_if_fail (iter->stamp == model->stamp);
  g_return_if_fail (column < THUNAR_TREE_MODEL_N_COLUMNS);

  /* determine the item for the node */
  item = node->data;

  switch (column)
    {
    case THUNAR_TREE_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, (item != NULL) ? item->file : NULL);
      break;

    case THUNAR_TREE_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_LIKELY (item != NULL))
        g_value_set_static_string (value, thunar_file_get_display_name (item->file));
      else
        g_value_set_static_string (value, _("Loading..."));
      break;

    case THUNAR_TREE_MODEL_COLUMN_ATTR:
      g_value_init (value, PANGO_TYPE_ATTR_LIST);
      if (G_UNLIKELY (node->parent == model->root))
        g_value_set_boxed (value, thunar_pango_attr_list_bold ());
      else if (G_UNLIKELY (item == NULL))
        g_value_set_boxed (value, thunar_pango_attr_list_italic ());
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_tree_model_iter_next (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);

  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  /* check if we have any further nodes in this row */
  if (g_node_next_sibling (iter->user_data) != NULL)
    {
      iter->user_data = g_node_next_sibling (iter->user_data);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_children (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *children;

  g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  g_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  if (G_LIKELY (parent != NULL))
    children = g_node_first_child (parent->user_data);
  else
    children = g_node_first_child (model->root);

  if (G_LIKELY (children != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = children;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_has_child (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);

  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  return (g_node_first_child (iter->user_data) != NULL);
}



static gint
thunar_tree_model_iter_n_children (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);

  g_return_val_if_fail (iter == NULL || iter->user_data != NULL, 0);
  g_return_val_if_fail (iter == NULL || iter->stamp == model->stamp, 0);

  return g_node_n_children ((iter == NULL) ? model->root : iter->user_data);
}



static gboolean
thunar_tree_model_iter_nth_child (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *child;
  
  g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  g_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  child = g_node_nth_child ((parent != NULL) ? parent->user_data : model->root, n);
  if (G_LIKELY (child != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = child;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_parent (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *child)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *parent;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (child->user_data != NULL, FALSE);
  g_return_val_if_fail (child->stamp == model->stamp, FALSE);

  /* check if we have a parent for iter */
  parent = G_NODE (child->user_data)->parent;
  if (G_LIKELY (parent != model->root))
    {
      iter->user_data = parent;
      iter->stamp = model->stamp;
      return TRUE;
    }
  else
    {
      /* no "real parent" for this node */
      return FALSE;
    }
}



static void
thunar_tree_model_ref_node (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node;

  g_return_if_fail (iter->user_data != NULL);
  g_return_if_fail (iter->stamp == model->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == model->root))
    return;

  /* check if we have a dummy item here */
  item = node->data;
  if (G_UNLIKELY (item == NULL))
    {
      /* tell the parent to load the folder */
      thunar_tree_model_item_load_folder (node->parent->data);
    }
  else
    {
      /* increment the reference count */
      item->ref_count += 1;
    }
}



static void
thunar_tree_model_unref_node (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node;

  g_return_if_fail (iter->user_data != NULL);
  g_return_if_fail (iter->stamp == model->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == model->root))
    return;

  /* check if this a non-dummy item */
  item = node->data;
  if (G_LIKELY (item != NULL))
    {
      /* check if this was the last reference */
      if (G_UNLIKELY (item->ref_count == 1))
        {
          /* schedule to unload the folder contents */
          thunar_tree_model_item_unload_folder (item);
        }

      /* decrement the reference count */
      item->ref_count -= 1;
    }
}



static gint
thunar_tree_model_cmp_array (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  /* just sort by name (case-sensitive) */
  return thunar_file_compare_by_name (THUNAR_TREE_MODEL_ITEM (((const SortTuple *) a)->node->data)->file,
                                      THUNAR_TREE_MODEL_ITEM (((const SortTuple *) b)->node->data)->file,
                                      THUNAR_TREE_MODEL (user_data)->sort_case_sensitive);
}



static void
thunar_tree_model_sort (ThunarTreeModel *model,
                        GNode           *node)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  SortTuple   *sort_array;
  GNode       *child_node;
  gint         n_children;
  gint        *new_order;
  gint         n;

  /* determine the number of children of the node */
  n_children = g_node_n_children (node);
  if (G_UNLIKELY (n_children <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (n_children < 500))
    sort_array = g_newa (SortTuple, n_children);
  else
    sort_array = g_new (SortTuple, n_children);

  /* generate the sort array of tuples */
  for (child_node = g_node_first_child (node), n = 0; n < n_children; child_node = g_node_next_sibling (child_node), ++n)
    {
      sort_array[n].node = child_node;
      sort_array[n].offset = n;
    }

  /* sort the array using QuickSort */
  g_qsort_with_data (sort_array, n_children, sizeof (SortTuple), thunar_tree_model_cmp_array, model);

  /* start out with an empty child list */
  node->children = NULL;

  /* update our internals and generate the new order */
  new_order = g_newa (gint, n_children);
  for (n = 0; n < n_children; ++n)
    {
      /* yeppa, there's the new offset */
      new_order[n] = sort_array[n].offset;

      /* unlink and reinsert */
      sort_array[n].node->next = NULL;
      sort_array[n].node->prev = NULL;
      sort_array[n].node->parent = NULL;
      g_node_append (node, sort_array[n].node);
    }

  /* determine the iterator for the parent node */
  iter.stamp = model->stamp;
  iter.user_data = node;

  /* tell the view about the new item order */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, &iter, new_order);
  gtk_tree_path_free (path);

  /* cleanup if we used the heap */
  if (G_UNLIKELY (n_children >= 500))
    g_free (sort_array);
}



static void
thunar_tree_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarTreeModel   *model)
{
  g_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  g_return_if_fail (model->file_monitor == file_monitor);
  g_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* traverse the model and emit "row-changed" for the file's nodes */
  g_node_traverse (model->root, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_changed, file);
}



static ThunarTreeModelItem*
thunar_tree_model_item_new (ThunarTreeModel *model,
                            ThunarFile      *file)
{
  ThunarTreeModelItem *item;

  item = g_new0 (ThunarTreeModelItem, 1);
  item->file = g_object_ref (G_OBJECT (file));
  item->model = model;

  return item;
}



static void
thunar_tree_model_item_free (ThunarTreeModelItem *item)
{
  /* cancel any pending "load" idle source */
  if (G_UNLIKELY (item->load_idle_id != 0))
    g_source_remove (item->load_idle_id);

  /* disconnect from the folder */
  if (G_LIKELY (item->folder != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (item->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, item);
      g_object_unref (G_OBJECT (item->folder));
    }

  /* disconnect from the file */
  if (G_LIKELY (item->file != NULL))
    g_object_unref (G_OBJECT (item->file));

  /* release the item */
  g_free (item);
}



static void
thunar_tree_model_item_load_folder (ThunarTreeModelItem *item)
{
  /* schedule the "load" idle source (if not already done) */
  if (G_LIKELY (item->folder == NULL && item->load_idle_id == 0))
    {
      item->load_idle_id = g_idle_add_full (G_PRIORITY_HIGH, thunar_tree_model_item_load_idle,
                                            item, thunar_tree_model_item_load_idle_destroy);
    }
}



static void
thunar_tree_model_item_unload_folder (ThunarTreeModelItem *item)
{
  // FIXME: Unload the children of item and add a dummy child
}



static void
thunar_tree_model_item_files_added (ThunarTreeModelItem *item,
                                    GList               *files,
                                    ThunarFolder        *folder)
{
  ThunarTreeModelItem *child_item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (item->model);
  GtkTreePath         *child_path;
  GtkTreeIter          child_iter;
  ThunarFile          *file;
  GNode               *child_node;
  GNode               *node = NULL;
  GList               *lp;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (item->folder == folder);

  /* process all specified files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* we don't care for anything except folders */
      file = THUNAR_FILE (lp->data);
      if (!thunar_file_is_directory (file))
        continue;

      /* lookup the node for the item (on-demand) */
      if (G_UNLIKELY (node == NULL))
        node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

      /* allocate a new item for the file */
      child_item = thunar_tree_model_item_new (model, file);

      /* check if the node has only the dummy child */
      if (g_node_n_children (node) == 1 && g_node_first_child (node)->data == NULL)
        {
          /* replace the dummy node with the new node */
          child_node = g_node_first_child (node);
          child_node->data = child_item;

          /* determine the tree iter for the child */
          child_iter.stamp = model->stamp;
          child_iter.user_data = child_node;

          /* emit a "row-changed" for the new node */
          child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), child_path, &child_iter);
          gtk_tree_path_free (child_path);
        }
      else
        {
          /* insert a new item for the child */
          child_node = g_node_append_data (node, child_item);

          /* determine the tree iter for the child */
          child_iter.stamp = model->stamp;
          child_iter.user_data = child_node;

          /* emit a "row-inserted" for the new node */
          child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), child_path, &child_iter);
          gtk_tree_path_free (child_path);
        }

      /* add a dummy child node */
      child_node = g_node_append_data (child_node, NULL);

      /* determine the tree iter for the dummy */
      child_iter.stamp = model->stamp;
      child_iter.user_data = child_node;

      /* emit a "row-inserted" for the dummy node */
      child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), child_path, &child_iter);
      gtk_tree_path_free (child_path);
    }

  /* sort the folders if any new ones were added */
  if (G_LIKELY (node != NULL))
    thunar_tree_model_sort (model, node);
}



static void
thunar_tree_model_item_files_removed (ThunarTreeModelItem *item,
                                      GList               *files,
                                      ThunarFolder        *folder)
{
  ThunarTreeModel *model = item->model;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  GNode           *child_node;
  GNode           *node;
  GList           *lp;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (item->folder == folder);

  /* determine the node for the folder */
  node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

  /* process all files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* find the child node for the file */
      for (child_node = g_node_first_child (node); child_node != NULL; child_node = g_node_next_sibling (child_node))
        if (child_node->data != NULL && THUNAR_TREE_MODEL_ITEM (child_node->data)->file == lp->data)
          break;

      /* drop the child node (and all descendant nodes) from the model */
      if (G_LIKELY (child_node != NULL))
        g_node_traverse (child_node, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_remove, model);
    }

  /* determine the iterator for the folder node */
  iter.stamp = model->stamp;
  iter.user_data = node;

  /* emit "row-has-child-toggled" for the folder node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}



static void
thunar_tree_model_item_notify_loading (ThunarTreeModelItem *item,
                                       GParamSpec          *pspec,
                                       ThunarFolder        *folder)
{
  GNode *node;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (item->folder == folder);

  /* be sure to drop the dummy child node once the folder is loaded */
  if (G_LIKELY (!thunar_folder_get_loading (folder)))
    {
      /* lookup the node for the item... */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

      /* ...and drop the dummy for the node */
      thunar_tree_model_node_drop_dummy (node, item->model);
    }
}



static gboolean
thunar_tree_model_item_load_idle (gpointer user_data)
{
  ThunarTreeModelItem *item = user_data;
  GList               *files;
  GNode               *node;

  g_return_val_if_fail (item->folder == NULL, FALSE);

  GDK_THREADS_ENTER ();

  /* open the folder for the item */
  item->folder = thunar_folder_get_for_file (item->file);

  /* connect signals */
  g_signal_connect_swapped (G_OBJECT (item->folder), "files-added", G_CALLBACK (thunar_tree_model_item_files_added), item);
  g_signal_connect_swapped (G_OBJECT (item->folder), "files-removed", G_CALLBACK (thunar_tree_model_item_files_removed), item);
  g_signal_connect_swapped (G_OBJECT (item->folder), "notify::loading", G_CALLBACK (thunar_tree_model_item_notify_loading), item);

  /* load the initial set of files (if any) */
  files = thunar_folder_get_files (item->folder);
  if (G_UNLIKELY (files != NULL))
    thunar_tree_model_item_files_added (item, files, item->folder);

  /* drop the dummy item if the folder is already loaded */
  if (!thunar_folder_get_loading (item->folder))
    {
      /* lookup the node for the item... */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

      /* ...and drop the dummy for the node */
      thunar_tree_model_node_drop_dummy (node, item->model);
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_tree_model_item_load_idle_destroy (gpointer user_data)
{
  THUNAR_TREE_MODEL_ITEM (user_data)->load_idle_id = 0;
}



static void
thunar_tree_model_node_drop_dummy (GNode           *node,
                                   ThunarTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter  iter;

  /* check if we still have the dummy child node */
  if (g_node_n_children (node) == 1 && g_node_first_child (node)->data == NULL)
    {
      /* determine the iterator for the dummy */
      iter.stamp = model->stamp;
      iter.user_data = g_node_first_child (node);

      /* determine the path for the iterator */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      if (G_LIKELY (path != NULL))
        {
          /* drop the dummy from the model */
          g_node_destroy (iter.user_data);

          /* notify the view */
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

          /* release the path */
          gtk_tree_path_free (path);

          /* determine the iter to the parent node */
          iter.stamp = model->stamp;
          iter.user_data = node;

          /* emit a "row-has-child-toggled" for the parent */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}



static gboolean
thunar_tree_model_node_traverse_changed (GNode   *node,
                                         gpointer user_data)
{
  ThunarTreeModel *model;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  ThunarFile      *file = THUNAR_FILE (user_data);

  /* check if the node's file is the file that changed */
  if (node->data != NULL && THUNAR_TREE_MODEL_ITEM (node->data)->file == file)
    {
      /* determine the tree model from the item */
      model = THUNAR_TREE_MODEL_ITEM (node->data)->model;

      /* determine the iterator for the node */
      iter.stamp = model->stamp;
      iter.user_data = node;

      /* check if the changed node is not one of the root nodes */
      if (G_LIKELY (node->parent != model->root))
        {
          /* need to re-sort as the name of the file may have changed */
          thunar_tree_model_sort (model, node->parent);
        }

      /* determine the path for the node */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      if (G_LIKELY (path != NULL))
        {
          /* emit "row-changed" */
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_remove (GNode   *node,
                                        gpointer user_data)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (user_data);
  GtkTreeIter      iter;
  GtkTreePath     *path;

  /* determine the iterator for the node */
  iter.stamp = model->stamp;
  iter.user_data = node;

  /* determine the path for the node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  if (G_LIKELY (path != NULL))
    {
      /* release the item for the node */
      thunar_tree_model_node_traverse_free (node, user_data);

      /* remove the node from the tree */
      g_node_destroy (node);

      /* emit a "row-deleted" */
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      /* release the path */
      gtk_tree_path_free (path);
    }

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_sort (GNode   *node,
                                      gpointer user_data)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (user_data);

  /* we don't want to sort the children of the root node */
  if (G_LIKELY (node != model->root))
    thunar_tree_model_sort (model, node);

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_free (GNode   *node,
                                      gpointer user_data)
{
  if (G_LIKELY (node->data != NULL))
    thunar_tree_model_item_free (node->data);
  return FALSE;
}



/**
 * thunar_tree_model_get_default:
 *
 * Returns the default, shared #ThunarTreeModel instance.
 *
 * The caller is responsible to free the returned instance
 * using g_object_unref() when no longer needed.
 *
 * Return value: a reference to the default #ThunarTreeModel.
 **/
ThunarTreeModel*
thunar_tree_model_get_default (void)
{
  static ThunarTreeModel *model = NULL;
  ThunarPreferences      *preferences;

  if (G_LIKELY (model == NULL))
    {
      /* allocate the shared model on-demand */
      model = g_object_new (THUNAR_TYPE_TREE_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);

      /* synchronize the the global "misc-case-sensitive" preference */
      preferences = thunar_preferences_get ();
      g_object_set_data_full (G_OBJECT (model), I_("thunar-preferences"), preferences, g_object_unref);
      exo_binding_new (G_OBJECT (preferences), "misc-case-sensitive", G_OBJECT (model), "case-sensitive");
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



/**
 * thunar_tree_model_get_case_sensitive:
 * @model : a #ThunarTreeModel.
 *
 * Returns %TRUE if the sorting for @model is done
 * in a case-sensitive manner.
 *
 * Return value: %TRUE if @model is sorted case-sensitive.
 **/
gboolean
thunar_tree_model_get_case_sensitive (ThunarTreeModel *model)
{
  g_return_val_if_fail (THUNAR_IS_TREE_MODEL (model), FALSE);
  return model->sort_case_sensitive;
}



/**
 * thunar_tree_model_set_case_sensitive:
 * @model          : a #ThunarTreeModel.
 * @case_sensitive : %TRUE to sort @model case-sensitive.
 *
 * If @case_sensitive is %TRUE the @model will be sorted
 * in a case-sensitive manner.
 **/
void
thunar_tree_model_set_case_sensitive (ThunarTreeModel *model,
                                      gboolean         case_sensitive)
{
  g_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* normalize the setting */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (model->sort_case_sensitive != case_sensitive)
    {
      /* apply the new setting */
      model->sort_case_sensitive = case_sensitive;

      /* resort the model with the new setting */
      g_node_traverse (model->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_tree_model_node_traverse_sort, model);

      /* notify listeners */
      g_object_notify (G_OBJECT (model), "case-sensitive");
    }
}

