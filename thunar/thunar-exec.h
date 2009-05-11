/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_VFS_EXEC_H__
#define __THUNAR_VFS_EXEC_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS;

gboolean thunar_exec_parse     (const gchar  *exec,
                                GList        *path_list,
                                const gchar  *icon,
                                const gchar  *name,
                                const gchar  *path,
                                gboolean      terminal,
                                gint         *argc,
                                gchar      ***argv,
                                GError      **error) G_GNUC_INTERNAL;

gboolean thunar_exec_on_screen (GdkScreen    *screen,
                                const gchar  *working_directory,
                                gchar       **argv,
                                gchar       **envp,
                                GSpawnFlags   flags,
                                gboolean      startup_notify,
                                const gchar  *icon_name,
                                GError      **error) G_GNUC_INTERNAL;

gboolean thunar_exec_sync      (const gchar  *command_line,
                                GError      **error,
                                ...) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_EXEC_H__ */
