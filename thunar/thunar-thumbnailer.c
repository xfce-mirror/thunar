/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnailer.h>
#include <thunar/thunar-thumbnailer-proxy.h>
#include <thunar/thunar-thumbnailer-manager-proxy.h>



typedef enum
{
  THUNAR_THUMBNAILER_IDLE_ERROR,
  THUNAR_THUMBNAILER_IDLE_READY,
  THUNAR_THUMBNAILER_IDLE_STARTED,
} ThunarThumbnailerIdleType;



typedef struct _ThunarThumbnailerCall ThunarThumbnailerCall;
typedef struct _ThunarThumbnailerIdle ThunarThumbnailerIdle;



static void     thunar_thumbnailer_init_thumbnailer_proxy (ThunarThumbnailer     *thumbnailer,
                                                           DBusGConnection       *connection);
static void     thunar_thumbnailer_init_manager_proxy     (ThunarThumbnailer     *thumbnailer,
                                                           DBusGConnection       *connection);
static void     thunar_thumbnailer_finalize               (GObject               *object);
static gboolean thunar_thumbnailer_file_is_supported      (ThunarThumbnailer     *thumbnailer,
                                                           ThunarFile            *file);
static void     thunar_thumbnailer_thumbnailer_finished   (DBusGProxy            *proxy,
                                                           guint                  handle,
                                                           ThunarThumbnailer     *thumbnailer);
static void     thunar_thumbnailer_thumbnailer_error      (DBusGProxy            *proxy,
                                                           guint                  handle,
                                                           const gchar          **uris,
                                                           gint                   code,
                                                           const gchar           *message,
                                                           ThunarThumbnailer     *thumbnailer);
static void     thunar_thumbnailer_thumbnailer_ready      (DBusGProxy            *proxy,
                                                           const gchar          **uris,
                                                           ThunarThumbnailer     *thumbnailer);
static void     thunar_thumbnailer_thumbnailer_started    (DBusGProxy            *proxy,
                                                           guint                  handle,
                                                           ThunarThumbnailer     *thumbnailer);
static gpointer thunar_thumbnailer_queue_async            (ThunarThumbnailer     *thumbnailer,
                                                           const gchar          **uris,
                                                           const gchar          **mime_hints);
static gboolean thunar_thumbnailer_error_idle             (gpointer               user_data);
static gboolean thunar_thumbnailer_ready_idle             (gpointer               user_data);
static gboolean thunar_thumbnailer_started_idle           (gpointer               user_data);
static void     thunar_thumbnailer_call_free              (ThunarThumbnailerCall *call);
static void     thunar_thumbnailer_idle_free              (gpointer               data);



struct _ThunarThumbnailerClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailer
{
  GObject __parent__;

  /* proxies to communicate with D-Bus services */
  DBusGProxy *manager_proxy;
  DBusGProxy *thumbnailer_proxy;

  /* hash table to map D-Bus service handles to ThunarThumbnailer requests */
  GHashTable *handle_request_mapping;

  /* hash table to map ThunarThumbnailer requests to D-Bus service handles */
  GHashTable *request_handle_mapping;

  /* hash table to map ThunarThumbnailer requests to DBusGProxyCalls */
  GHashTable *request_call_mapping;

  /* hash table to map ThunarThumbnailer requests to URI arrays */
  GHashTable *request_uris_mapping;

  GMutex     *lock;

  /* cached array of MIME types for which thumbnails can be generated */
  gchar     **supported_types;

  /* last ThunarThumbnailer request ID */
  gpointer    last_request;

  /* IDs of idle functions */
  GList      *idles;
};

struct _ThunarThumbnailerCall
{
  ThunarThumbnailer *thumbnailer;
  gpointer           request;
};

struct _ThunarThumbnailerIdle
{
  ThunarThumbnailerIdleType type;
  ThunarThumbnailer        *thumbnailer;
  guint                     id;

  union
  {
    char                  **uris;
    gpointer                request;
  }                         data;
};



static DBusGProxy *thunar_thumbnailer_manager_proxy;
static DBusGProxy *thunar_thumbnailer_proxy;
static DBusGProxy *thunar_manager_proxy;



G_DEFINE_TYPE (ThunarThumbnailer, thunar_thumbnailer, G_TYPE_OBJECT);



