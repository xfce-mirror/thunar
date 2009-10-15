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

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <thunar/thunar-thumbnailer-proxy.h>
#endif

#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnailer.h>



/**
 * WARNING: The source code in this file may do harm to animals. Dead kittens
 * are to be expected.
 *
 *
 * ThunarThumbnailer is a class for requesting thumbnails from org.xfce.tumbler.*
 * D-Bus services. This header contains an in-depth description of its design to make
 * it easier to understand why things are the way they are.
 *
 * Please note that all D-Bus calls are performed asynchronously. 
 *
 *
 * Queue
 * =====
 *
 * ThunarThumbnailer maintains a wait queue to group individual thumbnail requests.
 * The wait queue is processed at most every 100ms. This is done to reduce the
 * overall D-Bus noise when dealing with a lot of requests. The more thumbnails are 
 * requested in a 100ms time slot, the less D-Bus methods are sent.
 *
 * Let N be the number of requests made for individual files in a 100ms slot. 
 * Compared to sending out one requests per file (which generates 4*N D-Bus messages,
 * 1 Queue, 1 Started, 1 Ready/Error and 1 Finished for each of the N files), the wait 
 * queue technique only causes 3+N D-Bus messages to be sent (1 Queue, 1 Started, 
 * N Ready/Error and 1 Finished). This can be further improved on the service side
 * if the D-Bus thumbnailer groups Ready/Error messages (which of course has drawbacks
 * with regards to the overall thumbnailing responsiveness).
 *
 * Before a URI is added to the wait queue, it is checked whether it isn't already
 * 1) in the wait queue, 2) part of a pending or active request or 3) part of a
 * finished request which has an idle function pending.
 *
 * When a request call is finally sent out, an internal request ID is created and 
 * associated with the corresponding DBusGProxyCall via the request_call_mapping hash 
 * table. It also remembers the URIs for the internal request ID in the 
 * request_uris_mapping hash table. 
 *
 * The D-Bus reply handler then checks if there was an delivery error or
 * not. If the request method was sent successfully, the handle returned by the
 * D-Bus thumbnailer is associated bidirectionally with the internal request ID via 
 * the request_handle_mapping and handle_request_mappings. If the request could
 * not be sent at all, the URIs array is dropped from request_uris_mapping. In 
 * both cases, the association of the internal request ID with the DBusGProxyCall
 * is removed from request_call_mapping.
 *
 * These hash tables play a major role in the Started, Finished, Error and Ready
 * signal handlers.
 *
 *
 * Started
 * =======
 *
 * When a Started signal is emitted by the D-Bus thumbnailer, ThunarThumbnailer
 * receives the handle and looks up the corresponding internal request ID. If
 * it exists (which it should), it schedules an idle function to handle the
 * signal in the application's main loop. 
 *
 * The idle function then looks up the URIs array for the request ID from the
 * request_uris_mapping. For each of these URIs the corresponding ThunarFile
 * is looked up from the file cache (which represents files currently being
 * used somewhere in the UI), and if the ThunarFile exists in the cache it's
 * thumb state is set to _LOADING (unless it's already marked as _READY).
 *
 *
 * Ready / Error
 * =============
 *
 * The Ready and Error signal handlers work exactly like Started except that
 * the Ready idle function sets the thumb state of the corresponding 
 * ThunarFile objects to _READY and the Error signal sets the state to _NONE.
 *
 *
 * Finished
 * ========
 *
 * The Finished signal handler looks up the internal request ID based on
 * the D-Bus thumbnailer handle. It then drops all corresponding information
 * from handle_request_mapping, request_handle_mapping and request_uris_mapping.
 */



#if HAVE_DBUS
typedef enum
{
  THUNAR_THUMBNAILER_IDLE_ERROR,
  THUNAR_THUMBNAILER_IDLE_READY,
  THUNAR_THUMBNAILER_IDLE_STARTED,
} ThunarThumbnailerIdleType;



typedef struct _ThunarThumbnailerCall ThunarThumbnailerCall;
typedef struct _ThunarThumbnailerIdle ThunarThumbnailerIdle;
typedef struct _ThunarThumbnailerItem ThunarThumbnailerItem;
#endif



static void                   thunar_thumbnailer_finalize               (GObject               *object);
#ifdef HAVE_DBUS              
static void                   thunar_thumbnailer_init_thumbnailer_proxy (ThunarThumbnailer     *thumbnailer,
                                                                         DBusGConnection       *connection);
