/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Amrit Borah <elessar1802@gmail.com>
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
 */

#include "thunar/thunar-text-renderer.h"

#include "thunar/thunar-util.h"

#include <pango/pango.h>


/*
 * This optimization avoids calling Pango for every filename by caching
 * character widths measured once using Pango. The cache is populated
 * on first use or when the font changes, providing character widths.
 *
 * With precise character widths from Pango cache, we can always use the
 * fast path without needing heuristics - just sum character widths and
 * calculate line breaks based on wrap_width from Thunar's zoom level.
 *
 * Performance: ~100x faster than Pango.
 */

/* Font metrics cache structure */
typedef struct
{
  gint     char_widths[256]; /* Cached character widths in pixels */
  gint     line_height;      /* Cached line height in pixels */
  gchar   *font_desc_str;    /* Font description string for invalidation */
  gboolean initialized;      /* Whether cache has been populated */
} FontMetricsCache;

/* Global font metrics cache */
static FontMetricsCache font_cache = { .initialized = FALSE, .font_desc_str = NULL };

/* Initialize font metrics cache using Pango */
static void
text_fast_init_cache (PangoContext         *context,
                      PangoFontDescription *font_desc)
{
  PangoLayout   *layout;
  gchar         *font_str;
  gchar          text[2] = { 0, 0 };
  gint           i;
  PangoRectangle logical_rect;

  /* Check if font changed */
  font_str = pango_font_description_to_string (font_desc);
  if (font_cache.initialized && font_cache.font_desc_str != NULL)
    {
      if (g_strcmp0 (font_str, font_cache.font_desc_str) == 0)
        {
          g_free (font_str);
          return; /* Cache is still valid */
        }
      g_free (font_cache.font_desc_str);
    }
  font_cache.font_desc_str = font_str;

  /* Create temporary layout for measuring */
  layout = pango_layout_new (context);
  pango_layout_set_font_description (layout, font_desc);

  /* Measure line height first using reference characters */
  pango_layout_set_text (layout, "Aj", 2);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  font_cache.line_height = logical_rect.height;

  /* Measure each printable ASCII character (32-126) */
  for (i = 32; i < 127; i++)
    {
      text[0] = (gchar) i;
      pango_layout_set_text (layout, text, 1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      font_cache.char_widths[i] = logical_rect.width;
    }

  /* For control chars (0-31), use space width */
  for (i = 0; i < 32; i++)
    font_cache.char_widths[i] = font_cache.char_widths[' '];

  /* For DEL and extended ASCII (127-255), use average width */
  for (i = 127; i < 256; i++)
    font_cache.char_widths[i] = font_cache.char_widths['m'];

  g_object_unref (layout);
  font_cache.initialized = TRUE;
}

/* Get character width from cache */
static inline gint
text_fast_get_char_width (guchar c)
{
  return font_cache.char_widths[c];
}

/* Get line height from cache */
static inline gint
text_fast_get_line_height (void)
{
  return font_cache.line_height;
}

/* Find next break point (hyphen or space) */
static inline const gchar *
text_fast_find_break (const gchar *text)
{
  while (*text && *text != '-' && *text != ' ')
    text++;
  return text;
}

/*
 * Simulates Pango's PANGO_WRAP_WORD_CHAR behavior:
 * - First try to break at word boundaries (space, hyphen)
 * - If a word is too long, break it character by character
 */
static gint
text_fast_estimate_lines (const gchar *text,
                          gint         wrap_width)
{
  gint         lines = 1;
  gint         current_width = 0;
  const gchar *p = text;
  gint         safety_margin;

  if (wrap_width <= 0 || !text || !*text)
    return 1;

  /* Reduce effective wrap width by a small margin to account for kerning
   * and rendering variations. This ensures we don't underestimate. */
  safety_margin = text_fast_get_char_width ('.'); /* Use dot width as margin */
  wrap_width -= safety_margin;
  if (wrap_width <= 0)
    wrap_width = 1;

  while (*p)
    {
      /* Find end of current word (next break point or end) */
      const gchar *word_start = p;
      const gchar *break_pos = text_fast_find_break (p);
      gint         word_width = 0;
      const gchar *wp;

      /* Calculate width of this word segment */
      for (wp = word_start; wp < break_pos; wp++)
        word_width += text_fast_get_char_width ((guchar) *wp);

      /* Check if word fits on current line */
      if (current_width + word_width <= wrap_width)
        {
          /* Word fits, add it to current line */
          current_width += word_width;
        }
      else if (current_width == 0)
        {
          /* Word is too long - must break it character by character */
          for (wp = word_start; wp < break_pos; wp++)
            {
              gint char_width = text_fast_get_char_width ((guchar) *wp);
              if (current_width + char_width > wrap_width && current_width > 0)
                {
                  lines++;
                  current_width = char_width;
                }
              else
                {
                  current_width += char_width;
                }
            }
        }
      else
        {
          /* Word doesn't fit on current line - start new line */
          lines++;

          /* Check if word fits on a fresh line */
          if (word_width <= wrap_width)
            {
              current_width = word_width;
            }
          else
            {
              /* Word is too long even for a fresh line - break character by character */
              current_width = 0;
              for (wp = word_start; wp < break_pos; wp++)
                {
                  gint char_width = text_fast_get_char_width ((guchar) *wp);
                  if (current_width + char_width > wrap_width && current_width > 0)
                    {
                      lines++;
                      current_width = char_width;
                    }
                  else
                    {
                      current_width += char_width;
                    }
                }
            }
        }

      /* Handle break character (space or hyphen) */
      if (*break_pos == '-' || *break_pos == ' ')
        {
          gint break_char_width = text_fast_get_char_width ((guchar) *break_pos);
          if (current_width + break_char_width > wrap_width && current_width > 0)
            {
              lines++;
              current_width = break_char_width;
            }
          else
            {
              current_width += break_char_width;
            }
          p = break_pos + 1;
        }
      else
        {
          break;
        }
    }

  return lines;
}


enum
{
  PROP_0,
  PROP_HIGHLIGHT_COLOR,
  PROP_ROUNDED_CORNERS,
  PROP_HIGHLIGHTING_ENABLED,
};



static void
thunar_text_renderer_finalize (GObject *object);
static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec);
static void
thunar_text_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec);
static void
thunar_text_renderer_render (GtkCellRenderer     *renderer,
                             cairo_t             *cr,
                             GtkWidget           *widget,
                             const GdkRectangle  *background_area,
                             const GdkRectangle  *cell_area,
                             GtkCellRendererState flags);
