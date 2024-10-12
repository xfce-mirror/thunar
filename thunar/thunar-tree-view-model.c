/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2023 Amrit Borah <elessar1802@gmail.com>
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



/*
 * The rough working of the Model -
 *
 * Each file/subdir/dir is a #_Node.
 * There are 2 types of #_Node - a file #_Node & a dummy Node.
 * A file #_Node is one which could also be a subdir. But the key
 * differentiating factor is that a file #_Node is always initialized with a file
 * while a dummy Node is initialed with NULL.
 * The dummy node's main purpose is to act as a child for a subdir
 * which has not been expanded in the view yet.
 * Whenever a #_Node is loaded, the node's file's children will be added as
 * child nodes. If any of these child nodes are non-empty directories then
 * they will additionally be given a dummy child node. When these subdirs
 * will be loaded (expanded in the view), the dummy child node will be replaced
 * by the actual children of the node.
 *
 * A Node's children are stored in sorted manner in a GSequence and additionally
 * a HashTable is used to quickly query and retrieve the required child.
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "thunar/thunar-application.h"
#include "thunar/thunar-file.h"
#include "thunar/thunar-gio-extensions.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-io-jobs.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-simple-job.h"
#include "thunar/thunar-tree-view-model.h"
#include "thunar/thunar-user.h"
#include "thunar/thunar-util.h"

#include <libxfce4util/libxfce4util.h>



/* A delay after which the children of the collapsed subdir will be
 * freed and replaced by a dummy node (cleanup); if the subdir is again
 * expanded before the delay elapses the scheduled cleanup will be cancelled */
#define CLEANUP_AFTER_COLLAPSE_DELAY 5000 /* in ms */

/* Defintions & typedefs */
typedef struct _Node Node;



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
  PROP_FOLDER_ITEM_COUNT,
  PROP_FILE_SIZE_BINARY,
  PROP_LOADING,
  N_PROPERTIES
};



static void
thunar_tree_view_model_drag_dest_init (GtkTreeDragDestIface *iface);
static void
thunar_tree_view_model_sortable_init (GtkTreeSortableIface *iface);
static void
thunar_tree_view_model_tree_model_init (GtkTreeModelIface *iface);
static void
thunar_tree_view_model_standard_view_model_init (ThunarStandardViewModelIface *iface);


/*************************************************
 * GObject Virtual Methods
 *************************************************/
static void
thunar_tree_view_model_dispose (GObject *object);
static void
thunar_tree_view_model_finalize (GObject *object);
static void
thunar_tree_view_model_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec);
static void
thunar_tree_view_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);


/*************************************************
 * GtkTreeDragDest Virtual Methods
 *************************************************/
static gboolean
thunar_tree_view_model_drag_data_received (GtkTreeDragDest  *dest,
                                           GtkTreePath      *path,
                                           GtkSelectionData *data);
static gboolean
thunar_tree_view_model_row_drop_possible (GtkTreeDragDest  *dest,
                                          GtkTreePath      *path,
                                          GtkSelectionData *data);


/*************************************************
 * GtkSortable Virtual Methods
 *************************************************/
static gboolean
thunar_tree_view_model_get_sort_column_id (GtkTreeSortable *sortable,
                                           gint            *sort_column_id,
                                           GtkSortType     *order);
static void
thunar_tree_view_model_set_sort_column_id (GtkTreeSortable *sortable,
                                           gint             sort_column_id,
                                           GtkSortType      order);
static void
thunar_tree_view_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                              GtkTreeIterCompareFunc func,
                                              gpointer               data,
                                              GDestroyNotify         destroy);
static void
thunar_tree_view_model_set_sort_func (GtkTreeSortable       *sortable,
                                      gint                   sort_column_id,
                                      GtkTreeIterCompareFunc func,
                                      gpointer               data,
                                      GDestroyNotify         destroy);
static gboolean
thunar_tree_view_model_has_default_sort_func (GtkTreeSortable *sortable);


/*************************************************
 * GtkTreeModel Virtual Methods
 *************************************************/
static GtkTreeModelFlags
thunar_tree_view_model_get_flags (GtkTreeModel *model);
static gint
thunar_tree_view_model_get_n_columns (GtkTreeModel *model);
static GType
thunar_tree_view_model_get_column_type (GtkTreeModel *model,
                                        gint          idx);
static gboolean
thunar_tree_view_model_get_iter (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreePath  *path);
static GtkTreePath *
thunar_tree_view_model_get_path (GtkTreeModel *model,
                                 GtkTreeIter  *iter);
static void
thunar_tree_view_model_get_value (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value);
static gboolean
thunar_tree_view_model_iter_next (GtkTreeModel *model,
                                  GtkTreeIter  *iter);
static gboolean
thunar_tree_view_model_iter_previous (GtkTreeModel *model,
                                      GtkTreeIter  *iter);
static gboolean
thunar_tree_view_model_iter_children (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent);
static gboolean
thunar_tree_view_model_iter_has_child (GtkTreeModel *model,
                                       GtkTreeIter  *iter);
static gint
thunar_tree_view_model_iter_n_children (GtkTreeModel *model,
                                        GtkTreeIter  *iter);
static gboolean
thunar_tree_view_model_iter_nth_child (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent,
                                       gint          n);
static gboolean
thunar_tree_view_model_iter_parent (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *child);

/*************************************************
 * ThunarStandardViewModel Virtual Methods
 *************************************************/
/* Getters */
static ThunarFolder *
thunar_tree_view_model_get_folder (ThunarStandardViewModel *model);
static gboolean
thunar_tree_view_model_get_show_hidden (ThunarStandardViewModel *model);
static gboolean
thunar_tree_view_model_get_file_size_binary (ThunarStandardViewModel *model);
static ThunarFile *
thunar_tree_view_model_get_file (ThunarStandardViewModel *model,
                                 GtkTreeIter             *iter);
static GList *
thunar_tree_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                            GList                   *files);
static ThunarJob *
thunar_tree_view_model_get_job (ThunarStandardViewModel *model);

/* Setters */
static void
thunar_tree_view_model_set_folder (ThunarStandardViewModel *model,
                                   ThunarFolder            *folder,
                                   gchar                   *search_query);
static void
thunar_tree_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                        gboolean                 show_hidden);
static void
thunar_tree_view_model_set_folders_first (ThunarStandardViewModel *model,
                                          gboolean                 folders_first);
static void
thunar_tree_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                             gboolean                 file_size_binary);
static void
thunar_tree_view_model_set_job (ThunarStandardViewModel *model,
                                ThunarJob               *job);


/*************************************************
 * Private Methods
 *************************************************/
/* Property Getters */
static gboolean
thunar_tree_view_model_get_case_sensitive (ThunarTreeViewModel *model);
static ThunarDateStyle
thunar_tree_view_model_get_date_style (ThunarTreeViewModel *model);
static const char *
thunar_tree_view_model_get_date_custom_style (ThunarTreeViewModel *model);
static gint
thunar_tree_view_model_get_num_files (ThunarTreeViewModel *model);
static gboolean
thunar_tree_view_model_get_folders_first (ThunarTreeViewModel *model);
static gint
thunar_tree_view_model_get_folder_item_count (ThunarTreeViewModel *model);
static gboolean
thunar_tree_view_model_get_loading (ThunarTreeViewModel *model);

/* Property Setters */
static void
thunar_tree_view_model_set_case_sensitive (ThunarTreeViewModel *model,
                                           gboolean             case_sensitive);
static void
thunar_tree_view_model_set_date_style (ThunarTreeViewModel *model,
                                       ThunarDateStyle      date_style);
static void
thunar_tree_view_model_set_date_custom_style (ThunarTreeViewModel *model,
                                              const char          *date_custom_style);
static void
thunar_tree_view_model_set_folder_item_count (ThunarTreeViewModel  *model,
                                              ThunarFolderItemCount count_as_dir_size);

/* Internal helper funcs */
static Node *
thunar_tree_view_model_new_node (ThunarFile *file);
static Node *
thunar_tree_view_model_new_dummy_node (void);
static gboolean
thunar_tree_view_model_node_has_dummy_child (Node *node);
static void
thunar_tree_view_model_node_add_child (Node *node,
                                       Node *child);
static void
thunar_tree_view_model_node_add_dummy_child (Node *node);
static void
thunar_tree_view_model_node_drop_dummy_child (Node *node);
static Node *
thunar_tree_view_model_dir_add_file (Node       *node,
                                     ThunarFile *file);
static void
thunar_tree_view_model_dir_remove_file (Node       *node,
                                        ThunarFile *file);
static Node *
thunar_tree_view_model_locate_file (ThunarTreeViewModel *model,
                                    ThunarFile          *file);
static gint
thunar_tree_view_model_cmp_nodes (gconstpointer a,
                                  gconstpointer b,
                                  gpointer      data);
static void
thunar_tree_view_model_sort (ThunarTreeViewModel *model);
static void
thunar_tree_view_model_load_dir (Node *node);
static void
thunar_tree_view_model_cleanup_model (ThunarTreeViewModel *model);
static void
thunar_tree_view_model_file_count_callback (ExoJob              *job,
                                            ThunarTreeViewModel *model);
static void
thunar_tree_view_model_node_destroy (Node *node);
static void
thunar_tree_view_model_dir_files_changed (Node       *node,
                                          GHashTable *files);
static void
thunar_tree_view_model_set_loading (ThunarTreeViewModel *model,
                                    gboolean             loading);
static gboolean
thunar_tree_view_model_update_search_files (ThunarTreeViewModel *model);
static void
thunar_tree_view_model_add_search_files (ThunarStandardViewModel *model,
                                         GList                   *files);


/*************************************************
 *  Struct Definitions
 *************************************************/
struct _ThunarTreeViewModelClass
{
  GObjectClass __parent__;

  /* signals */
  void (*error) (ThunarTreeViewModel *model,
                 const GError        *error);
  void (*search_done) (void);
};



struct _ThunarTreeViewModel
{
  GObject __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint stamp;
#endif

