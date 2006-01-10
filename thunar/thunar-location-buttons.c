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
 * Based on code written by Jonathan Blandford <jrb@gnome.org> for
 * Red Hat, Inc.
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

#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-pango-extensions.h>



#define THUNAR_LOCATION_BUTTONS_SCROLL_TIMEOUT  (200)



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void        thunar_location_buttons_class_init             (ThunarLocationButtonsClass *klass);
static void        thunar_location_buttons_navigator_init         (ThunarNavigatorIface       *iface);
static void        thunar_location_buttons_location_bar_init      (ThunarLocationBarIface     *iface);
static void        thunar_location_buttons_init                   (ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_finalize               (GObject                    *object);
static void        thunar_location_buttons_get_property           (GObject                    *object,
                                                                   guint                       prop_id,
                                                                   GValue                     *value,
                                                                   GParamSpec                 *pspec);
static void        thunar_location_buttons_set_property           (GObject                    *object,
                                                                   guint                       prop_id,
                                                                   const GValue               *value,
                                                                   GParamSpec                 *pspec);
static ThunarFile *thunar_location_buttons_get_current_directory  (ThunarNavigator            *navigator);
static void        thunar_location_buttons_set_current_directory  (ThunarNavigator            *navigator,
                                                                   ThunarFile                 *current_directory);
static gboolean    thunar_location_buttons_accept_focus           (ThunarLocationBar          *location_bar);
static void        thunar_location_buttons_unmap                  (GtkWidget                  *widget);
static void        thunar_location_buttons_size_request           (GtkWidget                  *widget,
                                                                   GtkRequisition             *requisition);
static void        thunar_location_buttons_size_allocate          (GtkWidget                  *widget,
                                                                   GtkAllocation              *allocation);
static void        thunar_location_buttons_state_changed          (GtkWidget                  *widget,
                                                                   GtkStateType                previous_state);
static void        thunar_location_buttons_style_set              (GtkWidget                  *widget,
                                                                   GtkStyle                   *previous_style);
static void        thunar_location_buttons_grab_notify            (GtkWidget                  *widget,
                                                                   gboolean                    was_grabbed);
static void        thunar_location_buttons_add                    (GtkContainer               *container,
                                                                   GtkWidget                  *widget);
static void        thunar_location_buttons_remove                 (GtkContainer               *container,
                                                                   GtkWidget                  *widget);
static void        thunar_location_buttons_forall                 (GtkContainer               *container,
                                                                   gboolean                    include_internals,
                                                                   GtkCallback                 callback,
                                                                   gpointer                    callback_data);
static GtkWidget  *thunar_location_buttons_make_button            (ThunarLocationButtons      *buttons,
                                                                   ThunarFile                 *file);
static void        thunar_location_buttons_remove_1               (GtkContainer               *container,
                                                                   GtkWidget                  *widget);
static gboolean    thunar_location_buttons_scroll_timeout         (gpointer                    user_data);
static void        thunar_location_buttons_scroll_timeout_destroy (gpointer                    user_data);
static void        thunar_location_buttons_stop_scrolling         (ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_update_sliders         (ThunarLocationButtons      *buttons);
static gboolean    thunar_location_buttons_slider_button_press    (GtkWidget                  *button,
                                                                   GdkEventButton             *event,
                                                                   ThunarLocationButtons      *buttons);
static gboolean    thunar_location_buttons_slider_button_release  (GtkWidget                  *button,
                                                                   GdkEventButton             *event,
                                                                   ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_scroll_left            (GtkWidget                  *button,
                                                                   ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_scroll_right           (GtkWidget                  *button,
                                                                   ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_clicked                (GtkWidget                  *button,
                                                                   ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_drag_data_get          (GtkWidget                  *button,
                                                                   GdkDragContext             *context,
                                                                   GtkSelectionData           *selection_data,
                                                                   guint                       info,
                                                                   guint                       time,
                                                                   ThunarLocationButtons      *buttons);
static void        thunar_location_buttons_drag_leave             (GtkWidget                  *button,
                                                                   GdkDragContext             *context,
                                                                   guint                       time,
                                                                   ThunarLocationButtons      *buttons);
static gboolean    thunar_location_buttons_drag_motion            (GtkWidget                  *button,
                                                                   GdkDragContext             *context,
                                                                   gint                        x,
                                                                   gint                        y,
                                                                   guint                       time,
                                                                   ThunarLocationButtons      *buttons);
static gboolean    thunar_location_buttons_enter_timeout          (gpointer                    user_data);
static void        thunar_location_buttons_enter_timeout_destroy  (gpointer                    user_data);



struct _ThunarLocationButtonsClass
{
  GtkContainerClass __parent__;
};

struct _ThunarLocationButtons
{
  GtkContainer __parent__;

  GtkWidget     *left_slider;
  GtkWidget     *right_slider;

  ThunarFile    *current_directory;

  gint           slider_width;
  gboolean       ignore_click : 1;

  GList         *list;
  GList         *first_scrolled_button;

  gint           scroll_timeout_id;

  GtkWidget     *enter_button;
  gint           enter_timeout_id;
};


static GQuark gtk_label_quark = 0;
static GQuark thunar_file_quark = 0;

static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, 0 },
};



static GObjectClass *thunar_location_buttons_parent_class;



GType
thunar_location_buttons_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarLocationButtonsClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_location_buttons_class_init,
        NULL,
        NULL,
        sizeof (ThunarLocationButtons),
        0,
        (GInstanceInitFunc) thunar_location_buttons_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_location_buttons_navigator_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo location_bar_info =
      {
        (GInterfaceInitFunc) thunar_location_buttons_location_bar_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_CONTAINER, I_("ThunarLocationButtons"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_LOCATION_BAR, &location_bar_info);
    }

  return type;
}



static void
thunar_location_buttons_class_init (ThunarLocationButtonsClass *klass)
{
  GtkContainerClass *gtkcontainer_class;
  GtkWidgetClass    *gtkwidget_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_location_buttons_parent_class = g_type_class_peek_parent (klass);

  gtk_label_quark = g_quark_from_static_string ("gtk-label");
  thunar_file_quark = g_quark_from_static_string ("thunar-file");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_buttons_finalize;
  gobject_class->get_property = thunar_location_buttons_get_property;
  gobject_class->set_property = thunar_location_buttons_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unmap = thunar_location_buttons_unmap;
  gtkwidget_class->size_request = thunar_location_buttons_size_request;
  gtkwidget_class->size_allocate = thunar_location_buttons_size_allocate;
  gtkwidget_class->state_changed = thunar_location_buttons_state_changed;
  gtkwidget_class->style_set = thunar_location_buttons_style_set;
  gtkwidget_class->grab_notify = thunar_location_buttons_grab_notify;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = thunar_location_buttons_add;
  gtkcontainer_class->remove = thunar_location_buttons_remove;
  gtkcontainer_class->forall = thunar_location_buttons_forall;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");

  /**
   * ThunarLocationButtons:spacing:
   *
   * The amount of space between the path buttons.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("spacing",
                                                             _("Spacing"),
                                                             _("The amount of space between the path buttons"),
                                                             0, G_MAXINT, 3,
                                                             G_PARAM_READABLE));
}



static void
thunar_location_buttons_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_location_buttons_get_current_directory;
  iface->set_current_directory = thunar_location_buttons_set_current_directory;
}



static void
thunar_location_buttons_location_bar_init (ThunarLocationBarIface *iface)
{
  iface->accept_focus = thunar_location_buttons_accept_focus;
  iface->is_standalone = (gpointer) exo_noop_true;
}



static void
thunar_location_buttons_init (ThunarLocationButtons *buttons)
{
  GtkWidget *arrow;

  GTK_WIDGET_SET_FLAGS (buttons, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (buttons), FALSE);

  buttons->enter_timeout_id = -1;
  buttons->scroll_timeout_id = -1;

  gtk_widget_push_composite_child ();

  buttons->left_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (buttons->left_slider), "button-press-event",
                    G_CALLBACK (thunar_location_buttons_slider_button_press), buttons);
  g_signal_connect (G_OBJECT (buttons->left_slider), "button-release-event",
                    G_CALLBACK (thunar_location_buttons_slider_button_release), buttons);
  g_signal_connect (G_OBJECT (buttons->left_slider), "clicked",
                    G_CALLBACK (thunar_location_buttons_scroll_left), buttons);
  gtk_container_add (GTK_CONTAINER (buttons), buttons->left_slider);
  gtk_widget_show (buttons->left_slider);

  arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (buttons->left_slider), arrow);
  gtk_widget_show (arrow);

  buttons->right_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (buttons->right_slider), "button-press-event",
                    G_CALLBACK (thunar_location_buttons_slider_button_press), buttons);
  g_signal_connect (G_OBJECT (buttons->right_slider), "button-release-event",
                    G_CALLBACK (thunar_location_buttons_slider_button_release), buttons);
  g_signal_connect (G_OBJECT (buttons->right_slider), "clicked",
                    G_CALLBACK (thunar_location_buttons_scroll_right), buttons);
  gtk_container_add (GTK_CONTAINER (buttons), buttons->right_slider);
  gtk_widget_show (buttons->right_slider);

  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (buttons->right_slider), arrow);
  gtk_widget_show (arrow);

  gtk_widget_pop_composite_child ();
}



static void
thunar_location_buttons_finalize (GObject *object)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (object);

  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* be sure to cancel any enter timeout */
  if (G_UNLIKELY (buttons->enter_timeout_id >= 0))
    g_source_remove (buttons->enter_timeout_id);

  /* be sure to cancel the scrolling */
  thunar_location_buttons_stop_scrolling (buttons);

  /* release from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (buttons), NULL);

  (*G_OBJECT_CLASS (thunar_location_buttons_parent_class)->finalize) (object);
}



static void
thunar_location_buttons_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (navigator));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_location_buttons_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (navigator, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_location_buttons_get_current_directory (ThunarNavigator *navigator)
{
  g_return_val_if_fail (THUNAR_IS_LOCATION_BUTTONS (navigator), NULL);
  return THUNAR_LOCATION_BUTTONS (navigator)->current_directory;
}



static void
thunar_location_buttons_set_current_directory (ThunarNavigator *navigator,
                                               ThunarFile      *current_directory)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (navigator);
  ThunarFile            *file_parent;
  ThunarFile            *file;
  GtkWidget             *button;
  GList                 *lp;

  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* verify that we're not already using the new directory */
  if (G_UNLIKELY (buttons->current_directory == current_directory))
    return;

  /* check if we already have a button for that directory */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (g_object_get_qdata (G_OBJECT (lp->data), thunar_file_quark) == current_directory)
      break;

  /* if we already have a button for that directory, just activate it */
  if (G_UNLIKELY (lp != NULL))
    {
      /* fake a "clicked" event for that button */
      gtk_button_clicked (GTK_BUTTON (lp->data));
    }
  else
    {
      /* regenerate the button list */
      if (G_LIKELY (buttons->current_directory != NULL))
        {
          g_object_unref (G_OBJECT (buttons->current_directory));

          /* remove all buttons */
          while (buttons->list != NULL)
            gtk_container_remove (GTK_CONTAINER (buttons), buttons->list->data);

          /* clear the first scrolled button state */
          buttons->first_scrolled_button = NULL;
        }

      buttons->current_directory = current_directory;

      if (G_LIKELY (current_directory != NULL))
        {
          g_object_ref (G_OBJECT (current_directory));

          gtk_widget_push_composite_child ();

          /* add the new buttons */
          for (file = current_directory; file != NULL; file = file_parent)
            {
              button = thunar_location_buttons_make_button (buttons, file);
              buttons->list = g_list_append (buttons->list, button);
              gtk_container_add (GTK_CONTAINER (buttons), button);
              gtk_widget_show (button);

              /* we use 'Home' as possible root, as well as real root nodes */
              if (!thunar_file_is_home (file) && !thunar_file_is_root (file))
                file_parent = thunar_file_get_parent (file, NULL);
              else 
                file_parent = NULL;

              if (G_LIKELY (file != current_directory))
                g_object_unref (G_OBJECT (file));
            }

          gtk_widget_pop_composite_child ();
        }
    }

  g_object_notify (G_OBJECT (buttons), "current-directory");
}



static gboolean
thunar_location_buttons_accept_focus (ThunarLocationBar *location_bar)
{
  return FALSE;
}



static void
thunar_location_buttons_unmap (GtkWidget *widget)
{
  /* be sure to stop the scroll timer before the buttons are unmapped */
  thunar_location_buttons_stop_scrolling (THUNAR_LOCATION_BUTTONS (widget));

  /* do the real unmap */
  (*GTK_WIDGET_CLASS (thunar_location_buttons_parent_class)->unmap) (widget);
}



static void
thunar_location_buttons_size_request (GtkWidget      *widget,
                                      GtkRequisition *requisition)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  GtkRequisition         child_requisition;
  GList                 *lp;
  gint                   spacing;

  gtk_widget_style_get (GTK_WIDGET (buttons),
                        "spacing", &spacing,
                        NULL);

  requisition->width = 0;
  requisition->height = 0;

  /* calculate the size of the biggest button */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      gtk_widget_size_request (GTK_WIDGET (lp->data), &child_requisition);
      requisition->width = MAX (child_requisition.width, requisition->width);
      requisition->height = MAX (child_requisition.height, requisition->height);
    }

  /* add space for the sliders if we have more than one path */
  buttons->slider_width = MIN (requisition->height * 2 / 3 + 5, requisition->height);
  if (buttons->list != NULL && buttons->list->next != NULL)
    requisition->width += (spacing + buttons->slider_width) * 2;

  gtk_widget_size_request (buttons->left_slider, &child_requisition);
  gtk_widget_size_request (buttons->right_slider, &child_requisition);

  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;

  widget->requisition = *requisition;
}



static void
thunar_location_buttons_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  GtkTextDirection       direction;
  GtkAllocation          child_allocation;
  GtkWidget             *child;
  gboolean               need_sliders = FALSE;
  gboolean               reached_end;
  GList                 *first_button;
  GList                 *lp;
  gint                   left_slider_offset = 0;
  gint                   right_slider_offset = 0;
  gint                   allocation_width;
  gint                   border_width;
  gint                   slider_space;
  gint                   spacing;
  gint                   width;

  gtk_widget_style_get (GTK_WIDGET (buttons),
                        "spacing", &spacing,
                        NULL);

  widget->allocation = *allocation;

  /* if no path is set, we don't have to allocate anything */
  if (G_UNLIKELY (buttons->list == NULL))
    return;

  direction = gtk_widget_get_direction (widget);
  border_width = GTK_CONTAINER (buttons)->border_width;
  allocation_width = allocation->width - 2 * border_width;

  /* check if we need to display the sliders */
  width = GTK_WIDGET (buttons->list->data)->requisition.width;
  for (lp = buttons->list->next; lp != NULL; lp = lp->next)
    width += GTK_WIDGET (lp->data)->requisition.width + spacing;

  if (width <= allocation_width)
    {
      first_button = g_list_last (buttons->list);
    }
  else
    {
      reached_end = FALSE;
      slider_space = 2 * (spacing + buttons->slider_width);

      if (buttons->first_scrolled_button != NULL)
        first_button = buttons->first_scrolled_button;
      else
        first_button = buttons->list;
      need_sliders = TRUE;

      width = GTK_WIDGET (first_button->data)->requisition.width;
      for (lp = first_button->prev; lp != NULL && !reached_end; lp = lp->prev)
        {
          child = lp->data;
          if (width + child->requisition.width + spacing + slider_space + allocation_width)
            reached_end = TRUE;
          else
            width += child->requisition.width + spacing;
        }

      while (first_button->next != NULL && !reached_end)
        {
          child = first_button->data;
          if (width + child->requisition.width + spacing + slider_space > allocation_width)
            {
              reached_end = TRUE;
            }
          else
            {
              width += child->requisition.width + spacing;
              first_button = first_button->next;
            }
        }
    }

  /* Now we allocate space to the buttons */
  child_allocation.y = allocation->y + border_width;
  child_allocation.height = MAX (1, allocation->height - border_width * 2);

  if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
    {
      child_allocation.x = allocation->x + allocation->width - border_width;
      if (need_sliders)
        {
          child_allocation.x -= spacing + buttons->slider_width;
          left_slider_offset = allocation->width - border_width - buttons->slider_width;
        }
    }
  else
    {
      child_allocation.x = allocation->x + border_width;
      if (need_sliders)
        {
          left_slider_offset = border_width;
          child_allocation.x += spacing + buttons->slider_width;
        }
    }

  for (lp = first_button; lp != NULL; lp = lp->prev)
    {
      child = lp->data;

      child_allocation.width = child->requisition.width;
      if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
        child_allocation.x -= child_allocation.width;

      /* check to see if we don't have any more space to allocate buttons */
      if (need_sliders && direction == GTK_TEXT_DIR_RTL)
        {
          if (child_allocation.x - spacing - buttons->slider_width < widget->allocation.x + border_width)
            break;
        }
      else if (need_sliders && direction == GTK_TEXT_DIR_LTR)
        {
          if (child_allocation.x + child_allocation.width + spacing + buttons->slider_width > widget->allocation.x + border_width + allocation_width)
            break;
        }

      gtk_widget_set_child_visible (GTK_WIDGET (lp->data), TRUE);
      gtk_widget_size_allocate (child, &child_allocation);

      if (direction == GTK_TEXT_DIR_RTL)
        {
          child_allocation.x -= spacing;
          right_slider_offset = border_width;
        }
      else
        {
          child_allocation.x += child_allocation.width + spacing;
          right_slider_offset = allocation->width - border_width - buttons->slider_width;
        }
    }

  /* now we go hide all the buttons that don't fit */
  for (; lp != NULL; lp = lp->prev)
    gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);
  for (lp = first_button->next; lp != NULL; lp = lp->next)
    gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);

  if (need_sliders)
    {
      child_allocation.width = buttons->slider_width;
      child_allocation.x = left_slider_offset + allocation->x;
      gtk_widget_size_allocate (buttons->left_slider, &child_allocation);
      gtk_widget_set_child_visible (buttons->left_slider, TRUE);
      gtk_widget_show_all (buttons->left_slider);

      child_allocation.x = right_slider_offset + allocation->x;
      gtk_widget_size_allocate (buttons->right_slider, &child_allocation);
      gtk_widget_set_child_visible (buttons->right_slider, TRUE);
      gtk_widget_show_all (buttons->right_slider);

      thunar_location_buttons_update_sliders (buttons);
    }
  else
    {
      gtk_widget_set_child_visible (buttons->left_slider, FALSE);
      gtk_widget_set_child_visible (buttons->right_slider, FALSE);
    }
}



