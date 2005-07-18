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

#ifndef __THUNAR_VFS_MIME_H__
#define __THUNAR_VFS_MIME_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeInfoClass ThunarVfsMimeInfoClass;
typedef struct _ThunarVfsMimeInfo      ThunarVfsMimeInfo;

#define THUNAR_VFS_TYPE_MIME_INFO             (thunar_vfs_mime_info_get_type ())
#define THUNAR_VFS_MIME_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_INFO, ThunarVfsMimeInfo))
#define THUNAR_VFS_MIME_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_INFO, ThunarVfsMimeInfoClass))
#define THUNAR_VFS_IS_MIME_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_INFO))
#define THUNAR_VFS_IS_MIME_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_INFO))
#define THUNAR_VFS_MIME_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_INFO, ThunarVfsMimeInfoClass))

GType              thunar_vfs_mime_info_get_type         (void) G_GNUC_CONST;

ThunarVfsMimeInfo *thunar_vfs_mime_info_ref              (ThunarVfsMimeInfo       *info);
void               thunar_vfs_mime_info_unref            (ThunarVfsMimeInfo       *info);

ThunarVfsMimeInfo *thunar_vfs_mime_info_get              (const gchar             *mime_type);
ThunarVfsMimeInfo *thunar_vfs_mime_info_get_for_file     (const gchar             *path);

const gchar       *thunar_vfs_mime_info_get_comment      (ThunarVfsMimeInfo       *info) G_GNUC_PURE;
const gchar       *thunar_vfs_mime_info_get_name         (const ThunarVfsMimeInfo *info) G_GNUC_PURE;

const gchar       *thunar_vfs_mime_info_get_media        (const ThunarVfsMimeInfo *info) G_GNUC_PURE;
const gchar       *thunar_vfs_mime_info_get_subtype      (const ThunarVfsMimeInfo *info) G_GNUC_PURE;

const gchar       *thunar_vfs_mime_info_lookup_icon_name (ThunarVfsMimeInfo       *info,
                                                          GtkIconTheme            *icon_theme);

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_H__ */
