/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
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

#include <thunar/thunar-abstract-icon-view.h>
#include <thunar/thunar-gallery-view.h>
#include <thunar/thunar-private.h>


static AtkObject   *thunar_gallery_view_get_accessible         (GtkWidget           *widget);
static void        *thunar_gallery_view_icon_hovered           (ExoIconView         *view,
                                                                GtkTreePath         *path,
                                                                ThunarGalleryView   *gallery_view);



struct _ThunarGalleryViewClass
{
  ThunarAbstractIconViewClass __parent__;
};

struct _ThunarGalleryView
{
  ThunarAbstractIconView __parent__;
};



G_DEFINE_TYPE (ThunarGalleryView, thunar_gallery_view, THUNAR_TYPE_ABSTRACT_ICON_VIEW)



static void
thunar_gallery_view_class_init (ThunarGalleryViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_gallery_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->zoom_level_property_name = "last-gallery-view-zoom-level";

}



static void
thunar_gallery_view_init (ThunarGalleryView *gallery_view)
{
  GtkWidget   *view;

  /* remove all cell renderers in the layout, since by default that includes not only the
   * icon renderer, but also the name renderer */
  view = gtk_bin_get_child (GTK_BIN (gallery_view));
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (view));

  /* add the icon renderer, but not the name renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (gallery_view)->icon_renderer), "follow-state", TRUE, "rounded-corners", TRUE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (gallery_view)->icon_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (gallery_view)->icon_renderer,
                                 "file", THUNAR_COLUMN_FILE);

  /* setup the icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (gallery_view)->icon_renderer),
                "ypad", 1u,
                "square-icons", TRUE,
                NULL);

  /* set up the icon-hovered signal and its corresponding callback */
  g_signal_connect (G_OBJECT (view), "icon-hovered", G_CALLBACK (thunar_gallery_view_icon_hovered), gallery_view);
}



static AtkObject*
thunar_gallery_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the icon view class */
  object = (*GTK_WIDGET_CLASS (thunar_gallery_view_parent_class)->get_accessible) (widget);

  /* set custom Atk properties for the icon view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_description (object, _("Gallery based directory listing"));
      atk_object_set_name (object, _("Gallery view"));
      atk_object_set_role (object, ATK_ROLE_DIRECTORY_PANE);
    }

  return object;
}

static void
*thunar_gallery_view_icon_hovered (ExoIconView        *view,
                                   GtkTreePath        *path,
                                   ThunarGalleryView  *gallery_view)
{
#ifndef NDEBUG
  g_print ("path: %s", gtk_tree_path_to_string (path));
#endif
}
