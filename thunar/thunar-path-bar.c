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
 * Based on code written by Jonathan Blandford <jrb@gnome.org> for Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-path-bar.h>
#include <thunar/thunar-path-button.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>



#define THUNAR_PATH_BAR_SCROLL_TIMEOUT (200)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void        thunar_path_bar_class_init                (ThunarPathBarClass   *klass);
static void        thunar_path_bar_navigator_init            (ThunarNavigatorIface *iface);
static void        thunar_path_bar_init                      (ThunarPathBar        *path_bar);
static void        thunar_path_bar_finalize                  (GObject              *object);
static void        thunar_path_bar_get_property              (GObject              *object,
                                                              guint                 prop_id,
                                                              GValue               *value,
                                                              GParamSpec           *pspec);
static void        thunar_path_bar_set_property              (GObject              *object,
                                                              guint                 prop_id,
                                                              const GValue         *value,
                                                              GParamSpec           *pspec);
static ThunarFile *thunar_path_bar_get_current_directory     (ThunarNavigator      *navigator);
static void        thunar_path_bar_set_current_directory     (ThunarNavigator      *navigator,
                                                              ThunarFile           *current_directory);
static void        thunar_path_bar_unmap                     (GtkWidget            *widget);
static void        thunar_path_bar_size_request              (GtkWidget            *widget,
                                                              GtkRequisition       *requisition);
static void        thunar_path_bar_size_allocate             (GtkWidget            *widget,
                                                              GtkAllocation        *allocation);
static void        thunar_path_bar_state_changed             (GtkWidget            *widget,
                                                              GtkStateType          previous_state);
static void        thunar_path_bar_grab_notify               (GtkWidget            *widget,
                                                              gboolean              was_grabbed);
static void        thunar_path_bar_add                       (GtkContainer         *container,
                                                              GtkWidget            *widget);
static void        thunar_path_bar_remove                    (GtkContainer         *container,
                                                              GtkWidget            *widget);
static void        thunar_path_bar_forall                    (GtkContainer         *container,
                                                              gboolean              include_internals,
                                                              GtkCallback           callback,
                                                              gpointer              callback_data);
static GtkWidget  *thunar_path_bar_make_button               (ThunarPathBar        *path_bar,
                                                              ThunarFile           *file);
static void        thunar_path_bar_remove_1                  (GtkContainer         *container,
                                                              GtkWidget            *widget);
static gboolean    thunar_path_bar_scroll_timeout            (gpointer              user_data);
static void        thunar_path_bar_scroll_timeout_destroy    (gpointer              user_data);
static void        thunar_path_bar_stop_scrolling            (ThunarPathBar        *path_bar);
static void        thunar_path_bar_update_sliders            (ThunarPathBar        *path_bar);
static gboolean    thunar_path_bar_slider_button_press       (GtkWidget            *button,
                                                              GdkEventButton       *event,
                                                              ThunarPathBar        *path_bar);
static gboolean    thunar_path_bar_slider_button_release     (GtkWidget            *button,
                                                              GdkEventButton       *event,
                                                              ThunarPathBar        *path_bar);
static void        thunar_path_bar_scroll_left               (GtkWidget            *button,
                                                              ThunarPathBar        *path_bar);
static void        thunar_path_bar_scroll_right              (GtkWidget            *button,
                                                              ThunarPathBar        *path_bar);
static void        thunar_path_bar_clicked                   (ThunarPathButton     *button,
                                                              ThunarPathBar        *path_bar);
static void        thunar_path_bar_context_menu              (ThunarPathButton     *button,
                                                              GdkEventButton       *event,
                                                              ThunarPathBar        *path_bar);



struct _ThunarPathBarClass
{
  GtkContainerClass __parent__;
};

struct _ThunarPathBar
{
  GtkContainer __parent__;

  GtkWidget   *left_slider;
  GtkWidget   *right_slider;

  ThunarFile  *current_directory;

  gint         slider_width;
  gboolean     ignore_click : 1;

  GList       *list;
  GList       *fake_root_button;
  GList       *first_scrolled_button;

  guint        scroll_timeout_id;
};



static GObjectClass *thunar_path_bar_parent_class;



