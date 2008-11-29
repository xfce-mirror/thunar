/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-shortcuts-icon-renderer.h>
#include <thunar/thunar-tree-model.h>
#include <thunar/thunar-tree-view.h>



/* the timeout (in ms) until the drag dest row will be expanded */
#define THUNAR_TREE_VIEW_EXPAND_TIMEOUT (750)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SHOW_HIDDEN,
};

/* Signal identifiers */
enum
{
  DELETE_SELECTED_FILES,
  LAST_SIGNAL,
};

/* Identifiers for DnD target types */
enum
{
  TARGET_TEXT_URI_LIST,
};



static void             thunar_tree_view_class_init                 (ThunarTreeViewClass  *klass);
static void             thunar_tree_view_navigator_init             (ThunarNavigatorIface *iface);
static void             thunar_tree_view_init                       (ThunarTreeView       *view);
static void             thunar_tree_view_finalize                   (GObject              *object);
static void             thunar_tree_view_get_property               (GObject              *object,
                                                                     guint                 prop_id,
                                                                     GValue               *value,
                                                                     GParamSpec           *pspec);
static void             thunar_tree_view_set_property               (GObject              *object,
                                                                     guint                 prop_id,
                                                                     const GValue         *value,
                                                                     GParamSpec           *pspec);
static ThunarFile      *thunar_tree_view_get_current_directory      (ThunarNavigator      *navigator);
static void             thunar_tree_view_set_current_directory      (ThunarNavigator      *navigator,
                                                                     ThunarFile           *current_directory);
static void             thunar_tree_view_realize                    (GtkWidget            *widget);
static void             thunar_tree_view_unrealize                  (GtkWidget            *widget);
static gboolean         thunar_tree_view_button_press_event         (GtkWidget            *widget,
                                                                     GdkEventButton       *event);
static gboolean         thunar_tree_view_button_release_event       (GtkWidget            *widget,
                                                                     GdkEventButton       *event);
static void             thunar_tree_view_drag_data_received         (GtkWidget            *widget,
                                                                     GdkDragContext       *context,
                                                                     gint                  x,
                                                                     gint                  y,
                                                                     GtkSelectionData     *selection_data,
                                                                     guint                 info,
                                                                     guint                 time);
static gboolean         thunar_tree_view_drag_drop                  (GtkWidget            *widget,
                                                                     GdkDragContext       *context,
                                                                     gint                  x,
                                                                     gint                  y,
                                                                     guint                 time);
static gboolean         thunar_tree_view_drag_motion                (GtkWidget            *widget,
                                                                     GdkDragContext       *context,
                                                                     gint                  x,
                                                                     gint                  y,
                                                                     guint                 time);
static void             thunar_tree_view_drag_leave                 (GtkWidget            *widget,
                                                                     GdkDragContext       *context,
                                                                     guint                 time);
static gboolean         thunar_tree_view_popup_menu                 (GtkWidget            *widget);
static void             thunar_tree_view_row_activated              (GtkTreeView          *tree_view,
                                                                     GtkTreePath          *path,
                                                                     GtkTreeViewColumn    *column);
static gboolean         thunar_tree_view_test_expand_row            (GtkTreeView          *tree_view,
                                                                     GtkTreeIter          *iter,
                                                                     GtkTreePath          *path);
static void             thunar_tree_view_row_collapsed              (GtkTreeView          *tree_view,
                                                                     GtkTreeIter          *iter,
                                                                     GtkTreePath          *path);
static gboolean         thunar_tree_view_delete_selected_files      (ThunarTreeView       *view);
static void             thunar_tree_view_context_menu               (ThunarTreeView       *view,
                                                                     GdkEventButton       *event,
                                                                     GtkTreeModel         *model,
                                                                     GtkTreeIter          *iter);
static GdkDragAction    thunar_tree_view_get_dest_actions           (ThunarTreeView       *view,
                                                                     GdkDragContext       *context,
                                                                     gint                  x,
                                                                     gint                  y,
                                                                     guint                 time,
                                                                     ThunarFile          **file_return);
static gboolean         thunar_tree_view_find_closest_ancestor      (ThunarTreeView       *view,
                                                                     GtkTreePath          *path,
                                                                     GtkTreePath         **ancestor_return,
                                                                     gboolean             *exact_return);
static ThunarFile      *thunar_tree_view_get_selected_file          (ThunarTreeView       *view);
static ThunarVfsVolume *thunar_tree_view_get_selected_volume        (ThunarTreeView       *view);
static void             thunar_tree_view_action_copy                (ThunarTreeView       *view);
static void             thunar_tree_view_action_create_folder       (ThunarTreeView       *view);
static void             thunar_tree_view_action_cut                 (ThunarTreeView       *view);
static void             thunar_tree_view_action_delete              (ThunarTreeView       *view);
static void             thunar_tree_view_action_eject               (ThunarTreeView       *view);
static void             thunar_tree_view_action_empty_trash         (ThunarTreeView       *view);
static gboolean         thunar_tree_view_action_mount               (ThunarTreeView       *view);
static void             thunar_tree_view_action_open                (ThunarTreeView       *view);
static void             thunar_tree_view_action_open_in_new_window  (ThunarTreeView       *view);
static void             thunar_tree_view_action_paste_into_folder   (ThunarTreeView       *view);
static void             thunar_tree_view_action_properties          (ThunarTreeView       *view);
static void             thunar_tree_view_action_unmount             (ThunarTreeView       *view);
static GClosure        *thunar_tree_view_new_files_closure          (ThunarTreeView       *view);
static void             thunar_tree_view_new_files                  (ThunarVfsJob         *job,
                                                                     GList                *path_list,
                                                                     ThunarTreeView       *view);
static gboolean         thunar_tree_view_visible_func               (ThunarTreeModel      *model,
                                                                     ThunarFile           *file,
                                                                     gpointer              user_data);
static gboolean         thunar_tree_view_selection_func             (GtkTreeSelection     *selection,
                                                                     GtkTreeModel         *model,
                                                                     GtkTreePath          *path,
                                                                     gboolean              path_currently_selected,
                                                                     gpointer              user_data);
static gboolean         thunar_tree_view_cursor_idle                (gpointer              user_data);
static void             thunar_tree_view_cursor_idle_destroy        (gpointer              user_data);
static gboolean         thunar_tree_view_drag_scroll_timer          (gpointer              user_data);
static void             thunar_tree_view_drag_scroll_timer_destroy  (gpointer              user_data);
static gboolean         thunar_tree_view_expand_timer               (gpointer              user_data);
static void             thunar_tree_view_expand_timer_destroy       (gpointer              user_data);



