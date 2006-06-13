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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-scandir.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>
#include <thunar-vfs/thunar-vfs-xfer.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_lstat(), g_rename(), g_rmdir() and g_unlink() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_lstat(path, statb) (lstat ((path), (statb)))
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_rmdir(path) (rmdir ((path)))
#define g_unlink(path) (unlink ((path)))
#endif



typedef struct _ThunarVfsTransferPair ThunarVfsTransferPair;



static void                   thunar_vfs_transfer_job_class_init    (ThunarVfsTransferJobClass *klass);
static void                   thunar_vfs_transfer_job_init          (ThunarVfsTransferJob      *transfer_job);
static void                   thunar_vfs_transfer_job_finalize      (GObject                   *object);
static void                   thunar_vfs_transfer_job_execute       (ThunarVfsJob              *job);
static void                   thunar_vfs_transfer_job_update        (ThunarVfsTransferJob      *transfer_job);
static gboolean               thunar_vfs_transfer_job_progress      (ThunarVfsFileSize          current_total_size,
                                                                     ThunarVfsFileSize          current_completed_size,
                                                                     gpointer                   user_data);
static GList                 *thunar_vfs_transfer_job_collect_pairs (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsTransferPair     *pair,
                                                                     GError                   **error);
static void                   thunar_vfs_transfer_job_copy_pair     (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsTransferPair     *pair);
static gboolean               thunar_vfs_transfer_job_overwrite     (ThunarVfsTransferJob      *transfer_job,
                                                                     const GError              *error);
static gboolean               thunar_vfs_transfer_job_skip          (ThunarVfsTransferJob      *transfer_job,
                                                                     const GError              *error);
static void                   thunar_vfs_transfer_job_skip_list_add (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsTransferPair     *pair);
static gboolean               thunar_vfs_transfer_job_skip_list_has (ThunarVfsTransferJob      *transfer_job,
                                                                     const ThunarVfsPath       *source_path);
static ThunarVfsTransferPair *thunar_vfs_transfer_job_alloc_pair    (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsPath             *source_path,
                                                                     ThunarVfsPath             *target_path,
                                                                     gboolean                   toplevel,
                                                                     GError                   **error);
static inline void            thunar_vfs_transfer_job_free_pair     (ThunarVfsTransferJob      *transfer_job,
                                                                     ThunarVfsTransferPair     *pair);



struct _ThunarVfsTransferJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsTransferJob
{
  ThunarVfsInteractiveJob __parent__;

  /* the VFS monitor */
  ThunarVfsMonitor *monitor;

  /* the list of pairs */
  GList            *pairs;

  /* the pair chunks (alloc-only) */
  GMemChunk        *pair_chunk;

  /* whether to move files instead of copying them */
  gboolean          move;

  /* the amount of completeness */
  ThunarVfsFileSize total_size;
  ThunarVfsFileSize completed_size;

  /* the last time we update the status info */
  GTimeVal          last_update_time;

  /* current file status */
  gboolean          current_path_changed;
  ThunarVfsPath    *current_path;
  ThunarVfsFileSize current_total_size;
  ThunarVfsFileSize current_completed_size;

  /* list of directories that should be deleted when the job finishes */
  GList            *directories_to_delete;

  /* the list of toplevel ThunarVfsPath's that were created by this job */
  GList            *new_files;

  /* the list of directories to skip */
  GList            *skip_list;
};

