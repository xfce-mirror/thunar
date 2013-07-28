/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <thunar/thunar-thumbnail-cache-proxy.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnail-cache.h>
#include <thunar/thunar-file.h>

#if GLIB_CHECK_VERSION (2, 32, 0)
#define _thumbnail_cache_lock(cache)   g_mutex_lock (&((cache)->lock))
#define _thumbnail_cache_unlock(cache) g_mutex_unlock (&((cache)->lock))
#else
#define _thumbnail_cache_lock(cache)   g_mutex_lock ((cache)->lock)
#define _thumbnail_cache_unlock(cache) g_mutex_unlock ((cache)->lock)
#endif



static void thunar_thumbnail_cache_finalize (GObject *object);



struct _ThunarThumbnailCacheClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailCache
{
  GObject     __parent__;

#ifdef HAVE_DBUS
  DBusGProxy *cache_proxy;

  GList      *move_source_queue;
  GList      *move_target_queue;
  guint       move_queue_idle_id;

  GList      *copy_source_queue;
  GList      *copy_target_queue;
  guint       copy_queue_idle_id;

  GList      *delete_queue;
  guint       delete_queue_idle_id;

  GList      *cleanup_queue;
  guint       cleanup_queue_idle_id;

#if GLIB_CHECK_VERSION (2, 32, 0)
  GMutex      lock;
#else
  GMutex     *lock;
#endif
#endif
};



G_DEFINE_TYPE (ThunarThumbnailCache, thunar_thumbnail_cache, G_TYPE_OBJECT)



static void
thunar_thumbnail_cache_class_init (ThunarThumbnailCacheClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine the parent type class */
  thunar_thumbnail_cache_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_thumbnail_cache_finalize;
}



static void
thunar_thumbnail_cache_init (ThunarThumbnailCache *cache)
{
#ifdef HAVE_DBUS
  DBusGConnection *connection;

  /* try to connect to D-Bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  if (connection != NULL)
    {
      /* create a proxy for the thumbnail cache service */
      cache->cache_proxy =
        dbus_g_proxy_new_for_name (connection,
                                   "org.freedesktop.thumbnails.Cache1",
                                   "/org/freedesktop/thumbnails/Cache1",
                                   "org.freedesktop.thumbnails.Cache1");

      /* release the D-Bus connection */
      dbus_g_connection_unref (connection);
    }

/* create a new mutex for accessing the cache from different threads */
#if GLIB_CHECK_VERSION (2, 32, 0)
  g_mutex_init (&cache->lock);
#else
  cache->lock = g_mutex_new ();
#endif
#endif
}



static void
thunar_thumbnail_cache_finalize (GObject *object)
{
#ifdef HAVE_DBUS
  ThunarThumbnailCache *cache = THUNAR_THUMBNAIL_CACHE (object);

  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* drop the move queue idle and all queued files */
  if (cache->move_queue_idle_id > 0)
    g_source_remove (cache->move_queue_idle_id);
  g_list_free_full (cache->move_source_queue, g_object_unref);
  g_list_free_full (cache->move_target_queue, g_object_unref);

  /* drop the copy queue idle and all queued files */
  if (cache->copy_queue_idle_id > 0)
    g_source_remove (cache->copy_queue_idle_id);
  g_list_free_full (cache->copy_source_queue, g_object_unref);
  g_list_free_full (cache->copy_target_queue, g_object_unref);

  /* drop the delete queue idle and all queued files */
  if (cache->delete_queue_idle_id > 0)
    g_source_remove (cache->delete_queue_idle_id);
  g_list_free_full (cache->delete_queue, g_object_unref);

  /* drop the cleanup queue idle and all queued files */
  if (cache->cleanup_queue_idle_id > 0)
    g_source_remove (cache->cleanup_queue_idle_id);
  g_list_free_full (cache->cleanup_queue, g_object_unref);

  /* check if we have a valid cache proxy */
  if (cache->cache_proxy != NULL)
    g_object_unref (cache->cache_proxy);

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);

  /* release the mutex itself */
#if GLIB_CHECK_VERSION (2, 32, 0)
  g_mutex_clear (&cache->lock);
#else
  g_mutex_free (cache->lock);
#endif
#endif

  (*G_OBJECT_CLASS (thunar_thumbnail_cache_parent_class)->finalize) (object);
}