struct _ThunarTreeViewClass
{
  GtkTreeViewClass __parent__;

  /* signals */
  gboolean (*delete_selected_files) (ThunarTreeView *view);
};

struct _ThunarTreeView
{
  GtkTreeView              __parent__;
  ThunarClipboardManager *clipboard;
  ThunarPreferences      *preferences;
  GtkCellRenderer        *icon_renderer;
  ThunarFile             *current_directory;
  ThunarTreeModel        *model;

  /* whether to display hidden/backup files */
  guint                   show_hidden : 1;

  /* drop site support */
  guint                   drop_data_ready : 1; /* whether the drop data was received already */
  guint                   drop_occurred : 1;
  GList                  *drop_path_list;      /* the list of URIs that are contained in the drop data */

  /* the "new-files" closure, which is used to
   * open newly created directories once done.
   */
  GClosure               *new_files_closure;

  /* the currently pressed mouse button, set in the
   * button-press-event handler if the associated
   * button-release-event should activate.
   */
  gint                    pressed_button;

#if GTK_CHECK_VERSION(2,8,0)
  /* id of the signal used to queue a resize on the
   * column whenever the shortcuts icon size is changed.
   */
  gint                    queue_resize_signal_id;
#endif

  /* set cursor to current directory idle source */
  gint                    cursor_idle_id;

  /* autoscroll during drag timer source */
  gint                    drag_scroll_timer_id;

  /* expand drag dest row timer source */
  gint                    expand_timer_id;
};



/* Target types for dropping into the tree view */
static const GtkTargetEntry drop_targets[] = {
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST },
};



static GObjectClass *thunar_tree_view_parent_class;
static guint         tree_view_signals[LAST_SIGNAL];



GType
thunar_tree_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarTreeViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_tree_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarTreeView),
        0,
        (GInstanceInitFunc) thunar_tree_view_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_tree_view_navigator_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_TREE_VIEW, I_("ThunarTreeView"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
    }

  return type;
}



static void
thunar_tree_view_class_init (ThunarTreeViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;
  GtkBindingSet    *binding_set;
  GObjectClass     *gobject_class;

  /* determine the parent type class */
  thunar_tree_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tree_view_finalize;
  gobject_class->get_property = thunar_tree_view_get_property;
  gobject_class->set_property = thunar_tree_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_tree_view_realize;
  gtkwidget_class->unrealize = thunar_tree_view_unrealize;
  gtkwidget_class->button_press_event = thunar_tree_view_button_press_event;
  gtkwidget_class->button_release_event = thunar_tree_view_button_release_event;
  gtkwidget_class->drag_data_received = thunar_tree_view_drag_data_received;
  gtkwidget_class->drag_drop = thunar_tree_view_drag_drop;
  gtkwidget_class->drag_motion = thunar_tree_view_drag_motion;
  gtkwidget_class->drag_leave = thunar_tree_view_drag_leave;
  gtkwidget_class->popup_menu = thunar_tree_view_popup_menu;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_tree_view_row_activated;
  gtktree_view_class->test_expand_row = thunar_tree_view_test_expand_row;
  gtktree_view_class->row_collapsed = thunar_tree_view_row_collapsed;

  klass->delete_selected_files = thunar_tree_view_delete_selected_files;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /**
   * ThunarTreeView:show-hidden:
   *
   * Whether to display hidden and backup folders
   * in this #ThunarTreeView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HIDDEN,
                                   g_param_spec_boolean ("show-hidden",
                                                         "show-hidden",
                                                         "show-hidden",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarTreeView::delete-selected-files:
   * @tree_view : a #ThunarTreeView.
   *
   * Emitted whenever the user presses the Delete key. This
   * is an internal signal used to bind the action to keys.
   **/
  tree_view_signals[DELETE_SELECTED_FILES] =
    g_signal_new (I_("delete-selected-files"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarTreeViewClass, delete_selected_files),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /* setup the key bindings for the tree view */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_SHIFT_MASK, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, GDK_SHIFT_MASK, "delete-selected-files", 0);
}



static void
thunar_tree_view_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_tree_view_get_current_directory;
  iface->set_current_directory = thunar_tree_view_set_current_directory;
}



static void
thunar_tree_view_init (ThunarTreeView *view)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;

  /* initialize the view internals */
  view->cursor_idle_id = -1;
  view->drag_scroll_timer_id = -1;
  view->expand_timer_id = -1;

  /* grab a reference on the preferences; be sure to redraw the view
   * whenever the "tree-icon-emblems" preference changes.
   */
  view->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::tree-icon-emblems", G_CALLBACK (gtk_widget_queue_draw), view);

  /* connect to the default tree model */
  view->model = thunar_tree_model_get_default ();
  thunar_tree_model_set_visible_func (view->model, thunar_tree_view_visible_func, view);
  gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (view->model));

  /* configure the tree view */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

  /* allocate a single column for our renderers */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         "sizing", GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                         "spacing", 2,
                         NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  /* queue a resize on the column whenever the icon size is changed */
#if GTK_CHECK_VERSION(2,8,0)
  view->queue_resize_signal_id = g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::tree-icon-size",
                                                           G_CALLBACK (gtk_tree_view_column_queue_resize), column);
#endif

  /* allocate the special icon renderer */
  view->icon_renderer = thunar_shortcuts_icon_renderer_new ();
  gtk_tree_view_column_pack_start (column, view->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, view->icon_renderer,
                                       "file", THUNAR_TREE_MODEL_COLUMN_FILE,
                                       "volume", THUNAR_TREE_MODEL_COLUMN_VOLUME,
                                       NULL);

  /* sync the "emblems" property of the icon renderer with the "tree-icon-emblems" preference
   * and the "size" property of the renderer with the "tree-icon-size" preference.
   */
  exo_binding_new (G_OBJECT (view->preferences), "tree-icon-size", G_OBJECT (view->icon_renderer), "size");
  exo_binding_new (G_OBJECT (view->preferences), "tree-icon-emblems", G_OBJECT (view->icon_renderer), "emblems");

  /* allocate the text renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "attributes", THUNAR_TREE_MODEL_COLUMN_ATTR,
                                       "text", THUNAR_TREE_MODEL_COLUMN_NAME,
                                       NULL);

  /* setup the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function (selection, thunar_tree_view_selection_func, view, NULL);

  /* enable drop support for the tree view */
  gtk_drag_dest_set (GTK_WIDGET (view), 0, drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
}



