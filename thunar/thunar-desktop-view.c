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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-desktop-model.h>
#include <thunar/thunar-desktop-view.h>
#include <thunar/thunar-file.h>

#include <thunarx/thunarx-gdk-pixbuf-extensions.h>



#define TILE_WIDTH  (48)
#define TILE_HEIGHT (32)



typedef struct _ThunarDesktopViewItem ThunarDesktopViewItem;

enum
{
  PROP_0,
  PROP_ICON_SIZE,
  PROP_MODEL,
  PROP_FILE_COLUMN,
  PROP_POSITION_COLUMN,
};



static void                   thunar_desktop_view_class_init          (ThunarDesktopViewClass *klass);
static void                   thunar_desktop_view_init                (ThunarDesktopView      *view);
static void                   thunar_desktop_view_finalize            (GObject                *object);
static void                   thunar_desktop_view_get_property        (GObject                *object,
                                                                       guint                   prop_id,
                                                                       GValue                 *value,
                                                                       GParamSpec             *pspec);
static void                   thunar_desktop_view_set_property        (GObject                *object,
                                                                       guint                   prop_id,
                                                                       const GValue           *value,
                                                                       GParamSpec             *pspec);
static void                   thunar_desktop_view_destroy             (GtkObject              *object);
static void                   thunar_desktop_view_size_request        (GtkWidget              *widget,
                                                                       GtkRequisition         *requisition);
static gboolean               thunar_desktop_view_button_press_event  (GtkWidget              *widget,
                                                                       GdkEventButton         *event);
static gboolean               thunar_desktop_view_expose_event        (GtkWidget              *widget,
                                                                       GdkEventExpose         *event);
static void                   thunar_desktop_view_build_items         (ThunarDesktopView      *view);
static ThunarDesktopViewItem *thunar_desktop_view_get_item_at_pos     (ThunarDesktopView      *view,
                                                                       gint                    x,
                                                                       gint                    y);
static void                   thunar_desktop_view_get_item_area       (ThunarDesktopView      *view,
                                                                       ThunarDesktopViewItem  *item,
                                                                       GdkRectangle           *area);
static void                   thunar_desktop_view_render_item         (ThunarDesktopView      *view,
                                                                       ThunarDesktopViewItem  *item,
                                                                       GdkRectangle           *item_area,
                                                                       GdkRectangle           *expose_area);
static void                   thunar_desktop_view_queue_render_item   (ThunarDesktopView      *view,
                                                                       ThunarDesktopViewItem  *item);
static void                   thunar_desktop_view_row_changed         (GtkTreeModel           *model,
                                                                       GtkTreePath            *path,
                                                                       GtkTreeIter            *iter,
                                                                       ThunarDesktopView      *view);
static void                   thunar_desktop_view_row_inserted        (GtkTreeModel           *model,
                                                                       GtkTreePath            *path,
                                                                       GtkTreeIter            *iter,
                                                                       ThunarDesktopView      *view);
static void                   thunar_desktop_view_row_deleted         (GtkTreeModel           *model,
                                                                       GtkTreePath            *path,
                                                                       ThunarDesktopView      *view);



struct _ThunarDesktopViewClass
{
  GtkWidgetClass __parent__;
};

struct _ThunarDesktopView
{
  GtkWidget __parent__;

  PangoLayout  *layout;

  GList        *items;

  GtkTreeModel *model;

  gint          icon_size;

  gint          file_column;
  gint          position_column;
};

struct _ThunarDesktopViewItem
{
  ThunarDesktopPosition position;
  GtkTreeIter           iter;
  gboolean              selected : 1;
};



G_DEFINE_TYPE (ThunarDesktopView, thunar_desktop_view, GTK_TYPE_WIDGET);



