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
#include <thunar/thunar-file.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-tree-view-model.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-util.h>



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



static void              thunar_tree_view_model_drag_dest_init (GtkTreeDragDestIface *iface);
static void              thunar_tree_view_model_sortable_init (GtkTreeSortableIface *iface);
static void              thunar_tree_view_model_tree_model_init (GtkTreeModelIface *iface);
static void              thunar_tree_view_model_standard_view_model_init (ThunarStandardViewModelIface *iface);


/*************************************************
 * GObject Virtual Methods
 *************************************************/
static void              thunar_tree_view_model_dispose (GObject *object);
static void              thunar_tree_view_model_finalize (GObject *object);
static void              thunar_tree_view_model_get_property (GObject    *object,
                                                              guint       prop_id,
                                                              GValue     *value,
                                                              GParamSpec *pspec);
static void              thunar_tree_view_model_set_property (GObject      *object,
                                                              guint         prop_id,
                                                              const GValue *value,
                                                              GParamSpec   *pspec);


/*************************************************
 * GtkTreeDragDest Virtual Methods
 *************************************************/
static gboolean          thunar_tree_view_model_drag_data_received (GtkTreeDragDest  *dest,
                                                                    GtkTreePath      *path,
                                                                    GtkSelectionData *data);
static gboolean          thunar_tree_view_model_row_drop_possible (GtkTreeDragDest  *dest,
                                                                   GtkTreePath      *path,
                                                                   GtkSelectionData *data);


/*************************************************
 * GtkSortable Virtual Methods
 *************************************************/
static gboolean          thunar_tree_view_model_get_sort_column_id (GtkTreeSortable *sortable,
                                                                    gint            *sort_column_id,
                                                                    GtkSortType     *order);
static void              thunar_tree_view_model_set_sort_column_id (GtkTreeSortable *sortable,
                                                                    gint             sort_column_id,
                                                                    GtkSortType      order);
static void              thunar_tree_view_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                                                       GtkTreeIterCompareFunc func,
                                                                       gpointer               data,
                                                                       GDestroyNotify         destroy);
static void              thunar_tree_view_model_set_sort_func (GtkTreeSortable       *sortable,
                                                               gint                   sort_column_id,
                                                               GtkTreeIterCompareFunc func,
                                                               gpointer               data,
                                                               GDestroyNotify         destroy);
static gboolean          thunar_tree_view_model_has_default_sort_func (GtkTreeSortable *sortable);


/*************************************************
 * GtkTreeModel Virtual Methods
 *************************************************/
static GtkTreeModelFlags thunar_tree_view_model_get_flags (GtkTreeModel *model);
static gint              thunar_tree_view_model_get_n_columns (GtkTreeModel *model);
static GType             thunar_tree_view_model_get_column_type (GtkTreeModel *model,
                                                                 gint          idx);
static gboolean          thunar_tree_view_model_get_iter (GtkTreeModel *model,
                                                          GtkTreeIter  *iter,
                                                          GtkTreePath  *path);
static GtkTreePath      *thunar_tree_view_model_get_path (GtkTreeModel *model,
                                                          GtkTreeIter  *iter);
static void              thunar_tree_view_model_get_value (GtkTreeModel *model,
                                                           GtkTreeIter  *iter,
                                                           gint          column,
                                                           GValue       *value);
static gboolean          thunar_tree_view_model_iter_next (GtkTreeModel *model,
                                                           GtkTreeIter  *iter);
static gboolean          thunar_tree_view_model_iter_children (GtkTreeModel *model,
                                                               GtkTreeIter  *iter,
                                                               GtkTreeIter  *parent);
static gboolean          thunar_tree_view_model_iter_has_child (GtkTreeModel *model,
                                                                GtkTreeIter  *iter);
static gint              thunar_tree_view_model_iter_n_children (GtkTreeModel *model,
                                                                 GtkTreeIter  *iter);
static gboolean          thunar_tree_view_model_iter_nth_child (GtkTreeModel *model,
                                                                GtkTreeIter  *iter,
                                                                GtkTreeIter  *parent,
                                                                gint          n);
static gboolean          thunar_tree_view_model_iter_parent (GtkTreeModel *model,
                                                             GtkTreeIter  *iter,
                                                             GtkTreeIter  *child);
static void              thunar_tree_view_model_ref_node (GtkTreeModel *tree_model,
                                                          GtkTreeIter  *iter);
