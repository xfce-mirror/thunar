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
 *
 * Based on code written by Jonathan Blandford <jrb@gnome.org> for
 * Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-location-button.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-location-buttons-ui.h>
#include <thunar/thunar-properties-dialog.h>



#define THUNAR_LOCATION_BUTTONS_SCROLL_TIMEOUT  (200)



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_UI_MANAGER,
};



static void           thunar_location_buttons_class_init                (ThunarLocationButtonsClass *klass);
static void           thunar_location_buttons_component_init            (ThunarComponentIface       *iface);
static void           thunar_location_buttons_navigator_init            (ThunarNavigatorIface       *iface);
static void           thunar_location_buttons_location_bar_init         (ThunarLocationBarIface     *iface);
static void           thunar_location_buttons_init                      (ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_finalize                  (GObject                    *object);
static void           thunar_location_buttons_get_property              (GObject                    *object,
                                                                         guint                       prop_id,
                                                                         GValue                     *value,
                                                                         GParamSpec                 *pspec);
static void           thunar_location_buttons_set_property              (GObject                    *object,
                                                                         guint                       prop_id,
                                                                         const GValue               *value,
                                                                         GParamSpec                 *pspec);
static GtkUIManager  *thunar_location_buttons_get_ui_manager            (ThunarComponent            *component);
static void           thunar_location_buttons_set_ui_manager            (ThunarComponent            *component,
                                                                         GtkUIManager               *ui_manager);
static ThunarFile    *thunar_location_buttons_get_current_directory     (ThunarNavigator            *navigator);
static void           thunar_location_buttons_set_current_directory     (ThunarNavigator            *navigator,
                                                                         ThunarFile                 *current_directory);
static void           thunar_location_buttons_unmap                     (GtkWidget                  *widget);
static void           thunar_location_buttons_size_request              (GtkWidget                  *widget,
                                                                         GtkRequisition             *requisition);
static void           thunar_location_buttons_size_allocate             (GtkWidget                  *widget,
                                                                         GtkAllocation              *allocation);
static void           thunar_location_buttons_state_changed             (GtkWidget                  *widget,
                                                                         GtkStateType                previous_state);
static void           thunar_location_buttons_grab_notify               (GtkWidget                  *widget,
                                                                         gboolean                    was_grabbed);
static void           thunar_location_buttons_add                       (GtkContainer               *container,
                                                                         GtkWidget                  *widget);
static void           thunar_location_buttons_remove                    (GtkContainer               *container,
                                                                         GtkWidget                  *widget);
static void           thunar_location_buttons_forall                    (GtkContainer               *container,
                                                                         gboolean                    include_internals,
                                                                         GtkCallback                 callback,
                                                                         gpointer                    callback_data);
static GtkWidget     *thunar_location_buttons_make_button               (ThunarLocationButtons      *buttons,
                                                                         ThunarFile                 *file);
static void           thunar_location_buttons_remove_1                  (GtkContainer               *container,
                                                                         GtkWidget                  *widget);
static gboolean       thunar_location_buttons_scroll_timeout            (gpointer                    user_data);
static void           thunar_location_buttons_scroll_timeout_destroy    (gpointer                    user_data);
static void           thunar_location_buttons_stop_scrolling            (ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_update_sliders            (ThunarLocationButtons      *buttons);
static gboolean       thunar_location_buttons_slider_button_press       (GtkWidget                  *button,
                                                                         GdkEventButton             *event,
                                                                         ThunarLocationButtons      *buttons);
static gboolean       thunar_location_buttons_slider_button_release     (GtkWidget                  *button,
                                                                         GdkEventButton             *event,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_scroll_left               (GtkWidget                  *button,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_scroll_right              (GtkWidget                  *button,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_clicked                   (ThunarLocationButton       *button,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_context_menu              (ThunarLocationButton       *button,
                                                                         GdkEventButton             *event,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_create_folder      (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_down_folder        (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_open               (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_open_in_new_window (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_paste_files_here   (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_properties         (GtkAction                  *action,
                                                                         ThunarLocationButtons      *buttons);



struct _ThunarLocationButtonsClass
{
  GtkContainerClass __parent__;
};

struct _ThunarLocationButtons
{
  GtkContainer __parent__;

  GtkActionGroup    *action_group;
  GtkUIManager      *ui_manager;
  guint              ui_merge_id;

  GtkWidget         *left_slider;
  GtkWidget         *right_slider;

  ThunarFile        *current_directory;

  gint               slider_width;
  gboolean           ignore_click : 1;

  GList             *list;
  GList             *first_scrolled_button;

  gint               scroll_timeout_id;
};



static const GtkActionEntry action_entries[] =
{
  { "location-buttons-down-folder", NULL, "Down Folder", "<alt>Down", NULL, G_CALLBACK (thunar_location_buttons_action_down_folder), },
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

      static const GInterfaceInfo component_info =
      {
        (GInterfaceInitFunc) thunar_location_buttons_component_init,
        NULL,
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
      g_type_add_interface_static (type, THUNAR_TYPE_COMPONENT, &component_info);
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

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_buttons_finalize;
  gobject_class->get_property = thunar_location_buttons_get_property;
  gobject_class->set_property = thunar_location_buttons_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unmap = thunar_location_buttons_unmap;
  gtkwidget_class->size_request = thunar_location_buttons_size_request;
  gtkwidget_class->size_allocate = thunar_location_buttons_size_allocate;
  gtkwidget_class->state_changed = thunar_location_buttons_state_changed;
  gtkwidget_class->grab_notify = thunar_location_buttons_grab_notify;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = thunar_location_buttons_add;
  gtkcontainer_class->remove = thunar_location_buttons_remove;
  gtkcontainer_class->forall = thunar_location_buttons_forall;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* Override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_UI_MANAGER, "ui-manager");

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
thunar_location_buttons_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = (gpointer) exo_noop_null;
  iface->set_selected_files = (gpointer) exo_noop;
  iface->get_ui_manager = thunar_location_buttons_get_ui_manager;
  iface->set_ui_manager = thunar_location_buttons_set_ui_manager;
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
  iface->accept_focus = (gpointer) exo_noop_false;
  iface->is_standalone = (gpointer) exo_noop_true;
}



static void
thunar_location_buttons_init (ThunarLocationButtons *buttons)
{
  GtkWidget *arrow;

  /* setup the action group for the location buttons */
  buttons->action_group = gtk_action_group_new ("ThunarLocationButtons");
  gtk_action_group_add_actions (buttons->action_group, action_entries, G_N_ELEMENTS (action_entries), buttons);

  GTK_WIDGET_SET_FLAGS (buttons, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (buttons), FALSE);

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

  /* disconnect from the selected files and the UI manager */
  thunar_component_set_selected_files (THUNAR_COMPONENT (buttons), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (buttons), NULL);

  /* be sure to cancel the scrolling */
  thunar_location_buttons_stop_scrolling (buttons);

  /* release from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (buttons), NULL);

  /* release our action group */
  g_object_unref (G_OBJECT (buttons->action_group));

  (*G_OBJECT_CLASS (thunar_location_buttons_parent_class)->finalize) (object);
}



static void
thunar_location_buttons_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_component_get_ui_manager (THUNAR_COMPONENT (object)));
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
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkUIManager*
thunar_location_buttons_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_LOCATION_BUTTONS (component)->ui_manager;
}



static void
thunar_location_buttons_set_ui_manager (ThunarComponent *component,
                                        GtkUIManager    *ui_manager)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (component);
  GError                *error = NULL;

  /* disconnect from the previous ui manager */
  if (G_UNLIKELY (buttons->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (buttons->ui_manager, buttons->action_group);

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (buttons->ui_manager, buttons->ui_merge_id);

      /* drop our reference on the previous UI manager */
      g_object_unref (G_OBJECT (buttons->ui_manager));
    }

  /* activate the new UI manager */
  buttons->ui_manager = ui_manager;

  /* connect to the new UI manager */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, buttons->action_group, -1);

      /* merge our UI control items with the new manager */
      buttons->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_location_buttons_ui, thunar_location_buttons_ui_length, &error);
      if (G_UNLIKELY (buttons->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarLocationButtons menus: %s", error->message);
          g_error_free (error);
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (buttons), "ui-manager");
}



static ThunarFile*
thunar_location_buttons_get_current_directory (ThunarNavigator *navigator)
{
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

  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* verify that we're not already using the new directory */
  if (G_UNLIKELY (buttons->current_directory == current_directory))
    return;

  /* check if we already have a button for that directory */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (thunar_location_button_get_file (lp->data) == current_directory)
      break;

  /* if we already have a button for that directory, just activate it */
  if (G_UNLIKELY (lp != NULL))
    {
      /* fake a "clicked" event for that button */
      thunar_location_button_clicked (THUNAR_LOCATION_BUTTON (lp->data));
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

  if (buttons->left_slider != NULL && include_internals)
    (*callback) (buttons->left_slider, callback_data);

  if (buttons->right_slider != NULL && include_internals)
    (*callback) (buttons->right_slider, callback_data);
}



static GtkWidget*
thunar_location_buttons_make_button (ThunarLocationButtons *buttons,
                                     ThunarFile            *file)
{
  GtkWidget *button;

  /* allocate the button */
  button = thunar_location_button_new ();

  /* connect to the file */
  thunar_location_button_set_file (THUNAR_LOCATION_BUTTON (button), file);

  /* the current directory is active */
  thunar_location_button_set_active (THUNAR_LOCATION_BUTTON (button), (file == buttons->current_directory));

  /* connect signal handlers */
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_location_buttons_clicked), buttons);
  g_signal_connect (G_OBJECT (button), "context-menu", G_CALLBACK (thunar_location_buttons_context_menu), buttons);

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

  /* find the button at the 'right' end that we have to make visible */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (lp->next != NULL && gtk_widget_get_child_visible (GTK_WIDGET (lp->next->data)))
      {
        right_button = lp;
        break;
      }

  /* find the last visible button on the 'left' end */
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
thunar_location_buttons_clicked (ThunarLocationButton  *button,
                                 ThunarLocationButtons *buttons)
{
  ThunarFile *directory;
  GList      *lp;

  g_return_if_fail (THUNAR_IS_LOCATION_BUTTON (button));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory associated with the clicked button */
  directory = thunar_location_button_get_file (button);

  /* disconnect from previous current directory (if any) */
  if (G_LIKELY (buttons->current_directory != NULL))
    g_object_unref (G_OBJECT (buttons->current_directory));

  /* setup the new current directory */
  buttons->current_directory = directory;

  /* take a reference on the new directory (if any) */
  if (G_LIKELY (directory != NULL))
    g_object_ref (G_OBJECT (directory));

  /* check if the button is visible on the button bar */
  if (!gtk_widget_get_child_visible (GTK_WIDGET (button)))
    {
      /* scroll to the button */
      buttons->first_scrolled_button = g_list_find (buttons->list, button);
      gtk_widget_queue_resize (GTK_WIDGET (buttons));
    }

  /* update all buttons */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      /* determine the directory for this button */
      button = THUNAR_LOCATION_BUTTON (lp->data);
      directory = thunar_location_button_get_file (button);

      /* update the location button state (making sure to not recurse with the "clicked" handler) */
      g_signal_handlers_block_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
      thunar_location_button_set_active (button, directory == buttons->current_directory);
      g_signal_handlers_unblock_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
    }

  /* notify the surrounding module that we want to change
   * to a different directory.
   */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (buttons), buttons->current_directory);
}



static void
thunar_location_buttons_context_menu (ThunarLocationButton  *button,
                                      GdkEventButton        *event,
                                      ThunarLocationButtons *buttons)
{
  ThunarClipboardManager *clipboard;
  GtkActionGroup         *action_group;
  GtkUIManager           *ui_manager;
  ThunarFile             *file;
  GtkAction              *action;
  GMainLoop              *loop;
  GtkWidget              *menu;
  gchar                  *tooltip;
  guint                   signal_id;
  guint                   merge_id;

  g_return_if_fail (THUNAR_IS_LOCATION_BUTTON (button));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the file for the button */
  file = thunar_location_button_get_file (button);
  if (G_UNLIKELY (file == NULL))
    return;

  /* we cannot popup the context menu without an ui manager */
  if (G_UNLIKELY (buttons->ui_manager == NULL))
    return;

  /* keep a reference on the ui manager */
  ui_manager = g_object_ref (G_OBJECT (buttons->ui_manager));

  /* be sure to keep a reference to the button and the path bar */
  g_object_ref (G_OBJECT (button));
  g_object_ref (G_OBJECT (buttons));

  /* grab a reference on the clipboard manager for this display */
  clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (button)));

  /* add temporary UI (popup menu) */
  merge_id = gtk_ui_manager_new_merge_id (ui_manager);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/", "ThunarLocationButtons::context-menu",
                         "ThunarLocationButtons::context-menu", GTK_UI_MANAGER_POPUP, FALSE);

  /* allocate a temporary action group */
  action_group = gtk_action_group_new ("ThunarLocationButtons::ContextMenu");
  gtk_ui_manager_insert_action_group (ui_manager, action_group, -1);

  /* add the "Open" action */
  tooltip = g_strdup_printf (_("Open \"%s\" in this window"), thunar_file_get_display_name (file));
  action = gtk_action_new ("ThunarLocationButtons::open", _("_Open"), tooltip, GTK_STOCK_OPEN);
  exo_binding_new_with_negation (G_OBJECT (button), "active", G_OBJECT (action), "sensitive");
  g_object_set_data_full (G_OBJECT (action), I_("thunar-location-button"), g_object_ref (G_OBJECT (button)), (GDestroyNotify) g_object_unref);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_location_buttons_action_open), buttons);
  gtk_action_group_add_action (action_group, action);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu",
                         gtk_action_get_name (action), gtk_action_get_name (action),
                         GTK_UI_MANAGER_MENUITEM, FALSE);
  g_object_unref (G_OBJECT (action));
  g_free (tooltip);

  /* add the "Open in New Window" action */
  tooltip = g_strdup_printf (_("Open \"%s\" in a new window"), thunar_file_get_display_name (file));
  action = gtk_action_new ("ThunarLocationButtons::open-in-new-window", _("Open in New Window"), tooltip, NULL);
  g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_location_buttons_action_open_in_new_window), buttons);
  gtk_action_group_add_action (action_group, action);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu",
                         gtk_action_get_name (action), gtk_action_get_name (action),
                         GTK_UI_MANAGER_MENUITEM, FALSE);
  g_object_unref (G_OBJECT (action));
  g_free (tooltip);

  /* add a separator */
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu", "separator1", NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);

  /* add the "Create Folder" action */
  tooltip = g_strdup_printf (_("Create a new folder in \"%s\""), thunar_file_get_display_name (file));
  action = gtk_action_new ("ThunarLocationButtons::create-folder", _("Create _Folder..."), tooltip, NULL);
  g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_location_buttons_action_create_folder), buttons);
  gtk_action_set_sensitive (action, thunar_file_is_writable (file));
  gtk_action_group_add_action (action_group, action);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu",
                         gtk_action_get_name (action), gtk_action_get_name (action),
                         GTK_UI_MANAGER_MENUITEM, FALSE);
  g_object_unref (G_OBJECT (action));
  g_free (tooltip);

  /* add a separator */
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu", "separator2", NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);

  /* add the "Paste Into Folder" action */
  tooltip = g_strdup_printf (_("Move or copy files previously selected by a Cut or Copy command into \"%s\""), thunar_file_get_display_name (file));
  action = gtk_action_new ("ThunarLocationButtons::paste-into-folder", _("Paste Into Folder"), tooltip, GTK_STOCK_PASTE);
  g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_location_buttons_action_paste_files_here), buttons);
  exo_binding_new (G_OBJECT (clipboard), "can-paste", G_OBJECT (action), "sensitive");
  gtk_action_group_add_action (action_group, action);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu",
                         gtk_action_get_name (action), gtk_action_get_name (action),
                         GTK_UI_MANAGER_MENUITEM, FALSE);
  g_object_unref (G_OBJECT (action));
  g_free (tooltip);

  /* add a separator */
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu", "separator3", NULL, GTK_UI_MANAGER_SEPARATOR, FALSE);

  /* add the "Properties" action */
  tooltip = g_strdup_printf (_("View the properties of the folder \"%s\""), thunar_file_get_display_name (file));
  action = gtk_action_new ("ThunarLocationButtons::properties", _("_Properties"), tooltip, GTK_STOCK_PROPERTIES);
  g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_location_buttons_action_properties), buttons);
  gtk_action_group_add_action (action_group, action);
  gtk_ui_manager_add_ui (ui_manager, merge_id, "/ThunarLocationButtons::context-menu",
                         gtk_action_get_name (action), gtk_action_get_name (action),
                         GTK_UI_MANAGER_MENUITEM, FALSE);
  g_object_unref (G_OBJECT (action));
  g_free (tooltip);

  /* be sure to update the UI manager first */
  gtk_ui_manager_ensure_update (ui_manager);

  /* determine the menu widget */
  menu = gtk_ui_manager_get_widget (ui_manager, "/ThunarLocationButtons::context-menu");
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (button)));
  exo_gtk_object_ref_sink (GTK_OBJECT (menu));

  /* run an internal main loop */
  gtk_grab_add (menu);
  loop = g_main_loop_new (NULL, FALSE);
  signal_id = g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
  g_main_loop_run (loop);
  g_signal_handler_disconnect (G_OBJECT (menu), signal_id);
  g_main_loop_unref (loop);
  gtk_grab_remove (menu);

  /* drop the temporary UI */
  gtk_ui_manager_remove_ui (ui_manager, merge_id);

  /* drop the temporary action group */
  gtk_ui_manager_remove_action_group (ui_manager, action_group);
  g_object_unref (G_OBJECT (action_group));

  /* cleanup */
  g_object_unref (G_OBJECT (ui_manager));
  g_object_unref (G_OBJECT (clipboard));
  g_object_unref (G_OBJECT (buttons));
  g_object_unref (G_OBJECT (button));
  g_object_unref (G_OBJECT (menu));
}



