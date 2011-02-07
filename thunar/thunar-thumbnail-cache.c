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

  GMutex     *lock;
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
  cache->lock = g_mutex_new ();
#endif
}



static void
thunar_thumbnail_cache_finalize (GObject *object)
{
  ThunarThumbnailCache *cache = THUNAR_THUMBNAIL_CACHE (object);

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  g_mutex_lock (cache->lock);

  /* check if we have a valid cache proxy */
  if (cache->cache_proxy != NULL)
    {
      /* release the cache proxy itself */
      g_object_unref (cache->cache_proxy);
    }

  /* release the cache lock */
  g_mutex_unlock (cache->lock);

  /* release the mutex itself */
  g_mutex_free (cache->lock);
#endif

  (*G_OBJECT_CLASS (thunar_thumbnail_cache_parent_class)->finalize) (object);
}



#ifdef HAVE_DBUS
static void
thunar_thumbnail_cache_move_async_reply (DBusGProxy *proxy,
                                         GError     *error,
                                         gpointer    user_data)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
}



static void
thunar_thumbnail_cache_move_async (ThunarThumbnailCache *cache,
                                   const gchar         **source_uris,
                                   const gchar         **target_uris)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (source_uris != NULL);
  _thunar_return_if_fail (target_uris != NULL);

  /* request a thumbnail cache update asynchronously */
  thunar_thumbnail_cache_proxy_move_async (cache->cache_proxy,
                                           source_uris, target_uris,
                                           thunar_thumbnail_cache_move_async_reply,
                                           NULL);
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
  gchar *source_uris[2] = { NULL, NULL };
  gchar *target_uris[2] = { NULL, NULL };

  _thunar_return_if_fail (THUNAR_IS_THUMBNAIL_CACHE (cache));
  _thunar_return_if_fail (G_IS_FILE (source_file));
  _thunar_return_if_fail (G_IS_FILE (target_file));

#ifdef HAVE_DBUS
  /* acquire a cache lock */
  g_mutex_lock (cache->lock);

  /* check if we have a valid proxy for the cache service */
  if (cache->cache_proxy != NULL)
    {
      /* build the source and target URI arrays */
      source_uris[0] = g_file_get_uri (source_file);
      target_uris[0] = g_file_get_uri (target_file);

      /* notify the thumbnail cache of the move operation. the cache
       * can thereby copy the thumbnail and we avoid regenerating it
       * just because the file name changed */
      thunar_thumbnail_cache_move_async (cache,
                                         (const gchar **)source_uris, 
                                         (const gchar **)target_uris);

      /* release the source and target URI */
      g_free (source_uris[0]);
      g_free (target_uris[0]);
    }

  /* release the cache lock */
  g_mutex_unlock (cache->lock);
#endif
}
