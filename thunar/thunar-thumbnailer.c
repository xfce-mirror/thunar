/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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
#include "config.h"
#endif

#include "thunar/thunar-marshal.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-thumbnailer-proxy.h"
#include "thunar/thunar-thumbnailer.h"

#include <libxfce4util/libxfce4util.h>



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
 * The proxy and the supported file types are intialized asynchronously, but
 * this is nearly completely transparent to the calling code because all requests
 * that arrive beforehand will be cached and sent out as soon as the initialization
 * is completed.
 *
 * When a request call is sent out, an internal request ID is created and tied
 * to the dbus call. While the call is running, an additional reference is held
 * to the thumbnailer so it cannot be finalized.
 *
 * The D-Bus reply handler then checks if there was an delivery error or
 * not. If the request method was sent successfully, the handle returned by the
 * D-Bus thumbnailer is associated bidirectionally with the internal request ID via
 * the request and handle values in the job structure.
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
 * from handle_request_mapping and request_handle_mapping.
 */



typedef enum
{
  THUNAR_THUMBNAILER_IDLE_ERROR,
  THUNAR_THUMBNAILER_IDLE_READY,
} ThunarThumbnailerIdleType;



typedef enum
{
  THUNAR_THUMBNAILER_PROXY_WAITING = 0,
  THUNAR_THUMBNAILER_PROXY_AVAILABLE,
  THUNAR_THUMBNAILER_PROXY_FAILED
} ThunarThumbnailerProxyState;



enum
{
  DBUS_THUMBNAIL_ERROR_UNSUPPORTED,
  DBUS_THUMBNAIL_ERROR_CONNECTION_ERROR,
  DBUS_THUMBNAIL_ERROR_INVALID_FORMAT,
  DBUS_THUMBNAIL_ERROR_IS_THUMBNAIL,
  DBUS_THUMBNAIL_ERROR_SAVE_FAILED,
  DBUS_THUMBNAIL_ERROR_UNSUPPORTED_FLAVOR,
};



typedef struct _ThunarThumbnailerJob ThunarThumbnailerJob;

/* Signal identifiers */
enum
{
  REQUEST_FINISHED,
  LAST_SIGNAL,
};

/* Property identifiers */
enum
{
  PROP_0,
  PROP_THUMBNAIL_SIZE,
  PROP_THUMBNAIL_MAX_FILE_SIZE,
};


static void
thunar_thumbnailer_finalize (GObject *object);
static gboolean
thunar_thumbnailer_init_thumbnailer_proxy (ThunarThumbnailer *thumbnailer);
static gboolean
thunar_thumbnailer_file_is_supported (ThunarThumbnailer *thumbnailer,
                                      ThunarFile        *file);
static void
thunar_thumbnailer_thumbnailer_finished (GDBusProxy        *proxy,
                                         guint              handle,
                                         ThunarThumbnailer *thumbnailer);
static void
thunar_thumbnailer_thumbnailer_error (GDBusProxy        *proxy,
                                      guint              handle,
                                      const gchar      **uris,
                                      gint               code,
                                      const gchar       *message,
                                      ThunarThumbnailer *thumbnailer);
static void
thunar_thumbnailer_thumbnailer_ready (GDBusProxy        *proxy,
                                      guint32            handle,
                                      const gchar      **uris,
                                      ThunarThumbnailer *thumbnailer);
static void
thunar_thumbnailer_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec);
static void
thunar_thumbnailer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec);

#define _thumbnailer_lock(thumbnailer) g_mutex_lock (&((thumbnailer)->lock))
#define _thumbnailer_unlock(thumbnailer) g_mutex_unlock (&((thumbnailer)->lock))


struct _ThunarThumbnailerClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailer
{
  GObject __parent__;

  /* proxies to communicate with D-Bus services */
  ThunarThumbnailerDBus      *thumbnailer_proxy;
  ThunarThumbnailerProxyState proxy_state;

  ThunarPreferences *preferences;

  /* running jobs */
  GSList *jobs;

  GMutex lock;

  /* cached MIME types -> URI schemes for which thumbs can be generated */
  GHashTable *supported;

  /* last ThunarThumbnailer request ID */
  guint last_request;

  /* size to use to store thumbnails */
  ThunarThumbnailSize thumbnail_size;

  /* maximum file size (in bytes) allowed to be thumbnailed */
  guint64 thumbnail_max_file_size;

