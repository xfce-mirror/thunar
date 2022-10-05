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

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-util.h>



/* convenience macros */
#define G_NODE(node)                 ((GNode *) (node))
#define THUNAR_LIST_MODEL_ITEM(item) ((ThunarListModelItem *) (item))
#define G_NODE_HAS_DUMMY(node)       (node->children != NULL \
                                      && node->children->data == NULL \
                                      && node->children->next == NULL)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
  PROP_DATE_STYLE,
  PROP_DATE_CUSTOM_STYLE,
  PROP_FOLDER,
  PROP_FOLDERS_FIRST,
  PROP_NUM_FILES,
  PROP_SHOW_HIDDEN,
  PROP_FILE_SIZE_BINARY,
  PROP_TREE_VIEW,
  N_PROPERTIES
};

/* Signal identifiers */
enum
{
  ERROR,
  SEARCH_DONE,
  LAST_SIGNAL,
};



typedef struct _ThunarListModelItem ThunarListModelItem;



typedef gint (*ThunarSortFunc) (const ThunarFile *a,
                                const ThunarFile *b,
                                gboolean          case_sensitive);

static void                 thunar_list_model_tree_model_init             (GtkTreeModelIface           *iface);
static void                 thunar_list_model_drag_dest_init              (GtkTreeDragDestIface        *iface);
static void                 thunar_list_model_sortable_init               (GtkTreeSortableIface        *iface);
static void                 thunar_list_model_dispose                     (GObject                     *object);
static void                 thunar_list_model_finalize                    (GObject                     *object);
static void                 thunar_list_model_get_property                (GObject                     *object,
                                                                           guint                        prop_id,
                                                                           GValue                      *value,
                                                                           GParamSpec                  *pspec);
static void                 thunar_list_model_set_property                (GObject                     *object,
                                                                           guint                        prop_id,
                                                                           const GValue                *value,
                                                                           GParamSpec                  *pspec);
static GtkTreeModelFlags    thunar_list_model_get_flags                   (GtkTreeModel                *model);
static gint                 thunar_list_model_get_n_columns               (GtkTreeModel                *model);
static GType                thunar_list_model_get_column_type             (GtkTreeModel                *model,
                                                                           gint                         idx);
static gboolean             thunar_list_model_get_iter                    (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter,
                                                                           GtkTreePath                 *path);
static GtkTreePath         *thunar_list_model_get_path                    (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter);
static void                 thunar_list_model_get_value                   (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter,
                                                                           gint                         column,
                                                                           GValue                      *value);
static gboolean             thunar_list_model_iter_next                   (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter);
static gboolean             thunar_list_model_iter_children               (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter,
                                                                           GtkTreeIter                 *parent);
static gboolean             thunar_list_model_iter_has_child              (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter);
static gint                 thunar_list_model_iter_n_children             (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter);
static gboolean             thunar_list_model_iter_nth_child              (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter,
                                                                           GtkTreeIter                 *parent,
                                                                           gint                         n);
static gboolean             thunar_list_model_iter_parent                 (GtkTreeModel                *model,
                                                                           GtkTreeIter                 *iter,
                                                                           GtkTreeIter                 *child);
static void                 thunar_list_model_ref_node                    (GtkTreeModel                *tree_model,
                                                                           GtkTreeIter                 *iter);
static void                 thunar_list_model_unref_node                  (GtkTreeModel                *tree_model,
                                                                           GtkTreeIter                 *iter);
static gboolean             thunar_list_model_drag_data_received          (GtkTreeDragDest             *dest,
                                                                           GtkTreePath                 *path,
                                                                           GtkSelectionData            *data);
static gboolean             thunar_list_model_row_drop_possible           (GtkTreeDragDest             *dest,
                                                                           GtkTreePath                 *path,
                                                                           GtkSelectionData            *data);
static gboolean             thunar_list_model_get_sort_column_id          (GtkTreeSortable             *sortable,
                                                                           gint                        *sort_column_id,
                                                                           GtkSortType                 *order);
static void                 thunar_list_model_set_sort_column_id          (GtkTreeSortable             *sortable,
                                                                           gint                         sort_column_id,
                                                                           GtkSortType                  order);
static void                 thunar_list_model_set_default_sort_func       (GtkTreeSortable             *sortable,
                                                                           GtkTreeIterCompareFunc       func,
                                                                           gpointer                     data,
                                                                           GDestroyNotify               destroy);
static void                 thunar_list_model_set_sort_func               (GtkTreeSortable             *sortable,
                                                                           gint                         sort_column_id,
                                                                           GtkTreeIterCompareFunc       func,
                                                                           gpointer                     data,
                                                                           GDestroyNotify               destroy);
static gboolean             thunar_list_model_has_default_sort_func       (GtkTreeSortable             *sortable);
static gint                 thunar_list_model_cmp_func                    (gconstpointer                a,
                                                                           gconstpointer                b,
                                                                           gpointer                     user_data);
static void                 thunar_list_model_sort                        (ThunarListModel             *store,
                                                                           GNode                       *node);
static void                 thunar_list_model_file_changed                (ThunarFileMonitor           *file_monitor,
                                                                           ThunarFile                  *file,
                                                                           ThunarListModel             *store);
static void                 thunar_list_model_folder_files_added          (ThunarFolder                *folder,
                                                                           GList                       *files,
                                                                           ThunarListModel             *store);
static void                 thunar_list_model_folder_files_removed        (ThunarFolder                *folder,
                                                                           GList                       *files,
                                                                           ThunarListModel             *store);
static void                 thunar_list_model_folder_destroy              (ThunarFolder                *folder,
                                                                           ThunarListModel             *store);
static void                 thunar_list_model_folder_error                (ThunarFolder                *folder,
                                                                           const GError                *error,
                                                                           ThunarListModel             *store);
static gint                 sort_by_date                                  (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive,
                                                                           gint                         type);
