/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-history-action.h>
#include <thunar/thunar-private.h>



static GtkWidget *thunar_history_action_create_tool_item  (GtkAction                *action);
static void       thunar_history_action_show_menu         (GtkWidget                *tool_item,
                                                           ThunarHistoryAction      *history_action,
                                                           guint button,
                                                           guint32 timestamp);



struct _ThunarHistoryActionClass
{
  GtkActionClass __parent__;
};

struct _ThunarHistoryAction
{
  GtkAction __parent__;

  guint popup_delay;
};



G_DEFINE_TYPE (ThunarHistoryAction, thunar_history_action, GTK_TYPE_ACTION)



static void
thunar_history_action_class_init (ThunarHistoryActionClass *klass)
{
  GtkActionClass *gtkaction_class;

  gtkaction_class = GTK_ACTION_CLASS (klass);
  gtkaction_class->create_tool_item = thunar_history_action_create_tool_item;

  /**
   * ThunarHistoryAction::show-menu:
   * @history_action : a #ThunarHistoryAction.
   * @menu           : the #GtkMenu to which to add the items to display.
   *
   * Emitted by the @history_action right before the @menu is shown.
   **/
  g_signal_new (I_("show-menu"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, GTK_TYPE_MENU);
}



static void
thunar_history_action_init (ThunarHistoryAction *actions_changed)
{
}



static gboolean
thunar_history_action_popup_delayed (gpointer data)
{
  GtkWidget           *button = GTK_WIDGET (data);
  ThunarHistoryAction *history_action;

  history_action = g_object_get_data (G_OBJECT (button), I_("thunar-history-action"));

  GDK_THREADS_ENTER ();
  thunar_history_action_show_menu (button, history_action, 3, 0);
  GDK_THREADS_LEAVE ();

  history_action->popup_delay = 0;

  return FALSE;
}



static gboolean
thunar_history_action_button_press_event (GtkWidget      *toggle_button,
                                          GdkEventButton *event,
                                          GtkWidget      *tool_item)
{
  ThunarHistoryAction *history_action;

  _thunar_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  history_action = g_object_get_data (G_OBJECT (toggle_button), I_("thunar-history-action"));

  if (event->button == 1)
    {
      /* shouldn't happen, but stop pending timeout */
      if (history_action->popup_delay != 0)
        g_source_remove (history_action->popup_delay);

      /* schedule a popup for button press */
      history_action->popup_delay = g_timeout_add (500, thunar_history_action_popup_delayed, toggle_button);
    }
  else if (event->button == 3)
    {
      /* directy show the menu */
      thunar_history_action_show_menu (toggle_button, history_action,
                                       event->button, event->time);
    }

  return FALSE;
}



static gboolean
thunar_history_action_button_release_event (GtkWidget      *toggle_button,
                                            GdkEventButton *event,
                                            GtkWidget      *tool_item)
{
  ThunarHistoryAction *history_action;

  _thunar_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  if (event->button == 1)
    {
      history_action = g_object_get_data (G_OBJECT (toggle_button), I_("thunar-history-action"));
      if (history_action->popup_delay != 0)
        {
          /* stop timeout */
          g_source_remove (history_action->popup_delay);
          history_action->popup_delay = 0;

          /* activate event */
          gtk_action_activate (GTK_ACTION (history_action));
        }
    }
  else
    {
      return TRUE;
    }

  /* bit of a strange trick to get the button untoggeled */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_button),
      gtk_widget_is_sensitive (toggle_button));

  return FALSE;
}



static gboolean
thunar_history_action_leave_notify_event (GtkWidget           *toggle_button,
                                          GdkEventCrossing    *event,
                                          ThunarHistoryAction *history_action)
{

  GtkAllocation alloc;

  _thunar_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_HISTORY_ACTION (history_action), FALSE);

  if (history_action->popup_delay != 0)
    {
      /* stop the timeout */
      g_source_remove (history_action->popup_delay);
      history_action->popup_delay = 0;

      /* if the user dragged to the bottom, directly popup the menu */
      gtk_widget_get_allocation (toggle_button, &alloc);
      if (event->x >= 0
          && event->x < alloc.width
          && event->y >= alloc.height)
        {
          thunar_history_action_show_menu (toggle_button, history_action,
                                           3, event->time);
        }
    }

  return FALSE;
}



static void
thunar_history_action_activate (GtkWidget           *toggle_button,
                                ThunarHistoryAction *history_action)
{
  _thunar_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));
  _thunar_return_if_fail (THUNAR_IS_HISTORY_ACTION (history_action));

  /* activate event (only key events trigger this function) */
  gtk_action_activate (GTK_ACTION (history_action));

  /* activate, so the code deactivates a bit later... */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_button), TRUE);
}



