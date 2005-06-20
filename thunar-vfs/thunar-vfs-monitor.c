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
#ifdef HAVE_SYS_EVENT_H
#include <sys/event.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-monitor.h>



/* the monitor timer interval */
#define THUNAR_VFS_MONITOR_INTERVAL (4 * 1000)



typedef struct _ThunarVfsWatch ThunarVfsWatch;



static void     thunar_vfs_monitor_class_init     (ThunarVfsMonitorClass *klass);
static void     thunar_vfs_monitor_init           (ThunarVfsMonitor      *monitor);
static void     thunar_vfs_monitor_finalize       (GObject               *object);
static gboolean thunar_vfs_monitor_event          (GIOChannel            *source,
                                                   GIOCondition           condition,
                                                   gpointer               user_data);
static gboolean thunar_vfs_monitor_timer          (gpointer               user_data);
static void     thunar_vfs_monitor_timer_destroy  (gpointer               user_data);



struct _ThunarVfsMonitorClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsMonitor
{
  GObject __parent__;

  GList *watches;
  gint   current_id;
  gint   timer_id;

  gint   kq;
  gint   kq_watch_id;
};

struct _ThunarVfsWatch
{
  gint                     id;
  gint                     fd;
  ThunarVfsInfo           *info;
  ThunarVfsMonitorCallback callback;
  gpointer                 user_data;
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
  GIOChannel *channel;

  monitor->current_id = 0;
  monitor->timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_VFS_MONITOR_INTERVAL,
                                          thunar_vfs_monitor_timer, monitor,
                                          thunar_vfs_monitor_timer_destroy);

  /* open a connection to the system's event queue */
#ifdef HAVE_KQUEUE
  monitor->kq = kqueue ();
#else
  monitor->kq = -1;
#endif

  if (G_LIKELY (monitor->kq >= 0))
    {
      /* make sure the connection is not passed on to child processes */
      fcntl (monitor->kq, F_SETFD, fcntl (monitor->kq, F_GETFD, 0) | FD_CLOEXEC);

      /* setup an io channel for the connection */
      channel = g_io_channel_unix_new (monitor->kq);
      monitor->kq_watch_id = g_io_add_watch (channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
                                             thunar_vfs_monitor_event, monitor);
      g_io_channel_unref (channel);
    }
  else
    {
      monitor->kq_watch_id = -1;
    }
}



static void
thunar_vfs_monitor_finalize (GObject *object)
{
  ThunarVfsMonitor *monitor = THUNAR_VFS_MONITOR (object);
  ThunarVfsWatch   *watch;
  GList            *lp;

  /* drop the still present (previously deleted) watches */
  for (lp = monitor->watches; lp != NULL; lp = lp->next)
    {
      watch = lp->data;
      if (G_UNLIKELY (watch->callback != NULL))
        {
          g_error ("Tried to finalize a ThunarVfsMonitor, which has "
                   "atleast one active watch monitoring \"%s\"",
                   thunar_vfs_uri_to_string (watch->info->uri, 0));
        }

#ifdef HAVE_KQUEUE
      if (G_LIKELY (watch->fd >= 0))
        {
          /* prepare the delete command */
          struct kevent ev;
          ev.ident = watch->fd;
          ev.filter = EVFILT_VNODE;
          ev.flags = EV_DELETE;
          ev.fflags = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_REVOKE;

          /* perform the delete operation */
          if (kevent (monitor->kq, &ev, 1, NULL, 0, NULL) < 0)
            g_error ("Failed to remove watch %d: %s", watch->id, g_strerror (errno));

          /* drop the file descriptor */
          close (watch->fd);
        }
#endif

      g_free (watch);
    }
  g_list_free (monitor->watches);

  /* stop the timer */
  if (G_LIKELY (monitor->timer_id >= 0))
    g_source_remove (monitor->timer_id);

  /* remove the main loop watch for the event loop */
  if (G_LIKELY (monitor->kq_watch_id >= 0))
    g_source_remove (monitor->kq_watch_id);

  /* destroy the event queue */
  if (G_LIKELY (monitor->kq >= 0))
    close (monitor->kq);

  G_OBJECT_CLASS (thunar_vfs_monitor_parent_class)->finalize (object);
}



