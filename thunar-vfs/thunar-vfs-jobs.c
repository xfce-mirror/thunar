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

#include <thunar-vfs/thunar-vfs-jobs.h>
#include <thunar-vfs/thunar-vfs-listdir-job.h>
#include <thunar-vfs/thunar-vfs-unlink-job.h>



/**
 * thunar_vfs_listdir:
 * @uri   : the #ThunarVfsURI for the folder that should be listed.
 * @error : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsListdirJob, which can be used to list the
 * contents of a directory (as specified by @uri). If the creation
 * of the job failes for some reason, %NULL will be returned and
 * @error will be set to point to a #GError describing the cause.
 * Else the newly allocated #ThunarVfsListdirJob will be returned
 * and the caller is responsible to call #thunar_vfs_job_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call #thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsListdirJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_listdir (ThunarVfsURI *uri,
                    GError      **error)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have a 'file://'-URI here */
  if (G_UNLIKELY (thunar_vfs_uri_get_scheme (uri) != THUNAR_VFS_URI_SCHEME_FILE))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("Only local directories can be listed"));
      return NULL;
    }

  /* allocate the job */
  return thunar_vfs_job_launch (thunar_vfs_listdir_job_new (uri));
}



/**
 * thunar_vfs_unlink:
 * @uris  : a list of #ThunarVfsURI<!---->s, that should be unlinked.
 * @error : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsInteractiveJob, which can be used to unlink
 * all files referenced by the @uris. If the creation of the job
 * failes for some reason, %NULL will be returned and @error will
 * be set to point to a #GError describing the cause. Else, the
 * newly allocated #ThunarVfsUnlinkJob will be returned, and the
 * caller is responsible to call #thunar_vfs_job_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call #thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsUnlinkJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_unlink (GList   *uris,
                   GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to allocate the job */
  job = thunar_vfs_unlink_job_new (uris, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



