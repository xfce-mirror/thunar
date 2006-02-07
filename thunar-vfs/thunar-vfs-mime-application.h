/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#if !defined (THUNAR_VFS_INSIDE_THUNAR_VFS_H) && !defined (THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __THUNAR_VFS_MIME_APPLICATION_H__
#define __THUNAR_VFS_MIME_APPLICATION_H__

#include <thunar-vfs/thunar-vfs-config.h>
#include <thunar-vfs/thunar-vfs-mime-handler.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeApplicationClass ThunarVfsMimeApplicationClass;
typedef struct _ThunarVfsMimeApplication      ThunarVfsMimeApplication;

#define THUNAR_VFS_TYPE_MIME_APPLICATION            (thunar_vfs_mime_application_get_type ())
#define THUNAR_VFS_MIME_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplication))
#define THUNAR_VFS_MIME_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplicationClass))
#define THUNAR_VFS_IS_MIME_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION))
#define THUNAR_VFS_IS_MIME_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_APPLICATION))
#define THUNAR_VFS_MIME_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_APPLICATION, ThunarVfsMimeApplicationClass))

GType                         thunar_vfs_mime_application_get_type            (void) G_GNUC_CONST;

ThunarVfsMimeApplication     *thunar_vfs_mime_application_new_from_desktop_id (const gchar                    *desktop_id) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarVfsMimeApplication     *thunar_vfs_mime_application_new_from_file       (const gchar                    *path,
                                                                               const gchar                    *desktop_id) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GList                        *thunar_vfs_mime_application_get_actions         (ThunarVfsMimeApplication       *mime_application) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
const gchar                  *thunar_vfs_mime_application_get_desktop_id      (const ThunarVfsMimeApplication *mime_application);
const gchar * const          *thunar_vfs_mime_application_get_mime_types      (const ThunarVfsMimeApplication *mime_application);

guint                         thunar_vfs_mime_application_hash                (gconstpointer                   mime_application);
gboolean                      thunar_vfs_mime_application_equal               (gconstpointer                   a,
                                                                               gconstpointer                   b);

/**
 * thunar_vfs_mime_application_get_command:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the command for @mime_application.
 *
 * Return value: the command for @mime_application.
 **/
#define thunar_vfs_mime_application_get_command(mime_application) (thunar_vfs_mime_handler_get_command (THUNAR_VFS_MIME_HANDLER ((mime_application))))

/**
 * thunar_vfs_mime_application_get_flags:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the #ThunarVfsMimeHandlerFlags for @mime_handler.
 *
 * Return value: the flags for @mime_application.
 **/
#define thunar_vfs_mime_application_get_flags(mime_application) (thunar_vfs_mime_handler_get_flags (THUNAR_VFS_MIME_HANDLER ((mime_application))))

/**
 * thunar_vfs_mime_application_get_name:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the name for @mime_application.
 *
 * Return value: the name for @mime_application.
 **/
#define thunar_vfs_mime_application_get_name(mime_application) (thunar_vfs_mime_handler_get_name (THUNAR_VFS_MIME_HANDLER ((mime_application))))

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_APPLICATION_H__ */
