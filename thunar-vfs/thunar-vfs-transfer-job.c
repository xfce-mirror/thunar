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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
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

#include <thunar-vfs/thunar-vfs-sysdep.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_chmod(path, mode) (chmod ((path), (mode)))
#define g_lstat(path, buffer) (lstat ((path), (buffer)))
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#define g_remove(path) (remove ((path)))
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#endif



typedef struct _ThunarVfsTransferBase ThunarVfsTransferBase;
typedef struct _ThunarVfsTransferItem ThunarVfsTransferItem;



static void     thunar_vfs_transfer_job_register_type   (GType                  *type);
static void     thunar_vfs_transfer_job_class_init      (ThunarVfsJobClass      *klass);
static void     thunar_vfs_transfer_job_init            (ThunarVfsTransferJob   *transfer_job);
static void     thunar_vfs_transfer_job_execute         (ThunarVfsJob           *job);
static void     thunar_vfs_transfer_job_finalize        (ThunarVfsJob           *job);
static gboolean thunar_vfs_transfer_job_skip            (ThunarVfsTransferJob   *job,
                                                         const gchar            *format,
                                                         const gchar            *filename);
static void     thunar_vfs_transfer_job_percent         (ThunarVfsTransferJob   *job);
static void     thunar_vfs_transfer_job_insert_base     (ThunarVfsTransferJob   *job,
                                                         const gchar            *source_path,
                                                         const gchar            *target_path);
static void     thunar_vfs_transfer_base_collect        (ThunarVfsTransferBase  *base);
static void     thunar_vfs_transfer_base_copy           (ThunarVfsTransferBase  *base);
static void     thunar_vfs_transfer_item_collect        (ThunarVfsTransferItem  *item);
static void     thunar_vfs_transfer_item_copy           (ThunarVfsTransferItem  *item);
static void     thunar_vfs_transfer_item_copy_directory (ThunarVfsTransferItem  *item,
                                                         const gchar            *source_path,
                                                         const gchar            *target_path);
static gboolean thunar_vfs_transfer_item_copy_legacy    (ThunarVfsTransferJob   *job,
                                                         gint                    source_fd,
                                                         gint                    target_fd);
static void     thunar_vfs_transfer_item_copy_regular   (ThunarVfsTransferItem  *item,
                                                         const gchar            *source_path,
                                                         const gchar            *target_path);
static void     thunar_vfs_transfer_item_copy_symlink   (ThunarVfsTransferItem  *item,
                                                         const gchar            *source_path,
                                                         const gchar            *target_path);



struct _ThunarVfsTransferJob
{
  ThunarVfsInteractiveJob __parent__;

  /* the list of item bases */
  ThunarVfsTransferBase  *bases;

  /* the various helper chunks */
  GMemChunk              *base_chunk;
  GMemChunk              *item_chunk;
  GStringChunk           *string_chunk;

  /* whether to move files instead of copying them */
  gboolean                move;

  /* the amount of completeness */
  ThunarVfsFileSize       total_bytes;
  ThunarVfsFileSize       completed_bytes;

  /* dirent item buffer, used to reduce stack overhead */
  struct dirent           dirent_buffer;
};

struct _ThunarVfsTransferBase
{
  ThunarVfsTransferJob  *job;
  ThunarVfsTransferBase *next;
  ThunarVfsTransferItem *items;
  gchar                 *source_path;
  gchar                 *target_path;
};

struct _ThunarVfsTransferItem
{
  ThunarVfsTransferBase *base;
  ThunarVfsTransferItem *children;
  ThunarVfsTransferItem *next;
  ThunarVfsFileMode      mode;
  ThunarVfsFileSize      size;
  gboolean               skipped;
  gchar                 *source_path;
  gchar                 *target_path;
};



static ThunarVfsJobClass *thunar_vfs_transfer_job_parent_class;



GType
thunar_vfs_transfer_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_transfer_job_register_type, &type);

  return type;
}



