/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Philip Waritschlager <philip@waritschlager.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __THUNAR_LOGGER_H__
#define __THUNAR_LOGGER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_LOGGER (thunar_logger_get_type ())
G_DECLARE_FINAL_TYPE (ThunarLogger, thunar_logger, THUNAR, LOGGER, GObject)

ThunarLogger *
thunar_logger_get_default (void);
void
thunar_logger_println (const gchar *format, ...);

G_END_DECLS

#endif /* __THUNAR_LOGGER_H__ */