static void
thunar_tree_view_finalize (GObject *object)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (object);

  /* release drop path list (if drag_leave wasn't called) */
  thunar_vfs_path_list_free (view->drop_path_list);

  /* disconnect the queue resize signal handler */
#if GTK_CHECK_VERSION(2,8,0)
  g_signal_handler_disconnect (G_OBJECT (view->preferences), view->queue_resize_signal_id);
#endif

  /* be sure to cancel the cursor idle source */
  if (G_UNLIKELY (view->cursor_idle_id >= 0))
    g_source_remove (view->cursor_idle_id);

  /* cancel any running autoscroll timer */
  if (G_LIKELY (view->drag_scroll_timer_id >= 0))
    g_source_remove (view->drag_scroll_timer_id);

  /* be sure to cancel the expand timer source */
  if (G_UNLIKELY (view->expand_timer_id >= 0))
    g_source_remove (view->expand_timer_id);

  /* reset the current-directory property */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (view), NULL);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (view->preferences));

  /* disconnect from the default tree model */
  g_object_unref (G_OBJECT (view->model));

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (view->new_files_closure != NULL))
    {
      g_closure_invalidate (view->new_files_closure);
      g_closure_unref (view->new_files_closure);
    }

  (*G_OBJECT_CLASS (thunar_tree_view_parent_class)->finalize) (object);
}



static void
thunar_tree_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_tree_view_get_show_hidden (THUNAR_TREE_VIEW (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tree_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_tree_view_set_show_hidden (THUNAR_TREE_VIEW (object), g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_tree_view_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_TREE_VIEW (navigator)->current_directory;
}



static void
thunar_tree_view_set_current_directory (ThunarNavigator *navigator,
                                        ThunarFile      *current_directory)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (navigator);
  ThunarFile     *file, *file_parent;

  /* check if we already use that directory */
  if (G_UNLIKELY (view->current_directory == current_directory))
    return;

  /* disconnect from the previous current directory */
  if (G_LIKELY (view->current_directory != NULL))
    g_object_unref (G_OBJECT (view->current_directory));

  /* activate the new current directory */
  view->current_directory = current_directory;

  /* connect to the new current directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the directory */
      g_object_ref (G_OBJECT (current_directory));

      /* update the filter if the new current directory, or one of it's parents, is hidden */
      if (!view->show_hidden)
        {
          /* look if the file or one of it's parents is hidden */
          for (file = g_object_ref (G_OBJECT (current_directory)); file != NULL; file = file_parent)
            {
              /* check if this file is hidden */
              if (thunar_file_is_hidden (file))
                {
                  /* update the filter */
                  thunar_tree_model_refilter (view->model);

                  /* release the file */
                  g_object_unref (G_OBJECT (file));

                  break;
                }

              /* get the file parent */
              file_parent = thunar_file_get_parent (file, NULL);

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }
        }

      /* schedule an idle source to set the cursor to the current directory */
      if (G_LIKELY (view->cursor_idle_id < 0))
        view->cursor_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_tree_view_cursor_idle, view, thunar_tree_view_cursor_idle_destroy);

      /* drop any existing "new-files" closure */
      if (G_UNLIKELY (view->new_files_closure != NULL))
        {
          g_closure_invalidate (view->new_files_closure);
          g_closure_unref (view->new_files_closure);
          view->new_files_closure = NULL;
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (view), "current-directory");
}



static void
thunar_tree_view_realize (GtkWidget *widget)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GdkDisplay     *display;

  /* let the parent class realize the widget */
  (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->realize) (widget);

  /* query the clipboard manager for the display */
  display = gtk_widget_get_display (widget);
  view->clipboard = thunar_clipboard_manager_get_for_display (display);
}



static void
thunar_tree_view_unrealize (GtkWidget *widget)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);

  /* release the clipboard manager reference */
  g_object_unref (G_OBJECT (view->clipboard));
  view->clipboard = NULL;

  /* let the parent class unrealize the widget */
  (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->unrealize) (widget);
}



static gboolean
thunar_tree_view_button_press_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GtkTreePath    *path;
  GtkTreeIter     iter;
  gboolean        result;

  /* reset the pressed button state */
  view->pressed_button = -1;

  /* completely ignore double middle clicks */
  if (event->type == GDK_2BUTTON_PRESS && event->button == 2)
    return TRUE;

  /* let the widget process the event first (handles focussing and scrolling) */
  result = (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->button_press_event) (widget, event);

  /* resolve the path at the cursor position */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* check if we should popup the context menu */
      if (G_UNLIKELY (event->button == 3 && event->type == GDK_BUTTON_PRESS))
        {
          /* determine the iterator for the path */
          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->model), &iter, path))
            {
              /* popup the context menu */
              thunar_tree_view_context_menu (view, event, GTK_TREE_MODEL (view->model), &iter);

              /* we effectively handled the event */
              result = TRUE;
            }
        }
      else if ((event->button == 1 || event->button == 2) && event->type == GDK_BUTTON_PRESS
            && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
          /* remember the button as pressed and handled it in the release handler */
          view->pressed_button = event->button;
        }

      /* release the path */
      gtk_tree_path_free (path);
    }

  return result;
}



static gboolean
thunar_tree_view_button_release_event (GtkWidget      *widget,
                                       GdkEventButton *event)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);

  /* check if we have an event matching the pressed button state */
  if (G_LIKELY (view->pressed_button == (gint) event->button))
    {
      /* check if we should simply open or open in new window */
      if (G_LIKELY (event->button == 1))
        thunar_tree_view_action_open (view);
      else if (G_UNLIKELY (event->button == 2))
        thunar_tree_view_action_open_in_new_window (view);
    }

  /* reset the pressed button state */
  view->pressed_button = -1;

  /* call the parent's release event handler */
  return (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->button_release_event) (widget, event);
}