static void
thunar_vfs_transfer_job_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsInteractiveJobClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_transfer_job_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsTransferJob),
    0,
    (GInstanceInitFunc) thunar_vfs_transfer_job_init,
    NULL,
  };

  *type = g_type_register_static (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                  "ThunarVfsTransferJob", &info, 0);
}



static void
thunar_vfs_transfer_job_class_init (ThunarVfsJobClass *klass)
{
  thunar_vfs_transfer_job_parent_class = g_type_class_peek_parent (klass);

  klass->execute = thunar_vfs_transfer_job_execute;
  klass->finalize = thunar_vfs_transfer_job_finalize;
}



static void
thunar_vfs_transfer_job_init (ThunarVfsTransferJob *transfer_job)
{
  /* allocate the various helper chunks */
  transfer_job->base_chunk = g_mem_chunk_new ("ThunarVfsTransferBase chunk",
                                              sizeof (ThunarVfsTransferBase),
                                              sizeof (ThunarVfsTransferBase) * 128,
                                              G_ALLOC_ONLY);
  transfer_job->item_chunk = g_mem_chunk_new ("ThunarVfsTransferItem chunk",
                                              sizeof (ThunarVfsTransferItem),
                                              sizeof (ThunarVfsTransferItem) * 256,
                                              G_ALLOC_ONLY);
  transfer_job->string_chunk = g_string_chunk_new (4096);
}



static void
thunar_vfs_transfer_job_execute (ThunarVfsJob *job)
{
  ThunarVfsTransferBase *base;
  ThunarVfsTransferJob  *transfer_job = THUNAR_VFS_TRANSFER_JOB (job);

  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Collecting files..."));

  /* collect all bases */
  for (base = transfer_job->bases; !thunar_vfs_job_cancelled (job) && base != NULL; base = base->next)
    thunar_vfs_transfer_base_collect (base);

  /* copy/move all bases */
  for (base = transfer_job->bases; !thunar_vfs_job_cancelled (job) && base != NULL; base = base->next)
    thunar_vfs_transfer_base_copy (base);
}



static void
thunar_vfs_transfer_job_finalize (ThunarVfsJob *job)
{
  ThunarVfsTransferJob *transfer_job = THUNAR_VFS_TRANSFER_JOB (job);

  /* destroy the various helper chunks */
  g_mem_chunk_destroy (transfer_job->base_chunk);
  g_mem_chunk_destroy (transfer_job->item_chunk);
  g_string_chunk_free (transfer_job->string_chunk);

  /* call the parents finalize method */
  if (THUNAR_VFS_JOB_CLASS (thunar_vfs_transfer_job_parent_class)->finalize != NULL)
    (*THUNAR_VFS_JOB_CLASS (thunar_vfs_transfer_job_parent_class)->finalize) (job);
}



static gboolean
thunar_vfs_transfer_job_skip (ThunarVfsTransferJob *job,
                              const gchar          *format,
                              const gchar          *filename)
{
  gchar *display;
  gchar  message[4096];
  gint   length;

  /* build the basic message from the message */
  display = g_filename_display_name (filename);
  length = g_snprintf (message, sizeof (message), format, display);
  g_free (display);

  /* append our question */
  g_strlcpy (message + length, _("\n\nDo you want to skip it?"), sizeof (message) - length);

  /* ask the user */
  return thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (job), message);
}



static void
thunar_vfs_transfer_job_percent (ThunarVfsTransferJob *job)
{
  gdouble percentage;

  percentage = (job->completed_bytes * 100.0) / MAX (job->total_bytes, 1);
  percentage = CLAMP (percentage, 0.0, 100.0);
  thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (job), percentage);
}