static gint                 sort_by_date_created                          (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_date_accessed                         (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_date_modified                         (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_date_deleted                          (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_recency                               (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_location                              (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_group                                 (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_mime_type                             (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_owner                                 (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_permissions                           (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_size                                  (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_size_in_bytes                         (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gint                 sort_by_type                                  (const ThunarFile            *a,
                                                                           const ThunarFile            *b,
                                                                           gboolean                     case_sensitive);
static gboolean             thunar_list_model_get_case_sensitive          (ThunarListModel             *store);
static void                 thunar_list_model_set_case_sensitive          (ThunarListModel             *store,
                                                                           gboolean                     case_sensitive);
static ThunarDateStyle      thunar_list_model_get_date_style              (ThunarListModel             *store);
static void                 thunar_list_model_set_date_style              (ThunarListModel             *store,
                                                                           ThunarDateStyle              date_style);
static const char*          thunar_list_model_get_date_custom_style       (ThunarListModel             *store);
static void                 thunar_list_model_set_date_custom_style       (ThunarListModel             *store,
                                                                           const char                  *date_custom_style);
static gint                 thunar_list_model_get_num_files               (ThunarListModel             *store);
static gboolean             thunar_list_model_get_folders_first           (ThunarListModel             *store);
static ThunarJob*           thunar_list_model_job_search_directory        (ThunarListModel             *model,
                                                                           const gchar                 *search_query_c,
                                                                           ThunarFile                  *directory);
static void                 thunar_list_model_search_folder               (ThunarListModel             *model,
                                                                           ThunarJob                   *job,
                                                                           gchar                       *uri,
                                                                           gchar                      **search_query_c_terms,
                                                                           enum ThunarListModelSearch   search_type,
                                                                           gboolean                     show_hidden);
static void                 thunar_list_model_cancel_search_job           (ThunarListModel             *model);
static ThunarListModelItem *thunar_list_model_item_new_with_file        (ThunarListModel             *model,
                                                                           ThunarFile                  *file) G_GNUC_MALLOC;
static void                 thunar_list_model_item_free                   (ThunarListModelItem         *item);
static void                 thunar_list_model_item_load_folder            (ThunarListModelItem         *item);
static void                 thunar_list_model_item_files_added            (ThunarListModelItem         *item,
                                                                           GList                       *files,
                                                                           ThunarFolder                *folder);
static void                 thunar_list_model_item_files_removed          (ThunarListModelItem         *item,
                                                                           GList                       *files,
                                                                           ThunarFolder                *folder);
static gboolean             thunar_list_model_item_load_idle              (gpointer                     user_data);
static void                 thunar_list_model_item_load_idle_destroy      (gpointer                     user_data);
static void                 thunar_list_model_item_notify_loading        (ThunarListModelItem         *item,
                                                                         GParamSpec                  *pspec,
                                                                         ThunarFolder                *folder);
static void                 thunar_list_model_node_insert_dummy         (GNode                       *parent,
                                                                         ThunarListModel             *model);
static void                 thunar_list_model_node_drop_dummy           (GNode                       *node,
                                                                         ThunarListModel             *model);
static gboolean             thunar_list_model_node_traverse_cleanup     (GNode                       *node,
                                                                         gpointer                     user_data);
static gboolean             thunar_list_model_node_traverse_changed     (GNode                       *node,
                                                                         gpointer                     user_data);
static gboolean             thunar_list_model_node_traverse_remove      (GNode                       *node,
                                                                         gpointer                     user_data);
static gboolean             thunar_list_model_node_traverse_sort        (GNode                       *node,
                                                                         gpointer                     user_data);
static gboolean             thunar_list_model_node_traverse_free        (GNode                       *node,
                                                                         gpointer                     user_data);



struct _ThunarListModelClass
{
  GObjectClass __parent__;

  /* signals */
  void (*error) (ThunarListModel *store,
                 const GError    *error);
  void (*search_done) (void);
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

  GNode          *root;
  GList          *hidden;
  ThunarFolder   *folder;
  gboolean        show_hidden : 1;
  gboolean        file_size_binary : 1;
  ThunarDateStyle date_style;
  char           *date_custom_style;

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

  /* searching runs in a separate thread which incrementally inserts results (files)
   * in the files_to_add list.
   * Periodically the main thread takes all the files in the files_to_add list
   * and adds them in the model. The list is then emptied.
   */
  ThunarJob     *recursive_search_job;
  GList         *files_to_add;
  GMutex         mutex_files_to_add;

  /* used to stop the periodic call to add_search_files when the search is finished/canceled */
  guint          update_search_results_timeout_id;

  guint          cleanup_idle_id;

  gboolean       tree_view;
};

struct _ThunarListModelItem
{
  gint             ref_count;
  guint            load_idle_id;
  ThunarFile      *file;
  ThunarFolder    *folder;
  ThunarListModel *model;

  /* list of children of this node that are
   * not visible in the treeview */
  GSList          *invisible_children;
};

typedef struct
{
  gint   offset;
  GNode *node;
} SortTuple;



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
   * ThunarListModel:date-custom-style:
   *
   * The style used for custom format of dates.
   **/
  list_model_props[PROP_DATE_CUSTOM_STYLE] =
      g_param_spec_string ("date-custom-style",
                           "DateCustomStyle",
                           NULL,
                           "%Y-%m-%d %H:%M:%S",
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
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel::tree-view:
   *
   * Tells whether the model is for a tree-view.
   **/
  list_model_props[PROP_TREE_VIEW] =
      g_param_spec_boolean ("tree-view",
                            "tree-view",
                            "tree-view",
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

  /**
   * ThunarListModel::search-done:
   * @store : a #ThunarListModel.
   * @error : a #GError that describes the problem.
   *
   * Emitted when a recursive search finishes.
   **/
  list_model_signals[SEARCH_DONE] =
      g_signal_new (I_("search-done"),
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarListModelClass, search_done),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);
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
  iface->ref_node         = thunar_list_model_ref_node;
  iface->unref_node       = thunar_list_model_unref_node;
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
  g_mutex_init (&store->mutex_files_to_add);

  store->cleanup_idle_id = 0;

  /* allocate the "virtual root node" */
  store->root = g_node_new (NULL);

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
  thunar_list_model_set_folder (THUNAR_LIST_MODEL (object), NULL, NULL);

  (*G_OBJECT_CLASS (thunar_list_model_parent_class)->dispose) (object);
}



static void
thunar_list_model_finalize (GObject *object)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (object);

  thunar_list_model_cancel_search_job (store);

  /* remove the cleanup idle */
  if (store->cleanup_idle_id != 0)
    g_source_remove (store->cleanup_idle_id);

  if (store->update_search_results_timeout_id > 0)
    {
      g_source_remove (store->update_search_results_timeout_id);
      store->update_search_results_timeout_id = 0;
    }
  thunar_g_list_free_full (store->files_to_add);
  store->files_to_add = NULL;

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (G_OBJECT (store->file_monitor), thunar_list_model_file_changed, store);
  g_object_unref (G_OBJECT (store->file_monitor));

  /* release all resources allocated to the model */
  if (store->root != NULL)
    {
      g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_list_model_node_traverse_free, NULL);
      g_node_destroy (store->root);
      store->root = NULL;
    }

  g_mutex_clear (&store->mutex_files_to_add);

  g_free (store->date_custom_style);

  /* start with tree view disabled */
  store->tree_view = FALSE;

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

    case PROP_DATE_CUSTOM_STYLE:
      g_value_set_string (value, thunar_list_model_get_date_custom_style (store));
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

    case PROP_TREE_VIEW:
      g_value_set_boolean (value, store->tree_view);
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

    case PROP_DATE_CUSTOM_STYLE:
      thunar_list_model_set_date_custom_style (store, g_value_get_string (value));
      break;

    case PROP_FOLDER:
      thunar_list_model_set_folder (store, g_value_get_object (value), NULL);
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

    case PROP_TREE_VIEW:
      store->tree_view = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_list_model_get_flags (GtkTreeModel *model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | (THUNAR_LIST_MODEL (model)->tree_view ? 0 : GTK_TREE_MODEL_LIST_ONLY);
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
    case THUNAR_COLUMN_DATE_CREATED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_ACCESSED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_MODIFIED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_DELETED:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_RECENCY:
      return G_TYPE_STRING;

    case THUNAR_COLUMN_LOCATION:
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

    case THUNAR_COLUMN_SIZE_IN_BYTES:
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
  GtkTreeIter      parent;
  const gint      *indices;
  gint             depth;
  gint             n;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* Nothing to get if folder is not set */
  if (store->root == NULL)
    return FALSE;

  /* determine the path depth */
  depth = gtk_tree_path_get_depth (path);

  /* determine the path indices */
  indices = gtk_tree_path_get_indices (path);

  /* initialize the parent iterator with the root element */
  GTK_TREE_ITER_INIT (parent, store->stamp, store->root);
  if (!gtk_tree_model_iter_nth_child (model, iter, &parent, indices[0]))
    return FALSE;

  for (n = 1; n < depth; ++n)
    {
      parent = *iter;
      if (!gtk_tree_model_iter_nth_child (model, iter, &parent, indices[n]))
        return FALSE;
    }

  return TRUE;
}



static GtkTreePath*
thunar_list_model_get_path (GtkTreeModel *model,
                            GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GtkTreePath     *path;
  GtkTreeIter      tmp_iter;
  GNode           *tmp_node;
  GNode           *node;
  gint             n;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_LIST_MODEL (model)->stamp, NULL);

  /* determine the node for the iterator */
  node = iter->user_data;

  /* check if the iter refers to the "virtual root node" */
  if (node->parent == NULL && node == store->root)
    return gtk_tree_path_new ();
  else if (G_UNLIKELY (node->parent == NULL))
    return NULL;

  if (node->parent == store->root)
    {
      path = gtk_tree_path_new ();
      tmp_node = g_node_first_child (store->root);
    }
  else
    {
      /* determine the iterator for the parent node */
      GTK_TREE_ITER_INIT (tmp_iter, store->stamp, node->parent);

      /* determine the path for the parent node */
      path = gtk_tree_model_get_path (model, &tmp_iter);

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
thunar_list_model_get_value (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ThunarListModelItem *item;
  ThunarGroup  *group;
  const gchar  *device_type;
  const gchar  *name;
  const gchar  *real_name;
  ThunarUser   *user;
  ThunarFolder *folder;
  GFile        *g_file;
  GFile        *g_file_parent;
  gchar        *str;
  gchar        *loading = g_strdup (_("Loading..."));
  GNode        *node = G_NODE (iter->user_data);

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (iter->stamp == (THUNAR_LIST_MODEL (model))->stamp);

  item = node->data;

  switch (column)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        str = thunar_file_get_date_string (item->file, THUNAR_FILE_DATE_CREATED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        str = thunar_file_get_date_string (item->file, THUNAR_FILE_DATE_ACCESSED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        str = thunar_file_get_date_string (item->file, THUNAR_FILE_DATE_MODIFIED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        str = thunar_file_get_date_string (item->file, THUNAR_FILE_DATE_DELETED, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_RECENCY:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        str = thunar_file_get_date_string (item->file, THUNAR_FILE_RECENCY, THUNAR_LIST_MODEL (model)->date_style, THUNAR_LIST_MODEL (model)->date_custom_style);
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_LOCATION:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        {
          g_file_parent = g_file_get_parent (thunar_file_get_file (item->file));
          str = NULL;

          /* g_file_parent will be NULL only if a search returned the root
          * directory somehow, or "file:///" is in recent:/// somehow.
          * These should be quite rare circumstances. */
          if (G_UNLIKELY (g_file_parent == NULL))
            {
              g_value_take_string (value, NULL);
              break;
            }

          /* Try and show a relative path beginning with the current folder's name to the parent folder.
          * Fall thru with str==NULL if that is not possible. */
          folder = THUNAR_LIST_MODEL (model)->folder;
          if (G_LIKELY (folder != NULL))
            {
              const gchar *folder_basename = thunar_file_get_basename( thunar_folder_get_corresponding_file (folder));
              GFile *g_folder = thunar_file_get_file (thunar_folder_get_corresponding_file (folder));
              if (g_file_equal (g_folder, g_file_parent))
                {
                  /* commonest non-prefix case: item location is directly inside the search folder */
                  str = g_strdup (folder_basename);
                }
              else
                {
                  str = g_file_get_relative_path (g_folder, g_file_parent);
                  /* str can still be NULL if g_folder is not a prefix of g_file_parent */
                  if (str != NULL)
                    {
                      gchar *tmp = g_build_path (G_DIR_SEPARATOR_S, folder_basename, str, NULL);
                      g_free (str);
                      str = tmp;
                    }
                }
            }

          /* catchall for when model->folder is not an ancestor of the parent (e.g. when searching recent:///).
          * In this case, show a prettified absolute URI or local path. */
          if (str == NULL)
            str = g_file_get_parse_name (g_file_parent);

          g_object_unref (g_file_parent);
        }
      g_value_take_string (value, item != NULL ? str : loading);
      break;

    case THUNAR_COLUMN_GROUP:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        {
          group = thunar_file_get_group (item->file);
        if (G_LIKELY (group != NULL))
          {
            g_value_set_string (value, thunar_group_get_name (group));
            g_object_unref (G_OBJECT (group));
          }
        else
          {
            g_value_set_static_string (value, _("Unknown"));
          }
        }
      else
        g_value_take_string (value, loading);
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item != NULL ? thunar_file_get_content_type (item->file) : _("Loading..."));

      break;

    case THUNAR_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item != NULL ? thunar_file_get_display_name (item->file) : _("Loading..."));
      break;

    case THUNAR_COLUMN_OWNER:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        {
          user = thunar_file_get_user (item->file);
          if (G_LIKELY (user != NULL))
            {
              /* determine sane display name for the owner */
              name = thunar_user_get_name (user);
              real_name = thunar_user_get_real_name (user);
              if(G_LIKELY (real_name != NULL))
                {
                  if(strcmp (name, real_name) == 0)
                    str = g_strdup (name);
                  else
                    str = g_strdup_printf ("%s (%s)", real_name, name);
                }
              else
                str = g_strdup (name);
              g_value_take_string (value, str);
              g_object_unref (G_OBJECT (user));
            }
          else
            {
              g_value_set_static_string (value, _("Unknown"));
            }
        }
      else
        g_value_set_static_string (value, _("Loading..."));
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, item != NULL ? thunar_file_get_mode_string (item->file) : loading);
      break;

    case THUNAR_COLUMN_SIZE:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        {
          if (thunar_file_is_mountable (item->file))
            {
              g_file = thunar_file_get_target_location (item->file);
              if (g_file == NULL)
                break;
              g_value_take_string (value, thunar_g_file_get_free_space_string (g_file, THUNAR_LIST_MODEL (model)->file_size_binary));
              g_object_unref (g_file);
              break;
            }
          if (!thunar_file_is_directory (item->file))
            g_value_take_string (value, thunar_file_get_size_string_formatted (item->file, THUNAR_LIST_MODEL (model)->file_size_binary));
        }
      else
        g_value_set_static_string (value, _("Loading..."));
      break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, item != NULL ? thunar_file_get_size_in_bytes_string (item->file) : loading);
      break;

    case THUNAR_COLUMN_TYPE:
      g_value_init (value, G_TYPE_STRING);
      if (item != NULL)
        {
          device_type = thunar_file_get_device_type (item->file);
          if (device_type != NULL)
            {
              g_value_take_string (value, g_strdup (device_type));
              break;
            }
          g_value_take_string (value, thunar_file_get_content_type_desc (item->file));
        }
      else
        g_value_set_static_string (value, _("Loading..."));
      break;

    case THUNAR_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, item != NULL ? item->file : NULL);
      break;

    case THUNAR_COLUMN_FILE_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item != NULL ? thunar_file_get_display_name (item->file) : _("Loading..."));
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

  /* check if we have any further nodes in this row */
  if (g_node_next_sibling (iter->user_data) != NULL)
    {
      iter->user_data = g_node_next_sibling (iter->user_data);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_children (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GNode           *children;

  _thunar_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (parent == NULL || parent->stamp == store->stamp, FALSE);

  if (G_LIKELY (parent != NULL))
    children = g_node_first_child (parent->user_data);
  else
    children = g_node_first_child (store->root);

  if (G_LIKELY (children != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, children);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_has_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (iter->stamp == THUNAR_LIST_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  return (g_node_first_child (iter->user_data) != NULL);

}



static gint
thunar_list_model_iter_n_children (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);

  _thunar_return_val_if_fail (iter == NULL || iter->user_data != NULL, 0);
  _thunar_return_val_if_fail (iter == NULL || iter->stamp == store->stamp, 0);

  return g_node_n_children ((iter == NULL) ? store->root : iter->user_data);
}



static gboolean
thunar_list_model_iter_nth_child (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GNode           *child;

  _thunar_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (parent == NULL || parent->stamp == store->stamp, FALSE);

  child = g_node_nth_child ((parent != NULL) ? parent->user_data : store->root, n);
  if (G_LIKELY (child != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, child);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_list_model_iter_parent (GtkTreeModel *model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *child)
{
  ThunarListModel *store = THUNAR_LIST_MODEL (model);
  GNode           *parent;

  _thunar_return_val_if_fail (iter != NULL, FALSE);
  _thunar_return_val_if_fail (child->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (child->stamp == store->stamp, FALSE);

  /* check if we have a parent for iter */
  parent = G_NODE (child->user_data)->parent;
  if (G_LIKELY (parent != store->root))
    {
      GTK_TREE_ITER_INIT (*iter, store->stamp, parent);
      return TRUE;
    }
  else
    {
      /* no "real parent" for this node */
      return FALSE;
    }
}



static void
thunar_list_model_ref_node (GtkTreeModel *model,
                            GtkTreeIter  *iter)
{
  ThunarListModelItem *item;
  ThunarListModel     *store = THUNAR_LIST_MODEL (model);
  GNode               *node;

  _thunar_return_if_fail (iter->user_data != NULL);
  _thunar_return_if_fail (iter->stamp == store->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == store->root))
    return;

  /* check if we have a dummy item here */
  item = node->data;
  if (G_UNLIKELY (item == NULL))
    {
      /* tell the parent to load the folder */
      thunar_list_model_item_load_folder (node->parent->data);
    }
  else
    {
      /* schedule a reload of the folder if it is cleaned earlier */
      if (G_UNLIKELY (item->ref_count == 0))
        thunar_list_model_item_load_folder (item);

      /* increment the reference count */
      item->ref_count += 1;
    }
}



static void
thunar_list_model_unref_node (GtkTreeModel *model,
                              GtkTreeIter  *iter)
{
  ThunarListModelItem *item;
  ThunarListModel     *store = THUNAR_LIST_MODEL (model);
  GNode               *node;

  _thunar_return_if_fail (iter->user_data != NULL);
  _thunar_return_if_fail (iter->stamp == store->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == store->root))
    return;

  /* check if this a non-dummy item, if so, decrement the reference count */
  item = node->data;
  if (G_LIKELY (item != NULL))
    item->ref_count -= 1;

  /* NOTE: we don't cleanup nodes when the item ref count is zero,
   * because GtkTreeView also does a lot of reffing when scrolling the
   * tree, which results in all sorts for glitches */
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
  else if (store->sort_func == sort_by_size_in_bytes)
    *sort_column_id = THUNAR_COLUMN_SIZE_IN_BYTES;
  else if (store->sort_func == sort_by_date_created)
    *sort_column_id = THUNAR_COLUMN_DATE_CREATED;
  else if (store->sort_func == sort_by_date_accessed)
    *sort_column_id = THUNAR_COLUMN_DATE_ACCESSED;
  else if (store->sort_func == sort_by_date_modified)
    *sort_column_id = THUNAR_COLUMN_DATE_MODIFIED;
  else if (store->sort_func == sort_by_date_deleted)
    *sort_column_id = THUNAR_COLUMN_DATE_DELETED;
  else if (store->sort_func == sort_by_recency)
    *sort_column_id = THUNAR_COLUMN_RECENCY;
  else if (store->sort_func == sort_by_location)
    *sort_column_id = THUNAR_COLUMN_LOCATION;
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
    case THUNAR_COLUMN_DATE_CREATED:
      store->sort_func = sort_by_date_created;
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      store->sort_func = sort_by_date_accessed;
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      store->sort_func = sort_by_date_modified;
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      store->sort_func = sort_by_date_deleted;
      break;

    case THUNAR_COLUMN_RECENCY:
      store->sort_func = sort_by_recency;
      break;

    case THUNAR_COLUMN_LOCATION:
      store->sort_func = sort_by_location;
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

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      store->sort_func = sort_by_size_in_bytes;
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
  g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_list_model_node_traverse_sort, store);

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
  a = THUNAR_LIST_MODEL_ITEM (((const SortTuple *) a)->node->data)->file;
  b = THUNAR_LIST_MODEL_ITEM (((const SortTuple *) b)->node->data)->file;

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
thunar_list_model_sort (ThunarListModel *store,
                        GNode           *node)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  SortTuple   *sort_array;
  GNode       *child_node;
  guint        n_children;
  gint        *new_order;
  guint        n;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

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
      _thunar_return_if_fail (child_node != NULL);
      _thunar_return_if_fail (child_node->data != NULL);

      sort_array[n].node = child_node;
      sort_array[n].offset = n;
    }

  /* sort the array using QuickSort */
  g_qsort_with_data (sort_array, n_children, sizeof (SortTuple), thunar_list_model_cmp_func, store);

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
  GTK_TREE_ITER_INIT (iter, store->stamp, node);

  /* tell the view about the new item order */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store), path, &iter, new_order);
  gtk_tree_path_free (path);

  /* cleanup if we used the heap */
  if (G_UNLIKELY (n_children >= 500))
    g_free (sort_array);
}



static gboolean
thunar_list_model_cleanup_idle (gpointer user_data)
{
  ThunarListModel *model = THUNAR_LIST_MODEL (user_data);

THUNAR_THREADS_ENTER

  /* walk through the tree and release all the nodes with a ref count of 0 */
  g_node_traverse (model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   thunar_list_model_node_traverse_cleanup, model);

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_list_model_cleanup_idle_destroy (gpointer user_data)
{
  THUNAR_LIST_MODEL (user_data)->cleanup_idle_id = 0;
}



static void
thunar_list_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarListModel   *store)
{
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  _thunar_return_if_fail (store->file_monitor == file_monitor);
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* traverse the model and emit "row-changed" for the file's nodes */
  if (thunar_file_is_directory (file) && store->root != NULL)
    g_node_traverse (store->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, thunar_list_model_node_traverse_changed, file);
}



static void
thunar_list_model_folder_destroy (ThunarFolder    *folder,
                                  ThunarListModel *store)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  thunar_list_model_set_folder (store, NULL, NULL);

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

  /* reset the current folder */
  thunar_list_model_set_folder (store, NULL, NULL);

  /* forward the error signal */
  g_signal_emit (G_OBJECT (store), list_model_signals[ERROR], 0, error);
}



static void
thunar_list_model_folder_files_added (ThunarFolder    *folder,
                                      GList           *files,
                                      ThunarListModel *store)
{
  ThunarListModelItem *item;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  ThunarFile          *file;
  GNode               *node;

  /* process all added files */
  for (GList *lp = files; lp != NULL; lp = lp->next)
    {
      /* take a reference on that file */
      file = THUNAR_FILE (g_object_ref (G_OBJECT (lp->data)));
      _thunar_return_if_fail (THUNAR_IS_FILE (file));

      /* check if the file should be hidden */
      if (!store->show_hidden && thunar_file_is_hidden (file))
        {
          store->hidden = g_list_prepend (store->hidden, file);
        }
      else
        {
          /* insert the file */
          item = thunar_list_model_item_new_with_file (store, file);
          node = g_node_append_data (store->root, item);

          /* generate an iterator for the new item */
          GTK_TREE_ITER_INIT (iter, store->stamp, node);

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
          gtk_tree_path_free (path);
        }
    }

  /* sort the rows */
  thunar_list_model_sort (store, store->root);

  /* number of visible files may have changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
}



static void
thunar_list_model_folder_files_removed (ThunarFolder    *folder,
                                        GList           *files,
                                        ThunarListModel *store)
{
  gboolean  found;

  /* drop all the referenced files from the model */
  for (GList *lp = files; lp != NULL; lp = lp->next)
    {
      found = FALSE;

      for (GNode *node = g_node_first_child (store->root); node != NULL; node = g_node_next_sibling (node))
        if (node->data != NULL && THUNAR_LIST_MODEL_ITEM (node->data)->file == lp->data)
          {
            g_node_traverse (node, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_list_model_node_traverse_remove, store);
            found = TRUE;
            break;
          }

      /* check if the file was found */
      if (!found)
        {
          /* file is hidden */
          _thunar_assert (g_list_find (store->hidden, lp->data) != NULL);
          store->hidden = g_list_remove (store->hidden, lp->data);
          g_object_unref (G_OBJECT (lp->data));
        }
    }

  /* this probably changed */
  g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_NUM_FILES]);
}



static ThunarListModelItem*
thunar_list_model_item_new_with_file (ThunarListModel *model,
                                      ThunarFile      *file)
{
  ThunarListModelItem *item;

  item = g_slice_new0 (ThunarListModelItem);
  item->file = THUNAR_FILE (g_object_ref (G_OBJECT (file)));
  item->model = model;

  return item;
}



static void
thunar_list_model_item_free (ThunarListModelItem *item)
{
  /* cancel any pending load idle source */
  if (G_UNLIKELY (item->load_idle_id != 0))
    g_source_remove (item->load_idle_id);

  /* disconnect from the folder */
  if (G_LIKELY (item->folder != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (item->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, item);
      g_object_unref (G_OBJECT (item->folder));
      item->folder = NULL;
    }

  /* free all the invisible children */
  if (item->invisible_children != NULL)
    {
      g_slist_free_full (item->invisible_children, g_object_unref);
      item->invisible_children = NULL;
    }

  /* disconnect from the file */
  if (G_LIKELY (item->file != NULL))
    {
      /* unwatch the trash */
      if (thunar_file_is_trash (item->file))
        thunar_file_unwatch (item->file);

      /* release and reset the file */
      g_object_unref (G_OBJECT (item->file));
      item->file = NULL;
    }

  /* release the item */
  g_slice_free (ThunarListModelItem, item);
}



static void
thunar_list_model_item_load_folder (ThunarListModelItem *item)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (item->file));

  /* schedule the "load" idle source (if not already done) */
  if (G_LIKELY (item->load_idle_id == 0 && item->folder == NULL))
    {
      item->load_idle_id = g_idle_add_full (G_PRIORITY_HIGH, thunar_list_model_item_load_idle,
                                            item, thunar_list_model_item_load_idle_destroy);
    }
}



static void
thunar_list_model_item_files_added (ThunarListModelItem *item,
                                    GList               *files,
                                    ThunarFolder        *folder)
{
  ThunarListModel     *model = THUNAR_LIST_MODEL (item->model);
  ThunarFile          *file;
  GNode               *node = NULL;
  GList               *lp;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);


  /* process all specified files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);

      /* if this file should be visible */
      if (!model->show_hidden && thunar_file_is_hidden (file))
        {
          /* file is invisible, insert it in the invisible list and continue */
          item->invisible_children = g_slist_prepend (item->invisible_children,
                                                      g_object_ref (G_OBJECT (file)));
          continue;
        }

      /* lookup the node for the item (on-demand) */
      if (G_UNLIKELY (node == NULL))
        node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
      _thunar_return_if_fail (node != NULL);

      thunar_list_model_add_child (model, node, file);
    }

  /* sort the folders if any new ones were added */
  if (G_LIKELY (node != NULL))
    thunar_list_model_sort (model, node);

  g_object_notify_by_pspec (G_OBJECT (model), list_model_props[PROP_NUM_FILES]);
}



static void
thunar_list_model_item_files_removed (ThunarListModelItem *item,
                                      GList               *files,
                                      ThunarFolder        *folder)
{
  ThunarListModel *model = item->model;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  GNode           *child_node;
  GNode           *node;
  GList           *lp;
  GSList          *inv_link;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);

  /* determine the node for the folder */
  node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
  _thunar_return_if_fail (node != NULL);

  /* check if the node has any visible children */
  if (G_LIKELY (node->children != NULL))
    {
      /* process all files */
      for (lp = files; lp != NULL; lp = lp->next)
        {
          /* find the child node for the file */
          for (child_node = g_node_first_child (node); child_node != NULL; child_node = g_node_next_sibling (child_node))
            if (child_node->data != NULL && THUNAR_LIST_MODEL_ITEM (child_node->data)->file == lp->data)
              break;

          /* drop the child node (and all descendant nodes) from the model */
          if (G_LIKELY (child_node != NULL))
            g_node_traverse (child_node, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_list_model_node_traverse_remove, model);
        }

      /* check if all children of the node where dropped */
      if (G_UNLIKELY (node->children == NULL))
        {
          /* determine the iterator for the folder node */
          GTK_TREE_ITER_INIT (iter, model->stamp, node);

          /* emit "row-has-child-toggled" for the folder node */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }

  /* we also need to release all the invisible folders */
  if (item->invisible_children != NULL)
    {
      for (lp = files; lp != NULL; lp = lp->next)
        {
          /* find the file in the hidden list */
          inv_link = g_slist_find (item->invisible_children, lp->data);
          if (inv_link != NULL)
            {
              /* release the file */
              g_object_unref (G_OBJECT (lp->data));

              /* remove from the list */
              item->invisible_children = g_slist_delete_link (item->invisible_children, inv_link);
            }
        }
    }

  g_object_notify_by_pspec (G_OBJECT (model), list_model_props[PROP_NUM_FILES]);
}



static void
thunar_list_model_item_notify_loading (ThunarListModelItem *item,
                                       GParamSpec          *pspec,
                                       ThunarFolder        *folder)
{
  GNode *node;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (item->model));

  /* be sure to drop the dummy child node once the folder is loaded */
  if (G_LIKELY (!thunar_folder_get_loading (folder)))
    {
      /* lookup the node for the item... */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
      _thunar_return_if_fail (node != NULL);

      /* ...and drop the dummy for the node */
      if (G_NODE_HAS_DUMMY (node))
        thunar_list_model_node_drop_dummy (node, item->model);
    }
}



static gboolean
thunar_list_model_item_load_idle (gpointer user_data)
{
  ThunarListModelItem *item = user_data;
  GList               *files;
#ifndef NDEBUG
  GNode               *node;
#endif

  _thunar_return_val_if_fail (item->folder == NULL, FALSE);

#ifndef NDEBUG
      /* find the node in the tree */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

      /* debug check to make sure the node is empty or contains a dummy node.
       * if this is not true, the node already contains sub folders which means
       * something went wrong. */
      _thunar_return_val_if_fail (node->children == NULL || G_NODE_HAS_DUMMY (node), FALSE);
#endif

THUNAR_THREADS_ENTER

  /* verify that we have a file */
  if (G_LIKELY (item->file != NULL) && thunar_file_is_directory (item->file))
    {
      /* open the folder for the item */
      item->folder = thunar_folder_get_for_file (item->file);
      if (G_LIKELY (item->folder != NULL))
        {
          /* connect signals */
          g_signal_connect_swapped (G_OBJECT (item->folder), "files-added", G_CALLBACK (thunar_list_model_item_files_added), item);
          g_signal_connect_swapped (G_OBJECT (item->folder), "files-removed", G_CALLBACK (thunar_list_model_item_files_removed), item);
          g_signal_connect_swapped (G_OBJECT (item->folder), "notify::loading", G_CALLBACK (thunar_list_model_item_notify_loading), item);

          /* load the initial set of files (if any) */
          files = thunar_folder_get_files (item->folder);
          if (G_UNLIKELY (files != NULL))
            thunar_list_model_item_files_added (item, files, item->folder);

          /* notify for "loading" if already loaded */
          if (!thunar_folder_get_loading (item->folder))
            g_object_notify (G_OBJECT (item->folder), "loading");
        }
    }

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_list_model_item_load_idle_destroy (gpointer user_data)
{
  THUNAR_LIST_MODEL_ITEM (user_data)->load_idle_id = 0;
}



static void
thunar_list_model_node_insert_dummy (GNode           *parent,
                                     ThunarListModel *model)
{
  GNode       *node;
  GtkTreeIter  iter;
  GtkTreePath *path;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (g_node_n_children (parent) == 0);

  /* add the dummy node */
  node = g_node_append_data (parent, NULL);

  /* determine the iterator for the dummy node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* tell the view about the dummy node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}



static void
thunar_list_model_node_drop_dummy (GNode           *node,
                                   ThunarListModel *model)
{
  GtkTreePath *path;
  GtkTreeIter  iter;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (G_NODE_HAS_DUMMY (node) && g_node_n_children (node) == 1);

  /* determine the iterator for the dummy */
  GTK_TREE_ITER_INIT (iter, model->stamp, node->children);

  /* determine the path for the iterator */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  if (G_LIKELY (path != NULL))
    {
      /* notify the view */
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      /* drop the dummy from the model */
      g_node_destroy (node->children);

      /* determine the iter to the parent node */
      GTK_TREE_ITER_INIT (iter, model->stamp, node);

      /* determine the path to the parent node */
      gtk_tree_path_up (path);

      /* emit a "row-has-child-toggled" for the parent */
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);

      /* release the path */
      gtk_tree_path_free (path);
    }
}



static gboolean
thunar_list_model_node_traverse_cleanup (GNode    *node,
                                         gpointer  user_data)
{
  ThunarListModelItem *item = node->data;
  ThunarListModel     *model = THUNAR_LIST_MODEL (user_data);

  if (item && item->folder != NULL && item->ref_count == 0)
    {
      /* disconnect from the folder */
      g_signal_handlers_disconnect_matched (G_OBJECT (item->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, item);
      g_object_unref (G_OBJECT (item->folder));
      item->folder = NULL;

      /* remove all the children of the node */
      while (node->children)
        g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                         thunar_list_model_node_traverse_remove, model);

      /* insert a dummy node */
      thunar_list_model_node_insert_dummy (node, model);
    }

  return FALSE;
}



static gboolean
thunar_list_model_node_traverse_changed (GNode   *node,
                                         gpointer user_data)
{
  ThunarListModel     *model;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  ThunarFile          *file = THUNAR_FILE (user_data);
  ThunarListModelItem *item = THUNAR_LIST_MODEL_ITEM (node->data);

  /* check if the node's file is the file that changed */
  if (G_UNLIKELY (item != NULL && item->file == file))
    {
      /* determine the tree model from the item */
      model = THUNAR_LIST_MODEL_ITEM (node->data)->model;

      /* determine the iterator for the node */
      GTK_TREE_ITER_INIT (iter, model->stamp, node);

      /* check if the changed node is not one of the root nodes */
      if (G_LIKELY (node->parent != model->root))
        {
          /* need to re-sort as the name of the file may have changed */
          thunar_list_model_sort (model, node->parent);
        }

      /* determine the path for the node */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      if (G_LIKELY (path != NULL))
        {
          /* emit "row-changed" */
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }

      /* stop traversing */
      return TRUE;
    }

  /* continue traversing */
  return FALSE;
}



static gboolean
thunar_list_model_node_traverse_remove (GNode   *node,
                                        gpointer user_data)
{
  ThunarListModel *model = THUNAR_LIST_MODEL (user_data);
  GtkTreeIter      iter;
  GtkTreePath     *path;

  _thunar_return_val_if_fail (node->children == NULL, FALSE);

  /* determine the iterator for the node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* determine the path for the node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  if (G_LIKELY (path != NULL))
    {
      /* emit a "row-deleted" */
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      /* release the item for the node */
      thunar_list_model_node_traverse_free (node, user_data);

      /* remove the node from the tree */
      g_node_destroy (node);

      /* release the path */
      gtk_tree_path_free (path);
    }

  return FALSE;
}



static gboolean
thunar_list_model_node_traverse_sort (GNode   *node,
                                      gpointer user_data)
{
  ThunarListModel *model = THUNAR_LIST_MODEL (user_data);

  thunar_list_model_sort (model, node);

  return FALSE;
}



static gboolean
thunar_list_model_node_traverse_free (GNode   *node,
                                      gpointer user_data)
{
  if (G_LIKELY (node->data != NULL))
    thunar_list_model_item_free (node->data);
  return FALSE;
}



static gboolean
thunar_list_model_node_traverse_visible (GNode    *node,
                                         gpointer  user_data)
{
  ThunarListModelItem *item = node->data;
  ThunarListModel     *model = THUNAR_LIST_MODEL (user_data);
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GNode               *child_node;
  GSList              *lp, *lnext;
  ThunarListModelItem *parent, *child;
  ThunarFile          *file;

  _thunar_return_val_if_fail (item == NULL || item->file == NULL || THUNAR_IS_FILE (item->file), FALSE);

  if (G_LIKELY (item != NULL && item->file != NULL))
    {
      /* check if this file should be visible in the treeview */
      if (!model->show_hidden && thunar_file_is_hidden (item->file))
        {
          /* delete all the children of the node */
          while (node->children)
            g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                             thunar_list_model_node_traverse_remove, model);

          /* generate an iterator for the item */
          GTK_TREE_ITER_INIT (iter, model->stamp, node);

          /* remove this item from the tree */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
          gtk_tree_path_free (path);

          /* insert the file in the invisible list of the parent */
          parent = node->parent->data;
          if (G_LIKELY (parent))
            parent->invisible_children = g_slist_prepend (parent->invisible_children,
                                                          g_object_ref (G_OBJECT (item->file)));

          /* free the item and destroy the node */
          thunar_list_model_item_free (item);
          g_node_destroy (node);
        }
      else if (!G_NODE_HAS_DUMMY (node))
        {
          /* this node should be visible. check if the node has invisible
           * files that should be visible too */
          for (lp = item->invisible_children, child_node = NULL; lp != NULL; lp = lnext)
            {
              lnext = lp->next;
              file = THUNAR_FILE (lp->data);

              _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

              if (model->show_hidden || !thunar_file_is_hidden (file))
                {
                  /* allocate a new item for the file */
                  child = thunar_list_model_item_new_with_file (model, file);

                  /* insert a new node for the child */
                  child_node = g_node_append_data (node, child);

                  /* determine the tree iter for the child */
                  GTK_TREE_ITER_INIT (iter, model->stamp, child_node);

                  /* emit a "row-inserted" for the new node */
                  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
                  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
                  gtk_tree_path_free (path);

                  /* release the reference on the file hold by the invisible list */
                  g_object_unref (G_OBJECT (file));

                  /* delete the file in the list */
                  item->invisible_children = g_slist_delete_link (item->invisible_children, lp);

                  /* insert dummy */
                  thunar_list_model_node_insert_dummy (child_node, model);
                }
            }

          /* sort this node if one of new children have been added */
          if (child_node != NULL)
            thunar_list_model_sort (model, node);
        }
    }

  return FALSE;
}



/**
 * thunar_list_model_refilter:
 * @model : a #ThunarTreeModel.
 *
 * Walks all the folders in the #ThunarTreeModel and updates their
 * visibility.
 **/
void
thunar_list_model_refilter (ThunarListModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));

  /* traverse all nodes to update their visibility */
  g_node_traverse (model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   thunar_list_model_node_traverse_visible, model);
}



