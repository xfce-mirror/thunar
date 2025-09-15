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

#ifndef __THUNAR_TREE_VIEW_MODEL_H__
#define __THUNAR_TREE_VIEW_MODEL_H__

#include "thunar/thunar-folder.h"
#include "thunar/thunar-job.h"

G_BEGIN_DECLS;

typedef enum ThunarTreeViewModelSearch
{
  THUNAR_TREE_VIEW_MODEL_SEARCH_RECURSIVE,
  THUNAR_TREE_VIEW_MODEL_SEARCH_NON_RECURSIVE
} ThunarTreeViewModelSearch;

/* Signal identifiers */
typedef enum ThunarTreeViewModelSignals
{
  THUNAR_TREE_VIEW_MODEL_ERROR,
  THUNAR_TREE_VIEW_MODEL_SEARCH_DONE,
  THUNAR_TREE_VIEW_MODEL_LAST_SIGNAL,
} ThunarTreeViewModelSignals;

typedef gint (*ThunarSortFunc) (const ThunarFile *a,
                                const ThunarFile *b,
                                gboolean          case_sensitive);


typedef struct _ThunarTreeViewModelClass ThunarTreeViewModelClass;
typedef struct _ThunarTreeViewModel      ThunarTreeViewModel;

#define THUNAR_TYPE_TREE_VIEW_MODEL (thunar_tree_view_model_get_type ())
#define THUNAR_TREE_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModel))
#define THUNAR_TREE_VIEW_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModelClass))
#define THUNAR_IS_TREE_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TREE_VIEW_MODEL))
#define THUNAR_IS_TREE_VIEW_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TREE_VIEW_MODEL))
#define THUNAR_TREE_VIEW_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModelClass))

GType
thunar_tree_view_model_get_type (void) G_GNUC_CONST;

ThunarTreeViewModel *
thunar_tree_view_model_new (void);

ThunarFolder *
thunar_tree_view_model_get_folder (ThunarTreeViewModel *model);
void
thunar_tree_view_model_set_folder (ThunarTreeViewModel *model,
                                   ThunarFolder        *folder,
                                   gchar               *search_query);

void
thunar_tree_view_model_set_folders_first (ThunarTreeViewModel *model,
                                          gboolean             folders_first);
void
thunar_tree_view_model_set_hidden_last (ThunarTreeViewModel *model,
                                        gboolean             hidden_last);
gboolean
thunar_tree_view_model_get_show_hidden (ThunarTreeViewModel *model);
void
thunar_tree_view_model_set_show_hidden (ThunarTreeViewModel *model,
                                        gboolean             show_hidden);

gboolean
thunar_tree_view_model_get_file_size_binary (ThunarTreeViewModel *model);
void
thunar_tree_view_model_set_file_size_binary (ThunarTreeViewModel *model,
                                             gboolean             file_size_binary);

ThunarFile *
thunar_tree_view_model_get_file (ThunarTreeViewModel *model,
                                 GtkTreeIter         *iter);


GList *
thunar_tree_view_model_get_paths_for_files (ThunarTreeViewModel *model,
                                            GList               *files);
GList *
thunar_tree_view_model_get_paths_for_pattern (ThunarTreeViewModel *model,
                                              const gchar         *pattern,
                                              gboolean             case_sensitive,
                                              gboolean             match_diacritics);
ThunarJob *
thunar_tree_view_model_get_job (ThunarTreeViewModel *model);
void
thunar_tree_view_model_set_job (ThunarTreeViewModel *model,
                                ThunarJob           *job);
void
thunar_tree_view_model_add_search_files (ThunarTreeViewModel *model,
                                         GList               *files);

void
thunar_tree_view_model_load_subdir (ThunarTreeViewModel *model,
                                    GtkTreeIter         *iter);
void
thunar_tree_view_model_schedule_unload (ThunarTreeViewModel *model,
                                        GtkTreeIter         *iter);
void
thunar_tree_view_model_check_file_in_model_before_use (ThunarTreeViewModel *model)

G_END_DECLS;

#endif /* !__THUNAR_TREE_VIEW_MODEL_H__ */
