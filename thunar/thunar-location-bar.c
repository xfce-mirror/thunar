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

#include <thunar/thunar-location-bar.h>
#include <thunar/thunar-private.h>



GType
thunar_location_bar_get_type (void)
{
  static volatile gsize type__volatile = 0;
  GType                 type;

  if (g_once_init_enter (&type__volatile))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarLocationBar"),
                                            sizeof (ThunarLocationBarIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, THUNAR_TYPE_COMPONENT);

      g_once_init_leave (&type__volatile, type);
    }

  return type__volatile;
}



/**
 * thunar_location_bar_accept_focus:
 * @location_bar : a #ThunarLocationBar.
 * @initial_text : the inital text for @location_bar if
 *                 focus is accepted, or %NULL if the
 *                 text of @location_bar shouldn't be
 *                 altered.
 *
 * If the implementation of the #ThunarLocationBar interface
 * supports entering a location into a text widget, then the
 * text widget will be focused and the method will return
 * %TRUE. The #ThunarLocationEntry is an example for such
 * an implementation.
 *
 * Else if the implementation offers no way to enter a new
 * location as text, it will simply return %FALSE here. The
 * #ThunarLocationButtons class is an example for such an
 * implementation.
 *
 * Return value: %TRUE if the @location_bar gave focus to
 *               a text entry widget provided by @location_bar,
 *               else %FALSE.
 **/
gboolean
thunar_location_bar_accept_focus (ThunarLocationBar *location_bar,
                                  const gchar       *initial_text)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BAR (location_bar), FALSE);
  return (*THUNAR_LOCATION_BAR_GET_IFACE (location_bar)->accept_focus) (location_bar, initial_text);
}



/**
 * thunar_location_bar_is_standalone:
 * @location_bar : a #ThunarLocationBar.
 *
 * Returns %TRUE if @location_bar should not be placed in the location
 * toolbar, but should be treated as a standalone component, which is
 * placed near to the view pane. Else, if %FALSE is returned, the
 * @location_bar will be placed into the location toolbar.
 *
 * Return value: %FALSE to embed @location_bar into the location toolbar,
 *               %TRUE to treat it as a standalone component.
 **/
gboolean
thunar_location_bar_is_standalone (ThunarLocationBar *location_bar)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BAR (location_bar), FALSE);
  return (*THUNAR_LOCATION_BAR_GET_IFACE (location_bar)->is_standalone) (location_bar);
}
