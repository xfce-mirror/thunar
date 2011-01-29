/* $Id$ */
/*-
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>



/* Convenience macros */
#define G_SLIST(list) ((GSList *) (list))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
  PROP_DATE_STYLE,
  PROP_FOLDER,
  PROP_FOLDERS_FIRST,
  PROP_NUM_FILES,
  PROP_SHOW_HIDDEN,
};

/* Signal identifiers */
enum
{
  ERROR,
  LAST_SIGNAL,
};



typedef struct _SortTuple SortTuple;



static void               thunar_list_model_tree_model_init       (GtkTreeModelIface      *iface);
static void               thunar_list_model_drag_dest_init        (GtkTreeDragDestIface   *iface);
static void               thunar_list_model_sortable_init         (GtkTreeSortableIface   *iface);
static void               thunar_list_model_finalize              (GObject                *object);
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
                                                                   gint                    idx);
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
                                                                   GDestroyNotify          destroy);
static void               thunar_list_model_set_sort_func         (GtkTreeSortable        *sortable,
                                                                   gint                    sort_column_id,
                                                                   GtkTreeIterCompareFunc  func,
                                                                   gpointer                data,
                                                                   GDestroyNotify          destroy);
static gboolean           thunar_list_model_has_default_sort_func (GtkTreeSortable        *sortable);
static gint               thunar_list_model_cmp                   (ThunarListModel        *store,
                                                                   ThunarFile             *a,
                                                                   ThunarFile             *b);
static gint               thunar_list_model_cmp_array             (gconstpointer           a,
                                                                   gconstpointer           b,
                                                                   gpointer                user_data);
static gint               thunar_list_model_cmp_list              (gconstpointer           a,
                                                                   gconstpointer           b,
                                                                   gpointer                user_data);
static gboolean           thunar_list_model_remove                (ThunarListModel        *store,
                                                                   GtkTreeIter            *iter,
                                                                   gboolean                silently);