static void
thunar_desktop_view_class_init (ThunarDesktopViewClass *klass)
{
  GtkObjectClass *gtkobject_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_desktop_view_finalize;
  gobject_class->get_property = thunar_desktop_view_get_property;
  gobject_class->set_property = thunar_desktop_view_set_property;

  gtkobject_class = GTK_OBJECT_CLASS (klass);
  gtkobject_class->destroy = thunar_desktop_view_destroy;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunar_desktop_view_size_request;
  gtkwidget_class->button_press_event = thunar_desktop_view_button_press_event;
  gtkwidget_class->expose_event = thunar_desktop_view_expose_event;

  /**
   * ThunarDesktopView:icon-size:
   *
   * The icon size in pixels.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_int ("icon-size",
                                                     _("Icon size"),
                                                     _("The icon size in pixels"),
                                                     1, G_MAXINT, 48,
                                                     EXO_PARAM_READWRITE));

  /**
   * ThunarDesktopView:model:
   *
   * The model which is displayed by this #ThunarDesktopView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        _("Model"),
                                                        _("The model to display"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarDesktopView:file-column:
   *
   * The column in the model which contains the #ThunarFile.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE_COLUMN,
                                   g_param_spec_int ("file-column",
                                                     _("File column"), 
                                                     _("The column which contains the file"),
                                                     -1, G_MAXINT, -1,
                                                     EXO_PARAM_READWRITE));

  /**
   * ThunarDesktopView:position-column:
   *
   * The column in the model which contains the location (as #ThunarDesktopPosition).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_POSITION_COLUMN,
                                   g_param_spec_int ("position-column",
                                                     _("Position column"),
                                                     _("The column which contains the position"),
                                                     -1, G_MAXINT, -1,
                                                     EXO_PARAM_READWRITE));
}



static void
thunar_desktop_view_init (ThunarDesktopView *view)
{
  GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);

  view->layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), NULL);
  pango_layout_set_alignment (view->layout, PANGO_ALIGN_CENTER);
  pango_layout_set_wrap (view->layout, PANGO_WRAP_WORD_CHAR);

  view->file_column = -1;
  view->position_column = -1;

  thunar_desktop_view_set_icon_size (view, 48);
}



static void
thunar_desktop_view_finalize (GObject *object)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  g_object_unref (G_OBJECT (view->layout));

  (*G_OBJECT_CLASS (thunar_desktop_view_parent_class)->finalize) (object);
}



static void
thunar_desktop_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      g_value_set_int (value, thunar_desktop_view_get_icon_size (view));
      break;

    case PROP_MODEL:
      g_value_set_object (value, thunar_desktop_view_get_model (view));
      break;

    case PROP_FILE_COLUMN:
      g_value_set_int (value, thunar_desktop_view_get_file_column (view));
      break;

    case PROP_POSITION_COLUMN:
      g_value_set_int (value, thunar_desktop_view_get_position_column (view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_desktop_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      thunar_desktop_view_set_icon_size (view, g_value_get_int (value));
      break;

    case PROP_MODEL:
      thunar_desktop_view_set_model (view, g_value_get_object (value));
      break;

    case PROP_FILE_COLUMN:
      thunar_desktop_view_set_file_column (view, g_value_get_int (value));
      break;

    case PROP_POSITION_COLUMN:
      thunar_desktop_view_set_position_column (view, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_desktop_view_destroy (GtkObject *object)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  thunar_desktop_view_set_model (view, NULL);

  (*GTK_OBJECT_CLASS (thunar_desktop_view_parent_class)->destroy) (object);
}



static void
thunar_desktop_view_size_request (GtkWidget      *widget,
                                  GtkRequisition *requisition)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (widget);

  requisition->width = view->icon_size;
  requisition->height = view->icon_size;
}



static gboolean
thunar_desktop_view_button_press_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  ThunarDesktopViewItem *item;
  ThunarDesktopView     *view = THUNAR_DESKTOP_VIEW (widget);

  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      item = thunar_desktop_view_get_item_at_pos (view, event->x, event->y);
      if (item != NULL)
        {
          item->selected = TRUE;
          thunar_desktop_view_queue_render_item (view, item);
        }
      else
        {
          thunar_desktop_view_unselect_all (view);
        }
    }

  return (event->button == 1);
}



static gboolean
thunar_desktop_view_expose_event (GtkWidget      *widget,
                                  GdkEventExpose *event)
{
  ThunarDesktopViewItem *item;
  ThunarDesktopView     *view = THUNAR_DESKTOP_VIEW (widget);
  GdkRectangle           item_area;
  GList                 *lp;
  gint                   screen_number;

  /* check if all necessary attributes are set */
  if (view->model == NULL || view->file_column < 0 || view->position_column < 0)
    return FALSE;

  screen_number = gdk_screen_get_number (gtk_widget_get_screen (widget));

  for (lp = view->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;

      /* check if the item should be display on this screen */
      if (G_UNLIKELY (item->position.screen_number != screen_number))
        continue;

      thunar_desktop_view_get_item_area (view, item, &item_area);

      if (gdk_region_rect_in (event->region, &item_area) != GDK_OVERLAP_RECTANGLE_OUT)
        thunar_desktop_view_render_item (view, item, &item_area, &event->area);
    }

  return TRUE;
}



