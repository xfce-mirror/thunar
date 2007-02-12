/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_JPEGLIB_H
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#include<jpeglib.h>
#endif

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



static GdkPixbuf*
tvtj_jpeg_load (const JOCTET *content,
                gsize         length,
                gint          size)
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_source_mgr        source;
  TvtjErrorHandler              handler;
  guchar                       *lines[1];
  guchar                       *buffer = NULL;
  guchar                       *pixels = NULL;
  guchar                       *p;
  gint                          out_num_components;
  gint                          n;

  /* setup JPEG error handling */
  cinfo.err = jpeg_std_error (&handler.mgr);
  handler.mgr.error_exit = fatal_error_handler;
  handler.mgr.output_message = (gpointer) exo_noop;
  if (setjmp (handler.setjmp_buffer))
    goto error;

  /* setup the source */
  source.bytes_in_buffer = length;
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

  /* generate a pixbuf for the pixel data */
  return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
                                   (cinfo.out_color_components == 4), 8,
                                   cinfo.output_width, cinfo.output_height,
                                   cinfo.output_width * out_num_components,
                                   (GdkPixbufDestroyNotify) g_free, NULL);

error:
  jpeg_destroy_decompress (&cinfo);
  g_free (buffer);
  g_free (pixels);
  return NULL;
}



typedef struct
{
  const guchar *data_ptr;
  guint         data_len;

  guint         thumb_compression;
  union
  {
    struct /* thumbnail JPEG */
    {
      guint     length;
      guint     offset;
    } thumb_jpeg;
    struct /* thumbnail TIFF */
    {
      guint     length;
      guint     offset;
      guint     interp;
      guint     height;
      guint     width;
    } thumb_tiff;
  } thumb;

  gboolean      big_endian;
} TvtjExif;



static guint
tvtj_exif_get_ushort (const TvtjExif *exif,
                      gconstpointer   data)
{
  if (G_UNLIKELY (exif->big_endian))
    return GUINT16_FROM_BE (*((const guint16 *) data));
  else
    return GUINT16_FROM_LE (*((const guint16 *) data));
}



static guint
tvtj_exif_get_ulong (const TvtjExif *exif,
                     gconstpointer   data)
{
  if (G_UNLIKELY (exif->big_endian))
    return GUINT32_FROM_BE (*((const guint32 *) data));
  else
    return GUINT32_FROM_LE (*((const guint32 *) data));
}