static void               thunar_list_model_sort                  (ThunarListModel        *store);
static void               thunar_list_model_file_changed          (ThunarFileMonitor      *file_monitor,
                                                                   ThunarFile             *file,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_folder_destroy        (ThunarFolder           *folder,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_folder_error          (ThunarFolder           *folder,
                                                                   const GError           *error,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_files_added           (ThunarFolder           *folder,
                                                                   GList                  *files,
                                                                   ThunarListModel        *store);
static void               thunar_list_model_files_removed         (ThunarFolder           *folder,
                                                                   GList                  *files,
                                                                   ThunarListModel        *store);
static gint               sort_by_date_accessed                   (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_date_modified                   (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_file_name                       (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_group                           (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_mime_type                       (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_name                            (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_owner                           (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_permissions                     (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_size                            (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_type                            (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);



struct _ThunarListModelClass
{
  GObjectClass __parent__;

  /* signals */
  void (*error) (ThunarListModel *store,
                 const GError    *error);
};

struct _ThunarListModel
{
  GObject __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint           stamp;
#endif

  gint            nrows;
  GSList         *rows;
  GSList         *hidden;
  ThunarFolder   *folder;
  gboolean        show_hidden;
  ThunarDateStyle date_style;

  /* Use the shared ThunarFileMonitor instance, so we
   * do not need to connect "changed" handler to every
   * file in the model.
   */
  ThunarFileMonitor      *file_monitor;

  /* ids for the "row-inserted" and "row-deleted" signals
   * of GtkTreeModel to speed up folder changing.
   */
  guint          row_inserted_id;
  guint          row_deleted_id;

  gboolean       sort_case_sensitive;
  gboolean       sort_folders_first;
  gint           sort_sign;   /* 1 = ascending, -1 descending */
  gint         (*sort_func) (const ThunarFile *a,
                             const ThunarFile *b,
                             gboolean          case_sensitive);
};

struct _SortTuple
{
  /* the order is important, see thunar_list_model_sort() */
  gint    offset;
  GSList *row;
};



static guint list_model_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (ThunarListModel, thunar_list_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_list_model_tree_model_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_list_model_drag_dest_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE, thunar_list_model_sortable_init))



static void
thunar_list_model_class_init (ThunarListModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = thunar_list_model_finalize;
  gobject_class->get_property = thunar_list_model_get_property;
  gobject_class->set_property = thunar_list_model_set_property;

  /**
   * ThunarListModel:case-sensitive:
   *
   * Tells whether the sorting should be case sensitive.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CASE_SENSITIVE,
                                   g_param_spec_boolean ("case-sensitive",
                                                         "case-sensitive",
                                                         "case-sensitive",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarListModel:date-style:
   *
   * The style used to format dates.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DATE_STYLE,
                                   g_param_spec_enum ("date-style",
                                                      "date-style",
                                                      "date-style",
                                                      THUNAR_TYPE_DATE_STYLE,
                                                      THUNAR_DATE_STYLE_SIMPLE,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarListModel:folder:
   *
   * The folder presented by this #ThunarListModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLDER,
                                   g_param_spec_object ("folder",
                                                        "folder",
                                                        "folder",
                                                        THUNAR_TYPE_FOLDER,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarListModel::folders-first:
   *
   * Tells whether to always sort folders before other files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLDERS_FIRST,
                                   g_param_spec_boolean ("folders-first",
                                                         "folders-first",
                                                         "folders-first",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarListModel::num-files:
   *
   * The number of files in the folder presented by this #ThunarListModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NUM_FILES,
                                   g_param_spec_uint ("num-files",
                                                      "num-files",
                                                      "num-files",
                                                      0, G_MAXUINT, 0,
                                                      EXO_PARAM_READABLE));

  /**
   * ThunarListModel::show-hidden:
   *
   * Tells whether to include hidden (and backup) files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HIDDEN,
                                   g_param_spec_boolean ("show-hidden",
                                                         "show-hidden",
                                                         "show-hidden",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarListModel::error:
   * @store : a #ThunarListModel.
   * @error : a #GError that describes the problem.
   *
   * Emitted when an error occurs while loading the
   * @store content.
   **/
  list_model_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarListModelClass, error),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
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
  /* generate a unique stamp if we're in debug mode */
#ifndef NDEBUG
  store->stamp                = g_random_int ();
#endif

  store->row_inserted_id      = g_signal_lookup ("row-inserted", GTK_TYPE_TREE_MODEL);
  store->row_deleted_id       = g_signal_lookup ("row-deleted", GTK_TYPE_TREE_MODEL);

  store->sort_case_sensitive  = TRUE;
  store->sort_folders_first   = TRUE;
  store->sort_sign            = 1;
  store->sort_func            = sort_by_name;

  /* connect to the shared ThunarFileMonitor, so we don't need to
   * connect "changed" to every single ThunarFile we own.
   */
  store->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (store->file_monitor), "file-changed", 
                    G_CALLBACK (thunar_list_model_file_changed), store);
}



static void
thunar_list_model_finalize (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  /* unlink from the folder (if any) */
  thunar_list_model_set_folder (store, NULL);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (G_OBJECT (store->file_monitor), thunar_list_model_file_changed, store);
  g_object_unref (G_OBJECT (store->file_monitor));

  (*G_OBJECT_CLASS (thunar_list_model_parent_class)->finalize) (object);
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
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_list_model_get_case_sensitive (store));
      break;

    case PROP_DATE_STYLE:
      g_value_set_enum (value, thunar_list_model_get_date_style (store));
      break;

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
    case PROP_CASE_SENSITIVE:
      thunar_list_model_set_case_sensitive (store, g_value_get_boolean (value));
      break;

    case PROP_DATE_STYLE:
      thunar_list_model_set_date_style (store, g_value_get_enum (value));
      break;

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
  return THUNAR_N_COLUMNS;
}



static GType
thunar_list_model_get_column_type (GtkTreeModel *model,
                                   gint          idx)
{
  switch (idx)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_MODIFIED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_GROUP:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_MIME_TYPE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_OWNER:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_PERMISSIONS:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_SIZE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_TYPE:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_COLUMN_FILE_NAME:
      return G_TYPE_STRING;
    }

  _thunar_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_list_model_get_iter (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            GtkTreePath  *path)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GSList          *row;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the row for the path */
  row = g_slist_nth (store->rows, gtk_tree_path_get_indices (path)[0]);
  if (G_LIKELY (row != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, row);
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_list_model_get_path (GtkTreeModel *model,
                            GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  gint             idx = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, NULL);

  idx = g_slist_position (store->rows, iter->user_data);
  if (G_LIKELY (idx >= 0))
    return gtk_tree_path_new_from_indices (idx, -1);

  return NULL;
}



static void
thunar_list_model_get_value (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ThunarGroup *group;
  const gchar *content_type;
  const gchar *name;
  const gchar *real_name;
  ThunarUser  *user;
  ThunarFile  *file;
  gchar       *str;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp);

  file = THUNAR_FILE (G_SLIST (iter->user_data)->data);

  switch (column)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_ACCESSED, THUNAR_LIST_MODEL (model)->date_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      g_value_init (value, G_TYPE_STRING);
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, THUNAR_LIST_MODEL (model)->date_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_GROUP:
      g_value_init (value, G_TYPE_STRING);
      group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          g_value_set_string (value, thunar_group_get_name (group));
          g_object_unref (G_OBJECT (group));
        }
      else
        {
          g_value_set_static_string (value, _("Unknown"));
        }
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_content_type (file));
      break;

    case THUNAR_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    case THUNAR_COLUMN_OWNER:
      g_value_init (value, G_TYPE_STRING);
      user = thunar_file_get_user (file);
      if (G_LIKELY (user != NULL))
        {
          /* determine sane display name for the owner */
          name = thunar_user_get_name (user);
          real_name = thunar_user_get_real_name (user);
          str = G_LIKELY (real_name != NULL) ? g_strdup_printf ("%s (%s)", real_name, name) : g_strdup (name);
          g_value_take_string (value, str);
          g_object_unref (G_OBJECT (user));
        }
      else
        {
          g_value_set_static_string (value, _("Unknown"));
        }
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, thunar_file_get_mode_string (file));
      break;

    case THUNAR_COLUMN_SIZE:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, thunar_file_get_size_string (file));
      break;

    case THUNAR_COLUMN_TYPE:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (thunar_file_is_symlink (file)))
        g_value_take_string (value, g_strdup_printf (_("link to %s"), thunar_file_get_symlink_target (file)));
      else
        {
          content_type = thunar_file_get_content_type (file);
          if (content_type != NULL)
            g_value_take_string (value, g_content_type_get_description (content_type));
        }
      break;

    case THUNAR_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, file);
      break;

    case THUNAR_COLUMN_FILE_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_basename (file));
      break;

    default:
      _thunar_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_list_model_iter_next (GtkTreeModel *model,
                             GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp, FALSE);

  iter->user_data = g_slist_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
thunar_list_model_iter_children (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL && store->rows != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, store->rows);
      return TRUE;
    }

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

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);

  return (iter == NULL) ? store->nrows : 0;
}



static gboolean
thunar_list_model_iter_nth_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, g_slist_nth (store->rows, n));
      return (iter->user_data != NULL);
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
thunar_list_model_drag_data_received (GtkTreeDragDest  *dest,
                                      GtkTreePath      *path,
                                      GtkSelectionData *data)
{
  return FALSE;
}



static gboolean
thunar_list_model_row_drop_possible (GtkTreeDragDest  *dest,
                                     GtkTreePath      *path,
                                     GtkSelectionData *data)
{
  return FALSE;
}