GType
thunar_path_bar_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPathBarClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_path_bar_class_init,
        NULL,
        NULL,
        sizeof (ThunarPathBar),
        0,
        (GInstanceInitFunc) thunar_path_bar_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_path_bar_navigator_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_CONTAINER, I_("ThunarPathBar"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
    }

  return type;
}



static void
thunar_path_bar_class_init (ThunarPathBarClass *klass)
{
  GtkContainerClass *gtkcontainer_class;
  GtkWidgetClass    *gtkwidget_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_path_bar_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_path_bar_finalize;
  gobject_class->get_property = thunar_path_bar_get_property;
  gobject_class->set_property = thunar_path_bar_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unmap = thunar_path_bar_unmap;
  gtkwidget_class->size_request = thunar_path_bar_size_request;
  gtkwidget_class->size_allocate = thunar_path_bar_size_allocate;
  gtkwidget_class->state_changed = thunar_path_bar_state_changed;
  gtkwidget_class->grab_notify = thunar_path_bar_grab_notify;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = thunar_path_bar_add;
  gtkcontainer_class->remove = thunar_path_bar_remove;
  gtkcontainer_class->forall = thunar_path_bar_forall;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /**
   * ThunarPathBar:spacing:
   *
   * The amount of space between the path path_bar.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("spacing",
                                                             _("Spacing"),
                                                             _("The amount of space between the path buttons"),
                                                             0, G_MAXINT, 3,
                                                             G_PARAM_READABLE));

  /**
   * ThunarPathBar::context-menu:
   * @path_bar : a #ThunarPathBar.
   * @event    : a #GdkEventButton.
   * @file     : a #ThunarFile.
   *
   * Emitted by @path_bar to inform the containing navigation bar that
   * a context menu for @file should be popped up, utilizing the @event.
   **/
  g_signal_new (I_("context-menu"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                _thunar_marshal_VOID__BOXED_OBJECT,
                G_TYPE_NONE, 2, GDK_TYPE_EVENT, THUNAR_TYPE_FILE);
}



static void
thunar_path_bar_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_path_bar_get_current_directory;
  iface->set_current_directory = thunar_path_bar_set_current_directory;
}



static void
thunar_path_bar_init (ThunarPathBar *path_bar)
{
  GtkWidget *arrow;

  GTK_WIDGET_SET_FLAGS (path_bar, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

  path_bar->left_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (path_bar->left_slider), "button-press-event", G_CALLBACK (thunar_path_bar_slider_button_press), path_bar);
  g_signal_connect (G_OBJECT (path_bar->left_slider), "button-release-event", G_CALLBACK (thunar_path_bar_slider_button_release), path_bar);
  g_signal_connect (G_OBJECT (path_bar->left_slider), "clicked", G_CALLBACK (thunar_path_bar_scroll_left), path_bar);
  gtk_container_add (GTK_CONTAINER (path_bar), path_bar->left_slider);
  gtk_widget_show (path_bar->left_slider);

  arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (path_bar->left_slider), arrow);
  gtk_widget_show (arrow);

  path_bar->right_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (path_bar->right_slider), "button-press-event", G_CALLBACK (thunar_path_bar_slider_button_press), path_bar);
  g_signal_connect (G_OBJECT (path_bar->right_slider), "button-release-event", G_CALLBACK (thunar_path_bar_slider_button_release), path_bar);
  g_signal_connect (G_OBJECT (path_bar->right_slider), "clicked", G_CALLBACK (thunar_path_bar_scroll_right), path_bar);
  gtk_container_add (GTK_CONTAINER (path_bar), path_bar->right_slider);
  gtk_widget_show (path_bar->right_slider);

  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (path_bar->right_slider), arrow);
  gtk_widget_show (arrow);
}