  Node         *root;
  ThunarFolder *dir;

  /* Hashtable for quick access to all directories and sub-directories which are managed by this view
   * The key is the corresponding #ThunarFile of the directory, and the value is the #_Node of the directory */
  GHashTable *subdirs;

  gboolean       sort_case_sensitive : 1;
  gboolean       sort_folders_first : 1;
  gint           sort_sign; /* 1 = ascending, -1 descending */
  ThunarSortFunc sort_func;

  /* Options */
  ThunarFolderItemCount folder_item_count;
  gboolean              file_size_binary : 1;
  ThunarDateStyle       date_style;
  char                 *date_custom_style;
  gboolean              show_hidden;

  gint n_visible_files;
  gint loading;

  gchar **search_terms;

  ThunarJob *search_job;
  GList     *search_files;
  GMutex     mutex_add_search_files;

  guint update_search_results_timeout_id;
};



struct _Node
{
  ThunarFile   *file;
  ThunarFolder *dir;

  Node          *parent;
  GSequenceIter *ptr; /* self ref */

  gint     depth;
  gint     n_children;
  gboolean loaded;
  gboolean expanded;
  gboolean file_watch_active;

  /* set of all the children gfiles;
   * contains mappings of (gfile -> GSequenceIter *(_child_node->ptr)) */
  GHashTable *set;
  GHashTable *hidden_files;

  GSequence           *children; /* Nodes */
  ThunarTreeViewModel *model;

  guint scheduled_unload_id;
};



/*************************************************
 *  GType Definitions
 *************************************************/
G_DEFINE_TYPE_WITH_CODE (ThunarTreeViewModel, thunar_tree_view_model, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_tree_view_model_tree_model_init) G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_tree_view_model_drag_dest_init) G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE, thunar_tree_view_model_sortable_init) G_IMPLEMENT_INTERFACE (THUNAR_TYPE_STANDARD_VIEW_MODEL, thunar_tree_view_model_standard_view_model_init))



/* Reference to GParamSpec(s) of all the N_PROPERTIES */
static GParamSpec *tree_model_props[N_PROPERTIES] = {
  NULL,
};



static void
thunar_tree_view_model_class_init (ThunarTreeViewModelClass *klass)
{
  GObjectClass *gobject_class;
  gpointer      g_iface;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_tree_view_model_dispose;
  gobject_class->finalize = thunar_tree_view_model_finalize;
  gobject_class->get_property = thunar_tree_view_model_get_property;
  gobject_class->set_property = thunar_tree_view_model_set_property;

  g_iface = g_type_default_interface_peek (THUNAR_TYPE_STANDARD_VIEW_MODEL);
  /**
   * ThunarTreeViewModel:case-sensitive:
   *
   * Tells whether the sorting should be case sensitive.
   **/
  tree_model_props[PROP_CASE_SENSITIVE] =
  g_param_spec_override ("case-sensitive",
                         g_object_interface_find_property (g_iface, "case-sensitive"));

  /**
   * ThunarTreeViewModel:date-style:
   *
   * The style used to format dates.
   **/
  tree_model_props[PROP_DATE_STYLE] =
  g_param_spec_override ("date-style",
                         g_object_interface_find_property (g_iface, "date-style"));

  /**
   * ThunarTreeViewModel:date-custom-style:
   *
   * The style used for custom format of dates.
   **/
  tree_model_props[PROP_DATE_CUSTOM_STYLE] =
  g_param_spec_override ("date-custom-style",
                         g_object_interface_find_property (g_iface, "date-custom-style"));

  /**
   * ThunarTreeViewModel:folder:
   *
   * The folder presented by this #ThunarTreeViewModel.
   **/
  tree_model_props[PROP_FOLDER] =
  g_param_spec_override ("folder",
                         g_object_interface_find_property (g_iface, "folder"));

  /**
   * ThunarTreeViewModel::folders-first:
   *
   * Tells whether to always sort folders before other files.
   **/
  tree_model_props[PROP_FOLDERS_FIRST] =
  g_param_spec_override ("folders-first",
                         g_object_interface_find_property (g_iface, "folders-first"));

  /**
   * ThunarTreeViewModel::num-files:
   *
   * The number of files in the folder presented by this #ThunarTreeViewModel.
   **/
  tree_model_props[PROP_NUM_FILES] =
  g_param_spec_override ("num-files",
                         g_object_interface_find_property (g_iface, "num-files"));

  /**
   * ThunarTreeViewModel::show-hidden:
   *
   * Tells whether to include hidden (and backup) files.
   **/
  tree_model_props[PROP_SHOW_HIDDEN] =
  g_param_spec_override ("show-hidden",
                         g_object_interface_find_property (g_iface, "show-hidden"));

  /**
   * ThunarTreeViewModel::misc-file-size-binary:
   *
   * Tells whether to format file size in binary.
   **/
  tree_model_props[PROP_FILE_SIZE_BINARY] =
  g_param_spec_override ("file-size-binary",
                         g_object_interface_find_property (g_iface, "file-size-binary"));

  /**
   * ThunarTreeViewModel:folder-item-count:
   *
   * Tells when the size column of folders should show the number of containing files
   **/
  tree_model_props[PROP_FOLDER_ITEM_COUNT] =
  g_param_spec_override ("folder-item-count",
                         g_object_interface_find_property (g_iface, "folder-item-count"));

  /**
   * ThunarTreeViewModel:loading:
   *
   * Tells if the model is yet loading a folder
   **/
  tree_model_props[PROP_LOADING] =
  g_param_spec_override ("loading",
                         g_object_interface_find_property (g_iface, "loading"));

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, tree_model_props);
}



static void
thunar_tree_view_model_init (ThunarTreeViewModel *model)
{
  /* generate a unique stamp if we're in debug mode */
#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif

  model->search_job = NULL;
  model->update_search_results_timeout_id = 0;

  model->search_terms = NULL;
  model->search_files = NULL;
  g_mutex_init (&model->mutex_add_search_files);

  model->sort_func = thunar_file_compare_by_name;
  model->loading = 0;

  model->subdirs = g_hash_table_new (g_direct_hash, g_direct_equal);
}



static void
thunar_tree_view_model_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = thunar_tree_view_model_drag_data_received;
  iface->row_drop_possible = thunar_tree_view_model_row_drop_possible;
}



static void
thunar_tree_view_model_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = thunar_tree_view_model_get_sort_column_id;
  iface->set_sort_column_id = thunar_tree_view_model_set_sort_column_id;
  iface->set_sort_func = thunar_tree_view_model_set_sort_func;
  iface->set_default_sort_func = thunar_tree_view_model_set_default_sort_func;
  iface->has_default_sort_func = thunar_tree_view_model_has_default_sort_func;
}



static void
thunar_tree_view_model_standard_view_model_init (ThunarStandardViewModelIface *iface)
{
  iface->get_job = thunar_tree_view_model_get_job;
  iface->set_job = thunar_tree_view_model_set_job;
  iface->get_file = thunar_tree_view_model_get_file;
  iface->get_folder = thunar_tree_view_model_get_folder;
  iface->set_folder = thunar_tree_view_model_set_folder;
  iface->get_show_hidden = thunar_tree_view_model_get_show_hidden;
  iface->set_show_hidden = thunar_tree_view_model_set_show_hidden;
  iface->get_paths_for_files = thunar_tree_view_model_get_paths_for_files;
  iface->get_file_size_binary = thunar_tree_view_model_get_file_size_binary;
  iface->set_file_size_binary = thunar_tree_view_model_set_file_size_binary;
  iface->set_folders_first = thunar_tree_view_model_set_folders_first;
  iface->add_search_files = thunar_tree_view_model_add_search_files;
}



static void
thunar_tree_view_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_tree_view_model_get_flags;
  iface->get_n_columns = thunar_tree_view_model_get_n_columns;
  iface->get_column_type = thunar_tree_view_model_get_column_type;
  iface->get_iter = thunar_tree_view_model_get_iter;
  iface->get_path = thunar_tree_view_model_get_path;
  iface->get_value = thunar_tree_view_model_get_value;
  iface->iter_next = thunar_tree_view_model_iter_next;
  iface->iter_previous = thunar_tree_view_model_iter_previous;
  iface->iter_children = thunar_tree_view_model_iter_children;
  iface->iter_has_child = thunar_tree_view_model_iter_has_child;
  iface->iter_n_children = thunar_tree_view_model_iter_n_children;
  iface->iter_nth_child = thunar_tree_view_model_iter_nth_child;
  iface->iter_parent = thunar_tree_view_model_iter_parent;
}



/*************************************************
 *  GObject Virtual Func definitions
 *************************************************/
static void
thunar_tree_view_model_dispose (GObject *object)
{
  /* unlink from the folder (if any) */
  thunar_tree_view_model_set_folder (THUNAR_STANDARD_VIEW_MODEL (object), NULL, NULL);

  (*G_OBJECT_CLASS (thunar_tree_view_model_parent_class)->dispose) (object);
}



static void
thunar_tree_view_model_finalize (GObject *object)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (object);

  /* this will cancel the search job if any*/
  thunar_tree_view_model_set_folder (THUNAR_STANDARD_VIEW_MODEL (object),
                                     NULL, NULL);

  g_mutex_clear (&model->mutex_add_search_files);

  g_free (model->date_custom_style);
  g_strfreev (model->search_terms);

  g_hash_table_destroy (model->subdirs);

  (*G_OBJECT_CLASS (thunar_tree_view_model_parent_class)->finalize) (object);
}