static gboolean
thunar_list_model_get_sort_column_id (GtkTreeSortable *sortable,
                                      gint            *sort_column_id,
                                      GtkSortType     *order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (store->sort_func == sort_by_mime_type)
    *sort_column_id = THUNAR_COLUMN_MIME_TYPE;
  else if (store->sort_func == sort_by_name)
    *sort_column_id = THUNAR_COLUMN_NAME;
  else if (store->sort_func == sort_by_file_name)
    *sort_column_id = THUNAR_COLUMN_FILE_NAME;
  else if (store->sort_func == sort_by_permissions)
    *sort_column_id = THUNAR_COLUMN_PERMISSIONS;
  else if (store->sort_func == sort_by_size)
    *sort_column_id = THUNAR_COLUMN_SIZE;
  else if (store->sort_func == sort_by_date_accessed)
    *sort_column_id = THUNAR_COLUMN_DATE_ACCESSED;
  else if (store->sort_func == sort_by_date_modified)
    *sort_column_id = THUNAR_COLUMN_DATE_MODIFIED;
  else if (store->sort_func == sort_by_type)
    *sort_column_id = THUNAR_COLUMN_TYPE;
  else if (store->sort_func == sort_by_owner)
    *sort_column_id = THUNAR_COLUMN_OWNER;
  else if (store->sort_func == sort_by_group)
    *sort_column_id = THUNAR_COLUMN_GROUP;
  else
    _thunar_assert_not_reached ();

  if (order != NULL)
    {
      if (store->sort_sign > 0)
        *order = GTK_SORT_ASCENDING;
      else
        *order = GTK_SORT_DESCENDING;
    }

  return TRUE;
}



static void
thunar_list_model_set_sort_column_id (GtkTreeSortable *sortable,
                                      gint             sort_column_id,
                                      GtkSortType      order)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (sortable);

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  switch (sort_column_id)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
      store->sort_func = sort_by_date_accessed;
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      store->sort_func = sort_by_date_modified;
      break;

    case THUNAR_COLUMN_GROUP:
      store->sort_func = sort_by_group;
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      store->sort_func = sort_by_mime_type;
      break;

    case THUNAR_COLUMN_NAME:
      store->sort_func = sort_by_name;
      break;

    case THUNAR_COLUMN_OWNER:
      store->sort_func = sort_by_owner;
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      store->sort_func = sort_by_permissions;
      break;

    case THUNAR_COLUMN_SIZE:
      store->sort_func = sort_by_size;
      break;

    case THUNAR_COLUMN_TYPE:
      store->sort_func = sort_by_type;
      break;

    case THUNAR_COLUMN_FILE_NAME:
      store->sort_func = sort_by_file_name;
      break;

    default:
      _thunar_assert_not_reached ();
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
                                         GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
}



static void
thunar_list_model_set_sort_func (GtkTreeSortable       *sortable,
                                 gint                   sort_column_id,
                                 GtkTreeIterCompareFunc func,
                                 gpointer               data,
                                 GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
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

  if (G_LIKELY (store->sort_folders_first))
    {
      isdir_a = thunar_file_is_directory (a);
      isdir_b = thunar_file_is_directory (b);

      if (isdir_a && !isdir_b)
        return -1;
      else if (!isdir_a && isdir_b)
        return 1;
    }

  return (*store->sort_func) (a, b, store->sort_case_sensitive) * store->sort_sign;
}



static gint
thunar_list_model_cmp_array (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  return thunar_list_model_cmp (THUNAR_LIST_MODEL (user_data),
                                ((SortTuple *) a)->row->data,
                                ((SortTuple *) b)->row->data);
}



static gint
thunar_list_model_cmp_list (gconstpointer a,
                            gconstpointer b,
                            gpointer      user_data)
{
  return -thunar_list_model_cmp (THUNAR_LIST_MODEL (user_data),
                                 THUNAR_FILE (a),
                                 THUNAR_FILE (b));
}



static gboolean
thunar_list_model_remove (ThunarListModel *store,
                          GtkTreeIter     *iter,
                          gboolean         silently)
{
  GtkTreePath *path;
  GSList      *next;
  GSList      *row;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, FALSE);

  /* determine the row and the path for the row */
  row = G_SLIST (iter->user_data);
  path = gtk_tree_path_new_from_indices (g_slist_position (store->rows, row), -1);

  /* remember the next row */
  next = row->next;

  /* delete the file associated with this row */
  g_object_unref (G_OBJECT (row->data));

  /* remove the link from the list */
  store->rows = g_slist_delete_link (store->rows, row);
  store->nrows -= 1;

  /* notify other parties */
  if (G_LIKELY (!silently))
    {
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
      g_object_notify (G_OBJECT (store), "num-files");
    }
  gtk_tree_path_free (path);

  if (next != NULL)
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, next);
      return TRUE;
    }
  else
    {
#ifndef NDEBUG
      iter->stamp = 0;
#endif
      return FALSE;
    }
}



