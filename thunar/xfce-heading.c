/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#include "xfce-heading.h"



#define XFCE_HEADING_BORDER      6
#define XFCE_HEADING_SPACING    12
#define XFCE_HEADING_ICON_SIZE  48

#define XFCE_HEADING_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), XFCE_TYPE_HEADING, XfceHeadingPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_SUBTITLE,
  PROP_TITLE,
};



static void         xfce_heading_finalize       (GObject          *object);
static void         xfce_heading_get_property   (GObject          *object,
                                                 guint             prop_id,
                                                 GValue           *value,
                                                 GParamSpec       *pspec);
static void         xfce_heading_set_property   (GObject          *object,
                                                 guint             prop_id,
                                                 const GValue     *value,
                                                 GParamSpec       *pspec);
static void         xfce_heading_realize        (GtkWidget        *widget);
static void         xfce_heading_size_request   (GtkWidget        *widget,
                                                 GtkRequisition   *requisition);
static void         xfce_heading_style_set      (GtkWidget        *widget,
                                                 GtkStyle         *previous_style);
static gboolean     xfce_heading_expose_event   (GtkWidget        *widget,
                                                 GdkEventExpose   *event);
static AtkObject   *xfce_heading_get_accessible (GtkWidget        *widget);
static PangoLayout *xfce_heading_make_layout    (XfceHeading      *heading);
static GdkPixbuf   *xfce_heading_make_pixbuf    (XfceHeading      *heading);



struct _XfceHeadingPrivate
{
  GdkPixbuf *icon;
  gchar     *icon_name;
  gchar     *subtitle;
  gchar     *title;
};



G_DEFINE_TYPE (XfceHeading, xfce_heading, GTK_TYPE_WIDGET);



