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

#include <thunar/thunar-desktop-view.h>



enum
{
  PROP_0,
  PROP_DISPLAY,
};



static void thunar_desktop_view_class_init    (ThunarDesktopViewClass *klass);
static void thunar_desktop_view_init          (ThunarDesktopView      *view);
static void thunar_desktop_view_finalize      (GObject                *object);
static void thunar_desktop_view_get_property  (GObject                *object,
                                               guint                   prop_id,
                                               GValue                 *value,
                                               GParamSpec             *pspec);
static void thunar_desktop_view_set_property  (GObject                *object,
                                               guint                   prop_id,
                                               const GValue           *value,
                                               GParamSpec             *pspec);
static void thunar_desktop_view_realize       (GtkWidget              *widget);
static void thunar_desktop_view_unrealize     (GtkWidget              *widget);



struct _ThunarDesktopViewClass
{
  GtkWidgetClass __parent__;
};

struct _ThunarDesktopView
{
  GtkWidget __parent__;

  GdkDisplay *display;
};



static GObjectClass *parent_class;



G_DEFINE_TYPE (ThunarDesktopView, thunar_desktop_view, GTK_TYPE_WIDGET);



static void
thunar_desktop_view_class_init (ThunarDesktopViewClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_desktop_view_finalize;
  gobject_class->get_property = thunar_desktop_view_get_property;
  gobject_class->set_property = thunar_desktop_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_desktop_view_realize;
  gtkwidget_class->unrealize = thunar_desktop_view_unrealize;

  /**
   * ThunarDesktopView:display:
   *
   * The #GdkDisplay covered by this desktop view or %NULL if
   * the desktop view is not currently connected to any
   * display.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        _("Display"),
                                                        _("The display currently associated with"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE));
}



static void
thunar_desktop_view_init (ThunarDesktopView *view)
{
}



static void
thunar_desktop_view_finalize (GObject *object)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
    case PROP_DISPLAY:
      g_value_set_object (value, thunar_desktop_view_get_display (view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_desktop_view_set_property  (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      thunar_desktop_view_set_display (view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_desktop_view_realize (GtkWidget *widget)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (widget);

  // TODO: Create one GdkWindow per screen (each spanning
  // all monitors connected to the given screen)
  (void)&view;
}



static void
thunar_desktop_view_unrealize (GtkWidget *widget)
{
  ThunarDesktopView *view = THUNAR_DESKTOP_VIEW (widget);

  // TODO: Destroy all previously created GdkWindow's
  (void)&view;
}



/**
 * thunar_desktop_view_new:
 *
 * Creates a new #ThunarDesktopView, which is associated with
 * no #GdkDisplay. You'll need to set a display using
 * #thunar_desktop_view_set_display() before realizing the
 * view.
 *
 * Return value: the newly allocated #ThunarDesktopView instance.
 **/
GtkWidget*
thunar_desktop_view_new (void)
{
  return g_object_new (THUNAR_TYPE_DESKTOP_VIEW, NULL);
}



/**
 * thunar_desktop_view_new_with_display:
 * @display : a valid #GdkDisplay.
 *
 * Creates a new #ThunarDesktop and associates it with the given
 * @display. You can instantly show (and thereby realize) the
 * returned object.
 *
 * Return value: the newly allocated #ThunarDesktopView instance.
 **/
GtkWidget*
thunar_desktop_view_new_with_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return g_object_new (THUNAR_TYPE_DESKTOP_VIEW,
                       "display", display,
                       NULL);
}



/**
 * thunar_desktop_view_get_display:
 * @view : a #ThunarDesktopView.
 *
 * Queries the #GdkDisplay currently used by the @view. May return
 * %NULL if no display has been set yet.
 *
 * Return value: the #GdkDisplay @view is associated with or %NULL.
 **/
GdkDisplay*
thunar_desktop_view_get_display (ThunarDesktopView *view)
{
  g_return_val_if_fail (THUNAR_IS_DESKTOP_VIEW (view), NULL);
  return view->display;
}



/**
 * thunar_desktop_view_set_display:
 * @view    : a #ThunarDesktopView.
 * @display : a #GdkDisplay or %NULL.
 *
 * Sets the display to use for @view to @display. If @view was
 * previously connected to another #GdkDisplay, it'll first
 * disconnect from the old display, then connect to the new
 * display.
 **/
void
thunar_desktop_view_set_display (ThunarDesktopView *view,
                                 GdkDisplay        *display)
{
  g_return_if_fail (THUNAR_IS_DESKTOP_VIEW (view));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  /* don't do anything if we already use that display */
  if (G_UNLIKELY (display == view->display))
    return;

  if (view->display != NULL)
    {
      // TODO: Disconnect from the old display (take into account the
      // REALIZED state)
    }

  view->display = display;

  if (view->display != NULL)
    {
      // TODO: Connect to the new display (take into account the
      // REALIZED state)
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (view), "display");
}