struct _ThunarVfsTransferPair
{
  ThunarVfsPath    *source_path;
  ThunarVfsPath    *target_path;
  ThunarVfsFileSize source_size;
  guint             source_mode : 31;
  gboolean          toplevel : 1;
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
                                                 thunar_vfs_transfer_job_init,
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
thunar_vfs_transfer_job_init (ThunarVfsTransferJob *transfer_job)
{
  /* connect to the VFS monitor */
  transfer_job->monitor = thunar_vfs_monitor_get_default ();

  /* allocate the pair chunk (alloc-only) */
  transfer_job->pair_chunk = g_mem_chunk_new ("ThunarVfsTransferPair chunk",
                                              sizeof (ThunarVfsTransferPair),
                                              sizeof (ThunarVfsTransferPair) * 256,
                                              G_ALLOC_ONLY);
}



static void
thunar_vfs_transfer_job_finalize (GObject *object)
{
  ThunarVfsTransferJob *transfer_job = THUNAR_VFS_TRANSFER_JOB (object);
  GList                *lp;

  /* release the pairs */
  for (lp = transfer_job->pairs; lp != NULL; lp = lp->next)
    thunar_vfs_transfer_job_free_pair (transfer_job, lp->data);
  g_list_free (transfer_job->pairs);

  /* drop the list of directories to delete */
  thunar_vfs_path_list_free (transfer_job->directories_to_delete);

  /* release the new files list */
  thunar_vfs_path_list_free (transfer_job->new_files);

  /* release the skip list */
  thunar_vfs_path_list_free (transfer_job->skip_list);

  /* release the current path */
  if (G_LIKELY (transfer_job->current_path != NULL))
    thunar_vfs_path_unref (transfer_job->current_path);

  /* destroy the pair chunk */
  g_mem_chunk_destroy (transfer_job->pair_chunk);

  /* disconnect from the VFS monitor */
  g_object_unref (G_OBJECT (transfer_job->monitor));

  /* call the parents finalize method */
  (*G_OBJECT_CLASS (thunar_vfs_transfer_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_transfer_job_execute (ThunarVfsJob *job)
{
  ThunarVfsTransferPair *pair;
  ThunarVfsTransferJob  *transfer_job = THUNAR_VFS_TRANSFER_JOB (job);
  gboolean               result;
  GError                *error;
  gchar                 *source_absolute_path;
  gchar                 *target_absolute_path;
  gchar                 *display_name;
  GList                 *pairs;
  GList                 *tmp;
  GList                 *lp;

  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Collecting files..."));

  /* save the current list of pairs */
  pairs = transfer_job->pairs;
  transfer_job->pairs = NULL;

  /* collect pairs recursively */
  for (lp = pairs; lp != NULL; lp = lp->next)
    {
      /* determine the current pair */
      pair = (ThunarVfsTransferPair *) lp->data;

      /* check if we want to move files, and try rename() first */
      if (G_UNLIKELY (transfer_job->move))
        {
          /* determine the absolute source and target paths */
          source_absolute_path = thunar_vfs_path_dup_string (pair->source_path);
          target_absolute_path = thunar_vfs_path_dup_string (pair->target_path);

          /* perform the rename if the target does not already exist */
          result = (!g_file_test (target_absolute_path, G_FILE_TEST_EXISTS) && (g_rename (source_absolute_path, target_absolute_path) == 0));

          /* release the absolute source and target paths */
          g_free (source_absolute_path);
          g_free (target_absolute_path);

          /* fallback to copy logic if the simple rename didn't work */
          if (G_LIKELY (result))
            {
              /* schedule a "created" event for the target file */
              thunar_vfs_monitor_feed (transfer_job->monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, pair->target_path);

              /* schedule a "deleted" event for the source file */
              thunar_vfs_monitor_feed (transfer_job->monitor, THUNAR_VFS_MONITOR_EVENT_DELETED, pair->source_path);
            
              /* add the target file to the new files list */
              transfer_job->new_files = thunar_vfs_path_list_prepend (transfer_job->new_files, pair->target_path);

              /* rename worked, no need to copy this pair */
              thunar_vfs_transfer_job_free_pair (transfer_job, pair);
              continue;
            }
        }

      /* prepare the pair for copying */
      tmp = thunar_vfs_transfer_job_collect_pairs (transfer_job, pair, NULL);
      transfer_job->pairs = g_list_concat (transfer_job->pairs, tmp);
      transfer_job->pairs = g_list_append (transfer_job->pairs, pair);
    }

  /* release the temporary list */
  g_list_free (pairs);

  /* reverse the pair list, so we have the directories first */
  transfer_job->pairs = g_list_reverse (transfer_job->pairs);

  /* determine the total number of bytes to process */
  for (lp = transfer_job->pairs; lp != NULL; lp = lp->next)
    {
      pair = (ThunarVfsTransferPair *) lp->data;
      transfer_job->total_size += pair->source_size;
    }

  /* process the pairs */
  while (transfer_job->pairs != NULL)
    {
      /* check if the job was cancelled */
      if (thunar_vfs_job_cancelled (job))
        break;

      /* pick the next pair from the list */
      pair = transfer_job->pairs->data;
      lp = transfer_job->pairs->next;
      g_list_free_1 (transfer_job->pairs);
      transfer_job->pairs = lp;

      /* check if we should skip this pair */
      if (thunar_vfs_transfer_job_skip_list_has (transfer_job, pair->source_path))
        {
          /* add the size that was calculated for the skipped pair */
          transfer_job->completed_size += pair->source_size;
        }
      else
        {
          /* copy the pair */
          thunar_vfs_transfer_job_copy_pair (transfer_job, pair);
        }

      /* drop the pair, to reduce the overhead for finalize() */
      thunar_vfs_transfer_job_free_pair (transfer_job, pair);
    }

  /* delete the scheduled directories */
  if (transfer_job->directories_to_delete && transfer_job->move)
    {
      /* display info message */
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Deleting directories..."));

      /* delete the directories */
      for (lp = transfer_job->directories_to_delete; lp != NULL; lp = lp->next)
        {
          /* check if the job was cancelled */
          if (thunar_vfs_job_cancelled (job))
            break;

          /* check if we should skip this directory */
          if (thunar_vfs_transfer_job_skip_list_has (transfer_job, lp->data))
            continue;

          /* try to delete the source directory */
          source_absolute_path = thunar_vfs_path_dup_string (lp->data);
          if (g_rmdir (source_absolute_path) < 0 && errno != ENOENT)
            {
              /* ask the user whether we should skip this directory */
              display_name = g_filename_display_name (source_absolute_path);
              error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                   _("Failed to remove directory \"%s\": %s"),
                                   display_name, g_strerror (errno));
              thunar_vfs_transfer_job_skip (transfer_job, error);
              g_clear_error (&error);
              g_free (display_name);
            }
          else
            {
              /* schedule a "deleted" event for the source directory */
              thunar_vfs_monitor_feed (transfer_job->monitor, THUNAR_VFS_MONITOR_EVENT_DELETED, lp->data);
            }
          g_free (source_absolute_path);
        }
    }

  /* emit the "new-files" signal if we have any new files */
  if (G_LIKELY (transfer_job->new_files != NULL))
    thunar_vfs_interactive_job_new_files (THUNAR_VFS_INTERACTIVE_JOB (transfer_job), transfer_job->new_files);
}



static void
thunar_vfs_transfer_job_update (ThunarVfsTransferJob *transfer_job)
{
  ThunarVfsFileSize completed_size;
  gdouble           percentage;
  gchar            *display_name;

  /* check if we need to display a new file name */
  if (G_LIKELY (transfer_job->current_path_changed))
    {
      /* update the info message with the file name */
      display_name = g_filename_display_name (thunar_vfs_path_get_name (transfer_job->current_path));
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (transfer_job), display_name);
      g_free (display_name);

      /* remember that we don't need to update the info message again now */
      transfer_job->current_path_changed = FALSE;
    }

  /* determine total/completed sizes */
  completed_size = transfer_job->completed_size + transfer_job->current_completed_size;

  /* update the percentage */
  percentage = (completed_size * 100.0) / MAX (transfer_job->total_size, 1);
  percentage = CLAMP (percentage, 0.0, 100.0);
  thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (transfer_job), percentage);

  /* update the "last update time" */
  g_get_current_time (&transfer_job->last_update_time);
}



static inline gboolean
should_update (const GTimeVal now,
               const GTimeVal last)
{
  /* we want to update every 100ms */
  guint64 d = ((guint64) now.tv_sec - last.tv_sec) * G_USEC_PER_SEC
            + ((guint64) now.tv_usec - last.tv_usec);
  return (d > 100 * 1000);
}



static gboolean
thunar_vfs_transfer_job_progress (ThunarVfsFileSize current_total_size,
                                  ThunarVfsFileSize current_completed_size,
                                  gpointer          user_data)
{
  ThunarVfsTransferJob *transfer_job = THUNAR_VFS_TRANSFER_JOB (user_data);
  GTimeVal              now;

  /* update the current total/completed size */
  transfer_job->current_completed_size = current_completed_size;
  transfer_job->current_total_size = current_total_size;

  /* determine the current time */
  g_get_current_time (&now);

  /* check if we should update the user visible info */
  if (should_update (now, transfer_job->last_update_time))
    thunar_vfs_transfer_job_update (transfer_job);

  return !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (transfer_job));
}



