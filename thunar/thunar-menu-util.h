/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2017 Andre Miranda <andreldm@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_MENU_UTIL_H__
#define __THUNAR_MENU_UTIL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

void       thunar_menu_util_add_items_to_ui_manager  (GtkUIManager   *ui_manager,
                                                      GtkActionGroup *action_group,
                                                      gint            merge_id,
                                                      const gchar    *path,
                                                      GList          *items);

void       thunar_menu_util_add_items_to_menu        (GtkWidget *menu,
                                                      GList     *items);

G_END_DECLS

#endif /* !__THUNAR_MENU_UTIL_H__ */
