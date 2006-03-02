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

#include <thunar/thunar-gdk-pixbuf-extensions.h>



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




