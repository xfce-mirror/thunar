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

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-statusbar.h>

#ifdef HAVE_CAIRO
#include <cairo/cairo-xlib.h>
#endif



enum
{
  ICON_PROP_0,
  ICON_PROP_FILE,
  ICON_PROP_LOADING,
};



typedef struct _ThunarStatusbarIconClass ThunarStatusbarIconClass;
typedef struct _ThunarStatusbarIcon      ThunarStatusbarIcon;



#define THUNAR_TYPE_STATUSBAR_ICON             (thunar_statusbar_icon_get_type ())
#define THUNAR_STATUSBAR_ICON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_STATUSBAR_ICON, ThunarStatusbarIcon))
#define THUNAR_STATUSBAR_ICON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_STATUSBAR_ICON, ThunarStatusbarIconClass))
#define THUNAR_IS_STATUSBAR_ICON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_STATUSBAR_ICON))
#define THUNAR_IS_STATUSBAR_ICON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_STATUSBAR_ICON))
#define THUNAR_STATUSBAR_ICON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_STATUSBAR_ICON, ThunarStatusbarIconClass))



static GType       thunar_statusbar_icon_get_type      (void) G_GNUC_CONST;
static void        thunar_statusbar_icon_class_init    (ThunarStatusbarIconClass *klass);
static void        thunar_statusbar_icon_init          (ThunarStatusbarIcon      *statusbar_icon);
static void        thunar_statusbar_icon_finalize      (GObject                  *object);
static void        thunar_statusbar_icon_get_property  (GObject                  *object,
                                                        guint                     prop_id,
                                                        GValue                   *value,
                                                        GParamSpec               *pspec);
static void        thunar_statusbar_icon_set_property  (GObject                  *object,
                                                        guint                     prop_id,
                                                        const GValue             *value,
                                                        GParamSpec               *pspec);
static void        thunar_statusbar_icon_size_request  (GtkWidget                *widget,
                                                        GtkRequisition           *requisition);
static void        thunar_statusbar_icon_realize       (GtkWidget                *widget);
static void        thunar_statusbar_icon_unrealize     (GtkWidget                *widget);
static gboolean    thunar_statusbar_icon_expose_event  (GtkWidget                *widget,
                                                        GdkEventExpose           *event);
static void        thunar_statusbar_icon_drag_begin    (GtkWidget                *widget,
                                                        GdkDragContext           *context);
static void        thunar_statusbar_icon_drag_data_get (GtkWidget                *widget,
                                                        GdkDragContext           *context,
                                                        GtkSelectionData         *selection_data,
                                                        guint                     info,
                                                        guint                     time);
static gboolean    thunar_statusbar_icon_timer         (gpointer                  user_data);
static void        thunar_statusbar_icon_timer_destroy (gpointer                  user_data);
static ThunarFile *thunar_statusbar_icon_get_file      (ThunarStatusbarIcon      *statusbar_icon);
static void        thunar_statusbar_icon_set_file      (ThunarStatusbarIcon      *statusbar_icon,
                                                        ThunarFile               *file);
static gboolean    thunar_statusbar_icon_get_loading   (ThunarStatusbarIcon      *statusbar_icon);
static void        thunar_statusbar_icon_set_loading   (ThunarStatusbarIcon      *statusbar_icon,
                                                        gboolean                  loading);



struct _ThunarStatusbarIconClass
{
  GtkWidgetClass __parent__;
};

struct _ThunarStatusbarIcon
{
  GtkWidget __parent__;

  ThunarIconFactory *icon_factory;
  ThunarFile        *file;
  GdkPixbuf         *lucent;
  gint               angle;
  gint               timer_id;
};



static GObjectClass *thunar_statusbar_icon_parent_class;

static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, 0 },
};



static GType
thunar_statusbar_icon_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarStatusbarIconClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_statusbar_icon_class_init,
        NULL,
        NULL,
        sizeof (ThunarStatusbarIcon),
        0,
        (GInstanceInitFunc) thunar_statusbar_icon_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_WIDGET, "ThunarStatusbarIcon", &info, 0);
    }

  return type;
}




