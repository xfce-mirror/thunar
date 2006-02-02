/* $Id$ */
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

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-throbber.h>

#if !GTK_CHECK_VERSION(2,7,1) && defined(GDK_WINDOWING_X11) && defined(HAVE_CAIRO)
#include <cairo/cairo-xlib.h>
#include <gdk/gdkx.h>
#endif



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ANIMATED,
};



static void     thunar_throbber_class_init    (ThunarThrobberClass  *klass);
static void     thunar_throbber_init          (ThunarThrobber       *throbber);
static void     thunar_throbber_dispose       (GObject              *object);
static void     thunar_throbber_get_property  (GObject              *object,
                                               guint                 prop_id,
                                               GValue               *value,
                                               GParamSpec           *pspec);
static void     thunar_throbber_set_property  (GObject              *object,
                                               guint                 prop_id,
                                               const GValue         *value,
                                               GParamSpec           *pspec);
static void     thunar_throbber_size_request  (GtkWidget            *widget,
                                               GtkRequisition       *requisition);
static gboolean thunar_throbber_expose_event  (GtkWidget            *widget,
                                               GdkEventExpose       *event);
static gboolean thunar_throbber_timer         (gpointer              user_data);
static void     thunar_throbber_timer_destroy (gpointer              user_data);



struct _ThunarThrobberClass
{
  GtkWidgetClass __parent__;
};

struct _ThunarThrobber
{
  GtkWidget __parent__;

  gboolean animated;
  gint     angle;
  gint     timer_id;
};



static GObjectClass *thunar_throbber_parent_class;



GType
thunar_throbber_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarThrobberClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_throbber_class_init,
        NULL,
        NULL,
        sizeof (ThunarThrobber),
        0,
        (GInstanceInitFunc) thunar_throbber_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_WIDGET, I_("ThunarThrobber"), &info, 0);
    }

  return type;
}



static void
thunar_throbber_class_init (ThunarThrobberClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_throbber_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_throbber_dispose;
  gobject_class->get_property = thunar_throbber_get_property;
  gobject_class->set_property = thunar_throbber_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunar_throbber_size_request;
  gtkwidget_class->expose_event = thunar_throbber_expose_event;

  /**
   * ThunarThrobber:animated:
   *
   * Whether the throbber should display an animation.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ANIMATED,
                                   g_param_spec_boolean ("animated",
                                                         "animated",
                                                         "animated",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_throbber_init (ThunarThrobber *throbber)
{
  GTK_WIDGET_SET_FLAGS (throbber, GTK_NO_WINDOW);
  throbber->timer_id = -1;
}



static void
thunar_throbber_dispose (GObject *object)
{
  ThunarThrobber *throbber = THUNAR_THROBBER (object);

  /* stop any running animation */
  if (G_UNLIKELY (throbber->timer_id >= 0))
    g_source_remove (throbber->timer_id);

  (*G_OBJECT_CLASS (thunar_throbber_parent_class)->dispose) (object);
}