  /* Agregated thumbnailing requests per size, to be queued on timeout */
  ThunarThumbnailerJob *jobs_to_queue[N_THUMBNAIL_SIZES];

  /* Id's of the timeout sources per size, used to agrregate requets */
  guint jobs_to_queue_source_id[N_THUMBNAIL_SIZES];
};

struct _ThunarThumbnailerJob
{
  ThunarThumbnailer *thumbnailer;

  /* if this job is cancelled */
  guint cancelled : 1;

  /* data is saved here in case the queueing is delayed */
  /* If this is NULL, the request has been sent off. */
  GList *files; /* element type: ThunarFile */

  /* request number returned by ThunarThumbnailer */
  guint request;

  /* handle returned by the tumbler dbus service */
  guint handle;

  /* used to override the thumbnail size of ThunarThumbnailer */
  ThunarThumbnailSize thumbnail_size;
};

static guint thumbnailer_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarThumbnailer, thunar_thumbnailer, G_TYPE_OBJECT);



static void
thunar_thumbnailer_class_init (ThunarThumbnailerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_thumbnailer_finalize;
  gobject_class->get_property = thunar_thumbnailer_get_property;
  gobject_class->set_property = thunar_thumbnailer_set_property;

  /**
   * ThunarThumbnailer:request-finished:
   * @thumbnailer : a #ThunarThumbnailer.
   * @request     : id of the request that is finished.
   *
   * Emitted by @thumbnailer, when a request is finished
   * by the thumbnail generator
   **/
  thumbnailer_signals[REQUEST_FINISHED] =
  g_signal_new (I_ ("request-finished"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__UINT,
                G_TYPE_NONE, 1, G_TYPE_UINT);

  /**
   * ThunarThumbnailer:thumbnail-size:
   *
   * Size of the thumbnails to load
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_SIZE,
                                   g_param_spec_enum ("thumbnail-size",
                                                      "thumbnail-size",
                                                      "thumbnail-size",
                                                      THUNAR_TYPE_THUMBNAIL_SIZE,
                                                      THUNAR_THUMBNAIL_SIZE_NORMAL,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarThumbnailer:thumbnail-max-file-size:
   *
   * Maximum file size (in bytes) allowed to be thumbnailed
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_THUMBNAIL_MAX_FILE_SIZE,
                                   g_param_spec_uint64 ("thumbnail-max-file-size",
                                                        "thumbnail-max-file-size",
                                                        "thumbnail-max-file-size",
                                                        0, G_MAXUINT64, 0,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_thumbnailer_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarThumbnailer *thumbnailer = THUNAR_THUMBNAILER (object);

  switch (prop_id)
    {
    case PROP_THUMBNAIL_SIZE:
      g_value_set_enum (value, thumbnailer->thumbnail_size);
      break;

    case PROP_THUMBNAIL_MAX_FILE_SIZE:
      g_value_set_uint64 (value, thumbnailer->thumbnail_max_file_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_thumbnailer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarThumbnailer *thumbnailer = THUNAR_THUMBNAILER (object);

  switch (prop_id)
    {
    case PROP_THUMBNAIL_SIZE:
      thumbnailer->thumbnail_size = g_value_get_enum (value);
      break;

    case PROP_THUMBNAIL_MAX_FILE_SIZE:
      thumbnailer->thumbnail_max_file_size = g_value_get_uint64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_thumbnailer_free_job (ThunarThumbnailerJob *job)
{
  if (job->files)
    g_list_free_full (job->files, g_object_unref);

  if (job->thumbnailer && job->thumbnailer->thumbnailer_proxy && job->handle)
    thunar_thumbnailer_dbus_call_dequeue (job->thumbnailer->thumbnailer_proxy, job->handle, NULL, NULL, NULL);

  g_slice_free (ThunarThumbnailerJob, job);
}



static void
thunar_thumbnailer_queue_async_reply (GObject      *proxy,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  ThunarThumbnailerJob *job = user_data;
  ThunarThumbnailer    *thumbnailer;
  GError               *error = NULL;
  guint                 handle;

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER_DBUS (proxy));
  _thunar_return_if_fail (job != NULL);

  thumbnailer = THUNAR_THUMBNAILER (job->thumbnailer);

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  _thumbnailer_lock (thumbnailer);

  thunar_thumbnailer_dbus_call_queue_finish (THUNAR_THUMBNAILER_DBUS (proxy), &handle, res, &error);

  if (job->cancelled)
    {
      /* job is cancelled while there was no handle jet, so dequeue it now */
      thunar_thumbnailer_dbus_call_dequeue (THUNAR_THUMBNAILER_DBUS (proxy), handle, NULL, NULL, NULL);

      /* cleanup */
      thumbnailer->jobs = g_slist_remove (thumbnailer->jobs, job);
      thunar_thumbnailer_free_job (job);
    }
  else if (error == NULL)
    {
      if (handle == 0)
        {
          g_warning ("ThunarThumbnailer: got 0 handle (Queue)");
        }
      else
        {
          /* store the handle returned by tumbler */
          job->handle = handle;
        }
    }
  else
    {
      g_warning ("ThunarThumbnailer: Queue failed: %s", error->message);
    }

  _thumbnailer_unlock (thumbnailer);
  g_clear_error (&error);

  /* remove the additional reference we held during the call */
  g_object_unref (thumbnailer);
}



