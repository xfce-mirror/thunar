/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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
 * Based on code written by James Henstridge <james@daa.com.au> for GNOME.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <ft2build.h>
#include FT_FREETYPE_H



/* --- globals --- */
static gchar *opt_input = NULL;
static gchar *opt_output = NULL;
static gint   opt_size = 64;



/* --- command line options --- */
static GOptionEntry entries[] =
{
  { "input", 'i', 0, G_OPTION_ARG_FILENAME, &opt_input, "Name of file for which to create a thumbnail", "filename", },
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_output, "Name of the file to put the thumbnail", "filename", },
  { "size", 's', 0, G_OPTION_ARG_INT, &opt_size, "Size of thumbnail in pixels; the thumbnail will be at most N*N pixels large", "N", },
  { NULL, },
};



/* --- functions --- */
static const gchar*
tft_ft_strerror (FT_Error error)
{
#undef __FTERRORS_H__
#define FT_ERRORDEF(e,v,s) case e: return s;
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST
    switch (error)
      {
#include FT_ERRORS_H
      default:
        return "unknown";
      }
}




static void
tft_render_glyph (GdkPixbuf *pixbuf,
                  FT_Face    face,
                  FT_UInt    glyph,
                  gint      *pen_x,
                  gint      *pen_y)
{
  FT_GlyphSlot slot = face->glyph;
  FT_Error     error;
  guchar      *pixels;
  guchar       pixel;
  gint         rowstride;
  gint         height;
  gint         width;
  gint         off_x;
  gint         off_y;
  gint         off;
  gint         i, j;

  /* load the glyph */
  error = FT_Load_Glyph (face, glyph, FT_LOAD_DEFAULT);
  if (G_UNLIKELY (error != 0))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not load glyph", tft_ft_strerror (error));
      exit (EXIT_FAILURE);
    }

  /* render the glyph */
  error = FT_Render_Glyph (slot, ft_render_mode_normal);
  if (G_UNLIKELY (error != 0))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not render glyph", tft_ft_strerror (error));
      exit (EXIT_FAILURE);
    }

  off_x = *pen_x + slot->bitmap_left;
  off_y = *pen_y - slot->bitmap_top;

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  for (j = 0; j < slot->bitmap.rows; ++j)
    {
      if (j + off_y < 0 || j + off_y >= height)
        continue;

      for (i = 0; i < slot->bitmap.width; ++i)
        {
          if (i + off_x < 0 || i + off_x >= width)
            continue;

          switch (slot->bitmap.pixel_mode)
            {
            case ft_pixel_mode_mono:
              pixel = slot->bitmap.buffer[j * slot->bitmap.pitch + i / 8];
              pixel = 255 - ((pixel >> (7 - i % 8)) & 0x1) * 255;
              break;

            case ft_pixel_mode_grays:
              pixel = 255 - slot->bitmap.buffer[j * slot->bitmap.pitch + i];
              break;

            default:
              pixel = 255;
              break;
            }

          off = (j + off_y) * rowstride + 3 * (i + off_x);
          pixels[off + 0] = pixel;
          pixels[off + 1] = pixel;
          pixels[off + 2] = pixel;
        }
    }

  *pen_x += slot->advance.x >> 6;
}



static GdkPixbuf*
tft_scale_ratio (GdkPixbuf *source,
                 gint       dest_size)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;
  gint    dest_width;
  gint    dest_height;

  source_width  = gdk_pixbuf_get_width  (source);
  source_height = gdk_pixbuf_get_height (source);

  wratio = (gdouble) source_width  / (gdouble) dest_size;
  hratio = (gdouble) source_height / (gdouble) dest_size;

  if (hratio > wratio)
    {
      dest_width  = rint (source_width / hratio);
      dest_height = dest_size;
    }
  else
    {
      dest_width  = dest_size;
      dest_height = rint (source_height / wratio);
    }

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_HYPER);
}



static gboolean
tft_save_pixbuf (GdkPixbuf *pixbuf,
                 GError   **error)
{
  GdkPixbuf *subpixbuf;
  GdkPixbuf *scaled;
  gboolean   seen_pixel;
  gboolean   succeed;
  guchar    *pixels;
  gint       rowstride;
  gint       height;
  gint       width;
  gint       i, j;
  gint       trim_left;
  gint       trim_right;
  gint       trim_top;
  gint       trim_bottom;
  gint       offset;

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  for (i = 0; i < width; ++i)
    {
      seen_pixel = FALSE;
      for (j = 0; j < height; ++j)
        {
          offset = j * rowstride + 3 * i;
          seen_pixel = (pixels[offset + 0] != 0xff ||
                        pixels[offset + 1] != 0xff ||
                        pixels[offset + 2] != 0xff);
          if (seen_pixel)
            break;
        }
      
      if (seen_pixel)
        break;
    }

  trim_left = MIN (width, i);
  trim_left = MAX (trim_left - 8, 0);

  for (i = width - 1; i >= trim_left; --i)
    {
      seen_pixel = FALSE;
      for (j = 0; j < height; ++j)
        {
          offset = j * rowstride + 3 * i;
          seen_pixel = (pixels[offset + 0] != 0xff ||
                        pixels[offset+1] != 0xff ||
                        pixels[offset+2] != 0xff);
          if (seen_pixel)
            break;
        }
      
      if (seen_pixel)
        break;
    }

  trim_right = MAX (trim_left, i);
  trim_right = MIN (trim_right + 8, width - 1);

  for (j = 0; j < height; ++j)
    {
      seen_pixel = FALSE;
      for (i = 0; i < width; ++i)
        {
          offset = j * rowstride + 3 * i;
          seen_pixel = (pixels[offset + 0] != 0xff ||
                        pixels[offset + 1] != 0xff ||
                        pixels[offset + 2] != 0xff);
          if (seen_pixel)
            break;
        }
      
      if (seen_pixel)
        break;
    }

  trim_top = MIN (height, j);
  trim_top = MAX (trim_top - 8, 0);

  for (j = height - 1; j >= trim_top; --j)
    {
      seen_pixel = FALSE;
      for (i = 0; i < width; ++i)
        {
          offset = j * rowstride + 3 * i;
          seen_pixel = (pixels[offset + 0] != 0xff ||
                        pixels[offset + 1] != 0xff ||
                        pixels[offset + 2] != 0xff);
          if (seen_pixel)
            break;
        }
      
      if (seen_pixel)
        break;
    }

  trim_bottom = MAX (trim_top, j);
  trim_bottom = MIN (trim_bottom + 8, height - 1);

  /* determine the trimmed subpixbuf */
  subpixbuf = gdk_pixbuf_new_subpixbuf (pixbuf, trim_left, trim_top, trim_right - trim_left, trim_bottom - trim_top);

  /* check if we still need to scale down */
  if (gdk_pixbuf_get_width (subpixbuf) > opt_size || gdk_pixbuf_get_height (subpixbuf) > opt_size)
    {
      scaled = tft_scale_ratio (subpixbuf, opt_size);
      g_object_unref (G_OBJECT (subpixbuf));
      subpixbuf = scaled;
    }
  
  succeed = gdk_pixbuf_save (subpixbuf, opt_output, "png", error, NULL);
  g_object_unref (G_OBJECT (subpixbuf));

  return succeed;
}



