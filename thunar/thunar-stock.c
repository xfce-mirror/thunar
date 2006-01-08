/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <thunar/thunar-stock.h>



static const gchar *thunar_stock_icons[] =
{
  THUNAR_STOCK_GROUP,
  THUNAR_STOCK_USER,
};



/**
 * thunar_stock_init:
 *
 * Initializes the stock icons used by the Thunar
 * file manager.
 **/
void
thunar_stock_init (void)
{
  GtkIconFactory *icon_factory;
  GtkIconSource  *icon_source;
  GtkIconSet     *icon_set;
  gchar           icon_name[128];
  guint           n;

  /* allocate a new icon factory for the thunar stock icons */
  icon_factory = gtk_icon_factory_new ();

  /* all icon names start with "stock_" */
  memcpy (icon_name, "stock_", sizeof ("stock_") - 1);

  /* allocate an icon source */
  icon_source = gtk_icon_source_new ();

  /* register our stock icons */
  for (n = 0; n < G_N_ELEMENTS (thunar_stock_icons); ++n)
    {
      /* determine the icon name */
      strcpy (icon_name + (sizeof ("stock_") - 1), thunar_stock_icons[n]);
      gtk_icon_source_set_icon_name (icon_source, icon_name);

      /* setup the icon set */
      icon_set = gtk_icon_set_new ();
      gtk_icon_set_add_source (icon_set, icon_source);
      gtk_icon_factory_add (icon_factory, thunar_stock_icons[n], icon_set);
      gtk_icon_set_unref (icon_set);
    }

  /* register our icon factory as default */
  gtk_icon_factory_add_default (icon_factory);

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));
  gtk_icon_source_free (icon_source);
}




