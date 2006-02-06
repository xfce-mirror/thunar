/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * ownify it under the terms of the GNU Library General Public
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

#include <thunar-vfs/thunar-vfs-chown-job.h>
#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-scandir.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void     thunar_vfs_chown_job_class_init (ThunarVfsChownJobClass *klass);
static void     thunar_vfs_chown_job_init       (ThunarVfsChownJob      *chown_job);
static void     thunar_vfs_chown_job_finalize   (GObject                *object);
static void     thunar_vfs_chown_job_execute    (ThunarVfsJob           *job);
static gboolean thunar_vfs_chown_job_operate    (ThunarVfsChownJob      *chown_job,
                                                 ThunarVfsPath          *path,
                                                 GError                **error);



struct _ThunarVfsChownJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsChownJob
{
  ThunarVfsInteractiveJob __parent__;

  ThunarVfsMonitor *monitor;

  ThunarVfsPath    *path;
  gint              uid;
  gint              gid;
  gboolean          recursive;

  guint             total;
  guint             completed;
};



static GObjectClass *thunar_vfs_chown_job_parent_class;



GType
thunar_vfs_chown_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsChownJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_chown_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsChownJob),
        0,
        (GInstanceInitFunc) thunar_vfs_chown_job_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                     I_("ThunarVfsChownJob"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_chown_job_class_init (ThunarVfsChownJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_vfs_chown_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_chown_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_chown_job_execute;
}



static void
thunar_vfs_chown_job_init (ThunarVfsChownJob *chown_job)
{
  /* grab a reference on the VFS monitor */
  chown_job->monitor = thunar_vfs_monitor_get_default ();
}



static void
thunar_vfs_chown_job_finalize (GObject *object)
{
  ThunarVfsChownJob *chown_job = THUNAR_VFS_CHOWN_JOB (object);

  /* release the reference on the VFS monitor */
  g_object_unref (G_OBJECT (chown_job->monitor));

  /* release the base path */
  thunar_vfs_path_unref (chown_job->path);

  (*G_OBJECT_CLASS (thunar_vfs_chown_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_chown_job_execute (ThunarVfsJob *job)
{
  ThunarVfsChownJob *chown_job = THUNAR_VFS_CHOWN_JOB (job);
  gboolean           skip;
  gdouble            percentage;
  GError            *error = NULL;
  gchar             *message;
  GList             *path_list = NULL;
  GList             *lp;

  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Collecting files..."));

  /* check if we should operate recursively and collect the paths */
  if (G_UNLIKELY (chown_job->recursive))
    path_list = thunar_vfs_scandir (chown_job->path, &job->cancelled, THUNAR_VFS_SCANDIR_RECURSIVE, NULL, NULL);
  path_list = thunar_vfs_path_list_prepend (path_list, chown_job->path);

  /* determine the total number of paths (atleast one!) */
  chown_job->total = g_list_length (path_list);

  /* process all paths */
  for (lp = path_list; !thunar_vfs_job_cancelled (job) && lp != NULL; lp = lp->next)
    {
      /* try to perform the operation */
      if (!thunar_vfs_chown_job_operate (chown_job, lp->data, &error))
        {
          /* no need to ask if this is the last file */
          if (G_UNLIKELY (lp->next == NULL))
            break;

          /* ask the user whether we should skip the file */
          message = g_strdup_printf (_("%s.\n\nDo you want to skip it?"), error->message);
          skip = thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (job), message);
          g_clear_error (&error);
          g_free (message);

          /* check if we should skip */
          if (G_UNLIKELY (!skip))
            break;
        }

      /* we've just completed another item */
      chown_job->completed += 1;

      /* update the percentage */
      percentage = (chown_job->completed * 100.0) / chown_job->total;
      thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (job), CLAMP (percentage, 0.0, 100.0));

      /* release the path */
      thunar_vfs_path_unref (lp->data);
    }

  /* emit an error if some operation failed */
  if (G_UNLIKELY (error != NULL))
    {
      thunar_vfs_job_error (job, error);
      g_error_free (error);
    }

  /* release the (remaining) path list */
  for (; lp != NULL; lp = lp->next)
    thunar_vfs_path_unref (lp->data);
  g_list_free (path_list);
}



static gboolean
thunar_vfs_chown_job_operate (ThunarVfsChownJob *chown_job,
                              ThunarVfsPath     *path,
                              GError           **error)
{
  const gchar *message;
  struct stat  statb;
  gchar        absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar       *display_name;
  gid_t        gid;
  uid_t        uid;

  /* update the progress message */
  display_name = g_filename_display_name (thunar_vfs_path_get_name (path));
  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (chown_job), display_name);
  g_free (display_name);

  /* determine the absolute path */
  if (thunar_vfs_path_to_string (path, absolute_path, sizeof (absolute_path), error) < 0)
    return FALSE;

  /* try to stat the file */
  if (stat (absolute_path, &statb) < 0)
    {
      /* we just ignore ENOENT here */
      if (G_UNLIKELY (errno == ENOENT))
        return TRUE;

      message = _("Failed to determine file info of `%s': %s");
      goto error;
    }

  /* determine new gid/uid */
  gid = (chown_job->gid < 0) ? statb.st_gid : chown_job->gid;
  uid = (chown_job->uid < 0) ? statb.st_uid : chown_job->uid;

  /* try to apply the new gid/uid */
  if (chown (absolute_path, uid, gid) < 0)
    {
      /* again, ignore ENOENT */
      if (G_UNLIKELY (errno == ENOENT))
        return TRUE;

      if (chown_job->uid < 0)
        message = _("Failed to change file owner of `%s': %s");
      else
        message = _("Failed to change file group of `%s': %s");
      goto error;
    }

  /* feed a change notification event */
  thunar_vfs_monitor_feed (chown_job->monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, path);

  /* we did it */
  return TRUE;

error:
  display_name = g_filename_display_name (absolute_path);
  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), message, display_name, g_strerror (errno));
  g_free (display_name);
  return FALSE;
}



/**
 * thunar_vfs_chown_job_new:
 * @path      : the base path.
 * @uid       : the new user id or %-1 to keep the old uid.
 * @gid       : the new group id or %-1 to keep the old gid.
 * @recursive : whether to operate recursively.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsChownJob instance, which is
 * used to change the owne of @path (and maybe subfiles
 * and subfolders, depending on @recursive).
 *
 * Return value: the newly allocated #ThunarVfsChownJob.
 **/
ThunarVfsJob*
thunar_vfs_chown_job_new (ThunarVfsPath *path,
                          gint           uid,
                          gint           gid,
                          gboolean       recursive,
                          GError       **error)
{
  ThunarVfsChownJob *chown_job;
  
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (uid >= 0 || gid >= 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and initialize the new job */
  chown_job = g_object_new (THUNAR_VFS_TYPE_CHOWN_JOB, NULL);
  chown_job->path = thunar_vfs_path_ref (path);
  chown_job->uid = uid;
  chown_job->gid = gid;
  chown_job->recursive = recursive;

  return THUNAR_VFS_JOB (chown_job);
}



#define __THUNAR_VFS_TRANSFER_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
