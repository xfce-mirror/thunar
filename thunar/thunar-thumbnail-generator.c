/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-thumbnail-generator.h>



typedef struct _ThunarThumbnailInfo ThunarThumbnailInfo;



static void     thunar_thumbnail_generator_class_init (ThunarThumbnailGeneratorClass *klass);
static void     thunar_thumbnail_generator_init       (ThunarThumbnailGenerator      *generator);
static void     thunar_thumbnail_generator_finalize   (GObject                       *object);
static gboolean thunar_thumbnail_generator_idle       (gpointer                       user_data);
static gpointer thunar_thumbnail_generator_thread     (gpointer                       user_data);
static void     thunar_thumbnail_info_free            (ThunarThumbnailInfo           *info);



struct _ThunarThumbnailGeneratorClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailGenerator
{
  GObject __parent__;

  ThunarVfsThumbFactory *factory;
  volatile GThread      *thread;
  GMutex                *lock;
  GCond                 *cond;
  gint                   idle_id;

  GList                 *requests;
  GList                 *replies;
};

struct _ThunarThumbnailInfo
{
  ThunarFile    *file;
  ThunarVfsInfo *info;
};



G_DEFINE_TYPE (ThunarThumbnailGenerator, thunar_thumbnail_generator, G_TYPE_OBJECT);



static void
thunar_thumbnail_generator_class_init (ThunarThumbnailGeneratorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_thumbnail_generator_finalize;
}



static void
thunar_thumbnail_generator_init (ThunarThumbnailGenerator *generator)
{
  generator->lock = g_mutex_new ();
  generator->cond = g_cond_new ();
  generator->idle_id = -1;
}



static void
thunar_thumbnail_generator_finalize (GObject *object)
{
  ThunarThumbnailGenerator *generator = THUNAR_THUMBNAIL_GENERATOR (object);

  /* acquire the generator lock */
  g_mutex_lock (generator->lock);

  /* release all requests */
  g_list_foreach (generator->requests, (GFunc) thunar_thumbnail_info_free, NULL);
  g_list_free (generator->requests);
  generator->requests = NULL;

  /* wait for the generator thread to terminate */
  while (G_UNLIKELY (generator->thread != NULL))
    g_cond_wait (generator->cond, generator->lock);

  /* release all replies */
  g_list_foreach (generator->replies, (GFunc) thunar_thumbnail_info_free, NULL);
  g_list_free (generator->replies);
  generator->replies = NULL;

  /* drop the idle source (if any) */
  if (G_UNLIKELY (generator->idle_id >= 0))
    g_source_remove (generator->idle_id);

  /* release the thumbnail factory */
  g_object_unref (G_OBJECT (generator->factory));

  /* release the generator lock */
  g_mutex_unlock (generator->lock);

  /* release the cond and mutex */
  g_mutex_free (generator->lock);
  g_cond_free (generator->cond);

  (*G_OBJECT_CLASS (thunar_thumbnail_generator_parent_class)->finalize) (object);
}



static gboolean
thunar_thumbnail_generator_idle (gpointer user_data)
{
  ThunarThumbnailGenerator *generator = THUNAR_THUMBNAIL_GENERATOR (user_data);
  ThunarThumbnailInfo      *info;
  GList                    *infos;
  GList                    *lp;

  /* acquire the lock on the generator */
  g_mutex_lock (generator->lock);

  /* grab the infos returned from the thumbnailer thread */
  infos = generator->replies;
  generator->replies = NULL;

  /* reset the process idle id */
  generator->idle_id = -1;

  /* release the lock on the generator */
  g_mutex_unlock (generator->lock);

  /* acquire the GDK thread lock */
  GDK_THREADS_ENTER ();

  /* invoke "changed" signals on all files */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      /* invoke "changed" and release the info */
      info = (ThunarThumbnailInfo *) lp->data;
      thunar_file_changed (THUNAR_FILE (info->file));
      thunar_thumbnail_info_free (info);
    }
  g_list_free (infos);

  /* release the GDK thread lock */
  GDK_THREADS_LEAVE ();

  return FALSE;
}



