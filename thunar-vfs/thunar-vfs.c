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

#include <thunar-vfs/thunar-vfs.h>
#include <thunar-vfs/thunar-vfs-listdir-job.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>
#include <thunar-vfs/thunar-vfs-unlink-job.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static ThunarVfsMimeDatabase *thunar_vfs_mime_database = NULL;
static gint                   thunar_vfs_ref_count = 0;



/**
 * thunar_vfs_init:
 *
 * Initializes the ThunarVFS library.
 **/
void
thunar_vfs_init (void)
{
  extern void _thunar_vfs_job_init ();
  extern void _thunar_vfs_mime_init ();

  if (g_atomic_int_exchange_and_add (&thunar_vfs_ref_count, 1) == 0)
    {
      /* grab a reference on the mime database, so the global
       * instance stays alive while we use the ThunarVFS library.
       */
      thunar_vfs_mime_database = thunar_vfs_mime_database_get_default ();

      /* initialize the jobs framework */
      _thunar_vfs_job_init ();
    }
}



/**
 * thunar_vfs_shutdown:
 *
 * Shuts down the ThunarVFS library.
 **/
void
thunar_vfs_shutdown (void)
{
  extern void _thunar_vfs_job_shutdown ();
  extern void _thunar_vfs_mime_shutdown ();

  if (g_atomic_int_dec_and_test (&thunar_vfs_ref_count))
    {
      /* shutdown the jobs framework */
      _thunar_vfs_job_shutdown ();

      /* drop our reference on the global mime database */
      exo_object_unref (EXO_OBJECT (thunar_vfs_mime_database));
      thunar_vfs_mime_database = NULL;
    }
}



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
 * thunar_vfs_copy:
 * @source_uri_list : the list of #ThunarVfsURI<!---->s that should be copied.
 * @target_uri      : the #ThunarVfsURI of the target directory.
 * @error           : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsTransferJob, which can be used to copy
 * the files referenced by @source_uri_list to the directory referred
 * to by @target_uri. If the creation of the job failes for some reason,
 * %NULL will be returned and @error will be set to point to a #GError
 * describing the cause. Else the newly allocated #ThunarVfsTransferJob
 * will be returned and the caller is responsible to call
 * #thunar_vfs_job_unref().
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call #thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_copy (GList        *source_uri_list,
                 ThunarVfsURI *target_uri,
                 GError      **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (target_uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_transfer_job_new (source_uri_list, target_uri, FALSE, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



/**
 * thunar_vfs_move:
 * @source_uri_list : the list of #ThunarVfsURI<!---->s that should be moved.
 * @target_uri      : the #ThunarVfsURI of the target directory.
 * @error           : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsTransferJob, which can be used to move
 * the files referenced by @source_uri_list to the directory referred
 * to by @target_uri. If the creation of the job failes for some reason,
 * %NULL will be returned and @error will be set to point to a #GError
 * describing the cause. Else the newly allocated #ThunarVfsTransferJob
 * will be returned and the caller is responsible to call
 * #thunar_vfs_job_unref().
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call #thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_move (GList        *source_uri_list,
                 ThunarVfsURI *target_uri,
                 GError      **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (target_uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_transfer_job_new (source_uri_list, target_uri, TRUE, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



/**
 * thunar_vfs_unlink:
 * @uri_list : a list of #ThunarVfsURI<!---->s, that should be unlinked.
 * @error    : return location for errors or %NULL.
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
thunar_vfs_unlink (GList   *uri_list,
                   GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to allocate the job */
  job = thunar_vfs_unlink_job_new (uri_list, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



#define __THUNAR_VFS_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