static void
tvtj_exif_parse_ifd (TvtjExif     *exif,
                     const guchar *ifd_ptr,
                     guint         ifd_len)
{
  const guchar *subifd_ptr;
  guint         subifd_off;
  guint         value;
  guint         tag;
  guint         n;

  /* make sure we have a valid IFD here */
  if (G_UNLIKELY (ifd_len < 2))
    return;

  /* determine the number of entries */
  n = tvtj_exif_get_ushort (exif, ifd_ptr);

  /* advance to the IFD content */
  ifd_ptr += 2;
  ifd_len -= 2;

  /* validate the number of entries */
  if (G_UNLIKELY (n * 12 > ifd_len))
    n = ifd_len / 12;

  /* process all IFD entries */
  for (; n > 0; ifd_ptr += 12, --n)
    {
      /* determine the tag of this entry */
      tag = tvtj_exif_get_ushort (exif, ifd_ptr);
      if (tag == 0x8769 || tag == 0xa005)
        {
          /* check if we have a valid sub IFD offset here */
          subifd_off = tvtj_exif_get_ulong (exif, ifd_ptr + 8);
          subifd_ptr = exif->data_ptr + subifd_off;
          if (G_LIKELY (subifd_off < exif->data_len))
            {
              /* process the sub IFD recursively */
              tvtj_exif_parse_ifd (exif, subifd_ptr, exif->data_len - subifd_off);
            }
        }
      else if (tag == 0x0103)
        {
          /* verify that we have an ushort here (format 3) */
          if (tvtj_exif_get_ushort (exif, ifd_ptr + 2) == 3)
            {
              /* determine the thumbnail compression */
              exif->thumb_compression = tvtj_exif_get_ushort (exif, ifd_ptr + 8);
            }
        }
      else if (tag == 0x0100 || tag == 0x0101 || tag == 0x0106 || tag == 0x0111 || tag == 0x0117)
        {
          /* this can be either ushort or ulong */
          if (tvtj_exif_get_ushort (exif, ifd_ptr + 2) == 3)
            value = tvtj_exif_get_ushort (exif, ifd_ptr + 8);
          else if (tvtj_exif_get_ushort (exif, ifd_ptr + 2) == 4)
            value = tvtj_exif_get_ulong (exif, ifd_ptr + 8);
          else
            value = 0;

          /* and remember it appropriately */
          if (tag == 0x0100)
            exif->thumb.thumb_tiff.width = value;
          else if (tag == 0x0100)
            exif->thumb.thumb_tiff.height = value;
          else if (tag == 0x0106)
            exif->thumb.thumb_tiff.interp = value;
          else if (tag == 0x0111)
            exif->thumb.thumb_tiff.offset = value;
          else
            exif->thumb.thumb_tiff.length = value;
        }
      else if (tag == 0x0201 || tag == 0x0202)
        {
          /* verify that we have an ulong here (format 4) */
          if (tvtj_exif_get_ushort (exif, ifd_ptr + 2) == 4)
            {
              /* determine the value (thumbnail JPEG offset or length) */
              value = tvtj_exif_get_ulong (exif, ifd_ptr + 8);

              /* and remember it appropriately */
              if (G_LIKELY (tag == 0x201))
                exif->thumb.thumb_jpeg.offset = value;
              else
                exif->thumb.thumb_jpeg.length = value;
            }
        }
    }

  /* check for link to next IFD */
  subifd_off = tvtj_exif_get_ulong (exif, ifd_ptr);
  if (subifd_off != 0 && subifd_off < exif->data_len)
    {
      /* parse next IFD recursively as well */
      tvtj_exif_parse_ifd (exif, exif->data_ptr + subifd_off, exif->data_len - subifd_off);
    }
}



static GdkPixbuf*
tvtj_exif_extract_thumbnail (const guchar  *data,
                             guint          length,
                             gint           size)
{
  TvtjExif exif;
  guint    offset;

  /* make sure we have enough data */
  if (G_UNLIKELY (length < 6 + 8))
    return NULL;

  /* validate Exif header */
  if (memcmp (data, "Exif\0\0", 6) != 0)
    return NULL;

  /* advance to TIFF header */
  data += 6;
  length -= 6;

  /* setup Exif data struct */
  memset (&exif, 0, sizeof (exif));
  exif.data_ptr = data;
  exif.data_len = length;

  /* determine byte order */
  if (memcmp (data, "II", 2) == 0)
    exif.big_endian = FALSE;
  else if (memcmp (data, "MM", 2) == 0)
    exif.big_endian = TRUE;
  else
    return NULL;

  /* validate the TIFF header */
  if (tvtj_exif_get_ushort (&exif, data + 2) != 0x2a)
    return NULL;

  /* determine the first IFD offset */
  offset = tvtj_exif_get_ulong (&exif, data + 4);

  /* validate the offset */
  if (G_LIKELY (offset < length))
    {
      /* parse the first IFD (recursively parses the remaining...) */
      tvtj_exif_parse_ifd (&exif, data + offset, length - offset);

      /* check thumbnail compression type */
      if (G_LIKELY (exif.thumb_compression == 6)) /* JPEG */
        {
          /* check if we have a valid thumbnail JPEG */
          if (exif.thumb.thumb_jpeg.offset > 0 && exif.thumb.thumb_jpeg.length > 0
              && exif.thumb.thumb_jpeg.offset + exif.thumb.thumb_jpeg.length <= length)
            {
              /* try to load the embedded thumbnail JPEG */
              return tvtj_jpeg_load (data + exif.thumb.thumb_jpeg.offset, exif.thumb.thumb_jpeg.length, size);
            }
        }
      else if (exif.thumb_compression == 1) /* Uncompressed */
        {
          /* check if we have a valid thumbnail (current only RGB interpretations) */
          if (G_LIKELY (exif.thumb.thumb_tiff.interp == 2)
              && exif.thumb.thumb_tiff.offset > 0 && exif.thumb.thumb_tiff.length > 0
              && exif.thumb.thumb_tiff.offset + exif.thumb.thumb_tiff.length <= length
              && exif.thumb.thumb_tiff.height * exif.thumb.thumb_tiff.width == exif.thumb.thumb_tiff.length)
            {
              /* plain RGB data, just what we need for a GdkPixbuf */
              return gdk_pixbuf_new_from_data (g_memdup (data + exif.thumb.thumb_tiff.offset, exif.thumb.thumb_tiff.length),
                                               GDK_COLORSPACE_RGB, FALSE, 8, exif.thumb.thumb_tiff.width,
                                               exif.thumb.thumb_tiff.height, exif.thumb.thumb_tiff.width,
                                               (GdkPixbufDestroyNotify) g_free, NULL);
            }
        }
    }

  return NULL;
}



