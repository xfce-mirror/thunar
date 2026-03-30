/* vi:set et ai sw=2 sts=2 ts=2: */
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

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include "thunar/thunar-gtk-extensions.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-util.h"
#include "thunarx/thunarx.h"

#include <libxfce4ui/libxfce4ui.h>


static gboolean
thunar_gtk_menu_popup_at_focus (GtkMenu  *menu,
                                GdkEvent *event);

/**
 * thunar_gtk_label_set_a11y_relation:
 * @label  : a #GtkLabel.
 * @widget : a #GtkWidget.
 *
 * Sets the %ATK_RELATION_LABEL_FOR relation on @label for @widget, which means
 * accessiblity tools will identify @label as descriptive item for the specified
 * @widget.
 **/
void
thunar_gtk_label_set_a11y_relation (GtkLabel  *label,
                                    GtkWidget *widget)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;

  _thunar_return_if_fail (GTK_IS_WIDGET (widget));
  _thunar_return_if_fail (GTK_IS_LABEL (label));

  object = gtk_widget_get_accessible (widget);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (GTK_WIDGET (label)));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
  g_object_unref (relations);
}



/**
 * thunar_gtk_label_disable_hyphens:
 * @label : a #GtkLabel.
 *
 * Tells @label to not insert hyphens when splitting a string into multiple lines
 **/
void
thunar_gtk_label_disable_hyphens (GtkLabel *label)
{
#ifdef PANGO_VERSION_1_44
  gboolean       list_created = FALSE;
  PangoAttrList *attr_list;

  _thunar_return_if_fail (GTK_IS_LABEL (label));

  attr_list = gtk_label_get_attributes (label);
  if (attr_list == NULL)
    {
      attr_list = pango_attr_list_new ();
      list_created = TRUE;
    }

  pango_attr_list_insert (attr_list, pango_attr_insert_hyphens_new (FALSE));
  gtk_label_set_attributes (label, attr_list);

  /* We only need to dereference, if we created the list at our own */
  /* (the label takes the ownership) */
  if (list_created)
    pango_attr_list_unref (attr_list);
#endif
}



void
thunar_gtk_dialog_wrap_long_text (GtkDialog *dialog)
{
  GtkWidget *message_area;
  GList     *children, *lp;

  /* additionally wrap long single-word filenames at character boundaries */
  message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
  children = gtk_container_get_children (GTK_CONTAINER (message_area));
  for (lp = children; lp != NULL; lp = lp->next)
    {
      if (GTK_IS_LABEL (lp->data))
        {
          gtk_label_set_line_wrap_mode (GTK_LABEL (lp->data), PANGO_WRAP_WORD_CHAR);
          thunar_gtk_label_disable_hyphens (GTK_LABEL (lp->data));
        }
    }
  g_list_free (children);
}



/**
 * thunar_gtk_menu_thunarx_menu_item_new:
 * @thunarx_menu_item   : a #ThunarxMenuItem
 * @menu_to_append_item : #GtkMenuShell on which the item should be appended, or NULL
 *
 * method to create a #GtkMenuItem from a #ThunarxMenuItem and append it to the passed #GtkMenuShell
 * This method will as well add all sub-items in case the passed #ThunarxMenuItem is a submenu
 *
 * Return value: (transfer full): The new #GtkImageMenuItem.
 **/
