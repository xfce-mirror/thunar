/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>



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
  PROP_FILE_SIZE_BINARY,
  N_PROPERTIES
};

/* Signal identifiers */
enum
{
  ERROR,
  LAST_SIGNAL,
};



typedef gint (*ThunarSortFunc) (const ThunarFile *a,
                                const ThunarFile *b,
                                gboolean          case_sensitive);



static void               thunar_list_model_tree_model_init       (GtkTreeModelIface      *iface);
static void               thunar_list_model_drag_dest_init        (GtkTreeDragDestIface   *iface);
static void               thunar_list_model_sortable_init         (GtkTreeSortableIface   *iface);
static void               thunar_list_model_dispose               (GObject                *object);
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
static gint               thunar_list_model_cmp_func              (gconstpointer           a,
                                                                   gconstpointer           b,
                                                                   gpointer                user_data);
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
static gint               sort_by_group                           (const ThunarFile       *a,
                                                                   const ThunarFile       *b,
                                                                   gboolean                case_sensitive);
static gint               sort_by_mime_type                       (const ThunarFile       *a,
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

static gboolean           thunar_list_model_get_case_sensitive    (ThunarListModel        *store);
static void               thunar_list_model_set_case_sensitive    (ThunarListModel        *store,
                                                                   gboolean                case_sensitive);
static ThunarDateStyle    thunar_list_model_get_date_style        (ThunarListModel        *store);
static void               thunar_list_model_set_date_style        (ThunarListModel        *store,
                                                                   ThunarDateStyle         date_style);
static gint               thunar_list_model_get_num_files         (ThunarListModel        *store);
static gboolean           thunar_list_model_get_folders_first     (ThunarListModel        *store);



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

  GSequence      *rows;
  GSList         *hidden;
  ThunarFolder   *folder;
  gboolean        show_hidden : 1;
  gboolean        file_size_binary : 1;
  ThunarDateStyle date_style;

  /* Use the shared ThunarFileMonitor instance, so we
   * do not need to connect "changed" handler to every
   * file in the model.
   */
  ThunarFileMonitor *file_monitor;

  /* ids for the "row-inserted" and "row-deleted" signals
   * of GtkTreeModel to speed up folder changing.
   */
  guint          row_inserted_id;
  guint          row_deleted_id;

  gboolean       sort_case_sensitive : 1;
  gboolean       sort_folders_first : 1;
  gint           sort_sign;   /* 1 = ascending, -1 descending */
  ThunarSortFunc sort_func;
};



static guint       list_model_signals[LAST_SIGNAL];
static GParamSpec *list_model_props[N_PROPERTIES] = { NULL, };



G_DEFINE_TYPE_WITH_CODE (ThunarListModel, thunar_list_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_list_model_tree_model_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_list_model_drag_dest_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE, thunar_list_model_sortable_init))