static gboolean               thunar_thumbnailer_file_is_supported      (ThunarThumbnailer     *thumbnailer,
                                                                         ThunarFile            *file);
static void                   thunar_thumbnailer_thumbnailer_finished   (DBusGProxy            *proxy,
                                                                         guint                  handle,
                                                                         ThunarThumbnailer     *thumbnailer);
static void                   thunar_thumbnailer_thumbnailer_error      (DBusGProxy            *proxy,
                                                                         guint                  handle,
                                                                         const gchar          **uris,
                                                                         gint                   code,
                                                                         const gchar           *message,
                                                                         ThunarThumbnailer     *thumbnailer);
static void                   thunar_thumbnailer_thumbnailer_ready      (DBusGProxy            *proxy,
                                                                         guint32                handle,
                                                                         const gchar          **uris,
                                                                         ThunarThumbnailer     *thumbnailer);
static void                   thunar_thumbnailer_thumbnailer_started    (DBusGProxy            *proxy,
                                                                         guint                  handle,
                                                                         ThunarThumbnailer     *thumbnailer);
static gpointer               thunar_thumbnailer_queue_async            (ThunarThumbnailer     *thumbnailer,
                                                                         gchar                **uris,
                                                                         const gchar          **mime_hints);
static gboolean               thunar_thumbnailer_error_idle             (gpointer               user_data);
static gboolean               thunar_thumbnailer_ready_idle             (gpointer               user_data);
static gboolean               thunar_thumbnailer_started_idle           (gpointer               user_data);
static void                   thunar_thumbnailer_call_free              (ThunarThumbnailerCall *call);
static void                   thunar_thumbnailer_idle_free              (gpointer               data);
static ThunarThumbnailerItem *thunar_thumbnailer_item_new               (GFile                 *file,
                                                                         const gchar           *mime_hint);
static void                   thunar_thumbnailer_item_free              (gpointer               data);
#endif



struct _ThunarThumbnailerClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailer
{
  GObject __parent__;

#ifdef HAVE_DBUS
  /* proxies to communicate with D-Bus services */
  DBusGProxy *thumbnailer_proxy;

  /* wait queue used to delay (and thereby group) thumbnail requests */
  GList      *wait_queue;
  guint       wait_queue_idle_id;

  /* hash table to map D-Bus service handles to ThunarThumbnailer requests */
  GHashTable *handle_request_mapping;

  /* hash table to map ThunarThumbnailer requests to D-Bus service handles */
  GHashTable *request_handle_mapping;

  /* hash table to map ThunarThumbnailer requests to DBusGProxyCalls */
  GHashTable *request_call_mapping;

  /* hash table to map ThunarThumbnailer requests to URI arrays */
  GHashTable *request_uris_mapping;

  GMutex     *lock;

  /* cached arrays of URI schemes and MIME types for which thumbnails 
   * can be generated */
  gchar     **supported_schemes;
  gchar     **supported_types;

  /* last ThunarThumbnailer request ID */
  gpointer    last_request;

  /* IDs of idle functions */
  GList      *idles;
#endif
};

#ifdef HAVE_DBUS
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

struct _ThunarThumbnailerItem
{
  GFile *file;
  gchar *mime_hint;
};
#endif



#ifdef HAVE_DBUS
static DBusGProxy *thunar_thumbnailer_proxy;
#endif



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
#ifdef HAVE_DBUS
  DBusGConnection *connection;

  thumbnailer->lock = g_mutex_new ();
  thumbnailer->supported_schemes = NULL;
  thumbnailer->supported_types = NULL;
  thumbnailer->last_request = GUINT_TO_POINTER (0);
  thumbnailer->idles = NULL;
  thumbnailer->wait_queue_idle_id = 0;

  /* try to connect to D-Bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  /* initialize the proxies */
  thunar_thumbnailer_init_thumbnailer_proxy (thumbnailer, connection);

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
#endif
}



