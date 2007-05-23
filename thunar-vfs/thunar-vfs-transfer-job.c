/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-vfs/thunar-vfs-io-ops.h>
#include <thunar-vfs/thunar-vfs-io-scandir.h>
#include <thunar-vfs/thunar-vfs-job-private.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>
#include <thunar-vfs/thunar-vfs-alias.h>



typedef struct _ThunarVfsTransferNode ThunarVfsTransferNode;



static void                   thunar_vfs_transfer_job_class_init    (ThunarVfsTransferJobClass *klass);
static void                   thunar_vfs_transfer_job_finalize      (GObject                   *object);
static void                   thunar_vfs_transfer_job_execute       (ThunarVfsJob              *job);
static gboolean               thunar_vfs_transfer_job_progress      (ThunarVfsFileSize          chunk_size,
                                                                     gpointer                   callback_data);
static gboolean               thunar_vfs_transfer_job_copy_file     (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsPath             *source_path,
                                                                     ThunarVfsPath             *target_path,
                                                                     ThunarVfsPath            **target_path_return,
                                                                     GError                   **error);
static void                   thunar_vfs_transfer_job_node_copy     (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsTransferNode     *transfer_node,
                                                                     ThunarVfsPath             *target_path,
                                                                     ThunarVfsPath             *target_parent_path,
                                                                     GList                    **target_path_list_return,
                                                                     GError                   **error);
static void                   thunar_vfs_transfer_node_free         (ThunarVfsTransferNode     *transfer_node);
static gboolean               thunar_vfs_transfer_node_collect      (ThunarVfsTransferNode     *transfer_node,
                                                                     ThunarVfsFileSize         *total_size_return,
                                                                     volatile gboolean         *cancelled,
                                                                     GError                   **error);



struct _ThunarVfsTransferJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsTransferJob
{
  ThunarVfsInteractiveJob __parent__;

  /* whether to move files instead of copying them */
  gboolean                move;

  /* the source nodes and the matching target paths */
  GList                  *source_node_list;
  GList                  *target_path_list;

  /* the amount of completeness */
  ThunarVfsFileSize       total_size;
  ThunarVfsFileSize       completed_size;
};

struct _ThunarVfsTransferNode
{
  ThunarVfsPath         *source_path;
  ThunarVfsTransferNode *next;
  ThunarVfsTransferNode *children;
};



static GObjectClass *thunar_vfs_transfer_job_parent_class;



GType
thunar_vfs_transfer_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                                 "ThunarVfsTransferJob",
                                                 sizeof (ThunarVfsTransferJobClass),
                                                 thunar_vfs_transfer_job_class_init,
                                                 sizeof (ThunarVfsTransferJob),
                                                 NULL,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_transfer_job_class_init (ThunarVfsTransferJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_vfs_transfer_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_transfer_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_transfer_job_execute;
}



