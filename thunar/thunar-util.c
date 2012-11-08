/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef G_PLATFORM_WIN32
#include <direct.h>
#include <glib/gwin32.h>
#endif

#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>

#include <glib.h>
#include <glib/gstdio.h>



void
thunar_util_load_bookmarks (GFile               *bookmarks_file,
                            ThunarBookmarksFunc  foreach_func,
                            gpointer             user_data)
{
  gchar       *bookmarks_path;
  gchar        line[1024];
  const gchar *name;
  gchar       *space;
  FILE        *fp;
  gint         row_num = 1;
  GFile       *file;

  _thunar_return_if_fail (G_IS_FILE (bookmarks_file));
  _thunar_return_if_fail (g_file_is_native (bookmarks_file));
  _thunar_return_if_fail (foreach_func != NULL);

  /* determine the path to the GTK+ bookmarks file */
  bookmarks_path = g_file_get_path (bookmarks_file);

  /* append the GTK+ bookmarks (if any) */
  fp = fopen (bookmarks_path, "r");
  if (G_LIKELY (fp != NULL))
    {
      while (fgets (line, sizeof (line), fp) != NULL)
        {
          /* remove trailing spaces */
          g_strchomp (line);

          /* skip over empty lines */
          if (*line == '\0' || *line == ' ')
            continue;

          /* check if there is a custom name in the line */
          name = NULL;
          space = strchr (line, ' ');
          if (space != NULL)
            {
              /* break line */
              *space++ = '\0';

              /* get the custom name */
              if (G_LIKELY (*space != '\0'))
                name = space;
            }

          file = g_file_new_for_uri (line);

          /* callback */
          foreach_func (file, name, row_num++, user_data);

          g_object_unref (G_OBJECT (file));
        }

      fclose (fp);
    }

  g_free (bookmarks_path);
}



/**
 * thunar_util_expand_filename:
 * @filename          : a local filename.
 * @working_directory : #GFile of the current working directory.
 * @error             : return location for errors or %NULL.
 *
 * Takes a user-typed @filename and expands a tilde at the
 * beginning of the @filename. It also resolves paths prefixed with
 * '.' using the current working directory.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the expanded @filename or %NULL on error.
 **/
gchar *
thunar_util_expand_filename (const gchar  *filename,
                             GFile        *working_directory,
                             GError      **error)
{
  struct passwd *passwd;
  const gchar   *replacement;
  const gchar   *remainder;
  const gchar   *slash;
  gchar         *username;
  gchar         *pwd;
  gchar         *result = NULL;

  g_return_val_if_fail (filename != NULL, NULL);

  /* check if we have a valid (non-empty!) filename */
  if (G_UNLIKELY (*filename == '\0'))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid path"));
      return NULL;
    }

  /* check if we start with a '~' */
  if (*filename == '~')
    {
      /* examine the remainder of the filename */
      remainder = filename + 1;

      /* if we have only the slash, then we want the home dir */
      if (G_UNLIKELY (*remainder == '\0'))
        return g_strdup (xfce_get_homedir ());

      /* lookup the slash */
      for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash);

      /* check if a username was given after the '~' */
      if (G_LIKELY (slash == remainder))
        {
          /* replace the tilde with the home dir */
          replacement = xfce_get_homedir ();
        }
      else
        {
          /* lookup the pwd entry for the username */
          username = g_strndup (remainder, slash - remainder);
          passwd = getpwnam (username);
          g_free (username);

          /* check if we have a valid entry */
          if (G_UNLIKELY (passwd == NULL))
            {
              username = g_strndup (remainder, slash - remainder);
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unknown user \"%s\""), username);
              g_free (username);
              return NULL;
            }

          /* use the homedir of the specified user */
          replacement = passwd->pw_dir;
        }
    
      /* generate the filename */
      return g_build_filename (replacement, slash, NULL);
    }
  else if (*filename == '.')
    {
      /* examine the remainder of the filename */
      remainder = filename + 1;
      
      /* transform working directory into a filename string */
      if (G_LIKELY (working_directory != NULL))
        {
          pwd = g_file_get_path (working_directory);
    
          /* if we only have the slash then we want the working directory only */
          if (G_UNLIKELY (*remainder == '\0'))
            return pwd;

          /* concatenate working directory and remainder */
          result = g_build_filename (pwd, remainder, G_DIR_SEPARATOR_S, NULL);

          /* free the working directory string */
          g_free (pwd);
        }
      else
        result = g_strdup (filename);

      /* return the resulting path string */
      return result;
    }

  return g_strdup (filename);
}



