/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-sysdep.h>
#include <thunar-vfs/thunar-vfs-uri.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * _thunar_vfs_sysdep_readdir:
 * @dirp    : the %DIR pointer.
 * @entry   : the buffer to store the dir entry to.
 * @result  : result pointer.
 * @error   : return location for errors or %NULL.
 *
 * This function provides a thread-safe way to read the
 * next directory entry from a %DIR pointer. The caller
 * must provide a buffer where the result can be stored
 * to (the @entry parameter). If the operation succeeds,
 * %TRUE will be returned, and @result will be set to
 * point to %NULL or @result, depending on whether the
 * end of the directory was reached.
 *
 * This function will never return directory entries
 * for '.' and '..', as they aren't used throughout
 * Thunar.
 *
 * Return value: %FALSE if an error occurred, else %TRUE.
 **/
gboolean
_thunar_vfs_sysdep_readdir (gpointer        dirp,
                            struct dirent  *entry,
                            struct dirent **result,
                            GError        **error)
{
#ifndef HAVE_READDIR_R
  G_LOCK_DEFINE_STATIC (readdir);
#endif

  g_return_val_if_fail (dirp != NULL, FALSE);
  g_return_val_if_fail (entry != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  for (;;)
    {
#ifdef HAVE_READDIR_R
      if (G_UNLIKELY (readdir_r (dirp, entry, result) < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       g_strerror (errno));
          return FALSE;
        }
#else
      G_LOCK (readdir);
      *result = readdir (dirp);
      if (G_LIKELY (*result != NULL))
        *result = memcpy (entry, *result, sizeof (*entry));
      G_UNLOCK (readdir);
#endif

      /* ignore "." and ".." entries */
      if (G_LIKELY (*result != NULL))
        {
          const gchar *name = entry->d_name;
          if (G_UNLIKELY (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))))
            continue;
        }

      /* we got our next entry */
      break;
    }

  return TRUE;
}



/**
 * _thunar_vfs_sysdep_parse_exec:
 * @exec     : the value of the <literal>Exec</literal> field.
 * @uris     : the list of #ThunarVfsURI<!---->s.
 * @icon     : value of the <literal>Icon</literal> field or %NULL.
 * @name     : translated value for the <literal>Name</literal> field or %NULL.
 * @path     : full path to the desktop file or %NULL.
 * @terminal : whether to execute the command in a terminal.
 * @argc     : return location for the number of items placed into @argv.
 * @argv     : return location for the argument vector.
 * @error    : return location for errors or %NULL.
 *
 * Substitutes <literal>Exec</literal> parameter variables according
 * to the <ulink href="http://freedesktop.org/wiki/Standards_2fdesktop_2dentry_2dspec"
 * type="http">Desktop Entry Specification</ulink> and returns the
 * parsed argument vector (in @argv) and the number of items placed
 * into @argv (in @argc).
 *
 * The @icon, @name and @path fields are optional and may be %NULL
 * if you don't know their values. The @icon parameter should be
 * the value of the <literal>Icon</literal> field from the desktop
 * file, the @name parameter should be the translated <literal>Name</literal>
 * value, while the @path parameter should refer to the full path
 * to the desktop file, whose <literal>Exec</literal> field is
 * being parsed here.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
_thunar_vfs_sysdep_parse_exec (const gchar *exec,
                               GList       *uris,
                               const gchar *icon,
                               const gchar *name,
                               const gchar *path,
                               gboolean     terminal,
                               gint        *argc,
                               gchar     ***argv,
                               GError     **error)
{
  const gchar *p;
  gboolean     result;
  GString     *command_line = g_string_new (NULL);
  GList       *lp;
  gchar       *uri_string;
  gchar       *quoted;

  /* prepend terminal command if required */
  if (G_UNLIKELY (terminal))
    {
      g_string_append (command_line, "Terminal ");
      if (G_LIKELY (name != NULL))
        {
          quoted = g_shell_quote (name);
          g_string_append (command_line, "-T ");
          g_string_append (command_line, quoted);
          g_free (quoted);
        }
      g_string_append (command_line, "-x ");
    }

  for (p = exec; *p != '\0'; ++p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          switch (*++p)
            {
            case 'f':
              if (G_LIKELY (uris != NULL))
                {
                  quoted = g_shell_quote (thunar_vfs_uri_get_path (uris->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'F':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  quoted = g_shell_quote (thunar_vfs_uri_get_path (lp->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'u':
              /* we need to hide the host parameter here, because there are quite a few
               * applications out there (namely GNOME applications), which cannot handle
               * 'file:'-URIs with host names.
               */
              if (G_LIKELY (uris != NULL))
                {
                  uri_string = thunar_vfs_uri_to_string (uris->data, THUNAR_VFS_URI_HIDE_HOST);
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'U':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  uri_string = thunar_vfs_uri_to_string (lp->data, THUNAR_VFS_URI_HIDE_HOST);
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'd':
              if (G_LIKELY (uris != NULL))
                {
                  uri_string = g_path_get_dirname (thunar_vfs_uri_get_path (uris->data));
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'D':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  uri_string = g_path_get_dirname (thunar_vfs_uri_get_path (lp->data));
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'n':
              if (G_LIKELY (uris != NULL))
                {
                  quoted = g_shell_quote (thunar_vfs_uri_get_name (uris->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'N':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  quoted = g_shell_quote (thunar_vfs_uri_get_name (lp->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'i':
              if (G_LIKELY (icon != NULL))
                {
                  quoted = g_shell_quote (icon);
                  g_string_append (command_line, "--icon ");
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'c':
              if (G_LIKELY (name != NULL))
                {
                  quoted = g_shell_quote (name);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'k':
              if (G_LIKELY (path != NULL))
                {
                  quoted = g_shell_quote (path);
                  g_string_append (command_line, path);
                  g_free (quoted);
                }
              break;

            case '%':
              g_string_append_c (command_line, '%');
              break;
            }
        }
      else
        {
          g_string_append_c (command_line, *p);
        }
    }

  result = g_shell_parse_argv (command_line->str, argc, argv, error);

  g_string_free (command_line, TRUE);

  return result;
}




