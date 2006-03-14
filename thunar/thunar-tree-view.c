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

#include <thunar/thunar-application.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-tree-model.h>
#include <thunar/thunar-tree-view.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SHOW_HIDDEN,
};



static void         thunar_tree_view_class_init                 (ThunarTreeViewClass  *klass);
static void         thunar_tree_view_navigator_init             (ThunarNavigatorIface *iface);
static void         thunar_tree_view_init                       (ThunarTreeView       *view);
static void         thunar_tree_view_finalize                   (GObject              *object);
static void         thunar_tree_view_get_property               (GObject              *object,
                                                                 guint                 prop_id,
                                                                 GValue               *value,
                                                                 GParamSpec           *pspec);
static void         thunar_tree_view_set_property               (GObject              *object,
                                                                 guint                 prop_id,
                                                                 const GValue         *value,
                                                                 GParamSpec           *pspec);
static ThunarFile  *thunar_tree_view_get_current_directory      (ThunarNavigator      *navigator);
static void         thunar_tree_view_set_current_directory      (ThunarNavigator      *navigator,
                                                                 ThunarFile           *current_directory);
static gboolean     thunar_tree_view_button_press_event         (GtkWidget            *widget,
                                                                 GdkEventButton       *event);
static gboolean     thunar_tree_view_button_release_event       (GtkWidget            *widget,
                                                                 GdkEventButton       *event);
static gboolean     thunar_tree_view_popup_menu                 (GtkWidget            *widget);
static void         thunar_tree_view_row_activated              (GtkTreeView          *tree_view,
                                                                 GtkTreePath          *path,
                                                                 GtkTreeViewColumn    *column);
static void         thunar_tree_view_context_menu               (ThunarTreeView       *view,
                                                                 GdkEventButton       *event,
                                                                 GtkTreeModel         *filter,
                                                                 GtkTreeIter          *iter);
static gboolean     thunar_tree_view_find_closest_ancestor      (ThunarTreeView       *view,
                                                                 GtkTreePath          *path,
                                                                 GtkTreePath         **ancestor_return,
                                                                 gboolean             *exact_return);
static ThunarFile  *thunar_tree_view_get_selected_file          (ThunarTreeView       *view);
static void         thunar_tree_view_action_create_folder       (ThunarTreeView       *view);
static void         thunar_tree_view_action_open                (ThunarTreeView       *view);
static void         thunar_tree_view_action_open_in_new_window  (ThunarTreeView       *view);
static void         thunar_tree_view_action_properties          (ThunarTreeView       *view);
static GClosure    *thunar_tree_view_new_files_closure          (ThunarTreeView       *view);
static void         thunar_tree_view_new_files                  (ThunarVfsJob         *job,
                                                                 GList                *path_list,
                                                                 ThunarTreeView       *view);
static gboolean     thunar_tree_view_filter_visible_func        (GtkTreeModel         *filter,
                                                                 GtkTreeIter          *iter,
                                                                 gpointer              user_data);
static gboolean     thunar_tree_view_selection_func             (GtkTreeSelection     *selection,
                                                                 GtkTreeModel         *model,
                                                                 GtkTreePath          *path,
                                                                 gboolean              path_currently_selected,
                                                                 gpointer              user_data);
static gboolean     thunar_tree_view_expand_idle                (gpointer              user_data);
static void         thunar_tree_view_expand_idle_destroy        (gpointer              user_data);



struct _ThunarTreeViewClass
{
  GtkTreeViewClass __parent__;
};

struct _ThunarTreeView
{
  GtkTreeView        __parent__;
  ThunarPreferences *preferences;
  ThunarFile        *current_directory;
  ThunarTreeModel   *model;
  gboolean           show_hidden;

  /* the "new-files" closure, which is used to
   * open newly created directories once done.
   */
  GClosure          *new_files_closure;

  /* the currently pressed mouse button, set in the
   * button-press-event handler if the associated
   * button-release-event should activate.
   */
  gint               pressed_button;

#if GTK_CHECK_VERSION(2,8,0)
  /* id of the signal used to queue a resize on the
   * column whenever the shortcuts icon size is changed.
   */
  gint               queue_resize_signal_id;
#endif

  /* expand to current directory idle source */
  gint               expand_idle_id;
};