static void
thunar_location_buttons_action_create_folder (GtkAction             *action,
                                              ThunarLocationButtons *buttons)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  ThunarApplication     *application;
  ThunarFile            *directory;
  GList                  path_list;
  gchar                 *name;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_UNLIKELY (directory == NULL))
    return;

  /* lookup "inode/directory" mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (GTK_WIDGET (buttons), mime_info, _("New Folder"), _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* fake the path list */
      path_list.data = thunar_vfs_path_relative (thunar_file_get_path (directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      application = thunar_application_get ();
      thunar_application_mkdir (application, GTK_WIDGET (buttons), &path_list, NULL);
      g_object_unref (G_OBJECT (application));

      /* release the path */
      thunar_vfs_path_unref (path_list.data);

      /* release the file name */
      g_free (name);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_unref (mime_info);
}



static void
thunar_location_buttons_action_down_folder (GtkAction             *action,
                                            ThunarLocationButtons *buttons)
{
  GList *lp;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* lookup the active button */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (thunar_location_button_get_active (lp->data))
      {
        /* check if we have a folder below that one */
        if (G_LIKELY (lp->prev != NULL))
          {
            /* enter that folder */
            thunar_location_button_clicked (lp->prev->data);
          }

        break;
      }
}



static void
thunar_location_buttons_action_open (GtkAction             *action,
                                     ThunarLocationButtons *buttons)
{
  ThunarLocationButton *button;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* just emit "clicked" for the button associated with the action */
  button = g_object_get_data (G_OBJECT (action), "thunar-location-button");
  if (G_LIKELY (button != NULL))
    thunar_location_button_clicked (button);
}



static void
thunar_location_buttons_action_open_in_new_window (GtkAction             *action,
                                                   ThunarLocationButtons *buttons)
{
  ThunarApplication *application;
  ThunarFile        *directory;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL))
    {
      /* open a new window for the directory */
      application = thunar_application_get ();
      thunar_application_open_window (application, directory, gtk_widget_get_screen (GTK_WIDGET (buttons)));
      g_object_unref (G_OBJECT (application));
    }
}



static void
thunar_location_buttons_action_paste_files_here (GtkAction             *action,
                                                 ThunarLocationButtons *buttons)
{
  ThunarClipboardManager *clipboard;
  ThunarFile             *directory;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL))
    {
      /* paste files from the clipboard to the folder represented by this button */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (buttons)));
      thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_path (directory), GTK_WIDGET (buttons), NULL);
      g_object_unref (G_OBJECT (clipboard));
    }
}



static void
thunar_location_buttons_action_properties (GtkAction             *action,
                                           ThunarLocationButtons *buttons)
{
  ThunarFile *directory;
  GtkWidget  *toplevel;
  GtkWidget  *dialog;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL))
    {
      /* determine the toplevel window */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (buttons));
      if (G_LIKELY (toplevel != NULL && GTK_WIDGET_TOPLEVEL (toplevel)))
        {
          /* popup the properties dialog */
          dialog = thunar_properties_dialog_new ();
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), directory);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
          gtk_widget_show (dialog);
        }
    }
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