static void
thunar_tree_view_model_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_tree_view_model_get_case_sensitive (model));
      break;

    case PROP_DATE_STYLE:
      g_value_set_enum (value, thunar_tree_view_model_get_date_style (model));
      break;

    case PROP_DATE_CUSTOM_STYLE:
      g_value_set_string (value, thunar_tree_view_model_get_date_custom_style (model));
      break;

    case PROP_FOLDER:
      g_value_set_object (value, thunar_tree_view_model_get_folder (THUNAR_STANDARD_VIEW_MODEL (model)));
      break;

    case PROP_FOLDERS_FIRST:
      g_value_set_boolean (value, thunar_tree_view_model_get_folders_first (model));
      break;

    case PROP_NUM_FILES:
      g_value_set_uint (value, thunar_tree_view_model_get_num_files (model));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_tree_view_model_get_show_hidden (THUNAR_STANDARD_VIEW_MODEL (model)));
      break;

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, thunar_tree_view_model_get_file_size_binary (THUNAR_STANDARD_VIEW_MODEL (model)));
      break;

    case PROP_FOLDER_ITEM_COUNT:
      g_value_set_enum (value, thunar_tree_view_model_get_folder_item_count (model));
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, thunar_tree_view_model_get_loading (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tree_view_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      thunar_tree_view_model_set_case_sensitive (model, g_value_get_boolean (value));
      break;

    case PROP_DATE_STYLE:
      thunar_tree_view_model_set_date_style (model, g_value_get_enum (value));
      break;

    case PROP_DATE_CUSTOM_STYLE:
      thunar_tree_view_model_set_date_custom_style (model, g_value_get_string (value));
      break;

    case PROP_FOLDER:
      thunar_tree_view_model_set_folder (THUNAR_STANDARD_VIEW_MODEL (model), g_value_get_object (value), NULL);
      break;

    case PROP_FOLDERS_FIRST:
      thunar_tree_view_model_set_folders_first (THUNAR_STANDARD_VIEW_MODEL (model), g_value_get_boolean (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_tree_view_model_set_show_hidden (THUNAR_STANDARD_VIEW_MODEL (model), g_value_get_boolean (value));
      break;

    case PROP_FILE_SIZE_BINARY:
      thunar_tree_view_model_set_file_size_binary (THUNAR_STANDARD_VIEW_MODEL (model), g_value_get_boolean (value));
      break;

    case PROP_FOLDER_ITEM_COUNT:
      thunar_tree_view_model_set_folder_item_count (model, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/*************************************************
 *  GtkTreeDragDest Virtual Func definitions
 *************************************************/
static gboolean
thunar_tree_view_model_drag_data_received (GtkTreeDragDest  *dest,
                                           GtkTreePath      *path,
                                           GtkSelectionData *data)
{
  return FALSE;
}



static gboolean
thunar_tree_view_model_row_drop_possible (GtkTreeDragDest  *dest,
                                          GtkTreePath      *path,
                                          GtkSelectionData *data)
{
  return FALSE;
}



/*************************************************
 *  GtkSortable Virtual Func definitions
 *************************************************/
static gboolean
thunar_tree_view_model_get_sort_column_id (GtkTreeSortable *sortable,
                                           gint            *sort_column_id,
                                           GtkSortType     *order)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (sortable);

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);

  if (model->sort_func == thunar_cmp_files_by_mime_type)
    *sort_column_id = THUNAR_COLUMN_MIME_TYPE;
  else if (model->sort_func == thunar_file_compare_by_name)
    *sort_column_id = THUNAR_COLUMN_NAME;
  else if (model->sort_func == thunar_cmp_files_by_permissions)
    *sort_column_id = THUNAR_COLUMN_PERMISSIONS;
  else if (model->sort_func == thunar_cmp_files_by_size || model->sort_func == (ThunarSortFunc) thunar_cmp_files_by_size_and_items_count)
    *sort_column_id = THUNAR_COLUMN_SIZE;
  else if (model->sort_func == thunar_cmp_files_by_size_in_bytes)
    *sort_column_id = THUNAR_COLUMN_SIZE_IN_BYTES;
  else if (model->sort_func == thunar_cmp_files_by_date_created)
    *sort_column_id = THUNAR_COLUMN_DATE_CREATED;
  else if (model->sort_func == thunar_cmp_files_by_date_accessed)
    *sort_column_id = THUNAR_COLUMN_DATE_ACCESSED;
  else if (model->sort_func == thunar_cmp_files_by_date_modified)
    *sort_column_id = THUNAR_COLUMN_DATE_MODIFIED;
  else if (model->sort_func == thunar_cmp_files_by_date_deleted)
    *sort_column_id = THUNAR_COLUMN_DATE_DELETED;
  else if (model->sort_func == thunar_cmp_files_by_recency)
    *sort_column_id = THUNAR_COLUMN_RECENCY;
  else if (model->sort_func == thunar_cmp_files_by_location)
    *sort_column_id = THUNAR_COLUMN_LOCATION;
  else if (model->sort_func == thunar_cmp_files_by_type)
    *sort_column_id = THUNAR_COLUMN_TYPE;
  else if (model->sort_func == thunar_cmp_files_by_owner)
    *sort_column_id = THUNAR_COLUMN_OWNER;
  else if (model->sort_func == thunar_cmp_files_by_group)
    *sort_column_id = THUNAR_COLUMN_GROUP;
  else
    _thunar_assert_not_reached ();

  if (order != NULL)
    {
      if (model->sort_sign > 0)
        *order = GTK_SORT_ASCENDING;
      else
        *order = GTK_SORT_DESCENDING;
    }

  return TRUE;
}



static void
thunar_tree_view_model_set_sort_column_id (GtkTreeSortable *sortable,
                                           gint             sort_column_id,
                                           GtkSortType      order)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (sortable);

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  switch (sort_column_id)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      model->sort_func = thunar_cmp_files_by_date_created;
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      model->sort_func = thunar_cmp_files_by_date_accessed;
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      model->sort_func = thunar_cmp_files_by_date_modified;
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      model->sort_func = thunar_cmp_files_by_date_deleted;
      break;

    case THUNAR_COLUMN_RECENCY:
      model->sort_func = thunar_cmp_files_by_recency;
      break;

    case THUNAR_COLUMN_LOCATION:
      model->sort_func = thunar_cmp_files_by_location;
      break;

    case THUNAR_COLUMN_GROUP:
      model->sort_func = thunar_cmp_files_by_group;
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      model->sort_func = thunar_cmp_files_by_mime_type;
      break;

    case THUNAR_COLUMN_FILE_NAME:
    case THUNAR_COLUMN_NAME:
      model->sort_func = thunar_file_compare_by_name;
      break;

    case THUNAR_COLUMN_OWNER:
      model->sort_func = thunar_cmp_files_by_owner;
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      model->sort_func = thunar_cmp_files_by_permissions;
      break;

    case THUNAR_COLUMN_SIZE:
      model->sort_func = (model->folder_item_count != THUNAR_FOLDER_ITEM_COUNT_NEVER) ? (ThunarSortFunc) thunar_cmp_files_by_size_and_items_count : thunar_cmp_files_by_size;
      break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      model->sort_func = thunar_cmp_files_by_size_in_bytes;
      break;

    case THUNAR_COLUMN_TYPE:
      model->sort_func = thunar_cmp_files_by_type;
      break;

    default:
      _thunar_assert_not_reached ();
    }

  /* new sort sign */
  model->sort_sign = (order == GTK_SORT_ASCENDING) ? 1 : -1;

  /* re-sort the model */
  if (model->root != NULL)
    thunar_tree_view_model_sort (model);

  /* notify listining parties */
  gtk_tree_sortable_sort_column_changed (sortable);
}



static void
thunar_tree_view_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                              GtkTreeIterCompareFunc func,
                                              gpointer               data,
                                              GDestroyNotify         destroy)
{
  g_critical ("ThunarTreeViewModel has sorting facilities built-in!");
}



static void
thunar_tree_view_model_set_sort_func (GtkTreeSortable       *sortable,
                                      gint                   sort_column_id,
                                      GtkTreeIterCompareFunc func,
                                      gpointer               data,
                                      GDestroyNotify         destroy)
{
  g_critical ("ThunarTreeViewModel has sorting facilities built-in!");
}



static gboolean
thunar_tree_view_model_has_default_sort_func (GtkTreeSortable *sortable)
{
  return FALSE;
}



/*************************************************
 *  GtkTreeModel Virtual Func definitions
 *************************************************/
static GtkTreeModelFlags
thunar_tree_view_model_get_flags (GtkTreeModel *model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}



static gint
thunar_tree_view_model_get_n_columns (GtkTreeModel *model)
{
  return THUNAR_N_COLUMNS;
}



static GType
thunar_tree_view_model_get_column_type (GtkTreeModel *model,
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
thunar_tree_view_model_get_iter (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreePath  *path)
{
  ThunarTreeViewModel *_model;
  GSequenceIter       *ptr = NULL;
  gint                *indices;
  gint                 depth, loc;
  Node                *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  _model = THUNAR_TREE_VIEW_MODEL (model);

  if (_model->root == NULL)
    return FALSE;

  indices = gtk_tree_path_get_indices_with_depth (path, &depth);
  node = _model->root;
  for (gint d = 0; d < depth; d++)
    {
      loc = indices[d];

      if (loc < 0 || loc >= node->n_children)
        return FALSE;

      ptr = g_sequence_get_iter_at_pos (node->children, loc);
      if (g_sequence_iter_is_end (ptr))
        return FALSE;
      node = g_sequence_get (ptr);
    }

  GTK_TREE_ITER_INIT (*iter, _model->stamp, ptr);

  return TRUE;
}



static GtkTreePath *
thunar_tree_view_model_get_path (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  GtkTreePath *path;
  Node        *node;
  gint        *indices;
  gint         depth;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, NULL);
  _thunar_return_val_if_fail (iter->user_data != NULL, NULL);

  THUNAR_WARN_RETURN_VAL (g_sequence_iter_is_end (iter->user_data), NULL);

  node = g_sequence_get (iter->user_data);

  depth = node->depth;
  indices = g_malloc_n (depth, sizeof (gint));

  for (gint d = depth - 1; d >= 0; --d)
    {
      indices[d] = g_sequence_iter_get_position (node->ptr);
      node = node->parent;
    }

  path = gtk_tree_path_new_from_indicesv (indices, depth);
  g_free (indices);
  return path;
}



