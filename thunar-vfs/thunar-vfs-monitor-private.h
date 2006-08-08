/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#if !defined(THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __THUNAR_VFS_MONITOR_PRIVATE_H__
#define __THUNAR_VFS_MONITOR_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-monitor.h>

G_BEGIN_DECLS;

/* shared monitor instance */
extern ThunarVfsMonitor *_thunar_vfs_monitor G_GNUC_INTERNAL;

/* internal routines */
ThunarVfsPath *_thunar_vfs_monitor_handle_get_path (const ThunarVfsMonitorHandle *handle) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_MONITOR_PRIVATE_H__ */