static void
thunar_location_buttons_state_changed (GtkWidget   *widget,
                                       GtkStateType previous_state)
{
  if (!GTK_WIDGET_IS_SENSITIVE (widget))
    thunar_location_buttons_stop_scrolling (THUNAR_LOCATION_BUTTONS (widget));
}



static void
thunar_location_buttons_style_set (GtkWidget *widget,
                                   GtkStyle  *previous_style)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  ThunarIconFactory     *icon_factory;
  GtkIconTheme          *icon_theme;
  ThunarFile            *file;
  GdkPixbuf             *icon;
  GList                 *children;
  GList                 *lp;
  gint                   size;

  if (G_LIKELY (GTK_WIDGET_REALIZED (widget)))
    {
      /* lookup the icon size for buttons */
      gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &size, &size);

      /* determine the icon factory to use */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      /* update the icons for every button */
      for (lp = buttons->list; lp != NULL; lp = lp->next)
        {
          file = g_object_get_qdata (G_OBJECT (lp->data), thunar_file_quark);
          children = gtk_container_get_children (GTK_CONTAINER (GTK_BIN (lp->data)->child));
          icon = thunar_icon_factory_load_file_icon (icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, size);
          gtk_drag_source_set_icon_pixbuf (GTK_WIDGET (lp->data), icon);
          gtk_image_set_from_pixbuf (GTK_IMAGE (children->data), icon);
          g_object_unref (G_OBJECT (icon));
          g_list_free (children);
        }

      /* release the icon factory */
      g_object_unref (G_OBJECT (icon_factory));
    }

  (*GTK_WIDGET_CLASS (thunar_location_buttons_parent_class)->style_set) (widget, previous_style);
}



