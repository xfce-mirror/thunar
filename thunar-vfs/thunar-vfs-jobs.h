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

#ifndef __THUNAR_VFS_JOBS_H__
#define __THUNAR_VFS_JOBS_H__

#include <thunar-vfs/thunar-vfs-job.h>

G_BEGIN_DECLS;

ThunarVfsJob *thunar_vfs_listdir  (ThunarVfsURI *uri,
                                   GError      **error);

ThunarVfsJob *thunar_vfs_copy     (GList        *source_uri_list,
                                   ThunarVfsURI *target_uri,
                                   GError      **error);

ThunarVfsJob *thunar_vfs_move     (GList        *source_uri_list,
                                   ThunarVfsURI *target_uri,
                                   GError      **error);

ThunarVfsJob *thunar_vfs_unlink   (GList        *uri_list,
                                   GError      **error);

G_END_DECLS;

#endif /* !__THUNAR_VFS_JOBS_H__ */