static void
thunar_statusbar_icon_class_init (ThunarStatusbarIconClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  thunar_statusbar_icon_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_statusbar_icon_finalize;
  gobject_class->get_property = thunar_statusbar_icon_get_property;
  gobject_class->set_property = thunar_statusbar_icon_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunar_statusbar_icon_size_request;
  gtkwidget_class->realize = thunar_statusbar_icon_realize;
  gtkwidget_class->unrealize = thunar_statusbar_icon_unrealize;
  gtkwidget_class->expose_event = thunar_statusbar_icon_expose_event;
  gtkwidget_class->drag_begin = thunar_statusbar_icon_drag_begin;
  gtkwidget_class->drag_data_get = thunar_statusbar_icon_drag_data_get;

  /**
   * ThunarStatusbarIcon:file:
   *
   * The #ThunarFile whose icon should be displayed by this
   * #ThunarStatusbarIcon widget.
   **/
  g_object_class_install_property (gobject_class,
                                   ICON_PROP_FILE,
                                   g_param_spec_object ("file",
                                                        _("File"),
                                                        _("The file whose icon to display"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarStatusbarIcon:loading:
   *
   * Whether this #ThunarStatusbarIcon should display a
   * loading animation.
   **/
  g_object_class_install_property (gobject_class,
                                   ICON_PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         _("Loading"),
                                                         _("Whether to display a loading animation"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_statusbar_icon_init (ThunarStatusbarIcon *statusbar_icon)
{
  statusbar_icon->timer_id = -1;

  /* setup drag support for the icon */
  gtk_drag_source_set (GTK_WIDGET (statusbar_icon), GDK_BUTTON1_MASK,
                       drag_targets, G_N_ELEMENTS (drag_targets),
                       GDK_ACTION_LINK);
}



static void
thunar_statusbar_icon_finalize (GObject *object)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (object);

  /* drop the file reference */
  thunar_statusbar_icon_set_file (statusbar_icon, NULL);

  /* cancel the redraw timer */
  thunar_statusbar_icon_set_loading (statusbar_icon, FALSE);

  G_OBJECT_CLASS (thunar_statusbar_icon_parent_class)->finalize (object);
}



static void
thunar_statusbar_icon_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (object);

  switch (prop_id)
    {
    case ICON_PROP_FILE:
      g_value_set_object (value, thunar_statusbar_icon_get_file (statusbar_icon));
      break;

    case ICON_PROP_LOADING:
      g_value_set_boolean (value, thunar_statusbar_icon_get_loading (statusbar_icon));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_statusbar_icon_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (object);

  switch (prop_id)
    {
    case ICON_PROP_FILE:
      thunar_statusbar_icon_set_file (statusbar_icon, g_value_get_object (value));
      break;

    case ICON_PROP_LOADING:
      thunar_statusbar_icon_set_loading (statusbar_icon, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_statusbar_icon_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition)
{
  requisition->width = 16 + 2 * widget->style->xthickness;
  requisition->height = 16 + 2 * widget->style->ythickness;
}



static void
thunar_statusbar_icon_realize (GtkWidget *widget)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (widget);
  GdkWindowAttr        attr;
  GtkIconTheme        *icon_theme;
  gint                 attr_mask;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attr.x = widget->allocation.x;
  attr.y = widget->allocation.y;
  attr.width = widget->allocation.width;
  attr.height = widget->allocation.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_CHILD;
  attr.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attr.visual = gtk_widget_get_visual (widget);
  attr.colormap = gtk_widget_get_colormap (widget);
  attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (widget->parent->window, &attr, attr_mask);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gdk_window_set_user_data (widget->window, widget);

  /* connect to the icon factory */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  statusbar_icon->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
}



static void
thunar_statusbar_icon_unrealize (GtkWidget *widget)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (widget);

  /* disconnect from the icon factory */
  g_object_unref (G_OBJECT (statusbar_icon->icon_factory));
  statusbar_icon->icon_factory = NULL;

  GTK_WIDGET_CLASS (thunar_statusbar_icon_parent_class)->unrealize (widget);
}



static GdkPixbuf*
create_lucent_pixbuf (GdkPixbuf *src)
{
  GdkPixbuf *dest;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pixsrc;
  guchar *pixdest;
  gint width, height, has_alpha, src_row_stride, dst_row_stride;
  gint i, j;

  if (G_UNLIKELY (!gdk_pixbuf_get_has_alpha (src)))
    {
      g_object_ref (G_OBJECT (src));
      return src;
    }

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
                         gdk_pixbuf_get_has_alpha (src),
                         gdk_pixbuf_get_bits_per_sample (src),
                         gdk_pixbuf_get_width (src),
                         gdk_pixbuf_get_height (src));
  
  has_alpha = gdk_pixbuf_get_has_alpha (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  dst_row_stride = gdk_pixbuf_get_rowstride (dest);
  target_pixels = gdk_pixbuf_get_pixels (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);

  for (i = 0; i < height; i++)
    {
      pixdest = target_pixels + i * dst_row_stride;
      pixsrc = original_pixels + i * src_row_stride;
      for (j = 0; j < width; j++)
        {
          pixdest[0] = pixsrc[0];
          pixdest[1] = pixsrc[1];
          pixdest[2] = pixsrc[2];
          pixdest[3] = pixsrc[3] / 3;
          pixdest += 4;
          pixsrc += 4;
        }
    }

  return dest;
}



#if GTK_CHECK_VERSION(2,7,2)
static cairo_t*
get_cairo_context (GdkWindow *window)
{
  cairo_t *cr;
  gint     w;
  gint     h;

  gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);

  cr = gdk_cairo_create (window);
  cairo_scale (cr, h, w);
  cairo_translate (cr, 0.5, 0.5);

  return cr;
}
#elif defined(HAVE_CAIRO)
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
  cairo_scale (cr, h, w);
  cairo_translate (cr, 0.5, 0.5);

  return cr;
}
#endif



static gboolean
thunar_statusbar_icon_expose_event (GtkWidget      *widget,
                                    GdkEventExpose *event)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (widget);
  GdkRectangle         icon_area;
  GdkPixbuf           *icon;

  /* draw the icon if we have a file */
  if (G_LIKELY (statusbar_icon->file != NULL))
    {
      icon = thunar_file_load_icon (statusbar_icon->file, statusbar_icon->icon_factory, 16);

      /* use the lucent variant if we're currently loading */
      if (thunar_statusbar_icon_get_loading (statusbar_icon))
        {
          if (G_UNLIKELY (statusbar_icon->lucent == NULL))
            statusbar_icon->lucent = create_lucent_pixbuf (icon);

          g_object_unref (G_OBJECT (icon));
          icon = statusbar_icon->lucent;
          g_object_ref (G_OBJECT (icon));
        }

      icon_area.width = gdk_pixbuf_get_width (icon);
      icon_area.height = gdk_pixbuf_get_height (icon);
      icon_area.x = (widget->allocation.width - icon_area.width) / 2;
      icon_area.y = (widget->allocation.height - icon_area.height) / 2;

      gdk_draw_pixbuf (widget->window, widget->style->black_gc, icon,
                       0, 0, icon_area.x, icon_area.y,
                       icon_area.width, icon_area.height,
                       GDK_RGB_DITHER_NORMAL, 0, 0);

      g_object_unref (G_OBJECT (icon));
    }

#if defined(HAVE_CAIRO) || GTK_CHECK_VERSION(2,7,1)
  /* render the animation */
  if (thunar_statusbar_icon_get_loading (statusbar_icon))
    {
      GdkColor color;
      cairo_t *cr;
      gdouble  n;

      color = widget->style->fg[GTK_STATE_NORMAL];

      cr = get_cairo_context (widget->window);

      cairo_rotate (cr, (statusbar_icon->angle * 2.0 * M_PI) / 360.0);

      for (n = 0.0; n < 2.0; n += 0.25)
        {
          cairo_set_source_rgba (cr, color.red / 65535., color.green / 65535., color.blue / 65535., (n - 0.1) / 1.9);
          cairo_arc (cr, cos (n * M_PI) / 3.0, sin (n * M_PI) / 3.0, 0.12, 0.0, 2 * M_PI);
          cairo_fill (cr);
        }

      cairo_destroy (cr);
    }
#endif

  return TRUE;
}



static void
thunar_statusbar_icon_drag_begin (GtkWidget      *widget,
                                  GdkDragContext *context)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (widget);
  GdkPixbuf           *icon;

  /* setup the drag source icon */
  if (G_LIKELY (statusbar_icon->file != NULL))
    {
      icon = thunar_file_load_icon (statusbar_icon->file, statusbar_icon->icon_factory, 24);
      gtk_drag_source_set_icon_pixbuf (widget, icon);
      g_object_unref (G_OBJECT (icon));
    }

  if (GTK_WIDGET_CLASS (thunar_statusbar_icon_parent_class)->drag_begin != NULL)
    (*GTK_WIDGET_CLASS (thunar_statusbar_icon_parent_class)->drag_begin) (widget, context);
}



static void
thunar_statusbar_icon_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context,
                                     GtkSelectionData *selection_data,
                                     guint             info,
                                     guint             time)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (widget);
  ThunarVfsURI        *uri;
  GList               *uri_list;
  gchar               *uri_string;

  if (G_LIKELY (statusbar_icon->file != NULL))
    {
      /* determine the uri of the file in question */
      uri = thunar_file_get_uri (statusbar_icon->file);

      /* transform the uri into an uri list string */
      uri_list = thunar_vfs_uri_list_prepend (NULL, uri);
      uri_string = thunar_vfs_uri_list_to_string (uri_list, 0);
      thunar_vfs_uri_list_free (uri_list);

      /* set the uri for the drag selection */
      gtk_selection_data_set (selection_data, selection_data->target,
                              8, (guchar *) uri_string, strlen (uri_string));

      /* cleanup */
      g_free (uri_string);
    }
  else
    {
      /* abort the drag */
      gdk_drag_abort (context, time);
    }

  if (GTK_WIDGET_CLASS (thunar_statusbar_icon_parent_class)->drag_data_get != NULL)
    (*GTK_WIDGET_CLASS (thunar_statusbar_icon_parent_class)->drag_data_get) (widget, context, selection_data, info, time);
}



