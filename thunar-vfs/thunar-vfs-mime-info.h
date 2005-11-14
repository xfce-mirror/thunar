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

#ifndef __THUNAR_VFS_MIME_INFO_H__
#define __THUNAR_VFS_MIME_INFO_H__

#include <thunar-vfs/thunar-vfs-config.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeInfo ThunarVfsMimeInfo;
struct _ThunarVfsMimeInfo
{
  /*< private >*/
  gint          ref_count;
  gchar        *comment;
  gchar        *icon_name;
};


#define THUNAR_VFS_TYPE_MIME_INFO (thunar_vfs_mime_info_get_type ())

GType                            thunar_vfs_mime_info_get_type         (void) G_GNUC_CONST;

ThunarVfsMimeInfo               *thunar_vfs_mime_info_new              (const gchar             *name,
                                                                        gssize                   len) G_GNUC_MALLOC;

G_INLINE_FUNC ThunarVfsMimeInfo *thunar_vfs_mime_info_ref              (ThunarVfsMimeInfo       *info);
void                             thunar_vfs_mime_info_unref            (ThunarVfsMimeInfo       *info);

const gchar                     *thunar_vfs_mime_info_get_comment      (ThunarVfsMimeInfo       *info);
G_INLINE_FUNC const gchar       *thunar_vfs_mime_info_get_name         (const ThunarVfsMimeInfo *info);

gchar                           *thunar_vfs_mime_info_get_media        (const ThunarVfsMimeInfo *info) G_GNUC_MALLOC;
gchar                           *thunar_vfs_mime_info_get_subtype      (const ThunarVfsMimeInfo *info) G_GNUC_MALLOC;

guint                            thunar_vfs_mime_info_hash             (gconstpointer            info);
gboolean                         thunar_vfs_mime_info_equal            (gconstpointer            a,
                                                                        gconstpointer            b);

const gchar                     *thunar_vfs_mime_info_lookup_icon_name (ThunarVfsMimeInfo       *info,
                                                                        GtkIconTheme            *icon_theme);

G_INLINE_FUNC void               thunar_vfs_mime_info_list_free        (GList                   *info_list);


#if defined(THUNAR_VFS_COMPILATION)
void _thunar_vfs_mime_info_invalidate_icon_name (ThunarVfsMimeInfo *info) G_GNUC_INTERNAL;
#endif


/* inline function implementations */
#if defined(G_CAN_INLINE) || defined(__THUNAR_VFS_MIME_INFO_C__)
/**
 * thunar_vfs_mime_info_ref:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Increments the reference count on @info and returns
 * the reference to @info.
 *
 * Return value: a reference to @info.
 **/
G_INLINE_FUNC ThunarVfsMimeInfo*
thunar_vfs_mime_info_ref (ThunarVfsMimeInfo *info)
{
  exo_atomic_inc (&info->ref_count);
  return info;
}

/**
 * thunar_vfs_mime_info_get_name:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the full qualified name of the MIME type
 * described by the @info object.
 *
 * Return value: the name of @info.
 **/
G_INLINE_FUNC const gchar*
thunar_vfs_mime_info_get_name (const ThunarVfsMimeInfo *info)
{
  return ((const gchar *) info) + sizeof (*info);
}

/**
 * thunar_vfs_mime_info_list_free:
 * @info_list : a #GList of #ThunarVfsMimeInfo<!---->s
 *
 * Frees the list and all #ThunarVfsMimeInfo<!---->s
 * contained within the list.
 **/
G_INLINE_FUNC void
thunar_vfs_mime_info_list_free (GList *info_list)
{
  g_list_foreach (info_list, (GFunc) thunar_vfs_mime_info_unref, NULL);
  g_list_free (info_list);
}
#endif /* G_CAN_INLINE || __THUNAR_VFS_MIME_INFO_C__ */


G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_INFO_H__ */
