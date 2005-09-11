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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-gtk-extensions.h>



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

  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL && *action_name != '\0');

  /* query the action from the group */
  action = gtk_action_group_get_action (action_group, action_name);

  /* apply the sensitivity to the action */
#if GTK_CHECK_VERSION(2,6,0)
  gtk_action_set_sensitive (action, sensitive);
#else
  g_object_set (G_OBJECT (action), "sensitive", sensitive, NULL);
#endif
}


