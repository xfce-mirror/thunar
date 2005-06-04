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



static gboolean thunar_vfs_info_update (ThunarVfsInfo *info,
                                        GError       **error);



static ExoMimeDatabase *mime_database = NULL;



static gboolean
thunar_vfs_info_update (ThunarVfsInfo *info,
                        GError       **error)
{
  const gchar *path;
  struct stat  lsb;
  struct stat  sb;

  path = thunar_vfs_uri_get_path (info->uri);

  if (lstat (path, &lsb) < 0)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "Failed to stat file `%s': %s", path, g_strerror (errno));
      return FALSE;
    }

  if (!S_ISLNK (lsb.st_mode))
    {
      if (info->ctime != lsb.st_ctime
          || info->device != lsb.st_dev
          || info->inode != lsb.st_ino)
        {
          /* no symlink, so no link target */
          if (G_UNLIKELY (info->target != NULL))
            {
              g_free (info->target);
              info->target = NULL;
            }

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
    }
  else
    {
      gchar buffer[PATH_MAX + 1];
      gint  length;

      /* we have a symlink, query the link target */
      length = readlink (path, buffer, sizeof (buffer) - 1);
      if (length < 0)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       "Failed to read link target of `%s': %s", path,
                       g_strerror (errno));
          return FALSE;
        }
      buffer[length] = '\0';

      if (stat (path, &sb) == 0)
        {
          /* ok, link target is present */
          if (info->ctime != sb.st_ctime
              || info->device != sb.st_dev
              || info->inode != sb.st_ino)
            {
              /* update the link target */
              g_free (info->target);
              info->target = g_new (gchar, length + 1);
              memcpy (info->target, buffer, length + 1);

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
        }
      else
        {
          /* we have a broken symlink */
          if (info->type != THUNAR_VFS_FILE_TYPE_SYMLINK
              || info->ctime != lsb.st_ctime
              || info->device != lsb.st_dev
              || info->inode != lsb.st_ino)
            {

              /* update the link target */
              g_free (info->target);
              info->target = g_new (gchar, length + 1);
              memcpy (info->target, buffer, length + 1);

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
    }

  return TRUE;
}



/**
 * thunar_vfs_info_query:
 * @info  : pointer to an uninitialized #ThunarVfsInfo object.
 * @uri   : an #ThunarVfsURI instance.
 * @error : return location for errors.
 *
 * Return value: %TRUE if the operation succeed, else %FALSE.
 **/
gboolean
thunar_vfs_info_query (ThunarVfsInfo  *info,
                       ThunarVfsURI   *uri,
                       GError        **error)
{
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->uri == NULL, FALSE);
  g_return_val_if_fail (info->target == NULL, FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);

  g_object_ref (G_OBJECT (uri));
  info->uri = uri;

  if (!thunar_vfs_info_update (info, error))
    {
      g_object_unref (G_OBJECT (info->uri));
      info->uri = NULL;
      return FALSE;
    }

  return TRUE;
}



/**
 * thunar_vfs_info_get_mime_info:
 * @info : pointer to a valid #ThunarVfsInfo object.
 *
 * Determines the appropriate MIME type for @info. The
 * caller is responsible for freeing the returned object
 * using #g_object_unref() once it's no longer needed.
 *
 * Return value: the #ExoMimeInfo instance for @info.
 **/
ExoMimeInfo*
thunar_vfs_info_get_mime_info (ThunarVfsInfo *info)
{
  const gchar *path;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (info->uri), NULL);

  if (G_UNLIKELY (mime_database == NULL))
    mime_database = exo_mime_database_get_default ();

  switch (info->type)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:
      return exo_mime_database_get_info (mime_database, "inode/socket");

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      return exo_mime_database_get_info (mime_database, "inode/symlink");

    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:
      return exo_mime_database_get_info (mime_database, "inode/blockdevice");

    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      return exo_mime_database_get_info (mime_database, "inode/directory");

    case THUNAR_VFS_FILE_TYPE_CHARDEV:
      return exo_mime_database_get_info (mime_database, "inode/chardevice");

    case THUNAR_VFS_FILE_TYPE_FIFO:
      return exo_mime_database_get_info (mime_database, "inode/fifo");

    default:
      path = thunar_vfs_uri_get_path (THUNAR_VFS_URI (info->uri));
      return exo_mime_database_get_info_for_file (mime_database, path);
    }

  g_assert_not_reached ();
  return NULL;
}