static void
thunar_path_bar_finalize (GObject *object)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (object);

  _thunar_return_if_fail (THUNAR_IS_PATH_BAR (path_bar));

  /* be sure to cancel the scrolling */
  thunar_path_bar_stop_scrolling (path_bar);

  /* disconnect from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (path_bar), NULL);

  (*G_OBJECT_CLASS (thunar_path_bar_parent_class)->finalize) (object);
}



static void
thunar_path_bar_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_bar_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_path_bar_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_PATH_BAR (navigator)->current_directory;
}



static void
thunar_path_bar_set_current_directory (ThunarNavigator *navigator,
                                       ThunarFile      *current_directory)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (navigator);
  ThunarFile    *file_parent;
  ThunarFile    *file;
  GtkWidget     *button;
  GList         *lp;

  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* verify that we're not already using the new directory */
  if (G_UNLIKELY (path_bar->current_directory == current_directory))
    return;

  /* check if we already have a button for that directory */
  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    if (thunar_path_button_get_file (lp->data) == current_directory)
      break;

  /* if we already have a button for that directory, just activate it */
  if (G_UNLIKELY (lp != NULL))
    {
      /* fake a "clicked" event for that button */
      thunar_path_button_clicked (THUNAR_PATH_BUTTON (lp->data));
    }
  else
    {
      /* regenerate the button list */
      if (G_LIKELY (path_bar->current_directory != NULL))
        {
          g_object_unref (G_OBJECT (path_bar->current_directory));

          /* remove all path_bar */
          while (path_bar->list != NULL)
            gtk_container_remove (GTK_CONTAINER (path_bar), path_bar->list->data);

          /* clear the first scrolled and fake root buttons */
          path_bar->first_scrolled_button = NULL;
          path_bar->fake_root_button = NULL;
        }

      path_bar->current_directory = current_directory;

      if (G_LIKELY (current_directory != NULL))
        {
          g_object_ref (G_OBJECT (current_directory));

          /* add the new path_bar */
          for (file = current_directory; file != NULL; file = file_parent)
            {
              button = thunar_path_bar_make_button (path_bar, file);
              path_bar->list = g_list_append (path_bar->list, button);
              gtk_container_add (GTK_CONTAINER (path_bar), button);
              gtk_widget_show (button);

              /* use 'Home' as fake root button */
              if (thunar_file_is_home (file))
                path_bar->fake_root_button = g_list_last (path_bar->list);

              /* continue with the parent (if any) */
              file_parent = thunar_file_get_parent (file, NULL);

              if (G_LIKELY (file != current_directory))
                g_object_unref (G_OBJECT (file));
            }
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (path_bar), "current-directory");
}



static void
thunar_path_bar_unmap (GtkWidget *widget)
{
  /* be sure to stop the scroll timer before the path_bar are unmapped */
  thunar_path_bar_stop_scrolling (THUNAR_PATH_BAR (widget));

  /* do the real unmap */
  (*GTK_WIDGET_CLASS (thunar_path_bar_parent_class)->unmap) (widget);
}



static void
thunar_path_bar_size_request (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  GtkRequisition child_requisition;
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (widget);
  GList         *lp;
  gint           spacing;

  gtk_widget_style_get (widget, "spacing", &spacing, NULL);

  requisition->width = 0;
  requisition->height = 0;

  /* calculate the size of the biggest button */
  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    {
      gtk_widget_size_request (GTK_WIDGET (lp->data), &child_requisition);
      requisition->width = MAX (child_requisition.width, requisition->width);
      requisition->height = MAX (child_requisition.height, requisition->height);
    }

  /* add space for the sliders if we have more than one path */
  path_bar->slider_width = MIN (requisition->height * 2 / 3 + 5, requisition->height);
  if (path_bar->list != NULL && path_bar->list->next != NULL)
    requisition->width += (spacing + path_bar->slider_width) * 2;

  gtk_widget_size_request (path_bar->left_slider, &child_requisition);
  gtk_widget_size_request (path_bar->right_slider, &child_requisition);

  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;

  widget->requisition = *requisition;
}



