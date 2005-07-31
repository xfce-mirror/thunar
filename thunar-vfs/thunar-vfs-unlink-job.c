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

#include <thunar-vfs/thunar-vfs-sysdep.h>
#include <thunar-vfs/thunar-vfs-trash.h>
#include <thunar-vfs/thunar-vfs-unlink-job.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_remove(path) (remove ((path)))
#endif



typedef struct _ThunarVfsUnlinkBase ThunarVfsUnlinkBase;
typedef struct _ThunarVfsUnlinkItem ThunarVfsUnlinkItem;



static void thunar_vfs_unlink_job_register_type (GType               *type);
static void thunar_vfs_unlink_job_class_init    (ThunarVfsJobClass   *klass);
static void thunar_vfs_unlink_job_init          (ThunarVfsUnlinkJob  *unlink_job);
static void thunar_vfs_unlink_job_execute       (ThunarVfsJob        *job);
static void thunar_vfs_unlink_job_finalize      (ThunarVfsJob        *job);
static void thunar_vfs_unlink_base_new          (ThunarVfsUnlinkJob  *unlink_job,
                                                 const gchar         *path,
                                                 const gchar         *trash_info);
static void thunar_vfs_unlink_base_collect      (ThunarVfsUnlinkBase *base);
static void thunar_vfs_unlink_base_remove       (ThunarVfsUnlinkBase *base);
static void thunar_vfs_unlink_item_collect      (ThunarVfsUnlinkItem *item);
static void thunar_vfs_unlink_item_remove       (ThunarVfsUnlinkItem *item);



struct _ThunarVfsUnlinkJob
{
  ThunarVfsInteractiveJob __parent__;

  ThunarVfsUnlinkBase *bases;

  GMemChunk           *base_chunk;
  GMemChunk           *item_chunk;
  GStringChunk        *string_chunk;

  gint                 total_items;
  gint                 completed_items;

  /* dirent item buffer to reduce stack overhead */
  struct dirent        dirent_buffer;
};

struct _ThunarVfsUnlinkBase
{
  ThunarVfsUnlinkJob  *job;
  ThunarVfsUnlinkBase *next;
  ThunarVfsUnlinkItem *items;
  gchar               *path;
  gchar               *trash_info_path;
};

struct _ThunarVfsUnlinkItem
{
  ThunarVfsUnlinkBase *base;
  ThunarVfsUnlinkItem *children;
  ThunarVfsUnlinkItem *next;
  gchar               *path;
};



static ThunarVfsJobClass *thunar_vfs_unlink_job_parent_class;



GType
thunar_vfs_unlink_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_unlink_job_register_type, &type);

  return type;
}



static void
thunar_vfs_unlink_job_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsInteractiveJobClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_unlink_job_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsUnlinkJob),
    0,
    (GInstanceInitFunc) thunar_vfs_unlink_job_init,
    NULL,
  };

  *type = g_type_register_static (THUNAR_VFS_TYPE_INTERACTIVE_JOB,
                                  "ThunarVfsUnlinkJob", &info, 0);
}



static void
thunar_vfs_unlink_job_class_init (ThunarVfsJobClass *klass)
{
  thunar_vfs_unlink_job_parent_class = g_type_class_peek_parent (klass);

  klass->execute = thunar_vfs_unlink_job_execute;
  klass->finalize = thunar_vfs_unlink_job_finalize;
}



static void
thunar_vfs_unlink_job_init (ThunarVfsUnlinkJob *unlink_job)
{
  unlink_job->base_chunk = g_mem_chunk_new ("ThunarVfsUnlinkBase chunk",
                                            sizeof (ThunarVfsUnlinkBase),
                                            sizeof (ThunarVfsUnlinkBase) * 128,
                                            G_ALLOC_ONLY);
  unlink_job->item_chunk = g_mem_chunk_new ("ThunarVfsUnlinkItem chunk",
                                            sizeof (ThunarVfsUnlinkItem),
                                            sizeof (ThunarVfsUnlinkItem) * 256,
                                            G_ALLOC_ONLY);
  unlink_job->string_chunk = g_string_chunk_new (4096);
}



static void
thunar_vfs_unlink_job_execute (ThunarVfsJob *job)
{
  ThunarVfsUnlinkBase *base;
  ThunarVfsUnlinkJob  *unlink_job = THUNAR_VFS_UNLINK_JOB (job);

  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), _("Preparing..."));

  /* collect all bases */
  for (base = unlink_job->bases; !thunar_vfs_job_cancelled (job) && base != NULL; base = base->next)
    thunar_vfs_unlink_base_collect (base);

  /* remove all bases */
  for (base = unlink_job->bases; !thunar_vfs_job_cancelled (job) && base != NULL; base = base->next)
    thunar_vfs_unlink_base_remove (base);
}