static inline ThunarVfsPath*
make_target_path (ThunarVfsPath *target_base_path,
                  ThunarVfsPath *source_base_path,
                  ThunarVfsPath *source_path)
{
  typedef struct _Component
  {
    struct _Component *next;
    const gchar       *name;
  } Component;

  ThunarVfsPath *target_path = thunar_vfs_path_ref (target_base_path);
  ThunarVfsPath *path;
  Component     *components = NULL;
  Component     *component;

  /* determine the components in reverse order */
  for (; !thunar_vfs_path_equal (source_base_path, source_path); source_path = thunar_vfs_path_get_parent (source_path))
    {
      g_assert (source_path != NULL);
      component = g_newa (Component, 1);
      component->next = components;
      component->name = thunar_vfs_path_get_name (source_path);
      components = component;
    }

  /* verify state */
  g_assert (thunar_vfs_path_equal (source_base_path, source_path));

  /* generate the target path */
  for (component = components; component != NULL; component = component->next)
    {
      /* allocate relative path for the component */
      path = thunar_vfs_path_relative (target_path, component->name);
      thunar_vfs_path_unref (target_path);
      target_path = path;
    }

  return target_path;
}



static GList*
thunar_vfs_transfer_job_collect_pairs (ThunarVfsTransferJob  *transfer_job,
                                       ThunarVfsTransferPair *pair,
                                       GError               **error)
{
  ThunarVfsTransferPair *child;
  ThunarVfsPath         *target_path;
  GHashTable            *source_to_target;
  GError                *serror = NULL;
  GList                 *paths;
  GList                 *pairs = NULL;
  GList                 *lp;

  /* check if the pair refers to a directory */
  if (G_UNLIKELY (!S_ISDIR (pair->source_mode)))
    return NULL;

  /* scan the pair directory */
  paths = thunar_vfs_scandir (pair->source_path, &THUNAR_VFS_JOB (transfer_job)->cancelled, THUNAR_VFS_SCANDIR_RECURSIVE, NULL, &serror);
  if (G_UNLIKELY (serror != NULL))
    {
      g_propagate_error (error, serror);
      g_assert (paths == NULL);
    }
  else
    {
      /* simple optimization to save some memory (source->target directory mapping) */
      source_to_target = g_hash_table_new (thunar_vfs_path_hash, thunar_vfs_path_equal);
      g_hash_table_insert (source_to_target, pair->source_path, pair->target_path);

      /* translate the paths to pairs */
      for (lp = g_list_last (paths); lp != NULL; lp = lp->prev)
        {
          /* determine the target path */
          target_path = g_hash_table_lookup (source_to_target, thunar_vfs_path_get_parent (lp->data));
          if (G_LIKELY (target_path != NULL))
            target_path = thunar_vfs_path_relative (target_path, thunar_vfs_path_get_name (lp->data));
          else
            target_path = make_target_path (pair->target_path, pair->source_path, lp->data);

          /* try to allocate a pair for the child */
          child = thunar_vfs_transfer_job_alloc_pair (transfer_job, lp->data, target_path, FALSE, NULL);

          /* release the reference on the target path (the child holds the reference now) */
          thunar_vfs_path_unref (target_path);

          /* release the reference on the source path (the child holds the reference now) */
          thunar_vfs_path_unref (lp->data);

          /* prepend the child to the pair list */
          if (G_LIKELY (child != NULL))
            {
              /* cache directory source->target mappings */
              if (G_UNLIKELY (S_ISDIR (child->source_mode)))
                g_hash_table_insert (source_to_target, child->source_path, child->target_path);

              /* add to the list of pairs */
              pairs = g_list_prepend (pairs, child);
            }
        }

      /* drop the source->target directory mapping */
      g_hash_table_destroy (source_to_target);

      /* drop the path list */
      g_list_free (paths);
    }

  return pairs;
}



