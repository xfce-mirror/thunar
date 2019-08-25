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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/**
 * thunar_gtk_action_set_tooltip:
 * @action : a #GtkAction.
 * @format : the format string for the tooltip.
 * @...    : the parameters for @format.
 *
 * Convenience function to set a tooltip for a #GtkAction.
 **/
void
thunar_gtk_action_set_tooltip (GtkAction   *action,
                               const gchar *format,
                               ...)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  va_list var_args;
  gchar  *tooltip;

  _thunar_return_if_fail (g_utf8_validate (format, -1, NULL));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the tooltip */
  va_start (var_args, format);
  tooltip = g_strdup_vprintf (format, var_args);
  va_end (var_args);

  /* setup the tooltip for the action */
  gtk_action_set_tooltip (action, tooltip);

  /* release the tooltip */
  g_free (tooltip);
G_GNUC_END_IGNORE_DEPRECATIONS
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
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkAction *action;

  _thunar_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  _thunar_return_if_fail (action_name != NULL && *action_name != '\0');

  /* query the action from the group */
  action = gtk_action_group_get_action (action_group, action_name);

  /* apply the sensitivity to the action */
  gtk_action_set_sensitive (action, sensitive);
G_GNUC_END_IGNORE_DEPRECATIONS
}



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
}



/**
 * thunar_gtk_menu_run:
 * @menu : a #GtkMenu.
 *
 * Conveniance wrapper for thunar_gtk_menu_run_at_event_pointer, to run a menu for the current event
 **/
void
thunar_gtk_menu_run (GtkMenu *menu)
{
  GdkEvent *event = gtk_get_current_event ();
  thunar_gtk_menu_run_at_event (menu, event);
  gdk_event_free (event);
}



#if GTK_CHECK_VERSION (3, 24, 8)
static void
moved_to_rect_cb (GdkWindow          *window,
                  const GdkRectangle *flipped_rect,
                  const GdkRectangle *final_rect,
                  gboolean            flipped_x,
                  gboolean            flipped_y,
                  GtkMenu            *menu)
{
    g_signal_emit_by_name (menu, "popped-up", 0, flipped_rect, final_rect, flipped_x, flipped_y);
    g_signal_stop_emission_by_name (window, "moved-to-rect");
}



static void
popup_menu_realized (GtkWidget *menu,
                     gpointer   user_data)
{
    GdkWindow *toplevel = gtk_widget_get_window (gtk_widget_get_toplevel (menu));
    g_signal_handlers_disconnect_by_func (toplevel, moved_to_rect_cb, menu);
    g_signal_connect (toplevel, "moved-to-rect", G_CALLBACK (moved_to_rect_cb), menu);
}
#endif



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
thunar_gtk_menu_run_at_event (GtkMenu *menu,
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

#if GTK_CHECK_VERSION (3, 24, 8)
    /* Workaround for incorrect popup menus size */
    g_signal_connect (G_OBJECT (menu), "realize", G_CALLBACK (popup_menu_realized), NULL);
    gtk_widget_realize (GTK_WIDGET (menu));
#endif

  gtk_menu_popup_at_pointer (menu, event);
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);
  _thunar_return_val_if_fail (action_name != NULL, NULL);

  /* check all action groups associated with the ui manager */
  for (lp = gtk_ui_manager_get_action_groups (ui_manager); lp != NULL; lp = lp->next)
    {
      action = gtk_action_group_get_action (lp->data, action_name);
      if (G_LIKELY (action != NULL))
        return action;
    }
G_GNUC_END_IGNORE_DEPRECATIONS

  return NULL;
}



/**
 * thunar_gtk_widget_set_tooltip:
 * @widget : a #GtkWidget for which to set the tooltip.
 * @format : a printf(3)-style format string.
 * @...    : additional arguments for @format.
 *
 * Sets the tooltip for the @widget to a string generated
 * from the @format and the additional arguments in @...<!--->.
 **/
void
thunar_gtk_widget_set_tooltip (GtkWidget   *widget,
                               const gchar *format,
                               ...)
{
  va_list  var_args;
  gchar   *tooltip;

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
 * thunar_gtk_mount_operation_new:
 * @parent : a #GtkWindow or non-toplevel widget.
 *
 * Create a mount operation with some defaults.
 **/
GMountOperation *
thunar_gtk_mount_operation_new (gpointer parent)
{
  GMountOperation *operation;
  GdkScreen       *screen;
  GtkWindow       *window = NULL;

  screen = thunar_util_parse_parent (parent, &window);

  operation = gtk_mount_operation_new (window);
  g_mount_operation_set_password_save (G_MOUNT_OPERATION (operation), G_PASSWORD_SAVE_FOR_SESSION);
  if (window == NULL && screen != NULL)
    gtk_mount_operation_set_screen (GTK_MOUNT_OPERATION (operation), screen);

  return operation;
}
