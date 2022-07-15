/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-util.h>



enum
{
  PROP_0,
  PROP_HIGHLIGHT,
  PROP_HIGHLIGHT_SET,
  PROP_BORDER_RADIUS,
  PROP_BORDER_RADIUS_SET
};



static void thunar_text_renderer_finalize       (GObject               *object);
static void thunar_text_renderer_get_property   (GObject               *object,
                                                 guint                  prop_id,
                                                 GValue                *value,
                                                 GParamSpec            *pspec);
static void thunar_text_renderer_set_property   (GObject               *object,
                                                 guint                  prop_id,
                                                 const GValue          *value,
                                                 GParamSpec            *pspec);
static void thunar_text_renderer_render         (GtkCellRenderer       *renderer,
                                                 cairo_t               *cr,
                                                 GtkWidget             *widget,
                                                 const GdkRectangle    *background_area,
                                                 const GdkRectangle    *cell_area,
                                                 GtkCellRendererState   flags);



struct _ThunarTextRendererClass
{
  GtkCellRendererTextClass __parent__;

  void (*render_func) (GtkCellRenderer      *cell,
                       cairo_t              *cr,
                       GtkWidget            *widget,
                       const GdkRectangle   *background_area,
                       const GdkRectangle   *cell_area,
                       GtkCellRendererState  flags);
};

struct _ThunarTextRenderer
{
  GtkCellRendererText  __parent__;

  gchar               *highlight;
  gboolean             highlight_set;
  gchar               *border_radius;
  gboolean             border_radius_set;
};



G_DEFINE_TYPE (ThunarTextRenderer, thunar_text_renderer, GTK_TYPE_CELL_RENDERER_TEXT);



static void
thunar_text_renderer_class_init (ThunarTextRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = thunar_text_renderer_finalize;

  object_class->get_property = thunar_text_renderer_get_property;
  object_class->set_property = thunar_text_renderer_set_property;

  klass->render_func = cell_class->render;
  cell_class->render = thunar_text_renderer_render;

  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHT,
                                   g_param_spec_string ("highlight", "highlight", "highlight",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHT_SET,
                                   g_param_spec_boolean ("highlight-set", "highlight-set", "highlight-set",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BORDER_RADIUS,
                                   g_param_spec_string ("border-radius", "border-radius", "border-radius",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BORDER_RADIUS_SET,
                                   g_param_spec_boolean ("border-radius-set", "border-radius-set", "border-radius-set",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_text_renderer_init (ThunarTextRenderer *text_renderer)
{
  /* nothing to init */
}



static void
thunar_text_renderer_finalize (GObject *object)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  g_free (celltext->highlight);
  g_free (celltext->border_radius);

  G_OBJECT_CLASS (thunar_text_renderer_parent_class)->finalize (object);
}



static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT:
      g_value_set_string (value, celltext->highlight);
      break;

    case PROP_HIGHLIGHT_SET:
      g_value_set_boolean (value, celltext->highlight_set);
      break;

    case PROP_BORDER_RADIUS:
      g_value_set_string (value, celltext->border_radius);
      break;

    case PROP_BORDER_RADIUS_SET:
      g_value_set_boolean (value, celltext->border_radius_set);
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
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT:
      if (G_UNLIKELY (celltext->highlight != NULL))
        g_free (celltext->highlight);
      celltext->highlight = g_value_dup_string (value);
      break;

    case PROP_HIGHLIGHT_SET:
      celltext->highlight_set = g_value_get_boolean (value);
      break;

    case PROP_BORDER_RADIUS:
      if (G_UNLIKELY (celltext->border_radius != NULL))
        g_free (celltext->border_radius);
      celltext->border_radius = g_value_dup_string (value);
      break;

    case PROP_BORDER_RADIUS_SET:
      celltext->border_radius_set = g_value_get_boolean (value);
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
 * Return value: the newly allocated #ThunarTextRenderer.
 **/
GtkCellRenderer*
thunar_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_TEXT_RENDERER, NULL);
}



static void
thunar_text_renderer_render (GtkCellRenderer      *cell,
                             cairo_t              *cr,
                             GtkWidget            *widget,
                             const GdkRectangle   *background_area,
                             const GdkRectangle   *cell_area,
                             GtkCellRendererState  flags)
{
  thunar_util_clip_view_background (cell, cr, background_area, widget, flags);

  /* we only needed to manipulate the background_area, otherwise everything remains the same.
     Hence, we are simply running the original render function now */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
    ->render_func (cell, cr, widget, background_area, cell_area, flags);
}
