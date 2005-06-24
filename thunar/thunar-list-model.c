/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-list-model.h>



enum
{
  PROP_0,
  PROP_FOLDER,
  PROP_FOLDERS_FIRST,
  PROP_NUM_FILES,
  PROP_SHOW_HIDDEN,
};

typedef struct _Row       Row;
typedef struct _SortTuple SortTuple;



static void               thunar_list_model_init                  (ThunarListModel        *store);
static void               thunar_list_model_class_init            (ThunarListModelClass   *klass);
static void               thunar_list_model_tree_model_init       (GtkTreeModelIface      *iface);
static void               thunar_list_model_drag_source_init      (GtkTreeDragSourceIface *iface);
static void               thunar_list_model_drag_dest_init        (GtkTreeDragDestIface   *iface);
static void               thunar_list_model_sortable_init         (GtkTreeSortableIface   *iface);
static void               thunar_list_model_finalize              (GObject                *object);
static void               thunar_list_model_dispose               (GObject                *object);
static void               thunar_list_model_get_property          (GObject                *object,
                                                                   guint                   prop_id,
                                                                   GValue                 *value,
                                                                   GParamSpec             *pspec);
static void               thunar_list_model_set_property          (GObject                *object,
                                                                   guint                   prop_id,
                                                                   const GValue           *value,
                                                                   GParamSpec             *pspec);
static GtkTreeModelFlags  thunar_list_model_get_flags             (GtkTreeModel           *model);
static gint               thunar_list_model_get_n_columns         (GtkTreeModel           *model);
static GType              thunar_list_model_get_column_type       (GtkTreeModel           *model,
                                                                   gint                    index);
static gboolean           thunar_list_model_get_iter              (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter,
                                                                   GtkTreePath            *path);
static GtkTreePath       *thunar_list_model_get_path              (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter);
static void               thunar_list_model_get_value             (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter,
                                                                   gint                    column,
                                                                   GValue                 *value);
static gboolean           thunar_list_model_iter_next             (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter);
static gboolean           thunar_list_model_iter_children         (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter,
                                                                   GtkTreeIter            *parent);
static gboolean           thunar_list_model_iter_has_child        (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter);
static gint               thunar_list_model_iter_n_children       (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter);
static gboolean           thunar_list_model_iter_nth_child        (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter,
                                                                   GtkTreeIter            *parent,
                                                                   gint                    n);
static gboolean           thunar_list_model_iter_parent           (GtkTreeModel           *model,
                                                                   GtkTreeIter            *iter,
                                                                   GtkTreeIter            *child);
static gboolean           thunar_list_model_row_draggable         (GtkTreeDragSource      *source,
                                                                   GtkTreePath            *path);
static gboolean           thunar_list_model_drag_data_delete      (GtkTreeDragSource      *source,
                                                                   GtkTreePath            *path);
static gboolean           thunar_list_model_drag_data_get         (GtkTreeDragSource      *source,
                                                                   GtkTreePath            *path,
                                                                   GtkSelectionData       *data);
static gboolean           thunar_list_model_drag_data_received    (GtkTreeDragDest        *dest,
                                                                   GtkTreePath            *path,
                                                                   GtkSelectionData       *data);
static gboolean           thunar_list_model_row_drop_possible     (GtkTreeDragDest        *dest,
                                                                   GtkTreePath            *path,
                                                                   GtkSelectionData       *data);
static gboolean           thunar_list_model_get_sort_column_id    (GtkTreeSortable        *sortable,
                                                                   gint                   *sort_column_id,
                                                                   GtkSortType            *order);
static void               thunar_list_model_set_sort_column_id    (GtkTreeSortable        *sortable,
                                                                   gint                    sort_column_id,
                                                                   GtkSortType             order);
static void               thunar_list_model_set_default_sort_func (GtkTreeSortable        *sortable,
                                                                   GtkTreeIterCompareFunc  func,
                                                                   gpointer                data,
                                                                   GtkDestroyNotify        destroy);
static void               thunar_list_model_set_sort_func         (GtkTreeSortable        *sortable,
                                                                   gint                    sort_column_id,
                                                                   GtkTreeIterCompareFunc  func,
                                                                   gpointer                data,
                                                                   GtkDestroyNotify        destroy);