static gboolean
thunar_statusbar_icon_timer (gpointer user_data)
{
  ThunarStatusbarIcon *statusbar_icon = THUNAR_STATUSBAR_ICON (user_data);

  GDK_THREADS_ENTER ();
  statusbar_icon->angle = (statusbar_icon->angle + 30) % 360;
  gtk_widget_queue_draw (GTK_WIDGET (statusbar_icon));
  GDK_THREADS_LEAVE ();

  return TRUE;
}


static void
thunar_statusbar_icon_timer_destroy (gpointer user_data)
{
  THUNAR_STATUSBAR_ICON (user_data)->timer_id = -1;
}



static ThunarFile*
thunar_statusbar_icon_get_file (ThunarStatusbarIcon *statusbar_icon)
{
  g_return_val_if_fail (THUNAR_IS_STATUSBAR_ICON (statusbar_icon), NULL);
  return statusbar_icon->file;
}



static void
thunar_statusbar_icon_set_file (ThunarStatusbarIcon *statusbar_icon,
                                ThunarFile          *file)
{
  g_return_if_fail (THUNAR_IS_STATUSBAR_ICON (statusbar_icon));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* drop any cached icon */
  if (G_LIKELY (statusbar_icon->lucent != NULL))
    {
      g_object_unref (G_OBJECT (statusbar_icon->lucent));
      statusbar_icon->lucent = NULL;
    }

  /* disconnect from the previous file */
  if (G_LIKELY (statusbar_icon->file != NULL))
    {
      g_object_unref (G_OBJECT (statusbar_icon->file));
    }

  /* activate the new file */
  statusbar_icon->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the file */
      g_object_ref (G_OBJECT (file));
    }

  /* schedule a redraw with the new file */
  gtk_widget_queue_draw (GTK_WIDGET (statusbar_icon));
}