#ifdef HAVE_DBUS
static void
thunar_thumbnail_cache_move_copy_async_reply (DBusGProxy *proxy,
                                              GError     *error,
                                              gpointer    user_data)
{
  GList      *li;
  ThunarFile *file;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));

  for (li = user_data; li != NULL; li = li->next)
    {
      file = thunar_file_cache_lookup (G_FILE (li->data));
      g_object_unref (G_OBJECT (li->data));

      if (G_LIKELY (file != NULL))
        {
          /* if visible, let the view know there might be a thumb */
          thunar_file_changed (file);
          g_object_unref (file);
        }
    }

  g_list_free (user_data);
}



static void
thunar_thumbnail_cache_move_async (ThunarThumbnailCache *cache,
                                   const gchar         **source_uris,
                                   const gchar         **target_uris,
                                   gpointer              user_data)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (source_uris != NULL);
  _thunar_return_if_fail (target_uris != NULL);

  /* request a thumbnail cache update asynchronously */
  thunar_thumbnail_cache_proxy_move_async (cache->cache_proxy,
                                           source_uris, target_uris,
                                           thunar_thumbnail_cache_move_copy_async_reply,
                                           user_data);
}



static void
thunar_thumbnail_cache_copy_async (ThunarThumbnailCache *cache,
                                   const gchar         **source_uris,
                                   const gchar         **target_uris,
                                   gpointer              user_data)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (source_uris != NULL);
  _thunar_return_if_fail (target_uris != NULL);

  /* request a thumbnail cache update asynchronously */
  thunar_thumbnail_cache_proxy_copy_async (cache->cache_proxy,
                                           source_uris, target_uris,
                                           thunar_thumbnail_cache_move_copy_async_reply,
                                           user_data);
}



static void
thunar_thumbnail_cache_delete_async_reply (DBusGProxy *proxy,
                                           GError     *error,
                                           gpointer    user_data)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
}



static void
thunar_thumbnail_cache_delete_async (ThunarThumbnailCache *cache,
                                     const gchar         **uris)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (uris != NULL);

  /* request a thumbnail cache update asynchronously */
  thunar_thumbnail_cache_proxy_delete_async (cache->cache_proxy, uris,
                                             thunar_thumbnail_cache_delete_async_reply,
                                             NULL);
}



static void
thunar_thumbnail_cache_cleanup_async_reply (DBusGProxy *proxy,
                                            GError     *error,
                                            gpointer    user_data)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
}



static void
thunar_thumbnail_cache_cleanup_async (ThunarThumbnailCache *cache,
                                      const gchar *const    *base_uris)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (base_uris != NULL);

  /* request a thumbnail cache update asynchronously */
  thunar_thumbnail_cache_proxy_cleanup_async (cache->cache_proxy,
                                              (const gchar **)base_uris, 0,
                                              thunar_thumbnail_cache_cleanup_async_reply,
                                              NULL);
}



static gboolean
thunar_thumbnail_cache_process_queue (ThunarThumbnailCache *cache,
                                      gboolean              copy_async)
{
  GList  *sp;
  GList  *tp;
  gchar **source_uris;
  gchar **target_uris;
  guint   n_uris;
  guint   n;
  GList  *source_queue;
  GList  *target_queue;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache), FALSE);

  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* steal the lists */
  if (copy_async)
    {
      source_queue = cache->copy_source_queue;
      cache->copy_source_queue = NULL;
      target_queue = cache->copy_target_queue;
      cache->copy_target_queue = NULL;
    }
  else
    {
      source_queue = cache->move_source_queue;
      cache->move_source_queue = NULL;
      target_queue = cache->move_target_queue;
      cache->move_target_queue = NULL;
    }

  /* compute how many URIs there are */
  n_uris = g_list_length (source_queue);

  /* allocate a string array for the URIs */
  source_uris = g_new0 (gchar *, n_uris + 1);
  target_uris = g_new0 (gchar *, n_uris + 1);

  /* fill URI array with file URIs from the move queue */
  for (n = 0,
       sp = g_list_last (source_queue),
       tp = g_list_last (target_queue);
       sp != NULL && tp != NULL;
       sp = sp->prev, tp = tp->prev, ++n)
    {
      source_uris[n] = g_file_get_uri (sp->data);
      target_uris[n] = g_file_get_uri (tp->data);

      /* release the source object */
      g_object_unref (sp->data);
    }

  /* NULL-terminate the URI arrays */
  source_uris[n] = NULL;
  target_uris[n] = NULL;

  if (copy_async)
    {
      /* asynchronously copy the thumbnails */
      thunar_thumbnail_cache_copy_async (cache,
                                         (const gchar **)source_uris,
                                         (const gchar **)target_uris,
                                         target_queue);
    }
  else
    {
      /* asynchronously move the thumbnails */
      thunar_thumbnail_cache_move_async (cache,
                                         (const gchar **)source_uris,
                                         (const gchar **)target_uris,
                                         target_queue);
    }

  /* free the URI arrays */
  g_strfreev (source_uris);
  g_strfreev (target_uris);

  /* release the move queue lists */
  g_list_free (source_queue);

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);

  return FALSE;
}