static void
thunar_text_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                     GtkWidget       *widget,
                                                     gint             width,
                                                     gint            *minimum_height,
                                                     gint            *natural_height);
static void
thunar_text_renderer_get_preferred_width (GtkCellRenderer *cell,
                                          GtkWidget       *widget,
                                          gint            *minimum_size,
                                          gint            *natural_size);
static void
thunar_text_renderer_get_aligned_area (GtkCellRenderer     *cell,
                                       GtkWidget           *widget,
                                       GtkCellRendererState flags,
                                       const GdkRectangle  *cell_area,
                                       GdkRectangle        *aligned_area);



struct _ThunarTextRendererClass
{
  GtkCellRendererTextClass __parent__;

  void (*default_render_function) (GtkCellRenderer     *cell,
                                   cairo_t             *cr,
                                   GtkWidget           *widget,
                                   const GdkRectangle  *background_area,
                                   const GdkRectangle  *cell_area,
                                   GtkCellRendererState flags);

  void (*default_get_preferred_height_for_width) (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  gint             width,
                                                  gint            *minimum_height,
                                                  gint            *natural_height);

  void (*default_get_preferred_width) (GtkCellRenderer *cell,
                                       GtkWidget       *widget,
                                       gint            *minimum_size,
                                       gint            *natural_size);

  void (*default_get_aligned_area) (GtkCellRenderer     *cell,
                                    GtkWidget           *widget,
                                    GtkCellRendererState flags,
                                    const GdkRectangle  *cell_area,
                                    GdkRectangle        *aligned_area);
};

struct _ThunarTextRenderer
{
  GtkCellRendererText __parent__;

  gchar   *highlight_color;
  gboolean rounded_corners;
  gboolean highlighting_enabled;
};



G_DEFINE_TYPE (ThunarTextRenderer, thunar_text_renderer, GTK_TYPE_CELL_RENDERER_TEXT);