static inline void
maybe_replace_pair_target (ThunarVfsTransferPair *pair,
                           ThunarVfsPath         *previous_target_path,
                           ThunarVfsPath         *new_target_path)
{
  ThunarVfsPath *path;

  /* check if the target path is below the previous_target_path */
  for (path = thunar_vfs_path_get_parent (pair->target_path); path != NULL; path = thunar_vfs_path_get_parent (path))
    if (thunar_vfs_path_equal (previous_target_path, path))
      break;

  if (G_LIKELY (path != NULL))
    {
      /* replace the target path */
      path = make_target_path (new_target_path, path, pair->target_path);
      thunar_vfs_path_unref (pair->target_path);
      pair->target_path = path;
    }
}



static void
thunar_vfs_transfer_job_copy_pair (ThunarVfsTransferJob  *transfer_job,
                                   ThunarVfsTransferPair *pair)
{
  ThunarVfsPath *target_path;
  gboolean       skip;
  GError        *error = NULL;
  GList         *lp;
  gchar         *absolute_path;
  gchar         *display_name;

  /* update the current file path */
  if (G_LIKELY (transfer_job->current_path != NULL))
    thunar_vfs_path_unref (transfer_job->current_path);
  transfer_job->current_path = thunar_vfs_path_ref (pair->source_path);
  transfer_job->current_path_changed = TRUE;

  /* perform the xfer */
  do
    {
      /* start with 0 completed/total current size */
      transfer_job->current_completed_size = 0;
      transfer_job->current_total_size = 0;

      /* try to xfer the file */
      if (thunar_vfs_xfer_copy (pair->source_path, pair->target_path, &target_path, thunar_vfs_transfer_job_progress, transfer_job, &error))
        {
          /* if this was a toplevel pair, then add the target path to the list of new files */
          if (G_UNLIKELY (pair->toplevel))
            transfer_job->new_files = thunar_vfs_path_list_prepend (transfer_job->new_files, target_path);

          /* perform cleanup for moves */
          if (G_UNLIKELY (transfer_job->move))
            {
              /* if we have a directory, schedule it for later deletion, else try to delete it directly */
              if (G_UNLIKELY (((pair->source_mode & S_IFMT) >> 12) == THUNAR_VFS_FILE_TYPE_DIRECTORY))
                {
                  /* schedule the directory for later deletion */
                  transfer_job->directories_to_delete = thunar_vfs_path_list_prepend (transfer_job->directories_to_delete, pair->source_path);
                }
              else
                {
                  absolute_path = thunar_vfs_path_dup_string (pair->source_path);
                  if (g_unlink (absolute_path) < 0 && errno != ENOENT)
                    {
                      /* ask the user whether we should skip the file (used for cancellation only) */
                      display_name = g_filename_display_name (absolute_path);
                      error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                           _("Failed to remove \"%s\": %s"), display_name,
                                           g_strerror (errno));
                      thunar_vfs_transfer_job_skip (transfer_job, error);
                      g_clear_error (&error);
                      g_free (display_name);
                    }
                  else
                    {
                      /* schedule a deleted event for the source path */
                      thunar_vfs_monitor_feed (transfer_job->monitor, THUNAR_VFS_MONITOR_EVENT_DELETED, pair->source_path);
                    }
                  g_free (absolute_path);
                }
            }
          else if (!thunar_vfs_path_equal (pair->target_path, target_path))
            {
              /* replace the target path on the following elements */
              for (lp = transfer_job->pairs; lp != NULL; lp = lp->next)
                maybe_replace_pair_target (lp->data, pair->target_path, target_path);

              /* replace the previous target path with the real target path */
              thunar_vfs_path_unref (pair->target_path);
              pair->target_path = thunar_vfs_path_ref (target_path);
            }

          /* release the real target path */
          thunar_vfs_path_unref (target_path);
          break;
        }

      /* G_FILE_ERROR_INTR is returned when the job is cancelled during the copy operation */
      if (G_UNLIKELY (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_INTR))
        {
          g_error_free (error);
          break;
        }

      /* check the error */
      if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_EXIST)
        {
          /* ask the user whether we should remove the target first */
          skip = !thunar_vfs_transfer_job_overwrite (transfer_job, error);
          g_clear_error (&error);

          /* try to remove the target */
          if (G_LIKELY (!skip))
            {
              absolute_path = thunar_vfs_path_dup_string (pair->target_path);
              if (g_unlink (absolute_path) < 0 && errno != ENOENT)
                {
                  /* ask the user whether we should skip the file */
                  display_name = g_filename_display_name (absolute_path);
                  error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                       _("Failed to remove \"%s\": %s"), display_name,
                                       g_strerror (errno));
                  skip = thunar_vfs_transfer_job_skip (transfer_job, error);
                  g_free (display_name);
                }
              g_free (absolute_path);
            }
        }
      else
        {
          /* ask the user whether to skip this pair */
          skip = thunar_vfs_transfer_job_skip (transfer_job, error);
        }

      /* clear the error */
      g_clear_error (&error);

      /* check if we should skip this pair */
      if (G_LIKELY (skip))
        {
          thunar_vfs_transfer_job_skip_list_add (transfer_job, pair);
          break;
        }
    }
  while (!thunar_vfs_job_cancelled (THUNAR_VFS_JOB (transfer_job)));

  /* add the current file's size to the total completion state */
  transfer_job->completed_size += transfer_job->current_completed_size;

  /* reset the current completed/total size */
  transfer_job->current_completed_size = 0;
  transfer_job->current_total_size = 0;
}



