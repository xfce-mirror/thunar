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

#ifndef __THUNAR_VFS_INFO_H__
#define __THUNAR_VFS_INFO_H__

#include <thunar-vfs/thunar-vfs-mime-info.h>
#include <thunar-vfs/thunar-vfs-path.h>
#include <thunar-vfs/thunar-vfs-types.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsInfoMetadata:
 * @THUNAR_VFS_INFO_METADATA_FILE_LINK_TARGET    : the target of a symbolic link.
 * @THUNAR_VFS_INFO_METADATA_TRASH_ORIGINAL_PATH : the original path of a trashed resource.
 * @THUNAR_VFS_INFO_METADATA_TRASH_DELETION_DATE : the deletion date of a trashed resource as date string.
 *
 * Metadata categories for thunar_vfs_info_get_metadata().
 *
 * Since: 0.3.3
 **/
typedef enum /*< skip >*/
{
  THUNAR_VFS_INFO_METADATA_FILE_LINK_TARGET    = 0,
  THUNAR_VFS_INFO_METADATA_TRASH_ORIGINAL_PATH = 64,
  THUNAR_VFS_INFO_METADATA_TRASH_DELETION_DATE = 65,
} ThunarVfsInfoMetadata;


/**
 * THUNAR_VFS_TYPE_INFO:
 *
 * Returns the type if for #ThunarVfsInfo<!---->s, which is
 * a boxed type.
 **/
#define THUNAR_VFS_TYPE_INFO (thunar_vfs_info_get_type ())

/* Used to avoid a dependency of thunarx on thunar-vfs */
#ifndef __THUNAR_VFS_INFO_DEFINED__
#define __THUNAR_VFS_INFO_DEFINED__
typedef struct _ThunarVfsInfo ThunarVfsInfo;
#endif

struct _ThunarVfsInfo
{
  /* File type */
  ThunarVfsFileType type : 8;

  /* File permissions and special mode flags */
  ThunarVfsFileMode mode : 12;

  /* File flags */
  ThunarVfsFileFlags flags : 12;

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

  /* device id */
  ThunarVfsFileDevice device;

  /* file's mime type */
  ThunarVfsMimeInfo *mime_info;

  /* file's absolute path */
  ThunarVfsPath *path;

  /* file's custom icon (path or themed icon name) */
  gchar *custom_icon;

  /* file's display name (UTF-8) */
  gchar *display_name;

  /*< private >*/
  gint ref_count;
};

GType                        thunar_vfs_info_get_type         (void) G_GNUC_CONST;

ThunarVfsInfo               *thunar_vfs_info_new_for_path     (ThunarVfsPath        *path,
                                                               GError              **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_INLINE_FUNC ThunarVfsInfo *thunar_vfs_info_ref              (ThunarVfsInfo        *info);
void                         thunar_vfs_info_unref            (ThunarVfsInfo        *info);

ThunarVfsInfo               *thunar_vfs_info_copy             (const ThunarVfsInfo  *info) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_INLINE_FUNC const gchar   *thunar_vfs_info_get_custom_icon  (const ThunarVfsInfo  *info) G_GNUC_WARN_UNUSED_RESULT;
gboolean                     thunar_vfs_info_set_custom_icon  (ThunarVfsInfo        *info,
                                                               const gchar          *custom_icon,
                                                               GError              **error) G_GNUC_WARN_UNUSED_RESULT;

gboolean                     thunar_vfs_info_get_free_space   (const ThunarVfsInfo  *info,
                                                               ThunarVfsFileSize    *free_space_return) G_GNUC_WARN_UNUSED_RESULT;

gchar                       *thunar_vfs_info_get_metadata     (const ThunarVfsInfo  *info,
                                                               ThunarVfsInfoMetadata metadata,
                                                               GError              **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_INLINE_FUNC gchar         *thunar_vfs_info_read_link        (const ThunarVfsInfo  *info,
                                                               GError              **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean                     thunar_vfs_info_execute          (const ThunarVfsInfo  *info,
                                                               GdkScreen            *screen,
                                                               GList                *path_list,
                                                               const gchar          *working_directory,
                                                               GError              **error);

gboolean                     thunar_vfs_info_rename           (ThunarVfsInfo        *info,
                                                               const gchar          *name,
                                                               GError              **error);

gboolean                     thunar_vfs_info_matches          (const ThunarVfsInfo  *a,
                                                               const ThunarVfsInfo  *b) G_GNUC_WARN_UNUSED_RESULT;

void                         thunar_vfs_info_list_free        (GList                *info_list);


/* inline functions implementations */
#if defined(G_CAN_INLINE) || defined(__THUNAR_VFS_INFO_C__)
/**
 * thunar_vfs_info_ref:
 * @info : a #ThunarVfsInfo.
 *
 * Increments the reference count on @info by 1 and
 * returns a pointer to @info.
 *
 * Return value: a pointer to @info.
 **/
G_INLINE_FUNC ThunarVfsInfo*
thunar_vfs_info_ref (ThunarVfsInfo *info)
{
  exo_atomic_inc (&info->ref_count);
  return info;
}

/**
 * thunar_vfs_info_get_custom_icon:
 * @info : a #ThunarVfsInfo.
 *
 * Returns the custom icon for @info if there's
 * a custom icon, else %NULL.
 *
 * The custom icon can be a themed icon name or
 * an absolute path to an icon file in the local
 * file system.
 *
 * Return value: the custom icon for @info or %NULL.
 **/
G_INLINE_FUNC const gchar*
thunar_vfs_info_get_custom_icon (const ThunarVfsInfo *info)
{
  return info->custom_icon;
}

/**
 * thunar_vfs_info_read_link:
 * @info  : a #ThunarVfsInfo.
 * @error : return location for errors or %NULL.
 *
 * Reads the contents of the symbolic link to which @info refers to,
 * like the POSIX readlink() function. The returned string is in the
 * encoding used for filenames.
 *
 * The caller is responsible to free the returned string using g_free()
 * when no longer needed.
 *
 * Return value: a newly allocated string with the contents of the
 *               symbolic link, or %NULL if an error occurred.
 **/
G_INLINE_FUNC gchar*
thunar_vfs_info_read_link (const ThunarVfsInfo *info,
                           GError             **error)
{
  return thunar_vfs_info_get_metadata (info, THUNAR_VFS_INFO_METADATA_FILE_LINK_TARGET, error);
}
#endif /* G_CAN_INLINE || __THUNAR_VFS_INFO_C__ */


G_END_DECLS;

#endif /* !__THUNAR_VFS_INFO_H__ */
