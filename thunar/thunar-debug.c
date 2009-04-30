/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar/thunar-debug.h>

#include <glib/gstdio.h>



/**
 * thunar_debug_mark:
 * @file   : the filename.
 * @line   : the line in @file.
 * @format : the message format.
 * @...    : the @format parameters.
 *
 * Sets a profile mark.
 **/
void
thunar_debug_mark (const gchar *file,
                   const gint   line,
                   const gchar *format,
                   ...)
{
#ifdef G_ENABLE_DEBUG
  va_list args;
  gchar  *formatted;
  gchar  *message;

  va_start (args, format);
  formatted = g_strdup_vprintf (format, args);
  va_end (args);

  message = g_strdup_printf ("MARK: %s: %s:%d: %s", g_get_prgname(), file, line, formatted);

  g_access (message, F_OK);

  g_free (formatted);
  g_free (message);
#endif
}


