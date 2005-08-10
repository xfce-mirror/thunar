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


typedef struct _ThunarVfsMimeApplicationClass ThunarVfsMimeApplicationClass;
typedef struct _ThunarVfsMimeApplication      ThunarVfsMimeApplication;

#define THUNAR_VFS_TYPE_MIME_APPLICATION            (thunar_vfs_mime_application_get_type ())
#define THUNAR_VFS_MIME_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplication))
#define THUNAR_VFS_MIME_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplicationClass))
#define THUNAR_VFS_IS_MIME_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION))
#define THUNAR_VFS_IS_MIME_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_APPLICATION))
#define THUNAR_VFS_MIME_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplicationClass))

GType                     thunar_vfs_mime_application_get_type            (void) G_GNUC_CONST;

#define                   thunar_vfs_mime_application_ref exo_object_ref
#define                   thunar_vfs_mime_application_unref exo_object_unref

ThunarVfsMimeApplication *thunar_vfs_mime_application_new_from_desktop_id (const gchar                    *desktop_id) EXO_GNUC_MALLOC;

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