static void
thunar_tree_view_model_get_value (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value)
{
  Node         *node;
  ThunarGroup  *group = NULL;
  const gchar  *device_type;
  const gchar  *name;
  const gchar  *real_name;
  ThunarUser   *user = NULL;
  ThunarFolder *folder;
  gint32        item_count;
  GFile        *g_file;
  GFile        *g_file_parent = NULL;
  gchar        *str = NULL;
  ThunarFile   *file = NULL;

  _thunar_return_if_fail (THUNAR_STANDARD_VIEW_MODEL (model));
  _thunar_return_if_fail (iter->stamp == (THUNAR_TREE_VIEW_MODEL (model))->stamp);

  node = g_sequence_get (iter->user_data);
  file = node->file;
  if (file != NULL)
    g_object_ref (file);

  switch (column)
    {
    case THUNAR_COLUMN_DATE_CREATED:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_CREATED, THUNAR_TREE_VIEW_MODEL (model)->date_style, THUNAR_TREE_VIEW_MODEL (model)->date_custom_style);
      g_value_take_string (value, str); /* take str is nullable */
      break;

    case THUNAR_COLUMN_DATE_ACCESSED:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_ACCESSED, THUNAR_TREE_VIEW_MODEL (model)->date_style, THUNAR_TREE_VIEW_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_MODIFIED:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_MODIFIED, THUNAR_TREE_VIEW_MODEL (model)->date_style, THUNAR_TREE_VIEW_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_DATE_DELETED:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      str = thunar_file_get_date_string (file, THUNAR_FILE_DATE_DELETED, THUNAR_TREE_VIEW_MODEL (model)->date_style, THUNAR_TREE_VIEW_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_RECENCY:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      str = thunar_file_get_date_string (file, THUNAR_FILE_RECENCY, THUNAR_TREE_VIEW_MODEL (model)->date_style, THUNAR_TREE_VIEW_MODEL (model)->date_custom_style);
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_LOCATION:
      g_value_init (value, G_TYPE_STRING);
      /* NOTE: return val can be null due to file not having been loaded yet.
       * The non visible children are lazily loaded. */
      if (file != NULL)
        g_file_parent = g_file_get_parent (thunar_file_get_file (file));
      str = NULL;

      /* g_file_parent will be NULL if a search returned the root
       * directory somehow, or "file:///" is in recent:/// somehow.
       * These should be quite rare circumstances. */
      if (G_UNLIKELY (g_file_parent == NULL))
        {
          g_value_take_string (value, NULL);
          break;
        }

      /* Try and show a relative path beginning with the current folder's name to the parent folder.
       * Fall thru with str==NULL if that is not possible. */
      folder = THUNAR_TREE_VIEW_MODEL (model)->dir;
      if (G_LIKELY (folder != NULL))
        {
          const gchar *folder_basename = thunar_file_get_basename (thunar_folder_get_corresponding_file (folder));
          GFile       *g_folder = thunar_file_get_file (thunar_folder_get_corresponding_file (folder));
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
      g_value_take_string (value, str);
      break;

    case THUNAR_COLUMN_GROUP:
      g_value_init (value, G_TYPE_STRING);
      if (file != NULL)
        group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          g_value_set_string (value, thunar_group_get_name (group));
          g_object_unref (G_OBJECT (group));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;

    case THUNAR_COLUMN_MIME_TYPE:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      g_value_set_static_string (value, thunar_file_get_content_type (file));
      break;

    case THUNAR_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, _ ("Loading..."));
          break;
        }
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    case THUNAR_COLUMN_OWNER:
      g_value_init (value, G_TYPE_STRING);
      if (file != NULL)
        user = thunar_file_get_user (file);
      if (G_LIKELY (user != NULL))
        {
          /* determine sane display name for the owner */
          name = thunar_user_get_name (user);
          real_name = thunar_user_get_real_name (user);
          if (G_LIKELY (real_name != NULL))
            {
              if (strcmp (name, real_name) == 0)
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
          g_value_set_static_string (value, "");
        }
      break;

    case THUNAR_COLUMN_PERMISSIONS:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      g_value_take_string (value, thunar_file_get_mode_string (file));
      break;

    case THUNAR_COLUMN_SIZE:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      if (thunar_file_is_mountable (file))
        {
          g_file = thunar_file_get_target_location (file);
          if (g_file == NULL)
            break;
          g_value_take_string (value, thunar_g_file_get_free_space_string (g_file, THUNAR_TREE_VIEW_MODEL (model)->file_size_binary));
          g_object_unref (g_file);
          break;
        }
      else if (thunar_file_is_directory (file))
        {
          if (THUNAR_TREE_VIEW_MODEL (model)->folder_item_count == THUNAR_FOLDER_ITEM_COUNT_ALWAYS)
            {
              item_count = thunar_file_get_file_count (file, G_CALLBACK (thunar_tree_view_model_file_count_callback), model);
              if (item_count < 0)
                g_value_take_string (value, g_strdup (_("unknown")));
              else
                g_value_take_string (value, g_strdup_printf (ngettext ("%u item", "%u items", item_count), item_count));
            }

          /* If the option is set to always show folder sizes as item counts only for local files,
           * check if the files is local or not, and act accordingly */
          else if (THUNAR_TREE_VIEW_MODEL (model)->folder_item_count == THUNAR_FOLDER_ITEM_COUNT_ONLY_LOCAL)
            {
              if (thunar_file_is_local (file))
                {
                  item_count = thunar_file_get_file_count (file, G_CALLBACK (thunar_tree_view_model_file_count_callback), model);
                  if (item_count < 0)
                    g_value_take_string (value, g_strdup (_("unknown")));
                  else
                    g_value_take_string (value, g_strdup_printf (ngettext ("%u item", "%u items", item_count), item_count));
                }
              else
                g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_TREE_VIEW_MODEL (model)->file_size_binary));
            }
          else if (THUNAR_TREE_VIEW_MODEL (model)->folder_item_count != THUNAR_FOLDER_ITEM_COUNT_NEVER)
            g_warning ("Error, unknown enum value for folder_item_count in the list model");
        }
      else
        {
          g_value_take_string (value, thunar_file_get_size_string_formatted (file, THUNAR_TREE_VIEW_MODEL (model)->file_size_binary));
        }
      break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      g_value_take_string (value, thunar_file_get_size_in_bytes_string (file));
      break;

    case THUNAR_COLUMN_TYPE:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, "");
          break;
        }
      device_type = thunar_file_get_device_type (file);
      if (device_type != NULL)
        {
          g_value_take_string (value, g_strdup (device_type));
          break;
        }
      g_value_take_string (value, thunar_file_get_content_type_desc (file));
      break;

    case THUNAR_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, file);
      break;

    case THUNAR_COLUMN_FILE_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (file == NULL)
        {
          g_value_set_static_string (value, _ ("Loading..."));
          break;
        }
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    default:
      _thunar_assert_not_reached ();
      break;
    }

  if (file != NULL)
    g_object_unref (file);
}



static gboolean
thunar_tree_view_model_iter_next (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  GSequenceIter *ptr;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  THUNAR_WARN_RETURN_VAL (g_sequence_iter_is_end (iter->user_data), FALSE);

  ptr = g_sequence_iter_next (iter->user_data);
  if (g_sequence_iter_is_end (ptr))
    return FALSE;
  iter->user_data = ptr;
  return TRUE;
}



static gboolean
thunar_tree_view_model_iter_previous (GtkTreeModel *model,
                                      GtkTreeIter  *iter)
{
  GSequenceIter *ptr;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  THUNAR_WARN_RETURN_VAL (g_sequence_iter_is_begin (iter->user_data), FALSE);

  ptr = g_sequence_iter_prev (iter->user_data);
  if (g_sequence_iter_is_begin (ptr))
    return FALSE;
  iter->user_data = ptr;
  return TRUE;
}



static gboolean
thunar_tree_view_model_iter_children (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent)
{
  GSequenceIter *ptr;
  Node          *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (parent->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);

  if (parent == NULL)
    /* return iter corresponding to path -> "0"; i.e 1st iter */
    return gtk_tree_model_get_iter_first (model, iter);

  node = g_sequence_get (parent->user_data);
  if (node->n_children == 0 || node->children == NULL)
    return FALSE;

  ptr = g_sequence_get_iter_at_pos (node->children, 0);
  THUNAR_WARN_RETURN_VAL (g_sequence_iter_is_end (ptr), FALSE);
  GTK_TREE_ITER_INIT (*iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, ptr);

  return TRUE;
}



static gboolean
thunar_tree_view_model_iter_has_child (GtkTreeModel *model,
                                       GtkTreeIter  *iter)
{
  Node *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  node = g_sequence_get (iter->user_data);
  return node->n_children > 0;
}



static gint
thunar_tree_view_model_iter_n_children (GtkTreeModel *model,
                                        GtkTreeIter  *iter)
{
  ThunarTreeViewModel *_model;
  Node                *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);

  _model = THUNAR_TREE_VIEW_MODEL (model);

  /* As a special case if iter == NULL then return number of toplevel children */
  if (iter == NULL)
    return _model->root != NULL ? _model->root->n_children : 0;

  _thunar_return_val_if_fail (iter->stamp == _model->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  node = g_sequence_get (iter->user_data);
  return node->n_children;
}



static gboolean
thunar_tree_view_model_iter_nth_child (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent,
                                       gint          n)
{
  GSequenceIter *ptr;
  Node          *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);

  if (parent == NULL)
    node = THUNAR_TREE_VIEW_MODEL (model)->root;
  else
    node = g_sequence_get (parent->user_data);

  if (node == NULL || n >= node->n_children)
    return FALSE;

  ptr = g_sequence_get_iter_at_pos (node->children, n);
  THUNAR_WARN_RETURN_VAL (g_sequence_iter_is_end (ptr), FALSE);
  GTK_TREE_ITER_INIT (*iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, ptr);

  return TRUE;
}