static void
thunar_tree_view_drag_data_received (GtkWidget        *widget,
                                     GdkDragContext   *context,
                                     gint              x,
                                     gint              y,
                                     GtkSelectionData *selection_data,
                                     guint             info,
                                     guint             time)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GdkDragAction   actions;
  GdkDragAction   action;
  ThunarFile     *file;
  gboolean        succeed = FALSE;

  /* check if don't already know the drop data */
  if (G_LIKELY (!view->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TARGET_TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
        view->drop_path_list = thunar_vfs_path_list_from_string ((const gchar *) selection_data->data, NULL);

      /* reset the state */
      view->drop_data_ready = TRUE;
    }

  /* check if the data was droppped */
  if (G_UNLIKELY (view->drop_occurred))
    {
      /* reset the state */
      view->drop_occurred = FALSE;

      /* determine the drop position */
      actions = thunar_tree_view_get_dest_actions (view, context, x, y, time, &file);
      if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
        {
          /* ask the user what to do with the drop data */
          action = (context->action == GDK_ACTION_ASK)
                 ? thunar_dnd_ask (GTK_WIDGET (view), file, view->drop_path_list, time, actions)
                 : context->action;

          /* perform the requested action */
          if (G_LIKELY (action != 0))
            succeed = thunar_dnd_perform (GTK_WIDGET (view), file, view->drop_path_list, action, NULL);
        }

      /* release the file reference */
      if (G_LIKELY (file != NULL))
        g_object_unref (G_OBJECT (file));

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, time);

      /* disable the highlighting and release the drag data */
      thunar_tree_view_drag_leave (GTK_WIDGET (view), context, time);
    }
}



static gboolean
thunar_tree_view_drag_drop (GtkWidget      *widget,
                            GdkDragContext *context,
                            gint            x,
                            gint            y,
                            guint           time)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GdkAtom         target;

  /* determine the drop target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (G_LIKELY (target == gdk_atom_intern ("text/uri-list", FALSE)))
    {
      /* set state so the drag-data-received handler
       * knows that this is really a drop this time.
       */
      view->drop_occurred = TRUE;

      /* request the drag data from the source. */
      gtk_drag_get_data (widget, context, target, time);
    }
  else
    {
      /* we cannot handle the drop */
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_tree_view_drag_motion (GtkWidget      *widget,
                              GdkDragContext *context,
                              gint            x,
                              gint            y,
                              guint           time)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GdkAtom         target;

  /* determine the drop target */
  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (G_UNLIKELY (target != gdk_atom_intern ("text/uri-list", FALSE)))
    {
      /* reset the "drop-file" of the icon renderer */
      g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

      /* we cannot handle the drop */
      return FALSE;
    }

  /* request the drop data on-demand (if we don't have it already) */
  if (G_UNLIKELY (!view->drop_data_ready))
    {
      /* reset the "drop-file" of the icon renderer */
      g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

      /* request the drag data from the source */
      gtk_drag_get_data (widget, context, target, time);

      /* tell Gdk that we don't know whether we can drop */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      /* check if we can drop here */
      thunar_tree_view_get_dest_actions (view, context, x, y, time, NULL);
    }

  /* start the drag autoscroll timer if not already running */
  if (G_UNLIKELY (view->drag_scroll_timer_id < 0))
    {
      /* schedule the drag autoscroll timer */
      view->drag_scroll_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 50, thunar_tree_view_drag_scroll_timer,
                                                       view, thunar_tree_view_drag_scroll_timer_destroy);
    }

  return TRUE;
}



static void
thunar_tree_view_drag_leave (GtkWidget      *widget,
                             GdkDragContext *context,
                             guint           time)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);

  /* cancel any running autoscroll timer */
  if (G_LIKELY (view->drag_scroll_timer_id >= 0))
    g_source_remove (view->drag_scroll_timer_id);

  /* reset the "drop-file" of the icon renderer */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", NULL, NULL);

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (view->drop_data_ready))
    {
      thunar_vfs_path_list_free (view->drop_path_list);
      view->drop_data_ready = FALSE;
      view->drop_path_list = NULL;
    }

  /* call the parent's handler */
  (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->drag_leave) (widget, context, time);
}



static gboolean
thunar_tree_view_popup_menu (GtkWidget *widget)
{
  GtkTreeSelection *selection;
  ThunarTreeView   *view = THUNAR_TREE_VIEW (widget);
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* popup the context menu */
      thunar_tree_view_context_menu (view, NULL, model, &iter);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->popup_menu != NULL)
    {
      /* call the parent's "popup-menu" handler */
      return (*GTK_WIDGET_CLASS (thunar_tree_view_parent_class)->popup_menu) (widget);
    }

  return FALSE;
}



static void
thunar_tree_view_row_activated (GtkTreeView       *tree_view,
                                GtkTreePath       *path,
                                GtkTreeViewColumn *column)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (tree_view);

  /* call the parent's "row-activated" handler */
  if (GTK_TREE_VIEW_CLASS (thunar_tree_view_parent_class)->row_activated != NULL)
    (*GTK_TREE_VIEW_CLASS (thunar_tree_view_parent_class)->row_activated) (tree_view, path, column);

  /* toggle the expanded state of the activated row... */
  if (gtk_tree_view_row_expanded (tree_view, path))
    gtk_tree_view_collapse_row (tree_view, path);
  else
    gtk_tree_view_expand_row (tree_view, path, FALSE);

  /* ...open the selected folder */
  thunar_tree_view_action_open (view);
}



static gboolean
thunar_tree_view_test_expand_row (GtkTreeView *tree_view,
                                  GtkTreeIter *iter,
                                  GtkTreePath *path)
{
  ThunarVfsVolume *volume;
  ThunarTreeView  *view = THUNAR_TREE_VIEW (tree_view);
  GtkWidget       *window;
  gboolean         expandable = TRUE;
  GError          *error = NULL;

  /* determine the volume for the iterator */
  gtk_tree_model_get (GTK_TREE_MODEL (view->model), iter, THUNAR_TREE_MODEL_COLUMN_VOLUME, &volume, -1);

  /* check if we have a volume */
  if (G_UNLIKELY (volume != NULL))
    {
      /* check if we need to mount the volume first */
      if (!thunar_vfs_volume_is_mounted (volume))
        {
          /* determine the toplevel window */
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* try to mount the volume */
          expandable = thunar_vfs_volume_mount (volume, window, &error);
          if (G_UNLIKELY (!expandable))
            {
              /* display an error dialog to inform the user */
              thunar_dialogs_show_error (window, error, _("Failed to mount \"%s\""), thunar_vfs_volume_get_name (volume));
              g_error_free (error);
            }
        }

      /* release the volume */
      g_object_unref (G_OBJECT (volume));
    }

  /* cancel the cursor idle source if not expandable */
  if (!expandable && view->cursor_idle_id >= 0)
    g_source_remove (view->cursor_idle_id);

  return !expandable;
}



static void
thunar_tree_view_row_collapsed (GtkTreeView *tree_view,
                                GtkTreeIter *iter,
                                GtkTreePath *path)
{
  /* schedule a cleanup of the tree model */
  thunar_tree_model_cleanup (THUNAR_TREE_VIEW (tree_view)->model);
}



