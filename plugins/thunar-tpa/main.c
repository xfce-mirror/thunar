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

#include <thunar-tpa/thunar-tpa-icon.h>

#include <libxfce4panel/xfce-panel-plugin.h>



static void thunar_tpa_construct (XfcePanelPlugin *panel_plugin);



static void
thunar_tpa_construct (XfcePanelPlugin *panel_plugin)
{
  GtkWidget *icon;
  GtkWidget *item;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* setup the trash icon */
  icon = thunar_tpa_icon_new ();
  gtk_container_add (GTK_CONTAINER (panel_plugin), icon);
  xfce_panel_plugin_add_action_widget (panel_plugin, gtk_bin_get_child (GTK_BIN (icon)));
  gtk_widget_show (icon);

  /* add the "Empty Trash" menu item */
  item = gtk_menu_item_new_with_mnemonic (_("_Empty Trash"));
  exo_binding_new (G_OBJECT (icon), "full", G_OBJECT (item), "sensitive");
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tpa_icon_empty_trash), icon);
  xfce_panel_plugin_menu_insert_item (panel_plugin, GTK_MENU_ITEM (item));
  gtk_widget_show (item);

  /* configure the plugin */
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "size-changed", G_CALLBACK (thunar_tpa_icon_set_size), icon);
}



XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (thunar_tpa_construct);