static void
thunar_list_model_sort (ThunarListModel *store)
{
  GtkTreePath *path;
  SortTuple   *sort_array;
  GSList      *row;
  gint        *new_order;
  gint         n;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  if (G_UNLIKELY (store->nrows <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (store->nrows < 2000))
    sort_array = g_newa (SortTuple, store->nrows);
  else
    sort_array = g_new (SortTuple, store->nrows);

  /* generate the sort array of tuples */
  for (n = 0, row = store->rows; n < store->nrows; ++n, row = row->next)
    {
      sort_array[n].offset = n;
      sort_array[n].row = row;
    }

  /* sort the array using QuickSort */
  g_qsort_with_data (sort_array, store->nrows, sizeof (SortTuple), thunar_list_model_cmp_array, store);

  /* update our internals and generate the new order,
   * we reuse the memory for ths sort_array here, as
   * the sort_array items are larger (2x on ia32) than
   * the new_order items.
   */
  new_order = (gpointer) sort_array;

  /* need to grab the first row before grabbing the
   * remaining ones as sort_array[0].row will be
   * overwritten with the new_order[1] item on the
   * second run of the loop.
   */
  store->rows = sort_array[0].row;

  /* generate new_order and concat the rows */
  for (n = 0; n < store->nrows - 1; ++n)
    {
      new_order[n] = sort_array[n].offset;
      sort_array[n].row->next = sort_array[n + 1].row;
    }
  new_order[n] = sort_array[n].offset;
  sort_array[n].row->next = NULL;

  /* tell the view about the new item order */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
  gtk_tree_path_free (path);

  /* clean up if we used the heap */
  if (G_UNLIKELY (store->nrows >= 2000))
    g_free (sort_array);
}



static void
thunar_list_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarListModel   *store)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  GSList      *prev;
  GSList      *row;
  gint         n;

  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* check if we have a row for that file */
  for (n = 0, prev = NULL, row = store->rows; row != NULL; ++n, prev = row, row = row->next)
    if (G_UNLIKELY (row->data == file))
      {
        /* generate the iterator for this row */
        GTK_TREE_ITER_INIT (iter, store->stamp, row);

        /* notify the view that it has to redraw the file */
        path = gtk_tree_path_new_from_indices (n, -1);
        gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
        gtk_tree_path_free (path);

        /* check if the position of the row changed (because of its name may have changed) */
        if ((row->next != NULL && thunar_list_model_cmp (store, file, row->next->data) > 0)
            || (prev != NULL && thunar_list_model_cmp (store, file, prev->data) < 0))
          {
            /* re-sort the model with the new name for the file */
            thunar_list_model_sort (store);
          }

        return;
      }
}



static void
thunar_list_model_folder_destroy (ThunarFolder    *folder,
                                  ThunarListModel *store)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  thunar_list_model_set_folder (store, NULL);

  /* TODO: What to do when the folder is deleted? */
}



static void
thunar_list_model_folder_error (ThunarFolder    *folder,
                                const GError    *error,
                                ThunarListModel *store)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (error != NULL);

  /* forward the error signal */
  g_signal_emit (G_OBJECT (store), list_model_signals[ERROR], 0, error);

  /* reset the current folder */
  thunar_list_model_set_folder (store, NULL);
}



static void
thunar_list_model_files_added (ThunarFolder    *folder,
                               GList           *files,
                               ThunarListModel *store)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  ThunarFile  *file;
  GSList      *prev = NULL;
  GSList      *row;
  gint        *indices;
  gint         idx = 0;

  /* we use a simple trick here to avoid allocating
   * GtkTreePath's again and again, by simply accessing
   * the indices directly and only modifying the first
   * item in the integer array... looks a hack, eh?
   */
  path = gtk_tree_path_new_from_indices (0, -1);
  indices = gtk_tree_path_get_indices (path);
      
  /* process all added files */
  for (; files != NULL; files = files->next)
    {
      /* take a reference on that file */
      file = THUNAR_FILE (files->data);
      g_object_ref (G_OBJECT (file));

      /* check if the file should be hidden */
      if (!store->show_hidden && thunar_file_is_hidden (file))
        {
          store->hidden = g_slist_prepend (store->hidden, file);
        }
      else
        {
          /* allocate a new row */
          row = g_slist_alloc ();
          row->data = file;

          /* find the position to insert the file to */
          if (G_UNLIKELY (store->rows == NULL || thunar_list_model_cmp (store, file, store->rows->data) < 0))
            {
              prev        = NULL;
              idx       = 0;
              row->next   = store->rows;
              store->rows = row;
            }
          else
            {
              /* We use a simple optimization here to avoid going through the whole
               * list if the current file is to be inserted right before or after
               * the previous item (which is common).
               */
              if (G_UNLIKELY (prev == NULL || thunar_list_model_cmp (store, file, prev->data) < 0))
                {
                  prev = store->rows;
                  idx = 1;
                }

              for (; prev->next != NULL; ++idx, prev = prev->next)
                if (thunar_list_model_cmp (store, file, prev->next->data) < 0)
                  break;

              row->next  = prev->next;
              prev->next = row;
            }

          store->nrows += 1;

          /* generate an iterator for the new item */
          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          /* tell the view about the new item */
          indices[0] = idx;
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
        }
    }

  /* release the path */
  gtk_tree_path_free (path);

  /* number of visible files may have changed */
  g_object_notify (G_OBJECT (store), "num-files");
}



static void
thunar_list_model_files_removed (ThunarFolder    *folder,
                                 GList           *files,
                                 ThunarListModel *store)
{
  GtkTreeIter iter;
  ThunarFile *file;
  GSList     *row;
  GList      *lp;

  /* drop all the referenced files from the model */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* reference the file */
      file = THUNAR_FILE (lp->data);

      /* check if file is currently shown */
      for (row = store->rows; row != NULL; row = row->next)
        if (row->data == file)
          {
            GTK_TREE_ITER_INIT (iter, store->stamp, row);
            thunar_list_model_remove (store, &iter, FALSE);
            break;
          }

      /* check if the file was found */
      if (G_UNLIKELY (row == NULL))
        {
          /* file is hidden */
          _thunar_assert (g_slist_find (store->hidden, file) != NULL);
          store->hidden = g_slist_remove (store->hidden, file);
          g_object_unref (G_OBJECT (file));
        }
    }
}



static gint
sort_by_date_accessed (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  guint64 date_a;
  guint64 date_b;

  date_a = thunar_file_get_date (a, THUNAR_FILE_DATE_ACCESSED);
  date_b = thunar_file_get_date (b, THUNAR_FILE_DATE_ACCESSED);

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;

  return sort_by_name (a, b, case_sensitive);
}