static gboolean
thunar_tree_view_delete_selected_files (ThunarTreeView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW (view), FALSE);

  /* ask the user whether to delete the folder... */
  thunar_tree_view_action_delete (view);

  /* ...and we're done */
  return TRUE;
}



static void
thunar_tree_view_context_menu (ThunarTreeView *view,
                               GdkEventButton *event,
                               GtkTreeModel   *model,
                               GtkTreeIter    *iter)
{
  ThunarVfsVolume *volume;
  ThunarFile      *parent_file;
  ThunarFile      *file;
  GtkWidget       *image;
  GtkWidget       *menu;
  GtkWidget       *item;

  /* verify that we're connected to the clipboard manager */
  if (G_UNLIKELY (view->clipboard == NULL))
    return;

  /* determine the file and volume for the given iter */
  gtk_tree_model_get (model, iter,
                      THUNAR_TREE_MODEL_COLUMN_FILE, &file,
                      THUNAR_TREE_MODEL_COLUMN_VOLUME, &volume,
                      -1);

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* append the "Open" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_open), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL || volume != NULL));
  gtk_widget_show (item);

  /* set the stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* append the "Open in New Window" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("Open in New Window"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_open_in_new_window), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL || volume != NULL));
  gtk_widget_show (item);

  /* append a separator item */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  if (G_UNLIKELY (volume != NULL))
    {
      /* append the "Mount Volume" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Mount Volume"));
      gtk_widget_set_sensitive (item, !thunar_vfs_volume_is_mounted (volume));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_mount), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* check if the volume is ejectable */
      if (thunar_vfs_volume_is_ejectable (volume))
        {
          /* append the "Eject Volume" menu action */
          item = gtk_image_menu_item_new_with_mnemonic (_("E_ject Volume"));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_eject), view);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }
      else
        {
          /* append the "Unmount Volume" menu item */
          item = gtk_image_menu_item_new_with_mnemonic (_("_Unmount Volume"));
          gtk_widget_set_sensitive (item, thunar_vfs_volume_is_mounted (volume));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_unmount), view);
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
      gtk_widget_set_sensitive (item, (thunar_file_get_size (file) > 0));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_empty_trash), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* append a menu separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* check if we have a non-trashed resource */
  if (G_LIKELY (file != NULL && !thunar_file_is_trashed (file)))
    {
      /* append the "Create Folder" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("Create _Folder..."));
      gtk_widget_set_sensitive (item, thunar_file_is_writable (file));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_create_folder), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* append a separator item */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  if (G_LIKELY (file != NULL))
    {
      /* "Cut" and "Copy" don't make much sense for volumes */
      if (G_LIKELY (volume == NULL))
        {
          /* determine the parent file (required to determine "Cut" sensitivity) */
          parent_file = thunar_file_get_parent (file, NULL);

          /* append the "Cut" menu action */
          item = gtk_image_menu_item_new_with_mnemonic (_("Cu_t"));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_cut), view);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_set_sensitive (item, (parent_file != NULL && thunar_file_is_writable (parent_file)));
          gtk_widget_show (item);

          /* set the stock icon */
          image = gtk_image_new_from_stock (GTK_STOCK_CUT, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);

          /* append the "Copy" menu action */
          item = gtk_image_menu_item_new_with_mnemonic (_("_Copy"));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_copy), view);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_set_sensitive (item, (parent_file != NULL));
          gtk_widget_show (item);

          /* set the stock icon */
          image = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);

          /* cleanup */
          if (G_LIKELY (parent_file != NULL))
            g_object_unref (G_OBJECT (parent_file));
        }

      /* append the "Paste Into Folder" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Paste Into Folder"));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_paste_into_folder), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_set_sensitive (item, (thunar_file_is_writable (file) && thunar_clipboard_manager_get_can_paste (view->clipboard)));
      gtk_widget_show (item);

      /* set the stock icon */
      image = gtk_image_new_from_stock (GTK_STOCK_PASTE, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);

      /* "Delete" doesn't make much sense for volumes */
      if (G_LIKELY (volume == NULL))
        {
          /* determine the parent file (required to determine "Delete" sensitivity) */
          parent_file = thunar_file_get_parent (file, NULL);

          /* append the "Delete" menu action */
          item = gtk_image_menu_item_new_with_mnemonic (_("_Delete"));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_delete), view);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_set_sensitive (item, (parent_file != NULL && thunar_file_is_writable (parent_file)));
          gtk_widget_show (item);

          /* set the stock icon */
          image = gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);

          /* cleanup */
          if (G_LIKELY (parent_file != NULL))
            g_object_unref (G_OBJECT (parent_file));
        }

      /* append a separator item */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* append the "Properties" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("P_roperties..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_properties), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL));
  gtk_widget_show (item);

  /* set the stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* run the menu on the view's screen (taking over the floating reference on the menu) */
  thunar_gtk_menu_run (GTK_MENU (menu), GTK_WIDGET (view), NULL, NULL, (event != NULL) ? event->button : 0,
                       (event != NULL) ? event->time : gtk_get_current_event_time ());

  /* cleanup */
  if (G_UNLIKELY (volume != NULL))
    g_object_unref (G_OBJECT (volume));
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
}



static GdkDragAction
thunar_tree_view_get_dest_actions (ThunarTreeView *view,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           time,
                                   ThunarFile    **file_return)
{
  GdkDragAction actions = 0;
  GdkDragAction action = 0;
  GtkTreePath  *path = NULL;
  GtkTreeIter   iter;
  ThunarFile   *file = NULL;

  /* cancel any previously active expand timer */
  if (G_LIKELY (view->expand_timer_id >= 0))
    g_source_remove (view->expand_timer_id);

  /* determine the path for x/y */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y, &path, NULL, NULL, NULL))
    {
      /* determine the iter for the given path */
      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->model), &iter, path))
        {
          /* determine the file for the given path */
          gtk_tree_model_get (GTK_TREE_MODEL (view->model), &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
          if (G_LIKELY (file != NULL))
            {
              /* check if the file accepts the drop */
              actions = thunar_file_accepts_drop (file, view->drop_path_list, context, &action);
              if (G_UNLIKELY (actions == 0))
                {
                  /* reset file */
                  g_object_unref (G_OBJECT (file));
                  file = NULL;
                }
            }
        }
    }

  /* setup the new drag dest row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);

  /* check if we have drag dest row */
  if (G_LIKELY (path != NULL))
    {
      /* schedule a new expand timer to expand the drag dest row */
      view->expand_timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_TREE_VIEW_EXPAND_TIMEOUT,
                                                  thunar_tree_view_expand_timer, view,
                                                  thunar_tree_view_expand_timer_destroy);
    }

  /* setup the new "drop-file" */
  g_object_set (G_OBJECT (view->icon_renderer), "drop-file", file, NULL);

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, time);

  /* return the file if requested */
  if (G_LIKELY (file_return != NULL))
    {
      *file_return = file;
      file = NULL;
    }
  else if (G_UNLIKELY (file != NULL))
    {
      /* release the file */
      g_object_unref (G_OBJECT (file));
    }

  /* clean up */
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return actions;
}