static void              thunar_tree_view_model_unref_node (GtkTreeModel *tree_model,
                                                            GtkTreeIter  *iter);


/*************************************************
 * ThunarStandardViewModel Virtual Methods
 *************************************************/
/* Getters */
static ThunarFolder     *thunar_tree_view_model_get_folder (ThunarStandardViewModel *model);
static gboolean          thunar_tree_view_model_get_show_hidden (ThunarStandardViewModel *model);
static gboolean          thunar_tree_view_model_get_file_size_binary (ThunarStandardViewModel *model);
static ThunarFile       *thunar_tree_view_model_get_file (ThunarStandardViewModel *model,
                                                          GtkTreeIter             *iter);
static GList            *thunar_tree_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                                                     GList                   *files);
static GList            *thunar_tree_view_model_get_paths_for_pattern (ThunarStandardViewModel *model,
                                                                       const gchar             *pattern,
                                                                       gboolean                 case_sensitive,
                                                                       gboolean                 match_diacritics);
static gchar            *thunar_tree_view_model_get_statusbar_text (ThunarStandardViewModel *model,
                                                                    GList                   *selected_items);
static ThunarJob        *thunar_tree_view_model_get_job (ThunarStandardViewModel *model);

/* Setters */
static void              thunar_tree_view_model_set_folder (ThunarStandardViewModel *model,
                                                            ThunarFolder            *folder,
                                                            gchar                   *search_query);
static void              thunar_tree_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                                                 gboolean                 show_hidden);
static void              thunar_tree_view_model_set_folders_first (ThunarStandardViewModel *model,
                                                                   gboolean                 folders_first);
static void              thunar_tree_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                                                      gboolean                 file_size_binary);
static void              thunar_tree_view_model_set_job (ThunarStandardViewModel *model,
                                                         ThunarJob               *job);


/*************************************************
 * Private Methods
 *************************************************/
/* Property Getters */
static gboolean          thunar_tree_view_model_get_case_sensitive (ThunarTreeViewModel *model);
static ThunarDateStyle   thunar_tree_view_model_get_date_style (ThunarTreeViewModel *model);
static const char       *thunar_tree_view_model_get_date_custom_style (ThunarTreeViewModel *model);
static gint              thunar_tree_view_model_get_num_files (ThunarTreeViewModel *model);
static gboolean          thunar_tree_view_model_get_folders_first (ThunarTreeViewModel *model);
static gint              thunar_tree_view_model_get_folder_item_count (ThunarTreeViewModel *model);

/* Property Setters */
static void              thunar_tree_view_model_set_case_sensitive (ThunarTreeViewModel *model,
                                                                    gboolean             case_sensitive);
static void              thunar_tree_view_model_set_date_style (ThunarTreeViewModel *model,
                                                                ThunarDateStyle      date_style);
static void              thunar_tree_view_model_set_date_custom_style (ThunarTreeViewModel *model,
                                                                       const char          *date_custom_style);
static void              thunar_tree_view_model_set_folder_item_count (ThunarTreeViewModel  *model,
                                                                       ThunarFolderItemCount count_as_dir_size);

/* Internal helper funcs */
static void              thunar_tree_view_model_load_dir (ThunarTreeViewModel *model,
                                                          Node                *node);
static void              thunar_tree_view_model_cleanup (ThunarTreeViewModel *model,
                                                         gboolean             async);


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

  Node                 *root;
  ThunarFolder         *dir;

  gboolean              sort_case_sensitive : 1;
  gboolean              sort_folders_first  : 1;
  gint                  sort_sign; /* 1 = ascending, -1 descending */
  ThunarSortFunc        sort_func;

  /* Options */
  ThunarFolderItemCount folder_item_count;
  gboolean              file_size_binary : 1;
  ThunarDateStyle       date_style;
  char                 *date_custom_style;
  gboolean              show_hidden;

  gint                  n_visible_files;
};



struct _Node
{
  ThunarFile    *file;
  ThunarFolder  *dir;

  Node          *parent;
  GSequenceIter *ptr; /* self ref */

  gint           depth;
  gint           n_children;
  gint           loaded : 1;

  GSequence     *children; /* Nodes */
};



/*************************************************
 *  GType Definitions
 *************************************************/