static gint
sort_by_date_modified (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  guint64 date_a;
  guint64 date_b;

  date_a = thunar_file_get_date (a, THUNAR_FILE_DATE_MODIFIED);
  date_b = thunar_file_get_date (b, THUNAR_FILE_DATE_MODIFIED);

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;

  return sort_by_name (a, b, case_sensitive);
}



static gint
sort_by_file_name (const ThunarFile *a,
                   const ThunarFile *b,
                   gboolean          case_sensitive)
{
  const gchar *a_name = thunar_file_get_display_name (a);
  const gchar *b_name = thunar_file_get_display_name (b);

  if (!case_sensitive)
    return strcasecmp (a_name, b_name);
  else
    return strcmp (a_name, b_name);
}



static gint
sort_by_group (const ThunarFile *a,
               const ThunarFile *b,
               gboolean          case_sensitive)
{
  ThunarGroup *group_a;
  ThunarGroup *group_b;
  guint32      gid_a;
  guint32      gid_b;
  gint         result;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return sort_by_name (a, b, case_sensitive);

  group_a = thunar_file_get_group (a);
  group_b = thunar_file_get_group (b);

  if (group_a != NULL && group_b != NULL)
    {
      if (!case_sensitive)
        {
          result = strcasecmp (thunar_group_get_name (group_a),
                               thunar_group_get_name (group_b));
        }
      else
        {
          result = strcmp (thunar_group_get_name (group_a),
                           thunar_group_get_name (group_b));
        }
    }
  else
    {
      gid_a = g_file_info_get_attribute_uint32 (thunar_file_get_info (a),
                                                G_FILE_ATTRIBUTE_UNIX_GID);
      gid_b = g_file_info_get_attribute_uint32 (thunar_file_get_info (b),
                                                G_FILE_ATTRIBUTE_UNIX_GID);
      
      result = CLAMP ((gint) gid_a - (gint) gid_b, -1, 1);
    }

  if (group_a != NULL)
    g_object_unref (group_a);

  if (group_b != NULL)
    g_object_unref (group_b);
  
  if (result == 0)
    return sort_by_name (a, b, case_sensitive);
  else
    return result;
}



static gint
sort_by_mime_type (const ThunarFile *a,
                   const ThunarFile *b,
                   gboolean          case_sensitive)
{
  const gchar *content_type_a;
  const gchar *content_type_b;
  gint         result;

  content_type_a = thunar_file_get_content_type (a);
  content_type_b = thunar_file_get_content_type (b);

  result = strcasecmp (content_type_a, content_type_b);

  if (result == 0)
    result = sort_by_name (a, b, case_sensitive);

  return result;
}



static gint
sort_by_name (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  return thunar_file_compare_by_name (a, b, case_sensitive);
}



static gint
sort_by_owner (const ThunarFile *a,
               const ThunarFile *b,
               gboolean          case_sensitive)
{
  const gchar *name_a;
  const gchar *name_b;
  const gchar *real_name_a;
  const gchar *real_name_b;
  ThunarUser  *user_a;
  ThunarUser  *user_b;
  guint32      uid_a;
  guint32      uid_b;
  gchar       *str_a;
  gchar       *str_b;
  gint         result;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return sort_by_name (a, b, case_sensitive);

  user_a = thunar_file_get_user (a);
  user_b = thunar_file_get_user (b);

  if (user_a != NULL && user_b != NULL)
    {
      name_a = thunar_user_get_name (user_a);
      real_name_a = thunar_user_get_real_name (user_a);
      str_a = G_LIKELY (real_name_a != NULL) 
        ? g_strdup_printf ("%s (%s)", real_name_a, name_a) : g_strdup (name_a);

      name_b = thunar_user_get_name (user_b);
      real_name_b = thunar_user_get_real_name (user_b);
      str_b = G_LIKELY (real_name_b != NULL) 
        ? g_strdup_printf ("%s (%s)", real_name_b, name_b) : g_strdup (name_b);

      if (!case_sensitive)
        result = strcasecmp (str_a, str_b);
      else
        result = strcmp (str_a, str_b);

      g_free (str_a);
      g_free (str_b);
    }
  else
    {
      uid_a = g_file_info_get_attribute_uint32 (thunar_file_get_info (a),
                                                G_FILE_ATTRIBUTE_UNIX_UID);
      uid_b = g_file_info_get_attribute_uint32 (thunar_file_get_info (b),
                                                G_FILE_ATTRIBUTE_UNIX_UID);

      result = CLAMP ((gint) uid_a - (gint) uid_b, -1, 1);
    }

  if (result == 0)
    return sort_by_name (a, b, case_sensitive);
  else
    return result;
}



static gint
sort_by_permissions (const ThunarFile *a,
                     const ThunarFile *b,
                     gboolean          case_sensitive)
{
  ThunarFileMode mode_a;
  ThunarFileMode mode_b;

  mode_a = thunar_file_get_mode (a);
  mode_b = thunar_file_get_mode (b);

  if (mode_a < mode_b)
    return -1;
  else if (mode_a > mode_b)
    return 1;

  return sort_by_name (a, b, case_sensitive);
}



static gint
sort_by_size (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  guint64 size_a;
  guint64 size_b;

  size_a = thunar_file_get_size (a);
  size_b = thunar_file_get_size (b);

  if (size_a < size_b)
    return -1;
  else if (size_a > size_b)
    return 1;

  return sort_by_name (a, b, case_sensitive);
}