GtkWidget *
thunar_gtk_menu_thunarx_menu_item_new (GObject      *thunarx_menu_item,
                                       GtkMenuShell *menu_to_append_item)
{
  gchar       *name, *label_text, *tooltip_text, *icon_name, *accel_path;
  gboolean     sensitive;
  GtkWidget   *gtk_menu_item;
  GtkWidget   *gtk_submenu_item;
  ThunarxMenu *thunarx_menu;
  GList       *children;
  GList       *lp;
  GtkWidget   *submenu;
  GtkWidget   *image = NULL;
  GIcon       *icon = NULL;

  g_return_val_if_fail (THUNARX_IS_MENU_ITEM (thunarx_menu_item), NULL);

  g_object_get (G_OBJECT (thunarx_menu_item),
                "name", &name,
                "label", &label_text,
                "tooltip", &tooltip_text,
                "icon", &icon_name,
                "sensitive", &sensitive,
                "menu", &thunarx_menu,
                NULL);

  accel_path = g_strconcat ("<Actions>/ThunarActions/", name, NULL);
  if (icon_name != NULL)
    icon = g_icon_new_for_string (icon_name, NULL);
  if (icon != NULL)
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  gtk_menu_item = xfce_gtk_image_menu_item_new (label_text, tooltip_text, accel_path,
                                                G_CALLBACK (thunarx_menu_item_activate),
                                                G_OBJECT (thunarx_menu_item), image, menu_to_append_item);

  gtk_widget_set_sensitive (GTK_WIDGET (gtk_menu_item), sensitive);

  /* recursively add submenu items if any */
  if (gtk_menu_item != NULL && thunarx_menu != NULL)
    {
      children = thunarx_menu_get_items (thunarx_menu);
      submenu = gtk_menu_new ();
      for (lp = children; lp != NULL; lp = lp->next)
        {
          gtk_submenu_item = thunar_gtk_menu_thunarx_menu_item_new (lp->data, GTK_MENU_SHELL (submenu));

          /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item */
          g_signal_connect_swapped (G_OBJECT (gtk_submenu_item), "destroy", G_CALLBACK (g_object_unref), lp->data);
        }
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_menu_item), submenu);
      g_list_free (children);
    }

  g_free (name);
  g_free (accel_path);
  g_free (label_text);
  g_free (tooltip_text);
  g_free (icon_name);
  if (icon != NULL)
    g_object_unref (icon);
  if (thunarx_menu != NULL)
    g_object_unref (thunarx_menu);

  return gtk_menu_item;
}



/**
 * thunar_gtk_menu_clean:
 * @menu : a #GtkMenu.
 *
 * Walks through the menu and all submenus and removes them,
 * so that the result will be a clean #GtkMenu without any items
 **/
void
thunar_gtk_menu_clean (GtkMenu *menu)
{
  GList     *children, *lp;
  GtkWidget *submenu;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (lp = children; lp != NULL; lp = lp->next)
    {
      submenu = gtk_menu_item_get_submenu (lp->data);
      if (submenu != NULL)
        gtk_widget_destroy (submenu);
      gtk_container_remove (GTK_CONTAINER (menu), lp->data);
    }
  g_list_free (children);
}



/**
 * thunar_gtk_menu_run:
 * @menu : a #GtkMenu.
 *
 * Convenience wrapper for thunar_gtk_menu_run_at_event() to run a menu for the current event
 **/
void
thunar_gtk_menu_run (GtkMenu *menu)
{
  GdkEvent *event = gtk_get_current_event ();

  /* hide mnemonics in DnD menu by adding button-release-event parameters */
  if (event != NULL && event->type == GDK_DROP_START)
    {
      event->button.type = GDK_BUTTON_RELEASE;
      event->button.button = 3;
    }

  thunar_gtk_menu_run_at_event (menu, event);
  gdk_event_free (event);
}



