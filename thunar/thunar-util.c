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

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/**
 * thunar_util_looks_like_an_uri:
 * @string : an input string.
 *
 * Returns %TRUE if the @string looks like an URI
 * according to RFC 2396, %FALSE otherwise.
 *
 * Return value: %TRUE if @string looks like an URI.
 **/
gboolean
thunar_util_looks_like_an_uri (const gchar *string)
{
  _thunar_return_val_if_fail (string != NULL, FALSE);

  /* <scheme> starts with an alpha character */
  if (g_ascii_isalpha (*string))
    {
      /* <scheme> continues with (alpha | digit | "+" | "-" | ".")* */
      for (++string; g_ascii_isalnum (*string) || *string == '+' || *string == '-' || *string == '.'; ++string)
        ;

      /* <scheme> must be followed by ":" */
      return (*string == ':');
    }

  return FALSE;
}



/**
 * thunar_util_humanize_file_time:
 * @file_time : a #ThunarVfsFileTime.
 *
 * Returns a human readable date representation of the specified
 * @file_time. The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * Return value: a human readable date representation of @file_time.
 **/
gchar*
thunar_util_humanize_file_time (ThunarVfsFileTime file_time)
{
  const gchar *date_format;
  gchar       *date_string;
  GDate        dfile;
  GDate        dnow;
  gint         diff;

  /* check if the file_time is invalid */
  if (G_LIKELY (file_time != 0))
    {
      /* setup the GDate's */
#if GLIB_CHECK_VERSION(2,10,0)
      g_date_set_time_t (&dfile, file_time);
      g_date_set_time_t (&dnow, time (NULL));
#else
      g_date_set_time (&dfile, (GTime) file_time);
      g_date_set_time (&dnow, (GTime) time (NULL));
#endif

      /* determine the difference in days */
      diff = g_date_get_julian (&dnow) - g_date_get_julian (&dfile);
      if (diff == 0)
        {
          /* TRANSLATORS: file was modified less than one day ago */
          return g_strdup (_("Today"));
        }
      else if (diff == 1)
        {
          /* TRANSLATORS: file was modified less than two days ago */
          return g_strdup (_("Yesterday"));
        }
      else
        {
          if (diff > 1 && diff < 7)
            date_format = "%A"; /* Days from last week */
          else
            date_format = "%x"; /* Any other date */

          /* format the date string accordingly */
          date_string = g_new (gchar, 16);
          if (g_date_strftime (date_string, 16, date_format, &dfile) != 0)
            return date_string;
          g_free (date_string);
        }
    }

  /* the file_time is invalid */
  return g_strdup (_("Unknown"));
}



/**
 * thunar_util_parse_parent:
 * @parent        : a #GtkWidget, a #GdkScreen or %NULL.
 * @window_return : return location for the toplevel #GtkWindow or
 *                  %NULL.
 *
 * Determines the screen for the @parent and returns that #GdkScreen.
 * If @window_return is not %NULL, the pointer to the #GtkWindow is
 * placed into it, or %NULL if the window could not be determined.
 *
 * Return value: the #GdkScreen for the @parent.
 **/
GdkScreen*
thunar_util_parse_parent (gpointer    parent,
                          GtkWindow **window_return)
{
  GdkScreen *screen;
  GtkWidget *window = NULL;

  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), NULL);

  /* determine the proper parent */
  if (parent == NULL)
    {
      /* just use the default screen then */
      screen = gdk_screen_get_default ();
    }
  else if (GDK_IS_SCREEN (parent))
    {
      /* yep, that's a screen */
      screen = GDK_SCREEN (parent);
    }
  else
    {
      /* parent is a widget, so let's determine the toplevel window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (parent));
      if (GTK_WIDGET_TOPLEVEL (window))
        {
          /* make sure the toplevel window is shown */
          gtk_widget_show_now (window);
        }
      else
        {
          /* no toplevel, not usable then */
          window = NULL;
        }

      /* determine the screen for the widget */
      screen = gtk_widget_get_screen (GTK_WIDGET (parent));
    }

  /* check if we should return the window */
  if (G_LIKELY (window_return != NULL))
    *window_return = (GtkWindow *) window;

  return screen;
}



/**
 * thunar_util_time_from_rfc3339:
 * @date_string : an RFC 3339 encoded date string.
 *
 * Decodes the @date_string, which must be in the special RFC 3339
 * format <literal>YYYY-MM-DDThh:mm:ss</literal>. This method is
 * used to decode deletion dates of files in the trash. See the
 * Trash Specification for details.
 *
 * Return value: the time value matching the @date_string or
 *               %0 if the @date_string could not be parsed.
 **/
time_t
thunar_util_time_from_rfc3339 (const gchar *date_string)
{
  struct tm tm;

#ifdef HAVE_STRPTIME
  /* using strptime() its easy to parse the date string */
  if (G_UNLIKELY (strptime (date_string, "%FT%T", &tm) == NULL))
    return 0;
#else
  gulong val;

  /* be sure to start with a clean tm */
  memset (&tm, 0, sizeof (tm));

  /* parsing by hand is also doable for RFC 3339 dates */
  val = strtoul (date_string, (gchar **) &date_string, 10);
  if (G_UNLIKELY (*date_string != '-'))
    return 0;

  /* YYYY-MM-DD */
  tm.tm_year = val - 1900;
  date_string++;
  tm.tm_mon = strtoul (date_string, (gchar **) &date_string, 10) - 1;
  
  if (G_UNLIKELY (*date_string++ != '-'))
    return 0;
  
  tm.tm_mday = strtoul (date_string, (gchar **) &date_string, 10);

  if (G_UNLIKELY (*date_string++ != 'T'))
    return 0;
  
  val = strtoul (date_string, (gchar **) &date_string, 10);
  if (G_UNLIKELY (*date_string != ':'))
    return 0;

  /* hh:mm:ss */
  tm.tm_hour = val;
  date_string++;
  tm.tm_min = strtoul (date_string, (gchar **) &date_string, 10);
  
  if (G_UNLIKELY (*date_string++ != ':'))
    return 0;
  
  tm.tm_sec = strtoul (date_string, (gchar **) &date_string, 10);
#endif /* !HAVE_STRPTIME */

  /* translate tm to time_t */
  return mktime (&tm);
}