/* NOTE: assumes that the lock is held by the caller */
static gboolean
thunar_thumbnailer_begin_job (ThunarThumbnailer    *thumbnailer,
                              ThunarThumbnailerJob *job)
{
  gboolean            success = FALSE;
  const gchar       **mime_hints;
  gchar             **uris;
  GList              *lp;
  GList              *supported_files = NULL;
  guint               n;
  guint               n_items = 0;
  const gchar        *thumbnail_path;
  ThunarThumbnailSize thumbnail_size;

  if (thumbnailer->proxy_state == THUNAR_THUMBNAILER_PROXY_WAITING)
    {
      /* all pending jobs will be queued automatically once the proxy is available */
      return TRUE;
    }
  else if (thumbnailer->proxy_state == THUNAR_THUMBNAILER_PROXY_FAILED)
    {
      /* give another chance to the proxy */
      g_warning ("Thumbnailer Proxy Failed ... starting attempt to re-initialize");

      if (thumbnailer->thumbnailer_proxy != NULL)
        {
          /* disconnect from the thumbnailer proxy */
          g_signal_handlers_disconnect_by_data (thumbnailer->thumbnailer_proxy, thumbnailer);
          thumbnailer->proxy_state = THUNAR_THUMBNAILER_PROXY_WAITING;
          g_object_unref (thumbnailer->thumbnailer_proxy);
          thumbnailer->thumbnailer_proxy = NULL;
        }

      /* Only try once a second, in order to prevent flooding in case the proxy always fails */
      g_timeout_add_seconds (1, G_SOURCE_FUNC (thunar_thumbnailer_init_thumbnailer_proxy), thumbnailer);

      return FALSE;
    }

  thumbnail_size = job->thumbnail_size == THUNAR_THUMBNAIL_SIZE_DEFAULT ? thumbnailer->thumbnail_size : job->thumbnail_size;

  /* collect all supported files from the list that are neither in the
   * about to be queued (wait queue), nor already queued, nor already
   * processed (and awaiting to be refreshed) */
  for (lp = job->files; lp != NULL; lp = lp->next)
    {
      /* the icon factory only loads icons for regular files, symlinks and folders */
      if (!thunar_file_is_regular (lp->data) && !thunar_file_is_directory (lp->data) && !thunar_file_is_symlink (lp->data))
        {
          thunar_file_update_thumbnail (lp->data, THUNAR_FILE_THUMB_STATE_NONE, thumbnail_size);
          continue;
        }
      if (thunar_thumbnailer_file_is_supported (thumbnailer, lp->data))
        {
          guint max_size = thumbnailer->thumbnail_max_file_size;

          /* skip large files */
          if (max_size > 0 && thunar_file_get_size (lp->data) > max_size)
            continue;

          supported_files = g_list_prepend (supported_files, g_object_ref (lp->data));
          n_items++;
        }
      else
        {
          /* still a regular file, but the type is now known to tumbler but
           * maybe the application created a thumbnail */
          thumbnail_path = thunar_file_get_thumbnail_path (lp->data, thumbnail_size);

          /* test if a thumbnail can be found */
          if (thumbnail_path != NULL && g_file_test (thumbnail_path, G_FILE_TEST_EXISTS))
            thunar_file_update_thumbnail (lp->data, THUNAR_FILE_THUMB_STATE_READY, thumbnail_size);
          else
            thunar_file_update_thumbnail (lp->data, THUNAR_FILE_THUMB_STATE_NONE, thumbnail_size);
        }
    }

  /* check if we have any supported files */
  if (n_items > 0)
    {
      /* allocate arrays for URIs and mime hints */
      uris = g_new0 (gchar *, n_items + 1);
      mime_hints = g_new0 (const gchar *, n_items + 1);

      /* fill URI and MIME hint arrays with items from the wait queue */
      for (lp = supported_files, n = 0; lp != NULL; lp = lp->next, ++n)
        {
          /* save URI and MIME hint in the arrays */
          uris[n] = thunar_file_dup_uri (lp->data);
          mime_hints[n] = thunar_file_get_content_type (lp->data);
        }

      /* NULL-terminate both arrays */
      uris[n] = NULL;
      mime_hints[n] = NULL;

      /* increase the reference count while the dbus call is running */
      g_object_ref (thumbnailer);

      /* queue the request - asynchronously, of course */
      thunar_thumbnailer_dbus_call_queue (thumbnailer->thumbnailer_proxy,
                                          (const gchar *const *) uris,
                                          (const gchar *const *) mime_hints,
                                          thunar_thumbnail_size_get_nick (thumbnail_size),
                                          "foreground", 0,
                                          NULL,
                                          thunar_thumbnailer_queue_async_reply,
                                          job);

      /* free mime hints array */
      g_free (mime_hints);
      g_strfreev (uris);

      /* free the list of files passed in */
      g_list_free_full (job->files, g_object_unref);
      job->files = NULL;

      /* we need to reload the files after the job is finished */
      job->files = supported_files;

      /* we assume success if we've come so far */
      success = TRUE;
    }

  return success;
}