static GObjectClass *thunar_tree_view_parent_class;



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
  GObjectClass     *gobject_class;

  /* determine the parent type class */
  thunar_tree_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tree_view_finalize;
  gobject_class->get_property = thunar_tree_view_get_property;
  gobject_class->set_property = thunar_tree_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->button_press_event = thunar_tree_view_button_press_event;
  gtkwidget_class->button_release_event = thunar_tree_view_button_release_event;
  gtkwidget_class->popup_menu = thunar_tree_view_popup_menu;

  gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
  gtktree_view_class->row_activated = thunar_tree_view_row_activated;

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
  GtkTreeModel      *filter;

  /* initialize the view internals */
  view->expand_idle_id = -1;

  /* grab a reference on the preferences; be sure to redraw the view
   * whenever the "tree-icon-emblems" preference changes.
   */
  view->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::tree-icon-emblems", G_CALLBACK (gtk_widget_queue_draw), view);

  /* connect to the default tree model */
  view->model = thunar_tree_model_get_default ();

  /* setup the filter for the tree view */
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (view->model), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter), thunar_tree_view_filter_visible_func, view, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (view), filter);
  g_object_unref (G_OBJECT (filter));

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
  renderer = thunar_icon_renderer_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "file", THUNAR_TREE_MODEL_COLUMN_FILE,
                                       NULL);

  /* sync the "emblems" property of the icon renderer with the "tree-icon-emblems" preference
   * and the "size" property of the renderer with the "tree-icon-size" preference.
   */
  exo_binding_new (G_OBJECT (view->preferences), "tree-icon-size", G_OBJECT (renderer), "size");
  exo_binding_new (G_OBJECT (view->preferences), "tree-icon-emblems", G_OBJECT (renderer), "emblems");

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
}



static void
thunar_tree_view_finalize (GObject *object)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (object);

  /* disconnect the queue resize signal handler */
#if GTK_CHECK_VERSION(2,8,0)
  g_signal_handler_disconnect (G_OBJECT (view->preferences), view->queue_resize_signal_id);
#endif

  /* be sure to cancel the expand idle source */
  if (G_UNLIKELY (view->expand_idle_id >= 0))
    g_source_remove (view->expand_idle_id);

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
  GtkTreeModel   *filter;

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

      /* update the filter if the new current directory is hidden */
      if (!view->show_hidden && thunar_file_is_hidden (current_directory))
        {
          filter = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
          gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filter));
        }

      /* schedule an idle source to expand to the current directory */
      if (G_LIKELY (view->expand_idle_id < 0))
        view->expand_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_tree_view_expand_idle, view, thunar_tree_view_expand_idle_destroy);

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



static gboolean
thunar_tree_view_button_press_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (widget);
  GtkTreeModel   *filter;
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
          filter = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
          if (gtk_tree_model_get_iter (filter, &iter, path))
            {
              /* popup the context menu */
              thunar_tree_view_context_menu (view, event, filter, &iter);

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
  if (G_LIKELY (view->pressed_button == event->button))
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



static gboolean
thunar_tree_view_popup_menu (GtkWidget *widget)
{
  GtkTreeSelection *selection;
  ThunarTreeView   *view = THUNAR_TREE_VIEW (widget);
  GtkTreeModel     *filter;
  GtkTreeIter       iter;

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &filter, &iter))
    {
      /* popup the context menu */
      thunar_tree_view_context_menu (view, NULL, filter, &iter);
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



static void
thunar_tree_view_context_menu (ThunarTreeView *view,
                               GdkEventButton *event,
                               GtkTreeModel   *filter,
                               GtkTreeIter    *iter)
{
  ThunarFile *file;
  GtkWidget  *image;
  GtkWidget  *menu;
  GtkWidget  *item;
  GMainLoop  *loop;

  /* determine the file for the given iter */
  gtk_tree_model_get (filter, iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);

  /* prepare the internal loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* prepare the popup menu */
  menu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (view)));
  g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);
  exo_gtk_object_ref_sink (GTK_OBJECT (menu));

  /* append the "Open" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_open), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL));
  gtk_widget_show (item);

  /* set the stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* append the "Open in New Window" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("Open in New Window"));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_open_in_new_window), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL));
  gtk_widget_show (item);

  /* append a separator item */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* append the "Create Folder" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("Create _Folder..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_create_folder), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL && thunar_file_is_writable (file)));
  gtk_widget_show (item);

  /* append a separator item */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* append the "Properties" menu action */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Properties..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_tree_view_action_properties), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, (file != NULL));
  gtk_widget_show (item);

  /* set the stock icon */
  image = gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);

  /* run the internal main loop */
  gtk_grab_add (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, (event != NULL) ? event->button : 0,
                  (event != NULL) ? event->time : gtk_get_current_event_time ());
  g_main_loop_run (loop);
  gtk_grab_remove (menu);

  /* cleanup */
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  g_object_unref (G_OBJECT (menu));
  g_main_loop_unref (loop);
}