static void
thunar_location_buttons_grab_notify (GtkWidget *widget,
                                     gboolean   was_grabbed)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);

  if (!was_grabbed)
    thunar_location_buttons_stop_scrolling (buttons);
}



static void
thunar_location_buttons_add (GtkContainer *container,
                             GtkWidget    *widget)
{
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}



static void
thunar_location_buttons_remove (GtkContainer *container,
                                GtkWidget    *widget)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (container);
  GList                 *lp;

  if (widget == buttons->left_slider)
    {
      thunar_location_buttons_remove_1 (container, widget);
      buttons->left_slider = NULL;
      return;
    }
  else if (widget == buttons->right_slider)
    {
      thunar_location_buttons_remove_1 (container, widget);
      buttons->right_slider = NULL;
      return;
    }

  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (widget == GTK_WIDGET (lp->data))
      {
        thunar_location_buttons_remove_1 (container, widget);
        buttons->list = g_list_remove_link (buttons->list, lp);
        g_list_free (lp);
        return;
      }
}



static void
thunar_location_buttons_forall (GtkContainer *container,
                                gboolean      include_internals,
                                GtkCallback   callback,
                                gpointer      callback_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (container);
  GtkWidget             *child;
  GList                 *lp;

  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
  g_return_if_fail (callback != NULL);

  for (lp = buttons->list; lp != NULL; )
    {
      child = GTK_WIDGET (lp->data);
      lp = lp->next;

      (*callback) (child, callback_data);
    }

  if (buttons->left_slider != NULL)
    (*callback) (buttons->left_slider, callback_data);

  if (buttons->right_slider != NULL)
    (*callback) (buttons->right_slider, callback_data);
}



