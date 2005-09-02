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

#include <thunar/thunar-file.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunarx/thunarx-gdk-pixbuf-extensions.h>



enum
{
  PROP_0,
  PROP_DROP_FILE,
  PROP_FILE,
  PROP_FOLLOW_STATE,
  PROP_SIZE,
};



static void thunar_icon_renderer_class_init    (ThunarIconRendererClass *klass);
static void thunar_icon_renderer_init          (ThunarIconRenderer      *icon_renderer);
static void thunar_icon_renderer_finalize      (GObject                 *object);
static void thunar_icon_renderer_get_property  (GObject                 *object,
                                                guint                    prop_id,
                                                GValue                  *value,
                                                GParamSpec              *pspec);
static void thunar_icon_renderer_set_property  (GObject                 *object,
                                                guint                    prop_id,
                                                const GValue            *value,
                                                GParamSpec              *pspec);
static void thunar_icon_renderer_get_size      (GtkCellRenderer         *renderer,
                                                GtkWidget               *widget,
                                                GdkRectangle            *rectangle,
                                                gint                    *x_offset,
                                                gint                    *y_offset,
                                                gint                    *width,
                                                gint                    *height);
static void thunar_icon_renderer_render        (GtkCellRenderer         *renderer,
                                                GdkWindow               *window,
                                                GtkWidget               *widget,
                                                GdkRectangle            *background_area,
                                                GdkRectangle            *cell_area,
                                                GdkRectangle            *expose_area,
                                                GtkCellRendererState     flags);



struct _ThunarIconRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarIconRenderer
{
  GtkCellRenderer __parent__;

  ThunarFile *drop_file;
  ThunarFile *file;
  gboolean    follow_state;
  gint        size;
};



G_DEFINE_TYPE (ThunarIconRenderer, thunar_icon_renderer, GTK_TYPE_CELL_RENDERER);



static void
thunar_icon_renderer_class_init (ThunarIconRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_icon_renderer_finalize;
  gobject_class->get_property = thunar_icon_renderer_get_property;
  gobject_class->set_property = thunar_icon_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = thunar_icon_renderer_get_size;
  gtkcell_renderer_class->render = thunar_icon_renderer_render;

  /**
   * ThunarIconRenderer:drop-file:
   *
   * The file which should be rendered in the drop
   * accept state.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DROP_FILE,
                                   g_param_spec_object ("drop-file",
                                                        _("Drop file"),
                                                        _("The file which should be rendered in as drop acceptor"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:file:
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
   * ThunarIconRenderer::follow-state:
   *
   * Specifies whether the icon renderer should render icons
   * based on the selection state of the items. This is necessary
   * for #ExoIconView, which doesn't draw any item state indicators
   * itself.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_STATE,
                                   g_param_spec_boolean ("follow-state",
                                                         _("Follow state"),
                                                         _("Follow state"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:icon-size:
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
thunar_icon_renderer_init (ThunarIconRenderer *icon_renderer)
{
  icon_renderer->size = 24;
}



static void
thunar_icon_renderer_finalize (GObject *object)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  /* free the icon data */
  if (G_UNLIKELY (icon_renderer->drop_file != NULL))
    g_object_unref (G_OBJECT (icon_renderer->drop_file));
  if (G_LIKELY (icon_renderer->file != NULL))
    g_object_unref (G_OBJECT (icon_renderer->file));

  (*G_OBJECT_CLASS (thunar_icon_renderer_parent_class)->finalize) (object);
}



