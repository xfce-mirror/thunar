/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

#ifdef HAVE_LIBNOTIFY
#include <thunar/thunar-notify.h>
#endif
#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-icon-renderer.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-shortcuts-view.h>



/* Identifiers for signals */
enum
{
  SHORTCUT_ACTIVATED,
  LAST_SIGNAL,
};

/* Identifiers for DnD target types */
enum
{
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST,
};



static void           thunar_shortcuts_view_finalize                     (GObject                  *object);
static gboolean       thunar_shortcuts_view_button_press_event           (GtkWidget                *widget,
                                                                          GdkEventButton           *event);
static gboolean       thunar_shortcuts_view_button_release_event         (GtkWidget                *widget,
                                                                          GdkEventButton           *event);
static void           thunar_shortcuts_view_drag_begin                   (GtkWidget                *widget,
                                                                          GdkDragContext           *context);
static void           thunar_shortcuts_view_drag_data_received           (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          GtkSelectionData         *selection_data,
                                                                          guint                     info,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_drag_drop                    (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_drag_motion                  (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          guint                     timestamp);
static void           thunar_shortcuts_view_drag_leave                   (GtkWidget                *widget,
                                                                          GdkDragContext           *context,
                                                                          guint                     timestamp);
static gboolean       thunar_shortcuts_view_popup_menu                   (GtkWidget                *widget);
static void           thunar_shortcuts_view_row_activated                (GtkTreeView              *tree_view,
                                                                          GtkTreePath              *path,
                                                                          GtkTreeViewColumn        *column);
static void           thunar_shortcuts_view_context_menu                 (ThunarShortcutsView      *view,
                                                                          GdkEventButton           *event,
                                                                          GtkTreeModel             *model,
                                                                          GtkTreeIter              *iter);
static void           thunar_shortcuts_view_remove_activated             (GtkWidget                *item,
                                                                          ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_rename_activated             (GtkWidget                *item,
                                                                          ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_renamed                      (GtkCellRenderer          *renderer,
                                                                          const gchar              *path_string,
                                                                          const gchar              *text,
                                                                          ThunarShortcutsView      *view);
static GdkDragAction  thunar_shortcuts_view_compute_drop_actions         (ThunarShortcutsView      *view,
                                                                          GdkDragContext           *context,
                                                                          gint                      x,
                                                                          gint                      y,
                                                                          GtkTreePath             **path_return,
                                                                          GdkDragAction            *action_return,
                                                                          GtkTreeViewDropPosition  *position_return);
static GtkTreePath   *thunar_shortcuts_view_compute_drop_position        (ThunarShortcutsView      *view,
                                                                          gint                      x,
                                                                          gint                      y);
static void           thunar_shortcuts_view_drop_uri_list                (ThunarShortcutsView      *view,
                                                                          GList                    *path_list,
                                                                          GtkTreePath              *dst_path);
static void           thunar_shortcuts_view_open_clicked                 (ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_open                         (ThunarShortcutsView      *view,
                                                                          gboolean                  new_window);
static void           thunar_shortcuts_view_open_in_new_window_clicked   (ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_empty_trash                  (ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_eject                        (ThunarShortcutsView      *view);
static void           thunar_shortcuts_view_mount                        (ThunarShortcutsView      *view);
static gboolean       thunar_shortcuts_view_separator_func               (GtkTreeModel             *model,
                                                                          GtkTreeIter              *iter,
                                                                          gpointer                  user_data);



struct _ThunarShortcutsViewClass
{
  GtkTreeViewClass __parent__;
};

struct _ThunarShortcutsView
{
  GtkTreeView        __parent__;
  ThunarPreferences *preferences;
  GtkCellRenderer   *icon_renderer;
  
  ThunarxProviderFactory *provider_factory;

  /* the currently pressed mouse button, set in the
   * button-press-event handler if the associated
   * button-release-event should activate.
   */
  gint pressed_button;
  guint pressed_eject_button : 1;

  /* drop site support */
  guint  drop_data_ready : 1; /* whether the drop data was received already */
  guint  drop_occurred : 1;
  GList *drop_file_list;      /* the list of URIs that are contained in the drop data */

  /* id of the signal used to queue a resize on the
   * column whenever the shortcuts icon size is changed.
   */
  gint queue_resize_signal_id;
};



/* Target types for dragging from the shortcuts view */
static const GtkTargetEntry drag_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
};

/* Target types for dropping into the shortcuts view */
static const GtkTargetEntry drop_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
  { "text/uri-list", 0, TEXT_URI_LIST },
};



static guint view_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsView, thunar_shortcuts_view, GTK_TYPE_TREE_VIEW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))



static void
thunar_shortcuts_view_class_init (ThunarShortcutsViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_view_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->button_press_event = thunar_shortcuts_view_button_press_event;
  gtkwidget_class->button_release_event = thunar_shortcuts_view_button_release_event;
  gtkwidget_class->drag_begin = thunar_shortcuts_view_drag_begin;
  gtkwidget_class->drag_data_received = thunar_shortcuts_view_drag_data_received;
  gtkwidget_class->drag_drop = thunar_shortcuts_view_drag_drop;
  gtkwidget_class->drag_motion = thunar_shortcuts_view_drag_motion;
  gtkwidget_class->drag_leave = thunar_shortcuts_view_drag_leave;
  gtkwidget_class->popup_menu = thunar_shortcuts_view_popup_menu;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_shortcuts_view_row_activated;

  /**
   * ThunarShortcutsView:shortcut-activated:
   *
   * Invoked whenever a shortcut is activated by the user.
   **/
  view_signals[SHORTCUT_ACTIVATED] =
    g_signal_new (I_("shortcut-activated"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_TYPE_FILE);
}



static void
thunar_shortcuts_view_init (ThunarShortcutsView *view)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;

  /* configure the tree view */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
  
  /* grab a reference on the provider factory */
  view->provider_factory = thunarx_provider_factory_get_default ();

  /* grab a reference on the preferences; be sure to redraw the view
   * whenever the "shortcuts-icon-emblems" preference changes.
   */
  view->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::shortcuts-icon-emblems", G_CALLBACK (gtk_widget_queue_draw), view);

  /* allocate a single column for our renderers */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         "sizing", GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                         "spacing", 2,
                         NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  /* queue a resize on the column whenever the icon size is changed */
  view->queue_resize_signal_id = g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::shortcuts-icon-size",
                                                           G_CALLBACK (gtk_tree_view_column_queue_resize), column);

  /* allocate the special icon renderer */
  view->icon_renderer = thunar_shortcuts_icon_renderer_new ();
  gtk_tree_view_column_pack_start (column, view->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, view->icon_renderer,
                                       "file", THUNAR_SHORTCUTS_MODEL_COLUMN_FILE,
                                       "volume", THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME,
                                       NULL);

  /* sync the "emblems" property of the icon renderer with the "shortcuts-icon-emblems" preference
   * and the "size" property of the renderer with the "shortcuts-icon-size" preference.
   */
  exo_binding_new (G_OBJECT (view->preferences), "shortcuts-icon-size", G_OBJECT (view->icon_renderer), "size");
  exo_binding_new (G_OBJECT (view->preferences), "shortcuts-icon-emblems", G_OBJECT (view->icon_renderer), "emblems");

  /* allocate the text renderer (ellipsizing as required, but "File System" must fit) */
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "ellipsize-set", TRUE,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           "width-chars", g_utf8_strlen (_("File System"), -1),
                           NULL);
  g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (thunar_shortcuts_view_renamed), view);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
                                       NULL);

  /* allocate icon renderer for the eject symbol */
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "icon-name", THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT,
                                       NULL);


  /* enable drag support for the shortcuts view (actually used to support reordering) */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (view), GDK_BUTTON1_MASK, drag_targets,
                                          G_N_ELEMENTS (drag_targets), GDK_ACTION_MOVE);

  /* enable drop support for the shortcuts view (both internal reordering
   * and adding new shortcuts from other widgets)
   */
  gtk_drag_dest_set (GTK_WIDGET (view), 0, drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

  /* setup a row separator function to tell GtkTreeView about the separator */
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), thunar_shortcuts_view_separator_func, NULL, NULL);
}



