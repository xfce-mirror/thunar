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
#include "config.h"
#endif

#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-standard-view-model.h"
#include "thunar/thunar-util.h"

static void
thunar_standard_view_model_class_init (gpointer klass);

GType
thunar_standard_view_model_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_ ("ThunarStandardViewModel"),
                                            sizeof (ThunarStandardViewModelIface),
                                            (GClassInitFunc) (void (*) (void)) thunar_standard_view_model_class_init,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, GTK_TYPE_TREE_MODEL);
      g_type_interface_add_prerequisite (type, GTK_TYPE_TREE_DRAG_DEST);
      g_type_interface_add_prerequisite (type, GTK_TYPE_TREE_SORTABLE);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}

struct _MatchForeach
{
  GList        *paths;
  GPatternSpec *pspec;
  gboolean      match_diacritics;
  gboolean      case_sensitive;
};

static guint model_signals[THUNAR_STANDARD_VIEW_MODEL_LAST_SIGNAL];

static void
thunar_standard_view_model_class_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /**
       * ThunarStandardViewModel:case-sensitive:
       *
       * Tells whether the sorting should be case sensitive.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_boolean ("case-sensitive",
                                                                 "case-sensitive",
                                                                 "case-sensitive",
                                                                 TRUE,
                                                                 EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel:date-style:
       *
       * The style used to format dates.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_enum ("date-style",
                                                              "date-style",
                                                              "date-style",
                                                              THUNAR_TYPE_DATE_STYLE,
                                                              THUNAR_DATE_STYLE_SIMPLE,
                                                              EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel:date-custom-style:
       *
       * The style used for custom format of dates.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_string ("date-custom-style",
                                                                "DateCustomStyle",
                                                                NULL,
                                                                "%Y-%m-%d %H:%M:%S",
                                                                EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel:folder:
       *
       * The folder presented by this #ThunarStandardViewModel.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_object ("folder",
                                                                "folder",
                                                                "folder",
                                                                THUNAR_TYPE_FOLDER,
                                                                EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel::folders-first:
       *
       * Tells whether to always sort folders before other files.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_boolean ("folders-first",
                                                                 "folders-first",
                                                                 "folders-first",
                                                                 TRUE,
                                                                 EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel::num-files:
       *
       * The number of files in the folder presented by this #ThunarStandardViewModel.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_uint ("num-files",
                                                              "num-files",
                                                              "num-files",
                                                              0, G_MAXUINT, 0,
                                                              EXO_PARAM_READABLE));

      /**
       * ThunarStandardViewModel::show-hidden:
       *
       * Tells whether to include hidden (and backup) files.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_boolean ("show-hidden",
                                                                 "show-hidden",
                                                                 "show-hidden",
                                                                 FALSE,
                                                                 EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel::misc-file-size-binary:
       *
       * Tells whether to format file size in binary.
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_boolean ("file-size-binary",
                                                                 "file-size-binary",
                                                                 "file-size-binary",
                                                                 TRUE,
                                                                 EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel:folder-item-count:
       *
       * Tells when the size column of folders should show the number of containing files
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_enum ("folder-item-count",
                                                              "folder-item-count",
                                                              "folder-item-count",
                                                              THUNAR_TYPE_FOLDER_ITEM_COUNT,
                                                              TRUE,
                                                              EXO_PARAM_READWRITE));

      /**
       * ThunarStandardViewModel:loading:
       *
       * Tells if the model is yet loading a folder
       **/
      g_object_interface_install_property (klass,
                                           g_param_spec_boolean ("loading",
                                                                 "loading",
                                                                 "loading",
                                                                 FALSE,
                                                                 EXO_PARAM_READABLE));

      /**
       * ThunarStandardViewModel::error:
       * @store : a #ThunarStandardViewModel.
       * @error : a #GError that describes the problem.
       *
       * Emitted when an error occurs while loading the
       * @store content.
       **/
      model_signals[THUNAR_STANDARD_VIEW_MODEL_ERROR] =
      g_signal_new (I_ ("error"),
                    G_TYPE_FROM_INTERFACE (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarStandardViewModelIface, error),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

      /**
       * ThunarStandardViewModel::search-done:
       * @store : a #ThunarStandardViewModel.
       * @error : a #GError that describes the problem.
       *
       * Emitted when a recursive search finishes.
       **/
      model_signals[THUNAR_STANDARD_VIEW_MODEL_SEARCH_DONE] =
      g_signal_new (I_ ("search-done"),
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarStandardViewModelIface, search_done),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);
    }
}



// TODO: Make more detailed documentation for this.

/**
 * thunar_standard_view_model_get_folder:
 * @store : a valid #ThunarStandardViewModel object.
 *
 * Return value: the #ThunarFolder @store is associated with
 *               or %NULL if @store has no folder.
 **/
ThunarFolder *
thunar_standard_view_model_get_folder (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), NULL);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_folder) (model);
}



/**
 * thunar_standard_view_model_set_folder:
 * @store                       : a valid #ThunarStandardViewModel.
 * @folder                      : a #ThunarFolder or %NULL.
 * @search_query                : a #string or %NULL.
 **/
void
thunar_standard_view_model_set_folder (ThunarStandardViewModel *model,
                                       ThunarFolder            *folder,
                                       gchar                   *search_query)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  _thunar_return_if_fail (folder == NULL || THUNAR_IS_FOLDER (folder));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->set_folder) (model, folder, search_query);
}