static GtkWidget*
thunar_location_buttons_make_button (ThunarLocationButtons *buttons,
                                     ThunarFile            *file)
{
  ThunarIconFactory *icon_factory;
  GtkWidget         *button;
  GtkWidget         *image;
  GtkWidget         *label;
  GtkWidget         *hbox;
  GdkPixbuf         *icon;
  gint               size;

  gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &size, &size);

  /* allocate the button */
  button = gtk_toggle_button_new ();

  /* don't allow location buttons to grab the focus */
  GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);

  /* setup drag support for the button */
  gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK, drag_targets,
                       G_N_ELEMENTS (drag_targets), GDK_ACTION_LINK);
  gtk_drag_dest_set (GTK_WIDGET (button), GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_MOTION, drag_targets,
                     G_N_ELEMENTS (drag_targets), GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (button), "drag-data-get", G_CALLBACK (thunar_location_buttons_drag_data_get), buttons);
  g_signal_connect (G_OBJECT (button), "drag-leave", G_CALLBACK (thunar_location_buttons_drag_leave), buttons);
  g_signal_connect (G_OBJECT (button), "drag-motion", G_CALLBACK (thunar_location_buttons_drag_motion), buttons);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_widget_show (hbox);

  image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  icon_factory = thunar_icon_factory_get_default ();
  icon = thunar_icon_factory_load_file_icon (icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, size);
  gtk_drag_source_set_icon_pixbuf (button, icon);
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);
  g_object_unref (G_OBJECT (icon_factory));
  g_object_unref (G_OBJECT (icon));

  if (!thunar_file_is_root (file))
    {
      /* only non-root nodes have a label */
      label = g_object_new (GTK_TYPE_LABEL, NULL);
      exo_binding_new (G_OBJECT (file), "display-name", G_OBJECT (label), "label");
      g_object_set_qdata (G_OBJECT (button), gtk_label_quark, label);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      /* current directory gets a bold label */
      if (file == buttons->current_directory)
        gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
    }

  /* the current directory is toggled */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), (file == buttons->current_directory));

  /* take a reference on the ThunarFile for the button */
  g_object_set_qdata_full (G_OBJECT (button), thunar_file_quark, file, g_object_unref);
  g_object_ref (G_OBJECT (file));

  /* get notifications about user actions */
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (thunar_location_buttons_clicked), buttons);

  return button;
}