static void
thunar_throbber_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ThunarThrobber *throbber = THUNAR_THROBBER (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      g_value_set_boolean (value, thunar_throbber_get_animated (throbber));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_throbber_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ThunarThrobber *throbber = THUNAR_THROBBER (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      thunar_throbber_set_animated (throbber, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_throbber_size_request (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &requisition->width, &requisition->height);
  requisition->width += 2;
  requisition->height += 2;
}



#if GTK_CHECK_VERSION(2,7,1)
static cairo_t*
get_cairo_context (GdkWindow *window)
{
  return gdk_cairo_create (window);
}
#elif defined(GDK_WINDOWING_X11) && defined(HAVE_CAIRO)
static cairo_t*
get_cairo_context (GdkWindow *window)
{
  cairo_surface_t *surface;
  GdkDrawable     *drawable;
  cairo_t         *cr;
  gint             w;
  gint             h;
  gint             x;
  gint             y;

  gdk_window_get_internal_paint_info (window, &drawable, &x, &y);
  gdk_drawable_get_size (drawable, &w, &h);

  surface = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
                                       gdk_x11_visual_get_xvisual (gdk_drawable_get_visual (drawable)), w, h);
  cr = cairo_create (surface);
  cairo_surface_destroy (surface);

  cairo_translate (cr, -x, -y);

  return cr;
}
#endif



static gboolean
thunar_throbber_expose_event (GtkWidget      *widget,
                              GdkEventExpose *event)
{
#if GTK_CHECK_VERSION(2,7,1) || (defined(GDK_WINDOWING_X11) && defined(HAVE_CAIRO))
  ThunarThrobber *throbber = THUNAR_THROBBER (widget);
  GdkColor        color;
  cairo_t        *cr;
  gdouble         a, n;
  gint            size;
  gint            x, y;

  /* determine the foreground color */
  color = widget->style->fg[GTK_STATE_NORMAL];

  /* allocate a cairo context */
  cr = get_cairo_context (widget->window);

  /* setup clipping area */
#if GTK_CHECK_VERSION(2,7,1)
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);
#endif

  /* determine size and position of the throbber */
  size = MIN (widget->allocation.width, widget->allocation.height);
  x = widget->allocation.x + (widget->allocation.width - size) / 2;
  y = widget->allocation.y + (widget->allocation.height - size) / 2;

  /* apply required transformations */
  cairo_translate (cr, x, y);
  cairo_scale (cr, size, size);
  cairo_translate (cr, 0.5, 0.5);
  cairo_rotate (cr, (throbber->angle * M_PI) / 180.0);

  /* draw the circles */
  for (n = 0.0; n < 2.0; n += 0.25)
    {
      /* determine the alpha based on whether the timer is still active */
      a = (throbber->timer_id >= 0) ? (n + 0.5) / 3.0 : 0.30;

      /* render the circle */
      cairo_set_source_rgba (cr, color.red / 65535.0, color.green / 65535.0, color.blue / 65535.0, a);
      cairo_arc (cr, cos (n * M_PI) / 3.0, sin (n * M_PI) /3.0, 0.105, 0.0, 2.0 * M_PI);
      cairo_fill (cr);
    }

  cairo_destroy (cr);
#endif

  return TRUE;
}



static gboolean
thunar_throbber_timer (gpointer user_data)
{
  ThunarThrobber *throbber = THUNAR_THROBBER (user_data);

  GDK_THREADS_ENTER ();
  throbber->angle = (throbber->angle + 45) % 360;
  gtk_widget_queue_draw (GTK_WIDGET (throbber));
  GDK_THREADS_LEAVE ();

  return throbber->animated;
}



static void
thunar_throbber_timer_destroy (gpointer user_data)
{
  THUNAR_THROBBER (user_data)->timer_id = -1;
}



/**
 * thunar_throbber_new:
 *
 * Allocates a new #ThunarThrobber instance.
 *
 * Return value: the newly allocated #ThunarThrobber.
 **/
GtkWidget*
thunar_throbber_new (void)
{
  return g_object_new (THUNAR_TYPE_THROBBER, NULL);
}



/**
 * thunar_throbber_get_animated:
 * @throbber : a #ThunarThrobber.
 *
 * Returns whether @throbber is currently animated.
 *
 * Return value: %TRUE if @throbber is animated.
 **/
gboolean
thunar_throbber_get_animated (const ThunarThrobber *throbber)
{
  g_return_val_if_fail (THUNAR_IS_THROBBER (throbber), FALSE);
  return throbber->animated;
}



/**
 * thunar_throbber_set_animated:
 * @throbber : a #ThunarThrobber.
 * @animated : whether to animate @throbber.
 *
 * If @animated is %TRUE, @throbber will display an animation.
 **/
void
thunar_throbber_set_animated (ThunarThrobber *throbber,
                              gboolean        animated)
{
  g_return_if_fail (THUNAR_IS_THROBBER (throbber));

  /* check if we're already in the requested state */
  if (G_UNLIKELY (throbber->animated == animated))
    return;

  /* pick up the new state */
  throbber->animated = animated;

  /* start the timer if animated and not already running */
  if (animated && (throbber->timer_id < 0))
    {
      /* start the animation */
      throbber->timer_id = g_timeout_add_full (G_PRIORITY_LOW, 60, thunar_throbber_timer,
                                               throbber, thunar_throbber_timer_destroy);
    }

  /* schedule a redraw with the new animation state */
  gtk_widget_queue_draw (GTK_WIDGET (throbber));

  /* notify listeners */
  g_object_notify (G_OBJECT (throbber), "animated");
}


