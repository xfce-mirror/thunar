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

#include <thunar/thunar-details-view.h>



static void   thunar_details_view_class_init          (ThunarDetailsViewClass *klass);
static void   thunar_details_view_view_init           (ThunarViewIface        *iface);
static void   thunar_details_view_init                (ThunarDetailsView      *details_view);
static void   thunar_details_view_row_activated       (GtkTreeView            *tree_view,
                                                       GtkTreePath            *path,
                                                       GtkTreeViewColumn      *column);
static GList *thunar_details_view_get_selected_items  (ThunarView             *view);
static void   thunar_details_view_selection_changed   (GtkTreeSelection       *selection,
                                                       ThunarView             *view);



struct _ThunarDetailsViewClass
{
  GtkTreeViewClass __parent__;
};

struct _ThunarDetailsView
{
  GtkTreeView __parent__;
};



G_DEFINE_TYPE_WITH_CODE (ThunarDetailsView,
                         thunar_details_view,
                         GTK_TYPE_TREE_VIEW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW,
                                                thunar_details_view_view_init));



static void
thunar_details_view_class_init (ThunarDetailsViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_details_view_row_activated;
}



static void
thunar_details_view_view_init (ThunarViewIface *iface)
{
  iface->get_model = (gpointer)gtk_tree_view_get_model;
  iface->set_model = (gpointer)gtk_tree_view_set_model;
  iface->get_selected_items = thunar_details_view_get_selected_items;
}



static void
thunar_details_view_init (ThunarDetailsView *details_view)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;

  /* configure general aspects of the details view */
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (details_view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (details_view), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (details_view), THUNAR_LIST_MODEL_COLUMN_NAME);

  /* first column (icon, name) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "expand", TRUE,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Name"),
                         NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", THUNAR_LIST_MODEL_COLUMN_ICON_SMALL,
                                       NULL);
  renderer = g_object_new (EXO_TYPE_CELL_RENDERER_ELLIPSIZED_TEXT,
                           "ellipsize", EXO_PANGO_ELLIPSIZE_END,
                           "ellipsize-set", TRUE,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (details_view), column);

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
  gtk_tree_view_append_column (GTK_TREE_VIEW (details_view), column);

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
  gtk_tree_view_append_column (GTK_TREE_VIEW (details_view), column);

  /* fourth column (type) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Type"),
                         NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 0.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_TYPE,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_TYPE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (details_view), column);

  /* fifth column (modification date) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", TRUE,
                         "resizable", TRUE,
                         "title", _("Date modified"),
                         NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 0.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (details_view), column);

  /* configure the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (details_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (thunar_details_view_selection_changed), details_view);
}



static void
thunar_details_view_row_activated (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ThunarFile   *file;

  /* tell the controlling component that the user activated a file */
  model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_get_iter (model, &iter, path);
  file = thunar_list_model_get_file (THUNAR_LIST_MODEL (model), &iter);
  thunar_view_file_activated (THUNAR_VIEW (tree_view), file);

  /* invoke the row activated method on the parent class */
  if (GTK_TREE_VIEW_CLASS (thunar_details_view_parent_class)->row_activated != NULL)
    GTK_TREE_VIEW_CLASS (thunar_details_view_parent_class)->row_activated (tree_view, path, column);
}



static GList*
thunar_details_view_get_selected_items (ThunarView *view)
{
  GtkTreeSelection *selection;

  g_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (view), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  return gtk_tree_selection_get_selected_rows (selection, NULL);
}



static void
thunar_details_view_selection_changed (GtkTreeSelection *selection,
                                       ThunarView       *view)
{
  g_return_if_fail (THUNAR_IS_DETAILS_VIEW (view));

  /* tell everybody that we have a new selection of files */
  thunar_view_file_selection_changed (view);
}



/**
 * thunar_details_view_new:
 *
 * Allocates a new #ThunarDetailsView instance, which is not
 * associated with any #ThunarListModel yet.
 *
 * Return value: the newly allocated #ThunarDetailsView instance.
 **/
GtkWidget*
thunar_details_view_new (void)
{
  return g_object_new (THUNAR_TYPE_DETAILS_VIEW, NULL);
}