G_DEFINE_TYPE_WITH_CODE (ThunarTreeViewModel, thunar_tree_view_model, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, thunar_tree_view_model_drag_dest_init)
  G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE, thunar_tree_view_model_sortable_init)
  G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_tree_view_model_tree_model_init)
  G_IMPLEMENT_INTERFACE (THUNAR_TYPE_STANDARD_VIEW_MODEL, thunar_tree_view_model_standard_view_model_init))



/* Reference to GParamSpec(s) of all the N_PROPERTIES */
static GParamSpec *tree_model_props[N_PROPERTIES] = { NULL, };



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

  /* No need to install signals. Already done by the interface */
}



static void
thunar_tree_view_model_init (ThunarTreeViewModel *model)
{
  /* generate a unique stamp if we're in debug mode */
#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif
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
  iface->get_paths_for_pattern = thunar_tree_view_model_get_paths_for_pattern;
  iface->get_file_size_binary = thunar_tree_view_model_get_file_size_binary;
  iface->set_file_size_binary = thunar_tree_view_model_set_file_size_binary;
  iface->set_folders_first = thunar_tree_view_model_set_folders_first;
  iface->get_statusbar_text = thunar_tree_view_model_get_statusbar_text;
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
  ThunarTreeViewModel *model = THUNAr_tree_view_MODEL (sortable);

  _thunar_return_val_if_fail (THUNAR_IS_LIST_MODEL (model), FALSE);

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
  ThunarTreeViewModel *model = THUNAr_tree_view_MODEL (sortable);

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));

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
  // TODO: Sort the model

  /* notify listining parties */
  gtk_tree_sortable_sort_column_changed (sortable);
}



static void
thunar_tree_view_model_set_default_sort_func (GtkTreeSortable       *sortable,
                                              GtkTreeIterCompareFunc func,
                                              gpointer               data,
                                              GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
}



static void
thunar_tree_view_model_set_sort_func (GtkTreeSortable       *sortable,
                                      gint                   sort_column_id,
                                      GtkTreeIterCompareFunc func,
                                      gpointer               data,
                                      GDestroyNotify         destroy)
{
  g_critical ("ThunarListModel has sorting facilities built-in!");
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
  GSequenceIter       *ptr;
  gint                *indices;
  gint                 depth, loc;
  Node                *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth <= 0, FALSE);

  _model = THUNAR_TREE_VIEW_MODEL (model);

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
  ThunarTreeViewModel *_model;

  GtkTreePath         *path;
  Node                *node;
  gint                *indices;
  gint                 depth;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, NULL);
  _thunar_return_val_if_fail (iter->user_data == NULL, NULL);

  node = g_sequence_get (iter->user_data);

  depth = node->depth;
  indices = g_newa (gint, depth);

  for (gint d = depth - 1; d >= 0; --d)
    {
      indices[d] = g_sequence_iter_get_position (node->ptr);
      node = node->parent;
    }

  return gtk_tree_path_new_from_indicesv (indices, depth);
}



static void
thunar_tree_view_model_get_value (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value)
{
  /* TODO: */
}



static gboolean
thunar_tree_view_model_iter_next (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  GSequenceIter *ptr;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  ptr = iter->user_data;
  ptr = g_sequence_iter_next (ptr);
  return !g_sequence_iter_is_end (ptr);
}



static gboolean
thunar_tree_view_model_iter_children (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent)
{
  GSequenceIter *ptr;
  Node          *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);

  if (parent == NULL)
    /* return iter corresponding to path -> "0"; i.e 1st iter */
    return gtk_tree_model_get_iter_first (model, iter);

  node = g_sequence_get (parent->user_data);
  if (node->children == NULL)
    return FALSE;

  ptr = g_sequence_get_begin_iter (node->children);
  GTK_TREE_ITER_INIT (*iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, ptr);

  return TRUE;
}



static gboolean
thunar_tree_view_model_iter_has_child (GtkTreeModel *model,
                                       GtkTreeIter  *iter)
{
  Node *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  node = g_sequence_get (iter->user_data);
  return node->n_children > 0;
}



