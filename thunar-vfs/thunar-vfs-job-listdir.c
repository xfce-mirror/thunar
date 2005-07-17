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



typedef struct _ThunarVfsJobListdir ThunarVfsJobListdir;



#define THUNAR_VFS_TYPE_JOB_LISTDIR     (thunar_vfs_job_listdir_get_type ())
#define THUNAR_VFS_JOB_LISTDIR(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_JOB_LISTDIR, ThunarVfsJobListdir))
#define THUNAR_VFS_IS_JOB_LISTDIR(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_JOB_LISTDIR))



static GType thunar_vfs_job_listdir_get_type      (void) G_GNUC_CONST;
static void  thunar_vfs_job_listdir_register_type (GType             *type);
static void  thunar_vfs_job_listdir_class_init    (ThunarVfsJobClass *klass);
static void  thunar_vfs_job_listdir_callback      (ThunarVfsJob      *job);
static void  thunar_vfs_job_listdir_execute       (ThunarVfsJob      *job);
static void  thunar_vfs_job_listdir_finalize      (ThunarVfsJob      *job);


struct _ThunarVfsJobListdir
{
  ThunarVfsJob __parent__;

  ThunarVfsURI *uri;
  GSList       *infos;

  ThunarVfsJobListdirCallback callback;
  gpointer                    user_data;
};



static GType
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
  klass->callback = thunar_vfs_job_listdir_callback;
  klass->execute = thunar_vfs_job_listdir_execute;
  klass->finalize = thunar_vfs_job_listdir_finalize;
}



static void
thunar_vfs_job_listdir_callback (ThunarVfsJob *job)
{
  (*THUNAR_VFS_JOB_LISTDIR (job)->callback) (job, THUNAR_VFS_JOB_LISTDIR (job)->infos,
                                             THUNAR_VFS_JOB_LISTDIR (job)->user_data);
}



static void
thunar_vfs_job_listdir_execute (ThunarVfsJob *job)
{
  ThunarVfsJobListdir *job_listdir = THUNAR_VFS_JOB_LISTDIR (job);
  ThunarVfsInfo       *info;
  struct dirent        d_buffer;
  struct dirent       *d;
  GStringChunk        *names_chunk;
  ThunarVfsURI        *file_uri;
  GSList              *names;
  GSList              *lp;
  time_t               last_check_time;
  time_t               current_time;
  DIR                 *dp;

  dp = opendir (thunar_vfs_uri_get_path (job_listdir->uri));
  if (G_UNLIKELY (dp == NULL))
    {
      thunar_vfs_job_set_error (job, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
    }
  else
    {
      names_chunk = g_string_chunk_new (4 * 1024);
      names = NULL;

      /* We read the file names first here to
       * (hopefully) avoid a bit unnecessary
       * disk seeking.
       */
      for (;;)
        {
#ifdef HAVE_READDIR_R
          if (G_UNLIKELY (readdir_r (dp, &d_buffer, &d) < 0))
            {
              thunar_vfs_job_set_error (job, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
              break;
            }
#else
          G_LOCK (readdir);
          d = readdir (dp);
          if (G_LIKELY (d != NULL))
            {
              memcpy (&d_buffer, d, sizeof (*d));
              d = &d_buffer;
            }
          G_UNLOCK (readdir);
#endif

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
          if (thunar_vfs_job_cancelled (job) || thunar_vfs_job_failed (job))
            break;

          file_uri = thunar_vfs_uri_relative (job_listdir->uri, lp->data);
          info = thunar_vfs_info_new_for_uri (file_uri, NULL);
          if (G_LIKELY (info != NULL))
            job_listdir->infos = g_slist_prepend (job_listdir->infos, info);
          thunar_vfs_uri_unref (file_uri);

          current_time = time (NULL);
          if (current_time - last_check_time > 2)
            {
              last_check_time = current_time;
              thunar_vfs_job_callback (job);
              thunar_vfs_info_list_free (job_listdir->infos);
              job_listdir->infos = NULL;
            }
        }

      /* free the names */
      g_string_chunk_free (names_chunk);
      g_slist_free (names);
    }

  /* we did it */
  thunar_vfs_job_finish (job);
  thunar_vfs_job_callback (job);
}



static void
thunar_vfs_job_listdir_finalize (ThunarVfsJob *job)
{
  ThunarVfsJobListdir *job_listdir = THUNAR_VFS_JOB_LISTDIR (job);

  /* free the folder uri */
  if (G_LIKELY (job_listdir->uri != NULL))
    thunar_vfs_uri_unref (job_listdir->uri);

  /* free the leftover files */
  thunar_vfs_info_list_free (job_listdir->infos);
}



/**
 * thunar_vfs_job_listdir:
 * @folder_uri : the #ThunarVfsURI of the directory whose content to query.
 * @callback   : the callback function to handle the retrieved infos.
 * @user_data  : additional data to pass to @callback.
 *
 * FIXME: DOC!
 *
 * The caller is responsible for freeing the returned #ThunarVfsJob
 * using the #thunar_vfs_job_unref() function.
 *
 * Return value: the newly allocated #ThunarVfsJob, which performs
 *               the directory listing.
 **/
ThunarVfsJob*
thunar_vfs_job_listdir (ThunarVfsURI               *folder_uri,
                        ThunarVfsJobListdirCallback callback,
                        gpointer                    user_data)
{
  ThunarVfsJobListdir *job_listdir;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (folder_uri), NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  job_listdir = (ThunarVfsJobListdir *) g_type_create_instance (THUNAR_VFS_TYPE_JOB_LISTDIR);
  job_listdir->uri = thunar_vfs_uri_ref (folder_uri);
  job_listdir->callback = callback;
  job_listdir->user_data = user_data;

  return thunar_vfs_job_schedule (THUNAR_VFS_JOB (job_listdir));
}


