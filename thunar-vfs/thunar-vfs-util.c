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

#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-alias.h>



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
  if (buffer == NULL)
    {
      buffer = g_new (gchar, 32);
      buflen = 32;
    }

  if (G_UNLIKELY (size > 1024ul * 1024ul * 1024ul))
    g_snprintf (buffer, buflen, "%0.1f G", size / (1024.0 * 1024.0 * 1024.0));
  else if (size > 1024ul * 1024ul)
    g_snprintf (buffer, buflen, "%0.1f M", size / (1024.0 * 1024.0));
  else if (size > 1024ul)
    g_snprintf (buffer, buflen, "%0.1f K", size / 1024.0);
  else
    g_snprintf (buffer, buflen, "%lu B", (gulong) size);

  return buffer;
}



#define __THUNAR_VFS_UTIL_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