/**
 * thunar_list_model_cleanup:
 * @model : a #ThunarTreeModel.
 *
 * Walks all the folders in the #ThunarTreeModel and release them when
 * they are unused by the treeview.
 **/
void
thunar_list_model_cleanup (ThunarListModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));

  /* schedule an idle cleanup, if not already done */
  if (model->cleanup_idle_id == 0)
    {
      model->cleanup_idle_id = g_timeout_add_full (G_PRIORITY_LOW, 500, thunar_list_model_cleanup_idle,
                                                   model, thunar_list_model_cleanup_idle_destroy);
    }
}



/**
 * thunar_list_model_node_has_dummy:
 * @model : a #ThunarTreeModel.
 * @node : GNode to check
 *
 * Checks if node is a dummy node ( if it only has a dummy item )
 *
 * Return value: %TRUE if @node has a dummy item
 **/
gboolean
thunar_list_model_node_has_dummy (ThunarListModel *model,
                                  GNode           *node)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), TRUE);
  return G_NODE_HAS_DUMMY(node);
}



/**
 * thunar_list_model_add_child:
 * @model : a #ThunarTreeModel.
 * @node : GNode to add a child
 * @file : #ThunarFile to be added
 *
 * Creates a new #ThunarTreeModelItem as a child of @node and stores a reference to the passed @file
 * Automatically creates/removes dummy items if required
 **/
