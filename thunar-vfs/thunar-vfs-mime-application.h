/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __THUNAR_VFS_MIME_APPLICATION_H__
#define __THUNAR_VFS_MIME_APPLICATION_H__

#include <thunar-vfs/thunar-vfs-uri.h>

G_BEGIN_DECLS;

#define THUNAR_VFS_MIME_APPLICATION_ERROR (thunar_vfs_mime_application_error_quark ())

typedef enum
{
  THUNAR_VFS_MIME_APPLICATION_ERROR_LOCAL_FILES_ONLY,
} ThunarVfsMimeApplicationError;

GQuark thunar_vfs_mime_application_error_quark (void) G_GNUC_CONST;


typedef struct _ThunarVfsMimeApplication ThunarVfsMimeApplication;

#define THUNAR_VFS_TYPE_MIME_APPLICATION (thunar_vfs_mime_application_get_type ())

GType                     thunar_vfs_mime_application_get_type            (void) G_GNUC_CONST;

ThunarVfsMimeApplication *thunar_vfs_mime_application_new_from_desktop_id (const gchar                    *desktop_id) G_GNUC_MALLOC;

ThunarVfsMimeApplication *thunar_vfs_mime_application_ref                 (ThunarVfsMimeApplication       *application);
void                      thunar_vfs_mime_application_unref               (ThunarVfsMimeApplication       *application);

const gchar              *thunar_vfs_mime_application_get_desktop_id      (const ThunarVfsMimeApplication *application);
const gchar              *thunar_vfs_mime_application_get_name            (const ThunarVfsMimeApplication *application);

gboolean                  thunar_vfs_mime_application_exec                (const ThunarVfsMimeApplication *application,
                                                                           GdkScreen                      *screen,
                                                                           GList                          *uris,
                                                                           GError                        **error);
gboolean                  thunar_vfs_mime_application_exec_with_env       (const ThunarVfsMimeApplication *application,
                                                                           GdkScreen                      *screen,
                                                                           GList                          *uris,
                                                                           gchar                         **envp,
                                                                           GError                        **error);

const gchar              *thunar_vfs_mime_application_lookup_icon_name    (const ThunarVfsMimeApplication *application,
                                                                           GtkIconTheme                   *icon_theme);

guint                     thunar_vfs_mime_application_hash                (gconstpointer                   application);
gboolean                  thunar_vfs_mime_application_equal               (gconstpointer                   a,
                                                                           gconstpointer                   b);

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_APPLICATION_H__ */