static gboolean
thunar_vfs_transfer_job_overwrite (ThunarVfsTransferJob *transfer_job,
                                   const GError         *error)
{
  gboolean overwrite;
  gchar   *message;

  /* be sure to update the status info */
  thunar_vfs_transfer_job_update (transfer_job);

  /* ask the user whether to overwrite */
  message = g_strdup_printf (_("%s.\n\nDo you want to overwrite it?"), error->message);
  overwrite = thunar_vfs_interactive_job_overwrite (THUNAR_VFS_INTERACTIVE_JOB (transfer_job), message);
  g_free (message);

  return overwrite;
}



static gboolean
thunar_vfs_transfer_job_skip (ThunarVfsTransferJob *transfer_job,
                              const GError         *error)
{
  gboolean skip;
  gchar   *message;

  /* be sure to update the status info */
  thunar_vfs_transfer_job_update (transfer_job);

  /* ask the user whether to skip */
  message = g_strdup_printf (_("%s.\n\nDo you want to skip it?"), error->message);
  skip = thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (transfer_job), message);
  g_free (message);

  return skip;
}



static void
thunar_vfs_transfer_job_skip_list_add (ThunarVfsTransferJob  *transfer_job,
                                       ThunarVfsTransferPair *pair)
{
  /* add only directories to the skip_list */
  if (((pair->source_mode & S_IFMT) >> 12) == THUNAR_VFS_FILE_TYPE_DIRECTORY)
    transfer_job->skip_list = thunar_vfs_path_list_prepend (transfer_job->skip_list, pair->source_path);
}



