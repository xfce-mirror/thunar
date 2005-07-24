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

#include <thunar/thunar-details-view-icon-renderer.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-icon-factory.h>



enum
{
  PROP_0,
  PROP_FILE,
  PROP_SIZE,
};



static void thunar_details_view_icon_renderer_class_init    (ThunarDetailsViewIconRendererClass *klass);
static void thunar_details_view_icon_renderer_init          (ThunarDetailsViewIconRenderer      *icon_renderer);
static void thunar_details_view_icon_renderer_finalize      (GObject                            *object);
static void thunar_details_view_icon_renderer_get_property  (GObject                            *object,
                                                             guint                               prop_id,
                                                             GValue                             *value,
                                                             GParamSpec                         *pspec);
static void thunar_details_view_icon_renderer_set_property  (GObject                            *object,
                                                             guint                               prop_id,
                                                             const GValue                       *value,
                                                             GParamSpec                         *pspec);
static void thunar_details_view_icon_renderer_get_size      (GtkCellRenderer                    *renderer,
                                                             GtkWidget                          *widget,
                                                             GdkRectangle                       *rectangle,
                                                             gint                               *x_offset,
                                                             gint                               *y_offset,
                                                             gint                               *width,
                                                             gint                               *height);
static void thunar_details_view_icon_renderer_render        (GtkCellRenderer                    *renderer,
                                                             GdkWindow                          *window,
                                                             GtkWidget                          *widget,
                                                             GdkRectangle                       *background_area,
                                                             GdkRectangle                       *cell_area,
                                                             GdkRectangle                       *expose_area,
                                                             GtkCellRendererState                flags);



struct _ThunarDetailsViewIconRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarDetailsViewIconRenderer
{
  GtkCellRenderer __parent__;

  ThunarFile *file;
  gint        size;
};



G_DEFINE_TYPE (ThunarDetailsViewIconRenderer, thunar_details_view_icon_renderer, GTK_TYPE_CELL_RENDERER);



static void
thunar_details_view_icon_renderer_class_init (ThunarDetailsViewIconRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_details_view_icon_renderer_finalize;
  gobject_class->get_property = thunar_details_view_icon_renderer_get_property;
  gobject_class->set_property = thunar_details_view_icon_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = thunar_details_view_icon_renderer_get_size;
  gtkcell_renderer_class->render = thunar_details_view_icon_renderer_render;

  /**
   * ThunarDetailsViewIconRenderer:file:
   *
   * The file whose icon to render.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        _("File"),
                                                        _("The file whose icon to render"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarDetailsViewIconRenderer:icon-size:
   *
   * The icon size in pixels.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     _("Icon size"),
                                                     _("The icon size in pixels"),
                                                     1, G_MAXINT, 24,
                                                     EXO_PARAM_READWRITE));
}



static void
thunar_details_view_icon_renderer_init (ThunarDetailsViewIconRenderer *icon_renderer)
{
  icon_renderer->size = 24;
}



static void
thunar_details_view_icon_renderer_finalize (GObject *object)
{
  ThunarDetailsViewIconRenderer *icon_renderer = THUNAR_DETAILS_VIEW_ICON_RENDERER (object);

  /* free the icon data */
  if (G_LIKELY (icon_renderer->file != NULL))
    g_object_unref (G_OBJECT (icon_renderer->file));

  G_OBJECT_CLASS (thunar_details_view_icon_renderer_parent_class)->finalize (object);
}



static void
thunar_details_view_icon_renderer_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  ThunarDetailsViewIconRenderer *icon_renderer = THUNAR_DETAILS_VIEW_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, icon_renderer->file);
      break;

    case PROP_SIZE:
      g_value_set_int (value, icon_renderer->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_details_view_icon_renderer_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  ThunarDetailsViewIconRenderer *icon_renderer = THUNAR_DETAILS_VIEW_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      if (G_LIKELY (icon_renderer->file != NULL))
        g_object_unref (G_OBJECT (icon_renderer->file));
      icon_renderer->file = g_value_get_object (value);
      if (G_LIKELY (icon_renderer->file != NULL))
        g_object_ref (G_OBJECT (icon_renderer->file));
      break;

    case PROP_SIZE:
      icon_renderer->size = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}




