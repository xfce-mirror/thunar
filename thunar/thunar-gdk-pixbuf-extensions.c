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

#ifdef HAVE_MMINTRIN_H
#include <mmintrin.h>
#endif

#include <thunar/thunar-gdk-pixbuf-extensions.h>



/**
 * thunar_gdk_pixbuf_colorize:
 * @src   : the source #GdkPixbuf.
 * @color : the new color.
 *
 * Creates a new #GdkPixbuf based on @src, which is
 * colorized to @color.
 *
 * Return value: the colorized #GdkPixbuf.
 **/
GdkPixbuf*
thunar_gdk_pixbuf_colorize (const GdkPixbuf *src,
                            const GdkColor  *color)
{
  GdkPixbuf *dst;
  gboolean   has_alpha;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i;

  /* determine source parameters */
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  has_alpha = gdk_pixbuf_get_has_alpha (src);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), has_alpha, gdk_pixbuf_get_bits_per_sample (src), width, height);

  /* determine row strides on src/dst */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (src);

#if defined(__GNUC__) && defined(__MMX__)
  /* check if there's a good reason to use MMX */
  if (G_LIKELY (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0))
    {
      __m64 *pixdst = (__m64 *) gdk_pixbuf_get_pixels (dst);
      __m64 *pixsrc = (__m64 *) gdk_pixbuf_get_pixels (src);
      __m64  alpha_mask = _mm_set_pi8 (0xff, 0, 0, 0, 0xff, 0, 0, 0);
      __m64  color_factor = _mm_set_pi16 (0, color->blue, color->green, color->red);
      __m64  zero = _mm_setzero_si64 ();
      __m64  src, alpha, hi, lo;

      /* divide color components by 256 */
      color_factor = _mm_srli_pi16 (color_factor, 8);

      for (i = (width * height) >> 1; i > 0; --i)
        {
          /* read the source pixel */
          src = *pixsrc;

          /* remember the two alpha values */
          alpha = _mm_and_si64 (alpha_mask, src);

          /* extract the hi pixel */
          hi = _mm_unpackhi_pi8 (src, zero);
          hi = _mm_mullo_pi16 (hi, color_factor);

          /* extract the lo pixel */
          lo = _mm_unpacklo_pi8 (src, zero);
          lo = _mm_mullo_pi16 (lo, color_factor);

          /* prefetch the next two pixels */
          __builtin_prefetch (++pixsrc, 0, 1);

          /* divide by 256 */
          hi = _mm_srli_pi16 (hi, 8);
          lo = _mm_srli_pi16 (lo, 8);

          /* combine the 2 pixels again */
          src = _mm_packs_pu16 (lo, hi);

          /* write back the calculated color together with the alpha */
          *pixdst = _mm_or_si64 (alpha, src);

          /* advance the dest pointer */
          ++pixdst;
        }

      _mm_empty ();
    }
  else
#endif
    {
      guchar *dst_pixels = gdk_pixbuf_get_pixels (dst);
      guchar *src_pixels = gdk_pixbuf_get_pixels (src);
      guchar *pixdst;
      guchar *pixsrc;
      gint    red_value = color->red / 255.0;
      gint    green_value = color->green / 255.0;
      gint    blue_value = color->blue / 255.0;
      gint    j;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; j > 0; --j)
            {
              *pixdst++ = (*pixsrc++ * red_value) >> 8;
              *pixdst++ = (*pixsrc++ * green_value) >> 8;
              *pixdst++ = (*pixsrc++ * blue_value) >> 8;
              
              if (has_alpha)
                *pixdst++ = *pixsrc++;
            }
        }
    }

  return dst;
}



static void
draw_frame_row (GdkPixbuf *frame_image,
                gint       target_width,
                gint       source_width,
                gint       source_v_position,
                gint       dest_v_position,
                GdkPixbuf *result_pixbuf,
                gint       left_offset,
                gint       height)
{
  gint remaining_width;
  gint slab_width;
  gint h_offset;
  
  for (h_offset = 0, remaining_width = target_width; remaining_width > 0; h_offset += slab_width, remaining_width -= slab_width)
    {
      slab_width = remaining_width > source_width ? source_width : remaining_width;
      gdk_pixbuf_copy_area (frame_image, left_offset, source_v_position, slab_width, height, result_pixbuf, left_offset + h_offset, dest_v_position);
    }
}



static void
draw_frame_column (GdkPixbuf *frame_image,
                   gint       target_height,
                   gint       source_height,
                   gint       source_h_position,
                   gint       dest_h_position,
                   GdkPixbuf *result_pixbuf,
                   gint       top_offset,
                   gint       width)
{
  gint remaining_height;
  gint slab_height;
  gint v_offset;
  
  for (v_offset = 0, remaining_height = target_height; remaining_height > 0; v_offset += slab_height, remaining_height -= slab_height)
    {
      slab_height = remaining_height > source_height ? source_height : remaining_height;
      gdk_pixbuf_copy_area (frame_image, source_h_position, top_offset, width, slab_height, result_pixbuf, dest_h_position, top_offset + v_offset);
    }
}



/**
 * thunar_gdk_pixbuf_frame:
 * @src           : the source #GdkPixbuf.
 * @frame         : the frame #GdkPixbuf.
 * @left_offset   : the left frame offset.
 * @top_offset    : the top frame offset.
 * @right_offset  : the right frame offset.
 * @bottom_offset : the bottom frame offset.
 *
 * Embeds @src in @frame and returns the result as
 * new #GdkPixbuf.
 *
 * The caller is responsible to free the returned
 * #GdkPixbuf using g_object_unref().
 *
 * Return value: the framed version of @src.
 **/