static gboolean
thunar_vfs_transfer_job_skip_list_has (ThunarVfsTransferJob *transfer_job,
                                       const ThunarVfsPath  *source_path)
{
  const ThunarVfsPath *path;
  GList               *lp;

  /* check if the source_path or any of its ancestors is on the skip list */
  for (lp = transfer_job->skip_list; lp != NULL; lp = lp->next)
    for (path = source_path; path != NULL; path = thunar_vfs_path_get_parent (path))
      if (thunar_vfs_path_equal (path, lp->data))
        return TRUE;

  return FALSE;
}



static ThunarVfsTransferPair*
thunar_vfs_transfer_job_alloc_pair (ThunarVfsTransferJob *transfer_job,
                                    ThunarVfsPath        *source_path,
                                    ThunarVfsPath        *target_path,
                                    gboolean              toplevel,
                                    GError              **error)
{
  ThunarVfsTransferPair *pair;
  struct stat            source_statb;
  gchar                  source_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];

  /* determine the absolute path to the source file */
  if (thunar_vfs_path_to_string (source_path, source_absolute_path, sizeof (source_absolute_path), error) < 0)
    return NULL;

  /* determine the file info for the source file */
  if (g_lstat (source_absolute_path, &source_statb) < 0)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      return NULL;
    }

  /* allocate a new transfer pair */
  pair = g_chunk_new (ThunarVfsTransferPair, transfer_job->pair_chunk);
  pair->source_path = thunar_vfs_path_ref (source_path);
  pair->target_path = thunar_vfs_path_ref (target_path);
  pair->source_size = source_statb.st_size;
  pair->source_mode = source_statb.st_mode;
  pair->toplevel = toplevel;

  return pair;
}