static void
thunar_vfs_transfer_job_insert_base (ThunarVfsTransferJob *transfer_job,
                                     const gchar          *source_path,
                                     const gchar          *target_path)
{
  ThunarVfsTransferBase *base;
  ThunarVfsTransferItem *item;
  gchar                 *dirname;
  gchar                 *basename;

  /* make sure that source and target differ */
  if (G_UNLIKELY (strcmp (source_path, target_path) == 0))
    return;

  /* allocate the base */
  base = g_chunk_new0 (ThunarVfsTransferBase, transfer_job->base_chunk);
  base->job = transfer_job;

  /* setup the source path */
  dirname = g_path_get_dirname (source_path);
  base->source_path = g_string_chunk_insert (transfer_job->string_chunk, dirname);
  g_free (dirname);

  /* setup the target path */
  base->target_path = g_string_chunk_insert (transfer_job->string_chunk, target_path);

  /* hook up the new base */
  base->next = transfer_job->bases;
  transfer_job->bases = base;

  /* allocate the initial item (toplevel) */
  item = g_chunk_new0 (ThunarVfsTransferItem, transfer_job->item_chunk);
  item->base = base;

  /* setup the source/target path */
  basename = g_path_get_basename (source_path);
  item->source_path = g_string_chunk_insert (transfer_job->string_chunk, basename);
  item->target_path = item->source_path;
  g_free (basename);
  
  /* hook up the new item */
  base->items = item;
}



static void
thunar_vfs_transfer_base_collect (ThunarVfsTransferBase *base)
{
  ThunarVfsTransferItem *item;
  ThunarVfsTransferJob  *job = base->job;

  for (item = base->items; !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)) && item != NULL; item = item->next)
    thunar_vfs_transfer_item_collect (item);
}



static void
thunar_vfs_transfer_base_copy (ThunarVfsTransferBase *base)
{
  ThunarVfsTransferItem *item;
  ThunarVfsTransferJob  *job = base->job;

  for (item = base->items; !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)) && item != NULL; item = item->next)
    thunar_vfs_transfer_item_copy (item);
}



static void
thunar_vfs_transfer_item_collect (ThunarVfsTransferItem *item)
{
  ThunarVfsTransferBase *base = item->base;
  ThunarVfsTransferItem *child_item;
  ThunarVfsTransferJob  *job = base->job;
  struct dirent         *d;
  struct stat            sb;
  gchar                 *path;
  gint                   result;
  DIR                   *dirp;

again:
  /* try to stat() the current items path */
  path = g_build_filename (base->source_path, item->source_path, NULL);
  result = g_lstat (path, &sb);
  g_free (path);

  /* check if the stat()ing works */
  if (G_UNLIKELY (result < 0))
    {
      /* if the item does not exist, we'll simply skip it
       * (can only be caused by a race condition, which is
       * unlikely to happen).
       */
      if (G_UNLIKELY (errno == ENOENT || errno == ENOTDIR))
        {
          item->skipped = TRUE;
          return;
        }

      /* ask the user what to do */
      item->skipped = thunar_vfs_transfer_job_skip (job, _("Unable to query information about\nthe file %s."), item->source_path);

      /* check whether to cancel the job (cancellation state will be TRUE then). */
      if (G_UNLIKELY (!item->skipped))
        return;
    }

  /* skip special files */
  switch ((sb.st_mode & S_IFMT) >> 12)
    {
    case THUNAR_VFS_FILE_TYPE_SYMLINK:
    case THUNAR_VFS_FILE_TYPE_REGULAR:
    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      break;

    default:
      return;
    }

  /* update the job's total size */
  job->total_bytes += sb.st_size;

  /* update the item info */
  item->mode = sb.st_mode;
  item->size = sb.st_size;

  /* collect directory entries */
  if (G_UNLIKELY (((sb.st_mode & S_IFMT) >> 12) == THUNAR_VFS_FILE_TYPE_DIRECTORY))
    {
      /* open the directory */
      path = g_build_filename (base->source_path, item->source_path, NULL);
      dirp = opendir (path);
      g_free (path);

      if (G_UNLIKELY (dirp == NULL))
        {
          /* somebody modified the directory in the meantime,
           * just try again the whole procedure.
           */
          if (G_UNLIKELY (errno == ENOENT || errno == ENOTDIR))
            goto again;

          /* ask the user what to do */
          item->skipped = thunar_vfs_transfer_job_skip (job, _("Unable to open directory %s."), item->source_path);

          /* check whether to cancel the job (cancellation state will be TRUE then). */
          if (G_UNLIKELY (!item->skipped))
            return;
        }
      else
        {
          /* collect all directory entries */
          while (_thunar_vfs_sysdep_readdir (dirp, &job->dirent_buffer, &d, NULL) && d != NULL)
            {
              /* check if the operation was cancelled in the meantime */
              if (G_UNLIKELY (thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job))))
                break;

              /* allocate the child item */
              child_item = g_chunk_new0 (ThunarVfsTransferItem, job->item_chunk);
              child_item->base = base;

              /* set the source path */
              path = g_build_filename (item->source_path, d->d_name, NULL);
              child_item->source_path = g_string_chunk_insert (job->string_chunk, path);
              g_free (path);

              /* set the target path */
              if (G_UNLIKELY (strcmp (item->target_path, item->source_path) != 0))
                {
                  path = g_build_filename (item->target_path, d->d_name, NULL);
                  child_item->target_path = g_string_chunk_insert (job->string_chunk, path);
                  g_free (path);
                }
              else
                {
                  /* source and target relative paths are equal */
                  child_item->target_path = child_item->source_path;
                }

              /* connect the child item to the tree */
              child_item->next = item->children;
              item->children = child_item;

              /* collect the child item */
              thunar_vfs_transfer_item_collect (child_item);
            }

          closedir (dirp);
        }
    }
}