static void
thunar_thumbnailer_finalize (GObject *object)
{
#ifdef HAVE_DBUS
  ThunarThumbnailerIdle *idle;
  ThunarThumbnailer     *thumbnailer = THUNAR_THUMBNAILER (object);
  GList                 *list;
  GList                 *lp;

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  /* clear the request queue */
  g_list_foreach (thumbnailer->wait_queue, (GFunc) thunar_thumbnailer_item_free, NULL);
  g_list_free (thumbnailer->wait_queue);

  /* remove the request queue processing idle handler */
  if (thumbnailer->wait_queue_idle_id > 0)
    g_source_remove (thumbnailer->wait_queue_idle_id);

  /* abort all pending idle functions */
  for (lp = thumbnailer->idles; lp != NULL; lp = lp->next)
    {
      idle = lp->data;
      g_source_remove (idle->id);
    }

  /* free the idle list */
  g_list_free (thumbnailer->idles);

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

  /* free the cached URI schemes and MIME types array */
  g_strfreev (thumbnailer->supported_schemes);
  g_strfreev (thumbnailer->supported_types);

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  /* release the mutex */
  g_mutex_free (thumbnailer->lock);
#endif

  (*G_OBJECT_CLASS (thunar_thumbnailer_parent_class)->finalize) (object);
}



