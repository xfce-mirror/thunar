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

/********************************************************************************
 * WHAT IS THIS?                                                                *
 *                                                                              *
 * Fast thumbnail generator for image formats supported by gdk-pixbuf. Uses     *
 * mmap() if available to offer fast loading of images up to 8MB. The generated *
 * thumbnail is stored into a local file, usually a temporary file, as speci-   *
 * fied by the 3rd parameter.                                                   *
 ********************************************************************************/

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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

/* use g_open() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#endif



static void
size_prepared (GdkPixbufLoader *loader,
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



int
main (int argc, char **argv)
{
  GdkPixbufLoader *loader;
  struct stat      statb;
  GdkPixbuf       *pixbuf;
  GError          *err = NULL;
  guchar          *buffer;
  gint             size;
  gint             fd;
  gint             n;

  /* check the command line parameters */
  if (G_UNLIKELY (argc != 4))
    {
      fprintf (stderr, "Usage: %s <size> <input-file> <output-file>\n", argv[0]);
      return EXIT_FAILURE;
    }

  /* determine the size */
  size = strtol (argv[1], NULL, 10);
  if (G_UNLIKELY (size < 1 || size > 256))
    {
      fprintf (stderr, "%s: Invalid size %d.\n", argv[0], size);
      return EXIT_FAILURE;
    }

  /* initialize the GType system */
  g_type_init ();

  /* try to open the input file */
  fd = g_open (argv[2], O_RDONLY, 0000);
  if (G_UNLIKELY (fd < 0))
    {
err0: fprintf (stderr, "%s: Unable to open %s: %s.\n", argv[0], argv[2], g_strerror (errno));
      return EXIT_FAILURE;
    }

  /* try to stat the input file */
  if (fstat (fd, &statb) < 0)
    goto err0;

  /* verify that we have a regular file here */
  if (!S_ISREG (statb.st_mode))
    {
      errno = EINVAL;
      goto err0;
    }

  /* try to allocate the loader */
  loader = gdk_pixbuf_loader_new ();

  /* connect the "size-prepared" callback */
  g_signal_connect (G_OBJECT (loader), "size-prepared", G_CALLBACK (size_prepared), GINT_TO_POINTER (size));

#ifdef HAVE_MMAP
  /* try to mmap() the file if it's smaller than 8MB */
  if (G_LIKELY (statb.st_size <= 8 * 1024 * 1024))
    {
      /* map the image file into memory */
      buffer = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
      if (G_UNLIKELY (buffer == MAP_FAILED))
        goto nommap;

      /* tell the kernel that we'll read sequentially */
#ifdef HAVE_POSIX_MADVISE
      posix_madvise (buffer, statb.st_size, POSIX_MADV_SEQUENTIAL);
#endif

      /* feed the data into the loader */
      if (!gdk_pixbuf_loader_write (loader, buffer, statb.st_size, &err))
        goto err1;

      /* unmap the file */
      munmap (buffer, statb.st_size);
    }
  else
    {
nommap:
#endif
      /* allocate a read buffer */
      buffer = g_newa (guchar, 8192);

      /* read the file content */
      for (;;)
        {
          /* read the next chunk */
          n = read (fd, buffer, 8192);
          if (G_UNLIKELY (n < 0))
            {
              fprintf (stderr, "%s: Failed to read file %s: %s\n", argv[0], argv[2], g_strerror (errno));
              return EXIT_FAILURE;
            }

          /* feed the data into the loader */
          if (!gdk_pixbuf_loader_write (loader, buffer, n, &err))
            {
err1:         fprintf (stderr, "%s: Failed to read file %s: %s\n", argv[0], argv[2], err->message);
              return EXIT_FAILURE;
            }
        }
#ifdef HAVE_MMAP
    }
#endif

  /* check if we succeed */
  if (!gdk_pixbuf_loader_close (loader, &err))
    goto err1;

  /* determine the pixbuf... */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  /* try to save to the target location */
  if (!gdk_pixbuf_save (pixbuf, argv[3], "png", &err, NULL))
    {
      fprintf (stderr, "%s: Failed to write file %s: %s\n", argv[0], argv[3], err->message);
      return EXIT_FAILURE;
    }

  /* we did it */
  return EXIT_SUCCESS;
}

