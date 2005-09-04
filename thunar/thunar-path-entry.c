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
 *
 * The icon code is based on ideas from SexyIconEntry, which was written by
 * Christian Hammond <chipx86@chipx86.com>.
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

#include <thunar/thunar-path-entry.h>



#define ICON_MARGIN (2)



enum
{
  PROP_0,
  PROP_CURRENT_FILE,
};



static void     thunar_path_entry_class_init            (ThunarPathEntryClass *klass);
static void     thunar_path_entry_editable_init         (GtkEditableClass     *iface);
static void     thunar_path_entry_init                  (ThunarPathEntry      *path_entry);
static void     thunar_path_entry_finalize              (GObject              *object);
static void     thunar_path_entry_get_property          (GObject              *object,  
                                                         guint                 prop_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void     thunar_path_entry_set_property          (GObject              *object,  
                                                         guint                 prop_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static void     thunar_path_entry_size_request          (GtkWidget            *widget,
                                                         GtkRequisition       *requisition);
static void     thunar_path_entry_size_allocate         (GtkWidget            *widget,
                                                         GtkAllocation        *allocation);
static void     thunar_path_entry_realize               (GtkWidget            *widget);
static void     thunar_path_entry_unrealize             (GtkWidget            *widget);
static gboolean thunar_path_entry_focus                 (GtkWidget            *widget,
                                                         GtkDirectionType      direction);
static gboolean thunar_path_entry_expose_event          (GtkWidget            *widget,
                                                         GdkEventExpose       *event);
static gboolean thunar_path_entry_button_press_event    (GtkWidget            *widget,
                                                         GdkEventButton       *event);
static gboolean thunar_path_entry_button_release_event  (GtkWidget            *widget,
                                                         GdkEventButton       *event);
static gboolean thunar_path_entry_motion_notify_event   (GtkWidget            *widget,
                                                         GdkEventMotion       *event);
static void     thunar_path_entry_drag_data_get         (GtkWidget            *widget,
                                                         GdkDragContext       *context,
                                                         GtkSelectionData     *selection_data,
                                                         guint                 info,
                                                         guint                 time);
static void     thunar_path_entry_activate              (GtkEntry             *entry);
static void     thunar_path_entry_changed               (GtkEditable          *editable);
static void     thunar_path_entry_get_borders           (ThunarPathEntry      *path_entry,
                                                         gint                 *xborder,
                                                         gint                 *yborder);
static void     thunar_path_entry_get_text_area_size    (ThunarPathEntry      *path_entry,
                                                         gint                 *x,
                                                         gint                 *y,
                                                         gint                 *width,
                                                         gint                 *height);



struct _ThunarPathEntryClass
{
  GtkEntryClass __parent__;
};

struct _ThunarPathEntry
{
  GtkEntry __parent__;

  ThunarIconFactory *icon_factory;
  ThunarFile        *current_file;
  GdkWindow         *icon_area;

  gint               drag_button;
  gint               drag_x;
  gint               drag_y;
};



static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, 0, },
};

static GtkEditableClass *thunar_path_entry_editable_parent_iface;

G_DEFINE_TYPE_WITH_CODE (ThunarPathEntry,
                         thunar_path_entry,
                         GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                thunar_path_entry_editable_init));