static void
thunar_shortcuts_view_finalize (GObject *object)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (object);

  /* release drop path list (if drag_leave wasn't called) */
  thunar_g_file_list_free (view->drop_file_list);

  /* release the provider factory */
  g_object_unref (G_OBJECT (view->provider_factory));

  /* disconnect the queue resize signal handler */
  g_signal_handler_disconnect (G_OBJECT (view->preferences), view->queue_resize_signal_id);

  /* disconnect from the preferences object */
  g_signal_handlers_disconnect_matched (G_OBJECT (view->preferences), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
  g_object_unref (G_OBJECT (view->preferences));

  (*G_OBJECT_CLASS (thunar_shortcuts_view_parent_class)->finalize) (object);
}



static gboolean
thunar_shortcuts_view_button_press_event (GtkWidget      *widget,
                                          GdkEventButton *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeModel        *model;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  gboolean             result;
  gchar               *eject_icon;

  /* reset the pressed button state */
  view->pressed_button = -1;
  view->pressed_eject_button = 0;

  /* completely ignore double click events */
  if (event->type == GDK_2BUTTON_PRESS)
    return TRUE;

  /* let the widget process the event first (handles focussing and scrolling) */
  result = (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_press_event) (widget, event);

  /* resolve the path at the cursor position */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* check if we should popup the context menu */
      if (G_UNLIKELY (event->button == 3 && event->type == GDK_BUTTON_PRESS))
        {
          /* determine the iterator for the path */
          model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
          if (gtk_tree_model_get_iter (model, &iter, path))
            {
              /* popup the context menu */
              thunar_shortcuts_view_context_menu (view, event, model, &iter);

              /* we effectively handled the event */
              result = TRUE;
            }
        }
      else if ((event->button == 1 || event->button == 2) && event->type == GDK_BUTTON_PRESS
            && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
          /* check if we clicked the eject button area */
          gint icon_width, icon_height, column_width;
          column_width = gtk_tree_view_column_get_width (gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0));
          gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height);
          if (event->x >= column_width - icon_width - 3)
            {
              /* check if that shortcut actually has an eject button */
              model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
              if (gtk_tree_model_get_iter (model, &iter, path))
                {
                  gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT, &eject_icon, -1);
                  if (strlen (eject_icon) > 0)
                    view->pressed_eject_button = 1;
                  g_free (eject_icon);
                }
            }

          /*
          g_debug("thunar_shortcuts_view_button_press_event(): x: %f, y: %f; my width: %i, eject width: %i, eject: %i", 
                  event->x, event->y, column_width, icon_width, view->pressed_eject_button);
          */

          /* remember the button as pressed and handle it in the release handler */
          view->pressed_button = event->button;


        }

      /* release the path */
      gtk_tree_path_free (path);
    }

  return result;
}