static void
thunar_thumbnailer_class_init (ThunarThumbnailerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_thumbnailer_finalize;
}



static void
thunar_thumbnailer_init (ThunarThumbnailer *thumbnailer)
{
  DBusGConnection *connection;

  thumbnailer->lock = g_mutex_new ();
  thumbnailer->supported_types = NULL;
  thumbnailer->last_request = GUINT_TO_POINTER (0);
  thumbnailer->idles = NULL;

  /* try to connect to D-Bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  /* initialize the proxies */
  thunar_thumbnailer_init_thumbnailer_proxy (thumbnailer, connection);
  thunar_thumbnailer_init_manager_proxy (thumbnailer, connection);

  /* check if we have a thumbnailer proxy */
  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      /* we do, set up the hash tables */
      thumbnailer->request_handle_mapping = 
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
      thumbnailer->handle_request_mapping = 
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
      thumbnailer->request_call_mapping = 
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
      thumbnailer->request_uris_mapping =
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, 
                               (GDestroyNotify) g_strfreev);
    }

  /* release the D-Bus connection if we have one */
  if (connection != NULL)
    dbus_g_connection_unref (connection);
}



static void
thunar_thumbnailer_init_thumbnailer_proxy (ThunarThumbnailer *thumbnailer,
                                           DBusGConnection   *connection)
{
  /* we can't have a proxy without a D-Bus connection */
  if (connection == NULL)
    {
      thumbnailer->thumbnailer_proxy = NULL;
      return;
    }

  /* create the thumbnailer proxy shared by all ThunarThumbnailers on demand */
  if (thunar_thumbnailer_proxy == NULL)
    {
      /* create the shared thumbnailer proxy */
      thunar_thumbnailer_proxy = 
        dbus_g_proxy_new_for_name (connection, 
                                   "org.freedesktop.thumbnails.Thumbnailer",
                                   "/org/freedesktop/thumbnails/Thumbnailer",
                                   "org.freedesktop.thumbnails.Thumbnailer");

      /* make sure to set it to NULL when the last reference is dropped */
      g_object_add_weak_pointer (G_OBJECT (thunar_thumbnailer_proxy),
                                 (gpointer) &thunar_thumbnailer_proxy);

      thumbnailer->thumbnailer_proxy = thunar_thumbnailer_proxy;

      /* TODO this should actually be VOID:UINT,POINTER,INT,STRING */
      dbus_g_object_register_marshaller (_thunar_marshal_VOID__UINT_POINTER_UINT_STRING,
                                         G_TYPE_NONE,
                                         G_TYPE_UINT, 
                                         G_TYPE_STRV, 
                                         G_TYPE_UINT, 
                                         G_TYPE_STRING,
                                         G_TYPE_INVALID);

      dbus_g_object_register_marshaller ((GClosureMarshal) g_cclosure_marshal_VOID__POINTER,
                                         G_TYPE_NONE,
                                         G_TYPE_STRV,
                                         G_TYPE_INVALID);

      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Error", 
                               G_TYPE_UINT, G_TYPE_STRV, G_TYPE_UINT, G_TYPE_STRING, 
                               G_TYPE_INVALID);
      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Finished", 
                               G_TYPE_UINT, G_TYPE_INVALID);
      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Ready", 
                               G_TYPE_STRV, G_TYPE_INVALID);
      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Started", 
                               G_TYPE_UINT, G_TYPE_INVALID);
    }
  else
    {
      thumbnailer->thumbnailer_proxy = g_object_ref (thunar_thumbnailer_proxy);
    }

  dbus_g_proxy_connect_signal (thumbnailer->thumbnailer_proxy, "Error",
                               G_CALLBACK (thunar_thumbnailer_thumbnailer_error), 
                               thumbnailer, NULL);
  dbus_g_proxy_connect_signal (thumbnailer->thumbnailer_proxy, "Finished",
                               G_CALLBACK (thunar_thumbnailer_thumbnailer_finished), 
                               thumbnailer, NULL);
  dbus_g_proxy_connect_signal (thumbnailer->thumbnailer_proxy, "Ready",
                               G_CALLBACK (thunar_thumbnailer_thumbnailer_ready), 
                               thumbnailer, NULL);
  dbus_g_proxy_connect_signal (thumbnailer->thumbnailer_proxy, "Started", 
                               G_CALLBACK (thunar_thumbnailer_thumbnailer_started), 
                               thumbnailer, NULL);
}