static void
thunar_gtk_menu_popup_at_pointer (GtkMenu  *menu,
                                  GdkEvent *event)
{
  GdkWindow   *window;
  GdkDevice   *device;
  GdkDisplay  *display;
  GdkSeat     *seat;
  GdkRectangle rect = { 0, 0, 1, 1 };

  /* fallback if event not set */
  if (event == NULL)
    {
      gtk_menu_popup_at_pointer (menu, event);
      return;
    }

  /* create popup rect */
  window = gdk_event_get_window (event);
  if (window != NULL)
    {
      device = gdk_event_get_device (event);

      if (device != NULL && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
        {
          /* for keyboard interaction, try to position menu at keyboard focus */
          if (thunar_gtk_menu_popup_at_focus (menu, event))
            return;

          device = gdk_device_get_associated_device (device);
        }

      if (device != NULL)
        {
          /* Ensure that we have the toplevel window to work around a GTK3 Wayland bug
           * that prevents the user from dismissing the menu via click in some cases.
           * See: https://gitlab.xfce.org/xfce/thunar/-/issues/1592 */
          window = gdk_window_get_toplevel (window);

          gdk_window_get_device_position (window, device, &rect.x, &rect.y, NULL);
        }

      /* fallback needed for right-click DnD from xfdesktop to Thunar */
      if (device == NULL)
        {
          display = gdk_window_get_display (window);
          seat = gdk_display_get_default_seat (display);
          device = gdk_seat_get_pointer (seat);

          gdk_device_get_position (device, NULL, &rect.x, &rect.y);
          window = gdk_device_get_window_at_position (device, &rect.x, &rect.y);
        }
    }

  /* pop up the menu at the rectangle below the mouse cursor */
  gtk_menu_popup_at_rect (menu,
                          window,
                          &rect,
                          GDK_GRAVITY_SOUTH_EAST,
                          GDK_GRAVITY_NORTH_WEST,
                          event);
}

static gboolean
thunar_gtk_popup_menu_at_rect (GtkMenu     *menu,
                               GdkRectangle rect,
                               GdkWindow   *widget_window)
{
  GdkRectangle widget_area;

  if (!GDK_IS_WINDOW (widget_window))
    return FALSE;

  /* check if rect is inside widget_window area */
  widget_area.y = 0;
  widget_area.x = 0;
  widget_area.height = gdk_window_get_height (widget_window);
  widget_area.width = gdk_window_get_width (widget_window);
  if (!gdk_rectangle_intersect (&rect, &widget_area, &rect))
    return FALSE;

  gtk_menu_popup_at_rect (menu,
                          widget_window,
                          &rect,
                          GDK_GRAVITY_SOUTH_WEST,
                          GDK_GRAVITY_NORTH_WEST,
                          NULL);

  return TRUE;
}


static gboolean
thunar_gtk_menu_popup_at_focus (GtkMenu  *menu,
                                GdkEvent *event)
{
  GtkWidget *event_widget = gtk_get_event_widget (event);
  if (event_widget == NULL)
    return FALSE;

  /* sometimes is not the focus widget that handled the event, find the focused widget */
  GtkWidget *toplevel = gtk_widget_get_toplevel (event_widget);
  if (!GTK_IS_WINDOW (toplevel))
    return FALSE;

  GtkWidget *focus_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));
  if (focus_widget == NULL)
    return FALSE;

  /* to position items inside tree view, is required to get their rectangle */
  if (GTK_IS_TREE_VIEW (focus_widget))
    {
      GtkTreeView       *tree = GTK_TREE_VIEW (focus_widget);
      GtkTreeSelection  *selection = gtk_tree_view_get_selection (tree);
      GList             *selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
      if (selected_rows != NULL)
        {
          GdkRectangle  rect;
          GdkWindow    *widget_window;
          GtkTreePath       *path = (GtkTreePath *) g_list_last (selected_rows)->data;
          GtkTreeViewColumn *column = gtk_tree_view_get_column (tree, 0);

          gtk_tree_view_get_cell_area (tree, path, column, &rect);
          path = NULL;
          g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);

          /* convert rect coordinates to widget_window coordinates */
          gtk_tree_view_convert_bin_window_to_widget_coords (tree, rect.x, rect.y, &rect.x, &rect.y);

          widget_window = gtk_widget_get_window (focus_widget);
          if (thunar_gtk_popup_menu_at_rect (menu, rect, widget_window))
            return TRUE;
        }
    }

  /* position menu in files inside icon view */
  if (XFCE_IS_ICON_VIEW (focus_widget))
    {
      GList *selected_files = xfce_icon_view_get_selected_items (XFCE_ICON_VIEW (focus_widget));
      if (selected_files != NULL)
        {
          GdkRectangle rect;
          GdkWindow   *widget_window;
          GtkTreePath *path = (GtkTreePath *) g_list_last (selected_files)->data;

          xfce_icon_view_get_cell_area (XFCE_ICON_VIEW (focus_widget), path, NULL, &rect);
          path = NULL;
          g_list_free_full (selected_files, (GDestroyNotify) gtk_tree_path_free);

          /* convert rect coordinates to widget_window coordinates */
          GtkAdjustment *h_adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (focus_widget));
          GtkAdjustment *v_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (focus_widget));
          rect.x -= (int) gtk_adjustment_get_value (h_adjustment);
          rect.y -= (int) gtk_adjustment_get_value (v_adjustment);

          widget_window = gtk_widget_get_window (focus_widget);
          if (thunar_gtk_popup_menu_at_rect (menu, rect, widget_window))
            return TRUE;
        }
    }

  if (GTK_IS_NOTEBOOK (focus_widget))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK (focus_widget);
      gint         page_num = gtk_notebook_get_current_page (notebook);
      if (page_num > -1)
        {
          GtkWidget *page = gtk_notebook_get_nth_page (notebook, page_num);
          if (page != NULL)
            {
              GtkWidget *tab_label = gtk_notebook_get_tab_label (notebook, page);
              if (tab_label != NULL)
                focus_widget = tab_label;
            }
        }
    }

  gtk_menu_popup_at_widget (menu,
                            focus_widget,
                            GDK_GRAVITY_SOUTH_WEST,
                            GDK_GRAVITY_NORTH_WEST,
                            event);

  return TRUE;
}