void
thunar_list_model_add_child (ThunarListModel *model,
                             GNode           *node,
                             ThunarFile      *file)
{
  ThunarListModelItem *child_item;
  GNode               *child_node;
  GtkTreeIter          child_iter;
  GtkTreePath         *child_path;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* allocate a new item for the file */
  child_item = thunar_list_model_item_new_with_file (model, file);

  /* check if the node has only the dummy child */
  if (G_UNLIKELY (G_NODE_HAS_DUMMY (node)))
    {
      /* replace the dummy node with the new node */
      child_node = g_node_first_child (node);
      child_node->data = child_item;

      /* determine the tree iter for the child */
      GTK_TREE_ITER_INIT (child_iter, model->stamp, child_node);

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
      GTK_TREE_ITER_INIT (child_iter, model->stamp, child_node);

      /* emit a "row-inserted" for the new node */
      child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), child_path, &child_iter);
      gtk_tree_path_free (child_path);
    }

  /* add a dummy to the new child */
  if (thunar_file_is_directory (file))
    thunar_list_model_node_insert_dummy (child_node, model);
}



static gint
sort_by_date         (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive,
                      gint              type)
{
  guint64 date_a;
  guint64 date_b;

  date_a = thunar_file_get_date (a, type);
  date_b = thunar_file_get_date (b, type);

  if (date_a < date_b)
    return -1;
  else if (date_a > date_b)
    return 1;

  return thunar_file_compare_by_name (a, b, case_sensitive);
}



