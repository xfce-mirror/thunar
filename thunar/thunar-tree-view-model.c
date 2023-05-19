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

#include <thunar/thunar-tree-view-model.h>
#include <thunar/thunar-standard-view-model.h>

static void               thunar_list_model_standard_view_model_init    (ThunarStandardViewModelIface *iface);
static void               thunar_list_model_tree_model_init             (GtkTreeModelIface            *iface);
static void               thunar_list_model_drag_dest_init              (GtkTreeDragDestIface         *iface);
static void               thunar_list_model_sortable_init               (GtkTreeSortableIface         *iface);
static void               thunar_list_model_dispose                     (GObject                      *object);
static void               thunar_list_model_finalize                    (GObject                      *object);
static void               thunar_list_model_get_property                (GObject                      *object,
                                                                         guint                         prop_id,
                                                                         GValue                       *value,
                                                                         GParamSpec                   *pspec);
static void               thunar_list_model_set_property                (GObject                      *object,
                                                                         guint                         prop_id,
                                                                         const GValue                 *value,
                                                                         GParamSpec                   *pspec);
static ThunarFolder        *thunar_tree_view_model_get_folder             (ThunarStandardViewModel  *model);
static void                 thunar_tree_view_model_set_folder             (ThunarStandardViewModel  *model,
                                                                           ThunarFolder             *folder,
                                                                           gchar                    *search_query);

static void                 thunar_tree_view_model_set_folders_first      (ThunarStandardViewModel  *model,
                                                                           gboolean                  folders_first);

static gboolean             thunar_tree_view_model_get_show_hidden        (ThunarStandardViewModel  *model);
static void                 thunar_tree_view_model_set_show_hidden        (ThunarStandardViewModel  *model,
                                                                           gboolean                  show_hidden);

static gboolean             thunar_tree_view_model_get_file_size_binary   (ThunarStandardViewModel  *model);
static void                 thunar_tree_view_model_set_file_size_binary   (ThunarStandardViewModel  *model,
                                                                           gboolean                  file_size_binary);

static ThunarFile          *thunar_tree_view_model_get_file               (ThunarStandardViewModel  *model,
                                                                           GtkTreeIter              *iter);


static GList               *thunar_tree_view_model_get_paths_for_files    (ThunarStandardViewModel  *model,
                                                                           GList                    *files);
static GList               *thunar_tree_view_model_get_paths_for_pattern  (ThunarStandardViewModel  *model,
                                                                           const gchar              *pattern,
                                                                           gboolean                  case_sensitive,
                                                                           gboolean                  match_diacritics);

static gchar               *thunar_tree_view_model_get_statusbar_text     (ThunarStandardViewModel  *model,
                                                                           GList                    *selected_items);
static ThunarJob           *thunar_tree_view_model_get_job                (ThunarStandardViewModel  *model);
static void                 thunar_tree_view_model_set_job                (ThunarStandardViewModel  *model,
                                                                           ThunarJob                *job);

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
  N_PROPERTIES
};

static GParamSpec *tree_view_model_props[N_PROPERTIES] = { NULL, };

static void
thunar_standard_view_model_class_init (ThunarStandardViewModel *klass)
{
  GObjectClass *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->dispose      = thunar_tree_view_model_dispose;
  gobject_class->finalize     = thunar_tree_view_model_finalize;
  gobject_class->get_property = thunar_tree_view_model_get_property;
  gobject_class->set_property = thunar_tree_view_model_set_property;

  /**
   * ThunarListModel:case-sensitive:
   *
   * Tells whether the sorting should be case sensitive.
   **/
  tree_view_model_props[PROP_CASE_SENSITIVE] =
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
  tree_view_model_props[PROP_DATE_STYLE] =
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
  tree_view_model_props[PROP_DATE_CUSTOM_STYLE] =
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
  tree_view_model_props[PROP_FOLDER] =
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
  tree_view_model_props[PROP_FOLDERS_FIRST] =
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
  tree_view_model_props[PROP_NUM_FILES] =
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
  tree_view_model_props[PROP_SHOW_HIDDEN] =
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
  tree_view_model_props[PROP_FILE_SIZE_BINARY] =
      g_param_spec_boolean ("file-size-binary",
                            "file-size-binary",
                            "file-size-binary",
                            TRUE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarListModel:folder-item-count:
   *
   * Tells when the size column of folders should show the number of containing files
   **/
  tree_view_model_props[PROP_FOLDER_ITEM_COUNT] =
      g_param_spec_enum ("folder-item-count",
                         "folder-item-count",
                         "folder-item-count",
                         THUNAR_TYPE_FOLDER_ITEM_COUNT,
                         TRUE,
                         EXO_PARAM_READWRITE);

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, tree_view_model_props);
}
