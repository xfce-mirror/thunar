/* $Id$ */
/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-thumb-pixbuf.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_open() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#endif



static void
tvtp_size_prepared (GdkPixbufLoader *loader,
                    gint             width,
                    gint             height,
                    gpointer         user_data)
{
  gdouble wratio;
  gdouble hratio;
  gint    size = GPOINTER_TO_INT (user_data);

  /* check if we need to scale down */
  if (G_LIKELY (width > size || height > size))
    {
      /* calculate the new dimensions */
      wratio = (gdouble) width / (gdouble) size;
      hratio = (gdouble) height / (gdouble) size;

      if (hratio > wratio)
        {
          width = rint (width / hratio);
          height = size;
        }
      else
        {
          width = size;
          height = rint (height / wratio);
        }

      /* apply the new dimensions */
      gdk_pixbuf_loader_set_size (loader, MAX (width, 1), MAX (height, 1));
    }
}



/**
 * thunar_vfs_thumb_pixbuf_load:
 * @path      : the path to the image file.
 * @mime_type : the mime type of the image file.
 * @size      : the desired size of the image.
 *
 * Loads the image from the @path, using the image loader
 * for the given @mime_type and tries to scale it down to
 * @size if the source image is larger than @size.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the loaded and scaled image or %NULL.
 **/
GdkPixbuf*
thunar_vfs_thumb_pixbuf_load (const gchar *path,
                              const gchar *mime_type,
                              gint         size)
{
  GdkPixbufLoader *loader;
  struct stat      statb;
  GdkPixbuf       *pixbuf = NULL;
  gboolean         succeed;
  guchar          *buffer;
  gint             fd;
  gint             n;

  /* try to allocate the loader */
  loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, NULL);
  if (G_LIKELY (loader == NULL))
    goto end0;

  /* try to open the file */
  fd = g_open (path, O_RDONLY, 0000);
  if (G_UNLIKELY (fd < 0))
    goto end1;

  /* verify that we have a regular file here */
  if (fstat (fd, &statb) < 0 || !S_ISREG (statb.st_mode))
    goto end2;

  /* connect the "size-prepared" callback */
  g_signal_connect (G_OBJECT (loader), "size-prepared", G_CALLBACK (tvtp_size_prepared), GINT_TO_POINTER (size));

#ifdef HAVE_MMAP
  /* try to mmap() the file if it's smaller than 1MB */
  if (G_LIKELY (statb.st_size <= 1024 * 1024))
    {
      /* map the image file into memory */
      buffer = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
      if (G_UNLIKELY (buffer == MAP_FAILED))
        goto nommap;

      /* feed the data into the loader */
      succeed = gdk_pixbuf_loader_write (loader, buffer, statb.st_size, NULL);

      /* unmap the file */
      munmap (buffer, statb.st_size);
    }
  else
    {
nommap:
#endif
      /* allocate a read buffer */
      buffer = g_newa (guchar, 4096);

      /* read the file content */
      for (;;)
        {
          /* read the next chunk */
          n = read (fd, buffer, 4096);
          if (G_UNLIKELY (n <= 0))
            {
              succeed = (n == 0);
              break;
            }

          /* feed the data into the loader */
          succeed = gdk_pixbuf_loader_write (loader, buffer, n, NULL);
          if (G_UNLIKELY (!succeed))
            break;
        }
#ifdef HAVE_MMAP
    }
#endif

  /* tell the loader that we're done */
  succeed = (succeed && gdk_pixbuf_loader_close (loader, NULL));

  /* check if we succeed */
  if (G_LIKELY (succeed))
    {
      /* determine the pixbuf... */
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

      /* ...and take a reference for the caller */
      if (G_LIKELY (pixbuf != NULL))
        g_object_ref (G_OBJECT (pixbuf));
    }

end2:
  close (fd);
end1:
  g_object_unref (G_OBJECT (loader));
end0:
  return pixbuf;
}



#define __THUNAR_VFS_THUMB_PIXBUF_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