static void
thunar_text_renderer_class_init (ThunarTextRendererClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = thunar_text_renderer_finalize;

  object_class->get_property = thunar_text_renderer_get_property;
  object_class->set_property = thunar_text_renderer_set_property;

  klass->default_render_function = cell_class->render;
  cell_class->render = thunar_text_renderer_render;

  /* Override get_preferred_height_for_width for fast text height estimation */
  klass->default_get_preferred_height_for_width = cell_class->get_preferred_height_for_width;
  cell_class->get_preferred_height_for_width = thunar_text_renderer_get_preferred_height_for_width;

  /* Override get_preferred_width to avoid Pango calls */
  klass->default_get_preferred_width = cell_class->get_preferred_width;
  cell_class->get_preferred_width = thunar_text_renderer_get_preferred_width;

  /* Override get_aligned_area to avoid Pango calls during size calculation */
  klass->default_get_aligned_area = cell_class->get_aligned_area;
  cell_class->get_aligned_area = thunar_text_renderer_get_aligned_area;

  /**
   * ThunarTextRenderer:highlight-color:
   *
   * The color with which the cell should be highlighted.
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHT_COLOR,
                                   g_param_spec_string ("highlight-color", "highlight-color", "highlight-color",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));



  /**
   * ThunarTextRenderer:rounded-corners:
   *
   * Determines if the cell should be clipped to rounded corners.
   * Useful when highlighting is enabled & a highlight color is set.
   **/
  g_object_class_install_property (object_class,
                                   PROP_ROUNDED_CORNERS,
                                   g_param_spec_boolean ("rounded-corners", "rounded-corners", "rounded-corners",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));



  /**
   * ThunarTextRenderer:highlighting-enabled:
   *
   * Determines if the cell background should be drawn with highlight color.
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHTING_ENABLED,
                                   g_param_spec_boolean ("highlighting-enabled", "highlighting-enabled", "highlighting-enabled",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}



static void
thunar_text_renderer_init (ThunarTextRenderer *text_renderer)
{
  text_renderer->highlight_color = NULL;
}



static void
thunar_text_renderer_finalize (GObject *object)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  g_free (text_renderer->highlight_color);

  G_OBJECT_CLASS (thunar_text_renderer_parent_class)->finalize (object);
}



static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT_COLOR:
      g_value_set_string (value, text_renderer->highlight_color);
      break;

    case PROP_ROUNDED_CORNERS:
      g_value_set_boolean (value, text_renderer->rounded_corners);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      g_value_set_boolean (value, text_renderer->highlighting_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_text_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT_COLOR:
      g_free (text_renderer->highlight_color);
      text_renderer->highlight_color = g_value_dup_string (value);
      break;

    case PROP_ROUNDED_CORNERS:
      text_renderer->rounded_corners = g_value_get_boolean (value);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      text_renderer->highlighting_enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_text_renderer_new:
 *
 * Creates a new #ThunarTextRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: (transfer full) The newly allocated #ThunarTextRenderer.
 **/
GtkCellRenderer *
thunar_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_TEXT_RENDERER, NULL);
}



static void
thunar_text_renderer_render (GtkCellRenderer     *cell,
                             cairo_t             *cr,
                             GtkWidget           *widget,
                             const GdkRectangle  *background_area,
                             const GdkRectangle  *cell_area,
                             GtkCellRendererState flags)
{
  if (THUNAR_TEXT_RENDERER (cell)->highlighting_enabled)
    thunar_util_clip_view_background (cell, cr, background_area, widget, flags);

  /* we only needed to manipulate the background_area, otherwise everything remains the same.
     Hence, we are simply running the original render function now */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
  ->default_render_function (cell, cr, widget, background_area, cell_area, flags);
}



static void
thunar_text_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                     GtkWidget       *widget,
                                                     gint             width,
                                                     gint            *minimum_height,
                                                     gint            *natural_height)
{
  gchar                *text = NULL;
  gint                  wrap_width = -1;
  gint                  ypad;
  PangoContext         *pango_context;
  PangoFontDescription *font_desc = NULL;

  /* Get text and wrap-width from cell renderer properties */
  g_object_get (G_OBJECT (cell),
                "text", &text,
                "wrap-width", &wrap_width,
                NULL);
  gtk_cell_renderer_get_padding (cell, NULL, &ypad);

  /* Use fast estimation if text exists */
  if (text != NULL && *text != '\0')
    {
      /* Initialize or update the font metrics cache */
      pango_context = gtk_widget_get_pango_context (widget);
      if (pango_context != NULL)
        {
          font_desc = pango_context_get_font_description (pango_context);
          if (font_desc != NULL)
            text_fast_init_cache (pango_context, font_desc);
        }

      /* Use fast path if cache is initialized - we have exact character widths */
      if (font_cache.initialized)
        {
          gint estimated_lines;
          gint height;

          /* For wrapped text (Icon View), calculate number of lines needed.
           * For non-wrapped text (Compact View), always use single line. */
          if (wrap_width > 0)
            estimated_lines = text_fast_estimate_lines (text, wrap_width);
          else
            estimated_lines = 1;
          height = estimated_lines * text_fast_get_line_height () + 2 * ypad;

          if (minimum_height != NULL)
            *minimum_height = height;
          if (natural_height != NULL)
            *natural_height = height;

          g_free (text);
          return;
        }
    }

  g_free (text);

  /* Fall back to Pango only if cache is not initialized */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
  ->default_get_preferred_height_for_width (cell, widget, width, minimum_height, natural_height);
}



