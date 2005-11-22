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

#include <thunar/thunar-favourites-pane.h>
#include <thunar/thunar-favourites-view.h>
#include <thunar/thunar-side-pane.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void        thunar_favourites_pane_class_init            (ThunarFavouritesPaneClass *klass);
static void        thunar_favourites_pane_navigator_init        (ThunarNavigatorIface      *iface);
static void        thunar_favourites_pane_side_pane_init        (ThunarSidePaneIface       *iface);
static void        thunar_favourites_pane_init                  (ThunarFavouritesPane      *pane);
static void        thunar_favourites_pane_dispose               (GObject                   *object);
static void        thunar_favourites_pane_get_property          (GObject                   *object,
                                                                 guint                      prop_id,
                                                                 GValue                    *value,
                                                                 GParamSpec                *pspec);
static void        thunar_favourites_pane_set_property          (GObject                   *object,
                                                                 guint                      prop_id,
                                                                 const GValue              *value,
                                                                 GParamSpec                *pspec);
static ThunarFile *thunar_favourites_pane_get_current_directory (ThunarNavigator           *navigator);
static void        thunar_favourites_pane_set_current_directory (ThunarNavigator           *navigator,
                                                                 ThunarFile                *current_directory);



struct _ThunarFavouritesPaneClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarFavouritesPane
{
  GtkScrolledWindow __parent__;
  ThunarFile       *current_directory;
  GtkWidget        *view;
};



static GObjectClass *thunar_favourites_pane_parent_class;



GType
thunar_favourites_pane_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarFavouritesPaneClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_favourites_pane_class_init,
        NULL,
        NULL,
        sizeof (ThunarFavouritesPane),
        0,
        (GInstanceInitFunc) thunar_favourites_pane_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_favourites_pane_navigator_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo side_pane_info =
      {
        (GInterfaceInitFunc) thunar_favourites_pane_side_pane_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW, I_("ThunarFavouritesPane"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_SIDE_PANE, &side_pane_info);
    }

  return type;
}



static void
thunar_favourites_pane_class_init (ThunarFavouritesPaneClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_favourites_pane_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_favourites_pane_dispose;
  gobject_class->get_property = thunar_favourites_pane_get_property;
  gobject_class->set_property = thunar_favourites_pane_set_property;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");
}



static void
thunar_favourites_pane_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_favourites_pane_get_current_directory;
  iface->set_current_directory = thunar_favourites_pane_set_current_directory;
}



static void
thunar_favourites_pane_side_pane_init (ThunarSidePaneIface *iface)
{
}



static void
thunar_favourites_pane_init (ThunarFavouritesPane *pane)
{
  /* configure the GtkScrolledWindow */
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (pane), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (pane), NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (pane),
                                       GTK_SHADOW_IN);

  /* allocate the favourites view */
  pane->view = thunar_favourites_view_new ();
  gtk_container_add (GTK_CONTAINER (pane), pane->view);
  gtk_widget_show (pane->view);

  /* connect the "favourite-activated" signal */
  g_signal_connect_swapped (G_OBJECT (pane->view), "favourite-activated",
                            G_CALLBACK (thunar_navigator_change_directory), pane);
}



static void
thunar_favourites_pane_dispose (GObject *object)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);
  thunar_navigator_set_current_directory (navigator, NULL);
  G_OBJECT_CLASS (thunar_favourites_pane_parent_class)->dispose (object);
}



static void
thunar_favourites_pane_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (navigator));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_favourites_pane_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (navigator, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_favourites_pane_get_current_directory (ThunarNavigator *navigator)
{
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_PANE (navigator), NULL);
  return THUNAR_FAVOURITES_PANE (navigator)->current_directory;
}



static void
thunar_favourites_pane_set_current_directory (ThunarNavigator *navigator,
                                              ThunarFile      *current_directory)
{
  ThunarFavouritesPane *pane = THUNAR_FAVOURITES_PANE (navigator);

  g_return_if_fail (THUNAR_IS_FAVOURITES_PANE (pane));

  /* disconnect from the previously set current directory */
  if (G_LIKELY (pane->current_directory != NULL))
    g_object_unref (G_OBJECT (pane->current_directory));

  pane->current_directory = current_directory;

  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));

      /* select the file in the view (if possible) */
      thunar_favourites_view_select_by_file (THUNAR_FAVOURITES_VIEW (pane->view),
                                             pane->current_directory);
    }

  g_object_notify (G_OBJECT (pane), "current-directory");
}



/**
 * thunar_favourites_pane_new:
 *
 * Allocates a new #ThunarFavouritesPane instance.
 *
 * Return value: the newly allocated #ThunarFavouritesPane instance.
 **/
GtkWidget*
thunar_favourites_pane_new (void)
{
  return g_object_new (THUNAR_TYPE_FAVOURITES_PANE, NULL);
}