static void
thunar_thumbnailer_init (ThunarThumbnailer *thumbnailer)
{
  g_mutex_init (&thumbnailer->lock);

  _thumbnailer_lock (thumbnailer);

  /* initialize the proxies */
  thunar_thumbnailer_init_thumbnailer_proxy (thumbnailer);

  _thumbnailer_unlock (thumbnailer);

  /* grab a reference on the preferences */
  thumbnailer->preferences = thunar_preferences_get ();

  for (gint i = 0; i < N_THUMBNAIL_SIZES; i++)
    {
      thumbnailer->jobs_to_queue[i] = NULL;
      thumbnailer->jobs_to_queue_source_id[i] = 0;
    }

  g_object_bind_property (G_OBJECT (thumbnailer->preferences),
                          "misc-thumbnail-max-file-size",
                          G_OBJECT (thumbnailer),
                          "thumbnail-max-file-size",
                          G_BINDING_SYNC_CREATE);
}



static void
thunar_thumbnailer_finalize (GObject *object)
{
  ThunarThumbnailer *thumbnailer = THUNAR_THUMBNAILER (object);

  /* acquire the thumbnailer lock */
  _thumbnailer_lock (thumbnailer);

  for (gint i = 0; i < N_THUMBNAIL_SIZES; i++)
    {
      if (thumbnailer->jobs_to_queue_source_id[i] != 0)
        {
          g_source_remove (thumbnailer->jobs_to_queue_source_id[i]);
          thunar_thumbnailer_free_job (thumbnailer->jobs_to_queue[i]);
          thumbnailer->jobs_to_queue[i] = NULL;
          thumbnailer->jobs_to_queue_source_id[i] = 0;
        }
    }

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      /* disconnect from the thumbnailer proxy */
      g_signal_handlers_disconnect_by_data (thumbnailer->thumbnailer_proxy, thumbnailer);
    }

  /* remove all jobs */
  g_slist_free_full (thumbnailer->jobs, (GDestroyNotify) thunar_thumbnailer_free_job);

  /* release the thumbnailer proxy */
  if (thumbnailer->thumbnailer_proxy != NULL)
    g_object_unref (thumbnailer->thumbnailer_proxy);

  /* free the cached URI schemes and MIME types table */
  if (thumbnailer->supported != NULL)
    g_hash_table_unref (thumbnailer->supported);

  /* release the thumbnailer lock */
  _thumbnailer_unlock (thumbnailer);

  /* release the mutex */
  g_mutex_clear (&thumbnailer->lock);

  /* release our reference on the preferences */
  g_object_unref (thumbnailer->preferences);

  (*G_OBJECT_CLASS (thunar_thumbnailer_parent_class)->finalize) (object);
}



