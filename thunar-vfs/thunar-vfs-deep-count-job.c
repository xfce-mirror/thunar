/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include <thunar-vfs/thunar-vfs-deep-count-job.h>
#include <thunar-vfs/thunar-vfs-marshal.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* Signal identifiers */
enum
{
  STATUS_READY,
  LAST_SIGNAL,
};



static void     thunar_vfs_deep_count_job_class_init    (ThunarVfsJobClass      *klass);
static void     thunar_vfs_deep_count_job_finalize      (GObject                *object);
static void     thunar_vfs_deep_count_job_execute       (ThunarVfsJob           *job);
static gboolean thunar_vfs_deep_count_job_process       (ThunarVfsDeepCountJob  *deep_count_job,
                                                         const gchar            *dir_path,
                                                         struct stat            *statb);
static void     thunar_vfs_deep_count_job_status_ready  (ThunarVfsDeepCountJob  *deep_count_job);



struct _ThunarVfsDeepCountJobClass
{
  ThunarVfsJobClass __parent__;

  /* signals */
  void (*status_ready) (ThunarVfsJob *job,
                        guint64       total_size,
                        guint         file_count,
                        guint         directory_count,
                        guint         unreadable_directory_count);
};

struct _ThunarVfsDeepCountJob
{
  ThunarVfsJob   __parent__;
  ThunarVfsPath *path;

  /* the time of the last "status-ready" emission */
  GTimeVal       last_time;

  /* status information */
  guint64        total_size;
  guint          file_count;
  guint          directory_count;
  guint          unreadable_directory_count;
};



static GObjectClass *thunar_vfs_deep_count_job_parent_class;
static guint         deep_count_signals[LAST_SIGNAL];



