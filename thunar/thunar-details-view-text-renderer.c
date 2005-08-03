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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-details-view-text-renderer.h>



enum
{
  PROP_0,
  PROP_TEXT,
};



static void              thunar_details_view_text_renderer_class_init   (ThunarDetailsViewTextRendererClass *klass);
static void              thunar_details_view_text_renderer_init         (ThunarDetailsViewTextRenderer      *text_renderer);
static void              thunar_details_view_text_renderer_finalize     (GObject                            *object);
static void              thunar_details_view_text_renderer_get_property (GObject                            *object,
                                                                         guint                               prop_id,
                                                                         GValue                             *value,
                                                                         GParamSpec                         *pspec);
static void              thunar_details_view_text_renderer_set_property (GObject                            *object,
                                                                         guint                               prop_id,
                                                                         const GValue                       *value,
                                                                         GParamSpec                         *pspec);
static void              thunar_details_view_text_renderer_get_size     (GtkCellRenderer                    *renderer,
                                                                         GtkWidget                          *widget,
                                                                         GdkRectangle                       *cell_area,
                                                                         gint                               *x_offset,
                                                                         gint                               *y_offset,
                                                                         gint                               *width,
                                                                         gint                               *height);
static void              thunar_details_view_text_renderer_render       (GtkCellRenderer                    *renderer,
                                                                         GdkWindow                          *window,
                                                                         GtkWidget                          *widget,
                                                                         GdkRectangle                       *background_area,
                                                                         GdkRectangle                       *cell_area,
                                                                         GdkRectangle                       *expose_area,
                                                                         GtkCellRendererState                flags);
static PangoContext     *thunar_details_view_text_renderer_get_context  (ThunarDetailsViewTextRenderer      *text_renderer,
                                                                         GtkWidget                          *widget);
static PangoFontMetrics *thunar_details_view_text_renderer_get_metrics  (ThunarDetailsViewTextRenderer      *text_renderer,
                                                                         GtkWidget                          *widget);
static PangoLayout      *thunar_details_view_text_renderer_get_layout   (ThunarDetailsViewTextRenderer      *text_renderer,
                                                                         GtkWidget                          *widget);



struct _ThunarDetailsViewTextRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarDetailsViewTextRenderer
{
  GtkCellRenderer __parent__;

  PangoFontMetrics *metrics;
  PangoContext     *context;
  gchar             text[256];
};



G_DEFINE_TYPE (ThunarDetailsViewTextRenderer, thunar_details_view_text_renderer, GTK_TYPE_CELL_RENDERER);



static void
thunar_details_view_text_renderer_class_init (ThunarDetailsViewTextRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_details_view_text_renderer_finalize;
  gobject_class->get_property = thunar_details_view_text_renderer_get_property;
  gobject_class->set_property = thunar_details_view_text_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = thunar_details_view_text_renderer_get_size;
  gtkcell_renderer_class->render = thunar_details_view_text_renderer_render;

  /**
   * ThunarDetailsViewTextRenderer:text:
   *
   * The text to render.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        _("Text"),
                                                        _("The text to render"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_details_view_text_renderer_init (ThunarDetailsViewTextRenderer *text_renderer)
{
}



static void
thunar_details_view_text_renderer_finalize (GObject *object)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (object);

  /* drop the cached pango context/metrics */
  if (G_LIKELY (text_renderer->context != NULL))
    {
      g_object_unref (G_OBJECT (text_renderer->context));
      pango_font_metrics_unref (text_renderer->metrics);
    }

  G_OBJECT_CLASS (thunar_details_view_text_renderer_parent_class)->finalize (object);
}