static GdkPixbuf*
tvtj_jpeg_load_thumbnail (const JOCTET *content,
                          gsize         length,
                          gint          size)
{
  guint marker_len;
  guint marker;
  gsize n;

  /* valid JPEG headers begin with SOI (Start Of Image) */
  if (G_LIKELY (length >= 2 && content[0] == 0xff && content[1] == 0xd8))
    {
      /* search for an EXIF marker */
      for (length -= 2, n = 2; n < length; )
        {
          /* check for valid marker start */
          if (G_UNLIKELY (content[n++] != 0xff))
            break;

          /* determine the next marker */
          marker = content[n];

          /* skip additional padding */
          if (G_UNLIKELY (marker == 0xff))
            continue;

          /* stop at SOS marker */
          if (marker == 0xda)
            break;

          /* advance */
          ++n;

          /* check if valid */
          if (G_UNLIKELY (n + 2 >= length))
            break;

          /* determine the marker length */
          marker_len = (content[n] << 8) | content[n + 1];

          /* check if we have an exif marker here */
          if (marker == 0xe1 && n + marker_len <= length)
            {
              /* try to extract the Exif thumbnail */
              return tvtj_exif_extract_thumbnail (content + n + 2, marker_len - 2, size);
            }

          /* try next one then */
          n += marker_len;
        }
    }

  return NULL;
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
  struct stat statb;
  GdkPixbuf  *pixbuf = NULL;
  JOCTET     *content;
  gint        fd;

  /* try to open the file at the given path */
  fd = open (path, O_RDONLY);
  if (G_LIKELY (fd >= 0))
    {
      /* determine the status of the file */
      if (G_LIKELY (fstat (fd, &statb) == 0 && statb.st_size > 0))
        {
          /* try to mmap the file */
          content = (JOCTET *) mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);

          /* verify that the mmap was successful */
          if (G_LIKELY (content != (JOCTET *) MAP_FAILED))
            {
              /* try to load an embedded thumbnail first */
              pixbuf = tvtj_jpeg_load_thumbnail (content, statb.st_size, size);
              if (G_LIKELY (pixbuf == NULL))
                {
                  /* fall back to loading and scaling the image itself */
                  pixbuf = tvtj_jpeg_load (content, statb.st_size, size);
                }
            }

          /* unmap the file content */
          munmap ((void *) content, statb.st_size);
        }

      /* close the file */
      close (fd);
    }

  return pixbuf;
#else
  return NULL;
#endif
}



#define __THUNAR_VFS_THUMB_JPEG_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