static void
thunar_vfs_transfer_job_finalize (GObject *object)
{
  ThunarVfsTransferJob *transfer_job = THUNAR_VFS_TRANSFER_JOB (object);

  /* release the source node list */
  g_list_foreach (transfer_job->source_node_list, (GFunc) thunar_vfs_transfer_node_free, NULL);
  g_list_free (transfer_job->source_node_list);

  /* release the target path list */
  thunar_vfs_path_list_free (transfer_job->target_path_list);

  /* call the parents finalize method */
  (*G_OBJECT_CLASS (thunar_vfs_transfer_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_transfer_job_execute (ThunarVfsJob *job)
{
  ThunarVfsTransferNode *transfer_node;
  ThunarVfsTransferJob  *transfer_job = THUNAR_VFS_TRANSFER_JOB (job);
  ThunarVfsPath         *target_path;
  GError                *err = NULL;
  GList                 *new_files_list = NULL;
  GList                 *snext;
  GList                 *tnext;
  GList                 *sp;
  GList                 *tp;

  _thunar_vfs_return_if_fail (g_list_length (transfer_job->source_node_list) == g_list_length (transfer_job->target_path_list));
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_TRANSFER_JOB (transfer_job));

  /* update the progress information */
  _thunar_vfs_job_info_message (job, _("Collecting files..."));

  /* collect all source transfer nodes (try to move the stuff first) */
  for (sp = transfer_job->source_node_list, tp = transfer_job->target_path_list; sp != NULL; sp = snext, tp = tnext)
    {
      /* determine the next list items */
      snext = sp->next;
      tnext = tp->next;

      /* determine the current source node */
      transfer_node = sp->data;

      /* try to move the file/folder directly, otherwise fallback to copy logic */
      if (transfer_job->move && _thunar_vfs_io_ops_move_file (transfer_node->source_path, tp->data, &target_path, NULL))
        {
          /* add the target file to the new files list */
          new_files_list = g_list_prepend (new_files_list, target_path);

          /* move worked, don't need to keep this node */
          thunar_vfs_transfer_node_free (transfer_node);
          thunar_vfs_path_unref (tp->data);

          /* drop the matching list items */
          transfer_job->source_node_list = g_list_delete_link (transfer_job->source_node_list, sp);
          transfer_job->target_path_list = g_list_delete_link (transfer_job->target_path_list, tp);
        }
      else if (!thunar_vfs_transfer_node_collect (transfer_node, &transfer_job->total_size, &job->cancelled, &err))
        {
          /* failed to collect, cannot continue */
          break;
        }
    }

  /* check if we succeed so far */
  if (G_LIKELY (err == NULL))
    {
      /* perform the copy recursively for all source transfer nodes, that have not been skipped */
      for (sp = transfer_job->source_node_list, tp = transfer_job->target_path_list; sp != NULL; sp = sp->next, tp = tp->next)
        {
          /* check if the process was cancelled */
          if (thunar_vfs_job_cancelled (job))
            break;

          /* copy the file for this node */
          thunar_vfs_transfer_job_node_copy (transfer_job, sp->data, tp->data, NULL, &new_files_list, &err);
        }
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* EINTR should be ignored, as it indicates user cancellation */
      if (G_LIKELY (err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_INTR))
        _thunar_vfs_job_error (job, err);

      /* we're done */
      g_error_free (err);
    }

  /* emit the "new-files" signal if we have any new files */
  if (!thunar_vfs_job_cancelled (job))
    _thunar_vfs_job_new_files (job, new_files_list);
  thunar_vfs_path_list_free (new_files_list);
}



static gboolean
thunar_vfs_transfer_job_progress (ThunarVfsFileSize chunk_size,
                                  gpointer          callback_data)
{
  ThunarVfsTransferJob *transfer_job = THUNAR_VFS_TRANSFER_JOB (callback_data);
  gdouble               percent;

  /* update the current completed size */
  transfer_job->completed_size += chunk_size;

  /* check if we can calculate the percentage */
  if (G_LIKELY (transfer_job->total_size > 0))
    {
      /* calculate the new percentage */
      percent = (transfer_job->completed_size * 100.0) / transfer_job->total_size;
      _thunar_vfs_job_percent (THUNAR_VFS_JOB (transfer_job), percent);
    }

  return !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (transfer_job));
}



static gboolean
thunar_vfs_transfer_job_copy_file (ThunarVfsTransferJob *transfer_job,
                                   ThunarVfsPath        *source_path,
                                   ThunarVfsPath        *target_path,
                                   ThunarVfsPath       **target_path_return,
                                   GError              **error)
{
  ThunarVfsJobResponse response;
  GError              *err = NULL;

  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_TRANSFER_JOB (transfer_job), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (target_path_return != NULL, FALSE);

  /* various attemps to copy the file */
  while (err == NULL && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (transfer_job)))
    {
      /* try to copy the file from source_path to the target_path */
      if (_thunar_vfs_io_ops_copy_file (source_path, target_path, &target_path, thunar_vfs_transfer_job_progress, transfer_job, &err))
        {
          /* return the real target path */
          *target_path_return = target_path;
          return TRUE;
        }

      /* check if we can recover from this error */
      if (err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_EXIST)
        {
          /* reset the error */
          g_clear_error (&err);

          /* ask the user whether to replace the target file */
          response = _thunar_vfs_job_ask_replace (THUNAR_VFS_JOB (transfer_job), source_path, target_path);

          /* check if we should retry the copy operation */
          if (response == THUNAR_VFS_JOB_RESPONSE_RETRY)
            continue;

          /* try to remove the target file if we should overwrite */
          if (response == THUNAR_VFS_JOB_RESPONSE_YES && !_thunar_vfs_io_ops_remove (target_path, THUNAR_VFS_IO_OPS_IGNORE_ENOENT, &err))
            break;

          /* check if we should skip this file */
          if (response == THUNAR_VFS_JOB_RESPONSE_NO)
            {
              /* tell the caller that we skipped this one */
              *target_path_return = NULL;
              return TRUE;
            }
        }
    }

  /* check if we were cancelled */
  if (G_LIKELY (err == NULL))
    {
      /* user cancellation is represented by EINTR */
      _thunar_vfs_set_g_error_from_errno (error, EINTR);
    }
  else
    {
      /* propagate the error */
      g_propagate_error (error, err);
    }

  return FALSE;
}