static gint
sort_by_type (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  const gchar *content_type_a;
  const gchar *content_type_b;
  gchar       *description_a = NULL;
  gchar       *description_b = NULL;
  gint         result;

  /* we alter the description of symlinks here because they are 
   * displayed as "link to ..." in the detailed list view as well */

  if (thunar_file_is_symlink (a))
    {
      description_a = g_strdup_printf (_("link to %s"), 
                                       thunar_file_get_symlink_target (a));
    }
  else
    {
      content_type_a = thunar_file_get_content_type (a);
      description_a = g_content_type_get_description (content_type_a);
    }
  
  if (thunar_file_is_symlink (b))
    {
      description_b = g_strdup_printf (_("link to %s"),
                                       thunar_file_get_symlink_target (b));
    }
  else
    {
      content_type_b = thunar_file_get_content_type (b);
      description_b = g_content_type_get_description (content_type_b);
    }

  /* avoid calling strcasecmp with NULL parameters */
  if (description_a == NULL || description_b == NULL)
    {
      g_free (description_a);
      g_free (description_b);

      return 0;
    }

  if (!case_sensitive)
    result = strcasecmp (description_a, description_b);
  else
    result = strcmp (description_a, description_b);

  g_free (description_a);
  g_free (description_b);

  if (result == 0)
    return sort_by_name (a, b, case_sensitive);
  else
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
  return g_object_new (THUNAR_TYPE_LIST_MODEL, NULL);
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
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return g_object_new (THUNAR_TYPE_LIST_MODEL, "folder", folder, NULL);
}



/**
 * thunar_list_model_get_case_sensitive:
 * @store : a valid #ThunarListModel object.
 *
 * Returns %TRUE if the sorting is done in a case-sensitive
 * manner for @store.
 *
 * Return value: %TRUE if sorting is case-sensitive.
 **/
gboolean
thunar_list_model_get_case_sensitive (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->sort_case_sensitive;
}



/**
 * thunar_list_model_set_case_sensitive:
 * @store          : a valid #ThunarListModel object.
 * @case_sensitive : %TRUE to use case-sensitive sort for @store.
 *
 * If @case_sensitive is %TRUE the sorting in @store will be done
 * in a case-sensitive manner.
 **/
void
thunar_list_model_set_case_sensitive (ThunarListModel *store,
                                      gboolean         case_sensitive)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* normalize the setting */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (store->sort_case_sensitive != case_sensitive)
    {
      /* apply the new setting */
      store->sort_case_sensitive = case_sensitive;

      /* resort the model with the new setting */
      thunar_list_model_sort (store);
      
      /* notify listeners */
      g_object_notify (G_OBJECT (store), "case-sensitive");
    }
}



/**
 * thunar_list_model_get_date_style:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the #ThunarDateStyle used to format dates in
 *               the given @store.
 **/
ThunarDateStyle
thunar_list_model_get_date_style (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), THUNAR_DATE_STYLE_SIMPLE);
  return store->date_style;
}



/**
 * thunar_list_model_set_date_style:
 * @store      : a valid #ThunarListModel object.
 * @date_style : the #ThunarDateStyle that should be used to format
 *               dates in the @store.
 *
 * Chances the style used to format dates in @store to the specified
 * @date_style.
 **/