#ifdef HAVE_DBUS
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
                                   "org.freedesktop.thumbnails.Thumbnailer1",
                                   "/org/freedesktop/thumbnails/Thumbnailer1",
                                   "org.freedesktop.thumbnails.Thumbnailer1");

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

      dbus_g_object_register_marshaller ((GClosureMarshal) g_cclosure_marshal_VOID__UINT_POINTER,
                                         G_TYPE_NONE,
                                         G_TYPE_UINT,
                                         G_TYPE_STRV,
                                         G_TYPE_INVALID);

      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Error", 
                               G_TYPE_UINT, G_TYPE_STRV, G_TYPE_UINT, G_TYPE_STRING, 
                               G_TYPE_INVALID);
      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Finished", 
                               G_TYPE_UINT, G_TYPE_INVALID);
      dbus_g_proxy_add_signal (thumbnailer->thumbnailer_proxy, "Ready", 
                               G_TYPE_UINT, G_TYPE_STRV, G_TYPE_INVALID);
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

  /* no types are supported if we don't have a thumbnailer */
  if (thumbnailer->thumbnailer_proxy == NULL)
    {
      /* release the thumbnailer lock */
      g_mutex_unlock (thumbnailer->lock);
      return FALSE;
    }

  /* request the supported types on demand */
  if (thumbnailer->supported_schemes == NULL 
      || thumbnailer->supported_types == NULL)
    {
      /* request the supported types from the thumbnailer D-Bus service. We only do
       * this once, so using a non-async call should be ok */
      thunar_thumbnailer_proxy_get_supported (thumbnailer->thumbnailer_proxy,
                                              &thumbnailer->supported_schemes,
                                              &thumbnailer->supported_types, 
                                              NULL);
    }

  /* check if we have supported URI schemes and MIME types now */
  if (thumbnailer->supported_schemes != NULL
      && thumbnailer->supported_types != NULL)
    {
      /* determine the content type of the passed file */
      content_type = thunar_file_get_content_type (file);

      /* go through all the URI schemes we support */
      for (n = 0; !supported && thumbnailer->supported_schemes[n] != NULL; ++n)
        {
          /* check if the file has the current URI scheme */
          if (thunar_file_has_uri_scheme (file, thumbnailer->supported_schemes[n]))
            {
              /* check if the type of the file is a subtype of the supported type */
              if (g_content_type_is_a (content_type, thumbnailer->supported_types[n]))
                supported = TRUE;
            }
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
      idle = g_slice_new0 (ThunarThumbnailerIdle);
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
                                      guint32            handle,
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
      idle = g_slice_new0 (ThunarThumbnailerIdle);
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
      idle = g_slice_new0 (ThunarThumbnailerIdle);
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
#ifndef NDEBUG
  gchar                **uris;
#endif

  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (call != NULL);
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  g_mutex_lock (thumbnailer->lock);

  if (error != NULL)
    {
#ifndef NDEBUG
      /* get the URIs array for this request */
      uris = g_hash_table_lookup (thumbnailer->request_uris_mapping, call->request);

      /* the array should always exist, otherwise there's a bug in the program */
      _thunar_assert (uris != NULL);
#endif

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
                                gchar            **uris,
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
  thumbnailer_call = g_slice_new0 (ThunarThumbnailerCall);
  thumbnailer_call->request = request;
  thumbnailer_call->thumbnailer = g_object_ref (thumbnailer);

  /* remember the URIs for this request */
  g_hash_table_insert (thumbnailer->request_uris_mapping, request, uris);

  /* queue thumbnails for the given URIs asynchronously */
  call = thunar_thumbnailer_proxy_queue_async (thumbnailer->thumbnailer_proxy,
                                               (const gchar **)uris, mime_hints, 
                                               "default", 0, 
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



static gboolean
thunar_thumbnailer_file_is_in_wait_queue (ThunarThumbnailer *thumbnailer,
                                          ThunarFile        *file)
{
  ThunarThumbnailerItem *item;
  gboolean               in_wait_queue = FALSE;
  GList                 *lp;

  g_mutex_lock (thumbnailer->lock);

  for (lp = thumbnailer->wait_queue; !in_wait_queue && lp != NULL; lp = lp->next)
    {
      item = lp->data;

      if (g_file_equal (item->file, thunar_file_get_file (file)))
        in_wait_queue = TRUE;
    }

  g_mutex_unlock (thumbnailer->lock);

  return in_wait_queue;
}



static gboolean
thunar_thumbnailer_process_wait_queue (ThunarThumbnailer *thumbnailer)
{
  ThunarThumbnailerItem *item;
  gpointer               request;
  GList                 *lp;
  gchar                **mime_hints;
  gchar                **uris;
  guint                  n_items;
  guint                  n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);

  g_mutex_lock (thumbnailer->lock);

  /* determine how many URIs are in the wait queue */
  n_items = g_list_length (thumbnailer->wait_queue);

  /* allocate arrays for URIs and mime hints */
  uris = g_new0 (gchar *, n_items + 1);
  mime_hints = g_new0 (gchar *, n_items + 1);

  /* fill URI and MIME hint arrays with items from the wait queue */
  for (lp = g_list_last (thumbnailer->wait_queue), n = 0; lp != NULL; lp = lp->prev, ++n)
    {
      /* fetch the next item from the queue */
      item = lp->data;

      /* save URI and MIME hint in the arrays */
      uris[n] = g_file_get_uri (item->file);
      mime_hints[n] = item->mime_hint;

      /* destroy the GFile and the queue item. The MIME hints are free'd later */
      g_object_unref (item->file);
      g_slice_free (ThunarThumbnailerItem, item);
    }

  /* NULL-terminate both arrays */
  uris[n] = NULL;
  mime_hints[n] = NULL;

  /* queue a thumbnail request for the URIs from the wait queue */
  request = thunar_thumbnailer_queue_async (thumbnailer, uris, 
                                            (const gchar **)mime_hints);

  /* free mime hints array */
  g_strfreev (mime_hints);

  /* clear the wait queue */
  g_list_free (thumbnailer->wait_queue);
  thumbnailer->wait_queue = NULL;

  /* reset the wait queue idle ID */
  thumbnailer->wait_queue_idle_id = 0;

  g_mutex_unlock (thumbnailer->lock);

  return FALSE;
}



static gboolean
thunar_thumbnailer_file_is_queued (ThunarThumbnailer *thumbnailer,
                                   ThunarFile        *file)
{
  gboolean is_queued = FALSE;
  GList   *values;
  GList   *lp;
  gchar  **uris;
  gchar   *uri;
  guint    n;

  /* get a list with all URI arrays of already queued requests */
  values = g_hash_table_get_values (thumbnailer->request_uris_mapping);

  /* if we have none, the file cannot be queued ... or can it? ;) */
  if (values == NULL)
    return FALSE;

  /* determine the URI for this file */
  uri = thunar_file_dup_uri (file);

  /* iterate over all URI arrays */
  for (lp = values; !is_queued && lp != NULL; lp = lp->next)
    {
      uris = lp->data;

      /* check if the file is included in the URI array of the current request */
      for (n = 0; !is_queued && uris != NULL && uris[n] != NULL; ++n)
        if (g_utf8_collate (uri, uris[n]) == 0)
          is_queued = TRUE;
    }

  /* free the file URI */
  g_free (uri);

  /* free the URI array list */
  g_list_free (values);

  return is_queued;
}



static gboolean
thunar_thumbnailer_file_is_ready (ThunarThumbnailer *thumbnailer,
                                  ThunarFile        *file)
{
  ThunarThumbnailerIdle *idle;
  gboolean               is_ready = FALSE;
  GList                 *lp;
  gchar                 *uri;
  guint                  n;

  /* determine the URI or this file */
  uri = thunar_file_dup_uri (file);

  /* iterate over all idle structs */
  for (lp = thumbnailer->idles; !is_ready && lp != NULL; lp = lp->next)
    {
      /* skip invalid idles */
      if (lp->data != NULL)
        continue;

      idle = lp->data;

      /* skip non-ready idles and idles without any URIs */
      if (idle->type != THUNAR_THUMBNAILER_IDLE_READY || idle->data.uris == NULL)
        continue;

      /* check if the file is included in this ready idle */
      for (n = 0; !is_ready && idle->data.uris[n] != NULL; ++n)
        if (g_utf8_collate (uri, idle->data.uris[n]) == 0)
          is_ready = TRUE;
    }

  /* free the file URI */
  g_free (uri);

  return is_ready;
}



static void
thunar_thumbnailer_call_free (ThunarThumbnailerCall *call)
{
  _thunar_return_if_fail (call != NULL);

  /* drop the thumbnailer reference */
  g_object_unref (call->thumbnailer);

  /* free the struct */
  g_slice_free (ThunarThumbnailerCall, call);
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
  g_slice_free (ThunarThumbnailerIdle, idle);
}



static ThunarThumbnailerItem *
thunar_thumbnailer_item_new (GFile       *file,
                             const gchar *mime_hint)
{
  ThunarThumbnailerItem *item;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (mime_hint != NULL && mime_hint != '\0', NULL);

  item = g_slice_new0 (ThunarThumbnailerItem);
  item->file = g_object_ref (file);
  item->mime_hint = g_strdup (mime_hint);

  return item;
}


static void
thunar_thumbnailer_item_free (gpointer data)
{
  ThunarThumbnailerItem *item = data;

  g_object_unref (item->file);
  g_free (item->mime_hint);
  g_slice_free (ThunarThumbnailerItem, item);
}
#endif /* HAVE_DBUS */



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
#ifdef HAVE_DBUS
  ThunarThumbnailerItem *item;
#endif
  gboolean               success = FALSE;
#ifdef HAVE_DBUS
  GList                 *lp;
  GList                 *supported_files = NULL;
#endif

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (files != NULL, FALSE);

#ifdef HAVE_DBUS
  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy == NULL)
    {
      /* release the lock and abort */
      g_mutex_unlock (thumbnailer->lock);
      return FALSE;
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  /* collect all supported files from the list that are neither in the
   * about to be queued (wait queue), nor already queued, nor already 
   * processed (and awaiting to be refreshed) */
  for (lp = g_list_last (files); lp != NULL; lp = lp->prev)
    if (thunar_thumbnailer_file_is_supported (thumbnailer, lp->data))
      {
        if (!thunar_thumbnailer_file_is_in_wait_queue (thumbnailer, lp->data)
            && !thunar_thumbnailer_file_is_queued (thumbnailer, lp->data)
            && !thunar_thumbnailer_file_is_ready (thumbnailer, lp->data))
          {
            supported_files = g_list_prepend (supported_files, lp->data);
          }
      }

  /* check if we have any supported files */
  if (supported_files != NULL)
    {
      for (lp = supported_files; lp != NULL; lp = lp->next)
        {
          g_mutex_lock (thumbnailer->lock);

          /* allocate a thumbnailer item for the wait queue */
          item = thunar_thumbnailer_item_new (thunar_file_get_file (lp->data),
                                              thunar_file_get_content_type (lp->data));

          /* add the item to the wait queue */
          thumbnailer->wait_queue = g_list_prepend (thumbnailer->wait_queue, item);

          g_mutex_unlock (thumbnailer->lock);
        }

      g_mutex_lock (thumbnailer->lock);

      if (thumbnailer->wait_queue_idle_id == 0)
        {
          thumbnailer->wait_queue_idle_id = 
            g_timeout_add (100, (GSourceFunc) thunar_thumbnailer_process_wait_queue, 
                           thumbnailer);
        }

      g_mutex_unlock (thumbnailer->lock);
      
      /* free the list of supported files */
      g_list_free (supported_files);

      /* we assume success if we've come so far */
      success = TRUE;
    }
#endif /* HAVE_DBUS */

  return success;
}


#if 0 
static void
thunar_thumbnailer_unqueue (ThunarThumbnailer *thumbnailer,
                            gpointer           request)
{
#ifdef HAVE_DBUS
  gpointer handle;
#endif

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

#ifdef HAVE_DBUS
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
#endif
}
#endif