GType
thunar_vfs_deep_count_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsDeepCountJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_deep_count_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsDeepCountJob),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_JOB, I_("ThunarVfsDeepCountJob"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_deep_count_job_class_init (ThunarVfsJobClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_deep_count_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_deep_count_job_finalize;

  klass->execute = thunar_vfs_deep_count_job_execute;

  /**
   * ThunarVfsDeepCountJob::status-ready:
   * @job                        : a #ThunarVfsJob.
   * @total_size                 : the total size in bytes.
   * @file_count                 : the number of files.
   * @directory_count            : the number of directories.
   * @unreadable_directory_count : the number of unreadable directories.
   *
   * Emitted by the @job to inform listeners about new status.
   **/
  deep_count_signals[STATUS_READY] =
    g_signal_new (I_("status-ready"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ThunarVfsDeepCountJobClass, status_ready),
                  NULL, NULL,
                  _thunar_vfs_marshal_VOID__UINT64_UINT_UINT_UINT,
                  G_TYPE_NONE, 4,
                  G_TYPE_UINT64,
                  G_TYPE_UINT,
                  G_TYPE_UINT,
                  G_TYPE_UINT);
}



static void
thunar_vfs_deep_count_job_finalize (GObject *object)
{
  ThunarVfsDeepCountJob *deep_count_job = THUNAR_VFS_DEEP_COUNT_JOB (object);

  /* release the base path */
  if (G_LIKELY (deep_count_job->path != NULL))
    thunar_vfs_path_unref (deep_count_job->path);

  (*G_OBJECT_CLASS (thunar_vfs_deep_count_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_deep_count_job_execute (ThunarVfsJob *job)
{
  ThunarVfsDeepCountJob *deep_count_job = THUNAR_VFS_DEEP_COUNT_JOB (job);
  struct stat            statb;
  GError                *error;
  gchar                 *absolute_path;

  /* try to stat the base path */
  absolute_path = thunar_vfs_path_dup_string (deep_count_job->path);
  if (G_UNLIKELY (g_stat (absolute_path, &statb) < 0))
    {
      /* tell the listeners that the job failed */
      error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      thunar_vfs_job_error (job, error);
      g_error_free (error);
    }
  else if (!S_ISDIR (statb.st_mode))
    {
      /* the base path is not a directory */
      deep_count_job->total_size += statb.st_size;
      deep_count_job->file_count += 1;
    }
  else
    {
      /* process the directory recursively */
      if (thunar_vfs_deep_count_job_process (deep_count_job, absolute_path, &statb))
        {
          /* emit "status-ready" signal */
          thunar_vfs_job_emit (THUNAR_VFS_JOB (deep_count_job), deep_count_signals[STATUS_READY],
                               0, deep_count_job->total_size, deep_count_job->file_count,
                               deep_count_job->directory_count, deep_count_job->unreadable_directory_count);
        }
      else
        {
          /* base directory not readable */
          error = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to read folder contents"));
          thunar_vfs_job_error (job, error);
          g_error_free (error);
        }
    }

  /* release the base path */
  g_free (absolute_path);
}



static gboolean
thunar_vfs_deep_count_job_process (ThunarVfsDeepCountJob *deep_count_job,
                                   const gchar           *dir_path,
                                   struct stat           *statb)
{
  const gchar *name;
  gchar       *path;
  GDir        *dp;

  /* try to open the directory */
  dp = g_dir_open (dir_path, 0, NULL);
  if (G_LIKELY (dp == NULL))
    {
      /* unreadable directory */
      return FALSE;
    }

  /* process all items in this directory */
  while (!thunar_vfs_job_cancelled (THUNAR_VFS_JOB (deep_count_job)))
    {
      /* determine the next item */
      name = g_dir_read_name (dp);
      if (G_UNLIKELY (name == NULL))
        break;

      /* check if we should emit "status-ready" */
      thunar_vfs_deep_count_job_status_ready (deep_count_job);

      /* determine the full path to the item */
      path = g_build_filename (dir_path, name, NULL);

      /* stat the item */
      if (G_LIKELY (g_stat (path, statb) == 0))
        {
          /* add the size of this item */
          deep_count_job->total_size += statb->st_size;

          /* check if we have a directory here */
          if (S_ISDIR (statb->st_mode))
            {
              /* process the directory recursively */
              if (thunar_vfs_deep_count_job_process (deep_count_job, path, statb))
                {
                  /* directory was readable */
                  deep_count_job->directory_count += 1;
                }
              else
                {
                  /* directory was unreadable */
                  deep_count_job->unreadable_directory_count += 1;
                }
            }
          else
            {
              /* count it as file */
              deep_count_job->file_count += 1;
            }
        }
      else
        {
          /* check if we have a broken symlink here */
          if (g_lstat (path, statb) == 0 && S_ISLNK (statb->st_mode))
            {
              /* count the broken symlink as file */
              deep_count_job->total_size += statb->st_size;
              deep_count_job->file_count += 1;
            }
        }

      /* release the path */
      g_free (path);
    }

  /* close the dir handle */
  g_dir_close (dp);

  /* readable directory */
  return TRUE;
}



static void
thunar_vfs_deep_count_job_status_ready (ThunarVfsDeepCountJob *deep_count_job)
{
  GTimeVal current_time;

  /* check if we should update (at most every 128 files, but not more than fourth per second) */
  if (((deep_count_job->unreadable_directory_count + deep_count_job->directory_count + deep_count_job->file_count)) % 128)
    {
      /* determine the current time */
      g_get_current_time (&current_time);

      /* check if more than 250ms elapsed since the last "status-ready" */
      if (((current_time.tv_sec - deep_count_job->last_time.tv_sec) * 1000 + (current_time.tv_usec - deep_count_job->last_time.tv_usec) / 1000) >= 250)
        {
          /* remember the current time */
          deep_count_job->last_time = current_time;

          /* emit "status-ready" signal */
          thunar_vfs_job_emit (THUNAR_VFS_JOB (deep_count_job), deep_count_signals[STATUS_READY],
                               0, deep_count_job->total_size, deep_count_job->file_count,
                               deep_count_job->directory_count, deep_count_job->unreadable_directory_count);
        }
    }
}



/**
 * thunar_vfs_deep_count_job_new:
 * @path : a #ThunarVfsPath.
 *
 * Allocates a new #ThunarVfsDeepCountJob, which counts
 * the size of the file at @path or if @path is a directory
 * counts the size of all items in the directory and its
 * subdirectories.
 *
 * The caller is responsible to free the returned job
 * using g_object_unref() when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsDeepCountJob.
 **/
ThunarVfsJob*
thunar_vfs_deep_count_job_new (ThunarVfsPath *path)
{
  ThunarVfsDeepCountJob *deep_count_job;

  g_return_val_if_fail (path != NULL, NULL);

  /* allocate the new job */
  deep_count_job = g_object_new (THUNAR_VFS_TYPE_DEEP_COUNT_JOB, NULL);
  deep_count_job->path = thunar_vfs_path_ref (path);

  return THUNAR_VFS_JOB (deep_count_job);
}

