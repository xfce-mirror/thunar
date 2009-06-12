/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnailer.h>
#include <thunar/thunar-thumbnailer-proxy.h>



/* signal identifiers */
enum
{
  SIGNAL_ERROR,
  SIGNAL_FINISHED,
  SIGNAL_READY,
  SIGNAL_STARTED,
  LAST_SIGNAL,
};



static void thunar_thumbnailer_finalize             (GObject           *object);
static void thunar_thumbnailer_thumbnailer_error    (DBusGProxy        *proxy,
                                                     guint              handle,
                                                     const gchar      **uris,
                                                     gint               code,
                                                     const gchar       *message,
                                                     ThunarThumbnailer *thumbnailer);
static void thunar_thumbnailer_thumbnailer_finished (DBusGProxy        *proxy,
                                                     guint              handle,
                                                     ThunarThumbnailer *thumbnailer);
static void thunar_thumbnailer_thumbnailer_ready    (DBusGProxy        *proxy,
                                                     const gchar      **uris,
                                                     ThunarThumbnailer *thumbnailer);
static void thunar_thumbnailer_thumbnailer_started  (DBusGProxy        *proxy,
                                                     guint              handle,
                                                     ThunarThumbnailer *thumbnailer);



struct _ThunarThumbnailerClass
{
  GObjectClass __parent__;
};

struct _ThunarThumbnailer
{
  GObject __parent__;

  DBusGProxy *thumbnailer_proxy;
  GMutex     *lock;
  GList      *requests;
};



static DBusGProxy *thunar_thumbnailer_proxy;
static guint       thumbnailer_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarThumbnailer, thunar_thumbnailer, G_TYPE_OBJECT);



static void
thunar_thumbnailer_class_init (ThunarThumbnailerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_thumbnailer_finalize;

  /* TODO this should actually be VOID:UINT,POINTER,INT,STRING */
  thumbnailer_signals[SIGNAL_ERROR] =
    g_signal_new ("error",
                  THUNAR_TYPE_THUMBNAILER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _thunar_marshal_VOID__UINT_POINTER_UINT_STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT,
                  G_TYPE_STRV,
                  G_TYPE_UINT,
                  G_TYPE_STRING);

  thumbnailer_signals[SIGNAL_FINISHED] =
    g_signal_new ("finished",
                  THUNAR_TYPE_THUMBNAILER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT);

  thumbnailer_signals[SIGNAL_READY] =
    g_signal_new ("ready",
                  THUNAR_TYPE_THUMBNAILER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRV);

  thumbnailer_signals[SIGNAL_STARTED] =
    g_signal_new ("started", 
                  THUNAR_TYPE_THUMBNAILER,
                  G_SIGNAL_RUN_LAST, 
                  0, 
                  NULL, NULL, 
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 
                  1, 
                  G_TYPE_UINT);
}



static void
thunar_thumbnailer_init (ThunarThumbnailer *thumbnailer)
{
  DBusGConnection *connection;

  thumbnailer->lock = g_mutex_new ();

  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  if (connection != NULL)
    {
      if (thunar_thumbnailer_proxy == NULL)
        {
          thunar_thumbnailer_proxy = 
            dbus_g_proxy_new_for_name (connection, 
                                       "org.freedesktop.thumbnails.Thumbnailer",
                                       "/org/freedesktop/thumbnails/Thumbnailer",
                                       "org.freedesktop.thumbnails.Thumbnailer");

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

      dbus_g_connection_unref (connection);
    }
}



static void
thunar_thumbnailer_finalize (GObject *object)
{
  ThunarThumbnailer *thumbnailer = THUNAR_THUMBNAILER (object);

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  /* release the thumbnail factory */
  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      g_signal_handlers_disconnect_matched (thumbnailer->thumbnailer_proxy,
                                            G_SIGNAL_MATCH_DATA, 0, 0, 
                                            NULL, NULL, thumbnailer);
      g_object_unref (thumbnailer->thumbnailer_proxy);
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  /* release the mutex */
  g_mutex_free (thumbnailer->lock);

  (*G_OBJECT_CLASS (thunar_thumbnailer_parent_class)->finalize) (object);
}



static void
thunar_thumbnailer_thumbnailer_error (DBusGProxy        *proxy,
                                      guint              handle,
                                      const gchar      **uris,
                                      gint               code,
                                      const gchar       *message,
                                      ThunarThumbnailer *thumbnailer)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (uris != NULL);
  _thunar_return_if_fail (message != NULL);
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));
  
  g_debug ("error");

  g_signal_emit (thumbnailer, thumbnailer_signals[SIGNAL_ERROR], 0,
                 handle, uris, code, message);
}