static gint
sort_by_date_created (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_CREATED);
}



static gint
sort_by_date_accessed (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_ACCESSED);
}



static gint
sort_by_date_modified (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_MODIFIED);
}



static gint
sort_by_date_deleted (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_DATE_DELETED);
}



static gint
sort_by_recency      (const ThunarFile *a,
                      const ThunarFile *b,
                      gboolean          case_sensitive)
{
  return sort_by_date (a, b, case_sensitive, THUNAR_FILE_RECENCY);
}



static gint
sort_by_location (const ThunarFile *a,
                  const ThunarFile *b,
                  gboolean          case_sensitive)
{
  gchar *uri_a;
  gchar *uri_b;
  gchar *location_a;
  gchar *location_b;
  gint   result;

  uri_a = thunar_file_dup_uri (a);
  uri_b = thunar_file_dup_uri (b);

  location_a = g_path_get_dirname (uri_a);
  location_b = g_path_get_dirname (uri_b);

  result = strcasecmp (location_a, location_b);

  g_free (uri_a);
  g_free (uri_b);
  g_free (location_a);
  g_free (location_b);

  return result;
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
sort_by_size_in_bytes (const ThunarFile *a,
                       const ThunarFile *b,
                       gboolean          case_sensitive)
{
  return sort_by_size (a, b, case_sensitive);
}



static gint
sort_by_type (const ThunarFile *a,
              const ThunarFile *b,
              gboolean          case_sensitive)
{
  gchar       *description_a = NULL;
  gchar       *description_b = NULL;
  gint         result;

  /* we alter the description of symlinks here because they are
   * displayed as "... (link)" in the detailed list view as well */

  /* fetch the content type description for @file(s) a & b */
  description_a = thunar_file_get_content_type_desc (THUNAR_FILE (a));
  description_b = thunar_file_get_content_type_desc (THUNAR_FILE (b));

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
      g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_list_model_node_traverse_sort, store);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_CASE_SENSITIVE]);
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
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