static gboolean
thunar_shortcuts_view_button_release_event (GtkWidget      *widget,
                                            GdkEventButton *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* check if we have an event matching the pressed button state */
  if (G_LIKELY (view->pressed_button == (gint) event->button))
    {
      if (view->pressed_eject_button)
        thunar_shortcuts_view_eject (view);

      /* check if we should simply open or open in new window */
      if (G_LIKELY (event->button == 1))
        thunar_shortcuts_view_open (view, FALSE);
      else if (G_UNLIKELY (event->button == 2))
        thunar_shortcuts_view_open (view, TRUE);
    }

  /* reset the pressed button state */
  view->pressed_button = -1;
  view->pressed_eject_button = 0;

  /* call the parent's release event handler */
  return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_release_event) (widget, event);
}



static void
thunar_shortcuts_view_drag_begin (GtkWidget      *widget,
                                  GdkDragContext *context)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* reset the pressed button state */
  view->pressed_button = -1;

  /* call the parent's drag_begin() handler */
  if (GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_begin != NULL)
    (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_begin) (widget, context);
}



static void
thunar_shortcuts_view_drag_data_received (GtkWidget        *widget,
                                          GdkDragContext   *context,
                                          gint              x,
                                          gint              y,
                                          GtkSelectionData *selection_data,
                                          guint             info,
                                          guint             timestamp)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarShortcutsView    *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkDragAction           actions;
  GdkDragAction           action;
  GtkTreeModel           *model;
  GtkTreePath            *path;
  GtkTreeIter             iter;
  ThunarFile             *file;
  gboolean                succeed = FALSE;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* check if we don't already know the drop data */
  if (G_LIKELY (!view->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
        view->drop_file_list = thunar_g_file_list_new_from_string ((const gchar *) selection_data->data);

      /* reset the state */
      view->drop_data_ready = TRUE;
    }

  /* check if the data was droppped */
  if (G_UNLIKELY (view->drop_occurred))
    {
      /* reset the state */
      view->drop_occurred = FALSE;

      /* verify that we only handle text/uri-list here */
      if (G_LIKELY (info == TEXT_URI_LIST))
        {
          /* determine the drop actions */
          actions = thunar_shortcuts_view_compute_drop_actions (view, context, x, y, &path, &action, &position);
          if (G_LIKELY (actions != 0))
            {
              /* check if we should add a shortcut */
              if (position == GTK_TREE_VIEW_DROP_BEFORE || position == GTK_TREE_VIEW_DROP_AFTER)
                {
                  /* if position is "after", we need to advance the path,
                   * as the drop_uri_list() will insert "at" the path.
                   */
                  if (position == GTK_TREE_VIEW_DROP_AFTER)
                    gtk_tree_path_next (path);

                  /* just add the required shortcuts then */
                  thunar_shortcuts_view_drop_uri_list (view, view->drop_file_list, path);
                }
              else if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
                {
                  /* get the shortcuts model */
                  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

                  /* determine the iterator for the path */
                  if (gtk_tree_model_get_iter (model, &iter, path))
                    {
                      /* determine the file for the iter */
                      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
                      if (G_LIKELY (file != NULL))
                        {
                          /* ask the user what to do with the drop data */
                          if (G_UNLIKELY (action == GDK_ACTION_ASK))
                            action = thunar_dnd_ask (widget, file, view->drop_file_list, timestamp, actions);

                          /* perform the requested action */
                          if (G_LIKELY (action != 0))
                            {
                              /* really perform the drop :-) */
                              succeed = thunar_dnd_perform (widget, file, view->drop_file_list, action, NULL);
                            }

                          /* release the file */
                          g_object_unref (G_OBJECT (file));
                        }
                    }
                }

              /* release the tree path */
              gtk_tree_path_free (path);
            }
        }

      /* disable the drop highlighting */
      thunar_shortcuts_view_drag_leave (widget, context, timestamp);

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, timestamp);
    }
  else
    {
      gdk_drag_status (context, 0, timestamp);
    }
}