int
main (int argc, char **argv)
{
  GOptionContext *context;
  FT_Library      library;
  GdkPixbuf      *pixbuf;
  FT_Error        error;
  FT_UInt         glyph1;
  FT_UInt         glyph2;
  FT_Face         face;
  GError         *err = NULL;
  gint            pen_x;
  gint            pen_y;
  gint            n;

  /* parse the command line options */
  context = g_option_context_new ("- Create font thumbnails");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_help_enabled (context, TRUE);
  if (!g_option_context_parse (context, &argc, &argv, &err))
    {
      g_print ("%s: %s\n", g_get_prgname (), err->message);
      return EXIT_FAILURE;
    }

  /* verify that an input file was specified */
  if (G_UNLIKELY (opt_input == NULL))
    {
      g_print ("%s: %s\n", g_get_prgname (), "No input file specified");
      return EXIT_FAILURE;
    }

  /* verify that an output file was specified */
  if (G_UNLIKELY (opt_output == NULL))
    {
      g_print ("%s: %s\n", g_get_prgname (), "No output file specified");
      return EXIT_FAILURE;
    }

  /* verify the specified size */
  if (G_UNLIKELY (opt_size < 1))
    {
      g_print ("%s: %s\n", g_get_prgname (), "The specified size is invalid");
      return EXIT_FAILURE;
    }

  /* initialize the freetype library */
  error = FT_Init_FreeType (&library);
  if (G_UNLIKELY (error != 0))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not initialize freetype", tft_ft_strerror (error));
      return EXIT_FAILURE;
    }

  /* try to open the input file */
  error = FT_New_Face (library, opt_input, 0, &face);
  if (G_UNLIKELY (error != 0))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not open the input file", tft_ft_strerror (error));
      return EXIT_FAILURE;
    }

  /* set the pixel size */
  error = FT_Set_Pixel_Sizes (face, 0, opt_size);
  if (G_UNLIKELY (error != 0))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not set the pixel size", tft_ft_strerror (error));
      return EXIT_FAILURE;
    }

  /* set the character map */
  for (n = 0; n < face->num_charmaps; ++n)
    {
      /* check for a desired character map */
      if (face->charmaps[n]->encoding == ft_encoding_latin_1
          || face->charmaps[n]->encoding == ft_encoding_unicode
          || face->charmaps[n]->encoding == ft_encoding_apple_roman)
        {
          /* try to set the character map */
          error = FT_Set_Charmap (face, face->charmaps[n]);
          if (G_UNLIKELY (error != 0))
            {
              g_print ("%s: %s: %s\n", g_get_prgname (), "Could not set the character map", tft_ft_strerror (error));
              return EXIT_FAILURE;
            }
          break;
        }
    }

  /* determine preferred glyphs for the thumbnail (with appropriate fallbacks) */
  glyph1 = FT_Get_Char_Index (face, 'A');
  if (G_UNLIKELY (glyph1 == 0))
    glyph1 = MIN (65, face->num_glyphs - 1);
  glyph2 = FT_Get_Char_Index (face, 'a');
  if (G_UNLIKELY (glyph2 == 0))
    glyph2 = MIN (97, face->num_glyphs - 1);

  /* initialize the GType system */
  g_type_init ();

  /* allocate a pixbuf for the thumbnail */
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, opt_size * 3, (opt_size * 3) / 2);
  gdk_pixbuf_fill (pixbuf, 0xffffffff);

  /* initial pen position */
  pen_x = opt_size / 2;
  pen_y = opt_size;

  /* render the letters to the pixbuf */
  tft_render_glyph (pixbuf, face, glyph1, &pen_x, &pen_y);
  tft_render_glyph (pixbuf, face, glyph2, &pen_x, &pen_y);

  /* save the pixbuf */
  if (!tft_save_pixbuf (pixbuf, &err))
    {
      g_print ("%s: %s: %s\n", g_get_prgname (), "Could not save thumbnail", err->message);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