static void
thunar_thumbnailer_init_manager_proxy (ThunarThumbnailer *thumbnailer,
                                       DBusGConnection   *connection)
{
  /* we cannot have a proxy without a D-Bus connection */
  if (connection == NULL)
    {
      thumbnailer->manager_proxy = NULL;
      return;
    }

  /* create the manager proxy shared by all ThunarThumbnailers on demand */
  if (thunar_manager_proxy == NULL)
    {
      /* create the shared manager proxy */
      thunar_thumbnailer_manager_proxy = 
        dbus_g_proxy_new_for_name (connection, 
                                   "org.freedesktop.thumbnails.Manager",
                                   "/org/freedesktop/thumbnails/Manager",
                                   "org.freedesktop.thumbnails.Manager");

      /* make sure to set it to NULL when the last reference is dropped */
      g_object_add_weak_pointer (G_OBJECT (thunar_thumbnailer_manager_proxy),
                                 (gpointer) &thunar_thumbnailer_manager_proxy);

      thumbnailer->manager_proxy = thunar_thumbnailer_manager_proxy;
    }
  else
    {
      thumbnailer->manager_proxy = g_object_ref (thunar_thumbnailer_manager_proxy);
    }
}



static void
thunar_thumbnailer_finalize (GObject *object)
{
  ThunarThumbnailerIdle *idle;
  ThunarThumbnailer     *thumbnailer = THUNAR_THUMBNAILER (object);
  GList                 *list;
  GList                 *lp;

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  /* abort all pending idle functions */
  for (lp = thumbnailer->idles; lp != NULL; lp = lp->next)
    {
      idle = lp->data;
      g_source_remove (idle->id);
    }

  /* free the idle list */
  g_list_free (thumbnailer->idles);

  if (thumbnailer->manager_proxy != NULL)
    {
      /* disconnect from the manager proxy */
      g_signal_handlers_disconnect_matched (thumbnailer->manager_proxy,
                                            G_SIGNAL_MATCH_DATA, 0, 0,
                                            NULL, NULL, thumbnailer);
  
      /* release the manager proxy */
      g_object_unref (thumbnailer->manager_proxy);
    }

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      /* cancel all pending D-Bus calls */
      list = g_hash_table_get_values (thumbnailer->request_call_mapping);
      for (lp = list; lp != NULL; lp = lp->next)
        dbus_g_proxy_cancel_call (thumbnailer->thumbnailer_proxy, lp->data);
      g_list_free (list);

      g_hash_table_unref (thumbnailer->request_call_mapping);

#if 0 
      /* unqueue all pending requests */
      list = g_hash_table_get_keys (thumbnailer->handle_request_mapping);
      for (lp = list; lp != NULL; lp = lp->next)
        thunar_thumbnailer_unqueue_internal (thumbnailer, GPOINTER_TO_UINT (lp->data));
      g_list_free (list);
#endif

      g_hash_table_unref (thumbnailer->handle_request_mapping);
      g_hash_table_unref (thumbnailer->request_handle_mapping);
      g_hash_table_unref (thumbnailer->request_uris_mapping);

      /* disconnect from the thumbnailer proxy */
      g_signal_handlers_disconnect_matched (thumbnailer->thumbnailer_proxy,
                                            G_SIGNAL_MATCH_DATA, 0, 0, 
                                            NULL, NULL, thumbnailer);

      /* release the thumbnailer proxy */
      g_object_unref (thumbnailer->thumbnailer_proxy);
    }

  /* free the cached MIME types array */
  g_strfreev (thumbnailer->supported_types);

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  /* release the mutex */
  g_mutex_free (thumbnailer->lock);

  (*G_OBJECT_CLASS (thunar_thumbnailer_parent_class)->finalize) (object);
}



