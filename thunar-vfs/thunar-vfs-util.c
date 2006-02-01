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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * thunar_vfs_expand_filename:
 * @filename : a local filename.
 * @error    : return location for errors or %NULL.
 *
 * Takes a user-typed @filename and expands a tilde at the
 * beginning of the @filename.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the expanded @filename or %NULL on error.
 **/
gchar*
thunar_vfs_expand_filename (const gchar *filename,
                            GError     **error)
{
  struct passwd *passwd;
  const gchar   *replacement;
  const gchar   *remainder;
  const gchar   *slash;
  gchar         *username;

  g_return_val_if_fail (filename != NULL, NULL);

  /* check if we have a valid (non-empty!) filename */
  if (G_UNLIKELY (*filename == '\0'))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid path"));
      return NULL;
    }

  /* check if we start with a '~' */
  if (G_LIKELY (*filename != '~'))
    return g_strdup (filename);

  /* examine the remainder of the filename */
  remainder = filename + 1;

  /* if we have only the slash, then we want the home dir */
  if (G_UNLIKELY (*remainder == '\0'))
    return g_strdup (xfce_get_homedir ());

  /* lookup the slash */
  for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash)
    ;

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
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unknown user `%s'"), username);
          g_free (username);
          return NULL;
        }

      /* use the homedir of the specified user */
      replacement = passwd->pw_dir;
    }

  /* generate the filename */
  return g_build_filename (replacement, slash + 1, NULL);
}



/**
 * thunar_vfs_humanize_size:
 * @size   : size in bytes.
 * @buffer : destination buffer or %NULL to dynamically allocate a buffer.
 * @buflen : length of @buffer in bytes.
 *
 * The caller is responsible to free the returned string using g_free()
 * if you pass %NULL for @buffer. Else the returned string will be a
 * pointer to @buffer.
 *
 * Return value: a string containing a human readable description of @size.
 **/
gchar*
thunar_vfs_humanize_size (ThunarVfsFileSize size,
                          gchar            *buffer,
                          gsize             buflen)
{
  /* allocate buffer if necessary */
  if (buffer == NULL)
    {
      buffer = g_new (gchar, 32);
      buflen = 32;
    }

  if (G_UNLIKELY (size > 1024ul * 1024ul * 1024ul))
    g_snprintf (buffer, buflen, "%0.1f GB", size / (1024.0 * 1024.0 * 1024.0));
  else if (size > 1024ul * 1024ul)
    g_snprintf (buffer, buflen, "%0.1f MB", size / (1024.0 * 1024.0));
  else if (size > 1024ul)
    g_snprintf (buffer, buflen, "%0.1f kB", size / 1024.0);
  else
    g_snprintf (buffer, buflen, "%lu B", (gulong) size);

  return buffer;
}



#define __THUNAR_VFS_UTIL_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