static void
thunar_vfs_transfer_job_node_copy (ThunarVfsTransferJob  *transfer_job,
                                   ThunarVfsTransferNode *transfer_node,
                                   ThunarVfsPath         *target_path,
                                   ThunarVfsPath         *target_parent_path,
                                   GList                **target_path_list_return,
                                   GError               **error)
{
  ThunarVfsPath *target_path_return;
  gchar         *display_name;
  GError        *err = NULL;
  gboolean       retry;

  _thunar_vfs_return_if_fail ((target_path == NULL && target_parent_path != NULL) || (target_path != NULL && target_parent_path == NULL));
  _thunar_vfs_return_if_fail (target_path == NULL || transfer_node->next == NULL);
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_TRANSFER_JOB (transfer_job));
  _thunar_vfs_return_if_fail (error == NULL || *error == NULL);
  _thunar_vfs_return_if_fail (transfer_node != NULL);

  /* The caller can either provide a target_path or a target_parent_path, but not both. The toplevel
   * transfer_nodes (for which next is NULL) should be called with target_path, to get proper behavior
   * wrt restoring files from the trash. Other transfer_nodes will be called with target_parent_path.
   */

  /* process all transfer nodes in the row */
  for (; err == NULL && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (transfer_job)) && transfer_node != NULL; transfer_node = transfer_node->next)
    {
      /* guess the target_path for this file item (if not provided) */
      if (G_LIKELY (target_path == NULL))
        target_path = _thunar_vfs_path_child (target_parent_path, thunar_vfs_path_get_name (transfer_node->source_path));
      else
        target_path = thunar_vfs_path_ref (target_path);

      /* update the progress information */
      display_name = _thunar_vfs_path_dup_display_name (_thunar_vfs_path_is_local (target_path) ? target_path : transfer_node->source_path);
      _thunar_vfs_job_info_message (THUNAR_VFS_JOB (transfer_job), display_name);
      g_free (display_name);

retry_copy:
      /* copy the item specified by this node (not recursive!) */
      if (thunar_vfs_transfer_job_copy_file (transfer_job, transfer_node->source_path, target_path, &target_path_return, &err))
        {
          /* target_path_return == NULL: skip this file */
          if (G_LIKELY (target_path_return != NULL))
            {
              /* check if we have children to copy */
              if (transfer_node->children != NULL)
                {
                  /* copy all children for this node */
                  thunar_vfs_transfer_job_node_copy (transfer_job, transfer_node->children, NULL, target_path_return, NULL, &err);

                  /* free the resources allocated to the children */
                  thunar_vfs_transfer_node_free (transfer_node->children);
                  transfer_node->children = NULL;
                }

              /* check if the child copy failed */
              if (G_UNLIKELY (err != NULL))
                {
                  /* outa here, freeing the target paths */
                  thunar_vfs_path_unref (target_path_return);
                  thunar_vfs_path_unref (target_path);
                  break;
                }

              /* add the real target path to the return list */
              if (G_LIKELY (target_path_list_return != NULL))
                *target_path_list_return = g_list_prepend (*target_path_list_return, target_path_return);
              else
                thunar_vfs_path_unref (target_path_return);

retry_remove:
              /* try to remove the source directory if we should move instead of just copy */
              if (transfer_job->move && !_thunar_vfs_io_ops_remove (transfer_node->source_path, THUNAR_VFS_IO_OPS_IGNORE_ENOENT, &err))
                {
                  /* we can ignore ENOTEMPTY (which is mapped to G_FILE_ERROR_FAILED) */
                  if (err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_FAILED)
                    {
                      /* no error then... */
                      g_clear_error (&err);
                    }
                  else
                    {
                      /* ask the user whether to skip/retry the removal */
                      retry = (_thunar_vfs_job_ask_skip (THUNAR_VFS_JOB (transfer_job), "%s", err->message) == THUNAR_VFS_JOB_RESPONSE_RETRY);

                      /* reset the error */
                      g_clear_error (&err);

                      /* check whether to retry */
                      if (G_UNLIKELY (retry))
                        goto retry_remove;
                    }
                }
            }
        }
      else if (err != NULL && (err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_NOSPC)) /* ENOSPC cannot be skipped! */
        {
          /* ask the user whether to skip this node and all subnodes (-> cancellation) */
          retry = (_thunar_vfs_job_ask_skip (THUNAR_VFS_JOB (transfer_job), "%s", err->message) == THUNAR_VFS_JOB_RESPONSE_RETRY);

          /* reset the error */
          g_clear_error (&err);

          /* check whether to retry */
          if (G_UNLIKELY (retry))
            goto retry_copy;
        }

      /* release the guessed target_path */
      thunar_vfs_path_unref (target_path);
      target_path = NULL;
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
    }
}






