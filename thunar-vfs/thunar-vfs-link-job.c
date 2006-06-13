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

#include <thunar-vfs/thunar-vfs-link-job.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-xfer.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_unlink() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_unlink(path) (unlink ((path)))
#endif



static void thunar_vfs_link_job_class_init (ThunarVfsLinkJobClass *klass);
static void thunar_vfs_link_job_finalize   (GObject               *object);
static void thunar_vfs_link_job_execute    (ThunarVfsJob          *job);



struct _ThunarVfsLinkJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsLinkJob
{
  ThunarVfsInteractiveJob __parent__;

  /* source and target path list */
  GList *source_path_list;
  GList *target_path_list;
};



static GObjectClass *thunar_vfs_link_job_parent_class;



GType
thunar_vfs_link_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                                 "ThunarVfsLinkJob",
                                                 sizeof (ThunarVfsLinkJobClass),
                                                 thunar_vfs_link_job_class_init,
                                                 sizeof (ThunarVfsLinkJob),
                                                 NULL,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_link_job_class_init (ThunarVfsLinkJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent class */
  thunar_vfs_link_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_link_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_link_job_execute;
}



static void
thunar_vfs_link_job_finalize (GObject *object)
{
  ThunarVfsLinkJob *link_job = THUNAR_VFS_LINK_JOB (object);

  /* release the target path list */
  thunar_vfs_path_list_free (link_job->target_path_list);

  /* release the source path list */
  thunar_vfs_path_list_free (link_job->source_path_list);

  (*G_OBJECT_CLASS (thunar_vfs_link_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_link_job_execute (ThunarVfsJob *job)
{
  ThunarVfsLinkJob *link_job = THUNAR_VFS_LINK_JOB (job);
  ThunarVfsPath    *target_path;
  gboolean          overwrite;
  gdouble           percent;
  GError           *error = NULL;
  gchar             target_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar            *display_name;
  gchar            *message;
  guint             completed = 0;
  guint             total;
  GList            *target_path_list = NULL;
  GList            *sp;
  GList            *tp;

  /* determine the total number of source files */
  total = g_list_length (link_job->source_path_list);

  /* process all files */
  for (sp = link_job->source_path_list, tp = link_job->target_path_list; sp != NULL && tp != NULL; sp = sp->next, tp = tp->next)
    {
      /* check if the job was cancelled */
      if (thunar_vfs_job_cancelled (job))
        break;

      /* update the progress message */
      display_name = g_filename_display_name (thunar_vfs_path_get_name (sp->data));
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (link_job), display_name);
      g_free (display_name);

again:
      /* try to perform the symlink operation */
      if (thunar_vfs_xfer_link (sp->data, tp->data, &target_path, &error))
        {
          /* add the item to the target path list */
          target_path_list = g_list_prepend (target_path_list, target_path);
        }
      else if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_EXIST)
        {
          /* ask the user whether we should remove the target first */
          message = g_strdup_printf (_("%s.\n\nDo you want to overwrite it?"), error->message);
          overwrite = thunar_vfs_interactive_job_overwrite (THUNAR_VFS_INTERACTIVE_JOB (link_job), message);
          g_free (message);

          /* release the error */
          g_clear_error (&error);

          if (G_LIKELY (overwrite))
            {
              /* determine the absolute path to the target file */
              if (thunar_vfs_path_to_string (tp->data, target_absolute_path, sizeof (target_absolute_path), &error) < 0)
                {
error_and_cancel:
                  /* notify the user about the error */
                  thunar_vfs_job_error (job, error);
                  thunar_vfs_job_cancel (job);
                  g_clear_error (&error);
                  break;
                }

              /* try to unlink the target file */
              if (g_unlink (target_absolute_path) < 0 && errno != ENOENT)
                {
                  error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                       _("Failed to remove \"%s\": %s"), target_absolute_path,
                                       g_strerror (errno));
                  goto error_and_cancel;
                }

              /* try again... */
              goto again;
            }
        }
      else
        {
          /* ask the user whether to skip this file (used for cancellation only) */
          message = g_strdup_printf (_("%s.\n\nDo you want to skip it?"), error->message);
          thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (link_job), message);
          g_free (message);

          /* reset the error */
          g_clear_error (&error);
        }

      /* update the progress status */
      percent = (++completed * 100.0) / total;
      percent = CLAMP (percent, 0.0, 100.0);
      thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (link_job), percent);
    }

  /* emit the "new-files" signal if we have any new files */
  if (G_LIKELY (target_path_list != NULL))
    {
      thunar_vfs_interactive_job_new_files (THUNAR_VFS_INTERACTIVE_JOB (link_job), target_path_list);
      thunar_vfs_path_list_free (target_path_list);
    }
}



/**
 * thunar_vfs_link_job_new:
 * @source_path_list : the list of #ThunarVfsPath<!---->s to the source files.
 * @target_path_list : the list of #ThunarVfsPath<!---->s to the target files.
 * @error            : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsLinkJob, that symlinks all files from @source_path_list
 * to the paths listed in @target_path_list.
 *
 * The caller is responsible to free the returned object using g_object_unref() when
 * no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsLinkJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_link_job_new (GList   *source_path_list,
                         GList   *target_path_list,
                         GError **error)
{
  ThunarVfsLinkJob *link_job;

  g_return_val_if_fail (g_list_length (source_path_list) == g_list_length (target_path_list), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the new job */
  link_job = g_object_new (THUNAR_VFS_TYPE_LINK_JOB, NULL);
  link_job->source_path_list = thunar_vfs_path_list_copy (source_path_list);
  link_job->target_path_list = thunar_vfs_path_list_copy (target_path_list);

  return THUNAR_VFS_JOB (link_job);
}



#define __THUNAR_VFS_LINK_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