static void
xfce_heading_class_init (XfceHeadingClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  /* add our private data to the class */
  g_type_class_add_private (klass, sizeof (XfceHeadingPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_heading_finalize;
  gobject_class->get_property = xfce_heading_get_property;
  gobject_class->set_property = xfce_heading_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = xfce_heading_realize;
  gtkwidget_class->size_request = xfce_heading_size_request;
  gtkwidget_class->style_set = xfce_heading_style_set;
  gtkwidget_class->expose_event = xfce_heading_expose_event;
  gtkwidget_class->get_accessible = xfce_heading_get_accessible;

  /**
   * XfceHeading:icon:
   *
   * The #GdkPixbuf to display as icon, or %NULL to use the
   * "icon-name" property.
   *
   * Since: 4.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "icon",
                                                        "icon",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE));

  /**
   * XfceHeading:icon-name:
   *
   * If the "icon" property value is %NULL this is the name of
   * the icon to display instead (looked up using the icon theme).
   * If this property is also %NULL or the specified icon does not
   * exist in the selected icon theme, no icon will be displayed.
   *
   * Since: 4.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "icon-name",
                                                        "icon-name",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * XfceHeading:subtitle:
   *
   * The sub title that should be displayed below the
   * title. May be %NULL or the empty string to display
   * only the title.
   *
   * Since: 4.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle",
                                                        "subtitle",
                                                        "subtitle",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * XfceHeading:title:
   *
   * The title text to display in the heading.
   *
   * Since: 4.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "title",
                                                        "title",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}



static void
xfce_heading_init (XfceHeading *heading)
{
  /* setup the private data */
  heading->priv = XFCE_HEADING_GET_PRIVATE (heading);

  /* setup the widget parameters */
  GTK_WIDGET_UNSET_FLAGS (heading, GTK_NO_WINDOW);
}



static void
xfce_heading_finalize (GObject *object)
{
  XfceHeading *heading = XFCE_HEADING (object);

  /* release the private data */
  if (G_UNLIKELY (heading->priv->icon != NULL))
    g_object_unref (G_OBJECT (heading->priv->icon));
  g_free (heading->priv->icon_name);
  g_free (heading->priv->subtitle);
  g_free (heading->priv->title);

  (*G_OBJECT_CLASS (xfce_heading_parent_class)->finalize) (object);
}



static void
xfce_heading_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  XfceHeading *heading = XFCE_HEADING (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, xfce_heading_get_icon (heading));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, xfce_heading_get_icon_name (heading));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, xfce_heading_get_subtitle (heading));
      break;

    case PROP_TITLE:
      g_value_set_string (value, xfce_heading_get_title (heading));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_heading_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  XfceHeading *heading = XFCE_HEADING (object);

  switch (prop_id)
    {
    case PROP_ICON:
      xfce_heading_set_icon (heading, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      xfce_heading_set_icon_name (heading, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      xfce_heading_set_subtitle (heading, g_value_get_string (value));
      break;

    case PROP_TITLE:
      xfce_heading_set_title (heading, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_heading_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;

  /* mark the widget as realized */
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* setup the window attributes */
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_EXPOSURE_MASK;

  /* allocate the widget window */
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes,
                                   GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);
  gdk_window_set_user_data (widget->window, widget);

  /* connect the style to the window */
  widget->style = gtk_style_attach (widget->style, widget->window);

  /* set background color (using the base color) */
  gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
}



static void
xfce_heading_size_request (GtkWidget      *widget,
                           GtkRequisition *requisition)
{
  XfceHeading *heading = XFCE_HEADING (widget);
  PangoLayout *layout;
  GdkPixbuf   *pixbuf;
  gint         layout_width;
  gint         layout_height;
  gint         pixbuf_width = 0;
  gint         pixbuf_height = 0;

  /* determine the dimensions of the title text */
  layout = xfce_heading_make_layout (heading);
  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);
  g_object_unref (G_OBJECT (layout));

  /* determine the dimensions of the pixbuf */
  pixbuf = xfce_heading_make_pixbuf (heading);
  if (G_LIKELY (pixbuf != NULL))
    {
      pixbuf_width = gdk_pixbuf_get_width (pixbuf);
      pixbuf_height = gdk_pixbuf_get_height (pixbuf);
      g_object_unref (G_OBJECT (pixbuf));
    }

  /* determine the base dimensions */
  requisition->width = layout_width + pixbuf_width + ((pixbuf_width > 0) ? XFCE_HEADING_SPACING : 0);
  requisition->height = MAX (XFCE_HEADING_ICON_SIZE, MAX (pixbuf_height, layout_height));

  /* add border size */
  requisition->width += 2 * XFCE_HEADING_BORDER;
  requisition->height += 2 * XFCE_HEADING_BORDER;
}



static void
xfce_heading_style_set (GtkWidget *widget,
                        GtkStyle  *previous_style)
{
  /* check if we're already realized */
  if (GTK_WIDGET_REALIZED (widget))
    {
      /* set background color (using the base color) */
      gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
    }
}



static gboolean
xfce_heading_expose_event (GtkWidget      *widget,
                           GdkEventExpose *event)
{
  XfceHeading *heading = XFCE_HEADING (widget);
  PangoLayout *layout;
  GdkPixbuf   *pixbuf;
  gboolean     rtl;
  gint         width;
  gint         height;
  gint         x;
  gint         y;

  /* check if we should render from right to left */
  rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  /* determine the initial horizontal position */
  x = (rtl ? widget->allocation.width - XFCE_HEADING_BORDER : XFCE_HEADING_BORDER);

  /* check if we have a pixbuf to render */
  pixbuf = xfce_heading_make_pixbuf (heading);
  if (G_LIKELY (pixbuf != NULL))
    {
      /* determine the pixbuf dimensions */
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);

      /* determine the vertical position */
      y = (widget->allocation.height - height) / 2;

      /* render the pixbuf */
      gdk_draw_pixbuf (widget->window, widget->style->black_gc, pixbuf, 0, 0,
                       (rtl ? x - width : x), y, width, height,
                       GDK_RGB_DITHER_NORMAL, 0, 0);

      /* release the pixbuf */
      g_object_unref (G_OBJECT (pixbuf));

      /* advance the horizontal position */
      x += (rtl ? -1 : 1) * (width + XFCE_HEADING_SPACING);
    }

  /* generate the title layout */
  layout = xfce_heading_make_layout (heading);
  pango_layout_get_pixel_size (layout, &width, &height);

  /* determine the vertical position */
  y = (widget->allocation.height - height) / 2;

  /* render the title */
  gtk_paint_layout (widget->style, widget->window, GTK_WIDGET_STATE (widget), TRUE, &event->area,
                    widget, "heading", (rtl ? x - width : x), y, layout);

  /* release the layout */
  g_object_unref (G_OBJECT (layout));

  return FALSE;
}



static AtkObject*
xfce_heading_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  object = (*GTK_WIDGET_CLASS (xfce_heading_parent_class)->get_accessible) (widget);
  atk_object_set_role (object, ATK_ROLE_HEADER);

  return object;
}



