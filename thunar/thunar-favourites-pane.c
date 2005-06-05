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
static ThunarFile *thunar_favourites_pane_get_current_directory (ThunarSidePane            *side_pane);
static void        thunar_favourites_pane_set_current_directory (ThunarSidePane            *side_pane,
                                                                 ThunarFile                *current_directory);
static void        thunar_favourites_pane_file_destroy          (ThunarFile                *file,
                                                                 ThunarFavouritesPane      *pane);



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



static GObjectClass *parent_class;



G_DEFINE_TYPE_WITH_CODE (ThunarFavouritesPane,
                         thunar_favourites_pane,
                         GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_SIDE_PANE,
                                                thunar_favourites_pane_side_pane_init));



static void
thunar_favourites_pane_class_init (ThunarFavouritesPaneClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_favourites_pane_dispose;
  gobject_class->get_property = thunar_favourites_pane_get_property;
  gobject_class->set_property = thunar_favourites_pane_set_property;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");
}



static void
thunar_favourites_pane_side_pane_init (ThunarSidePaneIface *iface)
{
  iface->get_current_directory = thunar_favourites_pane_get_current_directory;
  iface->set_current_directory = thunar_favourites_pane_set_current_directory;
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
                            G_CALLBACK (thunar_side_pane_set_current_directory), pane);
}



static void
thunar_favourites_pane_dispose (GObject *object)
{
  ThunarSidePane *side_pane = THUNAR_SIDE_PANE (object);
  thunar_side_pane_set_current_directory (side_pane, NULL);
  G_OBJECT_CLASS (parent_class)->dispose (object);
}



static void
thunar_favourites_pane_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarSidePane *side_pane = THUNAR_SIDE_PANE (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_side_pane_get_current_directory (side_pane));
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
  ThunarSidePane *side_pane = THUNAR_SIDE_PANE (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_side_pane_set_current_directory (side_pane, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_favourites_pane_get_current_directory (ThunarSidePane *side_pane)
{
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_PANE (side_pane), NULL);
  return THUNAR_FAVOURITES_PANE (side_pane)->current_directory;
}



static void
thunar_favourites_pane_set_current_directory (ThunarSidePane *side_pane,
                                              ThunarFile     *current_directory)
{
  ThunarFavouritesPane *pane = THUNAR_FAVOURITES_PANE (side_pane);

  g_return_if_fail (THUNAR_IS_FAVOURITES_PANE (pane));

  if (G_LIKELY (pane->current_directory != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (pane->current_directory),
                                            G_CALLBACK (thunar_favourites_pane_file_destroy),
                                            pane);
      g_object_unref (G_OBJECT (pane->current_directory));
    }

  pane->current_directory = current_directory;

  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));
      g_signal_connect (G_OBJECT (current_directory), "destroy",
                        G_CALLBACK (thunar_favourites_pane_file_destroy), pane);

      /* select the file in the view (if possible) */
      thunar_favourites_view_select_by_file (THUNAR_FAVOURITES_VIEW (pane->view),
                                             pane->current_directory);
    }

  g_object_notify (G_OBJECT (pane), "current-directory");
}



static void
thunar_favourites_pane_file_destroy (ThunarFile           *file,
                                     ThunarFavouritesPane *pane)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FAVOURITES_PANE (pane));
  g_return_if_fail (pane->current_directory == file);

  /* we don't fire a notification on "current-directory" here,
   * as the surrounding instance will notice the destroy as
   * well and perform appropriate actions.
   */
  g_signal_handlers_disconnect_by_func (G_OBJECT (pane->current_directory),
                                        G_CALLBACK (thunar_favourites_pane_file_destroy),
                                        pane);
  g_object_unref (G_OBJECT (pane->current_directory));
  pane->current_directory = NULL;
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