static void
thunar_vfs_unlink_job_finalize (ThunarVfsJob *job)
{
  ThunarVfsUnlinkJob *unlink_job = THUNAR_VFS_UNLINK_JOB (job);

  /* free the various chunks */
  g_mem_chunk_destroy (unlink_job->base_chunk);
  g_mem_chunk_destroy (unlink_job->item_chunk);
  g_string_chunk_free (unlink_job->string_chunk);

  /* call the parents finalize method */
  if (THUNAR_VFS_JOB_CLASS (thunar_vfs_unlink_job_parent_class)->finalize != NULL)
    (*THUNAR_VFS_JOB_CLASS (thunar_vfs_unlink_job_parent_class)->finalize) (job);
}



static void
thunar_vfs_unlink_base_new (ThunarVfsUnlinkJob *unlink_job,
                            const gchar        *path,
                            const gchar        *trash_info)
{
  ThunarVfsUnlinkBase *base;
  ThunarVfsUnlinkItem *item;
  gchar               *basename;
  gchar               *dirname;

  /* determine base/dirname */
  basename = g_path_get_basename (path);
  dirname = g_path_get_dirname (path);

  /* allocate the base */
  base = g_chunk_new (ThunarVfsUnlinkBase, unlink_job->base_chunk);
  base->job = unlink_job;
  base->path = g_string_chunk_insert (unlink_job->string_chunk, dirname);
  base->next = unlink_job->bases;
  base->trash_info_path = (trash_info != NULL) ? g_string_chunk_insert (unlink_job->string_chunk, trash_info) : NULL;
  unlink_job->bases = base;

  /* setup the initial item (toplevel) */
  item = g_chunk_new (ThunarVfsUnlinkItem, unlink_job->item_chunk);
  item->base = base;
  item->children = NULL;
  item->next = NULL;
  item->path = g_string_chunk_insert (unlink_job->string_chunk, basename);
  base->items = item;

  /* cleanup */
  g_free (basename);
  g_free (dirname);
}



static void
thunar_vfs_unlink_base_collect (ThunarVfsUnlinkBase *base)
{
  ThunarVfsUnlinkItem *item;
  ThunarVfsUnlinkJob  *job = base->job;

  for (item = base->items; !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)) && item != NULL; item = item->next)
    thunar_vfs_unlink_item_collect (item);
}



static void
thunar_vfs_unlink_base_remove (ThunarVfsUnlinkBase *base)
{
  ThunarVfsUnlinkItem *item;
  ThunarVfsUnlinkJob  *job = base->job;

  /* remove all items */
  for (item = base->items; !thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)) && item != NULL; item = item->next)
    thunar_vfs_unlink_item_remove (item);

  /* remove the trashinfo file (if any) */
  if (G_UNLIKELY (base->trash_info_path != NULL))
    g_remove (base->trash_info_path);
}



static void
thunar_vfs_unlink_item_collect (ThunarVfsUnlinkItem *item)
{
  ThunarVfsUnlinkBase *base = item->base;
  ThunarVfsUnlinkItem *child_item;
  ThunarVfsUnlinkJob  *job = base->job;
  struct dirent       *d;
  gchar               *path;
  DIR                 *dirp;

  /* count this item */
  ++job->total_items;

  /* try to open this item as directory */
  path = g_build_filename (base->path, item->path, NULL);
  dirp = opendir (path);
  g_free (path);

  /* collect directory entries */
  if (G_UNLIKELY (dirp != NULL))
    {
      while (_thunar_vfs_sysdep_readdir (dirp, &job->dirent_buffer, &d, NULL) && d != NULL)
        {
          /* check if the operation was cancelled in the meantime */
          if (G_UNLIKELY (thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job))))
            break;

          /* allocate a new item */
          path = g_build_filename (item->path, d->d_name, NULL);
          child_item = g_chunk_new (ThunarVfsUnlinkItem, job->item_chunk);
          child_item->base = base;
          child_item->path = g_string_chunk_insert (job->string_chunk, path);
          child_item->children = NULL;
          g_free (path);

          /* connect the item to the tree */
          child_item->next = item->children;
          item->children = child_item;

          /* collect the new item */
          thunar_vfs_unlink_item_collect (child_item);
        }

      closedir (dirp);
    }
}



