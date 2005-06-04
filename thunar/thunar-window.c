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

#include <thunar/thunar-favourites-view.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-window.h>



static void thunar_window_class_init  (ThunarWindowClass *klass);
static void thunar_window_init        (ThunarWindow      *window);



struct _ThunarWindowClass
{
  GtkWindowClass __parent__;
};

struct _ThunarWindow
{
  GtkWindow __parent__;

  GtkWidget *view;
};



static GObjectClass *parent_class;



G_DEFINE_TYPE (ThunarWindow, thunar_window, GTK_TYPE_WINDOW);



static void
thunar_window_class_init (ThunarWindowClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);
}



static void
thunar_window_init (ThunarWindow *window)
{
  GtkWidget *paned;
  GtkWidget *side;
  GtkWidget *swin;

  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), _("Thunar"));
  
  paned = g_object_new (GTK_TYPE_HPANED, "border-width", 6, NULL);
  gtk_container_add (GTK_CONTAINER (window), paned);
  gtk_widget_show (paned);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_paned_pack1 (GTK_PANED (paned), swin, FALSE, FALSE);
  gtk_widget_show (swin);

  side = thunar_favourites_view_new ();
  gtk_container_add (GTK_CONTAINER (swin), side);
  gtk_widget_show (side);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_paned_pack2 (GTK_PANED (paned), swin, TRUE, FALSE);
  gtk_widget_show (swin);

#if 0
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;

  window->view = gtk_tree_view_new ();
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (window->view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (window->view), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (window->view),
                                   THUNAR_LIST_MODEL_COLUMN_NAME);
  gtk_container_add (GTK_CONTAINER (swin), window->view);
  gtk_widget_show (window->view);

  /* first column (icon, name) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Name"),
                         NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", THUNAR_LIST_MODEL_COLUMN_ICON_SMALL,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->view), column);

  /* second column (size) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Size"),
                         NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 1.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_SIZE,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_SIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->view), column);

  /* third column (permissions) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Permissions"),
                         NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 0.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_PERMISSIONS,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_PERMISSIONS);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->view), column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
#else
  window->view = exo_icon_view_new ();
  exo_icon_view_set_text_column (EXO_ICON_VIEW (window->view), THUNAR_LIST_MODEL_COLUMN_NAME);
  exo_icon_view_set_pixbuf_column (EXO_ICON_VIEW (window->view), THUNAR_LIST_MODEL_COLUMN_ICON_NORMAL);
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (window->view), GTK_SELECTION_MULTIPLE);
  gtk_container_add (GTK_CONTAINER (swin), window->view);
  gtk_widget_show (window->view);
#endif
}



/**
 * thunar_window_new_with_folder:
 * @folder : a #ThunarFolder instance.
 *
 * Return value: the newly allocated #ThunarWindow instance.
 **/
GtkWidget*
thunar_window_new_with_folder (ThunarFolder *folder)
{
  ThunarListModel *model;
  GtkWidget       *window;

  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);

  window = g_object_new (THUNAR_TYPE_WINDOW, NULL);

  model = thunar_list_model_new_with_folder (folder);
  g_object_set (G_OBJECT (THUNAR_WINDOW (window)->view),
                "model", model, NULL);
  g_object_unref (G_OBJECT (model));

  return window;
}