GdkPixbuf*
thunar_gdk_pixbuf_frame (GdkPixbuf *src,
                         GdkPixbuf *frame,
                         gint       left_offset,
                         gint       top_offset,
                         gint       right_offset,
                         gint       bottom_offset)
{
  GdkPixbuf *dst;
  gint       dst_width;
  gint       dst_height;
  gint       frame_width;
  gint       frame_height;
  gint       src_width;
  gint       src_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (src), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (frame), NULL);

  src_width = gdk_pixbuf_get_width (src);
  src_height = gdk_pixbuf_get_height (src);

  frame_width = gdk_pixbuf_get_width (frame);
  frame_height = gdk_pixbuf_get_height (frame);

  dst_width = src_width + left_offset + right_offset;
  dst_height = src_height + top_offset + bottom_offset;

  dst = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dst_width, dst_height);

  /* fill the destination if the source has an alpha channel */
  if (G_UNLIKELY (gdk_pixbuf_get_has_alpha (src)))
    gdk_pixbuf_fill (dst, 0xffffffff);

  /* draw the left top cornder and top row */
  gdk_pixbuf_copy_area (frame, 0, 0, left_offset, top_offset, dst, 0, 0);
  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, 0, 0, dst, left_offset, top_offset);

  /* draw the right top corner and left column */
  gdk_pixbuf_copy_area (frame, frame_width - right_offset, 0, right_offset, top_offset, dst, dst_width - right_offset, 0);
  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, 0, 0, dst, top_offset, left_offset);

  /* draw the bottom right corner and bottom row */
  gdk_pixbuf_copy_area (frame, frame_width - right_offset, frame_height - bottom_offset, right_offset,
                        bottom_offset, dst, dst_width - right_offset, dst_height - bottom_offset);
  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, frame_height - bottom_offset,
                  dst_height - bottom_offset, dst, left_offset, bottom_offset);

  /* draw the bottom left corner and the right column */
  gdk_pixbuf_copy_area (frame, 0, frame_height - bottom_offset, left_offset, bottom_offset, dst, 0, dst_height - bottom_offset);
  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, frame_width - right_offset,
                     dst_width - right_offset, dst, top_offset, right_offset);

  /* copy the source pixbuf into the framed area */
  gdk_pixbuf_copy_area (src, 0, 0, src_width, src_height, dst, left_offset, top_offset);

  return dst;
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
 * thunar_gdk_pixbuf_spotlight:
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
thunar_gdk_pixbuf_spotlight (const GdkPixbuf *src)
{
  GdkPixbuf *dst;
  gboolean   has_alpha;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i;

  /* determine source parameters */
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  has_alpha = gdk_pixbuf_get_has_alpha (src);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), has_alpha, gdk_pixbuf_get_bits_per_sample (src), width, height);

  /* determine src/dst row strides */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  
#if defined(__GNUC__) && defined(__MMX__)
  /* check if there's a good reason to use MMX */
  if (G_LIKELY (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0))
    {
      __m64 *pixdst = (__m64 *) gdk_pixbuf_get_pixels (dst);
      __m64 *pixsrc = (__m64 *) gdk_pixbuf_get_pixels (src);
      __m64  alpha_mask = _mm_set_pi8 (0xff, 0, 0, 0, 0xff, 0, 0, 0);
      __m64  twentyfour = _mm_set_pi8 (0, 24, 24, 24, 0, 24, 24, 24);
      __m64  zero = _mm_setzero_si64 ();

      for (i = (width * height) >> 1; i > 0; --i)
        {
          /* read the source pixel */
          __m64 src = *pixsrc;

          /* remember the two alpha values */
          __m64 alpha = _mm_and_si64 (alpha_mask, src);

          /* extract the hi pixel */
          __m64 hi = _mm_unpackhi_pi8 (src, zero);

          /* extract the lo pixel */
          __m64 lo = _mm_unpacklo_pi8 (src, zero);

          /* add (x >> 3) to x */
          hi = _mm_adds_pu16 (hi, _mm_srli_pi16 (hi, 3));
          lo = _mm_adds_pu16 (lo, _mm_srli_pi16 (lo, 3));

          /* prefetch next value */
          __builtin_prefetch (++pixsrc, 0, 1);

          /* combine the two pixels again */
          src = _mm_packs_pu16 (lo, hi);

          /* add 24 (with saturation) */
          src = _mm_adds_pu8 (src, twentyfour);

          /* drop the alpha channel from the temp color */
          src = _mm_andnot_si64 (alpha_mask, src);

          /* write back the calculated color */
          *pixdst = _mm_or_si64 (alpha, src);

          /* advance the dest pointer */
          ++pixdst;
        }

      _mm_empty ();
    }
  else
#endif
    {
      guchar *dst_pixels = gdk_pixbuf_get_pixels (dst);
      guchar *src_pixels = gdk_pixbuf_get_pixels (src);
      guchar *pixdst;
      guchar *pixsrc;
      gint    j;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;
      
          for (j = width; j > 0; --j)
            {
              *pixdst++ = lighten_channel (*pixsrc++);
              *pixdst++ = lighten_channel (*pixsrc++);
              *pixdst++ = lighten_channel (*pixsrc++);

              if (G_LIKELY (has_alpha))
                *pixdst++ = *pixsrc++;
            }
        }
    }

  return dst;
}