static gboolean
thunar_vfs_transfer_node_collect (ThunarVfsTransferNode *transfer_node,
                                  ThunarVfsFileSize     *total_size_return,
                                  volatile gboolean     *cancelled,
                                  GError               **error)
{
  ThunarVfsTransferNode *child_node;
  ThunarVfsFileSize      size;
  ThunarVfsFileType      type;
  GError                *err = NULL;
  GList                 *path_list;
  GList                 *lp;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (total_size_return != NULL, FALSE);
  _thunar_vfs_return_val_if_fail (transfer_node != NULL, FALSE);
  _thunar_vfs_return_val_if_fail (cancelled != NULL, FALSE);

  /* determine the file size and mode */
  if (!_thunar_vfs_io_ops_get_file_size_and_type (transfer_node->source_path, &size, &type, error))
    return FALSE;

  /* add the file size to the total size */
  *total_size_return += size;

  /* check if we have a directory here */
  if (type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
    {
      /* scan the directory and add appropriate children for this transfer node */
      path_list = _thunar_vfs_io_scandir (transfer_node->source_path, cancelled, 0, &err);
      for (lp = path_list; err == NULL && lp != NULL; lp = lp->next)
        {
          /* check if the user cancelled the process */
          if (G_UNLIKELY (*cancelled))
            {
              /* user cancellation is represented as EINTR */
              _thunar_vfs_set_g_error_from_errno (&err, EINTR);
            }
          else
            {
              /* allocate a new transfer node for the child */
              child_node = _thunar_vfs_slice_new0 (ThunarVfsTransferNode);
              child_node->source_path = lp->data;

              /* hook the child node into the child list */
              child_node->next = transfer_node->children;
              transfer_node->children = child_node;

              /* collect the child node */
              thunar_vfs_transfer_node_collect (child_node, total_size_return, cancelled, &err);
            }
        }

      /* release the remaining items on the path_list */
      g_list_foreach (lp, (GFunc) thunar_vfs_path_unref, NULL);
      g_list_free (path_list);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static void
thunar_vfs_transfer_node_free (ThunarVfsTransferNode *transfer_node)
{
  ThunarVfsTransferNode *transfer_next;

  /* free all nodes in a row */
  while (transfer_node != NULL)
    {
      /* free all children of this node */
      thunar_vfs_transfer_node_free (transfer_node->children);

      /* determine the next node */
      transfer_next = transfer_node->next;

      /* drop the source path for this node */
      thunar_vfs_path_unref (transfer_node->source_path);

      /* release the resources for this node */
      _thunar_vfs_slice_free (ThunarVfsTransferNode, transfer_node);

      /* continue with the next node */
      transfer_node = transfer_next;
    }
}



/**
 * thunar_vfs_transfer_job_new:
 * @source_path_list : a list of #ThunarVfsPath<!---->s that should be transferred.
 * @target_path_list : a list of #ThunarVfsPath<!---->s referring to the targets of the transfer.
 * @move             : whether to copy or move the files.
 * @error            : return location for errors or %NULL.
 *
 * Transfers the files from the @source_path_list to the files in the @target_path_list.
 * @source_path_list and @target_path_list must be of the same length.
 *
 * If @move is %FALSE, then all source/target path tuple, which refer to the same
 * file cause a duplicate action, which means a new unique target path will be
 * generated based on the source path.
 *
 * The caller is responsible to free the returned object using g_object_unref()
 * when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_transfer_job_new (GList   *source_path_list,
                             GList   *target_path_list,
                             gboolean move,
                             GError **error)
{
  ThunarVfsTransferNode *transfer_node;
  ThunarVfsTransferJob  *transfer_job;
  GList                 *sp, *tp;

  _thunar_vfs_return_val_if_fail (g_list_length (target_path_list) == g_list_length (source_path_list), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job object */
  transfer_job = g_object_new (THUNAR_VFS_TYPE_TRANSFER_JOB, NULL);
  transfer_job->move = move;

  /* add a transfer node for each source path, and a matching target parent path */
  for (sp = source_path_list, tp = target_path_list; sp != NULL; sp = sp->next, tp = tp->next)
    {
      /* verify that we don't transfer the root directory */
      if (G_UNLIKELY (thunar_vfs_path_is_root (sp->data) || thunar_vfs_path_is_root (tp->data)))
        {
          /* we don't support this, the file manager will prevent this anyway */
          _thunar_vfs_set_g_error_not_supported (error);
          g_object_unref (G_OBJECT (transfer_job));
          return NULL;
        }

      /* strip off all pairs with source=target when not copying */
      if (G_LIKELY (!move || !thunar_vfs_path_equal (sp->data, tp->data)))
        {
          /* allocate a node for the source and add it to the source list */
          transfer_node = _thunar_vfs_slice_new0 (ThunarVfsTransferNode);
          transfer_node->source_path = thunar_vfs_path_ref (sp->data);
          transfer_job->source_node_list = g_list_append (transfer_job->source_node_list, transfer_node);

          /* append the target path to the target list */
          transfer_job->target_path_list = thunar_vfs_path_list_append (transfer_job->target_path_list, tp->data);
        }
    }

  /* make sure that we did not mess up anything */
  _thunar_vfs_assert (g_list_length (transfer_job->source_node_list) == g_list_length (transfer_job->target_path_list));

  /* the job is ready */
  return THUNAR_VFS_JOB (transfer_job);
}



#define __THUNAR_VFS_TRANSFER_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