static void
thunar_location_buttons_remove_1 (GtkContainer *container,
                                  GtkWidget    *widget)
{
  gboolean need_resize = GTK_WIDGET_VISIBLE (widget);
  gtk_widget_unparent (widget);
  if (G_LIKELY (need_resize))
    gtk_widget_queue_resize (GTK_WIDGET (container));
}



static gboolean
thunar_location_buttons_scroll_timeout (gpointer user_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (user_data);

  GDK_THREADS_ENTER ();

  if (GTK_WIDGET_HAS_FOCUS (buttons->left_slider))
    thunar_location_buttons_scroll_left (buttons->left_slider, buttons);
  else if (GTK_WIDGET_HAS_FOCUS (buttons->right_slider))
    thunar_location_buttons_scroll_right (buttons->right_slider, buttons);

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_location_buttons_scroll_timeout_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_LOCATION_BUTTONS (user_data)->scroll_timeout_id = -1;
  GDK_THREADS_LEAVE ();
}



static void
thunar_location_buttons_stop_scrolling (ThunarLocationButtons *buttons)
{
  if (buttons->scroll_timeout_id >= 0)
    g_source_remove (buttons->scroll_timeout_id);
}



static void
thunar_location_buttons_update_sliders (ThunarLocationButtons *buttons)
{
  GtkWidget *button;

  if (G_LIKELY (buttons->list != NULL))
    {
      button = GTK_WIDGET (buttons->list->data);
      if (gtk_widget_get_child_visible (button))
        gtk_widget_set_sensitive (buttons->right_slider, FALSE);
      else
        gtk_widget_set_sensitive (buttons->right_slider, TRUE);

      button = GTK_WIDGET (g_list_last (buttons->list)->data);
      if (gtk_widget_get_child_visible (button))
        gtk_widget_set_sensitive (buttons->left_slider, FALSE);
      else
        gtk_widget_set_sensitive (buttons->left_slider, TRUE);
    }
}



