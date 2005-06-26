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

#include <sys/types.h>
#include <unistd.h>

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-uri.h>

G_BEGIN_DECLS;

/**
 * ThunarVfsFileType:
 * @THUNAR_VFS_FILE_TYPE_FIFO     : A named FIFO.
 * @THUNAR_VFS_FILE_TYPE_CHARDEV  : A character device node.
 * @THUNAR_VFS_FILE_TYPE_DIRECTORY: A directory node.
 * @THUNAR_VFS_FILE_TYPE_BLOCKDEV : A block device node.
 * @THUNAR_VFS_FILE_TYPE_REGULAR  : A regular file.
 * @THUNAR_VFS_FILE_TYPE_SYMLINK  : A broken symlink, for which the target does
 *                                  not exist (if the target would exist, the
 *                                  #ThunarVfsInfo object would have the type
 *                                  of the target).
 * @THUNAR_VFS_FILE_TYPE_SOCKET   : A unix domain socket.
 *
 * Describes the type of a file.
 **/
typedef enum {
  THUNAR_VFS_FILE_TYPE_SOCKET     = 0140000 >> 12,
  THUNAR_VFS_FILE_TYPE_SYMLINK    = 0120000 >> 12,
  THUNAR_VFS_FILE_TYPE_REGULAR    = 0100000 >> 12,
  THUNAR_VFS_FILE_TYPE_BLOCKDEV   = 0060000 >> 12,
  THUNAR_VFS_FILE_TYPE_DIRECTORY  = 0040000 >> 12,
  THUNAR_VFS_FILE_TYPE_CHARDEV    = 0020000 >> 12,
  THUNAR_VFS_FILE_TYPE_FIFO       = 0010000 >> 12,
  THUNAR_VFS_FILE_TYPE_UNKNOWN    = 0000000 >> 12,
} ThunarVfsFileType;

/**
 * ThunarVfsFileMode:
 * @THUNAR_VFS_FILE_MODE_SUID     : SUID bit.
 * @THUNAR_VFS_FILE_MODE_SGID     : SGID bit.
 * @THUNAR_VFS_FILE_MODE_STICKY   : Sticky bit.
 * @THUNAR_VFS_FILE_MODE_USR_ALL  : Owner can do everything.
 * @THUNAR_VFS_FILE_MODE_USR_READ : Owner can read the file.
 * @THUNAR_VFS_FILE_MODE_USR_WRITE: Owner can write the file.
 * @THUNAR_VFS_FILE_MODE_USR_EXEC : Owner can execute the file.
 * @THUNAR_VFS_FILE_MODE_GRP_ALL  : Owner group can do everything.
 * @THUNAR_VFS_FILE_MODE_GRP_READ : Owner group can read the file.
 * @THUNAR_VFS_FILE_MODE_GRP_WRITE: Owner group can write the file.
 * @THUNAR_VFS_FILE_MODE_GRP_EXEC : Owner group can execute the file.
 * @THUNAR_VFS_FILE_MODE_OTH_ALL  : Others can do everything.
 * @THUNAR_VFS_FILE_MODE_OTH_READ : Others can read the file.
 * @THUNAR_VFS_FILE_MODE_OTH_WRITE: Others can write the file.
 * @THUNAR_VFS_FILE_MODE_OTH_EXEC : Others can execute the file.
 *
 * Special flags and permissions of a filesystem entity.
 **/
typedef enum {
  THUNAR_VFS_FILE_MODE_SUID       = 04000,
  THUNAR_VFS_FILE_MODE_SGID       = 02000,
  THUNAR_VFS_FILE_MODE_STICKY     = 01000,
  THUNAR_VFS_FILE_MODE_USR_ALL    = 00700,
  THUNAR_VFS_FILE_MODE_USR_READ   = 00400,
  THUNAR_VFS_FILE_MODE_USR_WRITE  = 00200,
  THUNAR_VFS_FILE_MODE_USR_EXEC   = 00100,
  THUNAR_VFS_FILE_MODE_GRP_ALL    = 00070,
  THUNAR_VFS_FILE_MODE_GRP_READ   = 00040,
  THUNAR_VFS_FILE_MODE_GRP_WRITE  = 00020,
  THUNAR_VFS_FILE_MODE_GRP_EXEC   = 00010,
  THUNAR_VFS_FILE_MODE_OTH_ALL    = 00007,
  THUNAR_VFS_FILE_MODE_OTH_READ   = 00004,
  THUNAR_VFS_FILE_MODE_OTH_WRITE  = 00002,
  THUNAR_VFS_FILE_MODE_OTH_EXEC   = 00001,
} ThunarVfsFileMode;