static gpointer
thunar_thumbnail_generator_thread (gpointer user_data)
{
  ThunarThumbnailGenerator *generator = THUNAR_THUMBNAIL_GENERATOR (user_data);
  ThunarThumbnailInfo      *info;
  GdkPixbuf                *icon;

  for (;;)
    {
      /* lock the factory */
      g_mutex_lock (generator->lock);

      /* grab the first thumbnail from the request list */
      if (G_LIKELY (generator->requests != NULL))
        {
          info = generator->requests->data;
          generator->requests = g_list_remove (generator->requests, info);
        }
      else
        {
          info = NULL;
        }

      /* check if we have something to generate */
      if (G_UNLIKELY (info == NULL))
        {
          /* reset the thread pointer */
          generator->thread = NULL;
          g_cond_signal (generator->cond);
          g_mutex_unlock (generator->lock);
          break;
        }

      /* release the lock */
      g_mutex_unlock (generator->lock);

      /* try to generate the thumbnail */
      icon = thunar_vfs_thumb_factory_generate_thumbnail (generator->factory, info->info);

      /* store the thumbnail (or the failed notice) */
      thunar_vfs_thumb_factory_store_thumbnail (generator->factory, icon, info->info, NULL);

      /* place the thumbnail on the reply list and schedule the idle source */
      g_mutex_lock (generator->lock);
      generator->replies = g_list_prepend (generator->replies, info);
      if (G_UNLIKELY (generator->idle_id < 0))
        generator->idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_thumbnail_generator_idle, generator, NULL);
      g_mutex_unlock (generator->lock);

      /* release the icon (if any) */
      if (G_LIKELY (icon != NULL))
        g_object_unref (G_OBJECT (icon));
    }

  return NULL;
}



static void
thunar_thumbnail_info_free (ThunarThumbnailInfo *info)
{
  g_object_unref (G_OBJECT (info->file));
  thunar_vfs_info_unref (info->info);
  g_free (info);
}



/**
 * thunar_thumbnail_generator_new:
 * @factory : a #ThunarVfsThumbFactory.
 *
 * Allocates a new #ThunarThumbnailGenerator object,
 * which can be used to generate and store thumbnails
 * for files.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer needed.
 *
 * Return value: a newly allocated #ThunarThumbnailGenerator.
 **/
ThunarThumbnailGenerator*
thunar_thumbnail_generator_new (ThunarVfsThumbFactory *factory)
{
  ThunarThumbnailGenerator *generator;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);

  /* allocate the generator object */
  generator = g_object_new (THUNAR_TYPE_THUMBNAIL_GENERATOR, NULL);
  generator->factory = g_object_ref (G_OBJECT (factory));

  return generator;
}



/**
 * thunar_thumbnail_generator_enqueue:
 * @generator
 * @file
 **/
void
thunar_thumbnail_generator_enqueue (ThunarThumbnailGenerator *generator,
                                    ThunarFile               *file)
{
  ThunarThumbnailInfo *info;
  GError              *error = NULL;
  GList               *lp;

  g_return_if_fail (THUNAR_IS_THUMBNAIL_GENERATOR (generator));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* acquire the generator lock */
  g_mutex_lock (generator->lock);

  /* check if the file is already on the request list */
  for (lp = generator->requests; lp != NULL; lp = lp->next)
    if (((ThunarThumbnailInfo *) lp->data)->file == file)
      break;

  /* schedule a request for the file if it's not already done */
  if (G_LIKELY (lp == NULL))
    {
      /* allocate a thumbnail info for the file */
      info = g_new (ThunarThumbnailInfo, 1);
      info->file = g_object_ref (G_OBJECT (file));
      info->info = thunar_vfs_info_copy (thunar_file_get_info (file));

      /* schedule the request */
      generator->requests = g_list_append (generator->requests, info);

      /* start the generator thread on-demand */
      if (G_UNLIKELY (generator->thread == NULL))
        {
          generator->thread = g_thread_create_full (thunar_thumbnail_generator_thread, generator, 0,
                                                    FALSE, FALSE, G_THREAD_PRIORITY_LOW, &error);
          if (G_UNLIKELY (generator->thread == NULL))
            {
              g_warning ("Failed to launch thumbnail generator thread: %s", error->message);
              g_error_free (error);
            }
        }
    }

  /* release the generator lock */
  g_mutex_unlock (generator->lock);
}