static gint
thunar_thumbnailer_file_schemes_compare (gconstpointer a,
                                         gconstpointer b)
{
  const gchar *scheme_a = *(gconstpointer *) a;
  const gchar *scheme_b = *(gconstpointer *) b;

  /* sort file before other schemes */
  if (strcmp (scheme_a, "file") == 0)
    return -1;
  if (strcmp (scheme_b, "file") == 0)
    return 1;

  /* sort trash before other schemes */
  if (strcmp (scheme_a, "trash") == 0)
    return -1;
  if (strcmp (scheme_b, "trash") == 0)
    return 1;

  /* other order is just fine */
  return 0;
}



static void
thunar_thumbnailer_file_sort_schemes (gpointer mime_type,
                                      gpointer schemes,
                                      gpointer user_data)
{
  g_ptr_array_sort (schemes, thunar_thumbnailer_file_schemes_compare);
}



static void
thunar_thumbnailer_received_supported_types (ThunarThumbnailerDBus *proxy,
                                             GAsyncResult          *result,
                                             ThunarThumbnailer     *thumbnailer)
{
  guint      n;
  gchar    **schemes = NULL;
  gchar    **types = NULL;
  GPtrArray *schemes_array;
  GSList    *lp = NULL;
  GError    *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER_DBUS (proxy));

  _thumbnailer_lock (thumbnailer);


  if (!thunar_thumbnailer_dbus_call_get_supported_finish (proxy, &schemes, &types, result, &error))
    {
      g_slist_free_full (thumbnailer->jobs, (GDestroyNotify) thunar_thumbnailer_free_job);
      thumbnailer->jobs = NULL;

      g_warning ("ThunarThumbnailer: Failed to retrieve supported types: %s", error->message);
      g_clear_error (&error);

      thumbnailer->proxy_state = THUNAR_THUMBNAILER_PROXY_FAILED;

      if (thumbnailer->thumbnailer_proxy != NULL)
        {
          /* disconnect from the thumbnailer proxy */
          g_signal_handlers_disconnect_by_data (thumbnailer->thumbnailer_proxy, thumbnailer);

          g_object_unref (thumbnailer->thumbnailer_proxy);
          thumbnailer->thumbnailer_proxy = NULL;
        }

      _thumbnailer_unlock (thumbnailer);

      return;
    }

  if (G_LIKELY (schemes != NULL && types != NULL))
    {
      /* combine content types and uri schemes */
      for (n = 0; types[n] != NULL; ++n)
        {
          schemes_array = g_hash_table_lookup (thumbnailer->supported, types[n]);
          if (G_UNLIKELY (schemes_array == NULL))
            {
              /* create an array for the uri schemes this content type supports */
              schemes_array = g_ptr_array_new_with_free_func (g_free);
              g_ptr_array_add (schemes_array, schemes[n]);
              g_hash_table_insert (thumbnailer->supported, types[n], schemes_array);
            }
          else
            {
              /* add the uri scheme to the array of the content type */
              g_ptr_array_add (schemes_array, schemes[n]);

              /* cleanup */
              g_free (types[n]);
            }
        }

      /* remove arrays, we stole the values */
      g_free (types);
      g_free (schemes);

      /* sort array to optimize for local files */
      g_hash_table_foreach (thumbnailer->supported, thunar_thumbnailer_file_sort_schemes, NULL);
    }

  thumbnailer->proxy_state = THUNAR_THUMBNAILER_PROXY_AVAILABLE;

  /* now start delayed jobs */
  for (lp = thumbnailer->jobs; lp; lp = lp->next)
    {
      if (!thunar_thumbnailer_begin_job (thumbnailer, lp->data))
        {
          thunar_thumbnailer_free_job (lp->data);
          lp->data = NULL;
        }
    }
  thumbnailer->jobs = g_slist_remove_all (thumbnailer->jobs, NULL);

  g_clear_error (&error);

  _thumbnailer_unlock (thumbnailer);
}