static void
thunar_path_entry_class_init (ThunarPathEntryClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkEntryClass  *gtkentry_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_path_entry_finalize;
  gobject_class->get_property = thunar_path_entry_get_property;
  gobject_class->set_property = thunar_path_entry_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunar_path_entry_size_request;
  gtkwidget_class->size_allocate = thunar_path_entry_size_allocate;
  gtkwidget_class->realize = thunar_path_entry_realize;
  gtkwidget_class->unrealize = thunar_path_entry_unrealize;
  gtkwidget_class->focus = thunar_path_entry_focus;
  gtkwidget_class->expose_event = thunar_path_entry_expose_event;
  gtkwidget_class->button_press_event = thunar_path_entry_button_press_event;
  gtkwidget_class->button_release_event = thunar_path_entry_button_release_event;
  gtkwidget_class->motion_notify_event = thunar_path_entry_motion_notify_event;
  gtkwidget_class->drag_data_get = thunar_path_entry_drag_data_get;

  gtkentry_class = GTK_ENTRY_CLASS (klass);
  gtkentry_class->activate = thunar_path_entry_activate;

  /**
   * ThunarPathEntry:current-file:
   *
   * The #ThunarFile currently displayed by the path entry or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_FILE,
                                   g_param_spec_object ("current-file",
                                                        _("Current file"),
                                                        _("The currently displayed file"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPathEntry:icon-size:
   *
   * The preferred size of the icon displayed in the path entry.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("icon-size",
                                                             _("Icon size"),
                                                             _("The icon size for the path entry"),
                                                             1, G_MAXINT, 16, EXO_PARAM_READABLE));
}



static void
thunar_path_entry_editable_init (GtkEditableClass *iface)
{
  thunar_path_entry_editable_parent_iface = g_type_interface_peek_parent (iface);

  iface->changed = thunar_path_entry_changed;
}



static void
thunar_path_entry_init (ThunarPathEntry *path_entry)
{
  path_entry->drag_button = -1;
}



static void
thunar_path_entry_finalize (GObject *object)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  if (G_LIKELY (path_entry->current_file != NULL))
    g_object_unref (G_OBJECT (path_entry->current_file));

  G_OBJECT_CLASS (thunar_path_entry_parent_class)->finalize (object);
}



static void
thunar_path_entry_get_property (GObject    *object,  
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURRENT_FILE:
      g_value_set_object (value, thunar_path_entry_get_current_file (path_entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_entry_set_property (GObject      *object,  
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURRENT_FILE:
      thunar_path_entry_set_current_file (path_entry, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_entry_size_request (GtkWidget      *widget,
                                GtkRequisition *requisition)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  gint             text_height;
  gint             icon_size;
  gint             xborder;
  gint             yborder;

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "icon-size", &icon_size,
                        NULL);

  GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->size_request (widget, requisition);

  thunar_path_entry_get_text_area_size (path_entry, &xborder, &yborder, NULL, &text_height);

  requisition->width += icon_size + xborder + 2 * ICON_MARGIN;
  requisition->height = 2 * yborder + MAX (icon_size + 2 * ICON_MARGIN, text_height);
}



static void
thunar_path_entry_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GtkAllocation    icon_allocation;
  GtkAllocation    text_allocation;
  gint             icon_size;
  gint             text_height;
  gint             xborder;
  gint             yborder;

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "icon-size", &icon_size,
                        NULL);

  widget->allocation = *allocation;

  thunar_path_entry_get_text_area_size (path_entry, &xborder, &yborder, NULL, &text_height);

  text_allocation.y = yborder;
  text_allocation.width = allocation->width - icon_size - 2 * xborder - 2 * ICON_MARGIN;
  text_allocation.height = text_height;

  icon_allocation.y = yborder;
  icon_allocation.width = icon_size + 2 * ICON_MARGIN;
  icon_allocation.height = MAX (icon_size + 2 * ICON_MARGIN, text_height);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      text_allocation.x = xborder;
      icon_allocation.x = allocation->width - icon_allocation.width - xborder - 2 * ICON_MARGIN;
    }
  else
    {
      icon_allocation.x = xborder;
      text_allocation.x = allocation->width - text_allocation.width - xborder;
    }

  GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->size_allocate (widget, allocation);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (GTK_ENTRY (path_entry)->text_area,
                              text_allocation.x,
                              text_allocation.y,
                              text_allocation.width,
                              text_allocation.height);

      gdk_window_move_resize (path_entry->icon_area,
                              icon_allocation.x,
                              icon_allocation.y,
                              icon_allocation.width,
                              icon_allocation.height);
    }
}



static void
thunar_path_entry_realize (GtkWidget *widget)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GdkWindowAttr    attributes;
  GtkIconTheme    *icon_theme;
  gint             attributes_mask;
  gint             text_height;
  gint             icon_size;
  gint             spacing;

  /* query the proper icon factory */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  path_entry->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "icon-size", &icon_size,
                        NULL);

  GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->realize (widget);

  thunar_path_entry_get_text_area_size (path_entry, NULL, NULL, NULL, &text_height);
  spacing = widget->requisition.height -text_height;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_ENTER_NOTIFY_MASK
                        | GDK_EXPOSURE_MASK
                        | GDK_LEAVE_NOTIFY_MASK
                        | GDK_POINTER_MOTION_MASK;
  attributes.x = widget->allocation.x + widget->allocation.width - icon_size - spacing;
  attributes.y = widget->allocation.y + (widget->allocation.height - widget->requisition.height) / 2;
  attributes.width = icon_size + spacing;
  attributes.height = widget->requisition.height;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  path_entry->icon_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (path_entry->icon_area, widget);
  gdk_window_set_background (path_entry->icon_area, &widget->style->base[GTK_WIDGET_STATE (widget)]);
  gdk_window_show (path_entry->icon_area);

  gtk_widget_queue_resize (widget);
}



static void
thunar_path_entry_unrealize (GtkWidget *widget)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);

  /* destroy the icon window */
  gdk_window_set_user_data (path_entry->icon_area, NULL);
  gdk_window_destroy (path_entry->icon_area);
  path_entry->icon_area = NULL;

  /* disconnect from the icon factory */
  g_object_unref (G_OBJECT (path_entry->icon_factory));
  path_entry->icon_factory = NULL;

  /* let the GtkWidget class do the rest */
  GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->unrealize (widget);
}



