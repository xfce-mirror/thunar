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
 *
 * Based on code written by Alexander Larsson <alexl@redhat.com>
 * for libgnomeui.
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
#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_JPEGLIB_H
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#include<jpeglib.h>
#endif

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-thumb-jpeg.h>
#include <thunar-vfs/thunar-vfs-alias.h>



#if defined(HAVE_LIBJPEG) && defined(HAVE_MMAP)
typedef struct
{
  struct jpeg_error_mgr mgr;
  jmp_buf               setjmp_buffer;
} TvtjErrorHandler;



static void
fatal_error_handler (j_common_ptr cinfo)
{
  TvtjErrorHandler *handler = (TvtjErrorHandler *) cinfo->err;
  longjmp (handler->setjmp_buffer, 1);
}



static gboolean
tvtj_fill_input_buffer (j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr *source = cinfo->src;

  /* return a fake EOI marker so we will eventually terminate */
  if (G_LIKELY (source->bytes_in_buffer == 0))
    {
      static const JOCTET FAKE_EOI[2] =
      {
        (JOCTET) 0xff,
        (JOCTET) JPEG_EOI,
      };

      source->next_input_byte = FAKE_EOI;
      source->bytes_in_buffer = G_N_ELEMENTS (FAKE_EOI);
    }

  return TRUE;
}



static void
tvtj_skip_input_data (j_decompress_ptr cinfo,
                      glong            num_bytes)
{
  struct jpeg_source_mgr *source = cinfo->src;

  if (G_LIKELY (num_bytes > 0))
    {
      num_bytes = MIN (num_bytes, source->bytes_in_buffer);

      source->next_input_byte += num_bytes;
      source->bytes_in_buffer -= num_bytes;
    }
}



static inline gint
tvtj_denom (gint width,
            gint height,
            gint size)
{
  if (width > size * 8 && height > size * 8)
    return 8;
  else if (width > size * 4 && height > size * 4)
    return 4;
  else if (width > size * 2 && height > size * 2)
    return 2;
  else
    return 1;
}



static inline void
tvtj_convert_cmyk_to_rgb (j_decompress_ptr cinfo,
                          guchar          *line)
{
  guchar *p;
  gint    c, k, m, n, y;

  g_return_if_fail (cinfo != NULL);
  g_return_if_fail (cinfo->output_components == 4);
  g_return_if_fail (cinfo->out_color_space == JCS_CMYK);

  for (n = cinfo->output_width, p = line; n > 0; --n, p += 4)
    {
      c = p[0];
      m = p[1];
      y = p[2];
      k = p[3];

      if (cinfo->saw_Adobe_marker)
        {
          p[0] = k * c / 255;
          p[1] = k * m / 255;
          p[2] = k * y / 255;
        }
      else
        {
          p[0] = (255 - k) * (255 - c) / 255;
          p[1] = (255 - k) * (255 - m) / 255;
          p[2] = (255 - k) * (255 - y) / 255;
        }

      p[3] = 255;
    }
}
#endif


                        
/**
 * thunar_vfs_thumb_jpeg_load:
 * @path
 * @size
 *
 * Return value:
 **/
