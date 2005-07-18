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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-info.h>



/**
 * thunar_vfs_info_new_for_uri:
 * @uri   : the #ThunarVfsURI of the file whose info should be queried.
 * @error : return location for errors or %NULL.
 *
 * Queries the #ThunarVfsInfo for the given @uri. Returns the
 * #ThunarVfsInfo if the operation is successfull, else %NULL.
 * In the latter case, @error will be set to point to a #GError
 * describing the cause of the failure.
 *
 * Return value: the #ThunarVfsInfo for @uri or %NULL.
 **/
ThunarVfsInfo*
thunar_vfs_info_new_for_uri (ThunarVfsURI *uri,
                             GError      **error)
{
  ThunarVfsInfo *info;
  const gchar   *path;
  struct stat    lsb;
  struct stat    sb;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  path = thunar_vfs_uri_get_path (uri);

  if (G_UNLIKELY (lstat (path, &lsb) < 0))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "Failed to stat file `%s': %s", path, g_strerror (errno));
      return NULL;
    }

  info = g_new (ThunarVfsInfo, 1);
  info->uri = thunar_vfs_uri_ref (uri);
  info->ref_count = 1;

  if (G_LIKELY (!S_ISLNK (lsb.st_mode)))
    {
      info->type = (lsb.st_mode & S_IFMT) >> 12;
      info->mode = lsb.st_mode & 07777;
      info->flags = THUNAR_VFS_FILE_FLAGS_NONE;
      info->uid = lsb.st_uid;
      info->gid = lsb.st_gid;
      info->size = lsb.st_size;
      info->atime = lsb.st_atime;
      info->ctime = lsb.st_ctime;
      info->mtime = lsb.st_mtime;
      info->inode = lsb.st_ino;
      info->device = lsb.st_dev;
    }
  else
    {
      if (stat (path, &sb) == 0)
        {
          info->type = (sb.st_mode & S_IFMT) >> 12;
          info->mode = sb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = sb.st_uid;
          info->gid = sb.st_gid;
          info->size = sb.st_size;
          info->atime = sb.st_atime;
          info->ctime = sb.st_ctime;
          info->mtime = sb.st_mtime;
          info->inode = sb.st_ino;
          info->device = sb.st_dev;
        }
      else
        {
          info->type = THUNAR_VFS_FILE_TYPE_SYMLINK;
          info->mode = lsb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = lsb.st_uid;
          info->gid = lsb.st_gid;
          info->size = lsb.st_size;
          info->atime = lsb.st_atime;
          info->ctime = lsb.st_ctime;
          info->mtime = lsb.st_mtime;
          info->inode = lsb.st_ino;
          info->device = lsb.st_dev;
        }
    }

  switch (info->type)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:
      info->mime_info = thunar_vfs_mime_info_get ("inode/socket");
      break;

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      info->mime_info = thunar_vfs_mime_info_get ("inode/symlink");
      break;

    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:
      info->mime_info = thunar_vfs_mime_info_get ("inode/blockdevice");
      break;

    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      info->mime_info = thunar_vfs_mime_info_get ("inode/directory");
      break;

    case THUNAR_VFS_FILE_TYPE_CHARDEV:
      info->mime_info = thunar_vfs_mime_info_get ("inode/chardevice");
      break;

    case THUNAR_VFS_FILE_TYPE_FIFO:
      info->mime_info = thunar_vfs_mime_info_get ("inode/fifo");
      break;

    case THUNAR_VFS_FILE_TYPE_REGULAR:
      info->mime_info = thunar_vfs_mime_info_get_for_file (path);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return info;
}



/**
 * thunar_vfs_info_ref:
 * @info : a #ThunarVfsInfo.
 *
 * Increments the reference count on @info by 1 and
 * returns a pointer to @info.
 *
 * Return value: a pointer to @info.
 **/
ThunarVfsInfo*
thunar_vfs_info_ref (ThunarVfsInfo *info)
{
  g_return_val_if_fail (info->ref_count > 0, NULL);

  g_atomic_int_inc (&info->ref_count);

  return info;
}



/**
 * thunar_vfs_info_unref:
 * @info : a #ThunarVfsInfo.
 *
 * Decrements the reference count on @info by 1 and if
 * the reference count drops to zero as a result of this
 * operation, the @info will be freed completely.
 **/
void
thunar_vfs_info_unref (ThunarVfsInfo *info)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (info->ref_count > 0);

  if (g_atomic_int_dec_and_test (&info->ref_count))
    {
      thunar_vfs_mime_info_unref (info->mime_info);
      thunar_vfs_uri_unref (info->uri);

#ifndef G_DISABLE_CHECKS
      memset (info, 0xaa, sizeof (*info));
#endif

      g_free (info);
    }
}



/**
 * thunar_vfs_info_matches:
 * @a : a #ThunarVfsInfo.
 * @b : a #ThunarVfsInfo.
 *
 * Checks whether @a and @b refer to the same file
 * and share the same properties.
 *
 * Return value: %TRUE if @a and @b match.
 **/
gboolean
thunar_vfs_info_matches (const ThunarVfsInfo *a,
                         const ThunarVfsInfo *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);
  g_return_val_if_fail (a->ref_count > 0, FALSE);
  g_return_val_if_fail (b->ref_count > 0, FALSE);

  return thunar_vfs_uri_equal (a->uri, b->uri)
      && a->type == b->type
      && a->mode == b->mode
      && a->flags == b->flags
      && a->uid == b->uid
      && a->gid == b->gid
      && a->size == b->size
      && a->atime == b->atime
      && a->mtime == b->mtime
      && a->ctime == b->ctime
      && a->inode == b->inode
      && a->device == b->device
      && a->mime_info == b->mime_info;
}



/**
 * thunar_vfs_info_list_free:
 * @info_list : a list #ThunarVfsInfo<!---->s.
 *
 * Unrefs all #ThunarVfsInfo<!---->s in @info_list and
 * frees the list itself.
 *
 * This method always returns %NULL for the convenience of
 * being able to do:
 * <informalexample><programlisting>
 * info_list = thunar_vfs_info_list_free (info_list);
 * </programlisting></informalexample>
 *
 * Return value: the empty list (%NULL).
 **/
GSList*
thunar_vfs_info_list_free (GSList *info_list)
{
  g_slist_foreach (info_list, (GFunc) thunar_vfs_info_unref, NULL);
  g_slist_free (info_list);
  return NULL;
}

