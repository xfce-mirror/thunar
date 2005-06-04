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

#include <thunar/thunar-favourites-model.h>
#include <thunar/thunar-favourites-view.h>



static void     thunar_favourites_view_class_init     (ThunarFavouritesViewClass *klass);
static void     thunar_favourites_view_init           (ThunarFavouritesView      *view);
#if GTK_CHECK_VERSION(2,6,0)
static gboolean thunar_favourites_view_separator_func (GtkTreeModel              *model,
                                                       GtkTreeIter               *iter,
                                                       gpointer                   user_data);
#endif



struct _ThunarFavouritesViewClass
{
  GtkTreeViewClass __parent__;
};

struct _ThunarFavouritesView
{
  GtkTreeView __parent__;
};



G_DEFINE_TYPE (ThunarFavouritesView, thunar_favourites_view, GTK_TYPE_TREE_VIEW);



static void
thunar_favourites_view_class_init (ThunarFavouritesViewClass *klass)
{
}



static void
thunar_favourites_view_init (ThunarFavouritesView *view)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", THUNAR_FAVOURITES_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_FAVOURITES_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

#if GTK_CHECK_VERSION(2,6,0)
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), thunar_favourites_view_separator_func, NULL, NULL);
#endif
}



#if GTK_CHECK_VERSION(2,6,0)
static gboolean
thunar_favourites_view_separator_func (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gpointer      user_data)
{
  gboolean separator;
  gtk_tree_model_get (model, iter, THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR, &separator, -1);
  return separator;
}
#endif



/**
 * thunar_favourites_view_new:
 *
 * Allocates a new #ThunarFavouritesView instance and associates
 * it with the default #ThunarFavouritesModel instance.
 *
 * Return value: the newly allocated #ThunarFavouritesView instance.
 **/
GtkWidget*
thunar_favourites_view_new (void)
{
  ThunarFavouritesModel *model;
  GtkWidget             *view;

  model = thunar_favourites_model_get_default ();
  view = g_object_new (THUNAR_TYPE_FAVOURITES_VIEW,
                       "model", model, NULL);
  g_object_unref (G_OBJECT (model));

  return view;
}


