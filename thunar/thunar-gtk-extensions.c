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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-private.h>



static void
parent_set (GtkWidget *toolitem,
            GtkWidget *old_parent,
            GtkAction *action)
{
  /* the tooltips cannot be set before the toolitem has a parent */
  g_object_notify (G_OBJECT (action), "tooltip");
}



/**
 * thunar_gtk_action_group_create_tool_item:
 * @action_group : a #GtkActionGroup.
 * @action_name  : the name of a #GtkAction in @action_group.
 *
 * Convenience function to create a #GtkToolItem for the #GtkAction
 * named @action_name in @action_group.
 *
 * Return value: the #GtkToolItem for @action_name in @action_group.
 **/
GtkToolItem*
thunar_gtk_action_group_create_tool_item (GtkActionGroup *action_group,
                                          const gchar    *action_name)
{
  GtkAction *action;
  GtkWidget *widget;

  _thunar_return_val_if_fail (GTK_IS_ACTION_GROUP (action_group), NULL);
  _thunar_return_val_if_fail (action_name != NULL, NULL);

  action = gtk_action_group_get_action (action_group, action_name);
  if (G_UNLIKELY (action == NULL))
    return NULL;

  /* create the toolitem and take care of the tooltip work-around */
  widget = gtk_action_create_tool_item (action);
  g_signal_connect_object (G_OBJECT (widget), "parent-set", G_CALLBACK (parent_set), G_OBJECT (action), G_CONNECT_AFTER);

  return GTK_TOOL_ITEM (widget);
}



/**
 * thunar_gtk_action_group_set_action_sensitive:
 * @action_group : a #GtkActionGroup.
 * @action_name  : the name of a #GtkAction in @action_group.
 * @sensitive    : the new sensitivity.
 *
 * Convenience function to change the sensitivity of an action
 * in @action_group (whose name is @action_name) to @sensitive.
 **/
void
thunar_gtk_action_group_set_action_sensitive (GtkActionGroup *action_group,
                                              const gchar    *action_name,
                                              gboolean        sensitive)
{
  GtkAction *action;

  _thunar_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  _thunar_return_if_fail (action_name != NULL && *action_name != '\0');

  /* query the action from the group */
  action = gtk_action_group_get_action (action_group, action_name);

  /* apply the sensitivity to the action */
#if GTK_CHECK_VERSION(2,6,0)
  gtk_action_set_sensitive (action, sensitive);
#else
  g_object_set (G_OBJECT (action), "sensitive", sensitive, NULL);
#endif
}



/**
 * thunar_gtk_icon_factory_insert_icon:
 * @icon_factory : a #GtkIconFactory.
 * @stock_id     : the stock id of the icon to be inserted.
 * @icon_name    : the name of the themed icon or an absolute
 *                 path to an icon file.
 *
 * Inserts an entry into the @icon_factory, with the specified
 * @stock_id, for the given @icon_name, which can be either an
 * icon name (of a themed icon) or an absolute path to an icon
 * file.
 **/
void
thunar_gtk_icon_factory_insert_icon (GtkIconFactory *icon_factory,
                                     const gchar    *stock_id,
                                     const gchar    *icon_name)
{
  GtkIconSource *icon_source;
  GtkIconSet    *icon_set;

  _thunar_return_if_fail (GTK_IS_ICON_FACTORY (icon_factory));
  _thunar_return_if_fail (icon_name != NULL);
  _thunar_return_if_fail (stock_id != NULL);

  icon_set = gtk_icon_set_new ();
  icon_source = gtk_icon_source_new ();
  if (G_UNLIKELY (g_path_is_absolute (icon_name)))
    gtk_icon_source_set_filename (icon_source, icon_name);
  else
    gtk_icon_source_set_icon_name (icon_source, icon_name);
  gtk_icon_set_add_source (icon_set, icon_source);
  gtk_icon_factory_add (icon_factory, stock_id, icon_set);
  gtk_icon_source_free (icon_source);
  gtk_icon_set_unref (icon_set);
}



/**
 * thunar_gtk_ui_manager_get_action_by_name:
 * @ui_manager  : a #GtkUIManager.
 * @action_name : the name of a #GtkAction in @ui_manager.
 *
 * Looks up the #GtkAction with the given @action_name in all
 * #GtkActionGroup<!---->s associated with @ui_manager. Returns
 * %NULL if no such #GtkAction exists in @ui_manager.
 *
 * Return value: the #GtkAction of the given @action_name in
 *               @ui_manager or %NULL.
 **/
GtkAction*
thunar_gtk_ui_manager_get_action_by_name (GtkUIManager *ui_manager,
                                          const gchar  *action_name)
{
  GtkAction *action;
  GList     *lp;

  _thunar_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);
  _thunar_return_val_if_fail (action_name != NULL, NULL);

  /* check all action groups associated with the ui manager */
  for (lp = gtk_ui_manager_get_action_groups (ui_manager); lp != NULL; lp = lp->next)
    {
      action = gtk_action_group_get_action (lp->data, action_name);
      if (G_LIKELY (action != NULL))
        return action;
    }

  return NULL;
}



/**
 * thunar_gtk_widget_set_tooltip:
 * @widget : a #GtkWidget for which to set the tooltip.
 * @format : a printf(3)-style format string.
 * @...    : additional arguments for @format.
 *
 * Sets the tooltip for the @widget to a string generated
 * from the @format and the additional arguments in @...<!--->,
 * utilizing the shared #GtkTooltips instance.
 **/
void
thunar_gtk_widget_set_tooltip (GtkWidget   *widget,
                               const gchar *format,
                               ...)
{
  static GtkTooltips *tooltips = NULL;
  va_list             var_args;
  gchar              *tooltip;

  _thunar_return_if_fail (GTK_IS_WIDGET (widget));
  _thunar_return_if_fail (g_utf8_validate (format, -1, NULL));

  /* allocate the shared tooltips on-demand */
  if (G_UNLIKELY (tooltips == NULL))
    tooltips = gtk_tooltips_new ();

  /* determine the tooltip */
  va_start (var_args, format);
  tooltip = g_strdup_vprintf (format, var_args);
  va_end (var_args);

  /* setup the tooltip for the widget */
  gtk_tooltips_set_tip (tooltips, widget, tooltip, NULL);

  /* release the tooltip */
  g_free (tooltip);
}