static gboolean
thunar_tree_view_find_closest_ancestor (ThunarTreeView *view,
                                        GtkTreePath    *path,
                                        GtkTreePath   **ancestor_return,
                                        gboolean       *exact_return)
{
  GtkTreeModel *filter;
  GtkTreePath  *child_path;
  GtkTreeIter   child_iter;
  GtkTreeIter   iter;
  ThunarFile   *file;
  gboolean      found = FALSE;

  /* determine the iter for the current path */
  filter = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (!gtk_tree_model_get_iter (filter, &iter, path))
    return FALSE;

  /* process all items at this level */
  do
    {
      /* check if the file for iter is an ancestor of the current directory */
      gtk_tree_model_get (filter, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
      if (G_UNLIKELY (file == NULL))
        continue;

      /* check if this is the file we're looking for */
      if (G_UNLIKELY (file == view->current_directory))
        {
          /* we found the very best ancestor iter! */
          *ancestor_return = gtk_tree_model_get_path (filter, &iter);
          *exact_return = TRUE;
          found = TRUE;
        }
      else if (thunar_file_is_ancestor (view->current_directory, file))
        {
          /* check if we can find an even better ancestor below this node */
          if (gtk_tree_model_iter_children (filter, &child_iter, &iter))
            {
              child_path = gtk_tree_model_get_path (filter, &child_iter);
              found = thunar_tree_view_find_closest_ancestor (view, child_path, ancestor_return, exact_return);
              gtk_tree_path_free (child_path);
            }

          /* maybe not exact, but still an ancestor */
          if (G_UNLIKELY (!found))
            {
              /* we found an ancestor, not an exact match */
              *ancestor_return = gtk_tree_model_get_path (filter, &iter);
              *exact_return = FALSE;
              found = TRUE;
            }
        }

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }
  while (!found && gtk_tree_model_iter_next (filter, &iter));

  return found;
}



static ThunarFile*
thunar_tree_view_get_selected_file (ThunarTreeView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *filter;
  GtkTreeIter       iter;
  ThunarFile       *file = NULL;

  /* determine file for the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &filter, &iter))
    gtk_tree_model_get (filter, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);

  return file;
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

  g_return_if_fail (THUNAR_IS_TREE_VIEW (view));

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
thunar_tree_view_action_open (ThunarTreeView *view)
{
  ThunarFile *file;

  g_return_if_fail (THUNAR_IS_TREE_VIEW (view));

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

  g_return_if_fail (THUNAR_IS_TREE_VIEW (view));

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
thunar_tree_view_action_properties (ThunarTreeView *view)
{
  ThunarFile *file;
  GtkWidget  *dialog;
  GtkWidget  *toplevel;

  g_return_if_fail (THUNAR_IS_TREE_VIEW (view));

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
thunar_tree_view_filter_visible_func (GtkTreeModel *filter,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
  ThunarTreeView *view = THUNAR_TREE_VIEW (user_data);
  ThunarFile     *file;
  gboolean        visible = TRUE;

  /* if show_hidden is TRUE, nothing is filtered */
  if (G_LIKELY (!view->show_hidden))
    {
      /* check if we should display this row */
      gtk_tree_model_get (filter, iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
      if (G_LIKELY (file != NULL))
        {
          /* we display all non-hidden file and hidden files that are ancestors of the current directory */
          visible = !thunar_file_is_hidden (file) || (view->current_directory == file)
                 || (view->current_directory != NULL && thunar_file_is_ancestor (view->current_directory, file));

          /* release the reference on the file */
          g_object_unref (G_OBJECT (file));
        }
    }

  return visible;
}



static gboolean
thunar_tree_view_selection_func (GtkTreeSelection *selection,
                                 GtkTreeModel     *filter,
                                 GtkTreePath      *path,
                                 gboolean          path_currently_selected,
                                 gpointer          user_data)
{
  GtkTreeIter iter;
  ThunarFile *file;
  gboolean    result = FALSE;

  /* every row may be unselected at any time */
  if (G_UNLIKELY (path_currently_selected))
    return TRUE;

  /* determine the iterator for the path */
  if (gtk_tree_model_get_iter (filter, &iter, path))
    {
      /* determine the file for the iterator */
      gtk_tree_model_get (filter, &iter, THUNAR_TREE_MODEL_COLUMN_FILE, &file, -1);
      if (G_LIKELY (file != NULL))
        {
          /* only rows with files can be selected */
          result = TRUE;

          /* release file */
          g_object_unref (G_OBJECT (file));
        }
    }

  return result;
}



static gboolean
thunar_tree_view_expand_idle (gpointer user_data)
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
thunar_tree_view_expand_idle_destroy (gpointer user_data)
{
  THUNAR_TREE_VIEW (user_data)->expand_idle_id = -1;
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
  g_return_val_if_fail (THUNAR_IS_TREE_VIEW (view), FALSE);
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
  GtkTreeModel *filter;

  g_return_if_fail (THUNAR_IS_TREE_VIEW (view));

  /* normalize the value */
  show_hidden = !!show_hidden;

  /* check if we have a new setting */
  if (view->show_hidden != show_hidden)
    {
      /* apply the new setting */
      view->show_hidden = show_hidden;

      /* update the filter */
      filter = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filter));

      /* notify listeners */
      g_object_notify (G_OBJECT (view), "show-hidden");
    }
}