static gboolean
thunar_tree_view_model_iter_parent (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *child)
{
  GSequenceIter *ptr;
  Node          *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (child->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (child->user_data != NULL, FALSE);

  node = g_sequence_get (child->user_data);
  if (node->depth <= 1)
    return FALSE;

  ptr = node->parent->ptr;
  GTK_TREE_ITER_INIT (*iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, ptr);

  return TRUE;
}



static ThunarFolder *
thunar_tree_view_model_get_folder (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  return THUNAR_TREE_VIEW_MODEL (model)->dir;
}



static gboolean
thunar_tree_view_model_get_show_hidden (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  return THUNAR_TREE_VIEW_MODEL (model)->show_hidden;
}



static gboolean
thunar_tree_view_model_get_file_size_binary (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  return THUNAR_TREE_VIEW_MODEL (model)->file_size_binary;
}



static ThunarFile *
thunar_tree_view_model_get_file (ThunarStandardViewModel *model,
                                 GtkTreeIter             *iter)
{
  ThunarFile *file;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_VIEW_MODEL (model)->stamp, NULL);
  _thunar_return_val_if_fail (iter->user_data != NULL, NULL);

  file = ((Node *) g_sequence_get (iter->user_data))->file;
  return file != NULL ? g_object_ref (file) : NULL;
}



static GList *
thunar_tree_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                            GList                   *files)
{
  GtkTreePath *path;
  GtkTreeIter  tree_iter;
  Node        *node;
  GList       *paths = NULL;

  for (GList *lp = files; lp != NULL; lp = lp->next)
    {
      node = thunar_tree_view_model_locate_file (THUNAR_TREE_VIEW_MODEL (model), THUNAR_FILE (lp->data));

      if (node == NULL)
        continue;

      GTK_TREE_ITER_INIT (tree_iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, node->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &tree_iter);
      paths = g_list_prepend (paths, path);
    }

  return paths;
}



static ThunarJob *
thunar_tree_view_model_get_job (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  return THUNAR_TREE_VIEW_MODEL (model)->search_job;
}



G_NORETURN
static void
_thunar_tree_view_model_search_error (ThunarJob *job)
{
  g_error ("Error while searching recursively");
}



static void
_thunar_tree_view_model_search_finished (ThunarJob           *job,
                                         ThunarTreeViewModel *model)
{
  if (model->search_job)
    {
      g_signal_handlers_disconnect_by_data (model->search_job, model);
      g_object_unref (model->search_job);
      model->search_job = NULL;
    }

  if (model->update_search_results_timeout_id > 0)
    {
      thunar_tree_view_model_update_search_files (model);
      g_source_remove (model->update_search_results_timeout_id);
      model->update_search_results_timeout_id = 0;
    }

  thunar_g_list_free_full (model->search_files);
  model->search_files = NULL;

  g_signal_emit_by_name (model, "search-done");
}



static void
_thunar_tree_view_model_cancel_search_job (ThunarTreeViewModel *model)
{
  /* cancel the ongoing search if there is one */
  if (model->search_job)
    {
      exo_job_cancel (EXO_JOB (model->search_job));

      g_signal_handlers_disconnect_by_data (model->search_job, model);
      g_object_unref (model->search_job);
      model->search_job = NULL;
    }
}



static void
thunar_tree_view_model_set_folder (ThunarStandardViewModel *model,
                                   ThunarFolder            *folder,
                                   gchar                   *search_query)
{
  ThunarTreeViewModel *_model;
  gchar               *search_query_normalized;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  _model = THUNAR_TREE_VIEW_MODEL (model);

  _thunar_tree_view_model_cancel_search_job (_model);

  if (_model->update_search_results_timeout_id != 0)
    {
      g_source_remove (_model->update_search_results_timeout_id);
      _model->update_search_results_timeout_id = 0;
    }
  thunar_g_list_free_full (_model->search_files);
  _model->search_files = NULL;

  thunar_tree_view_model_cleanup_model (_model);
  _model->root = NULL;

  if (g_hash_table_size (_model->subdirs) > 0)
    g_hash_table_remove_all (_model->subdirs);

  if (_model->dir != NULL)
    g_object_unref (_model->dir);
  _model->dir = NULL;

  _model->dir = folder;
  if (_model->dir == NULL)
    return;

  g_object_freeze_notify (G_OBJECT (model));

  g_object_ref (_model->dir);
  _model->root = thunar_tree_view_model_new_node (thunar_folder_get_corresponding_file (_model->dir));
  _model->root->model = _model;

  if (search_query == NULL || strlen (g_strstrip (search_query)) == 0)
    thunar_tree_view_model_load_dir (_model->root);
  else
    {
      search_query_normalized = thunar_g_utf8_normalize_for_search (search_query, TRUE, TRUE);
      g_strfreev (_model->search_terms);
      _model->search_terms = thunar_util_split_search_query (search_query_normalized, NULL);
      if (_model->search_terms != NULL)
        {
          /* search the current folder
           * start a new recursive_search_job */
          _model->search_job = thunar_io_jobs_search_directory (THUNAR_STANDARD_VIEW_MODEL (_model), search_query_normalized, thunar_folder_get_corresponding_file (folder));
          g_signal_connect (_model->search_job, "error", G_CALLBACK (_thunar_tree_view_model_search_error), NULL);
          g_signal_connect (_model->search_job, "finished", G_CALLBACK (_thunar_tree_view_model_search_finished), _model);
          exo_job_launch (EXO_JOB (_model->search_job));

          /* add new results to the model every X ms */
          _model->update_search_results_timeout_id = g_timeout_add (500, G_SOURCE_FUNC (thunar_tree_view_model_update_search_files), _model);
        }
      g_free (search_query_normalized);
    }

  /* notify listeners that we have a new folder */
  g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_FOLDER]);
  g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_NUM_FILES]);
  g_object_thaw_notify (G_OBJECT (model));
}



static void
_thunar_tree_view_model_set_show_hidden (Node    *node,
                                         gpointer data)
{
  GHashTableIter iter;
  gpointer       key;

  /* we have a dummy node here! */
  if (node->file == NULL || node->dir == NULL
      || thunar_tree_view_model_node_has_dummy_child (node))
    return;

  g_sequence_foreach (node->children,
                      (GFunc) _thunar_tree_view_model_set_show_hidden, NULL);

  g_hash_table_iter_init (&iter, node->hidden_files);
  if (node->model->show_hidden)
    while (g_hash_table_iter_next (&iter, &key, NULL))
      thunar_tree_view_model_dir_add_file (node, THUNAR_FILE (key));
  else
    while (g_hash_table_iter_next (&iter, &key, NULL))
      thunar_tree_view_model_dir_remove_file (node, THUNAR_FILE (key));
}



static void
thunar_tree_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                        gboolean                 show_hidden)
{
  ThunarTreeViewModel *_model;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  _model = THUNAR_TREE_VIEW_MODEL (model);

  if (_model->show_hidden == show_hidden)
    return;

  THUNAR_TREE_VIEW_MODEL (model)->show_hidden = show_hidden;

  if (_model->root != NULL)
    _thunar_tree_view_model_set_show_hidden (_model->root, NULL);
}



static void
thunar_tree_view_model_set_folders_first (ThunarStandardViewModel *model,
                                          gboolean                 folders_first)
{
  ThunarTreeViewModel *_model;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  _model = THUNAR_TREE_VIEW_MODEL (model);

  /* check if the new setting differs */
  if ((_model->sort_folders_first && folders_first)
      || (!_model->sort_folders_first && !folders_first))
    return;

  /* apply the new setting (re-sorting the _model) */
  _model->sort_folders_first = folders_first;
  g_object_notify_by_pspec (G_OBJECT (_model), tree_model_props[PROP_FOLDERS_FIRST]);
  thunar_tree_view_model_sort (_model);

  /* emit a "changed" signal for each row, so the display is
     reloaded with the new folders first setting */
  gtk_tree_model_foreach (GTK_TREE_MODEL (_model),
                          (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed,
                          NULL);
}



static void
thunar_tree_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                             gboolean                 file_size_binary)
{
  ThunarTreeViewModel *_model;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  _model = THUNAR_TREE_VIEW_MODEL (model);

  /* normalize the setting */
  file_size_binary = !!file_size_binary;

  /* check if we have a new setting */
  if (_model->file_size_binary != file_size_binary)
    {
      /* apply the new setting */
      _model->file_size_binary = file_size_binary;

      /* resort the model with the new setting */
      thunar_tree_view_model_sort (_model);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (_model), tree_model_props[PROP_FILE_SIZE_BINARY]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new binary file size setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (_model),
                              (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



static void
thunar_tree_view_model_set_job (ThunarStandardViewModel *model,
                                ThunarJob               *job)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));
  THUNAR_TREE_VIEW_MODEL (model)->search_job = job;
}



static gboolean
thunar_tree_view_model_get_case_sensitive (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  return model->sort_case_sensitive;
}



static ThunarDateStyle
thunar_tree_view_model_get_date_style (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), THUNAR_DATE_STYLE_SIMPLE);
  return model->date_style;
}



static const char *
thunar_tree_view_model_get_date_custom_style (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  return model->date_custom_style;
}



static gint
thunar_tree_view_model_get_num_files (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), 0);
  return model->root != NULL ? model->root->n_children : 0;
}



static gboolean
thunar_tree_view_model_get_folders_first (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  return model->sort_folders_first;
}



static gint
thunar_tree_view_model_get_folder_item_count (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), THUNAR_FOLDER_ITEM_COUNT_NEVER);
  return model->folder_item_count;
}



static gboolean
thunar_tree_view_model_get_loading (ThunarTreeViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  return model->loading > 0;
}