static gboolean
thunar_path_entry_focus (GtkWidget       *widget,
                         GtkDirectionType direction)
{
  return GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->focus (widget, direction);
}



static gboolean
thunar_path_entry_expose_event (GtkWidget      *widget,
                                GdkEventExpose *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GdkPixbuf       *icon;
  gint             icon_height;
  gint             icon_width;
  gint             icon_size;
  gint             height;
  gint             width;

  if (event->window == path_entry->icon_area)
    {
      gtk_widget_style_get (GTK_WIDGET (widget),
                            "icon-size", &icon_size,
                            NULL);

      gdk_drawable_get_size (GDK_DRAWABLE (path_entry->icon_area), &width, &height);

      gtk_paint_flat_box (widget->style, path_entry->icon_area,
                          GTK_WIDGET_STATE (widget), GTK_SHADOW_NONE,
                          NULL, widget, "entry_bg",
                          0, 0, width, height);

      if (path_entry->current_file == NULL)
        icon = gtk_widget_render_icon (widget, GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_SMALL_TOOLBAR, "path_entry");
      else
        icon = thunar_file_load_icon (path_entry->current_file, THUNAR_FILE_ICON_STATE_DEFAULT, path_entry->icon_factory, icon_size);

      if (G_LIKELY (icon != NULL))
        {
          icon_width = gdk_pixbuf_get_width (icon);
          icon_height = gdk_pixbuf_get_height (icon);

          gdk_draw_pixbuf (path_entry->icon_area,
                           widget->style->black_gc,
                           icon, 0, 0,
                           (width - icon_width) / 2,
                           (height - icon_height) / 2,
                           icon_width, icon_height,
                           GDK_RGB_DITHER_NORMAL, 0, 0);

          g_object_unref (G_OBJECT (icon));
        }
    }
  else
    {
      return GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->expose_event (widget, event);
    }

  return TRUE;
}



static gboolean
thunar_path_entry_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);

  if (event->window == path_entry->icon_area && event->button == 1)
    {
      /* consume the event */
      path_entry->drag_button = event->button;
      path_entry->drag_x = event->x;
      path_entry->drag_x = event->y;
      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->button_press_event) (widget, event);
}



static gboolean
thunar_path_entry_button_release_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);

  if (event->window == path_entry->icon_area && event->button == path_entry->drag_button)
    {
      /* reset the drag button state */
      path_entry->drag_button = -1;
      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->button_release_event) (widget, event);
}



static gboolean
thunar_path_entry_motion_notify_event (GtkWidget      *widget,
                                       GdkEventMotion *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GdkDragContext  *context;
  GtkTargetList   *target_list;
  GdkPixbuf       *icon;
  gint             size;

  if (event->window == path_entry->icon_area && path_entry->drag_button >= 0 && path_entry->current_file != NULL
      && gtk_drag_check_threshold (widget, path_entry->drag_x, path_entry->drag_y, event->x, event->y))
    {
      /* create the drag context */
      target_list = gtk_target_list_new (drag_targets, G_N_ELEMENTS (drag_targets));
      context = gtk_drag_begin (widget, target_list, GDK_ACTION_COPY | GDK_ACTION_LINK, path_entry->drag_button, (GdkEvent *) event);
      gtk_target_list_unref (target_list);

      /* setup the drag icon (atleast 24px) */
      gtk_widget_style_get (widget, "icon-size", &size, NULL);
      icon = thunar_file_load_icon (path_entry->current_file, THUNAR_FILE_ICON_STATE_DEFAULT, path_entry->icon_factory, MAX (size, 24));
      if (G_LIKELY (icon != NULL))
        {
          gtk_drag_set_icon_pixbuf (context, icon, 0, 0);
          g_object_unref (G_OBJECT (icon));
        }

      /* reset the drag button state */
      path_entry->drag_button = -1;

      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->motion_notify_event) (widget, event);
}



static void
thunar_path_entry_drag_data_get (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint             time)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GList            uri_list;
  gchar           *uri_string;

  /* verify that we actually display a path */
  if (G_LIKELY (path_entry->current_file != NULL))
    {
      /* transform the uri for the current path into an uri string list */
      uri_list.data = thunar_file_get_uri (path_entry->current_file); uri_list.next = uri_list.prev = NULL;
      uri_string = thunar_vfs_uri_list_to_string (&uri_list, THUNAR_VFS_URI_STRING_ESCAPED);

      /* setup the uri list for the drag selection */
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) uri_string, strlen (uri_string));

      /* clean up */
      g_free (uri_string);
    }
}



static void
thunar_path_entry_activate (GtkEntry *entry)
{
  GTK_ENTRY_CLASS (thunar_path_entry_parent_class)->activate (entry);
}