static void
thunar_path_bar_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkTextDirection direction;
  GtkAllocation    child_allocation;
  ThunarPathBar   *path_bar = THUNAR_PATH_BAR (widget);
  GtkWidget       *child;
  gboolean         need_sliders = FALSE;
  gboolean         reached_end;
  GList           *first_button;
  GList           *lp;
  gint             left_slider_offset = 0;
  gint             right_slider_offset = 0;
  gint             allocation_width;
  gint             border_width;
  gint             slider_space;
  gint             spacing;
  gint             width;

  widget->allocation = *allocation;

  /* if no path is set, we don't have to allocate anything */
  if (G_UNLIKELY (path_bar->list == NULL))
    return;

  gtk_widget_style_get (widget, "spacing", &spacing, NULL);

  direction = gtk_widget_get_direction (widget);
  border_width = GTK_CONTAINER (path_bar)->border_width;
  allocation_width = allocation->width - 2 * border_width;

  /* check if we need to display the sliders */
  if (G_LIKELY (path_bar->fake_root_button != NULL))
    width = path_bar->slider_width + spacing;
  else
    width = 0;

  for (lp = path_bar->list->next; lp != NULL; lp = lp->next)
    {
      width += GTK_WIDGET (lp->data)->requisition.width + spacing;
      if (lp == path_bar->fake_root_button)
        break;
    }

  if (width <= allocation_width)
    {
      if (G_LIKELY (path_bar->fake_root_button != NULL))
        first_button = path_bar->fake_root_button;
      else
        first_button = g_list_last (path_bar->list);
    }
  else
    {
      reached_end = FALSE;
      slider_space = 2 * (spacing + path_bar->slider_width);

      if (path_bar->first_scrolled_button != NULL)
        first_button = path_bar->first_scrolled_button;
      else
        first_button = path_bar->list;
      need_sliders = TRUE;

      /* To see how much space we have, and how many buttons we can display.
       * We start at the first button, count forward until hit the new
       * button, then count backwards.
       */
      width = GTK_WIDGET (first_button->data)->requisition.width;
      for (lp = first_button->prev; lp != NULL && !reached_end; lp = lp->prev)
        {
          child = lp->data;
          if (width + child->requisition.width + spacing + slider_space > allocation_width)
            reached_end = TRUE;
          else if (lp == path_bar->fake_root_button)
            break;
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
              if (first_button == path_bar->fake_root_button)
                break;
              first_button = first_button->next;
            }
        }
    }

  /* Now we allocate space to the path_bar */
  child_allocation.y = allocation->y + border_width;
  child_allocation.height = MAX (1, allocation->height - border_width * 2);

  if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
    {
      child_allocation.x = allocation->x + allocation->width - border_width;
      if (need_sliders || path_bar->fake_root_button != NULL)
        {
          child_allocation.x -= spacing + path_bar->slider_width;
          left_slider_offset = allocation->width - border_width - path_bar->slider_width;
        }
    }
  else
    {
      child_allocation.x = allocation->x + border_width;
      if (need_sliders || path_bar->fake_root_button != NULL)
        {
          left_slider_offset = border_width;
          child_allocation.x += spacing + path_bar->slider_width;
        }
    }

  for (lp = first_button; lp != NULL; lp = lp->prev)
    {
      child = lp->data;

      child_allocation.width = child->requisition.width;
      if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
        child_allocation.x -= child_allocation.width;

      /* check to see if we don't have any more space to allocate path_bar */
      if (need_sliders && direction == GTK_TEXT_DIR_RTL)
        {
          if (child_allocation.x - spacing - path_bar->slider_width < widget->allocation.x + border_width)
            break;
        }
      else if (need_sliders && direction == GTK_TEXT_DIR_LTR)
        {
          if (child_allocation.x + child_allocation.width + spacing + path_bar->slider_width > widget->allocation.x + border_width + allocation_width)
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
          right_slider_offset = allocation->width - border_width - path_bar->slider_width;
        }
    }

  /* now we go hide all the path_bar that don't fit */
  for (; lp != NULL; lp = lp->prev)
    gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);
  for (lp = first_button->next; lp != NULL; lp = lp->next)
    gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);

  if (need_sliders || path_bar->fake_root_button != NULL)
    {
      child_allocation.width = path_bar->slider_width;
      child_allocation.x = left_slider_offset + allocation->x;
      gtk_widget_size_allocate (path_bar->left_slider, &child_allocation);
      gtk_widget_set_child_visible (path_bar->left_slider, TRUE);
      gtk_widget_show_all (path_bar->left_slider);
    }
  else
    {
      gtk_widget_set_child_visible (path_bar->left_slider, FALSE);
    }

  if (need_sliders)
    {
      child_allocation.width = path_bar->slider_width;
      child_allocation.x = right_slider_offset + allocation->x;
      gtk_widget_size_allocate (path_bar->right_slider, &child_allocation);
      gtk_widget_set_child_visible (path_bar->right_slider, TRUE);
      gtk_widget_show_all (path_bar->right_slider);

      thunar_path_bar_update_sliders (path_bar);
    }
  else
    {
      gtk_widget_set_child_visible (path_bar->right_slider, FALSE);
    }
}