static gint
thunar_tree_view_model_iter_n_children (GtkTreeModel *model,
                                        GtkTreeIter  *iter)
{
  Node *node;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model), FALSE);
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
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
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (parent->user_data != NULL, FALSE);

  node = g_sequence_get (parent->user_data);
  if (n >= node->n_children)
    return FALSE;

  ptr = g_sequence_get_iter_at_pos (node->children, n);
  _assert (!g_sequence_iter_is_end (ptr));
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
  _thunar_return_val_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp, FALSE);
  _thunar_return_val_if_fail (child->user_data != NULL, FALSE);

  node = g_sequence_get (child->user_data);
  if (node->depth <= 0)
    return FALSE;

  ptr = node->parent->ptr;
  GTK_TREE_ITER_INIT (*iter, THUNAR_TREE_VIEW_MODEL (model)->stamp, ptr);

  return TRUE;
}



static void
thunar_tree_view_model_ref_node (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  Node *node;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW_MODEL (model));
  _thunar_return_if_fail (iter->stamp != THUNAR_TREE_VIEW_MODEL (model)->stamp);
  _thunar_return_if_fail (iter->user_data != NULL);

  node = g_sequence_get (iter->user_data);
  /* TODO: load the dir */
}



static void
thunar_tree_view_model_unref_node (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  /* DO NOTHING */
}



static ThunarFolder *
thunar_tree_view_model_get_folder (ThunarStandardViewModel *model)
{
  return THUNAR_TREE_VIEW_MODEL (model)->dir;
}



static gboolean
thunar_tree_view_model_get_show_hidden (ThunarStandardViewModel *model)
{
  return THUNAR_TREE_VIEW_MODEL (model)->show_hidden;
}



static gboolean
thunar_tree_view_model_get_file_size_binary (ThunarStandardViewModel *model)
{
  return THUNAR_TREE_VIEW_MODEL (model)->file_size_binary;
}



static ThunarFile *
thunar_tree_view_model_get_file (ThunarStandardViewModel *model,
                                 GtkTreeIter             *iter)
{
  return ((Node *) g_sequence_get (iter->user_data))->file;
}



static GList *
thunar_tree_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                            GList                   *files)
{
  /* TODO: */
  return NULL;
}



static GList *
thunar_tree_view_model_get_paths_for_pattern (ThunarStandardViewModel *model,
                                              const gchar             *pattern,
                                              gboolean                 case_sensitive,
                                              gboolean                 match_diacritics)
{
  /* TODO: */
  return NULL;
}



static gchar *
thunar_tree_view_model_get_statusbar_text (ThunarStandardViewModel *model,
                                           GList                   *selected_items)
{
  /* TODO: */
  return NULL;
}



static ThunarJob *
thunar_tree_view_model_get_job (ThunarStandardViewModel *model)
{
  /* TODO: */
  return NULL;
}



static void
thunar_tree_view_model_set_folder (ThunarStandardViewModel *model,
                                   ThunarFolder            *folder,
                                   gchar                   *search_query)
{
  ThunarTreeViewModel *_model;

  _model = THUNAR_TREE_VIEW_MODEL (model);

  /* idle free & release */
  thunar_tree_view_model_cleanup (_model, TRUE);

  _model->dir = folder;

  thunar_tree_view_model_load_dir (_model, _model->root);
}



static void
thunar_tree_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                        gboolean                 show_hidden)
{
  /* TODO: */
}



static void
thunar_tree_view_model_set_folders_first (ThunarStandardViewModel *model,
                                          gboolean                 folders_first)
{
  /* TODO */
}



static void
thunar_tree_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                             gboolean                 file_size_binary)
{
  /* TODO */
}



static void
thunar_tree_view_model_set_job (ThunarStandardViewModel *model,
                                ThunarJob               *job)
{
  /* TODO */
}



static gboolean
thunar_tree_view_model_get_case_sensitive (ThunarTreeViewModel *model)
{
  return model->sort_case_sensitive;
}



static ThunarDateStyle
thunar_tree_view_model_get_date_style (ThunarTreeViewModel *model)
{
  return model->date_style;
}



static const char *
thunar_tree_view_model_get_date_custom_style (ThunarTreeViewModel *model)
{
  return model->date_custom_style;
}



static gint
thunar_tree_view_model_get_num_files (ThunarTreeViewModel *model)
{
  return model->n_visible_files;
}



static gboolean
thunar_tree_view_model_get_folders_first (ThunarTreeViewModel *model)
{
  return model->sort_folders_first;
}



static gint
thunar_tree_view_model_get_folder_item_count (ThunarTreeViewModel *model)
{
  return model->folder_item_count;
}



static void
thunar_tree_view_model_load_dir (ThunarTreeViewModel *model,
                                 Node                *node)
{
}