static void
thunar_list_model_class_init (ThunarListModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->dispose      = thunar_list_model_dispose;
  gobject_class->finalize     = thunar_list_model_finalize;
  gobject_class->get_property = thunar_list_model_get_property;
  gobject_class->set_property = thunar_list_model_set_property;

  /**
   * ThunarListModel:case-sensitive:
   *
   * Tells whether the sorting should be case sensitive.
   **/
  list_model_props[PROP_CASE_SENSITIVE] =
      g_param_spec_boolean ("case-sensitive",
                            "case-sensitive",
                            "case-sensitive",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:date-style:
   *
   * The style used to format dates.
   **/
  list_model_props[PROP_DATE_STYLE] =
      g_param_spec_enum ("date-style",
                         "date-style",
                         "date-style",
                         THUNAR_TYPE_DATE_STYLE,
                         THUNAR_DATE_STYLE_SIMPLE,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:folder:
   *
   * The folder presented by this #ThunarListModel.
   **/
  list_model_props[PROP_FOLDER] =
      g_param_spec_object ("folder",
                           "folder",
                           "folder",
                           THUNAR_TYPE_FOLDER,
                           EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::folders-first:
   *
   * Tells whether to always sort folders before other files.
   **/
  list_model_props[PROP_FOLDERS_FIRST] =
      g_param_spec_boolean ("folders-first",
                            "folders-first",
                            "folders-first",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::num-files:
   *
   * The number of files in the folder presented by this #ThunarListModel.
   **/
  list_model_props[PROP_NUM_FILES] =
      g_param_spec_uint ("num-files",
                         "num-files",
                         "num-files",
                         0, G_MAXUINT, 0,
                         EXO_PARAM_READABLE);

  /**
   * ThunarListModel::show-hidden:
   *
   * Tells whether to include hidden (and backup) files.
   **/
  list_model_props[PROP_SHOW_HIDDEN] =
      g_param_spec_boolean ("show-hidden",
                            "show-hidden",
                            "show-hidden",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::misc-file-size-binary:
   *
   * Tells whether to format file size in binary.
   **/
  list_model_props[PROP_FILE_SIZE_BINARY] =
      g_param_spec_boolean ("file-size-binary",
                            "file-size-binary",
                            "file-size-binary",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, list_model_props);

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
  store->stamp = g_random_int ();
#endif

  store->row_inserted_id = g_signal_lookup ("row-inserted", GTK_TYPE_TREE_MODEL);
  store->row_deleted_id = g_signal_lookup ("row-deleted", GTK_TYPE_TREE_MODEL);

  store->sort_case_sensitive = TRUE;
  store->sort_folders_first = TRUE;
  store->sort_sign = 1;
  store->sort_func = thunar_file_compare_by_name;
  store->rows = g_sequence_new (g_object_unref);

  /* connect to the shared ThunarFileMonitor, so we don't need to
   * connect "changed" to every single ThunarFile we own.
   */
  store->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (store->file_monitor), "file-changed",
                    G_CALLBACK (thunar_list_model_file_changed), store);
}



static void
thunar_list_model_dispose (GObject *object)
{
  /* unlink from the folder (if any) */
  thunar_list_model_set_folder (THUNAR_LIST_MODEL (object), NULL);

  (*G_OBJECT_CLASS (thunar_list_model_parent_class)->dispose) (object);
}



static void
thunar_list_model_finalize (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  g_sequence_free (store->rows);

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

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, thunar_list_model_get_file_size_binary (store));
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

    case PROP_FILE_SIZE_BINARY:
      thunar_list_model_set_file_size_binary (store, g_value_get_boolean (value));
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
  GSequenceIter   *row;
  gint             offset;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the row for the path */
  offset = gtk_tree_path_get_indices (path)[0];
  row = g_sequence_get_iter_at_pos (store->rows, offset);

  if (!g_sequence_iter_is_end (row))
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
  gint             idx;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, NULL);

  idx = g_sequence_iter_get_position (iter->user_data);
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

  file = g_sequence_get (iter->user_data);
  _thunar_assert (THUNAR_IS_FILE (file));

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
      g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_LIST_MODEL (model)->file_size_binary));
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

  iter->user_data = g_sequence_iter_next (iter->user_data);
  return !g_sequence_iter_is_end (iter->user_data);
}



static gboolean
thunar_list_model_iter_children (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL
      && g_sequence_get_length (store->rows) > 0))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, g_sequence_get_begin_iter (store->rows));
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

  return (iter == NULL) ? g_sequence_get_length (store->rows) : 0;
}



