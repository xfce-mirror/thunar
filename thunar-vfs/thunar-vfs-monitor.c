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

#ifdef HAVE_FAM_H
#include <fam.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdk.h>

#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void     thunar_vfs_monitor_class_init (ThunarVfsMonitorClass *klass);
static void     thunar_vfs_monitor_init       (ThunarVfsMonitor      *monitor);
static void     thunar_vfs_monitor_finalize   (GObject               *object);
#ifdef HAVE_LIBFAM
static void     thunar_vfs_monitor_fam_cancel (ThunarVfsMonitor      *monitor);
static void     thunar_vfs_monitor_fam_event  (ThunarVfsMonitor      *monitor,
                                               const FAMEvent        *fe);
static gboolean thunar_vfs_monitor_fam_watch (GIOChannel             *channel,
                                              GIOCondition            condition,
                                              gpointer                user_data);
#endif



struct _ThunarVfsMonitorClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsMonitor
{
  GObject __parent__;

  GMemChunk    *handle_chunk;
  GList        *handles;

  gint          reentrant_level;
  GList        *reentrant_removals;

#ifdef HAVE_LIBFAM
  FAMConnection fc;
  gint          fc_watch_id;
  gint          fc_sequence;
#endif
};

struct _ThunarVfsMonitorHandle
{
  ThunarVfsMonitorCallback callback;
  gpointer                 user_data;
  ThunarVfsURI            *uri;
  gboolean                 directory;

#ifdef HAVE_LIBFAM
  FAMRequest               fr;
#endif
};



G_DEFINE_TYPE (ThunarVfsMonitor, thunar_vfs_monitor, G_TYPE_OBJECT);



static void
thunar_vfs_monitor_class_init (ThunarVfsMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_monitor_finalize;
}



static void
thunar_vfs_monitor_init (ThunarVfsMonitor *monitor)
{
#ifdef HAVE_LIBFAM
  if (FAMOpen2 (&monitor->fc, PACKAGE_NAME) == 0)
    {
      GIOChannel *channel;

      channel = g_io_channel_unix_new (FAMCONNECTION_GETFD (&monitor->fc));
      monitor->fc_watch_id = g_io_add_watch (channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
                                             thunar_vfs_monitor_fam_watch, monitor);
      g_io_channel_unref (channel);

#ifdef HAVE_FAMNOEXISTS
      /* luckily gamin offers a way to avoid the FAMExists events */
      FAMNoExists (&monitor->fc);
#endif
    }
  else
    {
      monitor->fc_watch_id = -1;
    }
#endif

  /* allocate the memory chunk for the handles */
  monitor->handle_chunk = g_mem_chunk_create (ThunarVfsMonitorHandle, 64, G_ALLOC_AND_FREE);
}



static void
thunar_vfs_monitor_finalize (GObject *object)
{
  ThunarVfsMonitor *monitor = THUNAR_VFS_MONITOR (object);
  GList            *lp;

#ifdef HAVE_LIBFAM
  if (monitor->fc_watch_id >= 0)
    thunar_vfs_monitor_fam_cancel (monitor);
#endif

  /* we cannot get here if there's still one pending feed call */
  g_assert (monitor->reentrant_level == 0);
  g_assert (monitor->reentrant_removals == NULL);

  /* drop all handles */
  for (lp = monitor->handles; lp != NULL; lp = lp->next)
    thunar_vfs_uri_unref (((ThunarVfsMonitorHandle *) lp->data)->uri);
  g_list_free (monitor->handles);

  /* release the memory chunk */
  g_mem_chunk_destroy (monitor->handle_chunk);

  (*G_OBJECT_CLASS (thunar_vfs_monitor_parent_class)->finalize) (object);
}



#ifdef HAVE_LIBFAM
static void
thunar_vfs_monitor_fam_cancel (ThunarVfsMonitor *monitor)
{
  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (monitor->fc_watch_id >= 0);

  /* close the FAM connection */
  FAMClose (&monitor->fc);

  /* remove the I/O watch */
  g_source_remove (monitor->fc_watch_id);
  monitor->fc_watch_id = -1;
}