static void
thunar_path_bar_state_changed (GtkWidget   *widget,
                               GtkStateType previous_state)
{
  if (!GTK_WIDGET_IS_SENSITIVE (widget))
    thunar_path_bar_stop_scrolling (THUNAR_PATH_BAR (widget));
}



static void
thunar_path_bar_grab_notify (GtkWidget *widget,
                             gboolean   was_grabbed)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (widget);

  if (!was_grabbed)
    thunar_path_bar_stop_scrolling (path_bar);
}



static void
thunar_path_bar_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}



static void
thunar_path_bar_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (container);
  GList         *lp;

  if (widget == path_bar->left_slider)
    {
      thunar_path_bar_remove_1 (container, widget);
      path_bar->left_slider = NULL;
      return;
    }
  else if (widget == path_bar->right_slider)
    {
      thunar_path_bar_remove_1 (container, widget);
      path_bar->right_slider = NULL;
      return;
    }

  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    if (widget == GTK_WIDGET (lp->data))
      {
        thunar_path_bar_remove_1 (container, widget);
        path_bar->list = g_list_remove_link (path_bar->list, lp);
        g_list_free (lp);
        return;
      }
}



static void
thunar_path_bar_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (container);
  GtkWidget     *child;
  GList         *lp;

  _thunar_return_if_fail (THUNAR_IS_PATH_BAR (path_bar));
  _thunar_return_if_fail (callback != NULL);

  for (lp = path_bar->list; lp != NULL; )
    {
      child = GTK_WIDGET (lp->data);
      lp = lp->next;

      (*callback) (child, callback_data);
    }

  if (path_bar->left_slider != NULL && include_internals)
    (*callback) (path_bar->left_slider, callback_data);

  if (path_bar->right_slider != NULL && include_internals)
    (*callback) (path_bar->right_slider, callback_data);
}



static GtkWidget*
thunar_path_bar_make_button (ThunarPathBar *path_bar,
                             ThunarFile    *file)
{
  GtkWidget *button;

  /* allocate the button */
  button = thunar_path_button_new ();

  /* connect to the file */
  thunar_path_button_set_file (THUNAR_PATH_BUTTON (button), file);

  /* the current directory is active */
  thunar_path_button_set_active (THUNAR_PATH_BUTTON (button), (file == path_bar->current_directory));

  /* connect signal handlers */
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_path_bar_clicked), path_bar);
  g_signal_connect (G_OBJECT (button), "context-menu", G_CALLBACK (thunar_path_bar_context_menu), path_bar);

  return button;
}



static void
thunar_path_bar_remove_1 (GtkContainer *container,
                          GtkWidget    *widget)
{
  gboolean need_resize = GTK_WIDGET_VISIBLE (widget);
  gtk_widget_unparent (widget);
  if (G_LIKELY (need_resize))
    gtk_widget_queue_resize (GTK_WIDGET (container));
}