/**
 * thunar_gtk_menu_run_at_event:
 * @menu  : a #GtkMenu.
 * @event : a #GdkEvent which may be NULL if no previous event was stored.
 *
 * A simple wrapper around gtk_menu_popup_at_pointer(), which runs the @menu in a separate
 * main loop and returns only after the @menu was deactivated.
 *
 * This method automatically takes over the floating reference of @menu if any and
 * releases it on return. That means if you created the menu via gtk_menu_new() you'll
 * not need to take care of destroying the menu later.
 *
 **/
void
thunar_gtk_menu_run_at_event (GtkMenu  *menu,
                              GdkEvent *event)
{
  GMainLoop *loop;
  gulong     signal_id;

  _thunar_return_if_fail (GTK_IS_MENU (menu));

  /* take over the floating reference on the menu */
  g_object_ref_sink (G_OBJECT (menu));

  /* run an internal main loop */
  loop = g_main_loop_new (NULL, FALSE);
  signal_id = g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);
  thunar_gtk_menu_popup_at_pointer (menu, event);
  gtk_menu_reposition (menu);
  gtk_grab_add (GTK_WIDGET (menu));
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  gtk_grab_remove (GTK_WIDGET (menu));

  g_signal_handler_disconnect (G_OBJECT (menu), signal_id);

  /* release the menu reference */
  g_object_unref (G_OBJECT (menu));
}



/**
 * thunar_gtk_menu_hide_accel_labels:
 * @menu : a #GtkMenu instance
 *
 * Will hide the accel_labels of all menu items of this menu and its submenus
 **/
void
thunar_gtk_menu_hide_accel_labels (GtkMenu *menu)
{
  GList     *children, *lp;
  GtkWidget *submenu;

  _thunar_return_if_fail (GTK_IS_MENU (menu));

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (lp = children; lp != NULL; lp = lp->next)
    {
      xfce_gtk_menu_item_set_accel_label (lp->data, NULL);
      submenu = gtk_menu_item_get_submenu (lp->data);
      if (submenu != NULL)
        thunar_gtk_menu_hide_accel_labels (GTK_MENU (submenu));
    }
  g_list_free (children);
}



