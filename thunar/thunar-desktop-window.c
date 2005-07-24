/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
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

#include <thunar/thunar-desktop-model.h>
#include <thunar/thunar-desktop-window.h>
#include <thunar/thunar-desktop-view.h>



static void     thunar_desktop_window_class_init    (ThunarDesktopWindowClass *klass);
static void     thunar_desktop_window_init          (ThunarDesktopWindow      *window);
static void     thunar_desktop_window_size_request  (GtkWidget                *widget,
                                                     GtkRequisition           *requisition);
static void     thunar_desktop_window_realize       (GtkWidget                *widget);
static void     thunar_desktop_window_unrealize     (GtkWidget                *widget);
static gboolean thunar_desktop_window_forward_event (GtkWidget                *widget,
                                                     GdkEvent                 *event);



struct _ThunarDesktopWindowClass
{
  GtkWindowClass __parent__;
};

struct _ThunarDesktopWindow
{
  GtkWindow __parent__;

  GtkWidget *view;
};



G_DEFINE_TYPE (ThunarDesktopWindow, thunar_desktop_window, GTK_TYPE_WINDOW);



static void
thunar_desktop_window_class_init (ThunarDesktopWindowClass *klass)
{
  GtkWidgetClass *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunar_desktop_window_size_request;
  gtkwidget_class->realize = thunar_desktop_window_realize;
  gtkwidget_class->unrealize = thunar_desktop_window_unrealize;
  gtkwidget_class->button_press_event = (gpointer) thunar_desktop_window_forward_event;
  gtkwidget_class->button_release_event = (gpointer) thunar_desktop_window_forward_event;
}



static void
thunar_desktop_window_init (ThunarDesktopWindow *window)
{
  ThunarDesktopModel *model;

  GTK_WIDGET_UNSET_FLAGS (window, GTK_DOUBLE_BUFFERED);

  gtk_widget_add_events (GTK_WIDGET (window),
                         GDK_BUTTON_PRESS_MASK
                       | GDK_BUTTON_RELEASE_MASK);

  gtk_window_move (GTK_WINDOW (window), 0, 0);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DESKTOP);

  /* setup the view */
  window->view = thunar_desktop_view_new ();
  thunar_desktop_view_set_file_column (THUNAR_DESKTOP_VIEW (window->view), THUNAR_DESKTOP_MODEL_COLUMN_FILE);
  thunar_desktop_view_set_position_column (THUNAR_DESKTOP_VIEW (window->view), THUNAR_DESKTOP_MODEL_COLUMN_POSITION);
  gtk_container_add (GTK_CONTAINER (window), window->view);
  gtk_widget_show (window->view);

  /* setup the model */
  model = thunar_desktop_model_get_default ();
  thunar_desktop_view_set_model (THUNAR_DESKTOP_VIEW (window->view), GTK_TREE_MODEL (model));
  g_object_unref (G_OBJECT (model));
}



static void
thunar_desktop_window_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition)
{
  GdkScreen *screen = gtk_widget_get_screen (widget);

  requisition->width = gdk_screen_get_width (screen);
  requisition->height = gdk_screen_get_height (screen);
}



static void
thunar_desktop_window_realize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkWindow *root;
  GdkAtom    atom;
  Window     xid;

  GTK_WIDGET_CLASS (thunar_desktop_window_parent_class)->realize (widget);

  xid = GDK_DRAWABLE_XID (widget->window);
  screen = gtk_widget_get_screen (widget);
  root = gdk_screen_get_root_window (screen);

  /* configure this window to be a "desktop" window */
  atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
  gdk_property_change (widget->window,
                       gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
                       gdk_atom_intern ("ATOM", FALSE), 32,
                       GDK_PROP_MODE_REPLACE, (gpointer) &atom, 1);

  /* tell the root window that we have a new "desktop" window */
  gdk_property_change (root,
                       gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
                       gdk_atom_intern ("WINDOW", FALSE), 32,
                       GDK_PROP_MODE_REPLACE, (gpointer) &xid, 1);
  gdk_property_change (root,
                       gdk_atom_intern ("XFCE_DESKTOP_WINDOW", FALSE),
                       gdk_atom_intern ("WINDOW", FALSE), 32,
                       GDK_PROP_MODE_REPLACE, (gpointer) &xid, 1);

  /* XRandR support */
  g_signal_connect_swapped (G_OBJECT (screen), "size-changed", G_CALLBACK (gtk_widget_queue_resize), widget);
}



static void
thunar_desktop_window_unrealize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkWindow *root;

  screen = gtk_widget_get_screen (widget);
  root = gdk_screen_get_root_window (screen);

  /* disconnect the XRandR support handler */
  g_signal_handlers_disconnect_by_func (G_OBJECT (screen), gtk_widget_queue_resize, widget);

  /* unset root window properties */
  gdk_property_delete (root, gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", FALSE));
  gdk_property_delete (root, gdk_atom_intern ("XFCE_DESKTOP_WINDOW", FALSE));

  GTK_WIDGET_CLASS (thunar_desktop_window_parent_class)->unrealize (widget);
}



static gboolean
thunar_desktop_window_forward_event (GtkWidget *widget,
                                     GdkEvent  *event)
{
  if (G_LIKELY (GTK_BIN (widget)->child != NULL))
    return gtk_widget_event (GTK_BIN (widget)->child, event);
  return FALSE;
}



/**
 * thunar_desktop_window_new:
 *
 * Allocates a new #ThunarDesktopWindow instance.
 *
 * Return value: the newly allocated #ThunarDesktopWindow.
 **/
GtkWidget*
thunar_desktop_window_new (void)
{
  return thunar_desktop_window_new_with_screen (gdk_screen_get_default ());
}



/**
 * thunar_desktop_window_new_with_screen:
 * @screen : a #GdkScreen.
 *
 * Allocates a new #ThunarDesktopWindow instance and
 * associates it with the given @screen.
 *
 * Return value: the newly allocated #ThunarDesktopWindow.
 **/
GtkWidget*
thunar_desktop_window_new_with_screen (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return g_object_new (THUNAR_TYPE_DESKTOP_WINDOW,
                       "screen", screen,
                       NULL);
}