static gboolean
thunar_shortcuts_view_drag_drop (GtkWidget      *widget,
                                 GdkDragContext *context,
                                 gint            x,
                                 gint            y,
                                 guint           timestamp)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreePath         *dst_path;
  GtkTreePath         *src_path;
  GtkTreeIter          iter;
  GdkAtom              target;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), FALSE);

  /* determine the drop target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (G_LIKELY (target == gdk_atom_intern_static_string ("text/uri-list")))
    {
      /* set state so the drag-data-received handler
       * knows that this is really a drop this time.
       */
      view->drop_occurred = TRUE;

      /* request the drag data from the source. */
      gtk_drag_get_data (widget, context, target, timestamp);
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      /* compute the drop position */
      dst_path = thunar_shortcuts_view_compute_drop_position (view, x, y);

      /* determine the source path */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* we need to adjust the destination path here, because the path returned by
           * the drop position computation effectively points after the insert position,
           * which can led to unexpected results.
           */
          gtk_tree_path_prev (dst_path);
          if (!thunar_shortcuts_model_drop_possible (THUNAR_SHORTCUTS_MODEL (model), dst_path))
            gtk_tree_path_next (dst_path);

          /* perform the move */
          src_path = gtk_tree_model_get_path (model, &iter);
          thunar_shortcuts_model_move (THUNAR_SHORTCUTS_MODEL (model), src_path, dst_path);
          gtk_tree_path_free (src_path);
        }

      /* release the dst path */
      gtk_tree_path_free (dst_path);

      /* finish the dnd operation */
      gtk_drag_finish (context, TRUE, FALSE, timestamp);
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_shortcuts_view_drag_motion (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           timestamp)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarShortcutsView    *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkDragAction           action = 0;
  GtkTreeModel           *model;
  GtkTreePath            *path = NULL;
  GdkAtom                 target;

  /* reset the "drop-file" of the icon renderer */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

  /* determine the drag target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* request the drop data on-demand (if we don't have it already) */
      if (G_UNLIKELY (!view->drop_data_ready))
        {
          /* request the drag data from the source */
          gtk_drag_get_data (widget, context, target, timestamp);

          /* gdk_drag_status() will be called by drag_data_received */
          return TRUE;
        }
      else
        {
          /* compute the drop position */
          thunar_shortcuts_view_compute_drop_actions (view, context, x, y, &path, &action, &position);
        }
    }
  else if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    {
      /* check the action that should be performed */
      if (context->suggested_action == GDK_ACTION_LINK || (context->actions & GDK_ACTION_LINK) != 0)
        action = GDK_ACTION_LINK;
      else if (context->suggested_action == GDK_ACTION_COPY || (context->actions & GDK_ACTION_COPY) != 0)
        action = GDK_ACTION_COPY;
      else if (context->suggested_action == GDK_ACTION_MOVE || (context->actions & GDK_ACTION_MOVE) != 0)
        action = GDK_ACTION_MOVE;
      else
        return FALSE;

      /* compute the drop position for the coordinates */
      path = thunar_shortcuts_view_compute_drop_position (view, x, y);

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
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  /* check if we have a drop path */
  if (G_LIKELY (path != NULL))
    {
      /* highlight the appropriate row */
      gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), path, position);
      gtk_tree_path_free (path);
    }

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, timestamp);

  return TRUE;
}



static void
thunar_shortcuts_view_drag_leave (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  guint           timestamp)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* reset the "drop-file" of the icon renderer */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (view->drop_data_ready))
    {
      thunar_g_file_list_free (view->drop_file_list);
      view->drop_data_ready = FALSE;
      view->drop_file_list = NULL;
    }

  /* schedule a repaint to make sure the special drop icon for the target row
   * is reset to its default (http://bugzilla.xfce.org/show_bug.cgi?id=2498).
   */
  gtk_widget_queue_draw (widget);

  /* call the parent's handler */
  (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_leave) (widget, context, timestamp);
}



static gboolean
thunar_shortcuts_view_popup_menu (GtkWidget *widget)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreeIter          iter;

  /* determine the selection for the tree view */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* determine the selected row */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* popup the context menu */
      thunar_shortcuts_view_context_menu (view, NULL, model, &iter);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->popup_menu != NULL)
    {
      /* call the parent's "popup-menu" handler */
      return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->popup_menu) (widget);
    }

  return FALSE;
}



static void
thunar_shortcuts_view_row_activated (GtkTreeView       *tree_view,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (tree_view);

  /* call the parent's "row-activated" handler */
  if (GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated != NULL)
    (*GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated) (tree_view, path, column);

  /* open the selected shortcut */
  thunar_shortcuts_view_open (view, FALSE);
}