static gboolean
thunar_vfs_monitor_event (GIOChannel  *source,
                          GIOCondition condition,
                          gpointer     user_data)
{
#ifdef HAVE_KQUEUE
  ThunarVfsInfoResult result;
  ThunarVfsMonitor   *monitor = THUNAR_VFS_MONITOR (user_data);
  ThunarVfsWatch     *watch;
  struct kevent       events[16];
  gint                n;

  GDK_THREADS_ENTER ();

  n = kevent (monitor->kq, NULL, 0, events, G_N_ELEMENTS (events), NULL);
  while (--n >= 0)
    {
      watch = events[n].udata;
      if (G_UNLIKELY (watch->callback == NULL))
        continue;

      /* force an update on the info */
      result = thunar_vfs_info_update (watch->info, NULL);
      switch (result)
        {
        case THUNAR_VFS_INFO_RESULT_ERROR:
          watch->callback (monitor, THUNAR_VFS_MONITOR_DELETED, watch->info, watch->user_data);
          break;

        case THUNAR_VFS_INFO_RESULT_CHANGED:
          watch->callback (monitor, THUNAR_VFS_MONITOR_CHANGED, watch->info, watch->user_data);
          break;
        
        case THUNAR_VFS_INFO_RESULT_NOCHANGE:
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  GDK_THREADS_LEAVE ();
#endif

  /* keep the kqueue monitor going */
  return TRUE;
}



static gboolean
thunar_vfs_monitor_timer (gpointer user_data)
{
  ThunarVfsInfoResult result;
  ThunarVfsMonitor   *monitor = THUNAR_VFS_MONITOR (user_data);
  ThunarVfsWatch     *watch;
  GList              *changed = NULL;
  GList              *deleted = NULL;
  GList              *next;
  GList              *lp;

  GDK_THREADS_ENTER ();

  for (lp = monitor->watches; lp != NULL; lp = next)
    {
      watch = lp->data;
      next = lp->next;

      /* handle deletion of watches */
      if (G_UNLIKELY (watch->callback == NULL))
        {
#ifdef HAVE_KQUEUE
          if (G_LIKELY (watch->fd >= 0))
            {
              /* prepare the delete command */
              struct kevent ev;
              ev.ident = watch->fd;
              ev.filter = EVFILT_VNODE;
              ev.flags = EV_DELETE;
              ev.fflags = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_REVOKE;

              /* perform the delete operation */
              if (kevent (monitor->kq, &ev, 1, NULL, 0, NULL) < 0)
                g_error ("Failed to remove watch %d: %s", watch->id, g_strerror (errno));

              /* drop the file descriptor */
              close (watch->fd);
            }
#endif

          monitor->watches = g_list_delete_link (monitor->watches, lp);
          g_free (watch);
          continue;
        }

#ifdef HAVE_KQUEUE
      if (G_LIKELY (watch->fd >= 0))
        continue;
#endif

      /* force an update on the info */
      result = thunar_vfs_info_update (watch->info, NULL);
      switch (result)
        {
        case THUNAR_VFS_INFO_RESULT_ERROR:
          deleted = g_list_prepend (deleted, watch);
          break;

        case THUNAR_VFS_INFO_RESULT_CHANGED:
          changed = g_list_prepend (changed, watch);
          break;
        
        case THUNAR_VFS_INFO_RESULT_NOCHANGE:
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  /* notify all files that have been changed */
  if (G_UNLIKELY (changed != NULL))
    {
      for (lp = changed; lp != NULL; lp = lp->next)
        {
          watch = lp->data;
          if (G_LIKELY (watch->callback != NULL))
            watch->callback (monitor, THUNAR_VFS_MONITOR_CHANGED, watch->info, watch->user_data);
        }
      g_list_free (changed);
    }

  /* notify all files that have been deleted */
  if (G_UNLIKELY (deleted != NULL))
    {
      for (lp = deleted; lp != NULL; lp = lp->next)
        {
          watch = lp->data;
          if (G_LIKELY (watch->callback != NULL))
            watch->callback (monitor, THUNAR_VFS_MONITOR_DELETED, watch->info, watch->user_data);
        }
      g_list_free (deleted);
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_vfs_monitor_timer_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_VFS_MONITOR (user_data)->timer_id = -1;
  GDK_THREADS_LEAVE ();
}



/**
 * thunar_vfs_monitor_get_default:
 *
 * Returns the default #ThunarVfsMonitor which is shared
 * by all modules.
 *
 * You need to call #g_object_unref() on the returned
 * object when you don't need the monitor any longer.
 *
 * Return value: the default #ThunarVfsMonitor instance.
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
 * thunar_vfs_monitor_add_info:
 * @monitor   : a #ThunarVfsMonitor.
 * @info      : pointer to the #ThunarVfsInfo object to watch.
 * @callback  : callback function to invoke whenever changes to the file are detected.
 * @user_data : additional user specific data to pass to @callback.
 *
 * Adds @info to the list of watched entities of @monitor. Note that this
 * way of monitoring is very special and optimized for very low overhead.
 * No copy of @info is made, instead the #ThunarVfsInfo object referenced
 * by @info is modified directly whenever a change is noticed to avoid
 * an additional #stat() invokation. This means, that you'll need to
 * unregister the watch BEFORE freeing @info!
 *
 * Returns the unique id of the newly registered watch, which can later
 * be used to drop the watch from the @monitor using the method
 * #thunar_vfs_monitor_remove().
 *
 * Return value: the unique id of the newly created watch.
 **/
gint
thunar_vfs_monitor_add_info (ThunarVfsMonitor        *monitor,
                             ThunarVfsInfo           *info,
                             ThunarVfsMonitorCallback callback,
                             gpointer                 user_data)
{
  ThunarVfsWatch *watch;

  g_return_val_if_fail (THUNAR_VFS_IS_MONITOR (monitor), 0);
  g_return_val_if_fail (info != NULL && THUNAR_VFS_IS_URI (info->uri), 0);
  g_return_val_if_fail (callback != NULL, 0);

  /* prepare the watch */
  watch = g_new (ThunarVfsWatch, 1);
  watch->id = ++monitor->current_id;
  watch->info = info;
  watch->callback = callback;
  watch->user_data = user_data;

  /* add it to the list of active watches */
  monitor->watches = g_list_prepend (monitor->watches, watch);

#ifdef HAVE_KQUEUE
  if (G_LIKELY (monitor->kq >= 0))
    {
      watch->fd = open (thunar_vfs_uri_get_path (info->uri), O_RDONLY, 0);
      if (watch->fd >= 0)
        {
          /* prepare the event definition */
          struct kevent ev;
          ev.ident = watch->fd;
          ev.filter = EVFILT_VNODE;
          ev.flags = EV_ADD | EV_ENABLE | EV_CLEAR;
          ev.fflags = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_REVOKE;
          ev.udata = watch;

          /* try to add the event */
          if (kevent (monitor->kq, &ev, 1, NULL, 0, NULL) < 0)
            {
              close (watch->fd);
              watch->fd = -1;
            }
        }
    }
#endif

  return watch->id;
}



/**
 * thunar_vfs_monitor_add_path:
 * @monitor   : a #ThunarVfsMonitor.
 * @path      : the path to the file or directory to monitor for changes.
 * @callback  : a callback function to be invoked whenever changes are noticed on the @path.
 * @user_data : additional user specific data to pass to @callback.
 *
 * FIXME
 *
 * Returns the unique id of the newly registered watch, which can later
 * be used to drop the watch from the @monitor using the method
 * #thunar_vfs_monitor_remove().
 *
 * Return value: the watch id.
 **/
gint
thunar_vfs_monitor_add_path (ThunarVfsMonitor        *monitor,
                             const gchar             *path,
                             ThunarVfsMonitorCallback callback,
                             gpointer                 user_data)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MONITOR (monitor), 0);
  g_return_val_if_fail (g_path_is_absolute (path), 0);
  g_return_val_if_fail (callback != NULL, 0);

  g_error ("Not implemented yet!");

  return 0;
}



/**
 * thunar_vfs_monitor_remove_info:
 * @monitor : a #ThunarVfsMonitor.
 * @id      : the watch id.
 *
 * Removes an existing watch identified by @id from the
 * given @monitor.
 *
 * Note that the @id must be valid, else the application
 * will abort.
 **/
void
thunar_vfs_monitor_remove (ThunarVfsMonitor *monitor,
                           gint              id)
{
  ThunarVfsWatch *watch;
  GList          *lp;

  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (id > 0);

  for (lp = monitor->watches; lp != NULL; lp = lp->next)
    {
      watch = lp->data;
      if (watch->id == id && watch->callback != NULL)
        {
          /* mark this watch for deletion */
          watch->callback = NULL;
          return;
        }
    }

  g_error ("Watch %d not present in ThunarVfsMonitor", id);
}