static gboolean
thunar_tree_view_find_closest_ancestor (ThunarTreeView *view,
                                        GtkTreePath    *path,
                                        GtkTreePath   **ancestor_return,
                                        gboolean       *exact_return)
{
  GtkTreeModel *model = GTK_TREE_MODEL (view->model);
  GtkTreePath  *child_path;
  GtkTreeIter   child_iter;
  GtkTreeIter   iter;
  ThunarFile   *file;
  gboolean      found = FALSE;

  /* determine the iter for the current path */
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (view->model), &iter, path))
    return FALSE;

  /* process all items at this level */
  do
    {
      /* check if the file for iter is an ancestor of the current directory */
      gtk_tree_model_get (model, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
      if (G_UNLIKELY (file == NULL))
        continue;

      /* check if this is the file we're looking for */
      if (G_UNLIKELY (file == view->current_directory))
        {
          /* we found the very best ancestor iter! */
          *ancestor_return = gtk_tree_model_get_path (model, &iter);
          *exact_return = TRUE;
          found = TRUE;
        }
      else if (thunar_file_is_ancestor (view->current_directory, file))
        {
          /* check if we can find an even better ancestor below this node */
          if (gtk_tree_model_iter_children (model, &child_iter, &iter))
            {
              child_path = gtk_tree_model_get_path (model, &child_iter);
              found = thunar_tree_view_find_closest_ancestor (view, child_path, ancestor_return, exact_return);
              gtk_tree_path_free (child_path);
            }

          /* maybe not exact, but still an ancestor */
          if (G_UNLIKELY (!found))
            {
              /* we found an ancestor, not an exact match */
              *ancestor_return = gtk_tree_model_get_path (model, &iter);
              *exact_return = FALSE;
              found = TRUE;
            }
        }

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
  while (!found && gtk_tree_model_iter_next (model, &iter));

  return found;
}



static ThunarFile*
thunar_tree_view_get_selected_file (ThunarTreeView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file = NULL;

  /* determine file for the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);

  return file;
}



static ThunarVfsVolume*
thunar_tree_view_get_selected_volume (ThunarTreeView *view)
{
  GtkTreeSelection *selection;
  ThunarVfsVolume  *volume = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  /* determine file for the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, THUNAR_TREE_MODEL_COLUMN_VOLUME, &volume, -1);

  return volume;
}



static void
thunar_tree_view_action_copy (ThunarTreeView *view)
{
  ThunarFile *file;
  GList       list;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* verify that we're connected to the clipboard */
  if (G_UNLIKELY (view->clipboard == NULL))
    return;

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* fake a file list */
      list.data = file;
      list.next = NULL;
      list.prev = NULL;

      /* copy the selected to the clipboard */
      thunar_clipboard_manager_copy_files (view->clipboard, &list);

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_create_folder (ThunarTreeView *view)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  ThunarApplication     *application;
  ThunarFile            *directory;
  GList                  path_list;
  gchar                 *name;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* determine the selected directory */
  directory = thunar_tree_view_get_selected_file (view);
  if (G_UNLIKELY (directory == NULL))
    return;

  /* lookup "inode/directory" mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (GTK_WIDGET (view), mime_info, _("New Folder"), _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* fake the path list */
      path_list.data = thunar_vfs_path_relative (thunar_file_get_path (directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      application = thunar_application_get ();
      thunar_application_mkdir (application, GTK_WIDGET (view), &path_list, thunar_tree_view_new_files_closure (view));
      g_object_unref (G_OBJECT (application));

      /* release the path */
      thunar_vfs_path_unref (path_list.data);

      /* release the file name */
      g_free (name);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_unref (mime_info);
  g_object_unref (G_OBJECT (directory));
}



static void
thunar_tree_view_action_cut (ThunarTreeView *view)
{
  ThunarFile *file;
  GList       list;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* verify that we're connected to the clipboard */
  if (G_UNLIKELY (view->clipboard == NULL))
    return;

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* fake a file list */
      list.data = file;
      list.next = NULL;
      list.prev = NULL;

      /* cut the selected to the clipboard */
      thunar_clipboard_manager_cut_files (view->clipboard, &list);

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_delete (ThunarTreeView *view)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GList              file_list;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* fake a file list */
      file_list.data = file;
      file_list.next = NULL;
      file_list.prev = NULL;

      /* delete the file */
      application = thunar_application_get ();
      thunar_application_unlink_files (application, GTK_WIDGET (view), &file_list);
      g_object_unref (G_OBJECT (application));

      /* release the file */
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_eject (ThunarTreeView *view)
{
  ThunarVfsVolume *volume;
  GtkWidget       *window;
  GError          *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* determine the selected volume */
  volume = thunar_tree_view_get_selected_volume (view);
  if (G_LIKELY (volume != NULL))
    {
      /* determine the toplevel window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (view));

      /* try to eject the volume */
      if (!thunar_vfs_volume_eject (volume, window, &error))
        {
          /* display an error dialog to inform the user */
          thunar_dialogs_show_error (window, error, _("Failed to eject \"%s\""), thunar_vfs_volume_get_name (volume));
          g_error_free (error);
        }

      /* release the volume */
      g_object_unref (G_OBJECT (volume));
    }
}



static void
thunar_tree_view_action_empty_trash (ThunarTreeView *view)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* empty the trash bin (asking the user first) */
  application = thunar_application_get ();
  thunar_application_empty_trash (application, GTK_WIDGET (view));
  g_object_unref (G_OBJECT (application));
}



static gboolean
thunar_tree_view_action_mount (ThunarTreeView *view)
{
  ThunarVfsVolume *volume;
  GtkWidget       *window;
  gboolean         result = TRUE;
  GError          *error = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW (view), FALSE);

  /* determine the selected volume */
  volume = thunar_tree_view_get_selected_volume (view);
  if (G_UNLIKELY (volume != NULL))
    {
      /* check if the volume is not already mounted */
      if (!thunar_vfs_volume_is_mounted (volume))
        {
          /* determine the toplevel window */
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* try to mount the volume */
          result = thunar_vfs_volume_mount (volume, window, &error);
          if (G_UNLIKELY (!result))
            {
              /* display an error dialog to inform the user */
              thunar_dialogs_show_error (window, error, _("Failed to mount \"%s\""), thunar_vfs_volume_get_name (volume));
              g_error_free (error);
            }
        }

      /* release the volume */
      g_object_unref (G_OBJECT (volume));
    }

  return result;
}



static void
thunar_tree_view_action_open (ThunarTreeView *view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* if its a volume, make sure its mounted */
  if (!thunar_tree_view_action_mount (view))
    return;

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* open that folder in the main view */
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), file);
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_open_in_new_window (ThunarTreeView *view)
{
  ThunarApplication *application;
  ThunarFile        *file;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* if its a volume, make sure its mounted */
  if (!thunar_tree_view_action_mount (view))
    return;

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* open a new window for the selected folder */
      application = thunar_application_get ();
      thunar_application_open_window (application, file, gtk_widget_get_screen (GTK_WIDGET (view)));
      g_object_unref (G_OBJECT (application));
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_paste_into_folder (ThunarTreeView *view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* verify that we're connected to the clipboard */
  if (G_UNLIKELY (view->clipboard == NULL))
    return;

  /* determine the selected folder */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* paste the files from the clipboard to the selected folder */
      thunar_clipboard_manager_paste_files (view->clipboard, thunar_file_get_path (file), GTK_WIDGET (view), NULL);

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_properties (ThunarTreeView *view)
{
  ThunarFile *file;
  GtkWidget  *dialog;
  GtkWidget  *toplevel;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* determine the selected file */
  file = thunar_tree_view_get_selected_file (view);
  if (G_LIKELY (file != NULL))
    {
      /* determine the toplevel window */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
      if (G_LIKELY (toplevel != NULL && GTK_WIDGET_TOPLEVEL (toplevel)))
        {
          /* popup the properties dialog */
          dialog = thunar_properties_dialog_new ();
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), file);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
          gtk_widget_show (dialog);
        }

      /* release the file */
      g_object_unref (G_OBJECT (file));
    }
}



static void
thunar_tree_view_action_unmount (ThunarTreeView *view)
{
  ThunarVfsVolume *volume;
  GtkWidget       *window;
  GError          *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* determine the selected volume */
  volume = thunar_tree_view_get_selected_volume (view);
  if (G_LIKELY (volume != NULL))
    {
      /* determine the toplevel window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (view));

      /* try to unmount the volume */
      if (!thunar_vfs_volume_unmount (volume, window, &error))
        {
          /* display an error dialog */
          thunar_dialogs_show_error (window, error, _("Failed to unmount \"%s\""), thunar_vfs_volume_get_name (volume));
          g_error_free (error);
        }

      /* release the volume */
      g_object_unref (G_OBJECT (volume));
    }
}



static GClosure*
thunar_tree_view_new_files_closure (ThunarTreeView *view)
{
  /* drop any previous "new-files" closure */
  if (G_UNLIKELY (view->new_files_closure != NULL))
    {
      g_closure_invalidate (view->new_files_closure);
      g_closure_unref (view->new_files_closure);
    }

  /* allocate a new "new-files" closure */
  view->new_files_closure = g_cclosure_new (G_CALLBACK (thunar_tree_view_new_files), view, NULL);
  g_closure_ref (view->new_files_closure);
  g_closure_sink (view->new_files_closure);

  /* and return our new closure */
  return view->new_files_closure;
}



static void
thunar_tree_view_new_files (ThunarVfsJob   *job,
                            GList          *path_list,
                            ThunarTreeView *view)
{
  ThunarFile *file;

  /* check if we have exactly one new path */
  if (G_UNLIKELY (path_list == NULL || path_list->next != NULL))
    return;

  /* determine the file for the first path */
  file = thunar_file_cache_lookup (path_list->data);
  if (G_LIKELY (file != NULL && thunar_file_is_directory (file)))
    {
      /* change to the newly created folder */
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (view), file);
    }
}



