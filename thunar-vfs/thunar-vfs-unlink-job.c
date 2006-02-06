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
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-scandir.h>
#include <thunar-vfs/thunar-vfs-thumb.h>
#include <thunar-vfs/thunar-vfs-unlink-job.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_remove(path) (remove ((path)))
#define g_unlink(path) (unlink ((path)))
#endif



static void thunar_vfs_unlink_job_class_init (ThunarVfsJobClass   *klass);
static void thunar_vfs_unlink_job_finalize   (GObject             *object);
static void thunar_vfs_unlink_job_execute    (ThunarVfsJob        *job);
static void thunar_vfs_unlink_job_remove     (ThunarVfsUnlinkJob  *unlink_job,
                                              ThunarVfsPath       *path);



struct _ThunarVfsUnlinkJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsUnlinkJob
{
  ThunarVfsInteractiveJob __parent__;

  GList                  *path_list;

  gint                    total_items;
  gint                    completed_items;
};



static GObjectClass *thunar_vfs_unlink_job_parent_class;



GType
thunar_vfs_unlink_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsUnlinkJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_unlink_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsUnlinkJob),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                     I_("ThunarVfsUnlinkJob"),
                                     &info, 0);
    }

  return type;
}



static void
thunar_vfs_unlink_job_class_init (ThunarVfsJobClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent class */
  thunar_vfs_unlink_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_unlink_job_finalize;

  klass->execute = thunar_vfs_unlink_job_execute;
}



static void
thunar_vfs_unlink_job_finalize (GObject *object)
{
  ThunarVfsUnlinkJob *unlink_job = THUNAR_VFS_UNLINK_JOB (object);

  /* free the path list */
  thunar_vfs_path_list_free (unlink_job->path_list);

  /* call the parents finalize method */
  (*G_OBJECT_CLASS (thunar_vfs_unlink_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_unlink_job_execute (ThunarVfsJob *job)
{
  ThunarVfsUnlinkJob *unlink_job = THUNAR_VFS_UNLINK_JOB (job);
  gdouble             percent;
  GError             *error = NULL;
  GList              *paths;
  GList              *lp;
  guint               n;

  /* tell the user that we're preparing the unlink job */
  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Preparing..."));

  /* recursively collect the paths */
  for (lp = unlink_job->path_list; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* scan the directory */
      paths = thunar_vfs_scandir (lp->data, &job->cancelled, THUNAR_VFS_SCANDIR_RECURSIVE, NULL, &error);
      if (G_UNLIKELY (error != NULL))
        {
          /* we can safely ignore ENOTDIR errors here */
          if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOTDIR)
            {
              /* inform the user about the problem and abort the job */
              thunar_vfs_job_error (job, error);
              g_error_free (error);
              return;
            }

          /* reset the error */
          g_error_free (error);
          error = NULL;
        }

      /* prepend the new paths to the existing list */
      unlink_job->path_list = g_list_concat (paths, unlink_job->path_list);
    }

  /* determine the number of files to remove */
  unlink_job->total_items = g_list_length (unlink_job->path_list);

  /* perform the removal of the paths */
  for (lp = unlink_job->path_list, n = 0; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next, ++n)
    {
      /* remove the file for the current path */
      thunar_vfs_unlink_job_remove (unlink_job, lp->data);

      /* increment the completed items count */
      ++unlink_job->completed_items;

      /* update the progress status */
      if (G_UNLIKELY ((n % 8) == 0 || lp->next == NULL))
        {
          percent = (unlink_job->completed_items * 100.0) / unlink_job->total_items;
          percent = CLAMP (percent, 0.0, 100.0);
          thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (job), percent);
        }
    }
}



static void
thunar_vfs_unlink_job_remove (ThunarVfsUnlinkJob *unlink_job,
                              ThunarVfsPath      *path)
{
  ThunarVfsInfo *info;
  gchar         *message;
  gchar         *absolute_path;
  gchar         *thumbnail_path;

  /* determine the absolute path to the file */
  absolute_path = thunar_vfs_path_dup_string (path);

  /* determine the info for the file */
  info = _thunar_vfs_info_new_internal (path, absolute_path, NULL);
  if (G_LIKELY (info != NULL))
    {
      /* update the progress message */
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (unlink_job), info->display_name);

      /* try to unlink the file */
      if (G_UNLIKELY (g_remove (absolute_path) < 0 && errno != ENOENT))
        {
          message = g_strdup_printf (_("Unable to remove %s.\n\nDo you want to skip it?"), info->display_name);
          thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (unlink_job), message);
          g_free (message);
        }
      else
        {
          /* feed a delete event to the vfs monitor */
          thunar_vfs_monitor_feed (THUNAR_VFS_INTERACTIVE_JOB (unlink_job)->monitor,
                                   THUNAR_VFS_MONITOR_EVENT_DELETED, info->path);

          /* delete thumbnails for regular files */
          if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
            {
              /* ditch the normal thumbnail (if any) */
              thumbnail_path = thunar_vfs_thumbnail_for_path (path, THUNAR_VFS_THUMB_SIZE_NORMAL);
              g_unlink (thumbnail_path);
              g_free (thumbnail_path);

              /* ditch the large thumbnail (if any) */
              thumbnail_path = thunar_vfs_thumbnail_for_path (path, THUNAR_VFS_THUMB_SIZE_LARGE);
              g_unlink (thumbnail_path);
              g_free (thumbnail_path);
            }
        }

      /* release the info */
      thunar_vfs_info_unref (info);
    }

  /* release the absolute path */
  g_free (absolute_path);
}



/**
 * thunar_vfs_unlink_job_new:
 * @path_list : a list of #ThunarVfsPath<!---->s, that should be unlinked.
 * @error     : return location for errors or %NULL.
 *
 * Tries to allocate a new #ThunarVfsUnlinkJob, which can be used to
 * unlink the given @path_list.
 *
 * Return value: the newly allocated #ThunarVfsUnlinkJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_unlink_job_new (GList   *path_list,
                           GError **error)
{
  ThunarVfsUnlinkJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job instance */
  job = g_object_new (THUNAR_VFS_TYPE_UNLINK_JOB, NULL);
  job->path_list = thunar_vfs_path_list_copy (path_list);

  return THUNAR_VFS_JOB (job);
}



#define __THUNAR_VFS_UNLINK_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