static gboolean
thunar_list_model_iter_nth_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GSequenceIter   *row;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      row = g_sequence_get_iter_at_pos (store->rows, n);
      if (g_sequence_iter_is_end (row))
        return FALSE;

      GTK_TREE_ITER_INIT (*iter, store->stamp, row);
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
  else if (store->sort_func == thunar_file_compare_by_name)
    *sort_column_id = THUNAR_COLUMN_NAME;
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

    case THUNAR_COLUMN_FILE_NAME:
    case THUNAR_COLUMN_NAME:
      store->sort_func = thunar_file_compare_by_name;
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

    default:
      _thunar_assert_not_reached ();
    }

  /* new sort sign */
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
thunar_list_model_cmp_func (gconstpointer a,
                            gconstpointer b,
                            gpointer      user_data)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (user_data);
  gboolean         isdir_a;
  gboolean         isdir_b;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (a), 0);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (b), 0);

  if (G_LIKELY (store->sort_folders_first))
    {
      isdir_a = thunar_file_is_directory (a);
      isdir_b = thunar_file_is_directory (b);
      if (isdir_a != isdir_b)
        return isdir_a ? -1 : 1;
    }

  return (*store->sort_func) (a, b, store->sort_case_sensitive) * store->sort_sign;
}



static void
thunar_list_model_sort (ThunarListModel *store)
{
  GtkTreePath    *path;
  GSequenceIter **old_order;
  gint           *new_order;
  gint            n;
  gint            length;
  GSequenceIter  *row;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  length = g_sequence_get_length (store->rows);
  if (G_UNLIKELY (length <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (length < 2000))
    {
      old_order = g_newa (GSequenceIter *, length);
      new_order = g_newa (gint, length);
    }
  else
    {
      old_order = g_new (GSequenceIter *, length);
      new_order = g_new (gint, length);
    }

  /* store old order */
  row = g_sequence_get_begin_iter (store->rows);
  for (n = 0; n < length; ++n)
    {
      old_order[n] = row;
      row = g_sequence_iter_next (row);
    }

  /* sort */
  g_sequence_sort (store->rows, thunar_list_model_cmp_func, store);

  /* new_order[newpos] = oldpos */
  for (n = 0; n < length; ++n)
    new_order[g_sequence_iter_get_position (old_order[n])] = n;

  /* tell the view about the new item order */
  path = gtk_tree_path_new_root ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
  gtk_tree_path_free (path);

  /* clean up if we used the heap */
  if (G_UNLIKELY (length >= 2000))
    {
      g_free (old_order);
      g_free (new_order);
    }
}



static void
thunar_list_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarListModel   *store)
{
  GSequenceIter *row;
  GSequenceIter *end;
  gint           pos_after;
  gint           pos_before = 0;
  gint          *new_order;
  gint           length;
  gint           i, j;
  GtkTreePath   *path;
  GtkTreeIter    iter;

  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  while (row != end)
    {
      if (G_UNLIKELY (g_sequence_get (row) == file))
        {
          /* generate the iterator for this row */
          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          /* notify the view that it has to redraw the file */
          _thunar_assert (pos_before == g_sequence_iter_get_position (row));
          path = gtk_tree_path_new_from_indices (pos_before, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);

          /* check if the sorting changed */
          g_sequence_sort_changed (row, thunar_list_model_cmp_func, store);
          pos_after = g_sequence_iter_get_position (row);
          if (pos_after != pos_before)
            {
              /* do swap sorting here since its much faster than a complete sort */
              length = g_sequence_get_length (store->rows);
              if (G_LIKELY (length < 2000))
                new_order = g_newa (gint, length);
              else
                new_order = g_new (gint, length);

              /* new_order[newpos] = oldpos */
              for (i = 0, j = 0; i < length; ++i)
                {
                  if (G_UNLIKELY (i == pos_after))
                    {
                      new_order[i] = pos_before;
                    }
                  else
                    {
                      if (G_UNLIKELY (j == pos_before))
                        j++;
                      new_order[i] = j++;
                    }
                }

              /* tell the view about the new item order */
              path = gtk_tree_path_new_root ();
              gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, NULL, new_order);
              gtk_tree_path_free (path);

              /* clean up if we used the heap */
              if (G_UNLIKELY (length >= 2000))
                g_free (new_order);
            }

          break;
        }

      row = g_sequence_iter_next (row);
      pos_before++;
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
  GtkTreePath   *path;
  GtkTreeIter    iter;
  ThunarFile    *file;
  gint          *indices;
  GSequenceIter *row;
  GList         *lp;
  gboolean       has_handler;

  /* we use a simple trick here to avoid allocating
   * GtkTreePath's again and again, by simply accessing
   * the indices directly and only modifying the first
   * item in the integer array... looks a hack, eh?
   */
  path = gtk_tree_path_new_first ();
  indices = gtk_tree_path_get_indices (path);

  /* check if we have any handlers connected for "row-inserted" */
  has_handler = g_signal_has_handler_pending (G_OBJECT (store), store->row_inserted_id, 0, FALSE);

  /* process all added files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* take a reference on that file */
      file = g_object_ref (G_OBJECT (lp->data));
      _thunar_return_if_fail (THUNAR_IS_FILE (file));

      /* check if the file should be hidden */
      if (!store->show_hidden && thunar_file_is_hidden (file))
        {
          store->hidden = g_slist_prepend (store->hidden, file);
        }
      else
        {
          /* insert the file */
          row = g_sequence_insert_sorted (store->rows, file,
                                          thunar_list_model_cmp_func, store);

          if (has_handler)
            {
              /* generate an iterator for the new item */
              GTK_TREE_ITER_INIT (iter, store->stamp, row);

              indices[0] = g_sequence_iter_get_position (row);
              gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
            }
        }
    }

  /* release the path */
  gtk_tree_path_free (path);

  /* number of visible files may have changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
}



static void
thunar_list_model_files_removed (ThunarFolder    *folder,
                                 GList           *files,
                                 ThunarListModel *store)
{
  GList         *lp;
  GSequenceIter *row;
  GSequenceIter *end;
  GSequenceIter *next;
  GtkTreePath   *path;
  gboolean       found;

  /* drop all the referenced files from the model */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      found = FALSE;

      while (row != end)
        {
          next = g_sequence_iter_next (row);

          if (g_sequence_get (row) == lp->data)
            {
              /* setup path for "row-deleted" */
              path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);

              /* remove file from the model */
              g_sequence_remove (row);

              /* notify the view(s) */
              gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
              gtk_tree_path_free (path);

              /* no need to look in the hidden files */
              found = TRUE;

              break;
            }

          row = next;
        }

      /* check if the file was found */
      if (!found)
        {
          /* file is hidden */
          _thunar_assert (g_slist_find (store->hidden, lp->data) != NULL);
          store->hidden = g_slist_remove (store->hidden, lp->data);
          g_object_unref (G_OBJECT (lp->data));
        }
    }

  /* this probably changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
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

  return thunar_file_compare_by_name (a, b, case_sensitive);
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

  return thunar_file_compare_by_name (a, b, case_sensitive);
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
  const gchar *name_a;
  const gchar *name_b;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return thunar_file_compare_by_name (a, b, case_sensitive);

  group_a = thunar_file_get_group (a);
  group_b = thunar_file_get_group (b);

  if (group_a != NULL && group_b != NULL)
    {
      name_a = thunar_group_get_name (group_a);
      name_b = thunar_group_get_name (group_b);

      if (!case_sensitive)
        result = strcasecmp (name_a, name_b);
      else
        result = strcmp (name_a, name_b);
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
    return thunar_file_compare_by_name (a, b, case_sensitive);
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

  content_type_a = thunar_file_get_content_type (THUNAR_FILE (a));
  content_type_b = thunar_file_get_content_type (THUNAR_FILE (b));

  if (content_type_a == NULL)
    content_type_a = "";
  if (content_type_b == NULL)
    content_type_b = "";

  result = strcasecmp (content_type_a, content_type_b);

  if (result == 0)
    result = thunar_file_compare_by_name (a, b, case_sensitive);

  return result;
}



static gint
sort_by_owner (const ThunarFile *a,
               const ThunarFile *b,
               gboolean          case_sensitive)
{
  const gchar *name_a;
  const gchar *name_b;
  ThunarUser  *user_a;
  ThunarUser  *user_b;
  guint32      uid_a;
  guint32      uid_b;
  gint         result;

  if (thunar_file_get_info (a) == NULL || thunar_file_get_info (b) == NULL)
    return thunar_file_compare_by_name (a, b, case_sensitive);

  user_a = thunar_file_get_user (a);
  user_b = thunar_file_get_user (b);

  if (user_a != NULL && user_b != NULL)
    {
      /* compare the system names */
      name_a = thunar_user_get_name (user_a);
      name_b = thunar_user_get_name (user_b);

      if (!case_sensitive)
        result = strcasecmp (name_a, name_b);
      else
        result = strcmp (name_a, name_b);
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
    return thunar_file_compare_by_name (a, b, case_sensitive);
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

  return thunar_file_compare_by_name (a, b, case_sensitive);
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

  return thunar_file_compare_by_name (a, b, case_sensitive);
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
      content_type_a = thunar_file_get_content_type (THUNAR_FILE (a));
      description_a = g_content_type_get_description (content_type_a);
    }

  if (thunar_file_is_symlink (b))
    {
      description_b = g_strdup_printf (_("link to %s"),
                                       thunar_file_get_symlink_target (b));
    }
  else
    {
      content_type_b = thunar_file_get_content_type (THUNAR_FILE (b));
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
    return thunar_file_compare_by_name (a, b, case_sensitive);
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
 * thunar_list_model_get_case_sensitive:
 * @store : a valid #ThunarListModel object.
 *
 * Returns %TRUE if the sorting is done in a case-sensitive
 * manner for @store.
 *
 * Return value: %TRUE if sorting is case-sensitive.
 **/
static gboolean
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
static void
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
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_CASE_SENSITIVE]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new case-sensitive setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) gtk_tree_model_row_changed,
                              NULL);
    }
}



