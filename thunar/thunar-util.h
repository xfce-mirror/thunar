/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_UTIL_H__
#define __THUNAR_UTIL_H__

#include "thunar/thunar-enum-types.h"
#include "thunar/thunar-file.h"
#include "thunar/thunar-standard-view-model.h"

G_BEGIN_DECLS;

#define ALPHA_BACKDROP 0.5
#define ALPHA_FOCUSED 1.0

/* if we need an array of size greater
 * than this threshold; we'll use heap */
#define STACK_ALLOC_LIMIT 2000

/* Maximum number of emblems which can be used per file */
#define MAX_EMBLEMS_PER_FILE 4

typedef enum
{
  THUNAR_NEXT_FILE_NAME_MODE_NEW,
  THUNAR_NEXT_FILE_NAME_MODE_COPY,
  THUNAR_NEXT_FILE_NAME_MODE_LINK,
} ThunarNextFileNameMode;

typedef void (*ThunarBookmarksFunc) (GFile       *file,
                                     const gchar *name,
                                     gint         row_num,
                                     gpointer     user_data);

gchar *
thunar_util_str_get_extension (const gchar *name) G_GNUC_WARN_UNUSED_RESULT;

void
thunar_util_load_bookmarks (GFile              *bookmarks_file,
                            ThunarBookmarksFunc foreach_func,
                            gpointer            user_data);

gboolean
thunar_util_looks_like_an_uri (const gchar *string) G_GNUC_WARN_UNUSED_RESULT;

gchar *
thunar_util_expand_filename (const gchar *filename,
                             GFile       *working_directory,
                             GError     **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
guint64
thunar_util_get_file_time (GFileInfo         *file_info,
                           ThunarFileDateType date_type);
gchar *
thunar_util_humanize_file_time (guint64         file_time,
                                ThunarDateStyle date_style,
                                const gchar    *date_custom_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkScreen *
thunar_util_parse_parent (gpointer    parent,
                          GtkWindow **window_return) G_GNUC_WARN_UNUSED_RESULT;

time_t
thunar_util_time_from_rfc3339 (const gchar *date_string) G_GNUC_WARN_UNUSED_RESULT;

gchar *
thunar_util_change_working_directory (const gchar *new_directory) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void
thunar_setup_display_cb (gpointer data);
gchar *
thunar_util_next_new_file_name (ThunarFile            *dir,
                                const gchar           *file_name,
                                ThunarNextFileNameMode name_mode,
                                gboolean               is_directory);
gchar *
thunar_util_next_new_file_name_raw (GList                 *file_list,
                                    const gchar           *file_name,
                                    ThunarNextFileNameMode name_mode,
                                    gboolean               is_directory);

const char *
thunar_util_get_search_prefix (void);
gboolean
thunar_util_is_a_search_query (const gchar *string);
gchar *
thunar_util_strjoin_list (GList       *string_list,
                          const gchar *separator);
void
thunar_util_clip_view_background (GtkCellRenderer     *cell,
                                  cairo_t             *cr,
                                  const GdkRectangle  *background_area,
                                  GtkWidget           *widget,
                                  GtkCellRendererState flags);
gchar **
thunar_util_split_search_query (const gchar *search_query_normalized,
                                GError     **error);
gboolean
thunar_util_search_terms_match (gchar **terms,
                                gchar  *str);
gboolean
thunar_util_save_geometry_timer (gpointer user_data);
gchar *
thunar_util_get_statusbar_text_for_files (GHashTable     *files,
                                          gboolean        show_hidden,
                                          gboolean        show_file_size_binary_format,
                                          ThunarDateStyle date_style,
                                          const gchar    *date_custom_style,
                                          guint           status_bar_actve_info);
gchar *
thunar_util_get_statusbar_text_for_single_file (ThunarFile *file);
gchar *
thunar_util_accel_path_to_id (const gchar *accel_path);

G_END_DECLS;

#endif /* !__THUNAR_UTIL_H__ */
