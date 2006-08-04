/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_UTIL_H__
#define __THUNAR_UTIL_H__

#include <thunar-vfs/thunar-vfs.h>

G_BEGIN_DECLS;

gboolean   thunar_util_looks_like_an_uri  (const gchar      *string) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

gchar     *thunar_util_humanize_file_time (ThunarVfsFileTime file_time) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkScreen *thunar_util_parse_parent       (gpointer          parent,
                                           GtkWindow       **window_return) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

time_t     thunar_util_time_from_rfc3339  (const gchar      *date_string) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_UTIL_H__ */
