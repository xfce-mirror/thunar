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

#include <thunar/thunar-enum-types.h>

G_BEGIN_DECLS;

typedef void (*ThunarBookmarksFunc) (GFile       *file,
                                     const gchar *name,
                                     gint         row_num,
                                     gpointer     user_data);

gchar     *thunar_util_str_get_extension        (const gchar *name) G_GNUC_WARN_UNUSED_RESULT;

void       thunar_util_load_bookmarks           (GFile               *bookmarks_file,
                                                 ThunarBookmarksFunc  foreach_func,
                                                 gpointer             user_data);

gboolean   thunar_util_looks_like_an_uri        (const gchar    *string) G_GNUC_WARN_UNUSED_RESULT;

gchar     *thunar_util_expand_filename          (const gchar    *filename,
                                                 GFile          *working_directory,
                                                 GError        **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gchar     *thunar_util_humanize_file_time       (guint64         file_time,
                                                 ThunarDateStyle date_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkScreen *thunar_util_parse_parent             (gpointer        parent,
                                                 GtkWindow     **window_return) G_GNUC_WARN_UNUSED_RESULT;

time_t     thunar_util_time_from_rfc3339        (const gchar    *date_string) G_GNUC_WARN_UNUSED_RESULT;

gchar     *thunar_util_change_working_directory (const gchar    *new_directory) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_UTIL_H__ */