static void
thunar_thumbnailer_proxy_created (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      userdata)
{
  ThunarThumbnailer *thumbnailer = THUNAR_THUMBNAILER (userdata);
  GError            *error = NULL;

  thumbnailer->thumbnailer_proxy = thunar_thumbnailer_dbus_proxy_new_finish (result, &error);

  if (thumbnailer->thumbnailer_proxy == NULL)
    {
      _thumbnailer_lock (thumbnailer);

      thumbnailer->proxy_state = THUNAR_THUMBNAILER_PROXY_FAILED;
      g_warning ("ThunarThumbnailer: failed to create proxy: %s", error->message);
      g_clear_error (&error);

      g_slist_free_full (thumbnailer->jobs, (GDestroyNotify) thunar_thumbnailer_free_job);
      thumbnailer->jobs = NULL;

      _thumbnailer_unlock (thumbnailer);

      return;
    }

  /* More detailed information about the Thumbnailer sigbnals can be found here */
  /* https://wiki.gnome.org/Attic/DraftThumbnailerSpec */

  /* 'error' is signaled when thumnailing failed for some uris */
  /* Note that 'finished' will still be signaled after the whole thumbnailing request finished */
  g_signal_connect (thumbnailer->thumbnailer_proxy, "error",
                    G_CALLBACK (thunar_thumbnailer_thumbnailer_error), thumbnailer);

  /* 'finished' is signaled after a thumbnailing request finished .. no matter if sucessfull or not */
  g_signal_connect (thumbnailer->thumbnailer_proxy, "finished",
                    G_CALLBACK (thunar_thumbnailer_thumbnailer_finished), thumbnailer);

  /* 'ready' is signaled when thumnailing was sucessfull for some uris */
  /* Note that 'finished' will still be signaled after the whole thumbnailing request finished */
  g_signal_connect (thumbnailer->thumbnailer_proxy, "ready",
                    G_CALLBACK (thunar_thumbnailer_thumbnailer_ready), thumbnailer);


  /* begin retrieving supported file types */

  _thumbnailer_lock (thumbnailer);

  /* prepare table */
  g_clear_pointer (&thumbnailer->supported, g_hash_table_unref);
  thumbnailer->supported = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) g_ptr_array_unref);

  /* request the supported types from the thumbnailer D-Bus service. */
  thunar_thumbnailer_dbus_call_get_supported (thumbnailer->thumbnailer_proxy, NULL,
                                              (GAsyncReadyCallback) thunar_thumbnailer_received_supported_types,
                                              thumbnailer);

  _thumbnailer_unlock (thumbnailer);
}



static gboolean
thunar_thumbnailer_init_thumbnailer_proxy (ThunarThumbnailer *thumbnailer)
{
  thumbnailer->thumbnailer_proxy = NULL;
  thumbnailer->proxy_state = THUNAR_THUMBNAILER_PROXY_WAITING;

  /* create the thumbnailer proxy */
  thunar_thumbnailer_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                             "org.freedesktop.thumbnails.Thumbnailer1",
                                             "/org/freedesktop/thumbnails/Thumbnailer1",
                                             NULL,
                                             (GAsyncReadyCallback) thunar_thumbnailer_proxy_created,
                                             thumbnailer);

  return G_SOURCE_REMOVE;
}



/* NOTE: assumes the lock is being held by the caller*/
static gboolean
thunar_thumbnailer_file_is_supported (ThunarThumbnailer *thumbnailer,
                                      ThunarFile        *file)
{
  gchar       *content_type;
  gboolean     supported = FALSE;
  guint        n;
  GPtrArray   *schemes_array;
  const gchar *scheme;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER_DBUS (thumbnailer->thumbnailer_proxy), FALSE);
  _thunar_return_val_if_fail (thumbnailer->supported != NULL, FALSE);

  /* determine the content type of the passed file */
  if (thunar_file_is_symlink (file))
    {
      GFile *link_target;

      link_target = thunar_g_file_resolve_symlink (thunar_file_get_file (file));
      if (link_target == NULL)
        return FALSE;

      content_type = thunar_g_file_get_content_type (link_target);
      g_object_unref (link_target);
    }
  else
    content_type = g_strdup (thunar_file_get_content_type (file));

  /* abort if the content type is unknown */
  if (content_type == NULL)
    return FALSE;

  /* lookup the content type, no difficult parent type matching here */
  schemes_array = g_hash_table_lookup (thumbnailer->supported, content_type);
  if (schemes_array != NULL)
    {
      /* go through all the URI schemes this type supports */
      for (n = 0; !supported && n < schemes_array->len; ++n)
        {
          /* check if the file has the current URI scheme */
          scheme = g_ptr_array_index (schemes_array, n);
          if (thunar_file_has_uri_scheme (file, scheme))
            supported = TRUE;
        }
    }

  g_free (content_type);
  return supported;
}