/**
 * thunar_list_model_get_date_custom_style:
 * @store : a valid #ThunarListModel object.
 *
 * Return value: the style used to format customdates in the given @store.
 **/
static const char*
thunar_list_model_get_date_custom_style (ThunarListModel *store)
{
  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  return store->date_custom_style;
}



/**
 * thunar_list_model_set_date_custom_style:
 * @store             : a valid #ThunarListModel object.
 * @date_custom_style : the style that should be used to format
 *                      custom dates in the @store.
 *
 * Changes the style used to format custom dates in @store to the specified @date_custom_style.
 **/
static void
thunar_list_model_set_date_custom_style (ThunarListModel *store,
                                         const char      *date_custom_style)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* check if we have a new setting */
  if (g_strcmp0 (store->date_custom_style, date_custom_style) != 0)
    {
      /* apply the new setting */
      g_free (store->date_custom_style);
      store->date_custom_style = g_strdup (date_custom_style);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_DATE_CUSTOM_STYLE]);

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



ThunarJob*
thunar_list_model_get_job (ThunarListModel  *store)
{
  return store->recursive_search_job;
}



void
thunar_list_model_set_job (ThunarListModel  *store,
                           ThunarJob        *job)
{
  store->recursive_search_job = job;
}



static gboolean
add_search_files (gpointer user_data)
{
  ThunarListModel *model = user_data;

  g_mutex_lock (&model->mutex_files_to_add);

  thunar_list_model_folder_files_added (model->folder, model->files_to_add, model);
  g_list_free (model->files_to_add);
  model->files_to_add = NULL;

  g_mutex_unlock (&model->mutex_files_to_add);

  return TRUE;
}



static gboolean
_thunar_job_search_directory (ThunarJob  *job,
                               GArray     *param_values,
                               GError    **error)
{
  ThunarListModel            *model;
  ThunarFile                 *directory;
  const char                 *search_query_c;
  gchar                     **search_query_c_terms;
  GRegex                     *whitespace_regex;
  ThunarPreferences          *preferences;
  gboolean                    is_source_device_local;
  ThunarRecursiveSearchMode   mode;
  enum ThunarListModelSearch  search_type;
  gboolean                    show_hidden;

  search_type = THUNAR_LIST_MODEL_SEARCH_NON_RECURSIVE;

  /* grab a reference on the preferences */
  preferences = thunar_preferences_get ();

  /* determine the current recursive search mode */
  g_object_get (G_OBJECT (preferences), "misc-recursive-search", &mode, NULL);
  g_object_get (G_OBJECT (preferences), "last-show-hidden", &show_hidden, NULL);

  g_object_unref (preferences);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  model = g_value_get_object (&g_array_index (param_values, GValue, 0));
  search_query_c = g_value_get_string (&g_array_index (param_values, GValue, 1));
  directory = g_value_get_object (&g_array_index (param_values, GValue, 2));

  whitespace_regex = g_regex_new ("\\s+", 0, 0, error);
  if (whitespace_regex == NULL)
    return FALSE;
  search_query_c_terms = g_regex_split (whitespace_regex, search_query_c, 0);
  g_regex_unref (whitespace_regex);

  is_source_device_local = thunar_g_file_is_on_local_device (thunar_file_get_file (directory));
  if (mode == THUNAR_RECURSIVE_SEARCH_ALWAYS || (mode == THUNAR_RECURSIVE_SEARCH_LOCAL && is_source_device_local))
    search_type = THUNAR_LIST_MODEL_SEARCH_RECURSIVE;

  thunar_list_model_search_folder (model, job, thunar_file_dup_uri (directory), search_query_c_terms, search_type, show_hidden);

  g_strfreev (search_query_c_terms);

  return TRUE;
}



static ThunarJob*
thunar_list_model_job_search_directory (ThunarListModel *model,
                                        const gchar     *search_query_c,
                                        ThunarFile      *directory)
{
  return thunar_simple_job_new (_thunar_job_search_directory, 3,
                                THUNAR_TYPE_LIST_MODEL, model,
                                G_TYPE_STRING,          search_query_c,
                                THUNAR_TYPE_FILE,       directory);
}