static void
thunar_tree_view_model_set_case_sensitive (ThunarTreeViewModel *model,
                                           gboolean             case_sensitive)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  /* normalize the setting */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (model->sort_case_sensitive != case_sensitive)
    {
      /* apply the new setting */
      model->sort_case_sensitive = case_sensitive;

      /* resort the model with the new setting */
      thunar_tree_view_model_sort (model);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_CASE_SENSITIVE]);

      /* emit a "changed" signal for each row, so the display is
         reloaded with the new case-sensitive setting */
      gtk_tree_model_foreach (GTK_TREE_MODEL (model),
                              (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



static void
thunar_tree_view_model_set_date_style (ThunarTreeViewModel *model,
                                       ThunarDateStyle      date_style)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  /* check if we have a new setting */
  if (model->date_style != date_style)
    {
      /* apply the new setting */
      model->date_style = date_style;

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_DATE_STYLE]);

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (model),
                              (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



static void
thunar_tree_view_model_set_date_custom_style (ThunarTreeViewModel *model,
                                              const char          *date_custom_style)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  /* check if we have a new setting */
  if (g_strcmp0 (model->date_custom_style, date_custom_style) != 0)
    {
      /* apply the new setting */
      g_free (model->date_custom_style);
      model->date_custom_style = g_strdup (date_custom_style);

      /* notify listeners */
      g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_DATE_CUSTOM_STYLE]);

      /* emit a "changed" signal for each row, so the display is reloaded with the new date style */
      gtk_tree_model_foreach (GTK_TREE_MODEL (model),
                              (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed,
                              NULL);
    }
}



static void
thunar_tree_view_model_set_folder_item_count (ThunarTreeViewModel  *model,
                                              ThunarFolderItemCount count_as_dir_size)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  /* check if the new setting differs */
  if (model->folder_item_count == count_as_dir_size)
    return;

  model->folder_item_count = count_as_dir_size;
  g_object_notify_by_pspec (G_OBJECT (model), tree_model_props[PROP_FOLDER_ITEM_COUNT]);

  gtk_tree_model_foreach (GTK_TREE_MODEL (model), (GtkTreeModelForeachFunc) (void (*) (void)) gtk_tree_model_row_changed, NULL);

  /* re-sorting the model if needed */
  if (model->sort_func == thunar_cmp_files_by_size || model->sort_func == (ThunarSortFunc) thunar_cmp_files_by_size_and_items_count)
    {
      model->sort_func = (model->folder_item_count != THUNAR_FOLDER_ITEM_COUNT_NEVER) ? (ThunarSortFunc) thunar_cmp_files_by_size_and_items_count : thunar_cmp_files_by_size;
      thunar_tree_view_model_sort (model);
    }
}



static Node *
thunar_tree_view_model_new_node (ThunarFile *file)
{
  Node *_node = g_malloc (sizeof (Node));

  THUNAR_WARN_RETURN_VAL (file == NULL, NULL);

  _node->file = g_object_ref (file);

  _node->dir = NULL;

  /* Not been added to the model yet */
  _node->parent = NULL;
  _node->ptr = NULL;

  _node->depth = 0;
  _node->n_children = 0;
  /* if dir is NULL then no need to load; already loaded */
  _node->loaded = FALSE;

  _node->set = g_hash_table_new (g_direct_hash, g_direct_equal);
  _node->hidden_files = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
  _node->children = g_sequence_new (NULL);

  _node->scheduled_unload_id = 0;

  _node->file_watch_active = FALSE;

  return _node;
}



static Node *
thunar_tree_view_model_new_dummy_node (void)
{
  Node *_node = g_malloc (sizeof (Node));

  _node->file = NULL;

  _node->dir = NULL;

  /* Not been added to the model yet */
  _node->parent = NULL;
  _node->ptr = NULL;

  _node->depth = 0;
  _node->n_children = 0;
  /* if dir is NULL then no need to load; already loaded */
  _node->loaded = FALSE;

  _node->set = NULL;
  _node->hidden_files = NULL;
  _node->children = NULL;

  _node->scheduled_unload_id = 0;

  return _node;
}



static gboolean
thunar_tree_view_model_node_has_dummy_child (Node *node)
{
  GSequenceIter *iter;
  Node          *child;

  if (node->n_children != 1)
    return FALSE;
  iter = g_sequence_get_iter_at_pos (node->children, 0);
  child = g_sequence_get (iter);
  return child->file == NULL;
}



static void
thunar_tree_view_model_node_add_child (Node *node,
                                       Node *child)
{
  GtkTreeIter    tree_iter;
  GtkTreePath   *path;
  GSequenceIter *iter;
  Node          *_child;

  THUNAR_WARN_VOID_RETURN (child->file == NULL);

  child->depth = node->depth + 1;
  child->parent = node;
  child->model = node->model;

  if (thunar_tree_view_model_node_has_dummy_child (node))
    {
      /* replace the dummy node with the new child node */
      iter = g_sequence_get_iter_at_pos (node->children, 0);

      THUNAR_WARN_VOID_RETURN (iter == NULL);

      _child = g_sequence_get (iter);
      g_sequence_set (iter, child);
      child->ptr = iter;

      THUNAR_WARN_VOID_RETURN (iter == NULL);

      g_free (_child); /* free the dummy node */

      /* notify the view */
      GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, child->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (node->model), path, &tree_iter);
      gtk_tree_path_free (path);
    }
  else
    {
      node->n_children++;
      child->ptr = g_sequence_insert_sorted (node->children, child,
                                             thunar_tree_view_model_cmp_nodes, node->model);

      /* notify the view */
      GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, child->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (node->model), path, &tree_iter);
      gtk_tree_path_free (path);
    }

  g_hash_table_insert (node->set, child->file, child->ptr);
}



static void
thunar_tree_view_model_node_add_dummy_child (Node *node)
{
  GtkTreeIter  tree_iter;
  GtkTreePath *path;
  Node        *dummy;

  dummy = thunar_tree_view_model_new_dummy_node ();
  dummy->depth = node->depth + 1;
  dummy->parent = node;
  node->n_children++;

  dummy->ptr = g_sequence_prepend (node->children, dummy);

  GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, dummy->ptr);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (node->model), path, &tree_iter);
  gtk_tree_path_free (path);
}



static void
thunar_tree_view_model_node_drop_dummy_child (Node *node)
{
  Node          *dummy;
  GtkTreeIter    tree_iter;
  GtkTreePath   *path;
  GSequenceIter *iter;

  _thunar_return_if_fail (thunar_tree_view_model_node_has_dummy_child (node));


  THUNAR_WARN_VOID_RETURN (node->n_children != 1);

  iter = g_sequence_get_iter_at_pos (node->children, 0);

  THUNAR_WARN_VOID_RETURN (g_sequence_iter_is_end (iter));

  dummy = g_sequence_get (iter);
  THUNAR_WARN_VOID_RETURN (iter == NULL || dummy->file != NULL);

  GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, dummy->ptr);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (node->model), path);
  gtk_tree_path_free (path);

  node->n_children--;

  THUNAR_WARN_VOID_RETURN (node->n_children != 0);

  g_sequence_remove (dummy->ptr);
  g_free (dummy);

  if (node->ptr != NULL)
    {
      GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, node->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (node->model), path, &tree_iter);
      gtk_tree_path_free (path);
    }
}



static Node *
thunar_tree_view_model_dir_add_file (Node       *node,
                                     ThunarFile *file)
{
  GtkTreeIter  tree_iter;
  GtkTreePath *path;
  Node        *child;

  if (g_hash_table_contains (node->set, file))
    {
      GSequenceIter *iter = g_hash_table_lookup (node->set, file);
      return g_sequence_get (iter);
    }

  child = thunar_tree_view_model_new_node (file);
  thunar_tree_view_model_node_add_child (node, child);

  if (thunar_file_is_directory (file)
      && !thunar_file_is_empty_directory (file))
    thunar_tree_view_model_node_add_dummy_child (child);

  /* notify the model if a child has been added to previously empty folder */
  if (node->ptr != NULL && node->n_children == 1)
    {
      GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, node->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (node->model), path, &tree_iter);
      gtk_tree_path_free (path);
    }

  return child;
}



static void
thunar_tree_view_model_dir_remove_file (Node       *node,
                                        ThunarFile *file)
{
  GtkTreeIter    tree_iter;
  GtkTreePath   *path;
  GSequenceIter *iter;

  iter = g_hash_table_lookup (node->set, file);

  THUNAR_WARN_VOID_RETURN (iter == NULL);

  GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);

  thunar_tree_view_model_node_destroy (g_sequence_get (iter));

  g_sequence_remove (iter);
  g_hash_table_remove (node->set, file);
  node->n_children--;

  /* row deletion will trigger selection change if the file is selected;
   * thus it is important to free the node before calling row_deleted */
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (node->model), path);
  gtk_tree_path_free (path);

  THUNAR_WARN_VOID_RETURN (node->n_children < 0);

  /* notify the model if all children have been deleted */
  if (node->ptr != NULL && node->n_children == 0)
    {
      GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, node->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (node->model), path, &tree_iter);
      gtk_tree_path_free (path);
    }
}



static Node *
thunar_tree_view_model_locate_file (ThunarTreeViewModel *model,
                                    ThunarFile          *file)
{
  ThunarFile    *parent;
  GSequenceIter *iter;
  Node          *parent_node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (!thunar_file_has_parent (file))
    return NULL;

  parent = thunar_file_get_parent (file, NULL);

  if (parent == NULL)
    return NULL;

  if (!g_hash_table_contains (model->subdirs, parent))
    {
      g_object_unref (parent);
      return NULL;
    }

  parent_node = g_hash_table_lookup (model->subdirs, parent);
  g_object_unref (parent);
  THUNAR_WARN_RETURN_VAL (parent_node == NULL, NULL);

  if (!g_hash_table_contains (parent_node->set, file))
    return NULL;

  iter = g_hash_table_lookup (parent_node->set, file);
  THUNAR_WARN_RETURN_VAL (iter == NULL, NULL);

  return g_sequence_get (iter);
}



