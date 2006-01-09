/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_UTIL_H__
#define __THUNAR_VFS_UTIL_H__

#include <glib.h>

#include <thunar-vfs/thunar-vfs-types.h>

G_BEGIN_DECLS;

gchar *thunar_vfs_expand_filename (const gchar      *filename,
                                   GError          **error) G_GNUC_MALLOC;

gchar *thunar_vfs_humanize_size   (ThunarVfsFileSize size,
                                   gchar            *buffer,
                                   gsize             buflen);

G_END_DECLS;

#endif /* !__THUNAR_VFS_UTIL_H__ */