static void
thunar_history_action_toolbar_configured (GtkWidget *tool_item,
                                          GtkWidget *toggle_button)
{
  GtkWidget *icon;
  GtkAction *action;

  gtk_button_set_relief (GTK_BUTTON (toggle_button),
      gtk_tool_item_get_relief_style (GTK_TOOL_ITEM (tool_item)));

  icon = gtk_bin_get_child (GTK_BIN (toggle_button));
  action = g_object_get_data (G_OBJECT (toggle_button), I_("thunar-history-action"));
  gtk_image_set_from_stock (GTK_IMAGE (icon),
                            gtk_action_get_stock_id (action),
                            gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (tool_item)));
}



static GtkWidget*
thunar_history_action_create_tool_item (GtkAction *action)
{
  GtkWidget *tool_item;
  GtkWidget *button;
  GtkWidget *icon;

  _thunar_return_val_if_fail (THUNAR_IS_HISTORY_ACTION (action), NULL);

  /* allocate the tool item with an empty menu */
  tool_item = g_object_new (GTK_TYPE_TOOL_ITEM, NULL);
  gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (tool_item), TRUE);

  button = gtk_toggle_button_new ();
  gtk_container_add (GTK_CONTAINER (tool_item), button);
  gtk_button_set_relief (GTK_BUTTON (button),
      gtk_tool_item_get_relief_style (GTK_TOOL_ITEM (tool_item)));
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_widget_show (button);

  icon = gtk_image_new_from_stock (gtk_action_get_stock_id (action),
      gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (tool_item)));
  gtk_container_add (GTK_CONTAINER (button), icon);
  gtk_widget_show (icon);

  g_object_set_data (G_OBJECT (button), I_("thunar-history-action"), action);

  g_signal_connect (G_OBJECT (tool_item), "toolbar-reconfigured",
      G_CALLBACK (thunar_history_action_toolbar_configured), button);
  g_signal_connect (G_OBJECT (button), "button-press-event",
      G_CALLBACK (thunar_history_action_button_press_event), tool_item);
  g_signal_connect (G_OBJECT (button), "button-release-event",
      G_CALLBACK (thunar_history_action_button_release_event), tool_item);
  g_signal_connect (G_OBJECT (button), "leave-notify-event",
      G_CALLBACK (thunar_history_action_leave_notify_event), action);
  g_signal_connect (G_OBJECT (button), "activate",
      G_CALLBACK (thunar_history_action_activate), action);

  return tool_item;
}



static void
thunar_history_action_position_menu (GtkMenu  *menu,
                                     gint     *x,
                                     gint     *y,
                                     gboolean *push_in,
                                     gpointer  user_data)
{
  GtkWidget     *button = GTK_WIDGET (user_data);
  GtkAllocation  alloc;

  gdk_window_get_origin (gtk_widget_get_window (button), x, y);

  gtk_widget_get_allocation (button, &alloc);
  *x += alloc.x;
  *y += alloc.y + alloc.height;
  *push_in = FALSE;
}



static void
thunar_history_action_menu_deactivate (GtkWidget *toggle_button)
{
  /* untoggle the button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_button), FALSE);
}



static void
thunar_history_action_show_menu (GtkWidget           *toggle_button,
                                 ThunarHistoryAction *history_action,
                                 guint                button,
                                 guint32              timestamp)
{
  GtkWidget *menu;

  _thunar_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));
  _thunar_return_if_fail (THUNAR_IS_HISTORY_ACTION (history_action));

  /* allocate a new menu for the action */
  menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (menu), toggle_button, NULL);
  g_signal_connect_swapped (G_OBJECT (menu), "deactivate",
      G_CALLBACK (thunar_history_action_menu_deactivate), toggle_button);

  /* generate the menu items */
  g_signal_emit_by_name (G_OBJECT (history_action), "show-menu", menu);

  /* toggle the button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_button), TRUE);

  /* show menu */
  gtk_menu_popup (GTK_MENU (menu),
                  NULL, NULL,
                  thunar_history_action_position_menu, toggle_button,
                  button,
                  timestamp ? timestamp : gtk_get_current_event_time ());
}



/**
 * thunar_history_action_new:
 * @name     : the name for the action.
 * @label    : the label for the action.
 * @tooltip  : the tooltip for the action.
 * @stock_id : the stock-id for the action.
 *
 * Allocates a new #ThunarHistoryAction with the specified
 * parameters.
 *
 * Return value: the newly allocated #ThunarHistoryAction.
 **/
GtkAction*
thunar_history_action_new (const gchar *name,
                           const gchar *label,
                           const gchar *tooltip,
                           const gchar *stock_id)
{
  gchar     *fulltip;
  GtkAction *action;

  _thunar_return_val_if_fail (name != NULL, NULL);

  /* extend history tooltip with function of the button */
  fulltip = g_strconcat (tooltip, "\n", _("Right-click or pull down to show history"), NULL);

  action = g_object_new (THUNAR_TYPE_HISTORY_ACTION,
                         "name", name,
                         "label", label,
                         "tooltip", fulltip,
                         "stock-id", stock_id,
                         NULL);

  g_free (fulltip);

  return action;
}

