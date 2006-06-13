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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-mkdir-job.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#define g_unmkdir(path) (unmkdir ((path)))
#endif



static void thunar_vfs_mkdir_job_class_init (ThunarVfsMkdirJobClass *klass);
static void thunar_vfs_mkdir_job_finalize   (GObject                *object);
static void thunar_vfs_mkdir_job_execute    (ThunarVfsJob           *job);



struct _ThunarVfsMkdirJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsMkdirJob
{
  ThunarVfsInteractiveJob __parent__;
  GList                  *path_list;
};



static GObjectClass *thunar_vfs_mkdir_job_parent_class;



GType
thunar_vfs_mkdir_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                                 "ThunarVfsMkdirJob",
                                                 sizeof (ThunarVfsMkdirJobClass),
                                                 thunar_vfs_mkdir_job_class_init,
                                                 sizeof (ThunarVfsMkdirJob),
                                                 NULL,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_mkdir_job_class_init (ThunarVfsMkdirJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent class */
  thunar_vfs_mkdir_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_mkdir_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_mkdir_job_execute;
}



static void
thunar_vfs_mkdir_job_finalize (GObject *object)
{
  ThunarVfsMkdirJob *mkdir_job = THUNAR_VFS_MKDIR_JOB (object);

  /* release the path list */
  thunar_vfs_path_list_free (mkdir_job->path_list);

  (*G_OBJECT_CLASS (thunar_vfs_mkdir_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_mkdir_job_execute (ThunarVfsJob *job)
{
  ThunarVfsMkdirJob *mkdir_job = THUNAR_VFS_MKDIR_JOB (job);
  gdouble            percent;
  GError            *error;
  gchar             *absolute_path;
  gchar             *display_name;
  guint              completed = 0;
  guint              total;
  GList             *directories = NULL;
  GList             *lp;

  /* determine the total number of directories to create */
  total = g_list_length (mkdir_job->path_list);

  /* process all directories */
  for (lp = mkdir_job->path_list; lp != NULL; lp = lp->next)
    {
      /* check if the job was cancelled */
      if (thunar_vfs_job_cancelled (job))
        break;

      /* update the progress message */
      display_name = g_filename_display_name (thunar_vfs_path_get_name (lp->data));
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (mkdir_job), display_name);
      g_free (display_name);

      /* determine the absolute path */
      absolute_path = thunar_vfs_path_dup_string (lp->data);

      /* try to create the target directory */
      if (g_mkdir (absolute_path, 0755) < 0)
        {
          /* generate an error describing the problem */
          display_name = g_filename_display_name (absolute_path);
          error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                               _("Failed to create directory \"%s\": %s"),
                               display_name, g_strerror (errno));
          g_free (display_name);

          /* notify theuser about the problem and cancel the job */
          thunar_vfs_job_error (job, error);
          thunar_vfs_job_cancel (job);
          g_error_free (error);
        }
      else
        {
          /* feed a "created" event for the new directory */
          thunar_vfs_monitor_feed (THUNAR_VFS_INTERACTIVE_JOB (mkdir_job)->monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, lp->data);

          /* add the path to the list of successfully created directories */
          directories = g_list_prepend (directories, lp->data);
        }

      /* update the progress status */
      percent = (++completed * 100.0) / total;
      percent = CLAMP (percent, 0.0, 100.0);
      thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (mkdir_job), percent);

      /* release the absolute path */
      g_free (absolute_path);
    }

  /* emit the "new-files" signal if we have created any directories */
  if (G_LIKELY (directories != NULL))
    {
      thunar_vfs_interactive_job_new_files (THUNAR_VFS_INTERACTIVE_JOB (mkdir_job), directories);
      g_list_free (directories);
    }
}



/**
 * thunar_vfs_mkdir_job_new:
 * @path_list : the list of #ThunarVfsPath<!---->s to the directories that should be created.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsMkdirJob, that creates directories for all #ThunarVfsPath<!---->s
 * listed in @path_list.
 *
 * The caller is responsible to free the returned object using g_object_unref() when
 * no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsMkdirJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_mkdir_job_new (GList   *path_list,
                          GError **error)
{
  ThunarVfsMkdirJob *mkdir_job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the new job */
  mkdir_job = g_object_new (THUNAR_VFS_TYPE_MKDIR_JOB, NULL);
  mkdir_job->path_list = thunar_vfs_path_list_copy (path_list);

  return THUNAR_VFS_JOB (mkdir_job);
}



#define __THUNAR_VFS_MKDIR_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
