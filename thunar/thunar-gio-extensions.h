/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_GIO_EXTENSIONS_H__
#define __THUNAR_GIO_EXTENSIONS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

GFile    *thunar_g_file_new_for_home             (void);
GFile    *thunar_g_file_new_for_root             (void);
GFile    *thunar_g_file_new_for_trash            (void);
GFile    *thunar_g_file_new_for_desktop          (void);
GFile    *thunar_g_file_new_for_user_special_dir (GUserDirectory        dir);
                                                                      
gboolean  thunar_g_file_is_root                  (GFile                *file);
gboolean  thunar_g_file_is_trashed               (GFile                *file);
gboolean  thunar_g_file_is_home                  (GFile                *file);
gboolean  thunar_g_file_is_desktop               (GFile                *file);
                                                                      
GKeyFile *thunar_g_file_query_key_file           (GFile                *file,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
gboolean  thunar_g_file_write_key_file           (GFile                *file,
                                                  GKeyFile             *key_file,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
                                                                      
gchar    *thunar_g_file_get_location             (GFile                *file);
                                                                      
gboolean  thunar_g_vfs_is_uri_scheme_supported   (const gchar          *scheme);

/**
 * THUNAR_TYPE_G_FILE_LIST:
 *
 * Returns the type ID for #GList<!---->s of #GFile<!---->s which is a 
 * boxed type.
 **/
#define THUNAR_TYPE_G_FILE_LIST (thunar_g_file_list_get_type ())

GType     thunar_g_file_list_get_type               (void);

GList    *thunar_g_file_list_new_from_string        (const gchar       *string);
gchar    *thunar_g_file_list_to_string              (GList             *list);
GList    *thunar_g_file_list_append                 (GList             *list,
                                                     GFile             *file);
GList    *thunar_g_file_list_prepend                (GList             *list,
                                                     GFile             *file);
GList    *thunar_g_file_list_copy                   (GList             *list);
void      thunar_g_file_list_free                   (GList             *list);

gboolean  thunar_g_volume_is_removable              (GVolume           *volume);
gboolean  thunar_g_volume_is_mounted                (GVolume           *volume);
gboolean  thunar_g_volume_is_present                (GVolume           *volume);

gboolean  thunar_g_app_info_launch                  (GAppInfo          *info,
                                                     GFile             *working_directory,
                                                     GList             *path_list,
                                                     GAppLaunchContext *context,
                                                     GError           **error);

G_END_DECLS

#endif /* !__THUNAR_GIO_EXTENSIONS_H__ */