static inline void
thunar_vfs_transfer_job_free_pair (ThunarVfsTransferJob  *transfer_job,
                                   ThunarVfsTransferPair *pair)
{
  thunar_vfs_path_unref (pair->source_path);
  thunar_vfs_path_unref (pair->target_path);
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
  ThunarVfsTransferPair *pair;
  ThunarVfsTransferJob  *transfer_job;
  GList                 *sp, *tp;

  g_return_val_if_fail (g_list_length (target_path_list) == g_list_length (source_path_list), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job object */
  transfer_job = g_object_new (THUNAR_VFS_TYPE_TRANSFER_JOB, NULL);
  transfer_job->move = move;

  /* just create a pair for every (source, target) tuple */
  for (sp = source_path_list, tp = target_path_list; sp != NULL; sp = sp->next, tp = tp->next)
    {
      /* verify that we don't transfer the root directory */
      if (G_UNLIKELY (thunar_vfs_path_is_root (sp->data) || thunar_vfs_path_is_root (tp->data)))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Cannot transfer the root directory"));
          goto failure;
        }

      /* strip off all pairs with source=target when not copying */
      if (G_LIKELY (!move || !thunar_vfs_path_equal (sp->data, tp->data)))
        {
          /* allocate a pair */
          pair = thunar_vfs_transfer_job_alloc_pair (transfer_job, sp->data, tp->data, TRUE, error);
          if (G_UNLIKELY (pair == NULL))
            goto failure;
          transfer_job->pairs = g_list_prepend (transfer_job->pairs, pair);
        }
    }

  /* we did it */
  return THUNAR_VFS_JOB (transfer_job);

  /* some of the sources failed */
failure:
  g_object_unref (G_OBJECT (transfer_job));
  return NULL;
}



#define __THUNAR_VFS_TRANSFER_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