void
thunar_list_model_set_date_style (ThunarListModel *store,
                                  ThunarDateStyle  date_style)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if we have a new setting */
  if (store->date_style != date_style)
    {
      /* apply the new setting */
      store->date_style = date_style;

      /* notify listeners */
      g_object_notify (G_OBJECT (store), "date-style");

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store), (GtkTreeModelForeachFunc) gtk_tree_model_row_changed, NULL);
    }
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
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
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
  GtkTreePath *path;
  GtkTreeIter  iter;
  ThunarFile  *file;
  gboolean     has_handler;
  GSList      *rows;
  GSList      *row;
  GList       *files;
  GList       *lp;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (folder == NULL || THUNAR_IS_FOLDER (folder));

  /* check if we're not already using that folder */
  if (G_UNLIKELY (store->folder == folder))
    return;

  /* unlink from the previously active folder (if any) */
  if (G_LIKELY (store->folder != NULL))
    {
      /* check if we have any handlers connected for "row-deleted" */
      has_handler = g_signal_has_handler_pending (G_OBJECT (store), store->row_deleted_id, 0, FALSE);

      /* remember the rows */
      rows = store->rows;

      /* remove existing entries */
      path = gtk_tree_path_new_from_indices (0, -1);
      while (store->nrows > 0)
        {
          /* grab the next row */
          row = store->rows;

          /* delete the file associated with this row */
          g_object_unref (G_OBJECT (row->data));

          /* remove the row from the list */
          store->rows = row->next;
          store->nrows--;

          /* notify the view(s) if they're actually
           * interested in the "row-deleted" signal.
           */
          if (G_UNLIKELY (has_handler))
            gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
        }
      gtk_tree_path_free (path);

      /* drop the remembered rows memory */
      g_slist_free (rows);

      /* remove hidden entries */
      g_slist_foreach (store->hidden, (GFunc) g_object_unref, NULL);
      g_slist_free (store->hidden);
      store->hidden = NULL;

      /* unregister signals and drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (store->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (G_OBJECT (store->folder));
    }

  /* ... just to be sure! */
  _thunar_assert (store->rows == NULL);
  _thunar_assert (store->nrows == 0);

  /* activate the new folder */
  store->folder = folder;

  /* connect to the new folder (if any) */
  if (folder != NULL)
    {
      g_object_ref (G_OBJECT (folder));

      /* sort the files _before_ adding them to the store (reverse order -> prepend below) */
      files = g_list_copy (thunar_folder_get_files (folder));
      if (G_LIKELY (files != NULL))
        {
          /* check if we have any handlers connected for "row-inserted" */
          has_handler = g_signal_has_handler_pending (G_OBJECT (store), store->row_inserted_id, 0, FALSE);

          /* sort the files before inserting them into the model */
          files = g_list_sort_with_data (files, thunar_list_model_cmp_list, store);

          /* setup path for "row-inserted" */
          path = gtk_tree_path_new_from_indices (0, -1);

          /* insert the files */
          for (lp = files; lp != NULL; lp = lp->next)
            {
              /* take a reference on the file */
              file = THUNAR_FILE (lp->data);
              g_object_ref (G_OBJECT (file));

              /* check if this file should be shown/hidden */
              if (!store->show_hidden && thunar_file_is_hidden (file))
                {
                  store->hidden = g_slist_prepend (store->hidden, file);
                }
              else
                {
                  row = g_slist_alloc ();
                  row->data = file;
                  row->next = store->rows;

                  store->rows = row;
                  store->nrows += 1;

                  /* check if we need to notify a view */
                  if (G_UNLIKELY (has_handler))
                    {
                      /* update the iter for the new row */
                      GTK_TREE_ITER_INIT (iter, store->stamp, row);

                      /* tell the view about the new row */
                      gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
                    }
                }
            }

          /* cleanup */
          gtk_tree_path_free (path);
          g_list_free (files);
        }

      /* connect signals to the new folder */
      g_signal_connect (G_OBJECT (store->folder), "destroy", G_CALLBACK (thunar_list_model_folder_destroy), store);
      g_signal_connect (G_OBJECT (store->folder), "error", G_CALLBACK (thunar_list_model_folder_error), store);
      g_signal_connect (G_OBJECT (store->folder), "files-added", G_CALLBACK (thunar_list_model_files_added), store);
      g_signal_connect (G_OBJECT (store->folder), "files-removed", G_CALLBACK (thunar_list_model_files_removed), store);
    }

  /* notify listeners that we have a new folder */
  g_object_freeze_notify (G_OBJECT (store));
  g_object_notify (G_OBJECT (store), "folder");
  g_object_notify (G_OBJECT (store), "num-files");
  g_object_thaw_notify (G_OBJECT (store));
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
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
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
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

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
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
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
  GSList       *prev;
  GSList       *row;
  GSList       *hidden_rows;
  GSList       *files;
  GSList       *lp;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

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

          row = g_slist_alloc ();
          row->data = file;

          if (G_UNLIKELY (store->rows == NULL || thunar_list_model_cmp (store, file, store->rows->data) < 0))
            {
              row->next = store->rows;
              store->rows = row;
            }
          else
            {
              for (prev = store->rows; prev->next != NULL; prev = prev->next)
                if (thunar_list_model_cmp (store, file, prev->next->data) < 0)
                  break;

              row->next = prev->next;
              prev->next = row;
            }

          store->nrows += 1;

          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          path = thunar_list_model_get_path (GTK_TREE_MODEL (store), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);
        }
      g_slist_free (store->hidden);
      store->hidden = NULL;
    }
  else
    {
      _thunar_assert (store->hidden == NULL);

      /* unmerge all hidden elements */
      for (hidden_rows = files = NULL, row = store->rows; row != NULL; row = row->next)
        if (thunar_file_is_hidden (row->data))
          {
            hidden_rows = g_slist_prepend (hidden_rows, row);
            files = g_slist_prepend (files, g_object_ref (row->data));
          }

      if (files != NULL)
        {
          for (lp = hidden_rows; lp != NULL; lp = lp->next)
            {
              GTK_TREE_ITER_INIT (iter, store->stamp, lp->data);
              thunar_list_model_remove (store, &iter, FALSE);
            }
          g_slist_free (hidden_rows);

          store->hidden = files;
        }
    }

  /* notify listeners about the new setting */
  g_object_freeze_notify (G_OBJECT (store));
  g_object_notify (G_OBJECT (store), "num-files");
  g_object_notify (G_OBJECT (store), "show-hidden");
  g_object_thaw_notify (G_OBJECT (store));
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
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, NULL);

  return g_object_ref (G_SLIST (iter->user_data)->data);
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
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);
  return store->nrows;
}



/**
 * thunar_list_model_get_paths_for_files:
 * @store : a #ThunarListModel instance.
 * @files : a list of #ThunarFile<!---->s.
 *
 * Determines the list of #GtkTreePath<!---->s for the #ThunarFile<!---->s
 * found in the @files list. If a #ThunarFile from the @files list is not
 * available in @store, no #GtkTreePath will be returned for it. So, in effect,
 * only #GtkTreePath<!---->s for the subset of @files available in @store will
 * be returned.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s for @files.
 **/
GList*
thunar_list_model_get_paths_for_files (ThunarListModel *store,
                                       GList           *files)
{
  GSList *row;
  GList  *paths = NULL;
  gint    idx = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  /* find the rows for the given files */
  for (row = store->rows; row != NULL; ++idx, row = row->next)
    if (g_list_find (files, row->data) != NULL)
      paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (idx, -1));

  return paths;
}



/**
 * thunar_list_model_get_paths_for_pattern:
 * @store   : a #ThunarListModel instance.
 * @pattern : the pattern to match.
 *
 * Looks up all rows in the @store that match @pattern and returns
 * a list of #GtkTreePath<!---->s corresponding to the rows.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s that match @pattern.
 **/
GList*
thunar_list_model_get_paths_for_pattern (ThunarListModel *store,
                                         const gchar     *pattern)
{
  GPatternSpec *pspec;
  GSList       *row;
  GList        *paths = NULL;
  gint          idx = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (pattern, -1, NULL), NULL);

  /* compile the pattern */
  pspec = g_pattern_spec_new (pattern);

  /* find all rows that match the given pattern */
  for (row = store->rows; row != NULL; ++idx, row = row->next)
    if (g_pattern_match_string (pspec, thunar_file_get_display_name (row->data)))
      paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (idx, -1));

  /* release the pattern */
  g_pattern_spec_free (pspec);

  return paths;
}