static void
thunar_shortcuts_view_context_menu (ThunarShortcutsView *view,
                                    GdkEventButton      *event,
                                    GtkTreeModel        *model,
                                    GtkTreeIter         *iter)
{
  GtkTreePath *path;
  ThunarFile  *file;
  GtkWidget   *image;
  GtkWidget   *menu;
  GtkWidget   *item;
  GtkWidget   *window;
  gboolean     mutable;
  GVolume     *volume;
  GList       *providers, *lp;
  GList       *actions = NULL, *tmp;

  /* determine the tree path for the given iter */
  path = gtk_tree_model_get_path (model, iter);
  if (G_UNLIKELY (path == NULL))
    return;

  /* check whether the shortcut at the given path is mutable */
  gtk_tree_model_get (model, iter,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume,
                      THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE, &mutable, 
                      -1);

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* append the "Open" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_open_clicked), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* set the stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* append the "Open in New Window" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("Open in New Window"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_open_in_new_window_clicked), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* append a menu separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* check if we have a volume here */
  if (G_UNLIKELY (volume != NULL))
    {
      /* append the "Mount Volume" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Mount Volume"));
      gtk_widget_set_sensitive (item, !thunar_g_volume_is_mounted (volume));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_mount), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* check if the volume is present and can be ejected */
      if (thunar_g_volume_is_present (volume) && thunar_g_volume_is_removable (volume))
        {
          /* append the "Eject Volume" menu action */
          item = gtk_image_menu_item_new_with_mnemonic (_("E_ject Volume"));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_eject), view);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* append a menu separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }
  else if (G_UNLIKELY (file != NULL && thunar_file_is_trashed (file) && thunar_file_is_root (file)))
    {
      /* append the "Empty Trash" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Empty Trash"));
      gtk_widget_set_sensitive (item, (thunar_file_get_item_count (file) > 0));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_empty_trash), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* append a menu separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* create provider menu items if there is a non-trashed file */
  if (G_LIKELY (file != NULL && !thunar_file_is_trashed (file)))
    {
      /* load the menu providers from the provider factory */
      providers = thunarx_provider_factory_list_providers (view->provider_factory, THUNARX_TYPE_MENU_PROVIDER);
      if (G_LIKELY (providers != NULL))
        {
          /* determine the toplevel window we belong to */
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* load the actions offered by the menu providers */
          for (lp = providers; lp != NULL; lp = lp->next)
            {
              tmp = thunarx_menu_provider_get_folder_actions (lp->data, window, THUNARX_FILE_INFO (file));
              actions = g_list_concat (actions, tmp);
              g_object_unref (G_OBJECT (lp->data));
            }
          g_list_free (providers);

          /* add the actions to the menu */
          for (lp = actions; lp != NULL; lp = lp->next)
            {
              item = gtk_action_create_menu_item (GTK_ACTION (lp->data));
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);

              /* release the reference on the action */
              g_object_unref (G_OBJECT (lp->data));
            }

          /* add a separator to the end of the menu */
          if (G_LIKELY (lp != actions))
            {
              /* append a menu separator */
              item = gtk_separator_menu_item_new ();
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }

          /* cleanup */
          g_list_free (actions);
        }
    }

  /* append the remove menu item */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Remove Shortcut"));
  g_object_set_data_full (G_OBJECT (item), I_("thunar-shortcuts-row"),
                          gtk_tree_row_reference_new (model, path),
                          (GDestroyNotify) gtk_tree_row_reference_free);
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_remove_activated), view);
  gtk_widget_set_sensitive (item, mutable);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* set the remove stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* append the rename menu item */
  item = gtk_image_menu_item_new_with_mnemonic (_("Re_name Shortcut"));
  g_object_set_data_full (G_OBJECT (item), I_("thunar-shortcuts-row"),
                          gtk_tree_row_reference_new (model, path),
                          (GDestroyNotify) gtk_tree_row_reference_free);
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_rename_activated), view);
  gtk_widget_set_sensitive (item, mutable);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* run the menu on the view's screen (taking over the floating reference on menu) */
  thunar_gtk_menu_run (GTK_MENU (menu), GTK_WIDGET (view), NULL, NULL, (event != NULL) ? event->button : 0,
                       (event != NULL) ? event->time : gtk_get_current_event_time ());

  /* clean up */
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_UNLIKELY (volume != NULL))
    g_object_unref (G_OBJECT (volume));
  gtk_tree_path_free (path);
}



static void
thunar_shortcuts_view_remove_activated (GtkWidget           *item,
                                        ThunarShortcutsView *view)
{
  GtkTreeRowReference *row;
  GtkTreeModel        *model;
  GtkTreePath         *path;

  row = g_object_get_data (G_OBJECT (item), I_("thunar-shortcuts-row"));
  path = gtk_tree_row_reference_get_path (row);
  if (G_LIKELY (path != NULL))
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      thunar_shortcuts_model_remove (THUNAR_SHORTCUTS_MODEL (model), path);
      gtk_tree_path_free (path);
    }
}



