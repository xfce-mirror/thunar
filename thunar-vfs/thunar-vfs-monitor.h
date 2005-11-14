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

#ifndef __THUNAR_VFS_MONITOR_H__
#define __THUNAR_VFS_MONITOR_H__

#include <thunar-vfs/thunar-vfs-path.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMonitorClass ThunarVfsMonitorClass;
typedef struct _ThunarVfsMonitor      ThunarVfsMonitor;

#define THUNAR_VFS_TYPE_MONITOR             (thunar_vfs_monitor_get_type ())
#define THUNAR_VFS_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MONITOR, ThunarVfsMonitor))
#define THUNAR_VFS_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MONITOR, ThunarVfsMonitorClass))
#define THUNAR_VFS_IS_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MONITOR))
#define THUNAR_VFS_IS_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MONITOR))
#define THUNAR_VFS_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MONITOR, ThunarVfsMonitorClass))

/**
 * ThunarVfsMonitorEvent:
 * @THUNAR_VFS_MONITOR_EVENT_CHANGED : a file or directory was changed.
 * @THUNAR_VFS_MONITOR_EVENT_CREATED : a file or directory was created.
 * @THUNAR_VFS_MONITOR_EVENT_DELETED : a file or directory was deleted.
 *
 * Describes an event that occurred on a #ThunarVfsMonitorHandle.
 **/
typedef enum
{
  THUNAR_VFS_MONITOR_EVENT_CHANGED,
  THUNAR_VFS_MONITOR_EVENT_CREATED,
  THUNAR_VFS_MONITOR_EVENT_DELETED,
} ThunarVfsMonitorEvent;

/**
 * ThunarVfsMonitorHandle:
 *
 * A handle on a file system entity, which is currently watched
 * by a #ThunarVfsMonitor.
 **/
typedef struct _ThunarVfsMonitorHandle ThunarVfsMonitorHandle;

/**
 * ThunarVfsMonitorCallback:
 * @monitor     : a #ThunarVfsMonitor.
 * @handle      : a #ThunarVfsMonitorHandle.
 * @event       : the event that occurred.
 * @handle_path : the #ThunarVfsPath that was specified when registering the @handle.
 * @event_path  : the #ThunarVfsPath on which the @event occurred.
 * @user_data   : the user data that was specified when registering the @handle with the @monitor.
 *
 * The prototype for callback functions that will be called by a #ThunarVfsMonitor
 * whenever one of its associated #ThunarVfsMonitorHandle<!---->s notice a
 * change.
 **/
typedef void (*ThunarVfsMonitorCallback)  (ThunarVfsMonitor       *monitor,
                                           ThunarVfsMonitorHandle *handle,
                                           ThunarVfsMonitorEvent   event,
                                           ThunarVfsPath          *handle_path,
                                           ThunarVfsPath          *event_path,
                                           gpointer                user_data);

GType                   thunar_vfs_monitor_get_type       (void) G_GNUC_CONST;

ThunarVfsMonitor       *thunar_vfs_monitor_get_default    (void);

ThunarVfsMonitorHandle *thunar_vfs_monitor_add_directory  (ThunarVfsMonitor        *monitor,
                                                           ThunarVfsPath           *path,
                                                           ThunarVfsMonitorCallback callback,
                                                           gpointer                 user_data);

ThunarVfsMonitorHandle *thunar_vfs_monitor_add_file       (ThunarVfsMonitor        *monitor,
                                                           ThunarVfsPath           *path,
                                                           ThunarVfsMonitorCallback callback,
                                                           gpointer                 user_data);

void                    thunar_vfs_monitor_remove         (ThunarVfsMonitor        *monitor,
                                                           ThunarVfsMonitorHandle  *handle);

void                    thunar_vfs_monitor_feed           (ThunarVfsMonitor        *monitor,
                                                           ThunarVfsMonitorEvent    event,
                                                           ThunarVfsPath           *path);

void                    thunar_vfs_monitor_wait           (ThunarVfsMonitor        *monitor);

G_END_DECLS;

#endif /* !__THUNAR_VFS_MONITOR_H__ */