static gboolean
thunar_tree_view_visible_func (ThunarTreeModel *model,
                               ThunarFile      *file,
                               gpointer         user_data)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (user_data);
  gboolean        visible = TRUE;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_TREE_MODEL (model), FALSE);

  /* if show_hidden is TRUE, nothing is filtered */
  if (G_LIKELY (!view->show_hidden))
    {
      /* we display all non-hidden file and hidden files that are ancestors of the current directory */
      visible = !thunar_file_is_hidden (file) || (view->current_directory == file)
                || (view->current_directory != NULL && thunar_file_is_ancestor (view->current_directory, file));
    }

  return visible;
}



static gboolean
thunar_tree_view_selection_func (GtkTreeSelection *selection,
                                 GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 gboolean          path_currently_selected,
                                 gpointer          user_data)
{
  ThunarVfsVolume *volume;
  GtkTreeIter      iter;
  ThunarFile      *file;
  gboolean         result = FALSE;

  /* every row may be unselected at any time */
  if (G_UNLIKELY (path_currently_selected))
    return TRUE;

  /* determine the iterator for the path */
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      /* determine the file for the iterator */
      gtk_tree_model_get (model, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
      if (G_LIKELY (file != NULL))
        {
          /* rows with files can be selected */
          result = TRUE;

          /* release file */
          g_object_unref (G_OBJECT (file));
        }
      else
        {
          /* but maybe the row has a volume */
          gtk_tree_model_get (model, &iter, THUNAR_TREE_MODEL_COLUMN_VOLUME, &volume, -1);
          if (G_LIKELY (volume != NULL))
            {
              /* rows with volumes can also be selected */
              result = TRUE;

              /* release volume */
              g_object_unref (G_OBJECT (volume));
            }
        }
    }

  return result;
}