static void
thunar_shortcuts_view_rename_activated (GtkWidget           *item,
                                        ThunarShortcutsView *view)
{
  GtkTreeRowReference *row;
  GtkTreeViewColumn   *column;
  GtkCellRenderer     *renderer;
  GtkTreePath         *path;
  GList               *renderers;

  row = g_object_get_data (G_OBJECT (item), I_("thunar-shortcuts-row"));
  path = gtk_tree_row_reference_get_path (row);
  if (G_LIKELY (path != NULL))
    {
      /* determine the text renderer */
      column = gtk_tree_view_get_column (GTK_TREE_VIEW (view), 0);
      renderers = gtk_cell_layout_get_cells  (GTK_CELL_LAYOUT (column));
      renderer = g_list_nth_data (renderers, 1);

      /* make sure the text renderer is editable */
      g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

      /* tell the tree view to start editing the given row */
      gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (view), path, column, renderer, TRUE);

      /* cleanup */
      gtk_tree_path_free (path);
      g_list_free (renderers);
    }
}



static void
thunar_shortcuts_view_renamed (GtkCellRenderer     *renderer,
                               const gchar         *path_string,
                               const gchar         *text,
                               ThunarShortcutsView *view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;

  /* reset the editable flag */
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

  /* perform the rename */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (gtk_tree_model_get_iter_from_string (model, &iter, path_string))
    thunar_shortcuts_model_rename (THUNAR_SHORTCUTS_MODEL (model), &iter, text);
}



static GdkDragAction
thunar_shortcuts_view_compute_drop_actions (ThunarShortcutsView     *view,
                                            GdkDragContext          *context,
                                            gint                     x,
                                            gint                     y,
                                            GtkTreePath            **path_return,
                                            GdkDragAction           *action_return,
                                            GtkTreeViewDropPosition *position_return)
{
  GtkTreeViewColumn *column;
  GdkDragAction      actions = 0;
  GdkRectangle       cell_area;
  GtkTreeModel      *model;
  GtkTreePath       *path;
  GtkTreeIter        iter;
  ThunarFile        *file;
  gint               cell_y;

  /* determine the shortcuts model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  /* determine the path for x/y */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y, &path, &column, NULL, &cell_y))
    {
      /* determine the background area of the column */
      gtk_tree_view_get_background_area (GTK_TREE_VIEW (view), path, column, &cell_area);

      /* check if 1/6th inside the cell and the path is within the model */
      if ((cell_y > (cell_area.height / 6) && cell_y < (cell_area.height - cell_area.height / 6))
          && gtk_tree_model_get_iter (model, &iter, path))
        {
          /* determine the file for the iterator */
          gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
          if (G_LIKELY (file != NULL))
            {
              /* check if the file accepts the drop */
              actions = thunar_file_accepts_drop (file, view->drop_file_list, context, action_return);
              if (G_LIKELY (actions != 0))
                {
                  /* we can drop into this location */
                  *position_return = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
                  *path_return = gtk_tree_path_copy (path);

                  /* display a drop icon for the file */
                  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", file, NULL);
                }

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }
        }

      /* release the path */
      gtk_tree_path_free (path);
    }

  /* check if we cannot drop into the shortcut */
  if (G_UNLIKELY (actions == 0))
    {
      /* check the action that should be performed */
      if (context->suggested_action == GDK_ACTION_LINK || (context->actions & GDK_ACTION_LINK) != 0)
        actions = GDK_ACTION_LINK;
      else if (context->suggested_action == GDK_ACTION_COPY || (context->actions & GDK_ACTION_COPY) != 0)
        actions = GDK_ACTION_COPY;
      else if (context->suggested_action == GDK_ACTION_MOVE || (context->actions & GDK_ACTION_MOVE) != 0)
        actions = GDK_ACTION_MOVE;
      else
        return 0;

      /* but maybe we can add as shortcut */
      path = thunar_shortcuts_view_compute_drop_position (view, x, y);

      /* check if path is about to append to the model */
      if (gtk_tree_path_get_indices (path)[0] >= gtk_tree_model_iter_n_children (model, NULL))
        {
          /* set the position to "after" and move the path to
           * point to the previous row instead; required to
           * get the highlighting in GtkTreeView correct.
           */
          *position_return = GTK_TREE_VIEW_DROP_AFTER;
          gtk_tree_path_prev (path);
        }
      else
        {
          /* drop before the path */
          *position_return = GTK_TREE_VIEW_DROP_BEFORE;
        }

      /* got it */
      *action_return = actions;
      *path_return = path;
    }

  return actions;
}



static GtkTreePath*
thunar_shortcuts_view_compute_drop_position (ThunarShortcutsView *view,
                                             gint                 x,
                                             gint                 y)
{
  GtkTreeViewColumn *column;
  GtkTreeModel      *model;
  GdkRectangle       area;
  GtkTreePath       *path;
  gint               n_rows;

  _thunar_return_val_if_fail (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) != NULL, NULL);
  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), NULL);

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

      /* find a suitable drop path (we cannot drop into the default shortcuts list) */
      for (; gtk_tree_path_get_indices (path)[0] < n_rows; gtk_tree_path_next (path))
        if (thunar_shortcuts_model_drop_possible (THUNAR_SHORTCUTS_MODEL (model), path))
          return path;
    }
  else
    {
      /* we'll append to the shortcuts list */
      path = gtk_tree_path_new_from_indices (n_rows, -1);
    }

  return path;
}