static void
thunar_desktop_view_build_items (ThunarDesktopView *view)
{
  ThunarDesktopViewItem *item;
  GtkTreeIter            iter;
  GValue                 value = { 0, };

  if (gtk_tree_model_get_iter_first (view->model, &iter))
    {
      do
        {
          item = g_new (ThunarDesktopViewItem, 1);
          item->iter = iter;

          if (G_LIKELY (view->position_column >= 0))
            {
              gtk_tree_model_get_value (view->model, &item->iter, view->position_column, &value);
              item->position = *((ThunarDesktopPosition *) g_value_get_boxed (&value));
              g_value_unset (&value);
            }
          else
            memset (&item->position, 0, sizeof (item->position));

          view->items = g_list_prepend (view->items, item);
        }
      while (gtk_tree_model_iter_next (view->model, &iter));
    }
}



static ThunarDesktopViewItem*
thunar_desktop_view_get_item_at_pos (ThunarDesktopView *view,
                                     gint               x,
                                     gint               y)
{
  ThunarDesktopViewItem *item;
  GdkRectangle           item_area;
  GdkRectangle           icon_area;
  GdkRectangle           text_area;
  ThunarFile            *file;
  GdkRegion             *region;
  GdkPixbuf             *icon;
  GList                 *lp;

  region = gdk_region_new ();

  for (lp = view->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;
      thunar_desktop_view_get_item_area (view, item, &item_area);

      gtk_tree_model_get (view->model, &item->iter, view->file_column, &file, -1);

      /* add the icon area */
      icon = thunar_file_load_icon (file, view->icon_size);
      icon_area.width = gdk_pixbuf_get_width (icon);
      icon_area.height = gdk_pixbuf_get_height (icon);
      icon_area.x = item_area.x + (TILE_WIDTH * 2 - icon_area.width) / 2;
      icon_area.y = item_area.y + (TILE_HEIGHT * 2 - icon_area.height) / 2;
      gdk_region_union_with_rect (region, &icon_area);
      g_object_unref (G_OBJECT (icon));

      /* add the text area */
      pango_layout_set_text (view->layout, thunar_file_get_special_name (file), -1);
      pango_layout_set_width (view->layout, item_area.width * PANGO_SCALE);
      pango_layout_get_pixel_size (view->layout, &text_area.width, &text_area.height);
      text_area.x = item_area.x + (item_area.width - text_area.width) / 2;
      text_area.y = item_area.y + TILE_HEIGHT * 2;
      gdk_region_union_with_rect (region, &text_area);

      g_object_unref (G_OBJECT (file));

      if (gdk_region_point_in (region, x, y))
        return item;
    }

  return NULL;
}



static void
thunar_desktop_view_get_item_area (ThunarDesktopView     *view,
                                   ThunarDesktopViewItem *item,
                                   GdkRectangle          *area)
{
  area->x = item->position.column * TILE_WIDTH + GTK_WIDGET (view)->allocation.x;
  area->y = item->position.row * TILE_HEIGHT + GTK_WIDGET (view)->allocation.y;
  area->width = TILE_WIDTH * 2;
  area->height = TILE_HEIGHT * 3;
}



static void
thunar_desktop_view_render_item (ThunarDesktopView     *view,
                                 ThunarDesktopViewItem *item,
                                 GdkRectangle          *item_area,
                                 GdkRectangle          *expose_area)
{
  GdkRectangle draw_area;
  GdkRectangle icon_area;
  GdkRectangle text_area;
  ThunarFile *file;
  GdkPixbuf  *colorized_icon;
  GdkPixbuf  *icon;

  gtk_tree_model_get (view->model, &item->iter, view->file_column, &file, -1);

  icon = thunar_file_load_icon (file, view->icon_size);

  icon_area.width = gdk_pixbuf_get_width (icon);
  icon_area.height = gdk_pixbuf_get_height (icon);
  icon_area.x = item_area->x + (TILE_WIDTH * 2 - icon_area.width) / 2;
  icon_area.y = item_area->y + (TILE_HEIGHT * 2 - icon_area.height) / 2;

  if (gdk_rectangle_intersect (expose_area, &icon_area, &draw_area))
    {
      if (G_UNLIKELY (item->selected))
        {
          colorized_icon = thunarx_gdk_pixbuf_colorize (icon, &GTK_WIDGET (view)->style->base[GTK_STATE_SELECTED]);
          g_object_unref (G_OBJECT (icon));
          icon = colorized_icon;
        }

      gdk_draw_pixbuf (GTK_WIDGET (view)->window, GTK_WIDGET (view)->style->black_gc, icon,
                       draw_area.x - icon_area.x, draw_area.y - icon_area.y,
                       draw_area.x, draw_area.y, draw_area.width, draw_area.height,
                       GDK_RGB_DITHER_NORMAL, 0, 0);
    }

  text_area.x = item_area->x;
  text_area.y = item_area->y + TILE_HEIGHT * 2;
  text_area.width = 2 * TILE_WIDTH;
  text_area.height = TILE_HEIGHT;

  if (gdk_rectangle_intersect (expose_area, &text_area, &draw_area))
    {
      pango_layout_set_text (view->layout, thunar_file_get_special_name (file), -1);
      pango_layout_set_width (view->layout, text_area.width * PANGO_SCALE);

      gtk_paint_layout (GTK_WIDGET (view)->style, GTK_WIDGET (view)->window,
                        item->selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,
                        TRUE, expose_area, GTK_WIDGET (view), "icon_view",
                        text_area.x, text_area.y, view->layout);
    }

  g_object_unref (G_OBJECT (file));
  g_object_unref (G_OBJECT (icon));
}



