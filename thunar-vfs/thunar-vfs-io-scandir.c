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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-io-scandir.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-os.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static GList *tvis_collect (ThunarVfsPath     *path,
                            volatile gboolean *cancelled,
                            gboolean           recursive,
                            gboolean           follow_links,
                            gchar             *buffer,
                            GError           **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;



static GList*
tvis_collect (ThunarVfsPath     *path,
              volatile gboolean *cancelled,
              gboolean           recursive,
              gboolean           follow_links,
              gchar             *buffer,
              GError           **error)
{
  GError *err = NULL;
  GList  *directories = NULL;
  GList  *child_path_list;
  GList  *path_list;
  GList  *lp;

  /* scan the directory, according to the path scheme */
  switch (thunar_vfs_path_get_scheme (path))
    {
    case THUNAR_VFS_PATH_SCHEME_FILE:
      if (thunar_vfs_path_to_string (path, buffer, THUNAR_VFS_PATH_MAXSTRLEN, error) >= 0)
        path_list = _thunar_vfs_os_scandir (path, buffer, follow_links, recursive ? &directories : NULL, error);
      else
        path_list = NULL;
      break;

    case THUNAR_VFS_PATH_SCHEME_TRASH:
      path_list = _thunar_vfs_io_trash_scandir (path, follow_links, recursive ? &directories : NULL, error);
      break;

    default:
      _thunar_vfs_assert_not_reached ();
      return NULL;
    }

  /* perform the recursion */
  for (lp = directories; lp != NULL; lp = lp->next)
    {
      /* check if the user cancelled the scanning */
      if (G_UNLIKELY (cancelled != NULL && *cancelled))
        {
          /* user cancellation is EINTR */
          _thunar_vfs_set_g_error_from_errno (error, EINTR);
error:
          thunar_vfs_path_list_free (path_list);
          path_list = NULL;
          break;
        }

      child_path_list = tvis_collect (lp->data, cancelled, TRUE, follow_links, buffer, &err);
      if (G_UNLIKELY (err != NULL))
        {
          /* we can ignore certain errors here */
          if (G_UNLIKELY (err->domain != G_FILE_ERROR || (err->code != G_FILE_ERROR_ACCES && err->code != G_FILE_ERROR_NOTDIR
                                                       && err->code != G_FILE_ERROR_NOENT && err->code != G_FILE_ERROR_PERM)))
            {
              /* propagate the error */
              g_propagate_error (error, err);
              goto error;
            }

          /* ignored that error */
          g_clear_error (&err);
        }

      /* append the paths to our list */
      path_list = g_list_concat (child_path_list, path_list);
    }

  /* cleanup */
  g_list_free (directories);

  return path_list;
}



/**
 * _thunar_vfs_io_scandir:
 * @path      : the #ThunarVfsPath of the directory to scan.
 * @cancelled : pointer to a volatile boolean variable, which if
 *              %TRUE means to cancel the scan operation. May be
 *              %NULL in which case the scanner cannot be
 *              cancelled.
 * @flags     : scanner flags.
 * @error     : return location for errors or %NULL.
 *
 * The caller is responsible to free the returned list
 * using thunar_vfs_path_list_free() when no longer
 * needed.
 *
 * If @cancelled becomes true during the scan operation, %NULL
 * will be returned and @error will be set to %G_FILE_ERROR_INTR.
 *
 * Return value:
 **/
GList*
_thunar_vfs_io_scandir (ThunarVfsPath          *path,
                        volatile gboolean      *cancelled,
                        ThunarVfsIOScandirFlags flags,
                        GError                **error)
{
  gchar buffer[THUNAR_VFS_PATH_MAXSTRLEN];

  _thunar_vfs_return_val_if_fail (path != NULL, NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to collect the paths */
  return tvis_collect (path, cancelled,
                       (flags & THUNAR_VFS_IO_SCANDIR_RECURSIVE),
                       (flags & THUNAR_VFS_IO_SCANDIR_FOLLOW_LINKS),
                       buffer, error);
}



#define __THUNAR_VFS_IO_SCANDIR_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