static gint
thunar_tree_view_model_cmp_nodes (gconstpointer a,
                                  gconstpointer b,
                                  gpointer      data)
{
  ThunarTreeViewModel *model = THUNAR_TREE_VIEW_MODEL (data);
  gboolean             isdir_a;
  gboolean             isdir_b;

  a = ((Node *) a)->file;
  b = ((Node *) b)->file;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (a), 0);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (b), 0);

  if (G_LIKELY (model->sort_folders_first))
    {
      isdir_a = thunar_file_is_directory (a);
      isdir_b = thunar_file_is_directory (b);
      if (isdir_a != isdir_b)
        return isdir_a ? -1 : 1;
    }

  return (*model->sort_func) (a, b, model->sort_case_sensitive) * model->sort_sign;
}



static void
_thunar_tree_view_model_sort (Node    *node,
                              gpointer data)
{
  GtkTreePath    *path;
  GtkTreeIter     iter;
  GSequenceIter **old_order;
  gint           *new_order;
  gint            n;
  gint            length;
  GSequenceIter  *row;

  if (!node->loaded || node->children == NULL)
    return;

  /* recursively sort */
  if (node->children != NULL)
    g_sequence_foreach (node->children, (GFunc) _thunar_tree_view_model_sort, NULL);

  length = node->n_children;
  if (G_UNLIKELY (length <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (length < STACK_ALLOC_LIMIT))
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
  row = g_sequence_get_begin_iter (node->children);
  for (n = 0; n < length; ++n)
    {
      old_order[n] = row;
      row = g_sequence_iter_next (row);
    }

  /* sort */
  g_sequence_sort (node->children, thunar_tree_view_model_cmp_nodes, node->model);

  /* new_order[newpos] = oldpos */
  for (n = 0; n < length; ++n)
    new_order[g_sequence_iter_get_position (old_order[n])] = n;

  /* tell the view about the new item order */
  if (node->ptr != NULL)
    {
      GTK_TREE_ITER_INIT (iter, node->model->stamp, node->ptr);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &iter);
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (node->model), path, &iter, new_order);
      gtk_tree_path_free (path);
    }
  else
    {
      path = gtk_tree_path_new ();
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (node->model), path, NULL, new_order);
      gtk_tree_path_free (path);
    }

  /* clean up if we used the heap */
  if (G_UNLIKELY (length >= STACK_ALLOC_LIMIT))
    {
      g_free (old_order);
      g_free (new_order);
    }
}



static void
thunar_tree_view_model_sort (ThunarTreeViewModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  if (model->root == NULL)
    return;

  /* sort the entire model */
  _thunar_tree_view_model_sort (model->root, NULL);
}



static void
_thunar_tree_view_model_folder_destroy (Node *node)
{
  /* if a subdir get's destroyed it's handled internally but if the current_folder
   * get's destroyed then we simply set_folder to NULL & cleanup things */
  if (node->parent == NULL)
    thunar_tree_view_model_set_folder (THUNAR_STANDARD_VIEW_MODEL (node->model), NULL, NULL);
}



static void
_thunar_tree_view_model_folder_error (Node         *node,
                                      const GError *error)
{
  _thunar_return_if_fail (error != NULL);

  if (node->n_children > 0)
    {
      if (node->parent == NULL)
        {
          /* set_folder func does not emit row-deleted signal instead relies
           * on ThunarStandardView to disconnect & reconnect the view quickly update
           * changes in current_directory. */
          GList *keys = g_hash_table_get_keys (node->set);

          for (GList *lp = keys; lp != NULL; lp = lp->next)
            thunar_tree_view_model_dir_remove_file (node, THUNAR_FILE (lp->data));

          g_list_free (keys);

          /* reset the model if error is with the current directory */
          thunar_tree_view_model_set_folder (THUNAR_STANDARD_VIEW_MODEL (node->model), NULL, NULL);
        }
      else
        thunar_tree_view_model_dir_remove_file (node->parent, node->file);
    }
}



static void
_thunar_tree_view_model_dir_files_added (Node       *node,
                                         GHashTable *files)
{
  ThunarFile    *file;
  GHashTableIter iter;
  gpointer       key;

  g_hash_table_iter_init (&iter, files);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      file = THUNAR_FILE (key);

      if (thunar_file_is_hidden (file) && !g_hash_table_contains (node->hidden_files, file))
        {
          g_hash_table_add (node->hidden_files, g_object_ref (file));

          if (!node->model->show_hidden)
            continue;
        }

      if (!g_hash_table_contains (node->set, file))
        thunar_tree_view_model_dir_add_file (node, file);
    }
}



static void
_thunar_tree_view_model_dir_files_removed (Node       *node,
                                           GHashTable *files)
{
  ThunarFile    *file;
  GHashTableIter iter;
  gpointer       key;

  g_hash_table_iter_init (&iter, files);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      file = THUNAR_FILE (key);

      /* we cannot trust thunar_file_is_hidden here;
       * don't know why. Maybe the file has gone through dispose */
      if (g_hash_table_contains (node->hidden_files, file))
        {
          g_hash_table_remove (node->hidden_files, file);

          if (!node->model->show_hidden)
            continue;
        }

      thunar_tree_view_model_dir_remove_file (node, file);
    }
}



static void
_thunar_tree_view_model_dir_notify_loading (Node         *node,
                                            GParamSpec   *spec,
                                            ThunarFolder *dir)
{
  if (!thunar_folder_get_loading (dir) && !node->loaded)
    {
      node->loaded = TRUE;

      if (thunar_tree_view_model_node_has_dummy_child (node))
        thunar_tree_view_model_node_drop_dummy_child (node);

      /* signal model that this dir is done loading */
      thunar_tree_view_model_set_loading (node->model, FALSE);
    }
}



static void
thunar_tree_view_model_load_dir (Node *node)
{
  GHashTable *files;

  _thunar_return_if_fail (node != NULL);

  if (G_UNLIKELY (node->file == NULL || !thunar_file_is_directory (node->file)))
    return;

  if (node->loaded)
    return;

  thunar_tree_view_model_set_loading (node->model, TRUE);

  node->dir = thunar_folder_get_for_file (node->file);
  THUNAR_WARN_VOID_RETURN (node->dir == NULL);

  g_signal_connect_swapped (G_OBJECT (node->dir), "destroy", G_CALLBACK (_thunar_tree_view_model_folder_destroy), node);
  g_signal_connect_swapped (G_OBJECT (node->dir), "error", G_CALLBACK (_thunar_tree_view_model_folder_error), node);
  g_signal_connect_swapped (G_OBJECT (node->dir), "files-added", G_CALLBACK (_thunar_tree_view_model_dir_files_added), node);
  g_signal_connect_swapped (G_OBJECT (node->dir), "files-removed", G_CALLBACK (_thunar_tree_view_model_dir_files_removed), node);
  g_signal_connect_swapped (G_OBJECT (node->dir), "files-changed", G_CALLBACK (thunar_tree_view_model_dir_files_changed), node);
  g_signal_connect_swapped (G_OBJECT (node->dir), "notify::loading", G_CALLBACK (_thunar_tree_view_model_dir_notify_loading), node);

  files = thunar_folder_get_files (node->dir);
  if (files != NULL)
    _thunar_tree_view_model_dir_files_added (node, files);

  /* If the folder is already loaded, directly update the loading state */
  if (!thunar_folder_get_loading (node->dir))
    _thunar_tree_view_model_dir_notify_loading (node, NULL, node->dir);

  g_hash_table_insert (node->model->subdirs, node->file, node);
}



static gboolean
_thunar_tree_view_model_cleanup_idle (Node *node)
{
  thunar_tree_view_model_node_destroy (node);

  return G_SOURCE_REMOVE;
}



static void
thunar_tree_view_model_cleanup_model (ThunarTreeViewModel *model)
{
  Node  *_node;
  GList *values;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  if (model->root == NULL)
    /* we have nothing to cleanup */
    return;

  /* since we are doing a lazy cleanup we should make sure
   to disconnect from the signal handlers of the folders */
  values = g_hash_table_get_values (model->subdirs);
  for (GList *dirs = values; dirs != NULL; dirs = dirs->next)
    {
      _node = dirs->data;

      THUNAR_WARN_VOID_RETURN (_node == NULL || _node->dir == NULL);

      g_signal_handlers_disconnect_by_data (G_OBJECT (_node->dir), _node);
      g_object_unref (_node->dir);
      _node->dir = NULL;
    }
  g_list_free (values);

  /* clean up the nodes in the background*/
  g_idle_add ((GSourceFunc) _thunar_tree_view_model_cleanup_idle, model->root);
  model->root = NULL;
}



static void
thunar_tree_view_model_file_count_callback (ExoJob              *job,
                                            ThunarTreeViewModel *model)
{
  GArray     *param_values;
  ThunarFile *file;
  ThunarFile *parent;
  GHashTable *files;
  Node       *parent_node;

  if (job == NULL)
    return;

  param_values = thunar_simple_job_get_param_values (THUNAR_SIMPLE_JOB (job));
  file = THUNAR_FILE (g_value_get_object (&g_array_index (param_values, GValue, 0)));

  if (file == NULL)
    return;

  parent = thunar_file_get_parent (file, NULL);
  if (parent == NULL)
    return;

  parent_node = thunar_tree_view_model_locate_file (model, parent);
  g_object_unref (parent);

  if (parent_node == NULL)
    return;

  files = g_hash_table_new (g_direct_hash, NULL);
  g_hash_table_add (files, file);
  thunar_tree_view_model_dir_files_changed (parent_node, files);
  g_hash_table_destroy (files);
}