static gboolean
thunar_statusbar_icon_get_loading (ThunarStatusbarIcon *statusbar_icon)
{
  g_return_val_if_fail (THUNAR_IS_STATUSBAR_ICON (statusbar_icon), FALSE);
  return (statusbar_icon->timer_id >= 0);
}



static void
thunar_statusbar_icon_set_loading (ThunarStatusbarIcon *statusbar_icon,
                                   gboolean             loading)
{
  g_return_if_fail (THUNAR_IS_STATUSBAR_ICON (statusbar_icon));

  if (loading && statusbar_icon->timer_id < 0)
    {
      statusbar_icon->timer_id = g_timeout_add_full (G_PRIORITY_LOW, 65, thunar_statusbar_icon_timer,
                                                     statusbar_icon, thunar_statusbar_icon_timer_destroy);
    }
  else if (!loading && statusbar_icon->timer_id >= 0)
    {
      g_source_remove (statusbar_icon->timer_id);
    }

  /* schedule a redraw with the new loading state */
  gtk_widget_queue_draw (GTK_WIDGET (statusbar_icon));
}




enum
{
  BAR_PROP_0,
  BAR_PROP_CURRENT_DIRECTORY,
  BAR_PROP_LOADING,
  BAR_PROP_TEXT,
};



static void        thunar_statusbar_class_init            (ThunarStatusbarClass *klass);
static void        thunar_statusbar_navigator_init        (ThunarNavigatorIface *iface);
static void        thunar_statusbar_init                  (ThunarStatusbar      *statusbar);
static void        thunar_statusbar_get_property          (GObject              *object,
                                                           guint                 prop_id,
                                                           GValue               *value,
                                                           GParamSpec           *pspec);