static gboolean
thunar_thumbnailer_file_is_supported (ThunarThumbnailer *thumbnailer,
                                      ThunarFile        *file)
{
  const gchar *content_type;
  gboolean     supported = FALSE;
  guint        n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  /* just assume all types are supported if we don't have a manager */
  if (thumbnailer->manager_proxy == NULL)
    {
      /* release the thumbnailer lock */
      g_mutex_unlock (thumbnailer->lock);
      return TRUE;
    }

  /* request the supported types on demand */
  if (thumbnailer->supported_types == NULL)
    {
      /* request the supported types from the manager D-Bus service. We only do
       * this once, so using a non-async call should be ok */
      thunar_thumbnailer_manager_proxy_get_supported (thumbnailer->manager_proxy,
                                                      &thumbnailer->supported_types, 
                                                      NULL);
    }

  /* check if we have supported types now */
  if (thumbnailer->supported_types != NULL)
    {
      /* determine the content type of the passed file */
      content_type = thunar_file_get_content_type (file);

      /* go through all the types */
      for (n = 0; !supported && thumbnailer->supported_types[n] != NULL; ++n)
        {
          /* check if the type of the file is a subtype of the supported type */
          if (g_content_type_is_a (content_type, thumbnailer->supported_types[n]))
            supported = TRUE;
        }
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  return supported;
}



static void
thunar_thumbnailer_thumbnailer_error (DBusGProxy        *proxy,
                                      guint              handle,
                                      const gchar      **uris,
                                      gint               code,
                                      const gchar       *message,
                                      ThunarThumbnailer *thumbnailer)
{
  ThunarThumbnailerIdle *idle;
  gpointer               request;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* look up the request ID for this D-Bus service handle */
  request = g_hash_table_lookup (thumbnailer->handle_request_mapping,
                                 GUINT_TO_POINTER (handle));

  /* check if we have a request for this handle */
  if (request != NULL)
    {
      /* allocate a new idle struct */
      idle = _thunar_slice_new0 (ThunarThumbnailerIdle);
      idle->type = THUNAR_THUMBNAILER_IDLE_ERROR;
      idle->thumbnailer = g_object_ref (thumbnailer);

      /* copy the URIs because we need them in the idle function */
      idle->data.uris = g_strdupv ((gchar **)uris);

      /* remember the idle struct because we might have to remove it in finalize() */
      thumbnailer->idles = g_list_prepend (thumbnailer->idles, idle);

      /* call the error idle function when we have the time */
      idle->id = g_idle_add_full (G_PRIORITY_LOW,
                                  thunar_thumbnailer_error_idle, idle,
                                  thunar_thumbnailer_idle_free);
    }
}



static void
thunar_thumbnailer_thumbnailer_finished (DBusGProxy        *proxy,
                                         guint              handle,
                                         ThunarThumbnailer *thumbnailer)
{
  gpointer request;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* look up the request ID for this D-Bus service handle */
  request = g_hash_table_lookup (thumbnailer->handle_request_mapping, 
                                 GUINT_TO_POINTER (handle));

  /* check if we have a request for this handle */
  if (request != NULL)
    {
      /* the request is finished, drop all the information about it */
      g_hash_table_remove (thumbnailer->handle_request_mapping, request);
      g_hash_table_remove (thumbnailer->request_handle_mapping, request);
      g_hash_table_remove (thumbnailer->request_uris_mapping, request);
    }
}



static void
thunar_thumbnailer_thumbnailer_ready (DBusGProxy        *proxy,
                                      const gchar      **uris,
                                      ThunarThumbnailer *thumbnailer)
{
  ThunarThumbnailerIdle *idle;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* check if we have any ready URIs */
  if (uris != NULL)
    {
      /* allocate a new idle struct */
      idle = _thunar_slice_new0 (ThunarThumbnailerIdle);
      idle->type = THUNAR_THUMBNAILER_IDLE_READY;
      idle->thumbnailer = g_object_ref (thumbnailer);

      /* copy the URI array because we need it in the idle function */
      idle->data.uris = g_strdupv ((gchar **)uris);

      /* remember the idle struct because we might have to remove it in finalize() */
      thumbnailer->idles = g_list_prepend (thumbnailer->idles, idle);

      /* call the ready idle function when we have the time */
      idle->id = g_idle_add_full (G_PRIORITY_LOW, 
                                 thunar_thumbnailer_ready_idle, idle, 
                                 thunar_thumbnailer_idle_free);
    }
}



static void
thunar_thumbnailer_thumbnailer_started (DBusGProxy        *proxy,
                                        guint              handle,
                                        ThunarThumbnailer *thumbnailer)
{
  ThunarThumbnailerIdle *idle;
  gpointer               request;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* look up the request for this D-Bus service handle */
  request = g_hash_table_lookup (thumbnailer->handle_request_mapping, 
                                 GUINT_TO_POINTER (handle));

  /* check if we have a request for this handle */
  if (request != NULL)
    {
      /* allocate a new idle struct */
      idle = _thunar_slice_new0 (ThunarThumbnailerIdle);
      idle->type = THUNAR_THUMBNAILER_IDLE_STARTED;
      idle->thumbnailer = g_object_ref (thumbnailer);

      /* remember the request because we need it in the idle function */
      idle->data.request = request;

      /* remember the idle struct because we might have to remove it in finalize() */
      thumbnailer->idles = g_list_prepend (thumbnailer->idles, idle);

      /* call the started idle function when we have the time */
      idle->id = g_idle_add_full (G_PRIORITY_LOW,
                                  thunar_thumbnailer_started_idle, idle, 
                                  thunar_thumbnailer_idle_free);
    }
}



static void
thunar_thumbnailer_queue_async_reply (DBusGProxy *proxy,
                                      guint       handle,
                                      GError     *error,
                                      gpointer    user_data)
{
  ThunarThumbnailerCall *call = user_data;
  ThunarThumbnailer     *thumbnailer = THUNAR_THUMBNAILER (call->thumbnailer);
  gchar                **uris;

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (call != NULL);
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  g_mutex_lock (thumbnailer->lock);

  if (error != NULL)
    {
      /* get the URIs array for this request */
      uris = g_hash_table_lookup (thumbnailer->request_uris_mapping, call->request);

      /* the array should always exist, otherwise there's a bug in the program */
      _thunar_assert (uris != NULL);

      /* the request is "finished", forget about its URIs */
      g_hash_table_remove (thumbnailer->request_uris_mapping, call->request);
    }
  else
    {
      /* remember that this request and D-Bus handle belong together */
      g_hash_table_insert (thumbnailer->request_handle_mapping,
                           call->request, GUINT_TO_POINTER (handle));
      g_hash_table_insert (thumbnailer->handle_request_mapping, 
                           GUINT_TO_POINTER (handle), call->request);
    }

  /* the queue call is finished, we can forget about its proxy call */
  g_hash_table_remove (thumbnailer->request_call_mapping, call->request);

  thunar_thumbnailer_call_free (call);

  g_mutex_unlock (thumbnailer->lock);
}



static gpointer
thunar_thumbnailer_queue_async (ThunarThumbnailer *thumbnailer,
                                const gchar      **uris,
                                const gchar      **mime_hints)
{
  ThunarThumbnailerCall *thumbnailer_call;
  DBusGProxyCall        *call;
  gpointer               request;
  guint                  request_no;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), 0);
  _thunar_return_val_if_fail (uris != NULL, 0);
  _thunar_return_val_if_fail (mime_hints != NULL, 0);
  _thunar_return_val_if_fail (DBUS_IS_G_PROXY (thumbnailer->thumbnailer_proxy), 0);

  /* compute the next request ID, making sure it's never 0 */
  request_no = GPOINTER_TO_UINT (thumbnailer->last_request) + 1;
  request_no = MAX (request_no, 1);
  
  /* remember the ID for the next request */
  thumbnailer->last_request = GUINT_TO_POINTER (request_no);

  /* use the new request ID for this request */
  request = thumbnailer->last_request;

  /* allocate a new call struct for the async D-Bus call */
  thumbnailer_call = _thunar_slice_new0 (ThunarThumbnailerCall);
  thumbnailer_call->request = request;
  thumbnailer_call->thumbnailer = g_object_ref (thumbnailer);

  /* queue thumbnails for the given URIs asynchronously */
  call = thunar_thumbnailer_proxy_queue_async (thumbnailer->thumbnailer_proxy,
                                               uris, mime_hints, 0, 
                                               thunar_thumbnailer_queue_async_reply,
                                               thumbnailer_call);

  /* remember to which request the call struct belongs */
  g_hash_table_insert (thumbnailer->request_call_mapping, request, call);

  /* return the request ID used for this request */
  return request;
}



