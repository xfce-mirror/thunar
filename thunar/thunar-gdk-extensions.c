/* $Id$ */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-private.h>



/**
 * thunar_gdk_screen_open:
 * @display_name : a fully qualified display name.
 * @error        : return location for errors or %NULL.
 *
 * Opens the screen referenced by the @display_name. Returns a 
 * reference on the #GdkScreen for @display_name or %NULL if
 * @display_name couldn't be opened.
 *
 * If @display_name is the empty string "", a reference on the
 * default screen will be returned.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the #GdkScreen for @display_name or %NULL if
 *               @display_name could not be opened.
 **/
GdkScreen*
thunar_gdk_screen_open (const gchar *display_name,
                        GError     **error)
{
  const gchar *other_name;
  GdkDisplay  *display = NULL;
  GdkScreen   *screen = NULL;
  GSList      *displays;
  GSList      *dp;
  gulong       n;
  gchar       *period;
  gchar       *name;
  gchar       *end;
  gint         num = 0;

  _thunar_return_val_if_fail (display_name != NULL, NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* check if the default screen should be opened */
  if (G_UNLIKELY (*display_name == '\0'))
    return g_object_ref (gdk_screen_get_default ());

  /* extract the display part of the name */
  name = g_strdup (display_name);
  period = strrchr (name, '.');
  if (period != NULL)
    {
      errno = 0;
      *period++ = '\0';
      end = period;
      n = strtoul (period, &end, 0);
      if (errno == 0 && period != end)
        num = n;
    }

  /* check if we already have that display */
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (dp = displays; dp != NULL; dp = dp->next)
    {
      other_name = gdk_display_get_name (dp->data);
      if (strncmp (other_name, name, strlen (name)) == 0)
        {
          display = dp->data;
          break;
        }
    }
  g_slist_free (displays);

  /* try to open the display if not already done */
  if (display == NULL)
    display = gdk_display_open (display_name);

  /* try to locate the screen on the display */
  if (display != NULL)
    {
      if (num >= 0 && num < gdk_display_get_n_screens (display))
        screen = gdk_display_get_screen (display, num);

      if (screen != NULL)
        g_object_ref (G_OBJECT (screen));
    }

  /* release the name buffer */
  g_free (name);

  /* check if we were successfull here */
  if (G_UNLIKELY (screen == NULL))
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open display \"%s\"", display_name);

  return screen;
}