static void        thunar_statusbar_set_property          (GObject              *object,
                                                           guint                 prop_id,
                                                           const GValue         *value,
                                                           GParamSpec           *pspec);
static ThunarFile *thunar_statusbar_get_current_directory (ThunarNavigator      *navigator);
static void        thunar_statusbar_set_current_directory (ThunarNavigator      *navigator,
                                                           ThunarFile           *current_directory);



struct _ThunarStatusbarClass
{
  GtkStatusbarClass __parent__;
};

struct _ThunarStatusbar
{
  GtkStatusbar __parent__;
  GtkWidget   *icon;
  guint        context_id;
};



G_DEFINE_TYPE_WITH_CODE (ThunarStatusbar,
                         thunar_statusbar,
                         GTK_TYPE_STATUSBAR,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR,
                                                thunar_statusbar_navigator_init));



static void
thunar_statusbar_class_init (ThunarStatusbarClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = thunar_statusbar_get_property;
  gobject_class->set_property = thunar_statusbar_set_property;

  /**
   * ThunarStatusbar:current-directory:
   *
   * Inherited from #ThunarNavigator.
   **/
  g_object_class_override_property (gobject_class,
                                    BAR_PROP_CURRENT_DIRECTORY,
                                    "current-directory");

  /**
   * ThunarStatusbar:loading:
   *
   * Write-only property, which tells the statusbar whether to display
   * a "loading" indicator or not.
   **/
  g_object_class_install_property (gobject_class,
                                   BAR_PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         _("Loading"),
                                                         _("Whether to display a loading animation"),
                                                         FALSE,
                                                         EXO_PARAM_WRITABLE));

  /**
   * ThunarStatusbar:text:
   *
   * The main text to be displayed in the statusbar. This property
   * can only be written.
   **/
  g_object_class_install_property (gobject_class,
                                   BAR_PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        _("Statusbar text"),
                                                        _("The main text to be displayed in the statusbar"),
                                                        NULL,
                                                        EXO_PARAM_WRITABLE));
}



