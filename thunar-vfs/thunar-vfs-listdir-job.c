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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-listdir-job.h>
#include <thunar-vfs/thunar-vfs-scandir.h>
#include <thunar-vfs/thunar-vfs-alias.h>



enum
{
  INFOS_READY,
  LAST_SIGNAL,
};



typedef struct _ThunarVfsListdirJobTask ThunarVfsListdirJobTask;



static void thunar_vfs_listdir_job_class_init (ThunarVfsJobClass       *klass);
static void thunar_vfs_listdir_job_finalize   (GObject                 *object);
static void thunar_vfs_listdir_job_execute    (ThunarVfsJob            *job);
static void thunar_vfs_listdir_job_task       (ThunarVfsListdirJobTask *task);


struct _ThunarVfsListdirJobClass
{
  ThunarVfsJobClass __parent__;
};

struct _ThunarVfsListdirJob
{
  ThunarVfsJob __parent__;
  ThunarVfsPath *path;
};

struct _ThunarVfsListdirJobTask
{
  GList *first;
  GList *last;
  guint  floc;
  gchar  fpath[THUNAR_VFS_PATH_MAXSTRLEN + 128];
};



static GObjectClass *thunar_vfs_listdir_job_parent_class;
static guint         listdir_signals[LAST_SIGNAL];



GType
thunar_vfs_listdir_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsListdirJobClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_listdir_job_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsListdirJob),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_JOB,
                                     I_("ThunarVfsListdirJob"),
                                     &info, 0);
    }

  return type;
}



