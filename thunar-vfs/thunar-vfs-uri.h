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

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsURI ThunarVfsURI;

#define THUNAR_VFS_TYPE_URI (thunar_vfs_uri_get_type ())

/**
 * ThunarVfsURIScheme:
 * @THUNAR_VFS_URI_SCHEME_COMPUTER : 'computer://' uris
 * @THUNAR_VFS_URI_SCHEME_FILE     : 'file://' uris
 * @THUNAR_VFS_URI_SCHEME_TRASH    : 'trash://' uris
 *
 * Currently supported URI types for #ThunarVfsURI.
 **/
typedef enum
{
  THUNAR_VFS_URI_SCHEME_COMPUTER,
  THUNAR_VFS_URI_SCHEME_FILE,
  THUNAR_VFS_URI_SCHEME_TRASH,
} ThunarVfsURIScheme;

GType              thunar_vfs_uri_get_type          (void) G_GNUC_CONST;

ThunarVfsURI      *thunar_vfs_uri_new               (const gchar            *identifier,
                                                     GError                **error) G_GNUC_MALLOC;
ThunarVfsURI      *thunar_vfs_uri_new_for_path      (const gchar            *path) G_GNUC_MALLOC;

ThunarVfsURI      *thunar_vfs_uri_ref               (ThunarVfsURI           *uri);
void               thunar_vfs_uri_unref             (ThunarVfsURI           *uri);

gboolean           thunar_vfs_uri_is_home           (const ThunarVfsURI     *uri);
gboolean           thunar_vfs_uri_is_root           (const ThunarVfsURI     *uri);

gchar             *thunar_vfs_uri_get_display_name  (const ThunarVfsURI     *uri) G_GNUC_MALLOC;
gchar             *thunar_vfs_uri_get_md5sum        (const ThunarVfsURI     *uri) G_GNUC_MALLOC;
const gchar       *thunar_vfs_uri_get_name          (const ThunarVfsURI     *uri);
const gchar       *thunar_vfs_uri_get_path          (const ThunarVfsURI     *uri);
ThunarVfsURIScheme thunar_vfs_uri_get_scheme        (const ThunarVfsURI     *uri);

ThunarVfsURI      *thunar_vfs_uri_parent            (const ThunarVfsURI     *uri) G_GNUC_MALLOC;
ThunarVfsURI      *thunar_vfs_uri_relative          (const ThunarVfsURI     *uri,
                                                     const gchar            *name) G_GNUC_MALLOC;

gchar             *thunar_vfs_uri_to_string         (const ThunarVfsURI     *uri) G_GNUC_MALLOC;

guint              thunar_vfs_uri_hash              (gconstpointer           uri);
gboolean           thunar_vfs_uri_equal             (gconstpointer           a,
                                                     gconstpointer           b);

GList             *thunar_vfs_uri_list_from_string  (const gchar            *string,
                                                     GError                **error) G_GNUC_MALLOC;
gchar             *thunar_vfs_uri_list_to_string    (GList                  *uri_list) G_GNUC_MALLOC;
GList             *thunar_vfs_uri_list_copy         (GList                  *uri_list) G_GNUC_MALLOC;
void               thunar_vfs_uri_list_free         (GList                  *uri_list);

#define thunar_vfs_uri_list_append(uri_list, uri) \
  g_list_append ((uri_list), thunar_vfs_uri_ref ((uri)))
#define thunar_vfs_uri_list_prepend(uri_list, uri) \
  g_list_prepend ((uri_list), thunar_vfs_uri_ref ((uri)))

G_END_DECLS;

#endif /* !__THUNAR_VFS_URI_H__ */