static gboolean
thunar_tree_view_cursor_idle (gpointer user_data)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (user_data);
  GtkTreePath    *ancestor = NULL;
  GtkTreePath    *parent;
  GtkTreePath    *path;
  gboolean        done = TRUE;

  GDK_THREADS_ENTER ();

  /* verify that we still have a current directory */
  if (G_LIKELY (view->current_directory != NULL))
    {
      /* determine the current cursor (fallback to root node) */
      gtk_tree_view_get_cursor (GTK_TREE_VIEW (view), &path, NULL);
      if (G_UNLIKELY (path == NULL))
        path = gtk_tree_path_new_first ();

      /* look for the closest ancestor in the whole tree, starting from the current path */
      for (; gtk_tree_path_get_depth (path) > 0; gtk_tree_path_up (path))
        {
          /* be sure to start from the first path at that level */
          while (gtk_tree_path_prev (path))
            ;

          /* try to find the closest ancestor relative to the current path */
          if (thunar_tree_view_find_closest_ancestor (view, path, &ancestor, &done))
            {
              /* expand to the best ancestor if not an exact match */
              if (G_LIKELY (!done))
                {
                  /* just expand everything up to the row and its children */
                  gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), ancestor);
                }
              else if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (view), ancestor))
                {
                  /* just expand everything up to the row, but not the children */
                  parent = gtk_tree_path_copy (ancestor);
                  if (gtk_tree_path_up (parent))
                    gtk_tree_view_expand_to_path (GTK_TREE_VIEW (view), parent);
                  gtk_tree_path_free (parent);
                }

              /* place cursor on the ancestor */
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), ancestor, NULL, FALSE);

              /* release the ancestor path */
              gtk_tree_path_free (ancestor);

              /* we did it */
              break;
            }
        }

      /* release the tree path */
      gtk_tree_path_free (path);
    }

  GDK_THREADS_LEAVE ();

  return !done;
}



static void
thunar_tree_view_cursor_idle_destroy (gpointer user_data)
{
  THUNAR_TREE_VIEW (user_data)->cursor_idle_id = -1;
}



static gboolean
thunar_tree_view_drag_scroll_timer (gpointer user_data)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (user_data);
  GtkAdjustment  *vadjustment;
#if GTK_CHECK_VERSION(2,8,0)
  GtkTreePath    *start_path;
  GtkTreePath    *end_path;
  GtkTreePath    *path;
#endif
  gfloat          value;
  gint            offset;
  gint            y, h;

  GDK_THREADS_ENTER ();

  /* verify that we are realized */
  if (GTK_WIDGET_REALIZED (view))
    {
      /* determine pointer location and window geometry */
      gdk_window_get_pointer (GTK_WIDGET (view)->window, NULL, &y, NULL);
      gdk_window_get_geometry (GTK_WIDGET (view)->window, NULL, NULL, NULL, &h, NULL);

      /* check if we are near the edge */
      offset = y - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (y - (h - 2 * 20), 0);

      /* change the vertical adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          vadjustment = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (view));

          /* determine the new value */
          value = CLAMP (vadjustment->value + 2 * offset, vadjustment->lower, vadjustment->upper - vadjustment->page_size);

          /* check if we have a new value */
          if (G_UNLIKELY (vadjustment->value != value))
            {
              /* apply the new value */
              gtk_adjustment_set_value (vadjustment, value);

#if GTK_CHECK_VERSION(2,8,0)
              /* drop any pending expand timer source, as its confusing
               * if a path is expanded while scrolling through the view.
               * reschedule it if the drag dest path is still visible.
               */
              if (G_UNLIKELY (view->expand_timer_id >= 0))
                {
                  /* drop the current expand timer source */
                  g_source_remove (view->expand_timer_id);

                  /* determine the visible range of the tree view */
                  if (gtk_tree_view_get_visible_range (GTK_TREE_VIEW (view), &start_path, &end_path))
                    {
                      /* determine the drag dest row */
                      gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (view), &path, NULL);
                      if (G_LIKELY (path != NULL))
                        {
                          /* check if the drag dest row is currently visible */
                          if (gtk_tree_path_compare (path, start_path) >= 0 && gtk_tree_path_compare (path, end_path) <= 0)
                            {
                              /* schedule a new expand timer to expand the drag dest row */
                              view->expand_timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_TREE_VIEW_EXPAND_TIMEOUT,
                                                                          thunar_tree_view_expand_timer, view,
                                                                          thunar_tree_view_expand_timer_destroy);
                            }

                          /* release the drag dest row */
                          gtk_tree_path_free (path);
                        }

                      /* release the start/end paths */
                      gtk_tree_path_free (start_path);
                      gtk_tree_path_free (end_path);
                    }
                }
#endif
            }
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_tree_view_drag_scroll_timer_destroy (gpointer user_data)
{
  THUNAR_TREE_VIEW (user_data)->drag_scroll_timer_id = -1;
}



static gboolean
thunar_tree_view_expand_timer (gpointer user_data)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (user_data);
  GtkTreePath    *path;

  GDK_THREADS_ENTER ();

  /* cancel the drag autoscroll timer when expanding a row */
  if (G_UNLIKELY (view->drag_scroll_timer_id >= 0))
    g_source_remove (view->drag_scroll_timer_id);

  /* determine the drag dest row */
  gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (view), &path, NULL);
  if (G_LIKELY (path != NULL))
    {
      /* expand the drag dest row */
      gtk_tree_view_expand_row (GTK_TREE_VIEW (view), path, FALSE);
      gtk_tree_path_free (path);
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_tree_view_expand_timer_destroy (gpointer user_data)
{
  THUNAR_TREE_VIEW (user_data)->expand_timer_id = -1;
}



/**
 * thunar_tree_view_new:
 *
 * Allocates a new #ThunarTreeView instance.
 *
 * Return value: the newly allocated #ThunarTreeView instance.
 **/
GtkWidget*
thunar_tree_view_new (void)
{
  return g_object_new (THUNAR_TYPE_TREE_VIEW, NULL);
}



/**
 * thunar_tree_view_get_show_hidden:
 * @view : a #ThunarTreeView.
 *
 * Returns %TRUE if hidden and backup folders are
 * shown in @view.
 *
 * Return value: %TRUE if hidden folders are shown.
 **/
gboolean
thunar_tree_view_get_show_hidden (ThunarTreeView *view)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_VIEW (view), FALSE);
  return view->show_hidden;
}



/**
 * thunar_tree_view_set_show_hidden:
 * @view        : a #ThunarTreeView.
 * @show_hidden : %TRUE to show hidden and backup folders.
 *
 * If @show_hidden is %TRUE, @view will shown hidden and
 * backup folders. Else, these folders will be hidden.
 **/
void
thunar_tree_view_set_show_hidden (ThunarTreeView *view,
                                  gboolean        show_hidden)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_VIEW (view));
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (view->model));

  /* normalize the value */
  show_hidden = !!show_hidden;

  /* check if we have a new setting */
  if (view->show_hidden != show_hidden)
    {
      /* apply the new setting */
      view->show_hidden = show_hidden;

      /* update the model */
      thunar_tree_model_refilter (view->model);

      /* notify listeners */
      g_object_notify (G_OBJECT (view), "show-hidden");
    }
}
