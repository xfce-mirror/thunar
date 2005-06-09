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



/* Identifiers for signals */
enum
{
  FAVOURITE_ACTIVATED,
  LAST_SIGNAL,
};

/* Identifiers for DnD target types */
enum
{
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST,
};



static void         thunar_favourites_view_class_init             (ThunarFavouritesViewClass *klass);
static void         thunar_favourites_view_init                   (ThunarFavouritesView      *view);
static void         thunar_favourites_view_row_activated          (GtkTreeView               *tree_view,
                                                                   GtkTreePath               *path,
                                                                   GtkTreeViewColumn         *column);
static void         thunar_favourites_view_drag_data_received     (GtkWidget                 *widget,
                                                                   GdkDragContext            *context,
                                                                   gint                       x,
                                                                   gint                       y,
                                                                   GtkSelectionData          *selection_data,
                                                                   guint                      info,
                                                                   guint                      time);
static gboolean     thunar_favourites_view_drag_drop              (GtkWidget                 *widget,
                                                                   GdkDragContext            *context,
                                                                   gint                       x,
                                                                   gint                       y,
                                                                   guint                      time);
static gboolean     thunar_favourites_view_drag_motion            (GtkWidget                 *widget,
                                                                   GdkDragContext            *context,
                                                                   gint                       x,
                                                                   gint                       y,
                                                                   guint                      time);
static GtkTreePath *thunar_favourites_view_compute_drop_position  (ThunarFavouritesView      *view,
                                                                   gint                       x,
                                                                   gint                       y);
#if GTK_CHECK_VERSION(2,6,0)
static gboolean     thunar_favourites_view_separator_func         (GtkTreeModel              *model,
                                                                   GtkTreeIter               *iter,
                                                                   gpointer                   user_data);
#endif



struct _ThunarFavouritesViewClass
{
  GtkTreeViewClass __parent__;

  /* signals */
  void (*favourite_activated) (ThunarFavouritesView *view,
                               ThunarFile           *file);
};

struct _ThunarFavouritesView
{
  GtkTreeView __parent__;
};



static guint view_signals[LAST_SIGNAL];

/* Target types for dragging from the favourites view */
static const GtkTargetEntry drag_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
};

/* Target types for dropping into the favourites view */
static const GtkTargetEntry drop_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
  { "text/uri-list", 0, TEXT_URI_LIST },
};



G_DEFINE_TYPE (ThunarFavouritesView, thunar_favourites_view, GTK_TYPE_TREE_VIEW);



static void
thunar_favourites_view_class_init (ThunarFavouritesViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->drag_data_received = thunar_favourites_view_drag_data_received;
  gtkwidget_class->drag_drop = thunar_favourites_view_drag_drop;
  gtkwidget_class->drag_motion = thunar_favourites_view_drag_motion;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_favourites_view_row_activated;

  /**
   * ThunarFavouritesView:favourite-activated:
   *
   * Invoked whenever a favourite is activated by the user.
   **/
  view_signals[FAVOURITE_ACTIVATED] =
    g_signal_new ("favourite-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFavouritesViewClass, favourite_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_TYPE_FILE);
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

  /* enable drag support for the favourites view (actually used to support reordering) */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (view), GDK_BUTTON1_MASK, drag_targets,
                                          G_N_ELEMENTS (drag_targets), GDK_ACTION_MOVE);

  /* enable drop support for the favourites view (both internal reordering
   * and adding new favourites from other widgets)
   */
  gtk_drag_dest_set (GTK_WIDGET (view), GTK_DEST_DEFAULT_ALL,
                     drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

#if GTK_CHECK_VERSION(2,6,0)
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), thunar_favourites_view_separator_func, NULL, NULL);
#endif
}



static void
thunar_favourites_view_row_activated (GtkTreeView       *tree_view,
                                      GtkTreePath       *path,
                                      GtkTreeViewColumn *column)
{
  ThunarFavouritesView *view = THUNAR_FAVOURITES_VIEW (tree_view);
  GtkTreeModel         *model;
  GtkTreeIter           iter;
  ThunarFile           *file;

  g_return_if_fail (THUNAR_IS_FAVOURITES_VIEW (view));
  g_return_if_fail (path != NULL);

  /* determine the iter for the path */
  model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_get_iter (model, &iter, path);

  /* determine the file for the favourite and invoke the signal */
  file = thunar_favourites_model_file_for_iter (THUNAR_FAVOURITES_MODEL (model), &iter);
  g_signal_emit (G_OBJECT (view), view_signals[FAVOURITE_ACTIVATED], 0, file);

  /* call the row-activated method in the parent class */
  if (GTK_TREE_VIEW_CLASS (thunar_favourites_view_parent_class)->row_activated != NULL)
    GTK_TREE_VIEW_CLASS (thunar_favourites_view_parent_class)->row_activated (tree_view, path, column);
}