/**
 * thunar_list_model_get_date_style:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the #ThunarDateStyle used to format dates in
 *               the given @store.
 **/
static ThunarDateStyle
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
static void
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
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_DATE_STYLE]);

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
  GtkTreePath   *path;
  gboolean       has_handler;
  GList         *files;
  GSequenceIter *row;
  GSequenceIter *end;
  GSequenceIter *next;

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

      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      /* remove existing entries */
      path = gtk_tree_path_new_first ();
      while (row != end)
        {
          /* remove the row from the list */
          next = g_sequence_iter_next (row);
          g_sequence_remove (row);
          row = next;

          /* notify the view(s) if they're actually
           * interested in the "row-deleted" signal.
           */
          if (G_LIKELY (has_handler))
            gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
        }
      gtk_tree_path_free (path);

      /* remove hidden entries */
      g_slist_free_full (store->hidden, g_object_unref);
      store->hidden = NULL;

      /* unregister signals and drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (store->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (G_OBJECT (store->folder));
    }

  /* ... just to be sure! */
  _thunar_assert (g_sequence_get_length (store->rows) == 0);

#ifndef NDEBUG
  /* new stamp since the model changed */
  store->stamp = g_random_int ();
#endif

  /* activate the new folder */
  store->folder = folder;

  /* freeze */
  g_object_freeze_notify (G_OBJECT (store));

  /* connect to the new folder (if any) */
  if (folder != NULL)
    {
      g_object_ref (G_OBJECT (folder));

      /* get the already loaded files */
      files = thunar_folder_get_files (folder);

      /* insert the files */
      if (files != NULL)
        thunar_list_model_files_added (folder, files, store);

      /* connect signals to the new folder */
      g_signal_connect (G_OBJECT (store->folder), "destroy", G_CALLBACK (thunar_list_model_folder_destroy), store);
      g_signal_connect (G_OBJECT (store->folder), "error", G_CALLBACK (thunar_list_model_folder_error), store);
      g_signal_connect (G_OBJECT (store->folder), "files-added", G_CALLBACK (thunar_list_model_files_added), store);
      g_signal_connect (G_OBJECT (store->folder), "files-removed", G_CALLBACK (thunar_list_model_files_removed), store);
    }

  /* notify listeners that we have a new folder */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FOLDER]);
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
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
static gboolean
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
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FOLDERS_FIRST]);
  thunar_list_model_sort (store);

  /* emit a "changed" signal for each row, so the display is
     reloaded with the new folders first setting */
  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          (GtkTreeModelForeachFunc) gtk_tree_model_row_changed,
                          NULL);
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
  GtkTreePath   *path;
  GtkTreeIter    iter;
  ThunarFile    *file;
  GSList        *lp;
  GSequenceIter *row;
  GSequenceIter *next;
  GSequenceIter *end;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if the settings differ */
  if (store->show_hidden == show_hidden)
    return;

  store->show_hidden = show_hidden;

  if (store->show_hidden)
    {
      for (lp = store->hidden; lp != NULL; lp = lp->next)
        {
          file = THUNAR_FILE (lp->data);

          /* insert file in the sorted position */
          row = g_sequence_insert_sorted (store->rows, file,
                                          thunar_list_model_cmp_func, store);

          GTK_TREE_ITER_INIT (iter, store->stamp, row);

          /* tell the view about the new row */
          path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);
        }
      g_slist_free (store->hidden);
      store->hidden = NULL;
    }
  else
    {
      _thunar_assert (store->hidden == NULL);

      /* remove all hidden files */
      row = g_sequence_get_begin_iter (store->rows);
      end = g_sequence_get_end_iter (store->rows);

      while (row != end)
        {
          next = g_sequence_iter_next (row);

          file = g_sequence_get (row);
          if (thunar_file_is_hidden (file))
            {
              /* store file in the list */
              store->hidden = g_slist_prepend (store->hidden, g_object_ref (file));

              /* setup path for "row-deleted" */
              path = gtk_tree_path_new_from_indices (g_sequence_iter_get_position (row), -1);

              /* remove file from the model */
              g_sequence_remove (row);

              /* notify the view(s) */
              gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
              gtk_tree_path_free (path);
            }

          row = next;
          _thunar_assert (end == g_sequence_get_end_iter (store->rows));
        }
    }

  /* notify listeners about the new setting */
  g_object_freeze_notify (G_OBJECT (store));
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_SHOW_HIDDEN]);
  g_object_thaw_notify (G_OBJECT (store));
}