static gboolean
thunar_location_buttons_slider_button_press (GtkWidget             *button,
                                             GdkEventButton        *event,
                                             ThunarLocationButtons *buttons)
{
  if (!GTK_WIDGET_HAS_FOCUS (button))
    gtk_widget_grab_focus (button);

  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  buttons->ignore_click = FALSE;

  if (button == buttons->left_slider)
    thunar_location_buttons_scroll_left (button, buttons);
  else if (button == buttons->right_slider)
    thunar_location_buttons_scroll_right (button, buttons);

  if (G_LIKELY (buttons->scroll_timeout_id < 0))
    {
      buttons->scroll_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_LOCATION_BUTTONS_SCROLL_TIMEOUT,
                                                       thunar_location_buttons_scroll_timeout, buttons,
                                                       thunar_location_buttons_scroll_timeout_destroy);
    }

  return FALSE;
}



static gboolean
thunar_location_buttons_slider_button_release (GtkWidget             *button,
                                               GdkEventButton        *event,
                                               ThunarLocationButtons *buttons)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      thunar_location_buttons_stop_scrolling (buttons);
      buttons->ignore_click = TRUE;
    }

  return FALSE;
}



static void
thunar_location_buttons_scroll_left (GtkWidget             *button,
                                     ThunarLocationButtons *buttons)
{
  GList *lp;

  if (G_UNLIKELY (buttons->ignore_click))
    {
      buttons->ignore_click = FALSE;
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (buttons));

  for (lp = g_list_last (buttons->list); lp != NULL; lp = lp->prev)
    if (lp->prev != NULL && gtk_widget_get_child_visible (GTK_WIDGET (lp->prev->data)))
      {
        buttons->first_scrolled_button = lp;
        break;
      }
}