static gboolean
thunar_path_bar_scroll_timeout (gpointer user_data)
{
  ThunarPathBar *path_bar = THUNAR_PATH_BAR (user_data);

  GDK_THREADS_ENTER ();

  if (GTK_WIDGET_HAS_FOCUS (path_bar->left_slider))
    thunar_path_bar_scroll_left (path_bar->left_slider, path_bar);
  else if (GTK_WIDGET_HAS_FOCUS (path_bar->right_slider))
    thunar_path_bar_scroll_right (path_bar->right_slider, path_bar);

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_path_bar_scroll_timeout_destroy (gpointer user_data)
{
  THUNAR_PATH_BAR (user_data)->scroll_timeout_id = 0;
}



static void
thunar_path_bar_stop_scrolling (ThunarPathBar *path_bar)
{
  if (path_bar->scroll_timeout_id != 0)
    g_source_remove (path_bar->scroll_timeout_id);
}



static void
thunar_path_bar_update_sliders (ThunarPathBar *path_bar)
{
  GtkWidget *button;

  if (G_LIKELY (path_bar->list != NULL))
    {
      button = GTK_WIDGET (path_bar->list->data);
      if (gtk_widget_get_child_visible (button))
        gtk_widget_set_sensitive (path_bar->right_slider, FALSE);
      else
        gtk_widget_set_sensitive (path_bar->right_slider, TRUE);

      button = GTK_WIDGET (g_list_last (path_bar->list)->data);
      if (gtk_widget_get_child_visible (button))
        gtk_widget_set_sensitive (path_bar->left_slider, FALSE);
      else
        gtk_widget_set_sensitive (path_bar->left_slider, TRUE);
    }
}



static gboolean
thunar_path_bar_slider_button_press (GtkWidget      *button,
                                     GdkEventButton *event,
                                     ThunarPathBar  *path_bar)
{
  if (!GTK_WIDGET_HAS_FOCUS (button))
    gtk_widget_grab_focus (button);

  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  path_bar->ignore_click = FALSE;

  if (button == path_bar->left_slider)
    thunar_path_bar_scroll_left (button, path_bar);
  else if (button == path_bar->right_slider)
    thunar_path_bar_scroll_right (button, path_bar);

  if (G_LIKELY (path_bar->scroll_timeout_id == 0))
    {
      path_bar->scroll_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_PATH_BAR_SCROLL_TIMEOUT,
                                                       thunar_path_bar_scroll_timeout, path_bar,
                                                       thunar_path_bar_scroll_timeout_destroy);
    }

  return FALSE;
}



static gboolean
thunar_path_bar_slider_button_release (GtkWidget      *button,
                                       GdkEventButton *event,
                                       ThunarPathBar  *path_bar)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      thunar_path_bar_stop_scrolling (path_bar);
      path_bar->ignore_click = TRUE;
    }

  return FALSE;
}



static void
thunar_path_bar_scroll_left (GtkWidget     *button,
                             ThunarPathBar *path_bar)
{
  GList *lp;

  if (G_UNLIKELY (path_bar->ignore_click))
    {
      path_bar->ignore_click = FALSE;
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  for (lp = g_list_last (path_bar->list); lp != NULL; lp = lp->prev)
    if (lp->prev != NULL && gtk_widget_get_child_visible (GTK_WIDGET (lp->prev->data)))
      {
        if (lp->prev == path_bar->fake_root_button)
          path_bar->fake_root_button = NULL;
        path_bar->first_scrolled_button = lp;
        break;
      }
}



static void
thunar_path_bar_scroll_right (GtkWidget     *button,
                              ThunarPathBar *path_bar)
{
  GtkTextDirection direction;
  GList           *right_button = NULL;
  GList           *left_button = NULL;
  GList           *lp;
  gint             space_available;
  gint             space_needed;
  gint             border_width;
  gint             spacing;

  if (G_UNLIKELY (path_bar->ignore_click))
    {
      path_bar->ignore_click = FALSE;
      return;
    }

  gtk_widget_style_get (GTK_WIDGET (path_bar),
                        "spacing", &spacing,
                        NULL);

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  border_width = GTK_CONTAINER (path_bar)->border_width;
  direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));

  /* find the button at the 'right' end that we have to make visible */
  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    if (lp->next != NULL && gtk_widget_get_child_visible (GTK_WIDGET (lp->next->data)))
      {
        right_button = lp;
        break;
      }

  /* find the last visible button on the 'left' end */
  for (lp = g_list_last (path_bar->list); lp != NULL; lp = lp->prev)
    if (gtk_widget_get_child_visible (GTK_WIDGET (lp->data)))
      {
        left_button = lp;
        break;
      }

  space_needed = GTK_WIDGET (right_button->data)->allocation.width + spacing;
  if (direction == GTK_TEXT_DIR_RTL)
    {
      space_available = path_bar->right_slider->allocation.x - GTK_WIDGET (path_bar)->allocation.x;
    }
  else
    {
      space_available = (GTK_WIDGET (path_bar)->allocation.x + GTK_WIDGET (path_bar)->allocation.width - border_width)
                      - (path_bar->right_slider->allocation.x + path_bar->right_slider->allocation.width);
    }

  /* We have space_available extra space that's not being used.  We
   * need space_needed space to make the button fit.  So we walk down
   * from the end, removing path_bar until we get all the space we
   * need.
   */
  while (space_available < space_needed)
    {
      space_available += GTK_WIDGET (left_button->data)->allocation.width + spacing;
      left_button = left_button->prev;
      path_bar->first_scrolled_button = left_button;
    }
}