static gboolean
thunar_thumbnail_cache_process_move_queue (gpointer user_data)
{
  return thunar_thumbnail_cache_process_queue (user_data, FALSE);
}



static void
thunar_thumbnail_cache_process_move_queue_destroy (gpointer user_data)
{
  THUNAR_THUMBNAIL_CACHE (user_data)->move_queue_idle_id = 0;
}



static gboolean
thunar_thumbnail_cache_process_copy_queue (gpointer user_data)
{
  return thunar_thumbnail_cache_process_queue (user_data, TRUE);
}



static void
thunar_thumbnail_cache_process_copy_queue_destroy (gpointer user_data)
{
  THUNAR_THUMBNAIL_CACHE (user_data)->copy_queue_idle_id = 0;
}



static gboolean
thunar_thumbnail_cache_process_delete_queue (ThunarThumbnailCache *cache)
{
  GList  *lp;
  gchar **uris;
  guint   n_uris;
  guint   n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache), FALSE);

  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* compute how many URIs there are */
  n_uris = g_list_length (cache->delete_queue);

  /* allocate a string array for the URIs */
  uris = g_new0 (gchar *, n_uris + 1);

  /* fill URI array with file URIs from the delete queue */
  for (lp = g_list_last (cache->delete_queue), n = 0; lp != NULL; lp = lp->prev, ++n)
    {
      uris[n] = g_file_get_uri (lp->data);

      /* release the file object */
      g_object_unref (lp->data);
    }

  /* NULL-terminate the URI array */
  uris[n] = NULL;

  /* asynchronously delete the thumbnails */
  thunar_thumbnail_cache_delete_async (cache, (const gchar **)uris);

  /* free the URI array */
  g_strfreev (uris);

  /* release the delete queue list */
  g_list_free (cache->delete_queue);
  cache->delete_queue = NULL;

  /* reset the delete queue idle ID */
  cache->delete_queue_idle_id = 0;

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);

  return FALSE;
}



static gboolean
thunar_thumbnail_cache_process_cleanup_queue (ThunarThumbnailCache *cache)
{
  GList  *lp;
  gchar **uris;
  guint   n_uris;
  guint   n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache), FALSE);

  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* compute how many URIs there are */
  n_uris = g_list_length (cache->cleanup_queue);

  /* allocate a string array for the URIs */
  uris = g_new0 (gchar *, n_uris + 1);

#ifndef NDEBUG
  g_debug ("cleanup:");
#endif

  /* fill URI array with file URIs from the cleanup queue */
  for (lp = g_list_last (cache->cleanup_queue), n = 0; lp != NULL; lp = lp->prev, ++n)
    {
      uris[n] = g_file_get_uri (lp->data);

#ifndef NDEBUG
      g_debug ("  %s", uris[n]);
#endif

      /* release the file object */
      g_object_unref (lp->data);
    }

  /* NULL-terminate the URI array */
  uris[n] = NULL;

  /* asynchronously cleanup the thumbnails */
  thunar_thumbnail_cache_cleanup_async (cache, (const gchar *const *)uris);

  /* free the URI array */
  g_strfreev (uris);

  /* release the cleanup queue list */
  g_list_free (cache->cleanup_queue);
  cache->cleanup_queue = NULL;

  /* reset the cleanup queue idle ID */
  cache->cleanup_queue_idle_id = 0;

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);

  return FALSE;
}
#endif /* HAVE_DBUS */