static void
thunar_location_buttons_scroll_right (GtkWidget             *button,
                                      ThunarLocationButtons *buttons)
{
  GtkTextDirection direction;
  GList           *right_button = NULL;
  GList           *left_button = NULL;
  GList           *lp;
  gint             space_available;
  gint             space_needed;
  gint             border_width;
  gint             spacing;

  if (G_UNLIKELY (buttons->ignore_click))
    {
      buttons->ignore_click = FALSE;
      return;
    }

  gtk_widget_style_get (GTK_WIDGET (buttons),
                        "spacing", &spacing,
                        NULL);

  gtk_widget_queue_resize (GTK_WIDGET (buttons));

  border_width = GTK_CONTAINER (buttons)->border_width;
  direction = gtk_widget_get_direction (GTK_WIDGET (buttons));

  /* find the button at the 'left' end that we have to make visible */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (lp->next != NULL && gtk_widget_get_child_visible (GTK_WIDGET (lp->next->data)))
      {
        right_button = lp;
        break;
      }

  /* find the last visible button on the 'right' end */
  for (lp = g_list_last (buttons->list); lp != NULL; lp = lp->prev)
    if (gtk_widget_get_child_visible (GTK_WIDGET (lp->data)))
      {
        left_button = lp;
        break;
      }

  space_needed = GTK_WIDGET (right_button->data)->allocation.width + spacing;
  if (direction == GTK_TEXT_DIR_RTL)
    {
      space_available = buttons->right_slider->allocation.x - GTK_WIDGET (buttons)->allocation.x;
    }
  else
    {
      space_available = (GTK_WIDGET (buttons)->allocation.x + GTK_WIDGET (buttons)->allocation.width - border_width)
                      - (buttons->right_slider->allocation.x + buttons->right_slider->allocation.width);
    }

  /* We have space_available extra space that's not being used.  We
   * need space_needed space to make the button fit.  So we walk down
   * from the end, removing buttons until we get all the space we
   * need.
   */
  while (space_available < space_needed)
    {
      space_available += GTK_WIDGET (left_button->data)->allocation.width + spacing;
      left_button = left_button->prev;
      buttons->first_scrolled_button = left_button;
    }
}