/**
 * thunar_list_model_get_statusbar_text:
 * @store          : a #ThunarListModel instance.
 * @selected_items : the list of selected items (as GtkTreePath's).
 *
 * Generates the statusbar text for @store with the given
 * @selected_items.
 *
 * This function is used by the #ThunarStandardView (and thereby
 * implicitly by #ThunarIconView and #ThunarDetailsView) to
 * calculate the text to display in the statusbar for a given
 * file selection.
 *
 * The caller is reponsible to free the returned text using
 * g_free() when it's no longer needed.
 *
 * Return value: the statusbar text for @store with the given
 *               @selected_items.
 **/
gchar*
thunar_list_model_get_statusbar_text (ThunarListModel *store,
                                      GList           *selected_items)
{
  const gchar *content_type;
  const gchar *original_path;
  GtkTreeIter  iter;
  ThunarFile  *file;
  guint64      size;
  guint64      size_summary;
  GSList      *row;
  GList       *lp;
  gchar       *absolute_path;
  gchar       *fspace_string;
  gchar       *display_name;
  gchar       *size_string;
  gchar       *text;
  gchar       *s;
  gint         height;
  gint         width;
  gint         n;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  if (selected_items == NULL)
    {
      /* try to determine a file for the current folder */
      file = (store->folder != NULL) ? thunar_folder_get_corresponding_file (store->folder) : NULL;

      /* check if we can determine the amount of free space for the volume */
      if (G_LIKELY (file != NULL && thunar_file_get_free_space (file, &size)))
        {
          /* humanize the free space */
          fspace_string = g_format_size_for_display (size);

          /* check if we have atleast one file in this folder */
          if (G_LIKELY (store->nrows > 0))
            {
              /* calculate the size of all items */
              for (row = store->rows, size_summary = 0; row != NULL; row = row->next)
                size_summary += thunar_file_get_size (row->data);

              /* humanize the size summary */
              size_string = g_format_size_for_display (size_summary);

              /* generate a text which includes the size of all items in the folder */
              text = g_strdup_printf (ngettext ("%d item (%s), Free space: %s", "%d items (%s), Free space: %s", store->nrows),
                                      store->nrows, size_string, fspace_string);

              /* cleanup */
              g_free (size_string);
            }
          else
            {
              /* just the standard text */
              text = g_strdup_printf (ngettext ("%d item, Free space: %s", "%d items, Free space: %s", store->nrows), store->nrows, fspace_string);
            }

          /* cleanup */
          g_free (fspace_string);
        }
      else
        {
          text = g_strdup_printf (ngettext ("%d item", "%d items", store->nrows), store->nrows);
        }
    }
  else if (selected_items->next == NULL)
    {
      /* resolve the iter for the single path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, selected_items->data);

      /* get the file for the given iter */
      file = THUNAR_FILE (G_SLIST (iter.user_data)->data);
      
      /* determine the content type of the file */
      content_type = thunar_file_get_content_type (file);

      /* calculate the text to be displayed */
      size_string = thunar_file_get_size_string (file);

      if (G_UNLIKELY (content_type != NULL && g_str_equal (content_type, "inode/symlink")))
        {
          text = g_strdup_printf (_("\"%s\" broken link"), thunar_file_get_display_name (file));
        }
      else if (G_UNLIKELY (thunar_file_is_symlink (file)))
        {
          text = g_strdup_printf (_("\"%s\" (%s) link to %s"), thunar_file_get_display_name (file),
                                  size_string, thunar_file_get_symlink_target (file));
        }
      else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT))
        {
          text = g_strdup_printf (_("\"%s\" shortcut"), thunar_file_get_display_name (file));
        }
      else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE))
        {
          text = g_strdup_printf (_("\"%s\" mountable"), thunar_file_get_display_name (file));
        }
      else
        {
          gchar *description;
          
          description = g_content_type_get_description (thunar_file_get_content_type (file));
          text = g_strdup_printf (_("\"%s\" (%s) %s"), thunar_file_get_display_name (file),
                                  size_string, description);
          g_free (description);
        }
      g_free (size_string);

      /* append the original path (if any) */
      original_path = thunar_file_get_original_path (file);
      if (G_UNLIKELY (original_path != NULL))
        {
          /* append the original path to the statusbar text */
          display_name = g_filename_display_name (original_path);
          s = g_strdup_printf ("%s, %s %s", text, _("Original Path:"), display_name);
          g_free (display_name);
          g_free (text);
          text = s;
        }
      else if (thunar_file_is_local (file) && thunar_file_is_regular (file))
        {
          /* check if we can determine the dimension of this file (only for image files) */
          absolute_path = g_file_get_path (thunar_file_get_file (file));
          if (absolute_path != NULL
              && gdk_pixbuf_get_file_info (absolute_path, &width, &height) != NULL)
            {
              /* append the image dimensions to the statusbar text */
              s = g_strdup_printf ("%s, %s %dx%d", text, _("Image Size:"), width, height);
              g_free (text);
              text = s;
            }
          g_free (absolute_path);
        }
    }
  else
    {
      /* sum up all sizes */
      for (lp = selected_items, n = 0, size_summary = 0; lp != NULL; lp = lp->next, ++n)
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, lp->data);
          size_summary += thunar_file_get_size (THUNAR_FILE (G_SLIST (iter.user_data)->data));
        }

      if (size_summary > 0)
        {
          size_string = g_format_size_for_display (size_summary);
          text = g_strdup_printf (ngettext ("%d item selected (%s)", "%d items selected (%s)", n), n, size_string);
          g_free (size_string);
        }
      else
        {
          text = g_strdup_printf (ngettext ("%d item selected", "%d items selected", n), n);
        }
    }

  return text;
}