static void
thunar_vfs_listdir_job_class_init (ThunarVfsJobClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent class */
  thunar_vfs_listdir_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_listdir_job_finalize;

  klass->execute = thunar_vfs_listdir_job_execute;

  /**
   * ThunarVfsListdirJob::infos-ready:
   * @job   : a #ThunarVfsJob.
   * @infos : a list of #ThunarVfsInfo<!---->s.
   *
   * This signal is emitted whenever there's a new bunch of
   * #ThunarVfsInfo<!---->s ready. This signal is garantied
   * to be never emitted with an @infos parameter of %NULL.
   **/
  listdir_signals[INFOS_READY] =
    g_signal_new (I_("infos-ready"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
thunar_vfs_listdir_job_finalize (GObject *object)
{
  ThunarVfsListdirJob *listdir_job = THUNAR_VFS_LISTDIR_JOB (object);

  /* free the folder path */
  if (G_LIKELY (listdir_job->path != NULL))
    thunar_vfs_path_unref (listdir_job->path);

  /* call the parents finalize method */
  (*G_OBJECT_CLASS (thunar_vfs_listdir_job_parent_class)->finalize) (object);
}



static gint
pathcmp (gconstpointer a,
         gconstpointer b)
{
  return -strcmp (thunar_vfs_path_get_name (a),
                  thunar_vfs_path_get_name (b));
}



static void
thunar_vfs_listdir_job_execute (ThunarVfsJob *job)
{
  GThreadPool *pool;
  GError      *error = NULL;
  GList       *list = NULL;
  GList       *lp;
  GList       *hp;
  guint        n;

  /* scan the given directory */
  list = thunar_vfs_scandir (THUNAR_VFS_LISTDIR_JOB (job)->path, THUNAR_VFS_SCANDIR_FOLLOW_LINKS, pathcmp, &error);
  if (G_LIKELY (list != NULL))
    {
      for (lp = hp = list, n = 0; lp != NULL; lp = lp->next, ++n)
        if ((n % 2) == 0)
          hp = hp->next;

      /* no need to parallelize for really small folders */
      if (G_UNLIKELY (n < 50))
        {
          /* initialize the task struct */
          ThunarVfsListdirJobTask task;
          task.first = list;
          task.last = NULL;

          /* put in the absolute path to the folder */
          task.floc = thunar_vfs_path_to_string (THUNAR_VFS_LISTDIR_JOB (job)->path, task.fpath, sizeof (task.fpath), &error);
          if (G_LIKELY (task.floc > 0))
            {
              /* append a path separator to the absolute folder path, so we can easily generate child paths */
              task.fpath[task.floc - 1] = G_DIR_SEPARATOR;

              /* run the task */
              thunar_vfs_listdir_job_task (&task);
            }
        }
      else
        {
          /* allocate memory for 2 tasks */
          ThunarVfsListdirJobTask tasks[2];

          /* initialize the first task */
          tasks[0].first = list;
          tasks[0].last = NULL;

          /* put in the absolute path to the folder */
          tasks[0].floc = thunar_vfs_path_to_string (THUNAR_VFS_LISTDIR_JOB (job)->path, tasks[0].fpath, sizeof (tasks[0].fpath), &error);
          if (G_LIKELY (tasks[0].floc > 0))
            {
              /* allocate a thread pool for the first task */
              pool = g_thread_pool_new ((GFunc) thunar_vfs_listdir_job_task, NULL, 1, FALSE, NULL);

              /* append a path separator to the absolute folder path, so we can easily generate child paths */
              tasks[0].fpath[tasks[0].floc - 1] = G_DIR_SEPARATOR;

              /* terminate the list for the first task */
              hp->prev->next = NULL;
              hp->prev = NULL;

              /* launch the first task */
              g_thread_pool_push (pool, &tasks[0], NULL);

              /* initialize the second task */
              tasks[1].first = hp;
              tasks[1].last = NULL;
              tasks[1].floc = tasks[0].floc;
              memcpy (tasks[1].fpath, tasks[0].fpath, tasks[0].floc);

              /* run the second task */
              thunar_vfs_listdir_job_task (&tasks[1]);

              /* wait for the first task to finish */
              g_thread_pool_free (pool, FALSE, TRUE);

              /* concatenate the info lists */
              if (G_UNLIKELY (tasks[0].last == NULL))
                {
                  list = tasks[1].first;
                }
              else
                {
                  tasks[0].last->next = tasks[1].first;
                  if (G_LIKELY (tasks[1].first != NULL))
                    tasks[1].first->prev = tasks[0].last;
                }
            }
        }

      /* release the path list in case of an early error */
      if (G_UNLIKELY (error != NULL))
        {
          thunar_vfs_path_list_free (list);
          list = NULL;
        }
    }

  /* emit appropriate signals */
  if (G_UNLIKELY (error != NULL))
    {
      thunar_vfs_job_error (job, error);
      g_error_free (error);
    }
  else if (G_LIKELY (list != NULL))
    {
      thunar_vfs_job_emit (job, listdir_signals[INFOS_READY], 0, list);
    }

  /* cleanup */
  thunar_vfs_info_list_free (list);
}



static void
thunar_vfs_listdir_job_task (ThunarVfsListdirJobTask *task)
{
  ThunarVfsInfo *info;
  GList         *sp;
  GList         *tp;

  for (sp = tp = task->first; sp != NULL; sp = sp->next)
    {
      /* generate the absolute path to the file */
      g_strlcpy (task->fpath + task->floc, thunar_vfs_path_get_name (sp->data), sizeof (task->fpath) - task->floc);

      /* try to determine the file info */
      info = _thunar_vfs_info_new_internal (sp->data, task->fpath, NULL);

      /* release the reference on the path (the info holds the reference now) */
      thunar_vfs_path_unref (sp->data);

      /* replace the path with the info on the list */
      if (G_LIKELY (info != NULL))
        {
          task->last = tp;
          tp->data = info;
          tp = tp->next;
        }
    }

  /* release the not-filled list items (only non-NULL in case of an info error) */
  if (G_UNLIKELY (tp != NULL))
    {
      if (G_LIKELY (tp->prev != NULL))
        tp->prev->next = NULL;
      g_list_free (tp);
    }
}



/**
 * thunar_vfs_job_new:
 * @folder_path : the #ThunarVfsPath of the directory whose contents to query.
 *
 * Allocates a new #ThunarVfsListdirJob object, which can be used to
 * query the contents of the directory @folder_path.
 *
 * You need to call thunar_vfs_job_launch() in order to start the
 * job. You may want to connect to ::finished, ::error-occurred and
 * ::infos-ready prior to launching the job.
 *
 * The caller is responsible to call g_object_unref() on the
 * returned object.
 *
 * Return value: the newly allocated #ThunarVfsJob, which performs the
 *               directory listing.
 **/
ThunarVfsJob*
thunar_vfs_listdir_job_new (ThunarVfsPath *folder_path)
{
  ThunarVfsListdirJob *listdir_job;

  listdir_job = g_object_new (THUNAR_VFS_TYPE_LISTDIR_JOB, NULL);
  listdir_job->path = thunar_vfs_path_ref (folder_path);

  return THUNAR_VFS_JOB (listdir_job);
}



#define __THUNAR_VFS_LISTDIR_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