static void
thunar_vfs_transfer_item_copy (ThunarVfsTransferItem *item)
{
  const ThunarVfsTransferBase *base = item->base;
  ThunarVfsTransferJob        *job = base->job;
  struct stat                  sb;
  gchar                       *source_path;
  gchar                       *target_path;
  gchar                       *display;
  gchar                       *message;

  /* check if the target exists (and not already a directory) */
  target_path = g_build_filename (base->target_path, item->target_path, NULL);
  if (!item->skipped && g_lstat (target_path, &sb) == 0 && !(S_ISDIR (sb.st_mode) && S_ISDIR (item->mode)))
    {
      /* ask the user whether we should overwrite the file */
      display = g_filename_display_name (item->target_path);
      message = g_strdup_printf (_("%s already exists.\n\nDo you want to overwrite it?"), display);
      item->skipped = !thunar_vfs_interactive_job_overwrite (THUNAR_VFS_INTERACTIVE_JOB (job), message);
      g_free (message);
      g_free (display);

      /* try to unlink the file */
      if (!item->skipped && g_remove (target_path) < 0 && errno != ENOENT)
        {
          /* ask the user whether to skip the item if the removal failes */
          item->skipped = thunar_vfs_transfer_job_skip (job, _("Unable to remove %s."), item->target_path);
        }
    }

  /* process if the item shouldn't be skipped and the job wasn't cancelled */
  if (G_LIKELY (!item->skipped && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job))))
    {
      /* update the info message to display the current file */
      display = g_filename_display_name (item->source_path);
      thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), display);
      g_free (display);

      /* generate the absolute source path */
      source_path = g_build_filename (base->source_path, item->source_path, NULL);

      /* try to rename the item if possible (for move) */
      if (job->move && g_rename (source_path, target_path) == 0)
        {
          job->completed_bytes += item->size;
        }
      else
        {
          /* perform the copy operation */
          switch ((item->mode & S_IFMT) >> 12)
            {
            case THUNAR_VFS_FILE_TYPE_SYMLINK:
              thunar_vfs_transfer_item_copy_symlink (item, source_path, target_path);
              break;

            case THUNAR_VFS_FILE_TYPE_REGULAR:
              thunar_vfs_transfer_item_copy_regular (item, source_path, target_path);
              break;

            case THUNAR_VFS_FILE_TYPE_DIRECTORY:
              thunar_vfs_transfer_item_copy_directory (item, source_path, target_path);
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          /* apply the original permissions */
          if (!item->skipped && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)))
            {
#ifdef HAVE_LCHMOD
              lchmod (target_path, item->mode & ~S_IFMT);
#else
              /* some systems lack the lchmod system call, so we have to
               * work-around that deficiency here by manually checking
               * whether we have a symlink or a "regular" fs entity.
               */
              if (!S_ISLNK (item->mode))
                g_chmod (target_path, item->mode & ~S_IFMT);
#endif
            }

          /* if we're moving, we'll have to unlink afterwards */
          if (job->move && !item->skipped && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job))
              && g_remove (source_path) < 0 && errno != ENOENT)
            {
              /* ask the user whether to skip the item if the removal failes (only for cancellation) */
              thunar_vfs_transfer_job_skip (job, _("Unable to remove %s."), item->source_path);
            }
        }

      g_free (source_path);
    }

  g_free (target_path);
}



