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

#include <thunar-vfs/thunar-vfs-info.h>

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
 * @THUNAR_VFS_MONITOR_CHANGED: a monitored file has changed.
 * @THUNAR_VFS_MONITOR_DELETED: a monitored file was deleted.
 * FIXME
 **/
typedef enum
{
  THUNAR_VFS_MONITOR_CHANGED,
  THUNAR_VFS_MONITOR_CREATED,
  THUNAR_VFS_MONITOR_DELETED,
} ThunarVfsMonitorEvent;

/**
 * ThunarVfsMonitorCallback:
 * FIXME
 **/
typedef void (*ThunarVfsMonitorCallback) (ThunarVfsMonitor     *monitor,
                                          ThunarVfsMonitorEvent event,
                                          ThunarVfsInfo        *info,
                                          gpointer              user_data);

GType             thunar_vfs_monitor_get_type     (void) G_GNUC_CONST;

ThunarVfsMonitor *thunar_vfs_monitor_get_default  (void);

gint              thunar_vfs_monitor_add_info     (ThunarVfsMonitor        *monitor,
                                                   ThunarVfsInfo           *info,
                                                   ThunarVfsMonitorCallback callback,
                                                   gpointer                 user_data);

gint              thunar_vfs_monitor_add_path     (ThunarVfsMonitor        *monitor,
                                                   const gchar             *path,
                                                   ThunarVfsMonitorCallback callback,
                                                   gpointer                 user_data);

void              thunar_vfs_monitor_remove       (ThunarVfsMonitor        *monitor,
                                                   gint                     id);

G_END_DECLS;

#endif /* !__THUNAR_VFS_MONITOR_H__ */