static PangoLayout*
xfce_heading_make_layout (XfceHeading *heading)
{
  PangoAttribute *attribute;
  PangoAttrList  *attr_list;
  PangoLayout    *layout;
  GString        *text;
  gint            title_length = 0;

  /* generate the full text */
  text = g_string_sized_new (128);
  if (G_LIKELY (heading->priv->title != NULL))
    {
      /* add the main title */
      title_length = strlen (heading->priv->title);
      g_string_append (text, heading->priv->title);
    }
  if (heading->priv->subtitle != NULL && *heading->priv->subtitle != '\0')
    {
      /* add an empty line between the title and the subtitle */
      if (G_LIKELY (heading->priv->title != NULL))
        g_string_append (text, "\n");

      /* add the subtitle */
      g_string_append (text, heading->priv->subtitle);
    }

  /* allocate and setup a new layout from the widget's context */
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (heading), text->str);

  /* allocate an attribute list (large bold title) */
  attr_list = pango_attr_list_new ();
  attribute = pango_attr_scale_new (PANGO_SCALE_LARGE); /* large title */
  attribute->start_index = 0;
  attribute->end_index = title_length;
  pango_attr_list_insert (attr_list, attribute);
  attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD); /* bold title */
  attribute->start_index = 0;
  attribute->end_index = title_length;
  pango_attr_list_insert (attr_list, attribute);
  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  /* cleanup */
  g_string_free (text, TRUE);

  return layout;
}


  
static GdkPixbuf*
xfce_heading_make_pixbuf (XfceHeading *heading)
{
  GtkIconTheme *icon_theme;
  GdkPixbuf    *pixbuf = NULL;
  GdkScreen    *screen;

  if (G_UNLIKELY (heading->priv->icon != NULL))
    {
      /* just use the specified icon */
      pixbuf = g_object_ref (G_OBJECT (heading->priv->icon));
    }
  else if (G_LIKELY (heading->priv->icon_name != NULL))
    {
      /* determine the icon theme for the current screen */
      screen = gtk_widget_get_screen (GTK_WIDGET (heading));
      icon_theme = gtk_icon_theme_get_for_screen (screen);

      /* try to load the icon from the icon theme */
      pixbuf = gtk_icon_theme_load_icon (icon_theme, heading->priv->icon_name,
                                         XFCE_HEADING_ICON_SIZE,
                                         GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    }

  return pixbuf;
}



/**
 * xfce_heading_new:
 *
 * Allocates a new #XfceHeading instance.
 *
 * Return value: the newly allocated #XfceHeading.
 *
 * Since: 4.4.0
 **/
GtkWidget*
xfce_heading_new (void)
{
  return g_object_new (XFCE_TYPE_HEADING, NULL);
}



/**
 * xfce_heading_get_icon:
 * @heading : a #XfceHeading.
 *
 * Returns the #GdkPixbuf that was set as icon for
 * @heading or %NULL if no icon is set. The returned
 * #GdkPixbuf object is owned by @heading.
 *
 * Return value: the icon for @heading, or %NULL.
 *
 * Since: 4.4.0
 **/
GdkPixbuf*
xfce_heading_get_icon (XfceHeading *heading)
{
  g_return_val_if_fail (XFCE_IS_HEADING (heading), NULL);
  return heading->priv->icon;
}



/**
 * xfce_heading_set_icon:
 * @heading : a #XfceHeading.
 * @icon    : the new icon or %NULL.
 *
 * If @icon is not %NULL, @heading will display the new @icon
 * aside the title. Else, if @icon is %NULL no icon is displayed
 * unless an icon name was set with xfce_heading_set_icon_name().
 *
 * Since: 4.4.0
 **/
void
xfce_heading_set_icon (XfceHeading *heading,
                       GdkPixbuf   *icon)
{
  g_return_if_fail (XFCE_IS_HEADING (heading));
  g_return_if_fail (icon == NULL || GDK_IS_PIXBUF (icon));

  /* check if we have a new icon */
  if (G_LIKELY (heading->priv->icon != icon))
    {
      /* disconnect from the previous icon */
      if (G_LIKELY (heading->priv->icon != NULL))
        g_object_unref (G_OBJECT (heading->priv->icon));

      /* activate the new icon */
      heading->priv->icon = icon;

      /* connect to the new icon */
      if (G_LIKELY (icon != NULL))
        g_object_ref (G_OBJECT (icon));

      /* schedule a resize */
      gtk_widget_queue_resize (GTK_WIDGET (heading));

      /* notify listeners */
      g_object_notify (G_OBJECT (heading), "icon");
    }
}



/**
 * xfce_heading_get_icon_name:
 * @heading : a #XfceHeading.
 *
 * Returns the icon name previously set by a call to
 * xfce_heading_set_icon_name() or %NULL if no icon name
 * is set for @heading.
 *
 * Return value: the icon name for @heading, or %NULL.
 *
 * Since: 4.4.0
 **/
G_CONST_RETURN gchar*
xfce_heading_get_icon_name (XfceHeading *heading)
{
  g_return_val_if_fail (XFCE_IS_HEADING (heading), NULL);
  return heading->priv->icon_name;
}



/**
 * xfce_heading_set_icon_name:
 * @heading   : a #XfceHeading.
 * @icon_name : the new icon name, or %NULL.
 *
 * If @icon_name is not %NULL and the "icon" property is set to
 * %NULL, see xfce_heading_set_icon(), the @heading will display
 * the name icon identified by the @icon_name.
 *
 * Since: 4.4.0
 **/
void
xfce_heading_set_icon_name (XfceHeading *heading,
                            const gchar *icon_name)
{
  g_return_if_fail (XFCE_IS_HEADING (heading));

  /* release the previous icon name */
  g_free (heading->priv->icon_name);

  /* activate the new icon name */
  heading->priv->icon_name = g_strdup (icon_name);

  /* schedule a resize */
  gtk_widget_queue_resize (GTK_WIDGET (heading));

  /* notify listeners */
  g_object_notify (G_OBJECT (heading), "icon-name");
}



/**
 * xfce_heading_get_subtitle:
 * @heading : a #XfceHeading.
 *
 * Returns the sub title displayed below the
 * main title of the @heading, or %NULL if
 * no subtitle is set.
 *
 * Return value: the subtitle of @heading, or %NULL.
 *
 * Since: 4.4.0
 **/
G_CONST_RETURN gchar*
xfce_heading_get_subtitle (XfceHeading *heading)
{
  g_return_val_if_fail (XFCE_IS_HEADING (heading), NULL);
  return heading->priv->subtitle;
}



/**
 * xfce_heading_set_subtitle:
 * @heading  : a #XfceHeading.
 * @subtitle : the new subtitle for @heading, or %NULL.
 *
 * If @subtitle is not %NULL and not the empty string, it
 * will be displayed by @heading below the main title.
 *
 * Since: 4.4.0
 **/
void
xfce_heading_set_subtitle (XfceHeading *heading,
                           const gchar *subtitle)
{
  g_return_if_fail (XFCE_IS_HEADING (heading));
  g_return_if_fail (subtitle == NULL || g_utf8_validate (subtitle, -1, NULL));

  /* release the previous subtitle */
  g_free (heading->priv->subtitle);

  /* activate the new subtitle */
  heading->priv->subtitle = g_strdup (subtitle);

  /* schedule a resize */
  gtk_widget_queue_resize (GTK_WIDGET (heading));

  /* notify listeners */
  g_object_notify (G_OBJECT (heading), "subtitle");
}



/**
 * xfce_heading_get_title:
 * @heading : a #XfceHeading.
 *
 * Returns the title displayed by the @heading.
 *
 * Return value: the title displayed by the @heading.
 *
 * Since: 4.4.0
 **/
G_CONST_RETURN gchar*
xfce_heading_get_title (XfceHeading *heading)
{
  g_return_val_if_fail (XFCE_IS_HEADING (heading), NULL);
  return heading->priv->title;
}



/**
 * xfce_heading_set_title:
 * @heading : a #XfceHeading.
 * @title   : the new title for the @heading.
 *
 * Sets the title displayed by the @heading to the
 * specified @title.
 *
 * Since: 4.4.0
 **/
void
xfce_heading_set_title (XfceHeading *heading,
                        const gchar *title)
{
  g_return_if_fail (XFCE_IS_HEADING (heading));
  g_return_if_fail (title == NULL || g_utf8_validate (title, -1, NULL));

  /* release the previous title */
  g_free (heading->priv->title);

  /* activate the new title */
  heading->priv->title = g_strdup (title);

  /* schedule a resize */
  gtk_widget_queue_resize (GTK_WIDGET (heading));

  /* notify listeners */
  g_object_notify (G_OBJECT (heading), "title");
}