static void
thunar_vfs_transfer_item_copy_directory (ThunarVfsTransferItem *item,
                                         const gchar           *source_path,
                                         const gchar           *target_path)
{
  ThunarVfsTransferItem *child_item;
  ThunarVfsTransferJob  *job = item->base->job;

  /* try to create the target directory (with full permissions first,
   * we'll change that later, since the original permissions may not
   * allow us to write to the directory afterwards).
   */
  if (!g_file_test (target_path, G_FILE_TEST_IS_DIR) && g_mkdir (target_path, 0700) < 0)
    {
      /* ask the user whether to skip the item (only for cancellation) */
      thunar_vfs_transfer_job_skip (job, _("Unable to create directory %s."), item->target_path);
    }
  else
    {
      /* update the percentage of completeness */
      job->completed_bytes += item->size;
      thunar_vfs_transfer_job_percent (job);

      /* process all children */
      for (child_item = item->children; !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)) && child_item != NULL; child_item = child_item->next)
        thunar_vfs_transfer_item_copy (child_item);
    }
}



static gboolean
thunar_vfs_transfer_item_copy_legacy (ThunarVfsTransferJob *job,
                                      gint                  source_fd,
                                      gint                  target_fd)
{
  struct stat sb;
  gboolean    succeed = FALSE;
  time_t      last_checked_time;
  time_t      current_time;
  gssize      i, j, m;
  gsize       bufsize, n;
  gchar      *buffer;

  /* determine file information from the source
   * (for optimal buffer sizes).
   */
  if (G_UNLIKELY (fstat (source_fd, &sb) < 0))
    return FALSE;

  last_checked_time = time (NULL);

  /* allocate the transfer buffer */
  bufsize = 4 * sb.st_blksize;
  buffer = g_new (gchar, bufsize);

  for (n = 0; n < sb.st_size && !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)); n += i)
    {
      i = read (source_fd, buffer, bufsize);
      if (G_UNLIKELY (i <= 0))
        goto done;

      for (m = 0; m < i; m += j)
        {
          j = write (target_fd, buffer + m, i - m);
          if (G_UNLIKELY (j <= 0))
            goto done;
        }

      /* advance the completed bytes count */
      job->completed_bytes += i;

      /* notify the user atleast once per second */
      current_time = time (NULL);
      if (current_time - last_checked_time > 0)
        {
          thunar_vfs_transfer_job_percent (job);
          last_checked_time = current_time;
        }
    }

  succeed = TRUE;

done:
  g_free (buffer);
  return succeed;
}



