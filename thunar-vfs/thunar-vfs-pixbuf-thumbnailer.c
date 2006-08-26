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

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <exo/exo.h>



int
main (int argc, char **argv)
{
  GdkPixbuf *pixbuf;
  GError    *err = NULL;
  glong      size;

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
      fprintf (stderr, "%s: Invalid size %ld.\n", argv[0], size);
      return EXIT_FAILURE;
    }

  /* initialize the GType system */
  g_type_init ();

  /* try to load the input image file */
  pixbuf = exo_gdk_pixbuf_new_from_file_at_max_size (argv[2], size, size, TRUE, &err);
  if (G_UNLIKELY (pixbuf == NULL))
    {
      fprintf (stderr, "%s: %s.\n", argv[0], err->message);
      return EXIT_FAILURE;
    }
  /* try to save to the target location */
  if (!gdk_pixbuf_save (pixbuf, argv[3], "png", &err, NULL))
    {
      fprintf (stderr, "%s: Failed to write file %s: %s\n", argv[0], argv[3], err->message);
      return EXIT_FAILURE;
    }

  /* we did it */
  return EXIT_SUCCESS;
}