static void
thunar_tree_view_model_node_destroy (Node *node)
{
  GSequenceIter *iter;

  /* stop possible file-watch */
  if (node->file != NULL && node->file_watch_active)
    {
      g_signal_handlers_disconnect_by_data (node->file, node);
      thunar_file_unwatch (node->file);
    }

  if (node->scheduled_unload_id != 0)
    {
      g_source_remove (node->scheduled_unload_id);
      node->scheduled_unload_id = 0;
    }

  if (node->file == NULL)
    {
      /* we have found a dummy node */
      g_free (node);
      return;
    }

  THUNAR_WARN_VOID_RETURN (node->file == NULL);

  if (node->dir != NULL)
    {
      g_signal_handlers_disconnect_by_data (G_OBJECT (node->dir), node);

      THUNAR_WARN_VOID_RETURN (!g_hash_table_contains (node->model->subdirs, node->file));

      g_hash_table_remove (node->model->subdirs, node->file);
      g_object_unref (node->dir);
    }

  while (node->n_children > 0)
    {
      iter = g_sequence_get_iter_at_pos (node->children, 0);
      thunar_tree_view_model_node_destroy (g_sequence_get (iter));
      g_sequence_remove (iter);
      node->n_children--;
    }

  g_sequence_free (node->children);
  g_hash_table_destroy (node->set);
  g_hash_table_destroy (node->hidden_files);

  g_object_unref (node->file);
  g_free (node);
}



static void
thunar_tree_view_model_dir_files_changed (Node       *node_parent,
                                          GHashTable *files)
{
  ThunarTreeViewModel *model = node_parent->model;
  GSequenceIter       *iter;
  GtkTreeIter          tree_iter;
  GHashTableIter       files_iter;
  GtkTreePath         *path;
  Node                *node;
  gint                 pos_after, pos_before;
  gint                *new_order;
  gint                 length;
  gboolean             dummy_added = FALSE;
  ThunarFile          *file;
  gpointer             key;

  g_hash_table_iter_init (&files_iter, files);
  while (g_hash_table_iter_next (&files_iter, &key, NULL))
    {
      file = THUNAR_FILE (key);

      /* two cases - file is hidden or not
       * 1. if it is in hidden list but not hidden anymore then add the new file
       * 2. if it is not hidden but has turned hidden hide it */
      if (g_hash_table_contains (node_parent->hidden_files, file) && !thunar_file_is_hidden (file))
        {
          g_hash_table_remove (node_parent->hidden_files, file);
          thunar_tree_view_model_dir_add_file (node_parent, file);
          continue;
        }

      /* file is now hidden but still in the visible list */
      if (g_hash_table_contains (node_parent->set, file) && thunar_file_is_hidden (file))
        {
          if (!model->show_hidden)
            thunar_tree_view_model_dir_remove_file (node_parent, file);
          g_hash_table_add (node_parent->hidden_files, g_object_ref (file));
          continue;
        }

      node = thunar_tree_view_model_locate_file (model, file);
      /* we dont have the file */
      if (node == NULL)
        continue;

      iter = node->ptr;

      pos_before = g_sequence_iter_get_position (iter);
      g_sequence_sort_changed (iter, thunar_tree_view_model_cmp_nodes, model);
      pos_after = g_sequence_iter_get_position (iter);

      if (pos_before != pos_after)
        {
          length = node_parent->n_children;
          new_order = g_malloc_n (length, sizeof (gint));

          /* new_order[newpos] = oldpos */
          for (gint i = 0, j = 0; i < length; ++i)
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

          if (node_parent->ptr != NULL)
            {
              GTK_TREE_ITER_INIT (tree_iter, model->stamp, node_parent->ptr);
              path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &tree_iter);
              gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, &tree_iter, new_order);
              gtk_tree_path_free (path);
            }
          else
            {
              path = gtk_tree_path_new_first ();
              gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, new_order);
              gtk_tree_path_free (path);
            }

          g_free (new_order);
        }

      if (thunar_file_is_directory (file)
          && !thunar_file_is_empty_directory (file)
          && !node->loaded
          && !thunar_tree_view_model_node_has_dummy_child (node))
        {
          dummy_added = TRUE;
          thunar_tree_view_model_node_add_dummy_child (node);
        }

      GTK_TREE_ITER_INIT (tree_iter, model->stamp, iter);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &tree_iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &tree_iter);
      if (dummy_added)
        gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &tree_iter);
      gtk_tree_path_free (path);
    } /* end for all files */
}



static void
thunar_tree_view_model_set_loading (ThunarTreeViewModel *model,
                                    gboolean             loading)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));

  if (model->loading == loading)
    return;

  model->loading = loading;

  g_object_notify (G_OBJECT (model), "loading");
}



void
thunar_tree_view_model_load_subdir (ThunarTreeViewModel *model,
                                    GtkTreeIter         *iter)
{
  Node *node;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));
  _thunar_return_if_fail (iter->stamp == model->stamp);
  _thunar_return_if_fail (iter->user_data != NULL);

  THUNAR_WARN_VOID_RETURN (g_sequence_iter_is_end (iter->user_data));

  node = g_sequence_get (iter->user_data);
  THUNAR_WARN_VOID_RETURN (node == NULL);

  node->expanded = TRUE;
  if (node->scheduled_unload_id != 0)
    g_source_remove (node->scheduled_unload_id);
  node->scheduled_unload_id = 0;
  thunar_tree_view_model_load_dir (node);
}



static gboolean
_thunar_tree_view_model_dir_unload_timeout (Node *node)
{
  GSequenceIter *iter;
  GtkTreeIter    tree_iter;
  GtkTreePath   *path;
  Node          *_node;

  node->loaded = FALSE;

  if (node->dir != NULL)
    {
      g_signal_handlers_disconnect_by_data (G_OBJECT (node->dir), node);
      g_object_unref (node->dir);
      node->dir = NULL;
    }

  GTK_TREE_ITER_INIT (tree_iter, node->model->stamp, node->ptr);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (node->model), &tree_iter);
  gtk_tree_path_down (path);

  while (node->n_children > 0)
    {
      iter = g_sequence_get_iter_at_pos (node->children, 0);
      _node = g_sequence_get (iter);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (node->model), path);
      thunar_tree_view_model_node_destroy (_node);
      g_sequence_remove (iter);
      node->n_children--;
    }

  gtk_tree_path_free (path);

  g_hash_table_remove_all (node->hidden_files);
  g_hash_table_remove_all (node->set);

  thunar_tree_view_model_node_add_dummy_child (node);

  g_hash_table_remove (node->model->subdirs, node->file);

  return G_SOURCE_REMOVE;
}



static void
_thunar_tree_view_model_dir_unload_timeout_destroy (Node *node)
{
  node->scheduled_unload_id = 0;
}



void
thunar_tree_view_model_schedule_unload (ThunarTreeViewModel *model,
                                        GtkTreeIter         *iter)
{
  Node *node;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));
  _thunar_return_if_fail (iter->stamp == model->stamp);
  _thunar_return_if_fail (iter->user_data != NULL);

  THUNAR_WARN_VOID_RETURN (g_sequence_iter_is_end (iter->user_data));

  node = g_sequence_get (iter->user_data);
  THUNAR_WARN_VOID_RETURN (node == NULL);

  node->expanded = FALSE;
  node->scheduled_unload_id = g_timeout_add_full (G_PRIORITY_LOW, CLEANUP_AFTER_COLLAPSE_DELAY,
                                                  G_SOURCE_FUNC (_thunar_tree_view_model_dir_unload_timeout),
                                                  node, (GDestroyNotify) _thunar_tree_view_model_dir_unload_timeout_destroy);
}



static gboolean
_thunar_tree_view_model_matches_search_terms (ThunarTreeViewModel *model,
                                              ThunarFile          *file)
{
  gboolean matched;
  gchar   *name_n;

  name_n = (gchar *) thunar_file_get_display_name (file);
  if (name_n == NULL)
    return FALSE;

  name_n = thunar_g_utf8_normalize_for_search (name_n, TRUE, TRUE);
  matched = thunar_util_search_terms_match (model->search_terms, name_n);
  g_free (name_n);

  return matched;
}



static void
_thunar_tree_view_model_search_file_destroyed (Node *node, ThunarFile *file)
{
  GHashTable *files = g_hash_table_new (g_direct_hash, NULL);

  g_hash_table_add (files, file);
  _thunar_tree_view_model_dir_files_removed (node->model->root, files);
  g_hash_table_destroy (files);
}



static void
_thunar_tree_view_model_search_file_changed (Node *node, ThunarFile *file)
{
  GHashTable *files = g_hash_table_new (g_direct_hash, NULL);

  g_hash_table_add (files, file);

  if (_thunar_tree_view_model_matches_search_terms (node->model, node->file) == TRUE)
    thunar_tree_view_model_dir_files_changed (node->model->root, files);
  else
    _thunar_tree_view_model_dir_files_removed (node->model->root, files);

  g_hash_table_destroy (files);
}



static gboolean
thunar_tree_view_model_update_search_files (ThunarTreeViewModel *model)
{
  ThunarFile *file;

  g_mutex_lock (&model->mutex_add_search_files);

  for (GList *lp = model->search_files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);
      if (THUNAR_IS_FILE (file) == FALSE)
        {
          g_warning ("failed to add file to search results");
          continue;
        }

      /* take a reference on that file */
      g_object_ref (file);

      if (_thunar_tree_view_model_matches_search_terms (model, file))
        {
          Node *node;
          node = thunar_tree_view_model_dir_add_file (model->root, lp->data);
          if (node != NULL)
            {
              /* start watching the file */
              g_signal_connect_swapped (file, "destroy", G_CALLBACK (_thunar_tree_view_model_search_file_destroyed), node);
              g_signal_connect_swapped (file, "changed", G_CALLBACK (_thunar_tree_view_model_search_file_changed), node);
              thunar_file_watch (file);
              node->file_watch_active = TRUE;
            }
        }

      g_object_unref (file);
    }

  if (model->search_files != NULL)
    g_list_free (model->search_files);
  model->search_files = NULL;
  g_mutex_unlock (&model->mutex_add_search_files);

  return TRUE;
}



static void
thunar_tree_view_model_add_search_files (ThunarStandardViewModel *model,
                                         GList                   *files)
{
  ThunarTreeViewModel *_model = THUNAR_TREE_VIEW_MODEL (model);

  g_mutex_lock (&_model->mutex_add_search_files);

  _model->search_files = g_list_concat (_model->search_files, files);

  g_mutex_unlock (&_model->mutex_add_search_files);
}