static void
thunar_vfs_transfer_item_copy_regular (ThunarVfsTransferItem *item,
                                       const gchar           *source_path,
                                       const gchar           *target_path)
{
  ThunarVfsTransferJob *job = item->base->job;
  gint                  source_fd;
  gint                  target_fd;

  /* try to open the source file */
  source_fd = open (source_path, O_RDONLY);
  if (G_UNLIKELY (source_fd < 0))
    {
      /* ask the user to skip (for cancellation only) */
      thunar_vfs_transfer_job_skip (job, _("Unable to open file %s."), item->source_path);
      return;
    }

  /* try to open the target file */
  target_fd = open (target_path, O_CREAT | O_WRONLY | O_TRUNC, 0700);
  if (G_UNLIKELY (target_fd < 0))
    {
      /* ask the user to skip (for cancellation only) */
      thunar_vfs_transfer_job_skip (job, _("Unable to open file %s."), item->target_path);
      goto failed;
    }

  /* try to copy the file */
  if (!thunar_vfs_transfer_item_copy_legacy (job, source_fd, target_fd))
    {
      /* ask the user to skip (for cancellation only) */
      thunar_vfs_transfer_job_skip (job, _("Unable to copy file %s."), item->source_path);
    }

  close (target_fd);
failed:
  close (source_fd);
}



static void
thunar_vfs_transfer_item_copy_symlink (ThunarVfsTransferItem *item,
                                       const gchar           *source_path,
                                       const gchar           *target_path)
{
  ThunarVfsTransferJob  *job = item->base->job;
  gchar                 *link_target;

  /* try to read the link target */
  link_target = g_file_read_link (source_path, NULL);
  if (G_UNLIKELY (link_target == NULL))
    {
      /* ask the user whether to skip the item (only used for cancellation) */
      thunar_vfs_transfer_job_skip (job, _("Unable to read link target of %s."), item->source_path);
    }
  else
    {
      /* try to create the symlink */
      if (symlink (link_target, target_path) < 0)
        {
          /* and just another skip (again, only for cancellation) */
          thunar_vfs_transfer_job_skip (job, _("Unable to create symlink %s."), item->target_path);
        }
      else
        {
          /* update the percentage of completeness */
          job->completed_bytes += item->size;
          thunar_vfs_transfer_job_percent (job);
        }

      g_free (link_target);
    }
}



/**
 * thunar_vfs_transfer_job_new:
 * @source_uri_list : a list #ThunarVfsURI<!---->s that should be transferred.
 * @target_uri      : a #ThunarVfsURI referring to the target of the transfer.
 * @move            : whether to copy or move the files.
 * @error           : return location for errors or %NULL.
 *
 * Tries to transfer all files referred to by @source_uri_list to the directory
 * referred to by @target_uri. If @move is %TRUE then all successfully transferred
 * files will be unlinked afterwards.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_transfer_job_new (GList        *source_uri_list,
                             ThunarVfsURI *target_uri,
                             gboolean      move,
                             GError      **error)
{
  ThunarVfsTransferJob *job;
  ThunarVfsURI         *uri;
  GList                *lp;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (target_uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job instance */
  job = (ThunarVfsTransferJob *) g_type_create_instance (THUNAR_VFS_TYPE_TRANSFER_JOB);
  job->move = move;

  /* process the source uris */
  for (lp = source_uri_list; lp != NULL; lp = lp->next)
    {
      /* verify the source uri */
      uri = THUNAR_VFS_URI (lp->data);
      if (thunar_vfs_uri_get_scheme (uri) != THUNAR_VFS_URI_SCHEME_FILE)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EINVAL),
                       _("URI scheme not supported for transfer operations"));
          thunar_vfs_job_unref (THUNAR_VFS_JOB (job));
          return NULL;
        }

      /* add a base for the source */
      thunar_vfs_transfer_job_insert_base (job, thunar_vfs_uri_get_path (uri),
                                           thunar_vfs_uri_get_path (target_uri));
    }

  return THUNAR_VFS_JOB (job);
}



