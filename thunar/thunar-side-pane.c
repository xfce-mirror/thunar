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

#include <thunar/thunar-side-pane.h>



static void thunar_side_pane_class_init (gpointer klass);



GType
thunar_side_pane_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarSidePaneIface),
        NULL,
        NULL,
        (GClassInitFunc) thunar_side_pane_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "ThunarSidePane",
                                     &info, 0);

      g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
    }

  return type;
}



static void
thunar_side_pane_class_init (gpointer klass)
{
  /**
   * ThunarSidePane:current-directory:
   *
   * The directory currently displayed by the side pane or %NULL if no
   * directory is currently selected for the whole window (for example
   * if there was an error opening the directory and the window is
   * currently displaying an error message to the user).
   *
   * Whenever the side pane wants the window to change to another
   * directory, it should set its internal current_directory variable
   * and afterwards call #g_object_notify() on the "current-directory"
   * property.
   *
   * The side pane should always display the current directory set from
   * outside (by the window), even if it seems as the user has selected
   * another directory.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("current-directory",
                                                            _("Current directory"),
                                                            _("The directory currently displayed in the side pane"),
                                                            THUNAR_TYPE_FILE,
                                                            G_PARAM_READWRITE));
}



/**
 * thunar_side_pane_get_current_directory:
 * @pane : a #ThunarSidePane instance.
 *
 * Returns the directory currently set for @pane or %NULL if
 * @pane has no current directory.
 *
 * Return value: the current directory for @pane or %NULL.
 **/
ThunarFile*
thunar_side_pane_get_current_directory (ThunarSidePane *pane)
{
  g_return_val_if_fail (THUNAR_IS_SIDE_PANE (pane), NULL);
  return THUNAR_SIDE_PANE_GET_IFACE (pane)->get_current_directory (pane);
}



/**
 * thunar_side_pane_set_current_directory:
 * @pane              : a #ThunarSidePane instance.
 * @current_directory : the new current directory for @pane or %NULL.
 *
 * Sets the new current directory that should be displayed by @pane
 * to @current_directory.
 **/
void
thunar_side_pane_set_current_directory (ThunarSidePane *pane,
                                        ThunarFile     *current_directory)
{
  g_return_if_fail (THUNAR_IS_SIDE_PANE (pane));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));
  THUNAR_SIDE_PANE_GET_IFACE (pane)->set_current_directory (pane, current_directory);
}