static void
thunar_vfs_unlink_item_remove (ThunarVfsUnlinkItem *item)
{
  ThunarVfsUnlinkBase *base = item->base;
  ThunarVfsUnlinkItem *child_item;
  ThunarVfsUnlinkJob  *job = base->job;
  gdouble              percent;
  gchar               *display;
  gchar               *message;
  gchar               *path;

  /* unlink all children first (if any) */
  for (child_item = item->children; child_item != NULL; child_item = child_item->next)
    thunar_vfs_unlink_item_remove (child_item);

  /* verify that the user did not cancel yet */
  if (thunar_vfs_job_cancelled (THUNAR_VFS_JOB (job)))
    return;

  /* update the progress message */
  display = g_filename_display_name (item->path);
  thunar_vfs_interactive_job_info_message (THUNAR_VFS_INTERACTIVE_JOB (job), display);
  g_free (display);

  /* try to unlink this item */
  path = g_build_filename (base->path, item->path, NULL);
  if (G_UNLIKELY (g_remove (path) < 0 && errno != ENOENT))
    {
      display = g_filename_display_name (item->path);
      message = g_strdup_printf (_("Unable to remove %s.\n\nDo you want to skip it?"), display);
      thunar_vfs_interactive_job_skip (THUNAR_VFS_INTERACTIVE_JOB (job), message);
      g_free (message);
      g_free (display);
    }
  g_free (path);

  /* update the progress status */
  percent = (++job->completed_items * 100.0) / MAX (job->total_items, 1);
  percent = CLAMP (percent, 0.0, 100.0);
  thunar_vfs_interactive_job_percent (THUNAR_VFS_INTERACTIVE_JOB (job), percent);
}



/**
 * thunar_vfs_unlink_job_new:
 * @uris  : a list of #ThunarVfsURI<!---->s, that should be unlinked.
 * @error : return location for errors or %NULL.
 *
 * Tries to allocate a new #ThunarVfsUnlinkJob, which can be used to
 * unlink the given @uris.
 *
 * Return value: the newly allocated #ThunarVfsUnlinkJob or %NULL on error.
 **/
ThunarVfsJob*
thunar_vfs_unlink_job_new (GList   *uris,
                           GError **error)
{
  ThunarVfsTrashManager *manager;
  ThunarVfsUnlinkJob    *job;
  ThunarVfsURIScheme     scheme;
  ThunarVfsTrash        *trash;
  gchar                 *info_path;
  gchar                 *filename;
  gchar                 *path;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job instance */
  job = (ThunarVfsUnlinkJob *) g_type_create_instance (THUNAR_VFS_TYPE_UNLINK_JOB);

  /* process the uris */
  for (; uris != NULL; uris = uris->next)
    {
      /* determine the URI scheme and path information */
      scheme = thunar_vfs_uri_get_scheme (uris->data);
      if (scheme == THUNAR_VFS_URI_SCHEME_FILE && !thunar_vfs_uri_is_root (uris->data))
        {
          thunar_vfs_unlink_base_new (job, thunar_vfs_uri_get_path (uris->data), NULL);
        }
      else if (scheme == THUNAR_VFS_URI_SCHEME_TRASH && !thunar_vfs_uri_is_root (uris->data))
        {
          /* determine the trash in which the file is stored */
          manager = thunar_vfs_trash_manager_get_default ();
          trash = thunar_vfs_trash_manager_resolve_uri (manager, uris->data, &filename, error);
          g_object_unref (G_OBJECT (manager));

          /* verify the trash */
          if (G_UNLIKELY (trash == NULL))
            {
              thunar_vfs_job_unref (THUNAR_VFS_JOB (job));
              return NULL;
            }

          /* determine the real path */
          path = thunar_vfs_trash_get_path (trash, filename);

          /* check if we're about to unlink a toplevel trash node */
          if (strchr (filename, '/') == NULL)
            {
              /* we need to remove the info file as well */
              info_path = thunar_vfs_trash_get_info_path (trash, filename);
              thunar_vfs_unlink_base_new (job, path, info_path);
              g_free (info_path);
            }
          else
            {
              /* we can treat it like a default unlink, no special
               * processing required here.
               */
              thunar_vfs_unlink_base_new (job, path, NULL);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (trash));
          g_free (filename);
          g_free (path);
        }
      else if (thunar_vfs_uri_is_root (uris->data))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EINVAL),
                       _("Unable to unlink root nodes"));
          thunar_vfs_job_unref (THUNAR_VFS_JOB (job));
          return NULL;
        }
      else 
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EINVAL),
                       _("URI scheme not supported for unlink operations"));
          thunar_vfs_job_unref (THUNAR_VFS_JOB (job));
          return NULL;
        }
    }

  return THUNAR_VFS_JOB (job);
}