static gboolean
thunar_thumbnailer_error_idle (gpointer user_data)
{
  ThunarThumbnailerIdle *idle = user_data;
  ThunarFile            *file;
  GFile                 *gfile;
  guint                  n;

  _thunar_return_val_if_fail (idle != NULL, FALSE);
  _thunar_return_val_if_fail (idle->type == THUNAR_THUMBNAILER_IDLE_ERROR, FALSE);

  /* iterate over all failed URIs */
  for (n = 0; idle->data.uris != NULL && idle->data.uris[n] != NULL; ++n)
    {
      /* look up the corresponding ThunarFile from the cache */
      gfile = g_file_new_for_uri (idle->data.uris[n]);
      file = thunar_file_cache_lookup (gfile);
      g_object_unref (gfile);

      /* check if we have a file for this URI in the cache */
      if (file != NULL)
        {
          /* set thumbnail state to none unless the thumbnail has already been created.
           * This is to prevent race conditions with the other idle functions */
          if (thunar_file_get_thumb_state (file) != THUNAR_FILE_THUMB_STATE_READY)
            thunar_file_set_thumb_state (file, THUNAR_FILE_THUMB_STATE_NONE);
        }
    }

  /* remove the idle struct */
  g_mutex_lock (idle->thumbnailer->lock);
  idle->thumbnailer->idles = g_list_remove (idle->thumbnailer->idles, idle);
  g_mutex_unlock (idle->thumbnailer->lock);

  /* remove the idle source, which also destroys the idle struct */
  return FALSE;
}



