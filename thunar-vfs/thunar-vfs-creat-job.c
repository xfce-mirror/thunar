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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-creat-job.h>
#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void thunar_vfs_creat_job_class_init (ThunarVfsCreatJobClass *klass);
static void thunar_vfs_creat_job_init       (ThunarVfsCreatJob      *creat_job);
static void thunar_vfs_creat_job_finalize   (GObject                *object);
static void thunar_vfs_creat_job_execute    (ThunarVfsJob           *job);



struct _ThunarVfsCreatJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsCreatJob
{
  ThunarVfsInteractiveJob __parent__;

  ThunarVfsMonitor *monitor;
  GList            *path_list;
};



static GObjectClass *thunar_vfs_creat_job_parent_class;



GType
thunar_vfs_creat_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsCreatJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_creat_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsCreatJob),
        0,
        (GInstanceInitFunc) thunar_vfs_creat_job_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                     I_("ThunarVfsCreatJob"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_creat_job_class_init (ThunarVfsCreatJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_vfs_creat_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_creat_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_creat_job_execute;
}



static void
thunar_vfs_creat_job_init (ThunarVfsCreatJob *creat_job)
{
  /* grab a reference on the VFS monitor */
  creat_job->monitor = thunar_vfs_monitor_get_default ();
}



static void
thunar_vfs_creat_job_finalize (GObject *object)
{
  ThunarVfsCreatJob *creat_job = THUNAR_VFS_CREAT_JOB (object);

  /* release our list of paths */
  thunar_vfs_path_list_free (creat_job->path_list);

  /* release the reference on the VFS monitor */
  g_object_unref (G_OBJECT (creat_job->monitor));

  (*G_OBJECT_CLASS (thunar_vfs_creat_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_creat_job_execute (ThunarVfsJob *job)
{
  ThunarVfsCreatJob *creat_job = THUNAR_VFS_CREAT_JOB (job);
  gboolean           overwrite;
  gboolean           skip;
  gdouble            percentage;
  gdouble            completed = 0.0;
  GError            *error = NULL;
  gchar              absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar             *display_name = NULL;
  gchar             *message;
  GList             *lp;
  gint               flags;
  gint               fd;

  /* process all paths */
  for (lp = creat_job->path_list; !thunar_vfs_job_cancelled (job) && lp != NULL; lp = lp->next)
    {
      /* determine the absolute path */
      if (thunar_vfs_path_to_string (lp->data, absolute_path, sizeof (absolute_path), &error) < 0)
        {
          thunar_vfs_job_error (job, error);
          g_error_free (error);
          break;
        }

      /* release the previous display name (if any) */
      g_free (display_name);

      /* determine a display name for the path */
      display_name = g_filename_display_name (absolute_path);

      /* update the info message */
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), display_name);

      /* first attempt for the open flags */
      flags = O_CREAT | O_EXCL | O_WRONLY;

again:
      /* try to create the file at the given path */
      fd = open (absolute_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
      if (G_UNLIKELY (fd < 0))
        {
          /* check if the file already exists */
          if (G_UNLIKELY (errno == EEXIST))
            {
              /* ask the user whether to override this path */
              message = g_strdup_printf (_("The file `%s' already exists. Do you want to replace it with an empty file?"), display_name);
              overwrite = thunar_vfs_interactive_job_overwrite (THUNAR_VFS_INTERACTIVE_JOB (job), message);
              g_free (message);

              /* check if we should unlink the old file */
              if (G_UNLIKELY (overwrite))
                {
                  /* try to unlink the file */
                  if (unlink (absolute_path) < 0)
                    {
                      /* ask the user whether to skip this path */
                      message = g_strdup_printf (_("Unable to remove `%s'.\n\nDo you want to skip it?"), display_name);
                      skip = thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (job), message);
                      g_free (message);

                      /* check if we should skip */
                      if (G_UNLIKELY (!skip))
                        break;
                    }
                  else
                    {
                      goto again;
                    }
                }
            }
          else
            {
              /* ask the user whether to skip this path */
              message = g_strdup_printf (_("Failed to create empty file `%s'.\n\nDo you want to skip it?"), display_name);
              skip = thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (job), message);
              g_free (message);

              /* check if we should skip */
              if (G_UNLIKELY (!skip))
                break;
            }
        }
      else
        {
          /* feed a change notification event */
          thunar_vfs_monitor_feed (creat_job->monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, lp->data);

          /* close the file */
          close (fd);
        }

      /* we've just completed another item */
      completed += 1;

      /* update the percentage */
      percentage = (completed * 100.0) / MAX (g_list_length (creat_job->path_list), 1);
      thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (job), CLAMP (percentage, 0.0, 100.0));
    }

  /* emit the "new-files" signal with the given path list (if not empty) */
  if (G_LIKELY (creat_job->path_list != NULL))
    thunar_vfs_interactive_job_new_files (THUNAR_VFS_INTERACTIVE_JOB (job), creat_job->path_list);

  /* cleanup */
  g_free (display_name);
}



/**
 * thunar_vfs_creat_job_new:
 * @path_list : the list of #ThunarVfsPath<!---->s to create.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsCreatJob to create new files at the
 * locations contained in the @path_list.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsCreatJob or %NULL
 *               on error.
 **/
ThunarVfsJob*
thunar_vfs_creat_job_new (GList   *path_list,
                          GError **error)
{
  ThunarVfsCreatJob *creat_job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the new job */
  creat_job = g_object_new (THUNAR_VFS_TYPE_CREAT_JOB, NULL);
  creat_job->path_list = thunar_vfs_path_list_copy (path_list);

  return THUNAR_VFS_JOB (creat_job);
}



#define __THUNAR_VFS_LINK_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
