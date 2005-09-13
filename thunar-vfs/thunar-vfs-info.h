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

#ifndef __THUNAR_VFS_INFO_H__
#define __THUNAR_VFS_INFO_H__

#include <gdk/gdk.h>

#include <thunar-vfs/thunar-vfs-mime-info.h>
#include <thunar-vfs/thunar-vfs-types.h>
#include <thunar-vfs/thunar-vfs-uri.h>

G_BEGIN_DECLS;

#define THUNAR_VFS_TYPE_INFO (thunar_vfs_info_get_type ())

/**
 * ThunarVfsInfo:
 *
 * The #ThunarVfsInfo structure provides information about a file system
 * entity.
 **/
typedef struct
{
  /* File type */
  ThunarVfsFileType type;

  /* File permissions and special mode flags */
  ThunarVfsFileMode mode;

  /* File flags */
  ThunarVfsFileFlags flags;

  /* Owner's user id */
  ThunarVfsUserId uid;

  /* Owner's group id */
  ThunarVfsGroupId gid;

  /* Size in bytes */
  ThunarVfsFileSize size;

  /* time of last access */
  ThunarVfsFileTime atime;

  /* time of last modification */
  ThunarVfsFileTime mtime;

  /* time of last status change */
  ThunarVfsFileTime ctime;

  /* inode id */
  ThunarVfsFileInode inode;

  /* device id */
  ThunarVfsFileDevice device;

  /* file's mime type */
  ThunarVfsMimeInfo *mime_info;

  /* file's URI */
  ThunarVfsURI *uri;

  /* file's display name (UTF-8) */
  gchar *display_name;

  /*< private >*/
  gchar **hints;
  gint    ref_count;
} ThunarVfsInfo;

GType          thunar_vfs_info_get_type    (void) G_GNUC_CONST;

ThunarVfsInfo *thunar_vfs_info_new_for_uri (ThunarVfsURI        *uri,
                                            GError             **error) G_GNUC_MALLOC;

ThunarVfsInfo *thunar_vfs_info_ref         (ThunarVfsInfo       *info);
void           thunar_vfs_info_unref       (ThunarVfsInfo       *info);

gboolean       thunar_vfs_info_execute     (const ThunarVfsInfo *info,
                                            GdkScreen           *screen,
                                            GList               *uris,
                                            GError             **error);

gboolean       thunar_vfs_info_rename      (ThunarVfsInfo       *info,
                                            const gchar         *name,
                                            GError             **error);

const gchar   *thunar_vfs_info_get_hint    (const ThunarVfsInfo *info,
                                            ThunarVfsFileHint    hint);

gboolean       thunar_vfs_info_matches     (const ThunarVfsInfo *a,
                                            const ThunarVfsInfo *b);

void           thunar_vfs_info_list_free   (GList               *info_list);

G_END_DECLS;

#endif /* !__THUNAR_VFS_INFO_H__ */
