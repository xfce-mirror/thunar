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

#ifndef __THUNAR_VFS_MIME_PARSER_H__
#define __THUNAR_VFS_MIME_PARSER_H__

#include <thunar-vfs/thunar-vfs-config.h>

G_BEGIN_DECLS;

gchar *_thunar_vfs_mime_parser_load_comment_from_file (const gchar *filename,
                                                       GError     **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_PARSER_H__ */
