/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2017 Andre Miranda <andreldm@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <thunar/thunar-menu-util.h>

#include <thunarx/thunarx.h>



static void
extension_action_callback (GtkAction *action,
                           gpointer callback_data)
{
  thunarx_menu_item_activate (THUNARX_MENU_ITEM (callback_data));
}



static GtkAction *
action_from_menu_item (GObject *item)
{
  gchar *name, *label, *tooltip, *icon_str;
  gboolean  sensitive, priority;
  GtkAction *action;

  g_return_val_if_fail (THUNARX_IS_MENU_ITEM (item), NULL);

  g_object_get (G_OBJECT (item),
                "name", &name,
                "label", &label,
                "tooltip", &tooltip,
                "icon", &icon_str,
                "sensitive", &sensitive,
                "priority", &priority,
                NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  action = gtk_action_new (name, label, tooltip, NULL);

  if (icon_str != NULL)
    {
      GIcon *icon = g_icon_new_for_string (icon_str, NULL);

      if (icon)
        {
          gtk_action_set_gicon (action, icon);
          g_object_unref (icon);
        }
    }

  gtk_action_set_sensitive (action, sensitive);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_object_set (action, "is-important", priority, NULL);

  g_signal_connect_data (action, "activate",
                         G_CALLBACK (extension_action_callback),
                         g_object_ref (item),
                         (GClosureNotify) (void (*)(void)) g_object_unref, 0);

  g_free (name);
  g_free (label);
  g_free (tooltip);
  g_free (icon_str);

  return action;
}



void
thunar_menu_util_add_items_to_ui_manager (GtkUIManager   *ui_manager,
                                          GtkActionGroup *action_group,
                                          gint            merge_id,
                                          const gchar    *path,
                                          GList          *items)
{
  GList           *lp;
  GtkAction       *action;
  ThunarxMenu     *menu;
  char            *subpath;
  char            *action_path;
  GList           *children;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* add the menu items to the UI manager */
  for (lp = items; lp != NULL; lp = lp->next)
    {
      action = action_from_menu_item (G_OBJECT (lp->data));
      g_object_get (G_OBJECT (lp->data), "menu", &menu, NULL);

      /* add the action to the action group */
      gtk_action_group_add_action (action_group, action);

      /* add the action to the UI manager */
      gtk_ui_manager_add_ui (ui_manager, merge_id, path,
                             gtk_action_get_name (GTK_ACTION (action)),
                             gtk_action_get_name (GTK_ACTION (action)),
                             (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM, FALSE);

      /* TODO: Receive action path from plugin as generic data as below or create a property in ThunarxMenuItem? */
      action_path = g_object_steal_data (G_OBJECT (lp->data), "action_path");
      if (action_path)
        {
          gtk_action_set_accel_path (action, action_path);
          g_free (action_path);
        }

      /* add submenu items if any */
      if (menu != NULL) {
        children = thunarx_menu_get_items (menu);
        subpath = g_build_path ("/", path, gtk_action_get_name (action), NULL);

        thunar_menu_util_add_items_to_ui_manager (ui_manager, action_group, merge_id,
                                                  subpath, children);

        thunarx_menu_item_list_free (children);
        g_free (subpath);
      }
G_GNUC_END_IGNORE_DEPRECATIONS

      /* release the reference on item and action */
      g_object_unref (G_OBJECT (lp->data));
      g_object_unref (G_OBJECT (action));
    }
}



void
thunar_menu_util_add_items_to_menu (GtkWidget *menu,
                                    GList     *items)
{
  GList           *lp;
  GtkAction       *action;
  GtkWidget       *item;
  GtkWidget       *submenu;
  ThunarxMenu     *thunarx_menu;
  GList           *children;

  /* add the menu items to the UI manager */
  for (lp = items; lp != NULL; lp = lp->next)
    {
      action = action_from_menu_item (G_OBJECT (lp->data));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      item = gtk_action_create_menu_item (action);
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* add submenu items if any */
      g_object_get (G_OBJECT (lp->data), "menu", &thunarx_menu, NULL);
      if (thunarx_menu != NULL) {
        children = thunarx_menu_get_items (thunarx_menu);

        submenu = gtk_menu_new ();
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

        thunar_menu_util_add_items_to_menu (submenu, children);

        thunarx_menu_item_list_free (children);
      }

      /* release the reference on item and action */
      g_object_unref (G_OBJECT (lp->data));
      g_object_unref (G_OBJECT (action));
    }
}