static void
thunar_path_bar_clicked (ThunarPathButton *button,
                         ThunarPathBar    *path_bar)
{
  ThunarFile *directory;
  GList      *lp;

  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_PATH_BAR (path_bar));

  /* determine the directory associated with the clicked button */
  directory = thunar_path_button_get_file (button);

  /* disconnect from previous current directory (if any) */
  if (G_LIKELY (path_bar->current_directory != NULL))
    g_object_unref (G_OBJECT (path_bar->current_directory));

  /* setup the new current directory */
  path_bar->current_directory = directory;

  /* take a reference on the new directory (if any) */
  if (G_LIKELY (directory != NULL))
    g_object_ref (G_OBJECT (directory));

  /* check if the button is visible on the button bar */
  if (!gtk_widget_get_child_visible (GTK_WIDGET (button)))
    {
      /* scroll to the button */
      path_bar->first_scrolled_button = g_list_find (path_bar->list, button);
      gtk_widget_queue_resize (GTK_WIDGET (path_bar));

      /* we may need to reset the fake_root_button */
      if (G_LIKELY (path_bar->fake_root_button != NULL))
        {
          /* check if the fake_root_button is before the first_scrolled_button (from right to left) */
          for (lp = path_bar->list; lp != NULL && lp != path_bar->first_scrolled_button; lp = lp->next)
            if (lp == path_bar->fake_root_button)
              {
                /* reset the fake_root_button */
                path_bar->fake_root_button = NULL;
                break;
              }
        }
    }

  /* update all path_bar */
  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    {
      /* determine the directory for this button */
      button = THUNAR_PATH_BUTTON (lp->data);
      directory = thunar_path_button_get_file (button);

      /* update the location button state (making sure to not recurse with the "clicked" handler) */
      g_signal_handlers_block_by_func (G_OBJECT (button), thunar_path_bar_clicked, path_bar);
      thunar_path_button_set_active (button, directory == path_bar->current_directory);
      g_signal_handlers_unblock_by_func (G_OBJECT (button), thunar_path_bar_clicked, path_bar);
    }

  /* notify the surrounding module that we want to change
   * to a different directory.
   */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (path_bar), path_bar->current_directory);
}



static void
thunar_path_bar_context_menu (ThunarPathButton *button,
                              GdkEventButton   *event,
                              ThunarPathBar    *path_bar)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_PATH_BAR (path_bar));

  /* determine the file for the button */
  file = thunar_path_button_get_file (button);
  if (G_LIKELY (file != NULL))
    {
      /* emit the "context-menu" signal on the path_bar */
      g_signal_emit_by_name (G_OBJECT (path_bar), "context-menu", event, file);
    }
}



/**
 * thunar_path_bar_new:
 * 
 * Creates a new #ThunarPathBar object, that does not display any path by default.
 *
 * Return value: the newly allocated #ThunarPathBar object.
 **/
GtkWidget*
thunar_path_bar_new (void)
{
  return g_object_new (THUNAR_TYPE_PATH_BAR, NULL);
}



/**
 * thunar_path_bar_down_folder:
 * @path_bar : a #ThunarPathBar.
 *
 * Goes down one folder in the @path_bar if possible.
 **/
void
thunar_path_bar_down_folder (ThunarPathBar *path_bar)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_PATH_BAR (path_bar));

  /* lookup the active button */
  for (lp = path_bar->list; lp != NULL; lp = lp->next)
    {
      if (thunar_path_button_get_active (lp->data))
        {
          /* check if we have a folder below that one */
          if (G_LIKELY (lp->prev != NULL))
            {
              /* enter that folder */
              thunar_path_button_clicked (lp->prev->data);
            }
          break;
        }
    }
}