static void
thunar_statusbar_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_statusbar_get_current_directory;
  iface->set_current_directory = thunar_statusbar_set_current_directory;
}



static void
thunar_statusbar_init (ThunarStatusbar *statusbar)
{
  GtkWidget *label;
  GtkWidget *box;

  /* remove the label from the statusbars frame */
  label = GTK_STATUSBAR (statusbar)->label;
  g_object_ref (G_OBJECT (label));
  gtk_container_remove (GTK_CONTAINER (GTK_STATUSBAR (statusbar)->frame), label);

  /* add a HBox instead */
  box = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (GTK_STATUSBAR (statusbar)->frame), box);
  gtk_widget_show (box);

  /* add the icon to the HBox */
  statusbar->icon = g_object_new (THUNAR_TYPE_STATUSBAR_ICON, NULL);
  gtk_box_pack_start (GTK_BOX (box), statusbar->icon, FALSE, FALSE, 0);
  gtk_widget_show (statusbar->icon);

  /* readd the label to the HBox */
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  g_object_unref (G_OBJECT (label));
  gtk_widget_show (label);
  
  statusbar->context_id =
    gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Main text");
  gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (statusbar), TRUE);
}



static void
thunar_statusbar_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    case BAR_PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_statusbar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);

  switch (prop_id)
    {
    case BAR_PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case BAR_PROP_LOADING:
      thunar_statusbar_set_loading (statusbar, g_value_get_boolean (value));
      break;

    case BAR_PROP_TEXT:
      thunar_statusbar_set_text (statusbar, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_statusbar_get_current_directory (ThunarNavigator *navigator)
{
  return thunar_statusbar_icon_get_file (THUNAR_STATUSBAR_ICON (THUNAR_STATUSBAR (navigator)->icon));
}



static void
thunar_statusbar_set_current_directory (ThunarNavigator *navigator,
                                        ThunarFile      *current_directory)
{
  /* setup the new directory for the icon */
  thunar_statusbar_icon_set_file (THUNAR_STATUSBAR_ICON (THUNAR_STATUSBAR (navigator)->icon), current_directory);

  /* notify others */
  g_object_notify (G_OBJECT (navigator), "current-directory");
}



/**
 * thunar_statusbar_new:
 *
 * Allocates a new #ThunarStatusbar instance with no
 * text set.
 *
 * Return value: the newly allocated #ThunarStatusbar instance.
 **/
GtkWidget*
thunar_statusbar_new (void)
{
  return g_object_new (THUNAR_TYPE_STATUSBAR, NULL);
}



/**
 * thunar_statusbar_set_loading:
 * @statusbar : a #ThunarStatusbar instance.
 * @loading   : %TRUE if the @statusbar should display a load indicator.
 **/
void
thunar_statusbar_set_loading (ThunarStatusbar *statusbar,
                              gboolean         loading)
{
  g_return_if_fail (THUNAR_IS_STATUSBAR (statusbar));

  thunar_statusbar_icon_set_loading (THUNAR_STATUSBAR_ICON (statusbar->icon), loading);
}



/**
 * thunar_statusbar_set_text:
 * @statusbar : a #ThunarStatusbar instance.
 * @text      : the main text to be displayed in @statusbar.
 *
 * Sets up a new main text for @statusbar.
 **/
void
thunar_statusbar_set_text (ThunarStatusbar *statusbar,
                           const gchar     *text)
{
  g_return_if_fail (THUNAR_IS_STATUSBAR (statusbar));
  g_return_if_fail (text != NULL);

  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), statusbar->context_id);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), statusbar->context_id, text);
}