static void
thunar_thumbnailer_thumbnailer_error (GDBusProxy        *proxy,
                                      guint              handle,
                                      const gchar      **uris,
                                      gint               code,
                                      const gchar       *message,
                                      ThunarThumbnailer *thumbnailer)
{
  GFile                *gfile;
  ThunarFile           *file;
  ThunarThumbnailerJob *job;

  _thunar_return_if_fail (G_IS_DBUS_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  _thumbnailer_lock (thumbnailer);
  for (GSList *lp = thumbnailer->jobs; lp != NULL; lp = lp->next)
    {
      job = lp->data;
      if (job->handle == handle)
        {
          for (const gchar **uri = uris; *uri != NULL; ++uri)
            {
              /* look up the corresponding ThunarFile from the cache */
              gfile = g_file_new_for_uri (*uri);
              file = thunar_file_cache_lookup (gfile);
              g_object_unref (gfile);

              /* check if we have a file for this URI in the cache */
              if (file != NULL)
                {
                  if (code == DBUS_THUMBNAIL_ERROR_IS_THUMBNAIL)
                    {
                      /* the file itself is a thumbnail, so use it as a thumbnail */
                      thunar_file_set_is_thumbnail (file, TRUE);
                      thunar_file_update_thumbnail (file, THUNAR_FILE_THUMB_STATE_READY, job->thumbnail_size);
                    }
                  else
                    {
                      /* tell everybody we're done here */
                      thunar_file_update_thumbnail (file, THUNAR_FILE_THUMB_STATE_NONE, job->thumbnail_size);
                      g_debug ("Failed to generate thumbnail for '%s': Error Code: %i - Error: %s\n", thunar_file_get_basename (file), code, message);
                    }

                  g_object_unref (file);
                }
            }
          break;
        }
    }
  _thumbnailer_unlock (thumbnailer);
}



static void
thunar_thumbnailer_thumbnailer_ready (GDBusProxy        *proxy,
                                      guint32            handle,
                                      const gchar      **uris,
                                      ThunarThumbnailer *thumbnailer)
{
  GFile                *gfile;
  ThunarFile           *file;
  ThunarThumbnailerJob *job;

  _thunar_return_if_fail (G_IS_DBUS_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));


  _thumbnailer_lock (thumbnailer);
  for (GSList *lp = thumbnailer->jobs; lp != NULL; lp = lp->next)
    {
      job = lp->data;
      if (job->handle == handle)
        {
          for (const gchar **uri = uris; *uri != NULL; ++uri)
            {
              /* look up the corresponding ThunarFile from the cache */
              gfile = g_file_new_for_uri (*uri);
              file = thunar_file_cache_lookup (gfile);
              g_object_unref (gfile);

              if (file != NULL)
                {
                  thunar_file_update_thumbnail (file, THUNAR_FILE_THUMB_STATE_READY, job->thumbnail_size);
                  g_object_unref (file);
                }
            }
        }
    }
  _thumbnailer_unlock (thumbnailer);
}