/**
 * thunar_list_model_get_file_size_binary:
 * @store : a valid #ThunarListModel object.
 *
 * Returns %TRUE if the file size should be formatted
 * as binary.
 *
 * Return value: %TRUE if file size format is binary.
 **/
gboolean
thunar_list_model_get_file_size_binary (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  return store->file_size_binary;
}



/**
 * thunar_list_model_set_file_size_binary:
 * @store            : a valid #ThunarListModel object.
 * @file_size_binary : %TRUE to format file size as binary.
 *
 * If @file_size_binary is %TRUE the file size should be
 * formatted as binary.
 **/
void
thunar_list_model_set_file_size_binary (ThunarListModel *store,
                                        gboolean         file_size_binary)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* normalize the setting */
  file_size_binary = !!file_size_binary;

  /* check if we have a new setting */
  if (store->file_size_binary != file_size_binary)
    {
      /* apply the new setting */
      store->file_size_binary = file_size_binary;

      /* resort the model with the new setting */
      thunar_list_model_sort (store);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FILE_SIZE_BINARY]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new binary file size setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) gtk_tree_model_row_changed,
                              NULL);
    }
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

  return g_object_ref (g_sequence_get (iter->user_data));
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
static gint
thunar_list_model_get_num_files (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), 0);
  return g_sequence_get_length (store->rows);
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
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s for @files.
 **/