GdkPixbuf*
thunar_vfs_thumb_jpeg_load (const gchar *path,
                            gint         size)
{
#if defined(HAVE_LIBJPEG) && defined(HAVE_MMAP)
  struct jpeg_decompress_struct cinfo;
  struct jpeg_source_mgr        source;
  TvtjErrorHandler              handler;
  struct stat                   statb;
  JOCTET                       *content;
  guchar                       *lines[1];
  guchar                       *buffer = NULL;
  guchar                       *pixels = NULL;
  guchar                       *p;
  gint                          out_num_components;
  gint                          fd;
  gint                          n;

  /* try to open the file at the given path */
  fd = open (path, O_RDONLY);
  if (G_UNLIKELY (fd < 0))
    return NULL;

  /* determine the status of the file */
  if (G_UNLIKELY (fstat (fd, &statb) < 0 || statb.st_size == 0))
    {
      close (fd);
      return NULL;
    }

  /* try to mmap the file */
  content = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);

  /* we can safely close the file now */
  close (fd);

  /* verify that the mmap was successful */
  if (G_UNLIKELY (content == (JOCTET *) MAP_FAILED))
    return NULL;

  /* setup JPEG error handling */
  cinfo.err = jpeg_std_error (&handler.mgr);
  handler.mgr.error_exit = fatal_error_handler;
  handler.mgr.output_message = (gpointer) exo_noop;
  if (setjmp (handler.setjmp_buffer))
    goto error;

  /* setup the source */
  source.bytes_in_buffer = statb.st_size;
  source.next_input_byte = content;
  source.init_source = (gpointer) exo_noop;
  source.fill_input_buffer = tvtj_fill_input_buffer;
  source.skip_input_data = tvtj_skip_input_data;
  source.resync_to_restart = jpeg_resync_to_restart;
  source.term_source = (gpointer) exo_noop;

  /* setup the JPEG decompress struct */
  jpeg_create_decompress (&cinfo);
  cinfo.src = &source;

  /* read the JPEG header from the file */
  jpeg_read_header (&cinfo, TRUE);

  /* configure the JPEG decompress struct */
  cinfo.scale_num = 1;
  cinfo.scale_denom = tvtj_denom (cinfo.image_width, cinfo.image_height, size);
  cinfo.dct_method = JDCT_FASTEST;
  cinfo.do_fancy_upsampling = FALSE;

  /* calculate the output dimensions */
  jpeg_calc_output_dimensions (&cinfo);

  /* verify the JPEG color space */
  if (cinfo.out_color_space != JCS_GRAYSCALE
      && cinfo.out_color_space != JCS_CMYK
      && cinfo.out_color_space != JCS_RGB)
    {
      /* we don't support this color space */
      goto error;
    }

  /* start the decompression */
  jpeg_start_decompress (&cinfo);

  /* allocate the pixel buffer and extra space for grayscale data */
  if (G_LIKELY (cinfo.num_components != 1))
    {
      pixels = g_malloc (cinfo.output_width * cinfo.output_height * cinfo.num_components);
      out_num_components = cinfo.num_components;
      lines[0] = pixels;
    }
  else
    {
      pixels = g_malloc (cinfo.output_width * cinfo.output_height * 3);
      buffer = g_malloc (cinfo.output_width);
      out_num_components = 3;
      lines[0] = buffer;
    }

  /* process the JPEG data */
  for (p = pixels; cinfo.output_scanline < cinfo.output_height; )
    {
      jpeg_read_scanlines (&cinfo, lines, 1);

      /* convert the data to RGB */
      if (cinfo.num_components == 1)
        {
          for (n = 0; n < cinfo.output_width; ++n)
            {
              p[n * 3 + 0] = buffer[n];
              p[n * 3 + 1] = buffer[n];
              p[n * 3 + 2] = buffer[n];
            }
          p += cinfo.output_width * 3;
        }
      else
        {
          if (cinfo.out_color_space == JCS_CMYK)
            tvtj_convert_cmyk_to_rgb (&cinfo, lines[0]);
          lines[0] += cinfo.output_width * cinfo.num_components;
        }
    }

  /* release the grayscale buffer */
  g_free (buffer);
  buffer = NULL;

  /* finish the JPEG decompression */
  jpeg_finish_decompress (&cinfo);
  jpeg_destroy_decompress (&cinfo);

  /* unmap the file content */
  munmap (content, statb.st_size);

  /* generate a pixbuf for the pixel data */
  return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
                                   (cinfo.out_color_components == 4), 8,
                                   cinfo.output_width, cinfo.output_height,
                                   cinfo.output_width * out_num_components,
                                   (GdkPixbufDestroyNotify) g_free, NULL);

error:
  jpeg_destroy_decompress (&cinfo);
  munmap (content, statb.st_size);
  g_free (buffer);
  g_free (pixels);
#endif

  return NULL;
}



#define __THUNAR_VFS_THUMB_JPEG_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
