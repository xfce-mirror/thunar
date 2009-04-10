/* $Id$ */
/*-
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

#ifndef __THUNAR_GLIB_EXTENSIONS_H__
#define __THUNAR_GLIB_EXTENSIONS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

GFile   *g_file_new_for_home         (void);
GFile   *g_file_new_for_root         (void);
GFile   *g_file_new_for_trash        (void);

gboolean g_file_is_root              (GFile       *file);
gboolean g_file_is_trashed           (GFile       *file);

GList   *g_file_list_new_from_string (const gchar *string);
gchar   *g_file_list_to_string       (GList       *list);
GList   *g_file_list_copy            (GList       *list);
void     g_file_list_free            (GList       *list);

G_END_DECLS

#endif /* !__THUNAR_GLIB_EXTENSIONS_H__ */
