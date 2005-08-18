/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on code originally written by Eazel, Inc. for the eel utility
 * library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunarx/thunarx-gdk-pixbuf-extensions.h>



/**
 * thunarx_gdk_pixbuf_colorize:
 * @src   : the source #GdkPixbuf.
 * @color : the new color.
 *
 * Creates a new #GdkPixbuf based on @src, which is
 * colorized to @color.
 *
 * Return value: the colorized #GdkPixbuf.
 **/
GdkPixbuf*
thunarx_gdk_pixbuf_colorize (const GdkPixbuf *src,
                             const GdkColor  *color)
{
  GdkPixbuf *dest;
  gboolean   has_alpha;
  guchar    *original_pixels;
  guchar    *target_pixels;
  guchar    *pixdest;
  guchar    *pixsrc;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       red_value;
  gint       green_value;
  gint       blue_value;
  gint       i;
  gint       j;

  red_value = color->red / 255.0;
  green_value = color->green / 255.0;
  blue_value = color->blue / 255.0;

  has_alpha = gdk_pixbuf_get_has_alpha (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  original_pixels = gdk_pixbuf_get_pixels (src);

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), has_alpha, gdk_pixbuf_get_bits_per_sample (src), width, height);

  dst_row_stride = gdk_pixbuf_get_rowstride (dest);
  target_pixels = gdk_pixbuf_get_pixels (dest);
  
  for (i = 0; i < height; i++)
    {
      pixdest = target_pixels + i*dst_row_stride;
      pixsrc = original_pixels + i*src_row_stride;
      
      for (j = 0; j < width; j++)
        {
          *pixdest++ = (*pixsrc++ * red_value) >> 8;
          *pixdest++ = (*pixsrc++ * green_value) >> 8;
          *pixdest++ = (*pixsrc++ * blue_value) >> 8;
          
          if (has_alpha)
            *pixdest++ = *pixsrc++;
        }
    }

  return dest;
}



static guchar
lighten_channel (guchar cur_value)
{
  gint new_value = cur_value;

  new_value += 24 + (new_value >> 3);
  if (G_UNLIKELY (new_value > 255))
    new_value = 255;

  return (guchar) new_value;
}



/**
 * thunarx_gdk_pixbuf_spotlight:
 * @src : the source #GdkPixbuf.
 *
 * Creates a lightened version of @src, suitable for
 * prelit state display of icons.
 *
 * The caller is responsible to free the returned
 * pixbuf using #g_object_unref().
 *
 * Return value: the lightened version of @src.
 **/
GdkPixbuf*
thunarx_gdk_pixbuf_spotlight (const GdkPixbuf *src)
{
  GdkPixbuf *dest;
  gboolean   has_alpha;
  guchar    *original_pixels;
  guchar    *target_pixels;
  guchar    *pixdest;
  guchar    *pixsrc;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i;
  gint       j;

  has_alpha = gdk_pixbuf_get_has_alpha (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  original_pixels = gdk_pixbuf_get_pixels (src);

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), has_alpha, gdk_pixbuf_get_bits_per_sample (src), width, height);

  dst_row_stride = gdk_pixbuf_get_rowstride (dest);
  target_pixels = gdk_pixbuf_get_pixels (dest);
  
  for (i = 0; i < height; ++i)
    {
      pixdest = target_pixels + i * dst_row_stride;
      pixsrc = original_pixels + i * src_row_stride;
  
      for (j = 0; j < width; ++j)
        {
          *pixdest++ = lighten_channel (*pixsrc++);
          *pixdest++ = lighten_channel (*pixsrc++);
          *pixdest++ = lighten_channel (*pixsrc++);

          if (G_LIKELY (has_alpha))
            *pixdest++ = *pixsrc++;
        }
    }

  return dest;
}




