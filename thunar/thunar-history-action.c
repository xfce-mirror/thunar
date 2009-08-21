/* $Id$ */
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
                                                           ThunarHistoryAction      *history_action);



struct _ThunarHistoryActionClass
{
  GtkActionClass __parent__;
};

struct _ThunarHistoryAction
{
  GtkAction __parent__;
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



static GtkWidget*
thunar_history_action_create_tool_item (GtkAction *action)
{
  GtkWidget *tool_item;

  _thunar_return_val_if_fail (THUNAR_IS_HISTORY_ACTION (action), NULL);

  /* allocate the tool item with an empty menu */
  tool_item = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON,
                            "menu", gtk_menu_new (),
                            NULL);

  /* be sure to be informed before the menu is shown */
  g_signal_connect (G_OBJECT (tool_item), "show-menu", G_CALLBACK (thunar_history_action_show_menu), action);

  return tool_item;
}



static void
thunar_history_action_show_menu (GtkWidget           *tool_item,
                                 ThunarHistoryAction *history_action)
{
  GtkWidget *menu;

  _thunar_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (tool_item));
  _thunar_return_if_fail (THUNAR_IS_HISTORY_ACTION (history_action));

  /* allocate a new menu for the action */
  menu = gtk_menu_new ();

  /* setup the appropriate screen for the menu */
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (tool_item));

  /* generate the menu items */
  g_signal_emit_by_name (G_OBJECT (history_action), "show-menu", menu);

  /* setup the menu for the tool item */
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (tool_item), menu);
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
  _thunar_return_val_if_fail (name != NULL, NULL);

  return g_object_new (THUNAR_TYPE_HISTORY_ACTION,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "stock-id", stock_id,
                       NULL);
}