static gboolean
thunar_thumbnailer_ready_idle (gpointer user_data)
{
  ThunarThumbnailerIdle *idle = user_data;
  ThunarFile            *file;
  GFile                 *gfile;
  guint                  n;

  _thunar_return_val_if_fail (idle != NULL, FALSE);
  _thunar_return_val_if_fail (idle->type == THUNAR_THUMBNAILER_IDLE_READY, FALSE);

  /* iterate over all failed URIs */
  for (n = 0; idle->data.uris != NULL && idle->data.uris[n] != NULL; ++n)
    {
      /* look up the corresponding ThunarFile from the cache */
      gfile = g_file_new_for_uri (idle->data.uris[n]);
      file = thunar_file_cache_lookup (gfile);
      g_object_unref (gfile);

      /* check if we have a file for this URI in the cache */
      if (file != NULL)
        {
          /* set thumbnail state to ready - we now have a thumbnail */
          thunar_file_set_thumb_state (file, THUNAR_FILE_THUMB_STATE_READY);
        }
    }

  /* remove the idle struct */
  g_mutex_lock (idle->thumbnailer->lock);
  idle->thumbnailer->idles = g_list_remove (idle->thumbnailer->idles, idle);
  g_mutex_unlock (idle->thumbnailer->lock);

  /* remove the idle source, which also destroys the idle struct */
  return FALSE;
}



static gboolean
thunar_thumbnailer_started_idle (gpointer user_data)
{
  ThunarThumbnailerIdle *idle = user_data;
  const gchar          **uris;
  ThunarFile            *file;
  GFile                 *gfile;
  guint                  n;

  _thunar_return_val_if_fail (idle != NULL, FALSE);
  _thunar_return_val_if_fail (idle->type == THUNAR_THUMBNAILER_IDLE_STARTED, FALSE);

  g_mutex_lock (idle->thumbnailer->lock);

  /* look up the URIs that belong to this request */
  uris = g_hash_table_lookup (idle->thumbnailer->request_uris_mapping, 
                              idle->data.request);

  /* iterate over all URIs if there are any */
  for (n = 0; uris != NULL && uris[n] != NULL; ++n)
    {
      /* look up the corresponding ThunarFile from the cache */
      gfile = g_file_new_for_uri (uris[n]);
      file = thunar_file_cache_lookup (gfile);
      g_object_unref (gfile);

      /* check if we have a file in the cache */
      if (file != NULL)
        {
          /* set the thumbnail state to loading unless we already have a thumbnail.
           * This is to prevent race conditions with the other idle functions */
          if (thunar_file_get_thumb_state (file) != THUNAR_FILE_THUMB_STATE_READY)
            thunar_file_set_thumb_state (file, THUNAR_FILE_THUMB_STATE_LOADING);
        }
    }
  

  /* remove the idle struct */
  idle->thumbnailer->idles = g_list_remove (idle->thumbnailer->idles, idle);

  g_mutex_unlock (idle->thumbnailer->lock);

  /* remove the idle source, which also destroys the idle struct */
  return FALSE;
}