static void
thunar_shortcuts_view_drop_uri_list (ThunarShortcutsView *view,
                                     GList               *path_list,
                                     GtkTreePath         *dst_path)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  ThunarFile   *file;
  GError       *error = NULL;
  GList        *lp;

  /* take a copy of the destination path */
  path = gtk_tree_path_copy (dst_path);

  /* process the URIs one-by-one and stop on error */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  for (lp = path_list; lp != NULL; lp = lp->next)
    {
      file = thunar_file_get (lp->data, &error);
      if (G_UNLIKELY (file == NULL))
        break;

      /* make sure, that only directories gets added to the shortcuts list */
      if (G_UNLIKELY (!thunar_file_is_directory (file)))
        {
          g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR,
                       _("The path \"%s\" does not refer to a directory"),
                       thunar_file_get_display_name (file));
          g_object_unref (file);
          break;
        }

      thunar_shortcuts_model_add (THUNAR_SHORTCUTS_MODEL (model), path, file);
      g_object_unref (file);
      gtk_tree_path_next (path);
    }

  /* release the tree path copy */
  gtk_tree_path_free (path);

  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (GTK_WIDGET (view), error, _("Failed to add new shortcut"));

      /* release the error */
      g_error_free (error);
    }
}



static void
thunar_shortcuts_view_open_clicked (ThunarShortcutsView *view)
{
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));
  thunar_shortcuts_view_open (view, FALSE);
}



static void
thunar_shortcuts_view_poke_file_finish (ThunarBrowser *browser,
                                        ThunarFile    *file,
                                        ThunarFile    *target_file,
                                        GError        *error,
                                        gpointer       user_data)
{
  ThunarApplication *application;
  gboolean           new_window = GPOINTER_TO_UINT (user_data);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (error == NULL)
    {
      if (new_window)
        {
          /* open a new window for the target folder */
          application = thunar_application_get ();
          thunar_application_open_window (application, target_file, 
                                          gtk_widget_get_screen (GTK_WIDGET (browser)), NULL);
          g_object_unref (application);
        }
      else
        {
          /* invoke the signal to change to that folder */
          g_signal_emit (browser, view_signals[SHORTCUT_ACTIVATED], 0, target_file);
        }
    }
  else
    {
      thunar_dialogs_show_error (GTK_WIDGET (browser), error, _("Failed to open \"%s\""),
                                 thunar_file_get_display_name (file));
    }
}



static void
thunar_shortcuts_view_poke_volume_finish (ThunarBrowser *browser,
                                          GVolume       *volume,
                                          ThunarFile    *mount_point,
                                          GError        *error,
                                          gpointer       user_data)
{
  gboolean new_window = GPOINTER_TO_UINT (user_data);
  gchar   *volume_name;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (browser));
  _thunar_return_if_fail (G_IS_VOLUME (volume));

  if (error == NULL)
    {
      thunar_browser_poke_file (browser, mount_point, GTK_WIDGET (browser),
                                thunar_shortcuts_view_poke_file_finish,
                                GUINT_TO_POINTER (new_window));
    }
  else
    {
      volume_name = g_volume_get_name (volume);
      thunar_dialogs_show_error (GTK_WIDGET (browser), error, 
                                 _("Failed to mount \"%s\""), volume_name);
      g_free (volume_name);
    }
}



static void
thunar_shortcuts_view_open (ThunarShortcutsView *view,
                            gboolean             new_window)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file;
  GVolume          *volume;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* avoid dealing with invalid selections */
  if (!GTK_IS_TREE_SELECTION (selection))
    return;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the file for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, 
                          THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, 
                          -1);

      if (G_LIKELY (volume != NULL))
        {
          thunar_browser_poke_volume (THUNAR_BROWSER (view), volume, view,
                                      thunar_shortcuts_view_poke_volume_finish,
                                      GUINT_TO_POINTER (new_window));
        }
      else if (file != NULL) 
        {
          thunar_browser_poke_file (THUNAR_BROWSER (view), file, view,
                                    thunar_shortcuts_view_poke_file_finish,
                                    GUINT_TO_POINTER (new_window));
        }

      if (file != NULL)
        g_object_unref (file);

      if (volume != NULL)
        g_object_unref (volume);
    }
}



static void
thunar_shortcuts_view_open_in_new_window_clicked (ThunarShortcutsView *view)
{
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));
  thunar_shortcuts_view_open (view, TRUE);
}



