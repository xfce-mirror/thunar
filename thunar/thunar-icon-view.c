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

#include <thunar/thunar-icon-view.h>



static void       thunar_icon_view_class_init         (ThunarIconViewClass *klass);
static void       thunar_icon_view_init               (ThunarIconView      *icon_view);
static AtkObject *thunar_icon_view_get_accessible     (GtkWidget           *widget);
static GList     *thunar_icon_view_get_selected_items (ThunarStandardView  *standard_view);
static void       thunar_icon_view_item_activated     (ExoIconView         *view,
                                                       GtkTreePath         *path,
                                                       ThunarIconView      *icon_view);



struct _ThunarIconViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarIconView
{
  ThunarStandardView __parent__;
};



G_DEFINE_TYPE (ThunarIconView, thunar_icon_view, THUNAR_TYPE_STANDARD_VIEW);



static void
thunar_icon_view_class_init (ThunarIconViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_icon_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->get_selected_items = thunar_icon_view_get_selected_items;
}



static void
thunar_icon_view_init (ThunarIconView *icon_view)
{
  GtkWidget *view;

  /* create the real view */
  view = exo_icon_view_new ();
  g_signal_connect (G_OBJECT (view), "item-activated",
                    G_CALLBACK (thunar_icon_view_item_activated), icon_view);
  g_signal_connect_swapped (G_OBJECT (view), "selection-changed",
                            G_CALLBACK (thunar_standard_view_selection_changed), icon_view);
  gtk_container_add (GTK_CONTAINER (icon_view), view);
  gtk_widget_show (view);

  /* initialize the icon view properties */
  exo_icon_view_set_text_column (EXO_ICON_VIEW (view), THUNAR_LIST_MODEL_COLUMN_NAME);
  exo_icon_view_set_pixbuf_column (EXO_ICON_VIEW (view), THUNAR_LIST_MODEL_COLUMN_ICON_NORMAL);
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (view), GTK_SELECTION_MULTIPLE);
}



static AtkObject*
thunar_icon_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the icon view class */
  object = GTK_WIDGET_CLASS (thunar_icon_view_parent_class)->get_accessible (widget);

  /* set custom Atk properties for the icon view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_name (object, _("Icon view"));
      atk_object_set_description (object, _("Icon based directory listing"));
    }

  return object;
}



static GList*
thunar_icon_view_get_selected_items (ThunarStandardView *standard_view)
{
  return exo_icon_view_get_selected_items (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_icon_view_item_activated (ExoIconView    *view,
                                 GtkTreePath    *path,
                                 ThunarIconView *icon_view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ThunarFile   *file;

  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));

  /* tell the controlling component, that the user activated a file */
  model = exo_icon_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  file = thunar_list_model_get_file (THUNAR_LIST_MODEL (model), &iter);
  if (thunar_file_is_directory (file))
    thunar_navigator_change_directory (THUNAR_NAVIGATOR (icon_view), file);
  g_object_unref (G_OBJECT (file));
}



/**
 * thunar_icon_view_new:
 *
 * Allocates a new #ThunarIconView instance, which is not
 * associated with any #ThunarListModel yet.
 *
 * Return value: the newly allocated #ThunarIconView instance.
 **/
GtkWidget*
thunar_icon_view_new (void)
{
  return g_object_new (THUNAR_TYPE_ICON_VIEW, NULL);
}