static void
thunar_details_view_text_renderer_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, text_renderer->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_details_view_text_renderer_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (object);
  const gchar                   *sval;

  switch (prop_id)
    {
    case PROP_TEXT:
      sval = g_value_get_string (value);
      g_strlcpy (text_renderer->text, G_UNLIKELY (sval == NULL) ? "" : sval, sizeof (text_renderer->text));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_details_view_text_renderer_get_size (GtkCellRenderer *renderer,
                                            GtkWidget       *widget,
                                            GdkRectangle    *cell_area,
                                            gint            *x_offset,
                                            gint            *y_offset,
                                            gint            *width,
                                            gint            *height)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (renderer);
  PangoFontMetrics              *metrics;
  gint                           char_width;
  gint                           char_height;
  gint                           text_length;
  gint                           text_width;
  gint                           text_height;

  /* calculate the character dimensions from the font-metrics */
  metrics = thunar_details_view_text_renderer_get_metrics (text_renderer, widget);
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  char_height = pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics);

  /* determine the text_length in characters */
  text_length = g_utf8_strlen (text_renderer->text, -1);

  /* calculate the appromixate text width/height */
  text_width = PANGO_PIXELS (char_width) * text_length;
  text_height = PANGO_PIXELS (char_height);

  /* update width/height */
  if (G_LIKELY (width != NULL))
    *width = text_width + 2 * renderer->xpad;
  if (G_LIKELY (height != NULL))
    *height = text_height + 2 * renderer->ypad;

  /* update the x/y offsets */
  if (G_LIKELY (cell_area != NULL))
    {
      if (G_LIKELY (x_offset != NULL))
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
                    * (cell_area->width - text_width - (2 * renderer->xpad));
          *x_offset = MAX (*x_offset, 0);
        }

      if (G_LIKELY (y_offset != NULL))
        {
          *y_offset = renderer->yalign * (cell_area->height - text_height - (2 * renderer->ypad));
          *y_offset = MAX (*y_offset, 0);
        }
    }
}



static void
thunar_details_view_text_renderer_render (GtkCellRenderer     *renderer,
                                          GdkWindow           *window,
                                          GtkWidget           *widget,
                                          GdkRectangle        *background_area,
                                          GdkRectangle        *cell_area,
                                          GdkRectangle        *expose_area,
                                          GtkCellRendererState flags)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (renderer);
  GtkStateType                   state;
  PangoLayout                   *layout;
  gint                           text_width;
  gint                           text_height;
  gint                           x_offset;
  gint                           y_offset;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
        state = GTK_STATE_SELECTED;
      else
        state = GTK_STATE_ACTIVE;
    }
  else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT
        && GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT)
    {
      state = GTK_STATE_PRELIGHT;
    }
  else
    {
      if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
        state = GTK_STATE_INSENSITIVE;
      else
        state = GTK_STATE_NORMAL;
    }

  layout = thunar_details_view_text_renderer_get_layout (text_renderer, widget);

  /* calculate the real text dimension */
  pango_layout_get_pixel_size (layout, &text_width, &text_height);

  /* calculate the real x-offset */
  x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
           * (cell_area->width - text_width - (2 * renderer->xpad));
  x_offset = MAX (x_offset, 0);

  /* calculate the real y-offset */
  y_offset = renderer->yalign * (cell_area->height - text_height - (2 * renderer->ypad));
  y_offset = MAX (y_offset, 0);

  gtk_paint_layout (widget->style, window, state, TRUE,
                    expose_area, widget, "cellrenderertext",
                    cell_area->x + x_offset + renderer->xpad,
                    cell_area->y + y_offset + renderer->ypad,
                    layout);

  g_object_unref (G_OBJECT (layout));
}



static PangoContext*
thunar_details_view_text_renderer_get_context (ThunarDetailsViewTextRenderer *text_renderer,
                                               GtkWidget                     *widget)
{
  PangoContext *context;

  context = gtk_widget_get_pango_context (widget);
  if (G_UNLIKELY (text_renderer->context != context))
    {
      /* drop any previously cached context/metrics */
      if (G_LIKELY (text_renderer->context != NULL))
        {
          g_object_unref (G_OBJECT (text_renderer->context));
          pango_font_metrics_unref (text_renderer->metrics);
          text_renderer->metrics = NULL;
        }

      text_renderer->context = g_object_ref (G_OBJECT (context));
    }

  return text_renderer->context;
}



static PangoFontMetrics*
thunar_details_view_text_renderer_get_metrics (ThunarDetailsViewTextRenderer *text_renderer,
                                               GtkWidget                     *widget)
{
  PangoContext *context;

  if (G_UNLIKELY (text_renderer->metrics == NULL))
    {
      context = thunar_details_view_text_renderer_get_context (text_renderer, widget);
      text_renderer->metrics = pango_context_get_metrics (context, widget->style->font_desc, NULL);
    }

  return text_renderer->metrics;
}



static PangoLayout*
thunar_details_view_text_renderer_get_layout (ThunarDetailsViewTextRenderer *text_renderer,
                                              GtkWidget                     *widget)
{
  PangoContext *context;
  PangoLayout  *layout;

  context = thunar_details_view_text_renderer_get_context (text_renderer, widget);
  layout = pango_layout_new (context);
  pango_layout_set_width (layout, -1);
  pango_layout_set_text (layout, text_renderer->text, -1);

  return layout;
}



/**
 * thunar_details_view_text_renderer_new:
 **/
GtkCellRenderer*
thunar_details_view_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_DETAILS_VIEW_TEXT_RENDERER, NULL);
}