static void
thunar_thumbnailer_call_free (ThunarThumbnailerCall *call)
{
  _thunar_return_if_fail (call != NULL);

  /* drop the thumbnailer reference */
  g_object_unref (call->thumbnailer);

  /* free the struct */
  _thunar_slice_free (ThunarThumbnailerCall, call);
}



static void
thunar_thumbnailer_idle_free (gpointer data)
{
  ThunarThumbnailerIdle *idle = data;

  _thunar_return_if_fail (idle != NULL);

  /* free the URI array if necessary */
  if (idle->type == THUNAR_THUMBNAILER_IDLE_READY 
      || idle->type == THUNAR_THUMBNAILER_IDLE_ERROR)
    {
      g_strfreev (idle->data.uris);
    }

  /* drop the thumbnailer reference */
  g_object_unref (idle->thumbnailer);

  /* free the struct */
  _thunar_slice_free (ThunarThumbnailerIdle, idle);
}



/**
 * thunar_thumbnailer_new:
 *
 * Allocates a new #ThunarThumbnailer object, which can be used to 
 * generate and store thumbnails for files.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer needed.
 *
 * Return value: a newly allocated #ThunarThumbnailer.
 **/
ThunarThumbnailer*
thunar_thumbnailer_new (void)
{
  return g_object_new (THUNAR_TYPE_THUMBNAILER, NULL);
}



gboolean
thunar_thumbnailer_queue_file (ThunarThumbnailer *thumbnailer,
                               ThunarFile        *file)
{
  GList files;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* fake a file list */
  files.data = file;
  files.next = NULL;
  files.prev = NULL;

  /* queue a thumbnail request for the file */
  return thunar_thumbnailer_queue_files (thumbnailer, &files);
}



gboolean
thunar_thumbnailer_queue_files (ThunarThumbnailer *thumbnailer,
                                GList             *files)
{
  const gchar **mime_hints;
  gboolean      success = FALSE;
  gpointer      request;
  GList        *lp;
  GList        *supported_files = NULL;
  gchar       **uris;
  guint         n_supported = 0;
  guint         n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (files != NULL, FALSE);

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      g_mutex_unlock (thumbnailer->lock);

      /* collect all supported files from the list */
      for (lp = g_list_last (files); lp != NULL; lp = lp->prev)
        if (thunar_thumbnailer_file_is_supported (thumbnailer, lp->data))
          {
            supported_files = g_list_prepend (supported_files, lp->data);
            n_supported += 1;
          }

      g_mutex_lock (thumbnailer->lock);

      /* check if we have any supported files */
      if (supported_files != NULL)
        {
          /* allocate arrays for URIs and mime hints */
          uris = g_new0 (gchar *, n_supported + 1);
          mime_hints = g_new0 (const gchar *, n_supported + 1);

          /* fill arrays with data from the supported files */
          for (lp = supported_files, n = 0; lp != NULL; lp = lp->next, ++n)
            {
              uris[n] = thunar_file_dup_uri (lp->data);
              mime_hints[n] = thunar_file_get_content_type (lp->data);
            }

          /* NULL-terminate both arrays */
          uris[n] = NULL;
          mime_hints[n] = NULL;

          /* queue a thumbnail request for the supported files */
          request = thunar_thumbnailer_queue_async (thumbnailer, 
                                                    (const gchar **)uris,
                                                    mime_hints);

          /* remember the URIs for this request */
          g_hash_table_insert (thumbnailer->request_uris_mapping, request, uris);

          /* free mime hints array */
          g_free (mime_hints);

          /* free the list of supported files */
          g_list_free (supported_files);

          /* we assume success if we've come so far */
          success = TRUE;
        }
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  return success;
}



void
thunar_thumbnailer_unqueue (ThunarThumbnailer *thumbnailer,
                            gpointer           request)
{
  gpointer handle;

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      handle = g_hash_table_lookup (thumbnailer->request_handle_mapping, request);

      thunar_thumbnailer_proxy_unqueue (thumbnailer->thumbnailer_proxy, 
                                        GPOINTER_TO_UINT (handle), NULL);

      g_hash_table_remove (thumbnailer->handle_request_mapping, handle);
      g_hash_table_remove (thumbnailer->request_handle_mapping, request);
      g_hash_table_remove (thumbnailer->request_uris_mapping, request);
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);
}