/**
 * thunar_standard_view_model_set_folders_first:
 * @store         : a #ThunarStandardViewModel.
 * @folders_first : %TRUE to let @store list folders first.
 **/
void
thunar_standard_view_model_set_folders_first (ThunarStandardViewModel *model,
                                              gboolean                 folders_first)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->set_folders_first) (model, folders_first);
}



/**
 * thunar_standard_view_model_get_show_hidden:
 * @store : a #ThunarStandardViewModel.
 *
 * Return value: %TRUE if hidden files will be shown, else %FALSE.
 **/
gboolean
thunar_standard_view_model_get_show_hidden (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), FALSE);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_show_hidden) (model);
}



/**
 * thunar_standard_view_model_set_show_hidden:
 * @store       : a #ThunarStandardViewModel.
 * @show_hidden : %TRUE if hidden files should be shown, else %FALSE.
 **/
void
thunar_standard_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                            gboolean                 show_hidden)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->set_show_hidden) (model, show_hidden);
}



/**
 * thunar_standard_view_model_get_file_size_binary:
 * @store : a valid #ThunarStandardViewModel object.
 *
 * Returns %TRUE if the file size should be formatted
 * as binary.
 *
 * Return value: %TRUE if file size format is binary.
 **/
gboolean
thunar_standard_view_model_get_file_size_binary (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), FALSE);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_file_size_binary) (model);
}



/**
 * thunar_standard_view_model_set_file_size_binary:
 * @store            : a valid #ThunarStandardViewModel object.
 * @file_size_binary : %TRUE to format file size as binary.
 *
 * If @file_size_binary is %TRUE the file size should be
 * formatted as binary.
 **/
void
thunar_standard_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                                 gboolean                 file_size_binary)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->set_file_size_binary) (model, file_size_binary);
}



/**
 * thunar_standard_view_model_get_file:
 * @store : a #ThunarStandardViewModel.
 * @iter  : a valid #GtkTreeIter for @store.
 *
 * Returns the #ThunarFile referred to by @iter. Free
 * the returned object using #g_object_unref() when
 * you are done with it.
 *
 * Return value: the #ThunarFile.
 **/
ThunarFile *
thunar_standard_view_model_get_file (ThunarStandardViewModel *model,
                                     GtkTreeIter             *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), NULL);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_file) (model, iter);
}



/**
 * thunar_standard_view_model_get_paths_for_files:
 * @store : a #ThunarStandardViewModel instance.
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
GList *
thunar_standard_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                                GList                   *files)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), NULL);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_paths_for_files) (model, files);
}



ThunarJob *
thunar_standard_view_model_get_job (ThunarStandardViewModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), NULL);
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->get_job) (model);
}



void
thunar_standard_view_model_set_job (ThunarStandardViewModel *model,
                                    ThunarJob               *job)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->set_job) (model, job);
}



void
thunar_standard_view_model_add_search_files (ThunarStandardViewModel *model,
                                             GList                   *files)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model));
  return (*THUNAR_STANDARD_VIEW_MODEL_GET_IFACE (model)->add_search_files) (model, files);
}



static gboolean
_thunar_standard_view_model_match_pattern_foreach (GtkTreeModel *model,
                                                   GtkTreePath  *path,
                                                   GtkTreeIter  *iter,
                                                   gpointer      data)
{
  ThunarFile           *file;
  const gchar          *display_name;
  gchar                *normalized_display_name;
  gboolean              name_matched;
  struct _MatchForeach *mf = (struct _MatchForeach *) data;

  file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), iter);
  if (file == NULL)
    return FALSE;

  display_name = thunar_file_get_display_name (file);
  if (display_name == NULL)
    return FALSE;

  normalized_display_name = thunar_g_utf8_normalize_for_search (display_name,
                                                                !mf->match_diacritics,
                                                                !mf->case_sensitive);
  name_matched = g_pattern_spec_match_string (mf->pspec, normalized_display_name);
  g_free (normalized_display_name);

  if (name_matched)
    mf->paths = g_list_prepend (mf->paths, gtk_tree_path_copy (path));

  return FALSE;
}



/**
 * thunar_standard_view_model_get_paths_for_pattern:
 * @store          : a #ThunarStandardViewModel instance.
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
GList *
thunar_standard_view_model_get_paths_for_pattern (ThunarStandardViewModel *model,
                                                  const gchar             *pattern,
                                                  gboolean                 case_sensitive,
                                                  gboolean                 match_diacritics)
{
  struct _MatchForeach mf;
  gchar               *normalized_pattern;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW_MODEL (model), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (pattern, -1, NULL), NULL);

  /* compile the pattern */
  normalized_pattern = thunar_g_utf8_normalize_for_search (pattern, !match_diacritics, !case_sensitive);
  mf.pspec = g_pattern_spec_new (normalized_pattern);
  g_free (normalized_pattern);

  mf.paths = NULL;
  mf.case_sensitive = case_sensitive;
  mf.match_diacritics = match_diacritics;

  /* find all rows that match the given pattern */
  gtk_tree_model_foreach (GTK_TREE_MODEL (model),
                          _thunar_standard_view_model_match_pattern_foreach, &mf);

  /* release the pattern */
  g_pattern_spec_free (mf.pspec);

  return mf.paths;
}