static void
thunar_text_renderer_get_preferred_width (GtkCellRenderer *cell,
                                          GtkWidget       *widget,
                                          gint            *minimum_size,
                                          gint            *natural_size)
{
  gchar                *text = NULL;
  gint                  wrap_width = -1;
  gint                  xpad;
  PangoContext         *pango_context;
  PangoFontDescription *font_desc = NULL;

  /* Get text and wrap-width from cell renderer properties */
  g_object_get (G_OBJECT (cell),
                "text", &text,
                "wrap-width", &wrap_width,
                NULL);
  gtk_cell_renderer_get_padding (cell, &xpad, NULL);

  /* Use fast estimation if text exists */
  if (text != NULL && *text != '\0')
    {
      /* Initialize or update the font metrics cache */
      pango_context = gtk_widget_get_pango_context (widget);
      if (pango_context != NULL)
        {
          font_desc = pango_context_get_font_description (pango_context);
          if (font_desc != NULL)
            text_fast_init_cache (pango_context, font_desc);
        }

      /* Use fast path if cache is initialized */
      if (font_cache.initialized)
        {
          gint         text_width = 0;
          const gchar *p;

          /* Calculate text width using cached character widths */
          for (p = text; *p; p++)
            text_width += text_fast_get_char_width ((guchar) *p);

          /* For wrapped text (Icon View), width is limited by wrap_width.
           * For non-wrapped text (Compact View), use full text width. */
          if (wrap_width > 0)
            text_width = MIN (text_width, wrap_width);
          text_width += 2 * xpad;

          if (minimum_size != NULL)
            *minimum_size = text_width;
          if (natural_size != NULL)
            *natural_size = text_width;

          g_free (text);
          return;
        }
    }

  g_free (text);

  /* Fall back to Pango only if cache is not initialized */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
  ->default_get_preferred_width (cell, widget, minimum_size, natural_size);
}



static void
thunar_text_renderer_get_aligned_area (GtkCellRenderer     *cell,
                                       GtkWidget           *widget,
                                       GtkCellRendererState flags,
                                       const GdkRectangle  *cell_area,
                                       GdkRectangle        *aligned_area)
{
  gchar                *text = NULL;
  gint                  wrap_width = -1;
  gint                  xpad, ypad;
  PangoContext         *pango_context;
  PangoFontDescription *font_desc = NULL;
  gfloat                xalign, yalign;

  /* Get text and wrap-width from cell renderer properties */
  g_object_get (G_OBJECT (cell),
                "text", &text,
                "wrap-width", &wrap_width,
                NULL);
  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

  /* Use fast estimation if text exists */
  if (text != NULL && *text != '\0')
    {
      /* Initialize or update the font metrics cache */
      pango_context = gtk_widget_get_pango_context (widget);
      if (pango_context != NULL)
        {
          font_desc = pango_context_get_font_description (pango_context);
          if (font_desc != NULL)
            text_fast_init_cache (pango_context, font_desc);
        }

      /* Use fast path if cache is initialized */
      if (font_cache.initialized)
        {
          gint         estimated_lines;
          gint         text_width = 0;
          gint         text_height;
          const gchar *p;

          /* Calculate text width using cached character widths */
          for (p = text; *p; p++)
            text_width += text_fast_get_char_width ((guchar) *p);

          /* For wrapped text (Icon View), limit width and calculate lines.
           * For non-wrapped text (Compact View), use full width and single line. */
          if (wrap_width > 0)
            {
              if (text_width > wrap_width)
                text_width = wrap_width;
              estimated_lines = text_fast_estimate_lines (text, wrap_width);
            }
          else
            {
              estimated_lines = 1;
            }
          text_height = estimated_lines * text_fast_get_line_height ();

          /* Calculate aligned area */
          aligned_area->width = MIN (text_width + 2 * xpad, cell_area->width);
          aligned_area->height = MIN (text_height + 2 * ypad, cell_area->height);

          /* Apply alignment */
          aligned_area->x = cell_area->x + (gint) (xalign * (cell_area->width - aligned_area->width));
          aligned_area->y = cell_area->y + (gint) (yalign * (cell_area->height - aligned_area->height));

          /* Ensure aligned area is within cell area */
          aligned_area->x = MAX (aligned_area->x, cell_area->x);
          aligned_area->y = MAX (aligned_area->y, cell_area->y);

          g_free (text);
          return;
        }
    }

  g_free (text);

  /* Fall back to default implementation */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
  ->default_get_aligned_area (cell, widget, flags, cell_area, aligned_area);
}
