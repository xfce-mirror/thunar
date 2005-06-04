/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_URI_H__
#define __THUNAR_VFS_URI_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsURIClass ThunarVfsURIClass;
typedef struct _ThunarVfsURI      ThunarVfsURI;

#define THUNAR_VFS_TYPE_URI             (thunar_vfs_uri_get_type ())
#define THUNAR_VFS_URI(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_URI, ThunarVfsURI))
#define THUNAR_VFS_URI_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_URI, ThunarVfsURIClass))
#define THUNAR_VFS_IS_URI(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_URI))
#define THUNAR_VFS_IS_URI_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_URI))
#define THUNAR_VFS_URI_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_URI, ThunarVfsURIClass))

GType          thunar_vfs_uri_get_type          (void) G_GNUC_CONST;

ThunarVfsURI  *thunar_vfs_uri_new_for_path      (const gchar  *path);

gboolean       thunar_vfs_uri_is_local          (ThunarVfsURI *uri);
gboolean       thunar_vfs_uri_is_root           (ThunarVfsURI *uri);

gchar         *thunar_vfs_uri_get_display_name  (ThunarVfsURI *uri);
const gchar   *thunar_vfs_uri_get_name          (ThunarVfsURI *uri);
const gchar   *thunar_vfs_uri_get_path          (ThunarVfsURI *uri);

ThunarVfsURI  *thunar_vfs_uri_parent            (ThunarVfsURI *uri);
ThunarVfsURI  *thunar_vfs_uri_relative          (ThunarVfsURI *uri,
                                                 const gchar  *name);

guint          thunar_vfs_uri_hash              (gconstpointer uri);
gboolean       thunar_vfs_uri_equal             (gconstpointer a,
                                                 gconstpointer b);

G_END_DECLS;

#endif /* !__THUNAR_VFS_URI_H__ */
