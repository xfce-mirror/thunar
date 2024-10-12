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

                         +--------------------------------------------+
                         |                                            |
                         |                                            |
                         |          ThunarStandardViewModel           |
                         |                (Interface)                 |
                         |                                            |
                         +---------------------+----------------------+
                                               |
               +------------------------implemented by-------------------------+
               |                               |                               |
               v                               v                               v
+--------------+------------+   +----------------------------+  +--------------+---------------+
|                           |   |                            |  |                              |
|  ThunarListModel          |   |  ThunarTreeViewModel       |  |                              |
|                           |   |                            |  |                              |
|---------------------------|   |----------------------------|  |                              |
|  + Used by                |   |  + Used by                 |  |                              |
|                           |   |                            |  |   New models for new views   |
|  - IconView               |   |  - DetailsView             |  |   E.g Miller View.           |
|                           |   |                            |  |                              |
|  - CompactView            |   |                            |  |                              |
|                           |   |                            |  |                              |
|  - ThunarPathEntry        |   |                            |  |                              |
|                           |   |                            |  |                              |
+---------------------------+   +----------------------------+  +------------------------------+

 */

#ifndef __THUNAR_STANDARD_VIEW_MODEL__
#define __THUNAR_STANDARD_VIEW_MODEL__

#include "thunar/thunar-folder.h"
#include "thunar/thunar-job.h"

G_BEGIN_DECLS;

typedef struct _ThunarStandardViewModelIface ThunarStandardViewModelIface;
typedef struct _ThunarStandardViewModel      ThunarStandardViewModel;

typedef enum ThunarStandardViewModelSearch
{
  THUNAR_STANDARD_VIEW_MODEL_SEARCH_RECURSIVE,
  THUNAR_STANDARD_VIEW_MODEL_SEARCH_NON_RECURSIVE
} ThunarStandardViewModelSearch;

/* Signal identifiers */
typedef enum ThunarStandardViewModelSignals
{
  THUNAR_STANDARD_VIEW_MODEL_ERROR,
  THUNAR_STANDARD_VIEW_MODEL_SEARCH_DONE,
  THUNAR_STANDARD_VIEW_MODEL_LAST_SIGNAL,
} ThunarStandardViewModelSignals;

typedef gint (*ThunarSortFunc) (const ThunarFile *a,
                                const ThunarFile *b,
                                gboolean          case_sensitive);

#define THUNAR_TYPE_STANDARD_VIEW_MODEL (thunar_standard_view_model_get_type ())
#define THUNAR_STANDARD_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_STANDARD_VIEW_MODEL, ThunarStandardViewModel))
#define THUNAR_IS_STANDARD_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_STANDARD_VIEW_MODEL))
#define THUNAR_STANDARD_VIEW_MODEL_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_STANDARD_VIEW_MODEL, ThunarStandardViewModelIface))

struct _ThunarStandardViewModelIface
{
  GTypeInterface __parent__;
  /* signals */
  void (*error) (ThunarStandardViewModel *model,
                 const GError            *error);
  void (*search_done) (void);

  /* virtual methods */
  ThunarFolder *(*get_folder) (ThunarStandardViewModel *model);
  void (*set_folder) (ThunarStandardViewModel *model,
                      ThunarFolder            *folder,
                      gchar                   *search_query);
  void (*set_folders_first) (ThunarStandardViewModel *model,
                             gboolean                 folders_first);
  gboolean (*get_show_hidden) (ThunarStandardViewModel *model);
  void (*set_show_hidden) (ThunarStandardViewModel *model,
                           gboolean                 show_hidden);
  gboolean (*get_file_size_binary) (ThunarStandardViewModel *model);
  void (*set_file_size_binary) (ThunarStandardViewModel *model,
                                gboolean                 file_size_binary);
  ThunarFile *(*get_file) (ThunarStandardViewModel *model,
                           GtkTreeIter             *iter);
  GList *(*get_paths_for_files) (ThunarStandardViewModel *model,
                                 GList                   *files);
  ThunarJob *(*get_job) (ThunarStandardViewModel *model);
  void (*set_job) (ThunarStandardViewModel *model,
                   ThunarJob               *job);
  void (*add_search_files) (ThunarStandardViewModel *model,
                            GList                   *files);
};

GType
thunar_standard_view_model_get_type (void) G_GNUC_CONST;

ThunarFolder *
thunar_standard_view_model_get_folder (ThunarStandardViewModel *model);
void
thunar_standard_view_model_set_folder (ThunarStandardViewModel *model,
                                       ThunarFolder            *folder,
                                       gchar                   *search_query);

void
thunar_standard_view_model_set_folders_first (ThunarStandardViewModel *model,
                                              gboolean                 folders_first);

gboolean
thunar_standard_view_model_get_show_hidden (ThunarStandardViewModel *model);
void
thunar_standard_view_model_set_show_hidden (ThunarStandardViewModel *model,
                                            gboolean                 show_hidden);

gboolean
thunar_standard_view_model_get_file_size_binary (ThunarStandardViewModel *model);
void
thunar_standard_view_model_set_file_size_binary (ThunarStandardViewModel *model,
                                                 gboolean                 file_size_binary);

ThunarFile *
thunar_standard_view_model_get_file (ThunarStandardViewModel *model,
                                     GtkTreeIter             *iter);


GList *
thunar_standard_view_model_get_paths_for_files (ThunarStandardViewModel *model,
                                                GList                   *files);
GList *
thunar_standard_view_model_get_paths_for_pattern (ThunarStandardViewModel *model,
                                                  const gchar             *pattern,
                                                  gboolean                 case_sensitive,
                                                  gboolean                 match_diacritics);
ThunarJob *
thunar_standard_view_model_get_job (ThunarStandardViewModel *model);
void
thunar_standard_view_model_set_job (ThunarStandardViewModel *model,
                                    ThunarJob               *job);
void
thunar_standard_view_model_add_search_files (ThunarStandardViewModel *model,
                                             GList                   *files);

G_END_DECLS;

#endif /* __THUNAR_STANDARD_VIEW_MODEL__ */