static void
thunar_details_view_icon_renderer_get_size (GtkCellRenderer *renderer,
                                            GtkWidget       *widget,
                                            GdkRectangle    *rectangle,
                                            gint            *x_offset,
                                            gint            *y_offset,
                                            gint            *width,
                                            gint            *height)
{
  ThunarDetailsViewIconRenderer *icon_renderer = THUNAR_DETAILS_VIEW_ICON_RENDERER (renderer);

  if (rectangle != NULL)
    {
      if (x_offset != NULL)
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? 1.0 - renderer->xalign : renderer->xalign)
                    * (rectangle->width - icon_renderer->size);
          *x_offset = MAX (*x_offset, 0) + renderer->xpad;
        }

      if (y_offset != NULL)
        {
          *y_offset = renderer->yalign * (rectangle->height - icon_renderer->size);
          *y_offset = MAX (*y_offset, 0) + renderer->ypad;
        }
    }
  else
    {
      if (x_offset != NULL)
        *x_offset = 0;

      if (y_offset != NULL)
        *y_offset = 0;
    }

  if (G_LIKELY (width != NULL))
    *width = (gint) renderer->xpad * 2 + icon_renderer->size;

  if (G_LIKELY (height != NULL))
    *height = (gint) renderer->ypad * 2 + icon_renderer->size;
}



static void
thunar_details_view_icon_renderer_render (GtkCellRenderer     *renderer,
                                          GdkWindow           *window,
                                          GtkWidget           *widget,
                                          GdkRectangle        *background_area,
                                          GdkRectangle        *cell_area,
                                          GdkRectangle        *expose_area,
                                          GtkCellRendererState flags)
{
  ThunarDetailsViewIconRenderer *icon_renderer = THUNAR_DETAILS_VIEW_ICON_RENDERER (renderer);
  ThunarIconFactory             *icon_factory;
  GdkRectangle                   icon_area;
  GdkRectangle                   draw_area;
  GdkPixbuf                     *scaled;
  GdkPixbuf                     *icon;
  GList                         *emblems;
  GList                         *lp;

  if (G_UNLIKELY (icon_renderer->file == NULL))
    return;

  /* load the main icon */
  icon_factory = thunar_icon_factory_get_default ();
  icon = thunar_file_load_icon (icon_renderer->file, icon_factory, icon_renderer->size);
  if (G_UNLIKELY (icon == NULL))
    return;

  icon_area.width = gdk_pixbuf_get_width (icon);
  icon_area.height = gdk_pixbuf_get_height (icon);

  /* scale down the icon on-demand */
  if (G_UNLIKELY (icon_area.width > icon_renderer->size || icon_area.height > icon_renderer->size))
    {
      scaled = exo_gdk_pixbuf_scale_ratio (icon, icon_renderer->size);
      g_object_unref (G_OBJECT (icon));
      icon = scaled;

      icon_area.width = gdk_pixbuf_get_width (icon);
      icon_area.height = gdk_pixbuf_get_height (icon);
    }

  icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
  icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

  if (gdk_rectangle_intersect (cell_area, &icon_area, &draw_area)
      && gdk_rectangle_intersect (expose_area, &icon_area, &draw_area))
    {
      gdk_draw_pixbuf (window, widget->style->black_gc, icon,
                       draw_area.x - icon_area.x, draw_area.y - icon_area.y,
                       draw_area.x, draw_area.y, draw_area.width, draw_area.height,
                       GDK_RGB_DITHER_NORMAL, 0, 0);
    }

  g_object_unref (G_OBJECT (icon));

  /* display the primary emblem as well (if any) */
  emblems = thunar_file_get_emblem_names (icon_renderer->file);
  if (emblems != NULL)
    {
      /* lookup the first emblem icon that exits in the icon theme */
      for (icon = NULL, lp = emblems; lp != NULL; lp = lp->next)
        {
          icon = thunar_icon_factory_load_icon (icon_factory, lp->data, icon_renderer->size, NULL, FALSE);
          if (G_LIKELY (icon != NULL))
            break;
        }

      if (G_LIKELY (icon != NULL))
        {
          icon_area.width = gdk_pixbuf_get_width (icon);
          icon_area.height = gdk_pixbuf_get_height (icon);
          icon_area.x = cell_area->x + (cell_area->width - icon_area.width - renderer->xpad);
          icon_area.y = cell_area->y + (cell_area->height - icon_area.height - renderer->ypad);

          if (gdk_rectangle_intersect (expose_area, &icon_area, &draw_area))
            {
              gdk_draw_pixbuf (window, widget->style->black_gc, icon,
                               draw_area.x - icon_area.x, draw_area.y - icon_area.y,
                               draw_area.x, draw_area.y, draw_area.width, draw_area.height,
                               GDK_RGB_DITHER_NORMAL, 0, 0);
            }

          g_object_unref (G_OBJECT (icon));
        }

      g_list_free (emblems);
    }
}



/**
 * thunar_details_view_icon_renderer_new:
 *
 * Creates a new #ThunarDetailsViewIconRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: the newly allocated #ThunarDetailsViewIconRenderer.
 **/
GtkCellRenderer*
thunar_details_view_icon_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_DETAILS_VIEW_ICON_RENDERER, NULL);
}