static void
thunar_thumbnailer_thumbnailer_finished (GDBusProxy        *proxy,
                                         guint              handle,
                                         ThunarThumbnailer *thumbnailer)
{
  ThunarThumbnailerJob *job;
  GSList               *lp;

  _thunar_return_if_fail (G_IS_DBUS_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  if (handle == 0)
    {
      g_warning ("ThunarThumbnailer: got 0 handle (Finished)");
      return;
    }

  _thumbnailer_lock (thumbnailer);

  for (lp = thumbnailer->jobs; lp != NULL; lp = lp->next)
    {
      job = lp->data;

      if (job->handle == handle)
        {
          /* this job is finished, forget about the handle */
          job->handle = 0;

          /* tell everybody we're done here */
          g_signal_emit (G_OBJECT (thumbnailer), thumbnailer_signals[REQUEST_FINISHED], 0, job->request);

          /* remove job from the list */
          thumbnailer->jobs = g_slist_delete_link (thumbnailer->jobs, lp);

          thunar_thumbnailer_free_job (job);
          break;
        }
    }

  _thumbnailer_unlock (thumbnailer);
}



/**
 * thunar_thumbnailer_get:
 *
 * Return a shared new #ThunarThumbnailer object, which can be used to
 * generate and store thumbnails for files.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer needed.
 *
 * Return value: a #ThunarThumbnailer.
 **/
ThunarThumbnailer *
thunar_thumbnailer_get (void)
{
  static ThunarThumbnailer *thumbnailer = NULL;

  if (G_UNLIKELY (thumbnailer == NULL))
    {
      thumbnailer = g_object_new (THUNAR_TYPE_THUMBNAILER, NULL);
      g_object_add_weak_pointer (G_OBJECT (thumbnailer), (gpointer) &thumbnailer);
    }

  return g_object_ref_sink (thumbnailer);
}



static gboolean
thunar_thumbnailer_queue_job_after_timeout (gpointer user_data)
{
  ThunarThumbnailerJob *job = user_data;
  ThunarThumbnailer    *thumbnailer = THUNAR_THUMBNAILER (job->thumbnailer);
  ThunarThumbnailSize   thumbnail_size = job->thumbnail_size;
  gboolean              success = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), G_SOURCE_REMOVE);

  /* acquire the thumbnailer lock */
  _thumbnailer_lock (thumbnailer);

  success = thunar_thumbnailer_begin_job (thumbnailer, job);
  if (success)
    {
      thumbnailer->jobs = g_slist_prepend (thumbnailer->jobs, job);
    }
  else
    {
      for (GList *lp = job->files; lp != NULL; lp = lp->next)
        {
          /* This job failed .. inform all files which are waiting for the result */
          thunar_file_update_thumbnail (lp->data, THUNAR_FILE_THUMB_STATE_NONE, job->thumbnail_size);
        }

      /* tell everybody we're done for that job */
      g_signal_emit (G_OBJECT (thumbnailer), thumbnailer_signals[REQUEST_FINISHED], 0, job->request);

      /* and drop it */
      thunar_thumbnailer_free_job (job);
    }

  thumbnailer->jobs_to_queue[thumbnail_size] = NULL;
  thumbnailer->jobs_to_queue_source_id[thumbnail_size] = 0;

  /* release the lock */
  _thumbnailer_unlock (thumbnailer);

  return G_SOURCE_REMOVE;
}



void
thunar_thumbnailer_queue_file (ThunarThumbnailer  *thumbnailer,
                               ThunarFile         *file,
                               guint              *request,
                               ThunarThumbnailSize size)
{
  gint request_no;

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (thumbnailer->jobs_to_queue_source_id[size] == 0)
    {
      ThunarThumbnailerJob *job;

      /* allocate a job */
      job = g_slice_new0 (ThunarThumbnailerJob);
      job->thumbnailer = thumbnailer;
      job->files = g_list_append (job->files, g_object_ref (file));
      job->thumbnail_size = size;

      /* queue a thumbnail request for the URIs from the wait queue */
      /* compute the next request ID, making sure it's never 0 */
      request_no = thumbnailer->last_request + 1;
      request_no = MAX (request_no, 1);

      /* remember the ID for the next request */
      thumbnailer->last_request = request_no;

      /* save the request number */
      job->request = request_no;

      thumbnailer->jobs_to_queue[size] = job;
      thumbnailer->jobs_to_queue_source_id[size] = g_timeout_add (100, thunar_thumbnailer_queue_job_after_timeout, job);
    }
  else
    {
      if (thumbnailer->jobs_to_queue[size]->files == NULL)
        g_warn_if_reached ();

      thumbnailer->jobs_to_queue[size]->files = g_list_prepend (thumbnailer->jobs_to_queue[size]->files, g_object_ref (file));
    }

  *request = thumbnailer->jobs_to_queue[size]->request;
  return;
}



void
thunar_thumbnailer_dequeue (ThunarThumbnailer *thumbnailer,
                            guint              request)
{
  ThunarThumbnailerJob *job;
  GSList               *lp;

  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* acquire the thumbnailer lock */
  _thumbnailer_lock (thumbnailer);

  for (lp = thumbnailer->jobs; lp != NULL; lp = lp->next)
    {
      job = lp->data;

      /* find the request in the list */
      if (job->request == request)
        {
          /* this job is cancelled */
          job->cancelled = TRUE;

          if (job->handle != 0)
            {
              /* remove job */
              thumbnailer->jobs = g_slist_delete_link (thumbnailer->jobs, lp);

              thunar_thumbnailer_free_job (job);
            }

          break;
        }
    }

  /* release the thumbnailer lock */
  _thumbnailer_unlock (thumbnailer);
}