static void
thunar_vfs_monitor_fam_event (ThunarVfsMonitor *monitor,
                              const FAMEvent   *fe)
{
  ThunarVfsMonitorHandle *handle;
  ThunarVfsMonitorEvent   event;
  ThunarVfsURI           *uri;
  GList                  *lp;

  /* lookup the handle for the request that caused the event */
  for (lp = monitor->handles; lp != NULL; lp = lp->next)
    if (((ThunarVfsMonitorHandle *) lp->data)->fr.reqnum == fe->fr.reqnum)
      break;

  /* yes, this can really happen, see
   * the FAM documentation.
   */
  if (G_UNLIKELY (lp == NULL))
    return;
  else
    handle = lp->data;

  /* translate the event code */
  switch (fe->code)
    {
    case FAMChanged:
      event = THUNAR_VFS_MONITOR_EVENT_CHANGED;
      break;

    case FAMCreated:
      event = THUNAR_VFS_MONITOR_EVENT_CREATED;
      break;

    case FAMDeleted:
      event = THUNAR_VFS_MONITOR_EVENT_DELETED;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  /* determine the URI */
  if (G_UNLIKELY (*fe->filename != '/'))
    uri = thunar_vfs_uri_relative (handle->uri, fe->filename);
  else if (strcmp (thunar_vfs_uri_get_path (handle->uri), fe->filename) == 0)
    uri = thunar_vfs_uri_ref (handle->uri);
  else
    uri = thunar_vfs_uri_new_for_path (fe->filename);

  /* invoke the callback, fortunately - due to our design - we
   * have no reentrancy issues here. ;-)
   */
  (*handle->callback) (monitor, handle, event, handle->uri, uri, handle->user_data);
  thunar_vfs_uri_unref (uri);
}



static gboolean
thunar_vfs_monitor_fam_watch (GIOChannel  *channel,
                              GIOCondition condition,
                              gpointer     user_data)
{
  ThunarVfsMonitor *monitor = THUNAR_VFS_MONITOR (user_data);
  FAMEvent          fe;

  /* check for an error on the FAM connection */
  if (G_UNLIKELY ((condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) != 0))
    {
error:
      /* terminate the FAM connection */
      GDK_THREADS_ENTER ();
      thunar_vfs_monitor_fam_cancel (monitor);
      GDK_THREADS_LEAVE ();

      /* thats it, no more FAM */
      return FALSE;
    }

  /* process all pending FAM events */
  while (FAMPending (&monitor->fc))
    {
      /* query the next pending event */
      if (G_UNLIKELY (FAMNextEvent (&monitor->fc, &fe) < 0))
        goto error;

      /* we ignore all events other than changed/created/deleted */
      if (G_UNLIKELY (fe.code != FAMChanged && fe.code != FAMCreated && fe.code != FAMDeleted))
        continue;

      /* process the event */
      GDK_THREADS_ENTER ();
      thunar_vfs_monitor_fam_event (monitor, &fe);
      GDK_THREADS_LEAVE ();
    }

  return TRUE;
}
#endif



/**
 * thunar_vfs_monitor_get_default:
 *
 * Returns the shared #ThunarVfsMonitor instance. The caller
 * is responsible to call #g_object_unref() on the returned
 * object when no longer needed.
 *
 * Return value: a reference to the shared #ThunarVfsMonitor
 *               instance.
 **/
ThunarVfsMonitor*
thunar_vfs_monitor_get_default (void)
{
  static ThunarVfsMonitor *monitor = NULL;

  if (G_UNLIKELY (monitor == NULL))
    {
      monitor = g_object_new (THUNAR_VFS_TYPE_MONITOR, NULL);
      g_object_add_weak_pointer (G_OBJECT (monitor), (gpointer) &monitor);
    }
  else
    {
      g_object_ref (G_OBJECT (monitor));
    }

  return monitor;
}



/**
 * thunar_vfs_monitor_add_directory:
 * @monitor   : a #ThunarVfsMonitor.
 * @uri       : the #ThunarVfsURI of the directory that should be watched.
 * @callback  : the callback function to invoke.
 * @user_data : additional data to pass to @callback.
 *
 * The @uri is currently limited to the #THUNAR_VFS_URI_SCHEME_FILE.
 *
 * Return value: the #ThunarVfsMonitorHandle for the new watch.
 **/
ThunarVfsMonitorHandle*
thunar_vfs_monitor_add_directory (ThunarVfsMonitor        *monitor,
                                  ThunarVfsURI            *uri,
                                  ThunarVfsMonitorCallback callback,
                                  gpointer                 user_data)
{
  ThunarVfsMonitorHandle *handle;

  g_return_val_if_fail (THUNAR_VFS_IS_MONITOR (monitor), NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_FILE, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  /* allocate a new handle */
  handle = g_chunk_new (ThunarVfsMonitorHandle, monitor->handle_chunk);
  handle->uri = thunar_vfs_uri_ref (uri);
  handle->callback = callback;
  handle->user_data = user_data;
  handle->directory = TRUE;

#ifdef HAVE_LIBFAM
  if (G_LIKELY (monitor->fc_watch_id >= 0))
    {
      /* generate a unique sequence number for the request */
      handle->fr.reqnum = ++monitor->fc_sequence;

      /* schedule the watch on the FAM daemon */
      if (FAMMonitorDirectory2 (&monitor->fc, thunar_vfs_uri_get_path (uri), &handle->fr) < 0)
        thunar_vfs_monitor_fam_cancel (monitor);
    }
#endif

  /* add the handle to the monitor */
  monitor->handles = g_list_prepend (monitor->handles, handle);

  return handle;
}



/**
 * thunar_vfs_monitor_add_file:
 * @monitor   : a #ThunarVfsMonitor.
 * @uri       : the #ThunarVfsURI of the file that should be watched.
 * @callback  : the callback function to invoke.
 * @user_data : additional data to pass to @callback.
 *
 * The @uri is currently limited to the #THUNAR_VFS_URI_SCHEME_FILE.
 *
 * Return value: the #ThunarVfsMonitorHandle for the new watch.
 **/
ThunarVfsMonitorHandle*
thunar_vfs_monitor_add_file (ThunarVfsMonitor        *monitor,
                             ThunarVfsURI            *uri,
                             ThunarVfsMonitorCallback callback,
                             gpointer                 user_data)
{
  ThunarVfsMonitorHandle *handle;

  g_return_val_if_fail (THUNAR_VFS_IS_MONITOR (monitor), NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_FILE, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  /* allocate a new handle */
  handle = g_chunk_new (ThunarVfsMonitorHandle, monitor->handle_chunk);
  handle->uri = thunar_vfs_uri_ref (uri);
  handle->callback = callback;
  handle->user_data = user_data;
  handle->directory = FALSE;

#ifdef HAVE_LIBFAM
  if (G_LIKELY (monitor->fc_watch_id >= 0))
    {
      /* generate a unique sequence number for the request */
      handle->fr.reqnum = ++monitor->fc_sequence;

      /* schedule the watch on the FAM daemon */
      if (FAMMonitorFile2 (&monitor->fc, thunar_vfs_uri_get_path (uri), &handle->fr) < 0)
        thunar_vfs_monitor_fam_cancel (monitor);
    }
#endif

  /* add the handle to the monitor */
  monitor->handles = g_list_prepend (monitor->handles, handle);

  return handle;
}



/**
 * thunar_vfs_monitor_remove:
 * @monitor : a #ThunarVfsMonitor.
 * @handle  : a valid #ThunarVfsMonitorHandle for @monitor.
 *
 * Removes @handle from @monitor.
 **/
void
thunar_vfs_monitor_remove (ThunarVfsMonitor       *monitor,
                           ThunarVfsMonitorHandle *handle)
{
  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (g_list_find (monitor->handles, handle) != NULL);
  g_return_if_fail (g_list_find (monitor->reentrant_removals, handle) == NULL);

#ifdef HAVE_LIBFAM
  /* drop the FAM request from the daemon */
  if (G_LIKELY (monitor->fc_watch_id >= 0 && handle->fr.reqnum > 0))
    {
      if (FAMCancelMonitor (&monitor->fc, &handle->fr) < 0)
        thunar_vfs_monitor_fam_cancel (monitor);
      handle->fr.reqnum = 0;
    }
#endif

  if (monitor->reentrant_level > 0)
    {
      /* we need to delay the removal to not fuck up the handles list */
      monitor->reentrant_removals = g_list_prepend (monitor->reentrant_removals, handle);
    }
  else
    {
      /* unlink the handle */
      monitor->handles = g_list_remove (monitor->handles, handle);

      /* free the handle */
      thunar_vfs_uri_unref (handle->uri);
      g_mem_chunk_free (monitor->handle_chunk, handle);
    }
}



/**
 * thunar_vfs_monitor_feed:
 * @monitor : a #ThunarVfsMonitor.
 * @event   : the #ThunarVfsMonitorEvent that should be emulated.
 * @uri     : the #ThunarVfsURI on which @event took place.
 *
 * Explicitly injects the given @event into @monitor<!---->s event
 * processing logic.
 **/
void
thunar_vfs_monitor_feed (ThunarVfsMonitor     *monitor,
                         ThunarVfsMonitorEvent event,
                         ThunarVfsURI         *uri)
{
  ThunarVfsMonitorHandle *handle;
  ThunarVfsURI           *parent_uri;
  GList                  *removals;
  GList                  *lp;

  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (event == THUNAR_VFS_MONITOR_EVENT_CHANGED
                 || event == THUNAR_VFS_MONITOR_EVENT_CREATED
                 || event == THUNAR_VFS_MONITOR_EVENT_DELETED);
  g_return_if_fail (monitor->reentrant_level >= 0);

  /* increate the reentrancy level */
  ++monitor->reentrant_level;

  /* process all handles affected directly by this event */
  for (lp = monitor->handles; lp != NULL; lp = lp->next)
    {
      /* check if this handle is scheduled for removal */
      handle = (ThunarVfsMonitorHandle *) lp->data;
      if (G_UNLIKELY (g_list_find (monitor->reentrant_removals, handle) != NULL))
        continue;

      if (G_UNLIKELY (thunar_vfs_uri_equal (handle->uri, uri)))
        (*handle->callback) (monitor, handle, event, handle->uri, uri, handle->user_data);
    }

  /* process all directory handles affected indirectly */
  if (G_LIKELY (!thunar_vfs_uri_is_root (uri)))
    {
      parent_uri = thunar_vfs_uri_parent (uri);
      for (lp = monitor->handles; lp != NULL; lp = lp->next)
        {
          /* check if this handle is scheduled for removal */
          handle = (ThunarVfsMonitorHandle *) lp->data;
          if (G_UNLIKELY (g_list_find (monitor->reentrant_removals, handle) != NULL))
            continue;

          if (G_UNLIKELY (handle->directory && thunar_vfs_uri_equal (handle->uri, parent_uri)))
            (*handle->callback) (monitor, handle, event, handle->uri, uri, handle->user_data);
        }
      thunar_vfs_uri_unref (parent_uri);
    }

  /* decrease the reentrancy level */
  if (--monitor->reentrant_level == 0)
    {
      /* query the list of handles to remove */
      removals = monitor->reentrant_removals;
      monitor->reentrant_removals = NULL;

      /* perform any scheduled removals */
      for (lp = removals; lp != NULL; lp = lp->next)
        thunar_vfs_monitor_remove (monitor, lp->data);
      g_list_free (removals);
    }
}



#define __THUNAR_VFS_MONITOR_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