/**
 * thunar_util_humanize_file_time:
 * @file_time   : a #guint64 timestamp.
 * @date_format : the #ThunarDateFormat used to humanize the @file_time.
 *
 * Returns a human readable date representation of the specified
 * @file_time. The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * Return value: a human readable date representation of @file_time
 *               according to the @date_format.
 **/
gchar*
thunar_util_humanize_file_time (guint64         file_time,
                                ThunarDateStyle date_style)
{
  const gchar *date_format;
  struct tm   *tfile;
  time_t       ftime;
  GDate        dfile;
  GDate        dnow;
  gint         diff;

  /* check if the file_time is valid */
  if (G_LIKELY (file_time != 0))
    {
      ftime = (time_t) file_time;

      /* determine the local file time */
      tfile = localtime (&ftime);

      /* check which style to use to format the time */
      if (date_style == THUNAR_DATE_STYLE_SIMPLE || date_style == THUNAR_DATE_STYLE_SHORT)
        {
          /* setup the dates for the time values */
          g_date_set_time_t (&dfile, (time_t) ftime);
          g_date_set_time_t (&dnow, time (NULL));

          /* determine the difference in days */
          diff = g_date_get_julian (&dnow) - g_date_get_julian (&dfile);
          if (diff == 0)
            {
              if (date_style == THUNAR_DATE_STYLE_SIMPLE)
                {
                  /* TRANSLATORS: file was modified less than one day ago */
                  return g_strdup (_("Today"));
                }
              else /* if (date_style == THUNAR_DATE_STYLE_SHORT) */
                {
                  /* TRANSLATORS: file was modified less than one day ago */
                  return exo_strdup_strftime (_("Today at %X"), tfile);
                }
            }
          else if (diff == 1)
            {
              if (date_style == THUNAR_DATE_STYLE_SIMPLE)
                {
                  /* TRANSLATORS: file was modified less than two days ago */
                  return g_strdup (_("Yesterday"));
                }
              else /* if (date_style == THUNAR_DATE_STYLE_SHORT) */
                {
                  /* TRANSLATORS: file was modified less than two days ago */
                  return exo_strdup_strftime (_("Yesterday at %X"), tfile);
                }
            }
          else
            {
              if (diff > 1 && diff < 7)
                {
                  /* Days from last week */
                  date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%A" : _("%A at %X");
                }
              else
                {
                  /* Any other date */
                  date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%x" : _("%x at %X");
                }

              /* format the date string accordingly */
              return exo_strdup_strftime (date_format, tfile);
            }
        }
      else if (date_style == THUNAR_DATE_STYLE_LONG)
        {
          /* use long, date(1)-like format string */
          return exo_strdup_strftime ("%c", tfile);
        }
      else /* if (date_style == THUNAR_DATE_STYLE_ISO) */
        {
          /* use ISO date formatting */
          return exo_strdup_strftime ("%Y-%m-%d %H:%M:%S", tfile);
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
      if (window != NULL
          && gtk_widget_is_toplevel (window))
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



gchar *
thunar_util_change_working_directory (const gchar *new_directory)
{
  gchar *old_directory;

  _thunar_return_val_if_fail (new_directory != NULL && *new_directory != '\0', NULL);

  /* try to determine the current working directory */
  old_directory = g_get_current_dir();

  /* try switching to the new working directory */
  if (g_chdir (new_directory) != 0)
    {
      /* switching failed, we don't need to return the old directory */
      g_free (old_directory);
      old_directory = NULL;
    }

  return old_directory;
}