static gboolean           thunar_list_model_has_default_sort_func (GtkTreeSortable        *sortable);
static gint               thunar_list_model_cmp                   (ThunarListModel        *store,
                                                                   ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               thunar_list_model_cmp_array             (gconstpointer           a,
                                                                   gconstpointer           b,
                                                                   gpointer                user_data);
static gint               thunar_list_model_cmp_slist             (gconstpointer           a,
                                                                   gconstpointer           b,
                                                                   gpointer                user_data);
static gboolean           thunar_list_model_remove                (ThunarListModel        *store,
                                                                   GtkTreeIter            *iter,
                                                                   gboolean                silently);
static void               thunar_list_model_sort                  (ThunarListModel        *store);
static void               thunar_list_model_file_changed          (ThunarFile             *file,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_file_destroy          (ThunarFile             *file,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_folder_destroy        (ThunarFolder           *folder,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_files_added           (ThunarFolder           *folder,
                                                                   GSList                 *files,
                                                                   ThunarListModel        *store);
static gint               sort_by_date_accessed                   (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_date_modified                   (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_mime_type                       (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_name                            (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_permissions                     (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_size                            (ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               sort_by_type                            (ThunarFile             *a,
                                                                   ThunarFile             *b);



struct _ThunarListModelClass
{
  GtkObjectClass __parent__;
};

struct _ThunarListModel
{
  GtkObject __parent__;

  guint          stamp;
  Row           *rows;
  gint           nrows;
  GSList        *hidden;
  ThunarFolder  *folder;
  gboolean       show_hidden;

  /* ids and closures for the "changed" and "destroy" signals
   * of ThunarFile's used to speed up signal registrations.
   */
  GClosure      *file_changed_closure;
  GClosure      *file_destroy_closure;
  gint           file_changed_id;
  gint           file_destroy_id;

  gboolean       sort_folders_first;
  gint           sort_sign;   /* 1 = ascending, -1 descending */
  gint         (*sort_func) (ThunarFile *a,
                             ThunarFile *b);
};

struct _Row
{
  ThunarFile *file;
  guint       changed_id;
  guint       destroy_id;
  Row        *next;
};

struct _SortTuple
{
  gint offset;
  Row *row;
};



static GMemChunk *row_chunk;



G_DEFINE_TYPE_WITH_CODE (ThunarListModel,
                         thunar_list_model,
                         GTK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                thunar_list_model_tree_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                thunar_list_model_drag_source_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
                                                thunar_list_model_drag_dest_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
                                                thunar_list_model_sortable_init));



static void
thunar_list_model_class_init (ThunarListModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = thunar_list_model_finalize;
  gobject_class->dispose      = thunar_list_model_dispose;
  gobject_class->get_property = thunar_list_model_get_property;
  gobject_class->set_property = thunar_list_model_set_property;

  row_chunk = g_mem_chunk_new ("ThunarListModel::Row",
                               sizeof (Row),
                               sizeof (Row) * 128,
                               G_ALLOC_AND_FREE);

  /**
   * ThunarListModel:folder:
   *
   * The folder presented by this #ThunarListModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLDER,
                                   g_param_spec_object ("folder",
                                                        _("Filer Vfs Folder"),
                                                        _("The stores folder"),
                                                        THUNAR_TYPE_FOLDER,
                                                        G_PARAM_READWRITE));

  /**
   * ThunarListModel::folders-first:
   *
   * Tells whether to always sort folders before other files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLDERS_FIRST,
                                   g_param_spec_boolean ("folders-first",
                                                         _("Folders first"),
                                                         _("Folders first in sorting"),
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /**
   * ThunarListModel::num-files:
   *
   * The number of files in the folder presented by this #ThunarListModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NUM_FILES,
                                   g_param_spec_uint ("num-files",
                                                      _("Number of files"),
                                                      _("Number of visible files"),
                                                      0, UINT_MAX, 0,
                                                      G_PARAM_READABLE));

  /**
   * ThunarListModel::show-hidden:
   *
   * Tells whether to include hidden (and backup) files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HIDDEN,
                                   g_param_spec_boolean ("show-hidden",
                                                         _("Show hidden"),
                                                         _("Whether to display hidden files"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}



static void
thunar_list_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags        = thunar_list_model_get_flags;
  iface->get_n_columns    = thunar_list_model_get_n_columns;
  iface->get_column_type  = thunar_list_model_get_column_type;
  iface->get_iter         = thunar_list_model_get_iter;
  iface->get_path         = thunar_list_model_get_path;
  iface->get_value        = thunar_list_model_get_value;
  iface->iter_next        = thunar_list_model_iter_next;
  iface->iter_children    = thunar_list_model_iter_children;
  iface->iter_has_child   = thunar_list_model_iter_has_child;
  iface->iter_n_children  = thunar_list_model_iter_n_children;
  iface->iter_nth_child   = thunar_list_model_iter_nth_child;
  iface->iter_parent      = thunar_list_model_iter_parent;
}



static void
thunar_list_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable    = thunar_list_model_row_draggable;
  iface->drag_data_delete = thunar_list_model_drag_data_delete;
  iface->drag_data_get    = thunar_list_model_drag_data_get;
}



static void
thunar_list_model_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = thunar_list_model_drag_data_received;
  iface->row_drop_possible = thunar_list_model_row_drop_possible;
}



static void
thunar_list_model_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id     = thunar_list_model_get_sort_column_id;
  iface->set_sort_column_id     = thunar_list_model_set_sort_column_id;
  iface->set_sort_func          = thunar_list_model_set_sort_func;
  iface->set_default_sort_func  = thunar_list_model_set_default_sort_func;
  iface->has_default_sort_func  = thunar_list_model_has_default_sort_func;
}



static void
thunar_list_model_init (ThunarListModel *store)
{
  store->stamp                = g_random_int ();
  store->rows                 = NULL;
  store->nrows                = 0;
  store->hidden               = NULL;
  store->folder               = NULL;
  store->show_hidden          = FALSE;

  store->file_changed_closure = g_cclosure_new_object (G_CALLBACK (thunar_list_model_file_changed), G_OBJECT (store));
  store->file_destroy_closure = g_cclosure_new_object (G_CALLBACK (thunar_list_model_file_destroy), G_OBJECT (store));
  store->file_changed_id      = g_signal_lookup ("changed", THUNAR_TYPE_FILE);
  store->file_destroy_id      = g_signal_lookup ("destroy", THUNAR_TYPE_FILE);

  store->sort_folders_first   = TRUE;
  store->sort_sign            = 1;
  store->sort_func            = sort_by_name;

  /* take over ownership of the closures */
  g_closure_ref (store->file_changed_closure);
  g_closure_ref (store->file_destroy_closure);
  g_closure_sink (store->file_changed_closure);
  g_closure_sink (store->file_destroy_closure);
}



static void
thunar_list_model_finalize (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  /* get rid of the closures */
  g_closure_unref (store->file_changed_closure);
  g_closure_unref (store->file_destroy_closure);

  G_OBJECT_CLASS (thunar_list_model_parent_class)->finalize (object);
}



static void
thunar_list_model_dispose (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  /* unlink from the folder (if any) */
  thunar_list_model_set_folder (store, NULL);

  G_OBJECT_CLASS (thunar_list_model_parent_class)->dispose (object);
}



static void
thunar_list_model_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_FOLDER:
      g_value_set_object (value, thunar_list_model_get_folder (store));
      break;

    case PROP_FOLDERS_FIRST:
      g_value_set_boolean (value, thunar_list_model_get_folders_first (store));
      break;

    case PROP_NUM_FILES:
      g_value_set_uint (value, thunar_list_model_get_num_files (store));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_list_model_get_show_hidden (store));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_list_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_FOLDER:
      thunar_list_model_set_folder (store, g_value_get_object (value));
      break;

    case PROP_FOLDERS_FIRST:
      thunar_list_model_set_folders_first (store, g_value_get_boolean (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_list_model_set_show_hidden (store, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_list_model_get_flags (GtkTreeModel *model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_list_model_get_n_columns (GtkTreeModel *model)
{
  return THUNAR_LIST_MODEL_N_COLUMNS;
}



static GType
thunar_list_model_get_column_type (GtkTreeModel *model,
                                   gint          index)
{
  switch (index)
    {
    case THUNAR_LIST_MODEL_COLUMN_DATE_ACCESSED:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_ICON_SMALL:
      return GDK_TYPE_PIXBUF;

    case THUNAR_LIST_MODEL_COLUMN_ICON_NORMAL:
      return GDK_TYPE_PIXBUF;

    case THUNAR_LIST_MODEL_COLUMN_ICON_LARGE:
      return GDK_TYPE_PIXBUF;

    case THUNAR_LIST_MODEL_COLUMN_MIME_TYPE:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_PERMISSIONS:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_SIZE:
      return G_TYPE_STRING;

    case THUNAR_LIST_MODEL_COLUMN_TYPE:
      return G_TYPE_STRING;
    }

  g_assert_not_reached ();
  return (GType) -1;
}



static gboolean
thunar_list_model_get_iter (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            GtkTreePath  *path)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  gint             index;
  gint             n;
  Row             *row;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  index = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (index >= store->nrows))
    return FALSE;

  for (n = 0, row = store->rows; n < index; ++n, row = row->next)
    g_assert (row != NULL);

  iter->stamp = store->stamp;
  iter->user_data = row;

  return TRUE;
}



static GtkTreePath*
thunar_list_model_get_path (GtkTreeModel *model,
                            GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GtkTreePath     *path;
  gint             index = 0;
  Row             *row;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  g_return_val_if_fail (iter->stamp == store->stamp, NULL);

  for (row = store->rows; row != NULL; ++index, row = row->next)
    if (row == iter->user_data)
      break;

  if (G_UNLIKELY (row == NULL))
    return NULL;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, index);
  return path;
}



static void
thunar_list_model_get_value (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ExoMimeInfo *mime_info;
  GdkPixbuf   *icon;
  gchar       *str;
  Row         *row;

  g_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  g_return_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp);

  row = (Row *) iter->user_data;
  g_assert (row != NULL);

  switch (column)
    {
    case THUNAR_LIST_MODEL_COLUMN_DATE_ACCESSED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (row->file, THUNAR_FILE_DATE_ACCESSED);
      if (G_LIKELY (str != NULL))
        g_value_take_string (value, str);
      else
        g_value_set_static_string (value, _("unknown"));
      break;

    case THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (row->file, THUNAR_FILE_DATE_MODIFIED);
      if (G_LIKELY (str != NULL))
        g_value_take_string (value, str);
      else
        g_value_set_static_string (value, _("unknown"));
      break;

    case THUNAR_LIST_MODEL_COLUMN_ICON_SMALL:
      g_value_init (value, GDK_TYPE_PIXBUF);
      icon = thunar_file_load_icon (row->file, 22);
      g_value_set_object (value, icon);
      g_object_unref (G_OBJECT (icon));
      break;

    case THUNAR_LIST_MODEL_COLUMN_ICON_NORMAL:
      g_value_init (value, GDK_TYPE_PIXBUF);
      icon = thunar_file_load_icon (row->file, 48);
      g_value_set_object (value, icon);
      g_object_unref (G_OBJECT (icon));
      break;

    case THUNAR_LIST_MODEL_COLUMN_ICON_LARGE:
      g_value_init (value, GDK_TYPE_PIXBUF);
      icon = thunar_file_load_icon (row->file, 64);
      g_value_set_object (value, icon);
      g_object_unref (G_OBJECT (icon));
      break;

    case THUNAR_LIST_MODEL_COLUMN_MIME_TYPE:
      g_value_init (value, G_TYPE_STRING);
      mime_info = thunar_file_get_mime_info (row->file);
      if (G_LIKELY (mime_info != NULL))
        {
          g_value_set_static_string (value, exo_mime_info_get_name (mime_info));
          g_object_unref (G_OBJECT (mime_info));
        }
      else
        g_value_set_static_string (value, _("unknown"));
      break;

    case THUNAR_LIST_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_display_name (row->file));
      break;

    case THUNAR_LIST_MODEL_COLUMN_PERMISSIONS:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, thunar_file_get_mode_string (row->file));
      break;

    case THUNAR_LIST_MODEL_COLUMN_SIZE:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_size_string (row->file);
      if (G_LIKELY (str != NULL))
        g_value_take_string (value, str);
      else
        g_value_set_static_string (value, "--");
      break;

    case THUNAR_LIST_MODEL_COLUMN_TYPE:
      g_value_init (value, G_TYPE_STRING);
      mime_info = thunar_file_get_mime_info (row->file);
      if (G_LIKELY (mime_info != NULL))
        {
          g_value_set_static_string (value, exo_mime_info_get_comment (mime_info));
          g_object_unref (G_OBJECT (mime_info));
        }
      else
        g_value_set_static_string (value, _("unknown"));
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
thunar_list_model_iter_next (GtkTreeModel *model,
                             GtkTreeIter  *iter)
{
  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), FALSE);
  g_return_val_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp, FALSE);

  iter->user_data = ((Row *) iter->user_data)->next;
  return (iter->user_data != NULL);
}



static gboolean
thunar_list_model_iter_children (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (parent != NULL)
    return FALSE;

  if (G_LIKELY (store->rows != NULL))
    {
      iter->stamp = store->stamp;
      iter->user_data = store->rows;
      return TRUE;
    }
  else
    return FALSE;
}



static gboolean
thunar_list_model_iter_has_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_list_model_iter_n_children (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);

  return (iter == NULL) ? store->nrows : 0;
}



static gboolean
thunar_list_model_iter_nth_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  Row        *row;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_UNLIKELY (parent != NULL))
    return FALSE;

  for (row = store->rows; row != NULL && n > 0; --n, row = row->next)
    ;

  if (G_LIKELY (row != NULL))
    {
      iter->stamp = store->stamp;
      iter->user_data = row;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_parent (GtkTreeModel *model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
thunar_list_model_row_draggable (GtkTreeDragSource *source,
                                 GtkTreePath       *path)
{
  return TRUE;
}



static gboolean
thunar_list_model_drag_data_delete (GtkTreeDragSource *source,
                                    GtkTreePath       *path)
{
  GtkTreeIter iter;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (source), FALSE);

  if (thunar_list_model_get_iter (GTK_TREE_MODEL (source), &iter, path))
    {
      thunar_list_model_remove (THUNAR_LIST_MODEL (source), &iter, FALSE);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_drag_data_get (GtkTreeDragSource *source,
                                 GtkTreePath       *path,
                                 GtkSelectionData  *data)
{
  if (gtk_tree_set_row_drag_data (data, (GtkTreeModel *) source, path))
    return TRUE;
  else
    return FALSE;
}



static gboolean
thunar_list_model_drag_data_received (GtkTreeDragDest  *dest,
                                      GtkTreePath      *path,
                                      GtkSelectionData *data)
{
  // TODO: Implement this function
  return FALSE;
}



static gboolean
thunar_list_model_row_drop_possible (GtkTreeDragDest  *dest,
                                     GtkTreePath      *path,
                                     GtkSelectionData *data)
{
  // TODO: Implement this function
  return FALSE;
}



static gboolean
thunar_list_model_get_sort_column_id (GtkTreeSortable *sortable,
                                      gint            *sort_column_id,
                                      GtkSortType     *order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (store->sort_func == sort_by_mime_type)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_MIME_TYPE;
  else if (store->sort_func == sort_by_name)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_NAME;
  else if (store->sort_func == sort_by_permissions)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_PERMISSIONS;
  else if (store->sort_func == sort_by_size)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_SIZE;
  else if (store->sort_func == sort_by_date_accessed)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_DATE_ACCESSED;
  else if (store->sort_func == sort_by_date_modified)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED;
  else if (store->sort_func == sort_by_type)
    *sort_column_id = THUNAR_LIST_MODEL_COLUMN_TYPE;
  else
    g_assert_not_reached ();

  if (store->sort_sign > 0)
    *order = GTK_SORT_ASCENDING;
  else
    *order = GTK_SORT_DESCENDING;

  return TRUE;
}



static void
thunar_list_model_set_sort_column_id (GtkTreeSortable *sortable,
                                      gint             sort_column_id,
                                      GtkSortType      order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  switch (sort_column_id)
    {
    case THUNAR_LIST_MODEL_COLUMN_DATE_ACCESSED:
      store->sort_func = sort_by_date_accessed;
      break;

    case THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED:
      store->sort_func = sort_by_date_modified;
      break;

    case THUNAR_LIST_MODEL_COLUMN_MIME_TYPE:
      store->sort_func = sort_by_mime_type;
      break;

    case THUNAR_LIST_MODEL_COLUMN_NAME:
      store->sort_func = sort_by_name;
      break;

    case THUNAR_LIST_MODEL_COLUMN_PERMISSIONS:
      store->sort_func = sort_by_permissions;
      break;

    case THUNAR_LIST_MODEL_COLUMN_SIZE:
      store->sort_func = sort_by_size;
      break;

    case THUNAR_LIST_MODEL_COLUMN_TYPE:
      store->sort_func = sort_by_type;
      break;

    default:
      g_assert_not_reached ();
    }

  store->sort_sign = (order == GTK_SORT_ASCENDING) ? 1 : -1;

  /* re-sort the store */
  thunar_list_model_sort (store);

  /* notify listining parties */
  gtk_tree_sortable_sort_column_changed (sortable);
}



static void
thunar_list_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                         GtkTreeIterCompareFunc func,
                                         gpointer               data,
                                         GtkDestroyNotify       destroy)
{
  g_error ("ThunarListModel has sorting facilities built-in!");
}



static void
thunar_list_model_set_sort_func (GtkTreeSortable       *sortable,
                                 gint                   sort_column_id,
                                 GtkTreeIterCompareFunc func,
                                 gpointer               data,
                                 GtkDestroyNotify       destroy)
{
  g_error ("ThunarListModel has sorting facilities built-in!");
}



static gboolean
thunar_list_model_has_default_sort_func (GtkTreeSortable *sortable)
{
  return FALSE;
}



static gint
thunar_list_model_cmp (ThunarListModel *store,
                       ThunarFile      *a,
                       ThunarFile      *b)
{
  gboolean isdir_a;
  gboolean isdir_b;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), -1);
  g_return_val_if_fail (THUNAR_IS_FILE (a), -1);
  g_return_val_if_fail (THUNAR_IS_FILE (b), -1);

  if (G_LIKELY (store->sort_folders_first))
    {
      isdir_a = thunar_file_is_directory (a);
      isdir_b = thunar_file_is_directory (b);

      if (isdir_a && !isdir_b)
        return -1;
      else if (!isdir_a && isdir_b)
        return 1;
    }

  return store->sort_func (a, b) * store->sort_sign;
}



static gint
thunar_list_model_cmp_array (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  return thunar_list_model_cmp (THUNAR_LIST_MODEL (user_data),
                                ((SortTuple *) a)->row->file,
                                ((SortTuple *) b)->row->file);
}



static gint
thunar_list_model_cmp_slist (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  return -thunar_list_model_cmp (THUNAR_LIST_MODEL (user_data),
                                 THUNAR_FILE (a),
                                 THUNAR_FILE (b));
}



static gboolean
thunar_list_model_remove (ThunarListModel  *store,
                          GtkTreeIter *iter,
                          gboolean     silently)
{
  GtkTreePath *path;
  Row         *next;
  Row         *prev = NULL;
  Row         *tmp;
  Row         *row;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  g_return_val_if_fail (iter->stamp == store->stamp, FALSE);

  row  = (Row *) iter->user_data;
  next = row->next;
  path = thunar_list_model_get_path (GTK_TREE_MODEL (store), iter);

  /* delete data associated with this row */
  g_signal_handler_disconnect (G_OBJECT (row->file), row->changed_id);
  g_signal_handler_disconnect (G_OBJECT (row->file), row->destroy_id);
  g_object_unref (row->file);

  /* remove the link from the list */
  for (tmp = store->rows; tmp != NULL; prev = tmp, tmp = tmp->next)
    {
      if (tmp == row)
        {
          if (prev != NULL)
            prev->next = tmp->next;
          else
            store->rows = tmp->next;

          tmp->next = NULL;
          store->nrows -= 1;
          break;
        }
    }
  g_chunk_free (row, row_chunk);

  /* notify other parties */
  if (G_LIKELY (!silently))
    {
      g_object_notify (G_OBJECT (store), "num-files");
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
    }
  gtk_tree_path_free (path);

  if (next != NULL)
    {
      iter->stamp     = store->stamp;
      iter->user_data = next;
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      return FALSE;
    }
}



static void
thunar_list_model_sort (ThunarListModel *store)
{
  GtkTreePath *path;
  SortTuple    tuple;
  GArray      *sort_array;
  gint        *new_order;
  gint         n;
  Row         *row;

  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  if (G_UNLIKELY (store->nrows <= 1))
    return;

  sort_array = g_array_sized_new (FALSE, FALSE,
                                  sizeof (SortTuple),
                                  store->nrows);

  for (n = 0, row = store->rows; n < store->nrows; ++n, row = row->next)
    {
      g_assert (row != NULL);

      tuple.offset = n;
      tuple.row = row;
      g_array_append_val (sort_array, tuple);
    }

  g_array_sort_with_data (sort_array, thunar_list_model_cmp_array, store);

  /* update our internals */
  for (n = 0; n < store->nrows - 1; ++n)
    g_array_index (sort_array, SortTuple, n).row->next = g_array_index (sort_array, SortTuple, n + 1).row;
  g_array_index (sort_array, SortTuple, n).row->next = NULL;
  store->rows = g_array_index (sort_array, SortTuple, 0).row;

  /* let the world know about our new order */
  new_order = g_new (gint, store->nrows);
  for (n = 0; n < store->nrows; ++n)
    new_order[n] = g_array_index (sort_array, SortTuple, n).offset;

  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
  gtk_tree_path_free (path);

  g_array_free (sort_array, TRUE);
  g_free (new_order);
}



static void
thunar_list_model_file_changed (ThunarFile      *file,
                                ThunarListModel *store)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  gint         n;
  Row         *row;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  for (n = 0, row = store->rows; row != NULL; ++n, row = row->next)
    if (G_UNLIKELY (row->file == file))
      {
        iter.stamp = store->stamp;
        iter.user_data = row;

        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, n);
        gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
        gtk_tree_path_free (path);
        return;
      }

  g_assert_not_reached ();
}



static void
thunar_list_model_file_destroy (ThunarFile      *file,
                                ThunarListModel *store)
{
  GtkTreeIter iter;
  Row        *row;

  /* check if file is currently shown */
  for (row = store->rows; row != NULL; row = row->next)
    if (row->file == file)
      {
        iter.stamp = store->stamp;
        iter.user_data = row;
        thunar_list_model_remove (store, &iter, FALSE);
        return;
      }

  /* file is hidden */
  g_assert (g_slist_find (store->hidden, file) != NULL);
  g_signal_handlers_disconnect_matched (G_OBJECT (file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                        store->file_destroy_id, 0, store->file_destroy_closure, NULL, NULL);
  store->hidden = g_slist_remove (store->hidden, file);
  g_object_unref (G_OBJECT (file));
}



static void
thunar_list_model_folder_destroy (ThunarFolder    *folder,
                                  ThunarListModel *store)
{
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  thunar_list_model_set_folder (store, NULL);

  // TODO: What to do when the folder is deleted?
}



static void
thunar_list_model_files_added (ThunarFolder    *folder,
                               GSList          *files,
                               ThunarListModel *store)
{
  GtkTreePath  *path;
  GtkTreeIter   iter;
  ThunarFile   *file;
  Row          *row;
  Row          *prev;

  // TODO: pre-sort files to get faster insert?!
  for (; files != NULL; files = files->next)
    {
      /* take a reference on that file */
      file = THUNAR_FILE (files->data);
      g_object_ref (G_OBJECT (file));

      /* check if the file should be hidden */
      if (!store->show_hidden && thunar_file_is_hidden (file))
        {
          g_signal_connect_closure_by_id (G_OBJECT (file), store->file_destroy_id,
                                          0, store->file_destroy_closure, TRUE);
          store->hidden = g_slist_prepend (store->hidden, file);
        }
      else
        {
          row = g_chunk_new (Row, row_chunk);
          row->file = file;
          row->changed_id = g_signal_connect_closure_by_id (G_OBJECT (file), store->file_changed_id,
                                                            0, store->file_changed_closure, TRUE);
          row->destroy_id = g_signal_connect_closure_by_id (G_OBJECT (file), store->file_destroy_id,
                                                            0, store->file_destroy_closure, TRUE);

          /* find the position to insert the file to */
          if (G_UNLIKELY (store->rows == NULL || thunar_list_model_cmp (store, file, store->rows->file) < 0))
            {
              row->next   = store->rows;
              store->rows = row;
            }
          else
            {
              for (prev = store->rows; prev->next != NULL; prev = prev->next)
                if (thunar_list_model_cmp (store, file, prev->next->file) < 0)
                  break;

              row->next  = prev->next;
              prev->next = row;
            }

          iter.stamp = store->stamp;
          iter.user_data = row;

          path = thunar_list_model_get_path (GTK_TREE_MODEL (store), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);

          store->nrows += 1;
        }
    }

  /* number of visible files may have changed */
  g_object_notify (G_OBJECT (store), "num-files");
}



static gint
sort_by_date_accessed (ThunarFile *a,
                       ThunarFile *b)
{
  ThunarVfsFileTime date_a;
  ThunarVfsFileTime date_b;
  gboolean          can_a;
  gboolean          can_b;

  can_a = thunar_file_get_date (a, THUNAR_FILE_DATE_ACCESSED, &date_a);
  can_b = thunar_file_get_date (b, THUNAR_FILE_DATE_ACCESSED, &date_b);

  if (G_UNLIKELY (!can_a && !can_b))
    return 0;
  else if (G_UNLIKELY (!can_a))
    return -1;
  else if (G_UNLIKELY (!can_b))
    return 1;

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;
  return 0;
}



static gint
sort_by_date_modified (ThunarFile *a,
                       ThunarFile *b)
{
  ThunarVfsFileTime date_a;
  ThunarVfsFileTime date_b;
  gboolean          can_a;
  gboolean          can_b;

  can_a = thunar_file_get_date (a, THUNAR_FILE_DATE_MODIFIED, &date_a);
  can_b = thunar_file_get_date (b, THUNAR_FILE_DATE_MODIFIED, &date_b);

  if (G_UNLIKELY (!can_a && !can_b))
    return 0;
  else if (G_UNLIKELY (!can_a))
    return -1;
  else if (G_UNLIKELY (!can_b))
    return 1;

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;
  return 0;
}



static gint
sort_by_mime_type (ThunarFile *a,
                   ThunarFile *b)
{
  ExoMimeInfo *info_a;
  ExoMimeInfo *info_b;
  gint         result;

  info_a = thunar_file_get_mime_info (a);
  info_b = thunar_file_get_mime_info (b);

  if (G_UNLIKELY (info_a == NULL && info_b == NULL))
    return 0;
  else if (G_UNLIKELY (info_a == NULL))
    {
      g_object_unref (G_OBJECT (info_b));
      return -1;
    }
  else if (G_UNLIKELY (info_b == NULL))
    {
      g_object_unref (G_OBJECT (info_a));
      return 1;
    }

  result = strcasecmp (exo_mime_info_get_name (info_a),
                       exo_mime_info_get_name (info_b));

  g_object_unref (G_OBJECT (info_b));
  g_object_unref (G_OBJECT (info_a));

  return result;
}



static gint
sort_by_name (ThunarFile *a,
              ThunarFile *b)
{
  return strcmp (thunar_file_get_display_name (a),
                 thunar_file_get_display_name (b));
}



static gint
sort_by_permissions (ThunarFile *a,
                     ThunarFile *b)
{
  ThunarVfsFileMode mode_a;
  ThunarVfsFileMode mode_b;

  mode_a = thunar_file_get_mode (a);
  mode_b = thunar_file_get_mode (b);

  if (mode_a < mode_b)
    return -1;
  else if (mode_a > mode_b)
    return 1;
  return 0;
}



static gint
sort_by_size (ThunarFile *a,
              ThunarFile *b)
{
  ThunarVfsFileSize size_a;
  ThunarVfsFileSize size_b;
  gboolean          can_a;
  gboolean          can_b;

  can_a = thunar_file_get_size (a, &size_a);
  can_b = thunar_file_get_size (b, &size_b);

  if (G_UNLIKELY (!can_a && !can_b))
    return 0;
  else if (G_UNLIKELY (!can_a))
    return -1;
  else if (G_UNLIKELY (!can_b))
    return 1;

  if (size_a < size_b)
    return -1;
  else if (size_a > size_b)
    return 1;
  return 0;
}



static gint
sort_by_type (ThunarFile *a,
              ThunarFile *b)
{
  ExoMimeInfo *info_a;
  ExoMimeInfo *info_b;
  gint         result;

  info_a = thunar_file_get_mime_info (a);
  info_b = thunar_file_get_mime_info (b);

  if (G_UNLIKELY (info_a == NULL && info_b == NULL))
    return 0;
  else if (G_UNLIKELY (info_a == NULL))
    {
      g_object_unref (G_OBJECT (info_b));
      return -1;
    }
  else if (G_UNLIKELY (info_b == NULL))
    {
      g_object_unref (G_OBJECT (info_a));
      return 1;
    }

  result = strcasecmp (exo_mime_info_get_comment (info_a),
                       exo_mime_info_get_comment (info_b));

  g_object_unref (G_OBJECT (info_b));
  g_object_unref (G_OBJECT (info_a));

  return result;
}



/**
 * thunar_list_model_new:
 *
 * Allocates a new #ThunarListModel not associated with
 * any #ThunarFolder.
 *
 * Return value: the newly allocated #ThunarListModel.
 **/
ThunarListModel*
thunar_list_model_new (void)
{
  ThunarListModel *store;

  /* allocate the new list model */
  store = g_object_new (THUNAR_TYPE_LIST_MODEL, NULL);

  /* drop the floating reference */
  g_object_ref (G_OBJECT (store));
  gtk_object_sink (GTK_OBJECT (store));

  return store;
}



/**
 * thunar_list_model_new_with_folder:
 * @folder : a valid #ThunarFolder object.
 *
 * Allocates a new #ThunarListModel instance and associates
 * it with @folder.
 *
 * Return value: the newly created #ThunarListModel instance.
 **/
ThunarListModel*
thunar_list_model_new_with_folder (ThunarFolder *folder)
{
  ThunarListModel *store;

  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);

  /* allocate the new list model */
  store = g_object_new (THUNAR_TYPE_LIST_MODEL,
                        "folder", folder,
                        NULL);

  /* drop the floating reference */
  g_object_ref (G_OBJECT (store));
  gtk_object_sink (GTK_OBJECT (store));

  return store;
}



/**
 * thunar_list_model_get_folder:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the #ThunarFolder @store is associated with
 *               or %NULL if @store has no folder.
 **/
ThunarFolder*
thunar_list_model_get_folder (ThunarListModel *store)
{
  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  return store->folder;
}



/**
 * thunar_list_model_set_folder:
 * @store  : a valid #ThunarListModel.
 * @folder : a #ThunarFolder or %NULL.
 **/
void
thunar_list_model_set_folder (ThunarListModel *store,
                              ThunarFolder    *folder)
{
  ThunarFile *file;
  GtkTreePath  *path;
  GtkTreeIter   iter;
  GSList       *files;
  GSList       *lp;
  Row          *row;

  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  g_return_if_fail (folder == NULL || THUNAR_IS_FOLDER (folder));

  /* check if we're not already using that folder */
  if (G_UNLIKELY (store->folder == folder))
    return;

  /* unlink from the previously active folder (if any) */
  if (G_LIKELY (store->folder != NULL))
    {
      /* remove existing entries (FIXME: this could be done faster!) */
      while (store->rows)
        {
          iter.stamp = store->stamp;
          iter.user_data = store->rows;
          thunar_list_model_remove (store, &iter, FALSE);
        }

      /* remove hidden entries */
      for (lp = store->hidden; lp != NULL; lp = lp->next)
        {
          g_signal_handlers_disconnect_matched (G_OBJECT (lp->data), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                                store->file_destroy_id, 0, store->file_destroy_closure, NULL, NULL);
          g_object_unref (G_OBJECT (lp->data));
        }
      g_slist_free (store->hidden);
      store->hidden = NULL;

      /* unregister signals and drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (store->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (G_OBJECT (store->folder));
    }

  /* ... just to be sure! */
  g_assert (store->rows == NULL);
  g_assert (store->nrows == 0);

  /* activate the new folder */
  store->folder = folder;

  /* connect to the new folder (if any) */
  if (folder != NULL)
    {
      g_object_ref (G_OBJECT (folder));

      /* sort the files _before_ adding them to the store (reverse order -> prepend below) */
      files = g_slist_copy (thunar_folder_get_files (folder));
      if (G_LIKELY (files != NULL))
        {
          files = g_slist_sort_with_data (files, thunar_list_model_cmp_slist, store);

          /* insert the files */
          for (lp = files; lp != NULL; lp = lp->next)
            {
              /* take a reference on the file */
              file = THUNAR_FILE (lp->data);
              g_object_ref (G_OBJECT (file));

              /* check if this file should be shown/hidden */
              if (!store->show_hidden && thunar_file_is_hidden (file))
                {
                  g_signal_handlers_disconnect_matched (G_OBJECT (file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                                        store->file_destroy_id, 0, store->file_destroy_closure, NULL, NULL);
                  store->hidden = g_slist_prepend (store->hidden, file);
                }
              else
                {
                  row = g_chunk_new (Row, row_chunk);
                  row->file = file;
                  row->changed_id = g_signal_connect_closure_by_id (G_OBJECT (file), store->file_changed_id,
                                                                    0, store->file_changed_closure, TRUE);
                  row->destroy_id = g_signal_connect_closure_by_id (G_OBJECT (file), store->file_destroy_id,
                                                                    0, store->file_destroy_closure, TRUE);
                  row->next = store->rows;

                  store->rows = row;
                  store->nrows += 1;
                }
            }

          /* notify other parties */
          path = gtk_tree_path_new ();
          gtk_tree_path_append_index (path, 0);
          for (row = store->rows; row != NULL; row = row->next)
            {
              iter.stamp = store->stamp;
              iter.user_data = row;
              gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
              gtk_tree_path_next (path);
            }
          gtk_tree_path_free (path);

          /* cleanup */
          g_slist_free (files);
        }

      /* connect signals to the new folder */
      g_signal_connect (G_OBJECT (store->folder), "files-added",
                        G_CALLBACK (thunar_list_model_files_added), store);
      g_signal_connect (G_OBJECT (store->folder), "destroy",
                        G_CALLBACK (thunar_list_model_folder_destroy), store);
    }

  /* notify listeners that we have a new folder */
  g_object_notify (G_OBJECT (store), "folder");
  g_object_notify (G_OBJECT (store), "num-files");
}



/**
 * thunar_list_model_get_folders_first:
 * @store : a #ThunarListModel.
 *
 * Determines whether @store lists folders first.
 *
 * Return value: %TRUE if @store lists folders first.
 **/
gboolean
thunar_list_model_get_folders_first (ThunarListModel *store)
{
  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->sort_folders_first;
}



/**
 * thunar_list_model_set_folders_first:
 * @store         : a #ThunarListModel.
 * @folders_first : %TRUE to let @store list folders first.
 **/
void
thunar_list_model_set_folders_first (ThunarListModel *store,
                                     gboolean         folders_first)
{
  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the new setting differs */
  if ((store->sort_folders_first && folders_first)
   || (!store->sort_folders_first && !folders_first))
    return;

  /* apply the new setting (re-sorting the store) */
  store->sort_folders_first = folders_first;
  g_object_notify (G_OBJECT (store), "folders-first");
  thunar_list_model_sort (store);
}



/**
 * thunar_list_model_get_show_hidden:
 * @store : a #ThunarListModel.
 *
 * Return value: %TRUE if hidden files will be shown, else %FALSE.
 **/
gboolean
thunar_list_model_get_show_hidden (ThunarListModel *store)
{
  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->show_hidden;
}



/**
 * thunar_list_model_set_show_hidden:
 * @store       : a #ThunarListModel.
 * @show_hidden : %TRUE if hidden files should be shown, else %FALSE.
 **/
void
thunar_list_model_set_show_hidden (ThunarListModel *store,
                                   gboolean         show_hidden)
{
  GtkTreePath  *path;
  GtkTreeIter   iter;
  ThunarFile   *file;
  GSList       *hidden_rows;
  GSList       *files;
  GSList       *lp;
  Row          *prev;
  Row          *row;

  g_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the settings differ */
  if ((store->show_hidden && show_hidden) || (!store->show_hidden && !show_hidden))
    return;

  store->show_hidden = show_hidden;

  if (store->show_hidden)
    {
      /* merge all hidden elements */
      for (files = store->hidden; files != NULL; files = files->next)
        {
          file = THUNAR_FILE (files->data);

          row = g_chunk_new (Row, row_chunk);
          row->file = file;
          row->changed_id = g_signal_connect_closure_by_id (G_OBJECT (file), store->file_changed_id,
                                                            0, store->file_changed_closure, TRUE);
          row->destroy_id = g_signal_handler_find (G_OBJECT (file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                                   store->file_destroy_id, 0, store->file_destroy_closure, NULL, NULL);

          if (G_UNLIKELY (store->rows == NULL
                       || thunar_list_model_cmp (store, file, store->rows->file) < 0))
            {
              row->next   = store->rows;
              store->rows = row;
            }
          else
            {
              for (prev = store->rows; prev->next != NULL; prev = prev->next)
                if (thunar_list_model_cmp (store, file, prev->next->file) < 0)
                  break;

              row->next = prev->next;
              prev->next = row;
            }

          iter.stamp = store->stamp;
          iter.user_data = row;

          path = thunar_list_model_get_path (GTK_TREE_MODEL (store), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);

          store->nrows += 1;
        }
      g_slist_free (store->hidden);
      store->hidden = NULL;
    }
  else
    {
      g_assert (store->hidden == NULL);

      /* unmerge all hidden elements */
      for (hidden_rows = files = NULL, row = store->rows; row != NULL; row = row->next)
        if (thunar_file_is_hidden (row->file))
          {
            g_signal_handlers_disconnect_matched (G_OBJECT (row->file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                                  store->file_changed_id, 0, store->file_changed_closure, NULL, NULL);
            hidden_rows = g_slist_prepend (hidden_rows, row);
            files = g_slist_prepend (files, g_object_ref (row->file));
          }

      if (files != NULL)
        {
          for (lp = hidden_rows; lp != NULL; lp = lp->next)
            {
              iter.stamp = store->stamp;
              iter.user_data = lp->data;
              thunar_list_model_remove (store, &iter, FALSE);
            }
          g_slist_free (hidden_rows);

          store->hidden = files;
        }
    }

  /* notify listeners about the new setting */
  g_object_notify (G_OBJECT (store), "num-files");
  g_object_notify (G_OBJECT (store), "show-hidden");
}



/**
 * thunar_list_model_get_file:
 * @store : a #ThunarListModel.
 * @iter  : a valid #GtkTreeIter for @store.
 *
 * Returns the #ThunarFile referred to by @iter. Free
 * the returned object using #g_object_unref() when
 * you are done with it.
 *
 * Return value: the #ThunarFile.
 **/
ThunarFile*
thunar_list_model_get_file (ThunarListModel *store,
                            GtkTreeIter     *iter)
{
  Row *row;

  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  g_return_val_if_fail (iter->stamp == store->stamp, NULL);

  row = (Row *) iter->user_data;

  g_assert (row != NULL);

  return g_object_ref (row->file);
}



/**
 * thunar_list_model_get_num_files:
 * @store : a #ThunarListModel.
 *
 * Counts the number of visible files for @store, taking into account the
 * current setting of "show-hidden".
 *
 * Return value: ahe number of visible files in @store.
 **/
gint
thunar_list_model_get_num_files (ThunarListModel *store)
{
  g_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);
  return store->nrows;
}