static void
thunar_shortcuts_view_empty_trash (ThunarShortcutsView *view)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* empty the trash bin (asking the user first) */
  application = thunar_application_get ();
  thunar_application_empty_trash (application, GTK_WIDGET (view), NULL);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_shortcuts_view_eject_finish (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (user_data);
  GtkWidget           *window;
  GVolume             *volume = G_VOLUME (object);
  GError              *error = NULL;
  gchar               *volume_name;

  _thunar_return_if_fail (G_IS_VOLUME (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* check if there was an error */
  if (!g_volume_eject_finish (volume, result, &error))
    {
      /* ignore GIO errors already handled */
      if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* display an error dialog to inform the user */
          volume_name = g_volume_get_name (volume);
          thunar_dialogs_show_error (window, error, _("Failed to eject \"%s\""), volume_name);
          g_free (volume_name);

          g_error_free (error);
        }
    }

#ifdef HAVE_LIBNOTIFY
  thunar_notify_eject_finish (volume);
#endif

  g_object_unref (view);
}



static void
thunar_shortcuts_view_unmount_finish (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (user_data);
  GtkWidget           *window;
  GMount              *mount = G_MOUNT (object);
  GError              *error = NULL;
  gchar               *mount_name;

  _thunar_return_if_fail (G_IS_MOUNT (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* check if there was an error */
  if (!g_mount_unmount_finish (mount, result, &error))
    {
      /* ignore GIO errors already handled */
      if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* display an error dialog to inform the user */
          mount_name = g_mount_get_name (mount);
          thunar_dialogs_show_error (window, error, _("Failed to eject \"%s\""), mount_name);
          g_free (mount_name);

          g_error_free (error);
        }
    }

#ifdef HAVE_LIBNOTIFY
  thunar_notify_unmount_finish (mount);
#endif

  g_object_unref (view);
}



static void
thunar_shortcuts_view_eject (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GVolume          *volume;
  GMount           *mount;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the volume for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, -1);
      if (G_UNLIKELY (volume != NULL))
        {
          /* determine what the appropriate method is: eject or unmount */
          if (g_volume_can_eject (volume))
            {
#ifdef HAVE_LIBNOTIFY
              thunar_notify_eject (volume);
#endif

              /* try to to eject the volume asynchronously */
              g_volume_eject (volume, G_MOUNT_UNMOUNT_NONE, NULL, 
                              thunar_shortcuts_view_eject_finish, 
                              g_object_ref (view));
            }
          else
            {
              /* determine the mount of the volume */
              mount = g_volume_get_mount (volume);
              if (G_LIKELY (mount != NULL))
                {
#ifdef HAVE_LIBNOTIFY
                  thunar_notify_unmount (mount);
#endif

                  /* the volume is mounted, try to unmount the mount */
                  g_mount_unmount (mount, G_MOUNT_UNMOUNT_NONE, NULL,
                                   thunar_shortcuts_view_unmount_finish, 
                                   g_object_ref (view));

                  /* release the mount */
                  g_object_unref (mount);
                }
            }

          /* cleanup */
          g_object_unref (volume);
        }
    }
}



static void
thunar_shortcuts_view_poke_volume_mount_finish (ThunarBrowser *browser,
                                                GVolume       *volume,
                                                ThunarFile    *mount_point,
                                                GError        *error,
                                                gpointer       ignored)
{
  gchar *volume_name;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (browser));
  _thunar_return_if_fail (G_IS_VOLUME (volume));

  if (error != NULL)
    {
      volume_name = g_volume_get_name (volume);
      thunar_dialogs_show_error (GTK_WIDGET (browser), error, 
                                 _("Failed to mount \"%s\""), volume_name);
      g_free (volume_name);
    }
}




static void
thunar_shortcuts_view_mount (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GVolume          *volume;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* avoid dealing with invalid selections */
  if (!GTK_IS_TREE_SELECTION (selection))
    return;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the file for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, 
                          THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, 
                          -1);

      if (G_LIKELY (volume != NULL))
        {
          thunar_browser_poke_volume (THUNAR_BROWSER (view), volume, view,
                                      thunar_shortcuts_view_poke_volume_mount_finish,
                                      NULL);
          g_object_unref (volume);
        }
    }
}



static gboolean
thunar_shortcuts_view_separator_func (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
  gboolean separator;
  gtk_tree_model_get (model, iter, THUNAR_SHORTCUTS_MODEL_COLUMN_SEPARATOR, &separator, -1);
  return separator;
}



/**
 * thunar_shortcuts_view_new:
 *
 * Allocates a new #ThunarShortcutsView instance and associates
 * it with the default #ThunarShortcutsModel instance.
 *
 * Return value: the newly allocated #ThunarShortcutsView instance.
 **/
GtkWidget*
thunar_shortcuts_view_new (void)
{
  ThunarShortcutsModel *model;
  GtkWidget             *view;

  model = thunar_shortcuts_model_get_default ();
  view = g_object_new (THUNAR_TYPE_SHORTCUTS_VIEW, "model", model, NULL);
  g_object_unref (G_OBJECT (model));

  return view;
}



/**
 * thunar_shortcuts_view_select_by_file:
 * @view : a #ThunarShortcutsView instance.
 * @file : a #ThunarFile instance.
 *
 * Looks up the first shortcut that refers to @file in @view and selects it.
 * If @file is not present in the underlying #ThunarShortcutsModel, no
 * shortcut will be selected afterwards.
 **/
void
thunar_shortcuts_view_select_by_file (ThunarShortcutsView *view,
                                      ThunarFile          *file)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* clear the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_unselect_all (selection);

  /* try to lookup a tree iter for the given file */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (thunar_shortcuts_model_iter_for_file (THUNAR_SHORTCUTS_MODEL (model), file, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