static void
thunar_list_model_cancel_search_job (ThunarListModel *model)
{
  /* cancel the ongoing search if there is one */
  if (model->recursive_search_job)
    {
      exo_job_cancel (EXO_JOB (model->recursive_search_job));

      g_signal_handlers_disconnect_matched (model->recursive_search_job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
      g_object_unref (model->recursive_search_job);
      model->recursive_search_job = NULL;
    }
}



static void
search_error (ThunarJob *job)
{
  g_error ("Error while searching recursively");
}



static void
search_finished (ThunarJob       *job,
                 ThunarListModel *store)
{
  if (store->recursive_search_job)
    {
      g_signal_handlers_disconnect_matched (store->recursive_search_job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (store->recursive_search_job);
      store->recursive_search_job = NULL;
    }

  if (store->update_search_results_timeout_id > 0)
    {
      add_search_files (store);
      g_source_remove (store->update_search_results_timeout_id);
      store->update_search_results_timeout_id = 0;
    }

  thunar_g_list_free_full (store->files_to_add);
  store->files_to_add = NULL;

  g_signal_emit_by_name (store, "search-done");
}



static void
thunar_list_model_search_folder (ThunarListModel           *model,
                                 ThunarJob                 *job,
                                 gchar                     *uri,
                                 gchar                    **search_query_c_terms,
                                 enum ThunarListModelSearch search_type,
                                 gboolean                   show_hidden)
{
  GCancellable    *cancellable;
  GFileEnumerator *enumerator;
  GFile           *directory;
  GList           *files_found = NULL; /* contains the matching files in this folder only */
  const gchar     *namespace;
  const gchar     *display_name;
  gchar           *display_name_c; /* converted to ignore case */
  gboolean         matched;

  cancellable = exo_job_get_cancellable (EXO_JOB (job));
  directory = g_file_new_for_uri (uri);
  g_free (uri);
  namespace = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
              G_FILE_ATTRIBUTE_STANDARD_TARGET_URI ","
              G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
              G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","
              G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
              G_FILE_ATTRIBUTE_STANDARD_NAME ", recent::*";

  /* The directory enumerator MUST NOT follow symlinks itself, meaning that any symlinks that
   * g_file_enumerator_next_file() emits are the actual symlink entries. This prevents one
   * possible source of infinitely deep recursion.
   *
   * There is otherwise no special handling of entries in the folder which are symlinks,
   * which allows them to appear in the search results. */
  enumerator = g_file_enumerate_children (directory, namespace, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
  if (enumerator == NULL)
    return;

  /* go through every file in the folder and check if it matches */
  while (exo_job_is_cancelled (EXO_JOB (job)) == FALSE)
    {
      GFile     *file;
      GFileInfo *info;
      GFileType  type;

      /* get GFile and GFileInfo */
      info = g_file_enumerator_next_file (enumerator, cancellable, NULL);
      if (G_UNLIKELY (info == NULL))
        break;

      if (g_file_has_uri_scheme (directory, "recent"))
        {
          file = g_file_new_for_uri (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));
          g_object_unref (info);
          info = g_file_query_info (file, namespace, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
          if (G_UNLIKELY (info == NULL))
            break;
        }
      else
        file = g_file_get_child (directory, g_file_info_get_name (info));

      /* respect last-show-hidden */
      if (show_hidden == FALSE)
        {
          /* same logic as thunar_file_is_hidden() */
          if (g_file_info_get_is_hidden (info) || g_file_info_get_is_backup (info))
            {
              g_object_unref (file);
              g_object_unref (info);
              continue;
            }
        }

      type = g_file_info_get_file_type (info);

      /* handle directories */
      if (type == G_FILE_TYPE_DIRECTORY && search_type == THUNAR_LIST_MODEL_SEARCH_RECURSIVE)
        {
          thunar_list_model_search_folder (model, job, g_file_get_uri (file), search_query_c_terms, search_type, show_hidden);
        }

      /* prepare entry display name */
      display_name = g_file_info_get_display_name (info);
      display_name_c = thunar_g_utf8_normalize_for_search (display_name, TRUE, TRUE);

      /* search for all substrings */
      matched = TRUE;
      for (gint i = 0; search_query_c_terms[i]; i++)
        {
          if (g_strrstr (display_name_c, search_query_c_terms[i]) == NULL)
            {
              matched = FALSE;
              break;
            }
        }
      if (matched)
        files_found = g_list_prepend (files_found, thunar_file_get (file, NULL));

      /* free memory */
      g_free (display_name_c);
      g_object_unref (file);
      g_object_unref (info);
    }

  g_object_unref (enumerator);
  g_object_unref (directory);

  if (exo_job_is_cancelled (EXO_JOB (job)))
    return;

  g_mutex_lock (&model->mutex_files_to_add);
  model->files_to_add = g_list_concat (model->files_to_add, files_found);
  g_mutex_unlock (&model->mutex_files_to_add);
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
 * @store                       : a valid #ThunarListModel.
 * @folder                      : a #ThunarFolder or %NULL.
 * @search_query                : a #string or %NULL.
 **/
void
thunar_list_model_set_folder (ThunarListModel *store,
                              ThunarFolder    *folder,
                              gchar           *search_query)
{
  GList *files = NULL;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));
  _thunar_return_if_fail (folder == NULL || THUNAR_IS_FOLDER (folder));

  /* unlink from the previously active folder (if any) */
  if (G_LIKELY (store->folder != NULL))
    {
      thunar_list_model_cancel_search_job (store);

      if (store->update_search_results_timeout_id > 0)
        {
          g_source_remove (store->update_search_results_timeout_id);
          store->update_search_results_timeout_id = 0;
        }
      thunar_g_list_free_full (store->files_to_add);
      store->files_to_add = NULL;

      if (store->cleanup_idle_id != 0)
        g_source_remove (store->cleanup_idle_id);

      /* block the file monitor */
      g_signal_handlers_block_by_func (store->file_monitor, thunar_list_model_file_changed, store);

      /* remove existing entries */
      while (store->root->children)
        g_node_traverse (store->root->children, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_list_model_node_traverse_remove, store);
      g_node_destroy (store->root);
      store->root = NULL;

      /* unblock the file monitor */
      g_signal_handlers_unblock_by_func (store->file_monitor, thunar_list_model_file_changed, store);

      g_list_free_full (store->hidden, g_object_unref);
      store->hidden = NULL;

      /* unregister signals and drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (store->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);
      g_object_unref (G_OBJECT (store->folder));
    }

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

      store->root = g_node_new (NULL);

      /* get the already loaded files or search for files matching the search_query
       * don't start searching if the query is empty, that would be a waste of resources
       */
      if (search_query == NULL || strlen (search_query) == 0)
        {
          files = thunar_folder_get_files (folder);
        }
      else
        {
          gchar *search_query_c;  /* normalized */

          search_query_c = thunar_g_utf8_normalize_for_search (search_query, TRUE, TRUE);
          files = NULL;

          /* search the current folder
           * start a new recursive_search_job */
          store->recursive_search_job = thunar_list_model_job_search_directory (store, search_query_c, thunar_folder_get_corresponding_file (folder));
          exo_job_launch (EXO_JOB (store->recursive_search_job));

          g_signal_connect (store->recursive_search_job, "error", G_CALLBACK (search_error), NULL);
          g_signal_connect (store->recursive_search_job, "finished", G_CALLBACK (search_finished), store);

          /* add new results to the model every X ms */
          store->update_search_results_timeout_id = g_timeout_add (500, add_search_files, store);

          g_free (search_query_c);
        }

      if (files != NULL)
        thunar_list_model_folder_files_added (folder, files, store);

      /* connect signals to the new folder */
      /* TODO: Are these required ? */
      g_signal_connect (G_OBJECT (store->folder), "destroy", G_CALLBACK (thunar_list_model_folder_destroy), store);
      g_signal_connect (G_OBJECT (store->folder), "error", G_CALLBACK (thunar_list_model_folder_error), store);
      g_signal_connect (G_OBJECT (store->folder), "files-added", G_CALLBACK (thunar_list_model_folder_files_added), store);
      g_signal_connect (G_OBJECT (store->folder), "files-removed", G_CALLBACK (thunar_list_model_folder_files_removed), store);
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
  g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_list_model_node_traverse_sort, store);

  /* emit a "changed" signal for each row, so the display is
     reloaded with the new folders first setting */
  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
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
  ThunarListModelItem *item;
  GNode               *node;
  GtkTreeIter          iter;
  GtkTreePath         *path;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (store));

  /* normalize the value */
  show_hidden = !!show_hidden;

  /* check if we have a new setting */
  if (store->show_hidden != show_hidden)
    {
      /* apply the new setting */
      store->show_hidden = show_hidden;

      /* update the model */
      thunar_list_model_refilter (store);

      if (show_hidden)
        {
          for (GList *lp = store->hidden; lp != NULL; lp = lp->next)
            {
              item = thunar_list_model_item_new_with_file (store, lp->data);
              node = g_node_append_data (store->root, item);
              GTK_TREE_ITER_INIT(iter, store->stamp, node);
              path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
              gtk_tree_model_row_inserted(GTK_TREE_MODEL (store), path, &iter);
              gtk_tree_path_free (path);
            }
          /* sort the view */
          g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_list_model_node_traverse_sort, store);
        }

      /* notify listeners */
      g_object_notify (G_OBJECT (store), "show-hidden");
    }
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
      g_node_traverse (store->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_list_model_node_traverse_sort, store);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (store), list_model_props[PROP_FILE_SIZE_BINARY]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new binary file size setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                              (GtkTreeModelForeachFunc) (void (*)(void)) gtk_tree_model_row_changed,
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
  GNode *node;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (iter->stamp == store->stamp, NULL);

  node = iter->user_data;
  if (node == NULL || node->data == NULL)
    return NULL;
  return g_object_ref (THUNAR_LIST_MODEL_ITEM (node->data)->file);
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
  return g_node_n_children (store->root);
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
  GtkTreeIter  iter;
  GList       *paths = NULL;
  ThunarFile  *file;
  GList       *item;
  gboolean     has_next;

  has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(store), &iter);
  while (has_next)
    {
      file = THUNAR_LIST_MODEL_ITEM (G_NODE (iter.user_data)->data)->file;
      item = g_list_find (files, file);
      if (item != NULL)
        paths = g_list_prepend (paths, gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter));
      has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL(store), &iter);
    }

  return paths;
}



/**
 * thunar_list_model_get_paths_for_pattern:
 * @store          : a #ThunarListModel instance.
 * @pattern        : the pattern to match.
 * @case_sensitive    : %TRUE to use case sensitive search.
 * @match_diacritics : %TRUE to use case sensitive search.
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
                                         const gchar     *pattern,
                                         gboolean         case_sensitive,
                                         gboolean         match_diacritics)
{
  GPatternSpec  *pspec;
  gchar         *normalized_pattern;
  GList         *paths = NULL;
  ThunarFile    *file;
  const gchar   *display_name;
  gchar         *normalized_display_name;
  gboolean       name_matched;
  gint           i = 0;
  GNode         *node;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (pattern, -1, NULL), NULL);

  /* compile the pattern */
  normalized_pattern = thunar_g_utf8_normalize_for_search (pattern, !match_diacritics, !case_sensitive);
  pspec = g_pattern_spec_new (normalized_pattern);
  g_free (normalized_pattern);

  node = g_node_first_child (store->root);

  /* find all rows that match the given pattern */
  while (node != NULL)
    {
      file = THUNAR_LIST_MODEL_ITEM (node->data)->file;
      display_name = thunar_file_get_display_name (file);

      normalized_display_name = thunar_g_utf8_normalize_for_search (display_name, !match_diacritics, !case_sensitive);
      name_matched = g_pattern_match_string (pspec, normalized_display_name);
      g_free (normalized_display_name);

      if (name_matched)
        {
          _thunar_assert (i == g_node_child_position (store->root, node));
          paths = g_list_prepend (paths, gtk_tree_path_new_from_indices (i, -1));
        }

      node = g_node_next_sibling (node);
      i++;
    }

  /* release the pattern */
  g_pattern_spec_free (pspec);

  return paths;
}



/**
 * thunar_list_model_get_statusbar_text_for_files:
 * @files                        : list of files for which a text is requested
 * @show_file_size_binary_format : weather the file size should be displayed in binary format
 *
 * Generates the statusbar text for the given @files.
 *
 * The caller is reponsible to free the returned text using
 * g_free() when it's no longer needed.
 *
 * Return value: the statusbar text for @store with the given @files.
 **/