static void
thunar_path_entry_changed (GtkEditable *editable)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (editable);
  ThunarVfsURI    *uri;
  const gchar     *text;
  ThunarFile      *file;

  text = gtk_entry_get_text (GTK_ENTRY (editable));
  uri = thunar_vfs_uri_new (text, NULL);
  file = (uri != NULL) ? thunar_file_get_for_uri (uri, NULL) : NULL;

  if (file != path_entry->current_file)
    {
      if (G_UNLIKELY (path_entry->current_file != NULL))
        g_object_unref (G_OBJECT (path_entry->current_file));

      path_entry->current_file = file;

      if (G_UNLIKELY (file != NULL))
        g_object_ref (G_OBJECT (file));

      g_object_notify (G_OBJECT (path_entry), "current-file");
    }

  if (G_UNLIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (uri != NULL))
    thunar_vfs_uri_unref (uri);
}



static void
thunar_path_entry_get_borders (ThunarPathEntry *path_entry,
                               gint            *xborder,
                               gint            *yborder)
{
	gboolean interior_focus;
	gint     focus_width;

	gtk_widget_style_get (GTK_WIDGET (path_entry),
                        "focus-line-width", &focus_width,
                        "interior-focus", &interior_focus,
                        NULL);

	if (gtk_entry_get_has_frame (GTK_ENTRY (path_entry)))
    {
		  *xborder = GTK_WIDGET (path_entry)->style->xthickness;
  		*yborder = GTK_WIDGET (path_entry)->style->ythickness;
	  }
	else
	  {
  		*xborder = 0;
	  	*yborder = 0;
  	}

	if (!interior_focus)
	  {
  		*xborder += focus_width;
	  	*yborder += focus_width;
  	}
}



static void
thunar_path_entry_get_text_area_size (ThunarPathEntry *path_entry,
                                      gint            *x,
                                      gint            *y,
                                      gint            *width,
                                      gint            *height)
{
	GtkRequisition requisition;
	GtkWidget     *widget = GTK_WIDGET (path_entry);
	gint           xborder;
  gint           yborder;

	gtk_widget_get_child_requisition (widget, &requisition);

  thunar_path_entry_get_borders (path_entry, &xborder, &yborder);

	if (x != NULL) *x = xborder;
	if (y != NULL) *y = yborder;
	if (width  != NULL) *width  = widget->allocation.width - xborder * 2;
	if (height != NULL) *height = requisition.height - yborder * 2;
}



/**
 * thunar_path_entry_new:
 * 
 * Allocates a new #ThunarPathEntry instance.
 *
 * Return value: the newly allocated #ThunarPathEntry.
 **/
GtkWidget*
thunar_path_entry_new (void)
{
  return g_object_new (THUNAR_TYPE_PATH_ENTRY, NULL);
}



/**
 * thunar_path_entry_get_current_file:
 * @path_entry : a #ThunarPathEntry.
 *
 * Returns the #ThunarFile currently being displayed by
 * @path_entry or %NULL if @path_entry doesn't contain
 * a valid #ThunarFile.
 *
 * Return value: the #ThunarFile for @path_entry or %NULL.
 **/
ThunarFile*
thunar_path_entry_get_current_file (ThunarPathEntry *path_entry)
{
  g_return_val_if_fail (THUNAR_IS_PATH_ENTRY (path_entry), NULL);
  return path_entry->current_file;
}



/**
 * thunar_path_entry_set_current_file:
 * @path_entry   : a #ThunarPathEntry.
 * @current_file : a #ThunarFile or %NULL.
 *
 * Sets the #ThunarFile that should be displayed by
 * @path_entry to @current_file.
 **/
void
thunar_path_entry_set_current_file (ThunarPathEntry *path_entry,
                                    ThunarFile      *current_file)
{
  ThunarVfsURI *uri;
  gchar        *uri_string;

  g_return_if_fail (THUNAR_IS_PATH_ENTRY (path_entry));
  g_return_if_fail (current_file == NULL || THUNAR_IS_FILE (current_file));

  uri = (current_file != NULL) ? thunar_file_get_uri (current_file) : NULL;
  if (G_UNLIKELY (uri == NULL))
    {
      gtk_entry_set_text (GTK_ENTRY (path_entry), "");
    }
  else if (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_FILE)
    {
      gtk_entry_set_text (GTK_ENTRY (path_entry), thunar_vfs_uri_get_path (uri));
    }
  else
    {
      uri_string = thunar_vfs_uri_to_string (uri, THUNAR_VFS_URI_STRING_UTF8);
      gtk_entry_set_text (GTK_ENTRY (path_entry), uri_string);
      g_free (uri_string);
    }

  gtk_editable_set_position (GTK_EDITABLE (path_entry), -1);

  gtk_widget_queue_draw (GTK_WIDGET (path_entry));
}