ThunarThumbnailCache *
thunar_thumbnail_cache_new (void)
{
  return g_object_new (THUNAR_TYPE_THUMBNAIL_CACHE, NULL);
}



void
thunar_thumbnail_cache_move_file (ThunarThumbnailCache *cache,
                                  GFile                *source_file,
                                  GFile                *target_file)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (G_IS_FILE (source_file));
  _thunar_return_if_fail (G_IS_FILE (target_file));

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* check if we have a valid proxy for the cache service */
  if (cache->cache_proxy != NULL)
    {
      /* cancel any pending timeout to process the move queue */
      if (cache->move_queue_idle_id > 0)
        {
          g_source_remove (cache->move_queue_idle_id);
          cache->move_queue_idle_id = 0;
        }

      /* add the files to the move queue */
      cache->move_source_queue = g_list_prepend (cache->move_source_queue,
                                                 g_object_ref (source_file));
      cache->move_target_queue = g_list_prepend (cache->move_target_queue,
                                                 g_object_ref (target_file));

      /* process the move queue in a 250ms timeout */
      cache->move_queue_idle_id =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 250, thunar_thumbnail_cache_process_move_queue,
                            cache, thunar_thumbnail_cache_process_move_queue_destroy);
    }

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);
#endif
}



void
thunar_thumbnail_cache_copy_file (ThunarThumbnailCache *cache,
                                  GFile                *source_file,
                                  GFile                *target_file)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (G_IS_FILE (source_file));
  _thunar_return_if_fail (G_IS_FILE (target_file));

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* check if we have a valid proxy for the cache service */
  if (cache->cache_proxy != NULL)
    {
      /* cancel any pending timeout to process the copy queue */
      if (cache->copy_queue_idle_id > 0)
        {
          g_source_remove (cache->copy_queue_idle_id);
          cache->copy_queue_idle_id = 0;
        }

      /* add the files to the copy queues */
      cache->copy_source_queue = g_list_prepend (cache->copy_source_queue,
                                                 g_object_ref (source_file));
      cache->copy_target_queue = g_list_prepend (cache->copy_target_queue,
                                                 g_object_ref (target_file));

      /* process the copy queue in a 250ms timeout */
      cache->copy_queue_idle_id =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 500, thunar_thumbnail_cache_process_copy_queue,
                            cache, thunar_thumbnail_cache_process_copy_queue_destroy);
    }

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);
#endif
}



void
thunar_thumbnail_cache_delete_file (ThunarThumbnailCache *cache,
                                    GFile                *file)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (G_IS_FILE (file));

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* check if we have a valid proxy for the cache service */
  if (cache->cache_proxy)
    {
      /* cancel any pending timeout to process the delete queue */
      if (cache->delete_queue_idle_id > 0)
        {
          g_source_remove (cache->delete_queue_idle_id);
          cache->delete_queue_idle_id = 0;
        }

      /* add the file to the delete queue */
      cache->delete_queue = g_list_prepend (cache->delete_queue, g_object_ref (file));

      /* process the delete queue in a 250ms timeout */
      cache->delete_queue_idle_id =
        g_timeout_add (500, (GSourceFunc) thunar_thumbnail_cache_process_delete_queue,
                       cache);
    }

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);
#endif
}



void
thunar_thumbnail_cache_cleanup_file (ThunarThumbnailCache *cache,
                                     GFile                *file)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (G_IS_FILE (file));

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  _thumbnail_cache_lock (cache);

  /* check if we have a valid proxy for the cache service */
  if (cache->cache_proxy)
    {
      /* cancel any pending timeout to process the cleanup queue */
      if (cache->cleanup_queue_idle_id > 0)
        {
          g_source_remove (cache->cleanup_queue_idle_id);
          cache->cleanup_queue_idle_id = 0;
        }

      /* add the file to the cleanup queue */
      cache->cleanup_queue = g_list_prepend (cache->cleanup_queue, g_object_ref (file));

      /* process the cleanup queue in a 250ms timeout */
      cache->cleanup_queue_idle_id =
        g_timeout_add (1000, (GSourceFunc) thunar_thumbnail_cache_process_cleanup_queue,
                       cache);
    }

  /* release the cache lock */
  _thumbnail_cache_unlock (cache);
#endif
}