static gchar*
thunar_list_model_get_statusbar_text_for_files (ThunarListModel *store,
                                                GList           *files,
                                                gboolean         show_file_size_binary_format)
{
  guint64            size_summary       = 0;
  gint               folder_count       = 0;
  gint               non_folder_count   = 0;
  GList             *lp;
  GList             *text_list          = NULL;
  gchar             *size_string        = NULL;
  gchar             *temp_string        = NULL;
  gchar             *folder_text        = NULL;
  gchar             *non_folder_text    = NULL;
  ThunarPreferences *preferences;
  guint              active;
  guint64            last_modified_date = 0;
  guint64            temp_last_modified_date;
  ThunarFile        *last_modified_file = NULL;
  gboolean           show_size, show_size_in_bytes, show_last_modified;

  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-status-bar-active-info", &active, NULL);
  show_size = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE);
  show_size_in_bytes = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES);
  show_last_modified = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_LAST_MODIFIED);
  g_object_unref (preferences);

  /* analyze files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      if (thunar_file_is_directory (lp->data))
        {
          folder_count++;
        }
      else
        {
          non_folder_count++;
          if (thunar_file_is_regular (lp->data))
            size_summary += thunar_file_get_size (lp->data);
        }
      temp_last_modified_date = thunar_file_get_date (lp->data, THUNAR_FILE_DATE_MODIFIED);
      if (last_modified_date <= temp_last_modified_date)
        {
          last_modified_date = temp_last_modified_date;
          last_modified_file = lp->data;
        }
    }

  if (non_folder_count > 0)
    {
      if (show_size == TRUE)
        {
          if (show_size_in_bytes == TRUE)
            {
              size_string = g_format_size_full (size_summary, G_FORMAT_SIZE_LONG_FORMAT
                                                              | (show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS
                                                                                              : G_FORMAT_SIZE_DEFAULT));
            }
          else
            {
              size_string = g_format_size_full (size_summary, show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS
                                                                                           : G_FORMAT_SIZE_DEFAULT);
            }
          non_folder_text = g_strdup_printf (ngettext ("%d file: %s",
                                                       "%d files: %s",
                                                       non_folder_count), non_folder_count, size_string);
          g_free (size_string);
        }
      else
        non_folder_text = g_strdup_printf (ngettext ("%d file", "%d files", non_folder_count), non_folder_count);
    }

  if (folder_count > 0)
    {
      folder_text = g_strdup_printf (ngettext ("%d folder",
                                               "%d folders",
                                               folder_count), folder_count);
    }

  if (non_folder_text == NULL && folder_text == NULL)
    text_list = g_list_append (text_list, g_strdup_printf (_("0 items")));
  if (folder_text != NULL)
    text_list = g_list_append (text_list, folder_text);
  if (non_folder_text != NULL)
    text_list = g_list_append (text_list, non_folder_text);

  if (show_last_modified && last_modified_file != NULL)
    {
      temp_string = thunar_file_get_date_string (last_modified_file, THUNAR_FILE_DATE_MODIFIED, store->date_style, store->date_custom_style);
      text_list = g_list_append (text_list, g_strdup_printf (_("Last Modified: %s"), temp_string));
      g_free (temp_string);
    }

  temp_string = thunar_util_strjoin_list (text_list, "  |  ");
  g_list_free_full (text_list, g_free);
  return temp_string;
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
  GList             *lp;
  GList             *text_list      = NULL;
  gchar             *temp_string    = NULL;
  gchar             *text           = "";
  gint               height;
  gint               width;
  ThunarPreferences *preferences;
  gboolean           show_image_size;
  gboolean           show_file_size_binary_format;
  GList             *relevant_files = NULL;
  guint              active;
  gboolean           show_size, show_size_in_bytes, show_filetype, show_display_name, show_last_modified;
  gpointer           item;

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (store), NULL);

  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-status-bar-active-info", &active, NULL);
  show_size = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE);
  show_size_in_bytes = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES);
  show_filetype = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_FILETYPE);
  show_display_name = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_DISPLAY_NAME);
  show_last_modified = thunar_status_bar_info_check_active (active, THUNAR_STATUS_BAR_INFO_LAST_MODIFIED);
  show_file_size_binary_format = thunar_list_model_get_file_size_binary(store);

  if (selected_items == NULL) /* nothing selected */
    {
      /* build a GList of all files */
      for (GNode *row = g_node_first_child (store->root); row != NULL; row = g_node_next_sibling (row))
        {
          relevant_files = g_list_append (relevant_files, row->data != NULL ? THUNAR_LIST_MODEL_ITEM (row->data)->file : NULL);
        }

      /* try to determine a file for the current folder */
      file = (store->folder != NULL) ? thunar_folder_get_corresponding_file (store->folder) : NULL;
      temp_string = thunar_list_model_get_statusbar_text_for_files (store, relevant_files, show_file_size_binary_format);
      text_list = g_list_append (text_list, temp_string);

      /* check if we can determine the amount of free space for the volume */
      if (G_LIKELY (file != NULL && thunar_g_file_get_free_space (thunar_file_get_file (file), &size, NULL)))
        {
          /* humanize the free space */
          gchar *size_string = g_format_size_full (size, show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
          temp_string = g_strdup_printf (_("Free space: %s"), size_string);
          text_list = g_list_append (text_list, temp_string);
          g_free (size_string);
        }

      g_list_free (relevant_files);
    }
  else if (selected_items->next == NULL) /* only one item selected */
    {
      /* resolve the iter for the single path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, selected_items->data);

      /* get the file for the given iter */
      item = G_NODE (iter.user_data)->data;
      file = (item != NULL) ? THUNAR_LIST_MODEL_ITEM (item)->file : NULL;
      if (file != NULL)
        {
          /* determine the content type of the file */
          content_type = thunar_file_get_content_type (file);

          if (show_display_name == TRUE)
            {
              temp_string = g_strdup_printf (_("\"%s\""), thunar_file_get_display_name (file));
              text_list = g_list_append (text_list, temp_string);
            }

          if (thunar_file_is_regular (file) || G_UNLIKELY (thunar_file_is_symlink (file)))
            {
              if (show_size == TRUE)
                {
                  if (show_size_in_bytes == TRUE)
                    temp_string = thunar_file_get_size_string_long (file, show_file_size_binary_format);
                  else
                    temp_string = thunar_file_get_size_string_formatted (file, show_file_size_binary_format);
                  text_list = g_list_append (text_list, temp_string);
                }
            }

          if (show_filetype == TRUE)
            {
              if (G_UNLIKELY (content_type != NULL && g_str_equal (content_type, "inode/symlink")))
                temp_string = g_strdup (_("broken link"));
              else if (G_UNLIKELY (thunar_file_is_symlink (file)))
                temp_string = g_strdup_printf (_("link to %s"), thunar_file_get_symlink_target (file));
              else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT))
                temp_string = g_strdup (_("shortcut"));
              else if (G_UNLIKELY (thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE))
                temp_string = g_strdup (_("mountable"));
              else
                {
                  gchar *description = g_content_type_get_description (content_type);
                  temp_string = g_strdup_printf (_("%s"), description);
                  g_free (description);
                }
              text_list = g_list_append (text_list, temp_string);
            }

          /* append the original path (if any) */
          original_path = thunar_file_get_original_path (file);
          if (G_UNLIKELY (original_path != NULL))
            {
              /* append the original path to the statusbar text */
              gchar *original_path_string = g_filename_display_name (original_path);
              temp_string = g_strdup_printf ("%s %s", _("Original Path:"), original_path_string);
              text_list = g_list_append (text_list, temp_string);
              g_free (original_path_string);
            }
          else if (thunar_file_is_local (file)
                  && thunar_file_is_regular (file)
                  && g_str_has_prefix (content_type, "image/")) /* bug #2913 */
            {
              /* check if the size should be visible in the statusbar, disabled by
              * default to avoid high i/o  */
              g_object_get (preferences, "misc-image-size-in-statusbar", &show_image_size, NULL);
              if (show_image_size)
                {
                  /* check if we can determine the dimension of this file (only for image files) */
                  gchar *file_path = g_file_get_path (thunar_file_get_file (file));
                  if (file_path != NULL && gdk_pixbuf_get_file_info (file_path, &width, &height) != NULL)
                    {
                      /* append the image dimensions to the statusbar text */
                      temp_string = g_strdup_printf ("%s %dx%d", _("Image Size:"), width, height);
                      text_list = g_list_append (text_list, temp_string);
                    }
                  g_free (file_path);
                }
            }

          if (show_last_modified)
            {
              gchar *date_string = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, store->date_style, store->date_custom_style);
              temp_string = g_strdup_printf (_("Last Modified: %s"), date_string);
              text_list = g_list_append (text_list, temp_string);
              g_free (date_string);
            }
        }
    }
  else /* more than one item selected */
    {
      gchar *selected_string;
      /* build GList of files from selection */
      for (lp = selected_items; lp != NULL; lp = lp->next)
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, lp->data);
          /* get the file for the given iter */
          item = G_NODE (iter.user_data)->data;
          if (item != NULL)
            {
              file = THUNAR_LIST_MODEL_ITEM (item)->file;
              if (file != NULL)
                relevant_files = g_list_append (relevant_files, file);
            }
        }
      selected_string = thunar_list_model_get_statusbar_text_for_files (store, relevant_files, show_file_size_binary_format);
      temp_string = g_strdup_printf (_("Selection: %s"), selected_string);
      text_list = g_list_append (text_list, temp_string);
      g_list_free (relevant_files);
      g_free (selected_string);
    }

  text = thunar_util_strjoin_list (text_list, "  |  ");
  g_list_free_full (text_list, g_free);
  g_object_unref (preferences);
  return text;
}