/**
 * thunar_gtk_widget_set_tooltip:
 * @widget : a #GtkWidget for which to set the tooltip.
 * @format : a printf(3)-style format string.
 * @...    : additional arguments for @format.
 *
 * Sets the tooltip for the @widget to a string generated
 * from the @format and the additional arguments in @...<!---->.
 **/
void
thunar_gtk_widget_set_tooltip (GtkWidget   *widget,
                               const gchar *format,
                               ...)
{
  va_list var_args;
  gchar  *tooltip;

  _thunar_return_if_fail (GTK_IS_WIDGET (widget));
  _thunar_return_if_fail (g_utf8_validate (format, -1, NULL));

  /* determine the tooltip */
  va_start (var_args, format);
  tooltip = g_strdup_vprintf (format, var_args);
  va_end (var_args);

  /* setup the tooltip for the widget */
  gtk_widget_set_tooltip_text (widget, tooltip);

  /* release the tooltip */
  g_free (tooltip);
}



/**
 * thunar_gtk_get_focused_widget:
 * Return value: (transfer none): currently focused widget or NULL, if there is none.
 **/
GtkWidget *
thunar_gtk_get_focused_widget (void)
{
  GtkApplication *app;
  GtkWindow      *window;
  app = GTK_APPLICATION (g_application_get_default ());
  if (NULL == app)
    return NULL;

  window = gtk_application_get_active_window (app);

  return gtk_window_get_focus (window);
}



/**
 * thunar_gtk_mount_operation_new:
 * @parent : a #GtkWindow or non-toplevel widget.
 *
 * Create a mount operation with some defaults.
 **/
GMountOperation *
thunar_gtk_mount_operation_new (gpointer parent)
{
  GMountOperation *operation;
  GtkWindow       *window;

  window = thunar_util_find_associated_window (parent);

  operation = gtk_mount_operation_new (window);
  g_mount_operation_set_password_save (G_MOUNT_OPERATION (operation), G_PASSWORD_SAVE_FOR_SESSION);

  /* If a specific screen is requested, we spawn the dialog on that screen */
  if (G_UNLIKELY (parent != NULL && GDK_IS_SCREEN (parent)))
    gtk_mount_operation_set_screen (GTK_MOUNT_OPERATION (operation), GDK_SCREEN (parent));

  return operation;
}



/**
 * thunar_gtk_editable_can_cut:
 *
 * Return value: TRUE if it's possible to cut text off of a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean
thunar_gtk_editable_can_cut (GtkEditable *editable)
{
  return gtk_editable_get_editable (editable) && thunar_gtk_editable_can_copy (editable);
}



/**
 * thunar_gtk_editable_can_copy:
 *
 * Return value: TRUE if it's possible to copy text from a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean
thunar_gtk_editable_can_copy (GtkEditable *editable)
{
  return gtk_editable_get_selection_bounds (editable, NULL, NULL);
}



/**
 * thunar_gtk_editable_can_paste:
 *
 * Return value: TRUE if it's possible to paste text to a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean
thunar_gtk_editable_can_paste (GtkEditable *editable)
{
  return gtk_editable_get_editable (editable);
}



/**
 * thunar_gtk_orientable_get_center_pos:
 *
 * Return value: The pixel corrsponding to the center of the orientable widget (either in height or in width)
 **/
gint
thunar_gtk_orientable_get_center_pos (GtkOrientable *orientable)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (GTK_WIDGET (orientable), &allocation);
  if (gtk_orientable_get_orientation (orientable) == GTK_ORIENTATION_HORIZONTAL)
    return (gint) (allocation.width / 2);
  else
    return (gint) (allocation.height / 2);
}



void
thunar_gtk_window_set_screen (GtkWindow *window,
                              GdkScreen *screen)
{
  /* If we want to run the dialog on a specific screen, we need to wipe the transient parent */
  /* That is, because the window cannot be transient for a window on a different screen (workspace on that screen) */
  gtk_window_set_transient_for (window, NULL);
  gtk_window_set_screen (window, screen);
}