static void
thunar_favourites_view_drag_data_received (GtkWidget        *widget,
                                           GdkDragContext   *context,
                                           gint              x,
                                           gint              y,
                                           GtkSelectionData *selection_data,
                                           guint             info,
                                           guint             time)
{
  ThunarFavouritesView *view = THUNAR_FAVOURITES_VIEW (widget);
  GtkTreeSelection     *selection;
  GtkTreeModel         *model;
  GtkTreePath          *dst_path;
  GtkTreePath          *src_path;
  GtkTreeIter           iter;

  g_return_if_fail (THUNAR_IS_FAVOURITES_VIEW (view));

  /* compute the drop position */
  dst_path = thunar_favourites_view_compute_drop_position (view, x, y);

  if (selection_data->target == gdk_atom_intern ("text/uri-list", FALSE))
    {
      g_error ("text/uri-list not handled yet");
    }
  else if (selection_data->target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* we need to adjust the destination path here, because the path returned by
           * the drop position computation effectively points after the insert position,
           * which can led to unexpected results.
           */
          gtk_tree_path_prev (dst_path);
          if (!thunar_favourites_model_drop_possible (THUNAR_FAVOURITES_MODEL (model), dst_path))
            gtk_tree_path_next (dst_path);

          /* perform the move */
          src_path = gtk_tree_model_get_path (model, &iter);
          thunar_favourites_model_move (THUNAR_FAVOURITES_MODEL (model), src_path, dst_path);
          gtk_tree_path_free (src_path);
        }
    }

  gtk_tree_path_free (dst_path);
}



static gboolean
thunar_favourites_view_drag_drop (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  gint            x,
                                  gint            y,
                                  guint           time)
{
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_VIEW (widget), FALSE);
  return TRUE;
}



static gboolean
thunar_favourites_view_drag_motion (GtkWidget      *widget,
                                    GdkDragContext *context,
                                    gint            x,
                                    gint            y,
                                    guint           time)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarFavouritesView   *view = THUNAR_FAVOURITES_VIEW (widget);
  GdkDragAction           action;
  GtkTreeModel           *model;
  GtkTreePath            *path;

  /* check the action that should be performed */
  if (context->suggested_action == GDK_ACTION_COPY || (context->actions & GDK_ACTION_COPY) != 0)
    action = GDK_ACTION_COPY;
  else if (context->suggested_action == GDK_ACTION_MOVE || (context->actions & GDK_ACTION_MOVE) != 0)
    action = GDK_ACTION_MOVE;
  else
    return FALSE;

  /* compute the drop position for the coordinates */
  path = thunar_favourites_view_compute_drop_position (view, x, y);

  /* check if path is about to append to the model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (gtk_tree_path_get_indices (path)[0] >= gtk_tree_model_iter_n_children (model, NULL))
    {
      /* set the position to "after" and move the path to
       * point to the previous row instead; required to
       * get the highlighting in GtkTreeView correct.
       */
      position = GTK_TREE_VIEW_DROP_AFTER;
      gtk_tree_path_prev (path);
    }

  /* highlight the appropriate row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), path, position);
  gtk_tree_path_free (path);

  gdk_drag_status (context, action, time);
  return TRUE;
}



static GtkTreePath*
thunar_favourites_view_compute_drop_position (ThunarFavouritesView *view,
                                              gint                  x,
                                              gint                  y)
{
  GtkTreeViewColumn *column;
  GtkTreeModel      *model;
  GdkRectangle       area;
  GtkTreePath       *path;
  gint               n_rows;

  g_return_val_if_fail (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) != NULL, NULL);
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_VIEW (view), NULL);

  /* query the number of rows in the model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  n_rows = gtk_tree_model_iter_n_children (model, NULL);

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y,
                                     &path, &column, &x, &y))
    {
      /* determine the exact path of the row the user is trying to drop 
       * (taking into account the relative y position)
       */
      gtk_tree_view_get_background_area (GTK_TREE_VIEW (view), path, column, &area);
      if (y >= area.height / 2)
        gtk_tree_path_next (path);

      /* find a suitable drop path (we cannot drop into the default favourites list) */
      for (; gtk_tree_path_get_indices (path)[0] < n_rows; gtk_tree_path_next (path))
        if (thunar_favourites_model_drop_possible (THUNAR_FAVOURITES_MODEL (model), path))
          return path;
    }
  else
    {
      /* we'll append to the favourites list */
      path = gtk_tree_path_new_from_indices (n_rows, -1);
    }

  return path;
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



/**
 * thunar_favourites_view_select_by_file:
 * @view : a #ThunarFavouritesView instance.
 * @file : a #ThunarFile instance.
 *
 * Looks up the first favourite that refers to @file in @view and selects it.
 * If @file is not present in the underlying #ThunarFavouritesModel, no
 * favourite will be selected afterwards.
 **/
void
thunar_favourites_view_select_by_file (ThunarFavouritesView *view,
                                       ThunarFile           *file)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  g_return_if_fail (THUNAR_IS_FAVOURITES_VIEW (view));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* clear the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_unselect_all (selection);

  /* try to lookup a tree iter for the given file */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (thunar_favourites_model_iter_for_file (THUNAR_FAVOURITES_MODEL (model), file, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