static void
thunar_location_buttons_clicked (GtkWidget             *button,
                                 ThunarLocationButtons *buttons)
{
  ThunarFile *directory;
  GtkWidget  *label;
  GList      *lp;

  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory associated with the clicked button */
  directory = g_object_get_qdata (G_OBJECT (button), thunar_file_quark);

  /* disconnect from previous current directory (if any) */
  if (G_LIKELY (buttons->current_directory != NULL))
    g_object_unref (G_OBJECT (buttons->current_directory));

  /* setup the new current directory */
  buttons->current_directory = directory;
  g_object_ref (G_OBJECT (directory));

  /* update all buttons */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      /* query button data */
      button = GTK_WIDGET (lp->data);
      label = g_object_get_qdata (G_OBJECT (button), gtk_label_quark);
      directory = g_object_get_qdata (G_OBJECT (button), thunar_file_quark);

      /* update the label (if any) */
      if (G_LIKELY (label != NULL))
        {
          /* current directory gets a bold label */
          if (directory == buttons->current_directory)
            gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_bold ());
          else
            gtk_label_set_attributes (GTK_LABEL (label), NULL);
        }

      /* update the toggle button state (making sure to not recurse with the "clicked" handler) */
      g_signal_handlers_block_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), directory == buttons->current_directory);
      g_signal_handlers_unblock_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
    }

  /* notify the surrounding module that we want to change
   * to a different directory.
   */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (buttons), buttons->current_directory);
}



static void
thunar_location_buttons_drag_data_get (GtkWidget             *button,
                                       GdkDragContext        *context,
                                       GtkSelectionData      *selection_data,
                                       guint                  info,
                                       guint                  time,
                                       ThunarLocationButtons *buttons)
{
  ThunarFile *file;
  GList       path_list;
  gchar      *uri_string;

  g_return_if_fail (GTK_IS_WIDGET (button));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the uri of the file in question */
  file = g_object_get_qdata (G_OBJECT (button), thunar_file_quark);

  /* transform the path into an uri list string */
  path_list.data = thunar_file_get_path (file); path_list.next = path_list.prev = NULL;
  uri_string = thunar_vfs_path_list_to_string (&path_list);

  /* set the uri list for the drag selection */
  gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) uri_string, strlen (uri_string));

  /* cleanup */
  g_free (uri_string);
}



static void
thunar_location_buttons_drag_leave (GtkWidget             *button,
                                    GdkDragContext        *context,
                                    guint                  time,
                                    ThunarLocationButtons *buttons)
{
  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* be sure to cancel any running enter timeout */
  if (G_LIKELY (buttons->enter_timeout_id >= 0))
    g_source_remove (buttons->enter_timeout_id);
}



static gboolean
thunar_location_buttons_drag_motion (GtkWidget             *button,
                                     GdkDragContext        *context,
                                     gint                   x,
                                     gint                   y,
                                     guint                  time,
                                     ThunarLocationButtons *buttons)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  g_return_val_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons), FALSE);

  /* schedule the enter timeout if not already done */
  if (G_UNLIKELY (buttons->enter_timeout_id < 0))
    {
      /* remember the new button */
      buttons->enter_button = button;
      g_object_add_weak_pointer (G_OBJECT (button), (gpointer) &buttons->enter_button);

      /* schedule the timeout */
      buttons->enter_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 500, thunar_location_buttons_enter_timeout,
                                                      buttons, thunar_location_buttons_enter_timeout_destroy);
    }

  /* tell GDK that it cannot drop here */
  gdk_drag_status (context, 0, time);

  return FALSE;
}



static gboolean
thunar_location_buttons_enter_timeout (gpointer user_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (user_data);

  GDK_THREADS_ENTER ();

  /* We emulate a "clicked" event here, because else the buttons
   * would be destroyed and replaced by new buttons, which causes
   * the Gtk DND code to dump core once the mouse leaves the area
   * of the new button.
   *
   * Besides that, handling this as "clicked" event allows the user
   * to go back to the initial directory.
   */
  if (G_LIKELY (buttons->enter_button != NULL))
    thunar_location_buttons_clicked (buttons->enter_button, buttons);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_location_buttons_enter_timeout_destroy (gpointer user_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (user_data);

  /* disconnect from the remembered button */
  if (G_LIKELY (buttons->enter_button != NULL))
    {
      g_object_remove_weak_pointer (G_OBJECT (buttons->enter_button), (gpointer) &buttons->enter_button);
      buttons->enter_button = NULL;
    }

  /* reset the timeout id */
  buttons->enter_timeout_id = -1;
}



/**
 * thunar_location_buttons_new:
 * 
 * Creates a new #ThunarLocationButtons object, that does
 * not display any path by default.
 *
 * Return value: the newly allocated #ThunarLocationButtons object.
 **/
GtkWidget*
thunar_location_buttons_new (void)
{
  return g_object_new (THUNAR_TYPE_LOCATION_BUTTONS, NULL);
}



