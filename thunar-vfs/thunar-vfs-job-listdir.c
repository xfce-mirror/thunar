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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <thunar-vfs/thunar-vfs-job-listdir.h>
#include <thunar-vfs/thunar-vfs-sysdep.h>



enum
{
  ERROR_OCCURRED,
  INFOS_READY,
  LAST_SIGNAL,
};



static void  thunar_vfs_job_listdir_register_type (GType             *type);
static void  thunar_vfs_job_listdir_class_init    (ThunarVfsJobClass *klass);
static void  thunar_vfs_job_listdir_execute       (ThunarVfsJob      *job);
static void  thunar_vfs_job_listdir_finalize      (ThunarVfsJob      *job);


struct _ThunarVfsJobListdir
{
  ThunarVfsJob __parent__;

  ThunarVfsURI *uri;
};



static guint listdir_signals[LAST_SIGNAL];



GType
thunar_vfs_job_listdir_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_job_listdir_register_type, &type);

  return type;
}



static void
thunar_vfs_job_listdir_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsJobClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_job_listdir_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsJobListdir),
    0,
    NULL,
    NULL,
  };

  *type = g_type_register_static (THUNAR_VFS_TYPE_JOB,
                                  "ThunarVfsJobListdir",
                                  &info, 0);
}



static void
thunar_vfs_job_listdir_class_init (ThunarVfsJobClass *klass)
{
  klass->execute = thunar_vfs_job_listdir_execute;
  klass->finalize = thunar_vfs_job_listdir_finalize;

  /**
   * ThunarVfsJobListdir::error-occurred:
   * @job   : a #ThunarVfsJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while listing the directory
   * contents.
   **/
  listdir_signals[ERROR_OCCURRED] =
    g_signal_new ("error-occurred",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsJobListdir::infos-ready:
   * @job   : a #ThunarVfsJob.
   * @infos : a list of #ThunarVfsInfo<!---->s.
   *
   * This signal is emitted whenever there's a new bunch of
   * #ThunarVfsInfo<!---->s ready. This signal is garantied
   * to be never emitted with an @infos parameter of %NULL.
   **/
  listdir_signals[INFOS_READY] =
    g_signal_new ("infos-ready",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
thunar_vfs_job_listdir_execute (ThunarVfsJob *job)
{
  ThunarVfsInfo *info;
  struct dirent  d_buffer;
  struct dirent *d;
  GStringChunk  *names_chunk;
  ThunarVfsURI  *file_uri;
  GError        *error = NULL;
  GSList        *infos = NULL;
  GSList        *names = NULL;
  GSList        *lp;
  time_t         last_check_time;
  time_t         current_time;
  DIR           *dp;

  dp = opendir (thunar_vfs_uri_get_path (THUNAR_VFS_JOB_LISTDIR (job)->uri));
  if (G_UNLIKELY (dp == NULL))
    {
      error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
    }
  else
    {
      names_chunk = g_string_chunk_new (4 * 1024);

      /* We read the file names first here to
       * (hopefully) avoid a bit unnecessary
       * disk seeking.
       */
      for (;;)
        {
          if (G_UNLIKELY (!_thunar_vfs_sysdep_readdir (dp, &d_buffer, &d, &error)))
            break;

          /* check for end of directory */
          if (G_UNLIKELY (d == NULL))
              break;

          names = g_slist_insert_sorted (names, g_string_chunk_insert (names_chunk, d->d_name), (GCompareFunc) strcmp);
        }

      closedir (dp);

      last_check_time = time (NULL);

      /* Next we query the info for each of the file names
       * queried in the loop above.
       */
      for (lp = names; lp != NULL; lp = lp->next)
        {
          /* check for cancellation/failure condition */
          if (thunar_vfs_job_cancelled (job) || error != NULL)
            break;

          file_uri = thunar_vfs_uri_relative (THUNAR_VFS_JOB_LISTDIR (job)->uri, lp->data);
          info = thunar_vfs_info_new_for_uri (file_uri, NULL);
          if (G_LIKELY (info != NULL))
            infos = g_slist_prepend (infos, info);
          thunar_vfs_uri_unref (file_uri);

          current_time = time (NULL);
          if (current_time - last_check_time > 2 && infos != NULL)
            {
              last_check_time = current_time;
              thunar_vfs_job_emit (job, listdir_signals[INFOS_READY], 0, infos);
              thunar_vfs_info_list_free (infos);
              infos = NULL;
            }
        }

      /* free the names */
      g_string_chunk_free (names_chunk);
      g_slist_free (names);
    }

  /* emit appropriate signals */
  if (G_UNLIKELY (error != NULL))
    {
      thunar_vfs_job_emit (job, listdir_signals[ERROR_OCCURRED], 0, error);
      g_error_free (error);
    }
  else if (G_LIKELY (infos != NULL))
    {
      thunar_vfs_job_emit (job, listdir_signals[INFOS_READY], 0, infos);
    }

  /* cleanup */
  thunar_vfs_info_list_free (infos);
}



static void
thunar_vfs_job_listdir_finalize (ThunarVfsJob *job)
{
  ThunarVfsJobListdir *job_listdir = THUNAR_VFS_JOB_LISTDIR (job);

  /* free the folder uri */
  if (G_LIKELY (job_listdir->uri != NULL))
    thunar_vfs_uri_unref (job_listdir->uri);
}



/**
 * thunar_vfs_job_new:
 * @folder_uri : the #ThunarVfsURI of the directory whose contents to query.
 *
 * Allocates a new #ThunarVfsJobListdir object, which can be used to
 * query the contents of the directory @folder_uri.
 *
 * You need to call #thunar_vfs_job_launch() in order to start the
 * job. You may want to connect to ::finished, ::error-occurred and
 * ::infos-ready prior to launching the job.
 *
 * The caller is responsible to call #thunar_vfs_job_unref() on the
 * returned object.
 *
 * Return value: the newly allocated #ThunarVfsJob, which performs the
 *               directory listing.
 **/
ThunarVfsJob*
thunar_vfs_job_listdir_new (ThunarVfsURI *folder_uri)
{
  ThunarVfsJobListdir *job_listdir;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (folder_uri), NULL);

  job_listdir = (ThunarVfsJobListdir *) g_type_create_instance (THUNAR_VFS_TYPE_JOB_LISTDIR);
  job_listdir->uri = thunar_vfs_uri_ref (folder_uri);

  return THUNAR_VFS_JOB (job_listdir);
}