static void
thunar_icon_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DROP_FILE:
      g_value_set_object (value, icon_renderer->drop_file);
      break;

    case PROP_FILE:
      g_value_set_object (value, icon_renderer->file);
      break;

    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, icon_renderer->follow_state);
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
thunar_icon_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DROP_FILE:
      if (G_LIKELY (icon_renderer->drop_file != NULL))
        g_object_unref (G_OBJECT (icon_renderer->drop_file));
      icon_renderer->drop_file = g_value_get_object (value);
      if (G_LIKELY (icon_renderer->drop_file != NULL))
        g_object_ref (G_OBJECT (icon_renderer->drop_file));
      break;

    case PROP_FILE:
      if (G_LIKELY (icon_renderer->file != NULL))
        g_object_unref (G_OBJECT (icon_renderer->file));
      icon_renderer->file = g_value_get_object (value);
      if (G_LIKELY (icon_renderer->file != NULL))
        g_object_ref (G_OBJECT (icon_renderer->file));
      break;

    case PROP_FOLLOW_STATE:
      icon_renderer->follow_state = g_value_get_boolean (value);
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
thunar_icon_renderer_get_size (GtkCellRenderer *renderer,
                               GtkWidget       *widget,
                               GdkRectangle    *rectangle,
                               gint            *x_offset,
                               gint            *y_offset,
                               gint            *width,
                               gint            *height)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (renderer);

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
thunar_icon_renderer_render (GtkCellRenderer     *renderer,
                             GdkWindow           *window,
                             GtkWidget           *widget,
                             GdkRectangle        *background_area,
                             GdkRectangle        *cell_area,
                             GdkRectangle        *expose_area,
                             GtkCellRendererState flags)
{
  ThunarFileIconState icon_state;
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (renderer);
  ThunarIconFactory  *icon_factory;
  GdkRectangle        emblem_area;
  GdkRectangle        icon_area;
  GdkRectangle        draw_area;
  GtkStateType        state;
  GdkPixbuf          *emblem;
  GdkPixbuf          *icon;
  GdkPixbuf          *temp;
  GList              *emblems;
  GList              *lp;

  if (G_UNLIKELY (icon_renderer->file == NULL))
    return;

  /* determine the icon state */
  icon_state = (icon_renderer->drop_file != icon_renderer->file)
             ? THUNAR_FILE_ICON_STATE_DEFAULT
             : THUNAR_FILE_ICON_STATE_DROP;

  /* load the main icon */
  icon_factory = thunar_icon_factory_get_default ();
  icon = thunar_file_load_icon (icon_renderer->file, icon_state, icon_factory, icon_renderer->size);
  if (G_UNLIKELY (icon == NULL))
    return;

  /* pre-light the item if we're dragging about it */
  if (G_UNLIKELY (icon_state == THUNAR_FILE_ICON_STATE_DROP))
    flags |= GTK_CELL_RENDERER_PRELIT;

  /* determine the real icon size */
  icon_area.width = gdk_pixbuf_get_width (icon);
  icon_area.height = gdk_pixbuf_get_height (icon);

  /* scale down the icon on-demand */
  if (G_UNLIKELY (icon_area.width > cell_area->width || icon_area.height > cell_area->height))
    {
      temp = exo_gdk_pixbuf_scale_ratio (icon, icon_renderer->size);
      g_object_unref (G_OBJECT (icon));
      icon = temp;

      icon_area.width = gdk_pixbuf_get_width (icon);
      icon_area.height = gdk_pixbuf_get_height (icon);
    }

  /* colorize the icon if we should follow the selection state */
  if ((flags & (GTK_CELL_RENDERER_SELECTED | GTK_CELL_RENDERER_PRELIT)) != 0 && icon_renderer->follow_state)
    {
      if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
        {
          state = GTK_WIDGET_HAS_FOCUS (widget) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;
          temp = thunarx_gdk_pixbuf_colorize (icon, &widget->style->base[state]);
          g_object_unref (G_OBJECT (icon));
          icon = temp;
        }

      if ((flags & GTK_CELL_RENDERER_PRELIT) != 0)
        {
          temp = thunarx_gdk_pixbuf_spotlight (icon);
          g_object_unref (G_OBJECT (icon));
          icon = temp;
        }
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
      for (emblem = NULL, lp = emblems; emblem == NULL && lp != NULL; lp = lp->next)
        emblem = thunar_icon_factory_load_icon (icon_factory, lp->data, icon_renderer->size, NULL, FALSE);

      if (G_LIKELY (emblem != NULL))
        {
          /* determine the dimensions of the emblem */
          emblem_area.width = gdk_pixbuf_get_width (emblem);
          emblem_area.height = gdk_pixbuf_get_height (emblem);

          /* shrink insane emblems */
          if (G_UNLIKELY (MAX (emblem_area.width, emblem_area.height) > (2 * icon_renderer->size) / 3))
            {
              temp = exo_gdk_pixbuf_scale_ratio (emblem, (2 * icon_renderer->size) / 3);
              emblem_area.width = gdk_pixbuf_get_width (temp);
              emblem_area.height = gdk_pixbuf_get_height (temp);
              g_object_unref (G_OBJECT (emblem));
              emblem = temp;
            }

          /* determine a good position for the emblem */
          emblem_area.x = MIN (icon_area.x + icon_area.width - emblem_area.width / 2,
                               cell_area->x + cell_area->width - emblem_area.width);
          emblem_area.y = MIN (icon_area.y + icon_area.height - emblem_area.height / 2,
                               cell_area->y + cell_area->height -emblem_area.height);

          /* render the emblem */
          if (gdk_rectangle_intersect (expose_area, &emblem_area, &draw_area))
            {
              gdk_draw_pixbuf (window, widget->style->black_gc, emblem,
                               draw_area.x - emblem_area.x, draw_area.y - emblem_area.y,
                               draw_area.x, draw_area.y, draw_area.width, draw_area.height,
                               GDK_RGB_DITHER_NORMAL, 0, 0);
            }

          g_object_unref (G_OBJECT (emblem));
        }

      g_list_free (emblems);
    }
}



/**
 * thunar_icon_renderer_new:
 *
 * Creates a new #ThunarIconRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: the newly allocated #ThunarIconRenderer.
 **/
GtkCellRenderer*
thunar_icon_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_ICON_RENDERER, NULL);
}