GList*
thunar_list_model_get_paths_for_files (ThunarListModel *store,
                                       GList           *files)
{
  GList         *paths = NULL;
  GSequenceIter *row;
  GSequenceIter *end;
  gint           i = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  /* find the rows for the given files */
  while (row != end)
    {
      if (g_list_find (files, g_sequence_get (row)) != NULL)
        {
          _thunar_assert (i == g_sequence_iter_get_position (row));
          paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (i, -1));
        }

      row = g_sequence_iter_next (row);
      i++;
    }

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
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s that match @pattern.
 **/
GList*
thunar_list_model_get_paths_for_pattern (ThunarListModel *store,
                                         const gchar     *pattern)
{
  GPatternSpec  *pspec;
  GList         *paths = NULL;
  GSequenceIter *row;
  GSequenceIter *end;
  ThunarFile    *file;
  gint           i = 0;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (pattern, -1, NULL), NULL);

  /* compile the pattern */
  pspec = g_pattern_spec_new (pattern);

  row = g_sequence_get_begin_iter (store->rows);
  end = g_sequence_get_end_iter (store->rows);

  /* find all rows that match the given pattern */
  while (row != end)
    {
      file = g_sequence_get (row);
      if (g_pattern_match_string (pspec, thunar_file_get_display_name (file)))
        {
          _thunar_assert (i == g_sequence_iter_get_position (row));
          paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (i, -1));
        }

      row = g_sequence_iter_next (row);
      i++;
    }

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
  const gchar       *content_type;
  const gchar       *original_path;
  GtkTreeIter        iter;
  ThunarFile        *file;
  guint64            size;
  guint64            size_summary;
  gint               folder_count;
  gint               non_folder_count;
  GList             *lp;
  gchar             *absolute_path;
  gchar             *fspace_string;
  gchar             *display_name;
  gchar             *size_string;
  gchar             *text;
  gchar             *folder_text;
  gchar             *non_folder_text;
  gchar             *s;
  gint               height;
  gint               width;
  gchar             *description;
  GSequenceIter     *row;
  GSequenceIter     *end;
  gint               nrows;
  ThunarPreferences *preferences;
  gboolean           show_image_size;
  gboolean           file_size_binary;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  file_size_binary = thunar_list_model_get_file_size_binary(store);

  if (selected_items == NULL)
    {
      /* try to determine a file for the current folder */
      file = (store->folder != NULL) ? thunar_folder_get_corresponding_file (store->folder) : NULL;

      nrows = g_sequence_get_length (store->rows);

      /* check if we can determine the amount of free space for the volume */
      if (G_LIKELY (file != NULL
          && thunar_g_file_get_free_space (thunar_file_get_file (file), &size, NULL)))
        {
          /* humanize the free space */
          fspace_string = g_format_size_full (size, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
          size_summary = 0;

          row = g_sequence_get_begin_iter (store->rows);
          end = g_sequence_get_end_iter (store->rows);

          /* calculate the size of all file items */
          while (row != end)
            {
              file = g_sequence_get (row);
              if (thunar_file_is_regular (file))
                size_summary += thunar_file_get_size (file);
              row = g_sequence_iter_next (row);
            }

          if (size_summary > 0)
            {
              /* generate a text which includes the size of all items in the folder */
              size_string = g_format_size_full (size_summary, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
              text = g_strdup_printf (ngettext ("%d item (%s), Free space: %s", "%d items (%s), Free space: %s", nrows),
                                      nrows, size_string, fspace_string);
              g_free (size_string);
            }
          else
            {
              /* just the standard text */
              text = g_strdup_printf (ngettext ("%d item, Free space: %s", "%d items, Free space: %s", nrows),
                                      nrows, fspace_string);
            }

          /* cleanup */
          g_free (fspace_string);
        }
      else
        {
          text = g_strdup_printf (ngettext ("%d item", "%d items", nrows), nrows);
        }
    }
  else if (selected_items->next == NULL)
    {
      /* resolve the iter for the single path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, selected_items->data);

      /* get the file for the given iter */
      file = g_sequence_get (iter.user_data);

      /* determine the content type of the file */
      content_type = thunar_file_get_content_type (file);

      if (G_UNLIKELY (content_type != NULL && g_str_equal (content_type, "inode/symlink")))
        {
          text = g_strdup_printf (_("\"%s\" broken link"), thunar_file_get_display_name (file));
        }
      else if (G_UNLIKELY (thunar_file_is_symlink (file)))
        {
          size_string = thunar_file_get_size_string_formatted (file, file_size_binary);
          text = g_strdup_printf (_("\"%s\" (%s) link to %s"), thunar_file_get_display_name (file),
                                  size_string, thunar_file_get_symlink_target (file));
          g_free (size_string);
        }
      else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT))
        {
          text = g_strdup_printf (_("\"%s\" shortcut"), thunar_file_get_display_name (file));
        }
      else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE))
        {
          text = g_strdup_printf (_("\"%s\" mountable"), thunar_file_get_display_name (file));
        }
      else if (thunar_file_is_regular (file))
        {
          description = g_content_type_get_description (content_type);
          size_string = thunar_file_get_size_string_formatted (file, file_size_binary);
          /* I18N, first %s is the display name of the file, 2nd the file size, 3rd the content type */
          text = g_strdup_printf (_("\"%s\" (%s) %s"), thunar_file_get_display_name (file),
                                  size_string, description);
          g_free (description);
          g_free (size_string);
        }
      else
        {
          description = g_content_type_get_description (content_type);
          /* I18N, first %s is the display name of the file, second the content type */
          text = g_strdup_printf (_("\"%s\" %s"), thunar_file_get_display_name (file), description);
          g_free (description);
        }

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
      else if (thunar_file_is_local (file)
               && thunar_file_is_regular (file)
               && g_str_has_prefix (content_type, "image/")) /* bug #2913 */
        {
          /* check if the size should be visible in the statusbar, disabled by
           * default to avoid high i/o  */
          preferences = thunar_preferences_get ();
          g_object_get (preferences, "misc-image-size-in-statusbar", &show_image_size, NULL);
          g_object_unref (preferences);

          if (show_image_size)
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
    }
  else
    {
      /* reset */
      size_summary = 0;
      folder_count = 0;
      non_folder_count = 0;

      /* analyze selection */
      for (lp = selected_items; lp != NULL; lp = lp->next)
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, lp->data);
          file = g_sequence_get (iter.user_data);
          if (thunar_file_is_directory (file))
            {
              folder_count++;
            }
          else
            {
              non_folder_count++;
              if (thunar_file_is_regular (file))
                size_summary += thunar_file_get_size (file);
            }
        }

     /* text for the items in the folder */
     if (non_folder_count > 0)
        {
          size_string = g_format_size_full (size_summary, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
          if (folder_count > 0)
            {
              /* item count if there are also folders in the selection */
              non_folder_text = g_strdup_printf (ngettext ("%d other item selected (%s)",
                                                           "%d other items selected (%s)",
                                                           non_folder_count), non_folder_count, size_string);
            }
          else
            {
              /* only non-folders are selected */
              non_folder_text = g_strdup_printf (ngettext ("%d item selected (%s)",
                                                           "%d items selected (%s)",
                                                           non_folder_count), non_folder_count, size_string);
            }
          g_free (size_string);
        }
      else
        {
          non_folder_text = NULL;
        }

      /* text for the folders */
      if (folder_count > 0)
        {
          folder_text = g_strdup_printf (ngettext ("%d folder selected",
                                                   "%d folders selected",
                                                   folder_count), folder_count);
        }
      else
        {
          folder_text = NULL;
        }

      if (folder_text == NULL)
        text = non_folder_text;
      else if (non_folder_text == NULL)
        text = folder_text;
      else
        {
          /* This is marked for translation in case a localizer
           * needs to change ", " to something else. The comma
           * is between the message about the number of folders
           * and the number of items in the selection */
          text = g_strdup_printf (_("%s, %s"), folder_text, non_folder_text);
          g_free (folder_text);
          g_free (non_folder_text);
        }
    }

  return text;
}