static void
thunar_desktop_view_queue_render_item (ThunarDesktopView     *view,
                                       ThunarDesktopViewItem *item)
{
  GdkRectangle item_area;

  if (G_LIKELY (GTK_WIDGET_REALIZED (view)))
    {
      thunar_desktop_view_get_item_area (view, item, &item_area);
      gdk_window_invalidate_rect (GTK_WIDGET (view)->window, &item_area, FALSE);
    }
}



static void
thunar_desktop_view_row_changed (GtkTreeModel      *model,
                                 GtkTreePath       *path,
                                 GtkTreeIter       *iter,
                                 ThunarDesktopView *view)
{
  // FIXME: Implement this
  g_error ("Not implemented!");
}



static void
thunar_desktop_view_row_inserted (GtkTreeModel      *model,
                                  GtkTreePath       *path,
                                  GtkTreeIter       *iter,
                                  ThunarDesktopView *view)
{
  // FIXME: Implement this
  g_error ("Not implemented!");
}



static void
thunar_desktop_view_row_deleted (GtkTreeModel      *model,
                                 GtkTreePath       *path,
                                 ThunarDesktopView *view)
{
  // FIXME: Implement this
  g_error ("Not implemented!");
}



/**
 * thunar_desktop_view_new:
 *
 * Allocates a new #ThunarDesktopView instance.
 *
 * Return value: the newly allocated #ThunarDesktopView.
 **/
GtkWidget*
thunar_desktop_view_new (void)
{
  return g_object_new (THUNAR_TYPE_DESKTOP_VIEW, NULL);
}



/**
 * thunar_desktop_view_get_icon_size:
 * @view : a #ThunarDesktopView.
 *
 * Returns the icon size set for @view.
 *
 * Return value: the icon size for @view.
 **/
gint
thunar_desktop_view_get_icon_size (ThunarDesktopView *view)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_VIEW (view), -1);
  return view->icon_size;
}



/**
 * thunar_desktop_view_set_icon_size:
 * @view      : a #ThunarDesktopView.
 * @icon_size : the new icon size for @view.
 *
 * Sets the icon size for @view to @icon_size.
 **/
void
thunar_desktop_view_set_icon_size (ThunarDesktopView *view,
                                   gint               icon_size)
{
  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));
  g_return_if_fail (icon_size > 0);

  if (G_LIKELY (view->icon_size != icon_size))
    {
      view->icon_size = icon_size;
      gtk_widget_queue_resize (GTK_WIDGET (view));
      g_object_notify (G_OBJECT (view), "icon-size");
    }
}



/**
 * thunar_desktop_view_get_model:
 * @view : a #ThunarDesktopView.
 *
 * Returns the #GtkTreeModel set for @view.
 *
 * Return value: the model of @view.
 **/
GtkTreeModel*
thunar_desktop_view_get_model (ThunarDesktopView *view)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_VIEW (view), NULL);
  return view->model;
}



/**
 * thunar_desktop_view_set_model:
 * @view  : a #ThunarDesktopView.
 * @model : a #GtkTreeModel or %NULL.
 *
 * Sets the model to use for @view to @model.
 **/