static void
thunar_thumbnailer_thumbnailer_finished (DBusGProxy        *proxy,
                                         guint              handle,
                                         ThunarThumbnailer *thumbnailer)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  g_signal_emit (thumbnailer, thumbnailer_signals[SIGNAL_FINISHED], 0, handle);
}



static void
thunar_thumbnailer_thumbnailer_ready (DBusGProxy        *proxy,
                                      const gchar      **uris,
                                      ThunarThumbnailer *thumbnailer)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (uris != NULL);
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));
  
  g_signal_emit (thumbnailer, thumbnailer_signals[SIGNAL_READY], 0, uris);
}



static void
thunar_thumbnailer_thumbnailer_started (DBusGProxy        *proxy,
                                        guint              handle,
                                        ThunarThumbnailer *thumbnailer)
{
  _thunar_return_if_fail (DBUS_IS_G_PROXY (proxy));
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  g_signal_emit (thumbnailer, thumbnailer_signals[SIGNAL_STARTED], 0, handle);
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
                               ThunarFile        *file,
                               guint             *handle)
{
  const gchar *mime_hints[2] = { NULL, NULL };
  gboolean     success = FALSE;
  gchar       *uris[2] = { NULL, NULL };

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (handle != NULL, FALSE);

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      uris[0] = thunar_file_dup_uri (file);
      mime_hints[0] = thunar_file_get_content_type (file);

      success =thunar_thumbnailer_proxy_queue (thumbnailer->thumbnailer_proxy,
                                               (const gchar **)uris, 
                                               mime_hints, 0, handle, NULL);

      g_free (uris[0]);
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  return success;
}



gboolean
thunar_thumbnailer_queue_files (ThunarThumbnailer *thumbnailer,
                                GList             *files,
                                guint             *handle)
{
  const gchar **mime_hints;
  gboolean      success = FALSE;
  GList        *lp;
  gchar       **uris;
  guint         n;

  _thunar_return_val_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer), FALSE);
  _thunar_return_val_if_fail (files != NULL, FALSE);
  _thunar_return_val_if_fail (handle != NULL, FALSE);

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy != NULL)
    {
      uris = g_new0 (gchar *, g_list_length (files) + 1);
      mime_hints = g_new0 (const gchar *, g_list_length (files) + 1);

      for (lp = g_list_last (files), n = 0; lp != NULL; lp = lp->prev, ++n)
        {
          uris[n] = thunar_file_dup_uri (lp->data);
          mime_hints[n] = thunar_file_get_content_type (lp->data);
        }

      uris[n] = NULL;
      mime_hints[n] = NULL;
          
      success = thunar_thumbnailer_proxy_queue (thumbnailer->thumbnailer_proxy, 
                                                (const gchar **)uris, mime_hints, 
                                                0, handle, NULL);

      g_strfreev (uris);
      g_free (mime_hints);
    }

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);

  return success;
}



void
thunar_thumbnailer_unqueue (ThunarThumbnailer *thumbnailer,
                            guint              handle)
{
  _thunar_return_if_fail (THUNAR_IS_THUMBNAILER (thumbnailer));

  /* acquire the thumbnailer lock */
  g_mutex_lock (thumbnailer->lock);

  if (thumbnailer->thumbnailer_proxy != NULL)
    thunar_thumbnailer_proxy_unqueue (thumbnailer->thumbnailer_proxy, handle, NULL);

  /* release the thumbnailer lock */
  g_mutex_unlock (thumbnailer->lock);
}
