/* $Id: thunar-details-view-text-renderer.c 16446 2005-08-03 15:28:50Z benny $ */
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



static void thunar_details_view_text_renderer_class_init   (ThunarDetailsViewTextRendererClass *klass);
static void thunar_details_view_text_renderer_finalize     (GObject                            *object);
static void thunar_details_view_text_renderer_get_property (GObject                            *object,
                                                            guint                               prop_id,
                                                            GValue                             *value,
                                                            GParamSpec                         *pspec);
static void thunar_details_view_text_renderer_set_property (GObject                            *object,
                                                            guint                               prop_id,
                                                            const GValue                       *value,
                                                            GParamSpec                         *pspec);
static void thunar_details_view_text_renderer_get_size     (GtkCellRenderer                    *renderer,
                                                            GtkWidget                          *widget,
                                                            GdkRectangle                       *cell_area,
                                                            gint                               *x_offset,
                                                            gint                               *y_offset,
                                                            gint                               *width,
                                                            gint                               *height);
static void thunar_details_view_text_renderer_render       (GtkCellRenderer                    *renderer,
                                                            GdkWindow                          *window,
                                                            GtkWidget                          *widget,
                                                            GdkRectangle                       *background_area,
                                                            GdkRectangle                       *cell_area,
                                                            GdkRectangle                       *expose_area,
                                                            GtkCellRendererState                flags);
static void thunar_details_view_text_renderer_invalidate   (ThunarDetailsViewTextRenderer      *text_renderer);
static void thunar_details_view_text_renderer_set_widget   (ThunarDetailsViewTextRenderer      *text_renderer,
                                                            GtkWidget                          *widget);



struct _ThunarDetailsViewTextRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarDetailsViewTextRenderer
{
  GtkCellRenderer __parent__;

  PangoLayout *layout;
  GtkWidget   *widget;
  gchar        text[256];
  gint         char_width;
  gint         char_height;
};



static GObjectClass *thunar_details_view_text_renderer_parent_class;



GType
thunar_details_view_text_renderer_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarDetailsViewTextRendererClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_details_view_text_renderer_class_init,
        NULL,
        NULL,
        sizeof (ThunarDetailsViewTextRenderer),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "ThunarDetailsViewTextRenderer", &info, 0);
    }

  return type;
}



static void
thunar_details_view_text_renderer_class_init (ThunarDetailsViewTextRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  thunar_details_view_text_renderer_parent_class = g_type_class_peek_parent (klass);

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
thunar_details_view_text_renderer_finalize (GObject *object)
{
  ThunarDetailsViewTextRenderer *text_renderer = THUNAR_DETAILS_VIEW_TEXT_RENDERER (object);

  /* drop the cached widget */
  thunar_details_view_text_renderer_set_widget (text_renderer, NULL);

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
  gint                           text_length;
  gint                           text_width;
  gint                           text_height;

  /* setup the new widget */
  thunar_details_view_text_renderer_set_widget (text_renderer, widget);

  /* determine the text_length in characters */
  text_length = g_utf8_strlen (text_renderer->text, -1);

  /* calculate the appromixate text width/height */
  text_width = text_renderer->char_width * text_length;
  text_height = text_renderer->char_height;

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
  gint                           text_width;
  gint                           text_height;
  gint                           x_offset;
  gint                           y_offset;

  /* setup the new widget */
  thunar_details_view_text_renderer_set_widget (text_renderer, widget);

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

  pango_layout_set_text (text_renderer->layout, text_renderer->text, -1);

  /* calculate the real text dimension */
  pango_layout_get_pixel_size (text_renderer->layout, &text_width, &text_height);

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
                    text_renderer->layout);
}



static void
thunar_details_view_text_renderer_invalidate (ThunarDetailsViewTextRenderer *text_renderer)
{
  thunar_details_view_text_renderer_set_widget (text_renderer, NULL);
}



static void
thunar_details_view_text_renderer_set_widget (ThunarDetailsViewTextRenderer *text_renderer,
                                              GtkWidget                     *widget)
{
  // FIXME: The sample text should be translatable with a hint to translators!
  static const gchar SAMPLE_TEXT[] = "The Quick Brown Fox Jumps Over the Lazy Dog";
  PangoRectangle     extents;

  if (G_LIKELY (widget == text_renderer->widget))
    return;

  /* disconnect from the previously set widget */
  if (G_UNLIKELY (text_renderer->widget != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (text_renderer->widget), thunar_details_view_text_renderer_invalidate, text_renderer);
      g_object_unref (G_OBJECT (text_renderer->layout));
      g_object_unref (G_OBJECT (text_renderer->widget));
    }

  /* activate the new widget */
  text_renderer->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    {
      /* take a reference on the widget */
      g_object_ref (G_OBJECT (widget));

      /* we need to recalculate the metrics when a new style (and thereby a new font) is set */
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "destroy", G_CALLBACK (thunar_details_view_text_renderer_invalidate), text_renderer);
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "style-set", G_CALLBACK (thunar_details_view_text_renderer_invalidate), text_renderer);

      /* calculate the average character dimensions */
      text_renderer->layout = gtk_widget_create_pango_layout (widget, SAMPLE_TEXT);
      pango_layout_get_pixel_extents (text_renderer->layout, NULL, &extents);
      pango_layout_set_width (text_renderer->layout, -1);
      text_renderer->char_width = extents.width / g_utf8_strlen (SAMPLE_TEXT, sizeof (SAMPLE_TEXT) - 1);
      text_renderer->char_height = extents.height;
    }
  else
    {
      text_renderer->layout = NULL;
      text_renderer->char_width = 0;
      text_renderer->char_height = 0;
    }
}



/**
 * thunar_details_view_text_renderer_new:
 **/
GtkCellRenderer*
thunar_details_view_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_DETAILS_VIEW_TEXT_RENDERER, NULL);
}