/**
 * ThunarVfsFileFlags:
 * @THUNAR_VFS_FILE_FLAGS_NONE    : No additional information available.
 * @THUNAR_VFS_FILE_FLAGS_SYMLINK : The file is a symlink. Whether or not
 *                                  the info fields refer to the symlink
 *                                  itself or the linked file, depends on 
 *                                  whether the symlink is broken or not.
 *
 * Flags providing additional information about the
 * file system entity.
 **/
typedef enum {
  THUNAR_VFS_FILE_FLAGS_NONE    = 0,
  THUNAR_VFS_FILE_FLAGS_SYMLINK = 1 << 0,
} ThunarVfsFileFlags;

/**
 * ThunarVfsFileDevice:
 * Datatype to represent the device number of a file.
 **/
typedef dev_t ThunarVfsFileDevice;

/**
 * ThunarVfsFileInode:
 * Datatype to represent the inode number of a file.
 **/
typedef ino_t ThunarVfsFileInode;

/**
 * ThunarVfsFileSize:
 * Datatype to represent file sizes (in bytes).
 **/
typedef off_t ThunarVfsFileSize;

/**
 * ThunarVfsFileTime:
 * Datatype to represent file times (in seconds).
 **/
typedef time_t ThunarVfsFileTime;

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
  uid_t uid;

  /* Owner's group id */
  gid_t gid;

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

  /* file's URI */
  ThunarVfsURI *uri;
} ThunarVfsInfo;

#define thunar_vfs_info_init(info)                                          \
G_STMT_START {                                                              \
  (info)->type = THUNAR_VFS_FILE_TYPE_UNKNOWN;                              \
  (info)->ctime = (ThunarVfsFileTime) -1;                                   \
  (info)->uri = NULL;                                                       \
} G_STMT_END

#define thunar_vfs_info_reset(info)                                         \
G_STMT_START {                                                              \
  if (G_LIKELY ((info)->uri != NULL))                                       \
    {                                                                       \
      thunar_vfs_uri_unref ((info)->uri);                                   \
      (info)->uri = NULL;                                                   \
    }                                                                       \
  (info)->type = THUNAR_VFS_FILE_TYPE_UNKNOWN;                              \
  (info)->ctime = (ThunarVfsFileTime) -1;                                   \
} G_STMT_END

/**
 * ThunarVfsInfoResult:
 * @THUNAR_VFS_INFO_NOCHANGE: file wasn't altered since last check.
 * @THUNAR_VFS_INFO_CHANGED : file was changed since last update.
 * @THUNAR_VFS_INFO_ERROR   : an error occured on checking the file.
 *
 * Possible return values for the #thunar_vfs_info_update() function,
 * which determine the result of the update.
 **/
typedef enum
{
  THUNAR_VFS_INFO_RESULT_NOCHANGE,
  THUNAR_VFS_INFO_RESULT_CHANGED,
  THUNAR_VFS_INFO_RESULT_ERROR,
} ThunarVfsInfoResult;

gboolean            thunar_vfs_info_query            (ThunarVfsInfo       *info,
                                                      ThunarVfsURI        *uri,
                                                      GError             **error);

ThunarVfsInfoResult thunar_vfs_info_update           (ThunarVfsInfo       *info,
                                                      GError             **error);

ExoMimeInfo        *thunar_vfs_info_get_mime_info    (ThunarVfsInfo        *info);

G_END_DECLS;

#endif /* !__THUNAR_VFS_INFO_H__ */
