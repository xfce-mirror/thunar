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



