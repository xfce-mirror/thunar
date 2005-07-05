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
 *
 * Inspired by the shortcuts list as found in the GtkFileChooser, which was
 * developed for Gtk+ by Red Hat, Inc.
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
static gboolean     thunar_favourites_view_button_press_event     (GtkWidget                 *widget,
                                                                   GdkEventButton            *event);
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
static void         thunar_favourites_view_row_activated          (GtkTreeView               *tree_view,
                                                                   GtkTreePath               *path,
                                                                   GtkTreeViewColumn         *column);
static GtkTreePath *thunar_favourites_view_compute_drop_position  (ThunarFavouritesView      *view,
                                                                   gint                       x,
                                                                   gint                       y);
static void         thunar_favourites_view_drop_uri_list          (ThunarFavouritesView      *view,
                                                                   const gchar               *uri_list,
                                                                   GtkTreePath               *dst_path);
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
  gtkwidget_class->button_press_event = thunar_favourites_view_button_press_event;
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



static gboolean
thunar_favourites_view_button_press_event (GtkWidget      *widget,
                                           GdkEventButton *event)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkWindow    *window;
  GtkWidget    *menu;
  GtkWidget    *item;
  GMainLoop    *loop;
  gboolean      result;
  GList        *actions;
  GList        *lp;

  /* let the widget process the event first (handles focussing and scrolling) */
  result = GTK_WIDGET_CLASS (thunar_favourites_view_parent_class)->button_press_event (widget, event);

  /* check if we have a right-click event */
  if (G_LIKELY (event->button != 3))
    return result;

  /* figure out the toplevel window */
  window = (GtkWindow *) gtk_widget_get_toplevel (widget);
  if (G_UNLIKELY (window == NULL))
    return result;

  /* resolve the path to the cursor position */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* query the list of actions for the given path */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
      actions = thunar_favourites_model_get_actions (THUNAR_FAVOURITES_MODEL (model), path, window);
      gtk_tree_path_free (path);

      /* check if we have actions for the given path */
      if (G_LIKELY (actions != NULL))
        {
          /* prepare the internal loop */
          loop = g_main_loop_new (NULL, FALSE);

          /* prepare the popup menu */
          menu = gtk_menu_new ();
          gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (widget));
          g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);

          /* add the action items to the menu */
          for (lp = actions; lp != NULL; lp = lp->next)
            {
              item = gtk_action_create_menu_item (GTK_ACTION (lp->data));
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              g_object_unref (G_OBJECT (lp->data));
              gtk_widget_show (item);
            }
          g_list_free (actions);

          /* run the internal loop */
          gtk_grab_add (menu);
          gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, event->time);
          g_main_loop_run (loop);
          gtk_grab_remove (menu);

          /* clean up */
          gtk_object_sink (GTK_OBJECT (menu));
          g_main_loop_unref (loop);

          /* we effectively handled the event */
          return TRUE;
        }
    }

  return result;
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
      thunar_favourites_view_drop_uri_list (view, selection_data->data, dst_path);
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



static void
thunar_favourites_view_drop_uri_list (ThunarFavouritesView *view,
                                      const gchar          *uri_list,
                                      GtkTreePath          *dst_path)
{
  GtkTreeModel *model;
  ThunarFile   *file;
  GtkWidget    *toplevel;
  GtkWidget    *dialog;
  GError       *error = NULL;
  gchar        *uri_string;
  GList        *uris;
  GList        *lp;

  uris = thunar_vfs_uri_list_from_string (uri_list, &error);
  if (G_LIKELY (error == NULL))
    {
      /* process the URIs one-by-one and stop on error */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      for (lp = uris; lp != NULL; lp = lp->next)
        {
          file = thunar_file_get_for_uri (lp->data, &error);
          if (G_UNLIKELY (file == NULL))
            break;

          /* make sure, that only directories gets added to the favourites list */
          if (G_UNLIKELY (!thunar_file_is_directory (file)))
            {
              uri_string = thunar_vfs_uri_to_string (lp->data, 0);
              g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR,
                           "The URI '%s' does not refer to a directory",
                           uri_string);
              g_object_unref (G_OBJECT (file));
              g_free (uri_string);
              break;
            }

          thunar_favourites_model_add (THUNAR_FAVOURITES_MODEL (model), dst_path, file);
          g_object_unref (G_OBJECT (file));
          gtk_tree_path_next (dst_path);
        }

      thunar_vfs_uri_list_free (uris);
    }

  if (G_UNLIKELY (error != NULL))
    {
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (toplevel),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                                   | GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "<span weight=\"bold\" size="
                                                   "\"larger\">%s</span>\n\n%s",
                                                   _("Could not add favourite"),
                                                   error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
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