void
thunar_desktop_view_set_model (ThunarDesktopView *view,
                               GtkTreeModel      *model)
{
  GType type;

  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (G_UNLIKELY (view->model == model))
    return;

  /* verify the new model (if any) */
  if (G_LIKELY (model != NULL))
    {
      g_return_if_fail ((gtk_tree_model_get_flags (model) & (GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY)) != 0);

      if (view->file_column >= 0)
        {
          type = gtk_tree_model_get_column_type (model, view->file_column);
          g_return_if_fail (type == THUNAR_TYPE_FILE);
        }

      if (view->position_column >= 0)
        {
          type = gtk_tree_model_get_column_type (model, view->position_column);
          g_return_if_fail (type == THUNAR_TYPE_DESKTOP_POSITION);
        }
    }

  /* disconnect from the previous model */
  if (G_LIKELY (view->model != NULL))
    {
      /* disconnect signals */
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->model), thunar_desktop_view_row_changed, view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->model), thunar_desktop_view_row_inserted, view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->model), thunar_desktop_view_row_deleted, view);

      /* drop items */
      g_list_foreach (view->items, (GFunc) g_free, NULL);
      g_list_free (view->items);
      view->items = NULL;

      /* drop the model */
      g_object_unref (G_OBJECT (view->model));
    }

  /* activate the new model */
  view->model = model;

  /* connect to the new model */
  if (G_LIKELY (model != NULL))
    {
      /* connect signals */
      g_signal_connect (G_OBJECT (model), "row-changed", G_CALLBACK (thunar_desktop_view_row_changed), view);
      g_signal_connect (G_OBJECT (model), "row-inserted", G_CALLBACK (thunar_desktop_view_row_inserted), view);
      g_signal_connect (G_OBJECT (model), "row-deleted", G_CALLBACK (thunar_desktop_view_row_deleted), view);

      /* generate the items */
      thunar_desktop_view_build_items (view);

      /* take a reference on the model */
      g_object_ref (G_OBJECT (model));
    }

  gtk_widget_queue_draw (GTK_WIDGET (view));
  g_object_notify (G_OBJECT (view), "model");
}



/**
 * thunar_desktop_view_get_file_column:
 * @view : a #ThunarDesktopView.
 *
 * Return value: the file column or -1.
 **/
gint
thunar_desktop_view_get_file_column (ThunarDesktopView *view)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_VIEW (view), -1);
  return view->file_column;
}



/**
 * thunar_desktop_view_set_file_column:
 * @view        : a #ThunarDesktopView.
 * @file_column : the new file column or -1.
 **/
void
thunar_desktop_view_set_file_column (ThunarDesktopView *view,
                                     gint               file_column)
{
  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));
  g_return_if_fail (file_column == -1 || file_column >= 0);

  if (G_LIKELY (view->file_column != file_column))
    {
      view->file_column = file_column;
      gtk_widget_queue_draw (GTK_WIDGET (view));
      g_object_notify (G_OBJECT (view), "file-column");
    }
}



/**
 * thunar_desktop_view_get_position_column:
 * @view : a #ThunarDesktopView.
 *
 * Returns the position column.
 *
 * Return value: the position column or -1.
 **/
gint
thunar_desktop_view_get_position_column (ThunarDesktopView *view)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_VIEW (view), -1);
  return view->position_column;
}



/**
 * thunar_desktop_view_set_position_column:
 * @view            : a #ThunarDesktopView.
 * @position_column : the new position column or -1.
 **/
void
thunar_desktop_view_set_position_column (ThunarDesktopView *view,
                                         gint               position_column)
{
  ThunarDesktopViewItem *item;
  GValue                 value = { 0, };
  GList                 *lp;

  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));
  g_return_if_fail (position_column == -1 || position_column >= 0);
  
  if (G_LIKELY (view->position_column != position_column))
    {
      /* update all item positions */
      if (G_LIKELY (view->model != NULL))
        {
          for (lp = view->items; lp != NULL; lp = lp->next)
            {
              item = lp->data;
              if (G_LIKELY (position_column >= 0))
                {
                  gtk_tree_model_get_value (view->model, &item->iter, position_column, &value);
                  item->position = *((ThunarDesktopPosition *) g_value_get_boxed (&value));
                  g_value_unset (&value);
                }
              else
                memset (&item->position, 0, sizeof (item->position));
            }
         }

      /* apply the new column */
      view->position_column = position_column;
      gtk_widget_queue_draw (GTK_WIDGET (view));
      g_object_notify (G_OBJECT (view), "position-column");
    }
}



/**
 * thunar_desktop_view_unselect_all:
 * @view : a #ThunarDesktopView.
 *
 * Unselects all selected items in @view.
 **/
void
thunar_desktop_view_unselect_all (ThunarDesktopView *view)
{
  ThunarDesktopViewItem *item;
  GList                 *lp;

  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));

  for (lp = view->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;
      if (G_UNLIKELY (item->selected))
        {
          item->selected = FALSE;
          thunar_desktop_view_queue_render_item (view, item);
        }
    }
}


