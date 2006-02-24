/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-shortcuts-icon-renderer.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-shortcuts-view.h>
#include <thunar/thunar-text-renderer.h>



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



static void         thunar_shortcuts_view_class_init             (ThunarShortcutsViewClass *klass);
static void         thunar_shortcuts_view_init                   (ThunarShortcutsView      *view);
static void         thunar_shortcuts_view_finalize               (GObject                  *object);
static gboolean     thunar_shortcuts_view_button_press_event     (GtkWidget                *widget,
                                                                  GdkEventButton           *event);
static gboolean     thunar_shortcuts_view_button_release_event   (GtkWidget                *widget,
                                                                  GdkEventButton           *event);
static gboolean     thunar_shortcuts_view_motion_notify_event    (GtkWidget                *widget,
                                                                  GdkEventMotion           *event);
static void         thunar_shortcuts_view_drag_data_received     (GtkWidget                *widget,
                                                                  GdkDragContext           *context,
                                                                  gint                      x,
                                                                  gint                      y,
                                                                  GtkSelectionData         *selection_data,
                                                                  guint                     info,
                                                                  guint                     time);
static gboolean     thunar_shortcuts_view_drag_drop              (GtkWidget                *widget,
                                                                  GdkDragContext           *context,
                                                                  gint                      x,
                                                                  gint                      y,
                                                                  guint                     time);
static gboolean     thunar_shortcuts_view_drag_motion            (GtkWidget                *widget,
                                                                  GdkDragContext           *context,
                                                                  gint                      x,
                                                                  gint                      y,
                                                                  guint                     time);
static void         thunar_shortcuts_view_drag_begin             (GtkWidget                *widget,
                                                                  GdkDragContext           *context);
static void         thunar_shortcuts_view_row_activated          (GtkTreeView              *tree_view,
                                                                  GtkTreePath              *path,
                                                                  GtkTreeViewColumn        *column);
static void         thunar_shortcuts_view_remove_activated       (GtkWidget                *item,
                                                                  ThunarShortcutsView      *view);
static void         thunar_shortcuts_view_rename_activated       (GtkWidget                *item,
                                                                  ThunarShortcutsView      *view);
static void         thunar_shortcuts_view_renamed                (GtkCellRenderer          *renderer,
                                                                  const gchar              *path_string,
                                                                  const gchar              *text,
                                                                  ThunarShortcutsView      *view);
static GtkTreePath *thunar_shortcuts_view_compute_drop_position  (ThunarShortcutsView      *view,
                                                                  gint                      x,
                                                                  gint                      y);
static void         thunar_shortcuts_view_drop_uri_list          (ThunarShortcutsView      *view,
                                                                  const gchar              *uri_list,
                                                                  GtkTreePath              *dst_path);
static void         thunar_shortcuts_view_open                   (ThunarShortcutsView      *view);
static void         thunar_shortcuts_view_open_in_new_window     (ThunarShortcutsView      *view);
static gboolean     thunar_shortcuts_view_eject                  (ThunarShortcutsView      *view);
static gboolean     thunar_shortcuts_view_mount                  (ThunarShortcutsView      *view);
static gboolean     thunar_shortcuts_view_unmount                (ThunarShortcutsView      *view);
static gboolean     thunar_shortcuts_view_separator_func         (GtkTreeModel             *model,
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

  /* TRUE if the next button_release_event should
   * fire a "row-activated" (single click support).
   */
  gboolean button_release_activates;

#if GTK_CHECK_VERSION(2,8,0)
  /* id of the signal used to queue a resize on the
   * column whenever the shortcuts icon size is changed.
   */
  gint queue_resize_signal_id;
#endif
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



static GObjectClass *thunar_shortcuts_view_parent_class;
static guint         view_signals[LAST_SIGNAL];



GType
thunar_shortcuts_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarShortcutsViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_shortcuts_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarShortcutsView),
        0,
        (GInstanceInitFunc) thunar_shortcuts_view_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_TREE_VIEW, I_("ThunarShortcutsView"), &info, 0);
    }

  return type;
}



static void
thunar_shortcuts_view_class_init (ThunarShortcutsViewClass *klass)
{
  GtkTreeViewClass *gtktree_view_class;
  GtkWidgetClass   *gtkwidget_class;
  GObjectClass     *gobject_class;

  /* determine the parent type class */
  thunar_shortcuts_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_view_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->button_press_event = thunar_shortcuts_view_button_press_event;
  gtkwidget_class->button_release_event = thunar_shortcuts_view_button_release_event;
  gtkwidget_class->motion_notify_event = thunar_shortcuts_view_motion_notify_event;
  gtkwidget_class->drag_data_received = thunar_shortcuts_view_drag_data_received;
  gtkwidget_class->drag_drop = thunar_shortcuts_view_drag_drop;
  gtkwidget_class->drag_motion = thunar_shortcuts_view_drag_motion;
  gtkwidget_class->drag_begin = thunar_shortcuts_view_drag_begin;

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
#if GTK_CHECK_VERSION(2,8,0)
  view->queue_resize_signal_id = g_signal_connect_swapped (G_OBJECT (view->preferences), "notify::shortcuts-icon-size",
                                                           G_CALLBACK (gtk_tree_view_column_queue_resize), column);
#endif

  /* allocate the special icon renderer */
  renderer = thunar_shortcuts_icon_renderer_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "file", THUNAR_SHORTCUTS_MODEL_COLUMN_FILE,
                                       "volume", THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME,
                                       NULL);

  /* sync the "emblems" property of the icon renderer with the "shortcuts-icon-emblems" preference
   * and the "size" property of the renderer with the "shortcuts-icon-size" preference.
   */
  exo_binding_new (G_OBJECT (view->preferences), "shortcuts-icon-size", G_OBJECT (renderer), "size");
  exo_binding_new (G_OBJECT (view->preferences), "shortcuts-icon-emblems", G_OBJECT (renderer), "emblems");

  /* allocate the text renderer */
  renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (view->preferences), "misc-single-click", G_OBJECT (renderer), "follow-prelit");
  g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (thunar_shortcuts_view_renamed), view);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
                                       NULL);

  /* enable drag support for the shortcuts view (actually used to support reordering) */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (view), GDK_BUTTON1_MASK, drag_targets,
                                          G_N_ELEMENTS (drag_targets), GDK_ACTION_MOVE);

  /* enable drop support for the shortcuts view (both internal reordering
   * and adding new shortcuts from other widgets)
   */
  gtk_drag_dest_set (GTK_WIDGET (view), GTK_DEST_DEFAULT_ALL,
                     drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

  /* setup a row separator function to tell GtkTreeView about the separator */
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), thunar_shortcuts_view_separator_func, NULL, NULL);
}



static void
thunar_shortcuts_view_finalize (GObject *object)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (object);

  /* disconnect the queue resize signal handler */
#if GTK_CHECK_VERSION(2,8,0)
  g_signal_handler_disconnect (G_OBJECT (view->preferences), view->queue_resize_signal_id);
#endif

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
  ThunarVfsVolume     *volume;
  GtkTreeModel        *model;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GtkTooltips         *tooltips;
  ThunarFile          *file;
  GtkWidget           *window;
  GtkWidget           *image;
  GtkWidget           *menu;
  GtkWidget           *item;
  GMainLoop           *loop;
  gboolean             single_click;
  gboolean             mutable;
  gboolean             result;
  GList               *actions;
  GList               *lp;

  /* check if we are in single-click mode */
  g_object_get (G_OBJECT (view->preferences), "misc-single-click", &single_click, NULL);

  /* check if the next button-release-event should activate the selected row (single click support) */
  view->button_release_activates = (single_click && event->type == GDK_BUTTON_PRESS && event->button == 1
                                 && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0);

  /* ignore all kinds of double click events in single-click mode */
  if (G_UNLIKELY (single_click && event->type == GDK_2BUTTON_PRESS))
    return TRUE;

  /* let the widget process the event first (handles focussing and scrolling) */
  result = (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_press_event) (widget, event);

  /* check if we have a right-click event */
  if (G_LIKELY (event->button != 3))
    {
      /* we open a new window for single/double-middle-clicks */
      if (G_UNLIKELY (event->type == (single_click ? GDK_BUTTON_PRESS : GDK_2BUTTON_PRESS) && event->button == 2))
        {
          /* just open in new window */
          thunar_shortcuts_view_open_in_new_window (view);
        }

      return result;
    }

  /* resolve the path at the cursor position */
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* allocate a new tooltips object */
      tooltips = gtk_tooltips_new ();
      exo_gtk_object_ref_sink (GTK_OBJECT (tooltips));

      /* determine the iterator for the selected row */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
      gtk_tree_model_get_iter (model, &iter, path);

      /* check whether the shortcut at the given path is mutable */
      gtk_tree_model_get (model, &iter,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume,
                          THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE, &mutable, 
                          -1);

      /* prepare the internal loop */
      loop = g_main_loop_new (NULL, FALSE);

      /* prepare the popup menu */
      menu = gtk_menu_new ();
      gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (widget));
      g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);
      exo_gtk_object_ref_sink (GTK_OBJECT (menu));

      /* append the "Open" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_open), widget);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* set the stock icon */
      image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);

      /* append the "Open in New Window" menu action */
      item = gtk_image_menu_item_new_with_mnemonic (_("Open in New Window"));
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_open_in_new_window), widget);
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
          gtk_widget_set_sensitive (item, !thunar_vfs_volume_is_mounted (volume));
          g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_mount), widget);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* check if the volume is a disc */
          if (thunar_vfs_volume_is_disc (volume))
            {
              /* append the "Eject Volume" menu action */
              item = gtk_image_menu_item_new_with_mnemonic (_("E_ject Volume"));
              g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_eject), widget);
              gtk_widget_set_sensitive (item, thunar_vfs_volume_is_ejectable (volume));
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }
          else
            {
              /* append the "Unmount Volume" menu item */
              item = gtk_image_menu_item_new_with_mnemonic (_("_Unmount Volume"));
              gtk_widget_set_sensitive (item, thunar_vfs_volume_is_mounted (volume));
              g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_unmount), widget);
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }

          /* append a menu separator */
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* append the custom actions for the selected file (if any) */
      if (G_LIKELY (file != NULL))
        {
          /* determine the toplevel window */
          window = gtk_widget_get_toplevel (widget);

          /* determine the actions for the selected file */
          actions = thunar_file_get_actions (file, window);

          /* check if we have any actions */
          if (G_LIKELY (actions != NULL))
            {
              /* append the actions */
              for (lp = actions; lp != NULL; lp = lp->next)
                {
                  /* append the menu item */
                  item = gtk_action_create_menu_item (GTK_ACTION (lp->data));
                  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
                  gtk_widget_show (item);

                  /* release the reference on the action */
                  g_object_unref (G_OBJECT (lp->data));
                }
              g_list_free (actions);

              /* append a menu separator */
              item = gtk_separator_menu_item_new ();
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }
        }

      /* append the remove menu item */
      item = gtk_image_menu_item_new_with_mnemonic (_("_Remove Shortcut"));
      g_object_set_data_full (G_OBJECT (item), I_("thunar-shortcuts-row"),
                              gtk_tree_row_reference_new (model, path),
                              (GDestroyNotify) gtk_tree_row_reference_free);
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_remove_activated), widget);
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
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_shortcuts_view_rename_activated), widget);
      gtk_widget_set_sensitive (item, mutable);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* append a menu separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* append the "Display icon emblems" item */
      item = gtk_check_menu_item_new_with_mnemonic (_("Display _Emblem Icons"));
      exo_mutual_binding_new (G_OBJECT (view->preferences), "shortcuts-icon-emblems", G_OBJECT (item), "active");
      gtk_tooltips_set_tip (tooltips, item, _("Display emblem icons in the shortcuts list for all files for which "
                                              "emblems have been defined in the file properties dialog."), NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* run the internal loop */
      gtk_grab_add (menu);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, event->time);
      g_main_loop_run (loop);
      gtk_grab_remove (menu);

      /* clean up */
      if (G_LIKELY (file != NULL))
        g_object_unref (G_OBJECT (file));
      if (G_UNLIKELY (volume != NULL))
        g_object_unref (G_OBJECT (volume));
      g_object_unref (G_OBJECT (tooltips));
      g_object_unref (G_OBJECT (menu));
      gtk_tree_path_free (path);
      g_main_loop_unref (loop);

      /* we effectively handled the event */
      return TRUE;
    }

  return result;
}



static gboolean
thunar_shortcuts_view_button_release_event (GtkWidget      *widget,
                                            GdkEventButton *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeViewColumn   *column;
  GtkTreePath         *path;
  gboolean             single_click;

  /* check if we're in single-click mode */
  g_object_get (G_OBJECT (view->preferences), "misc-single-click", &single_click, NULL);
  if (G_UNLIKELY (single_click && view->button_release_activates))
    {
      /* reset button_release_activates state */
      view->button_release_activates = FALSE;

      /* determine the path to the row that should be activated */
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), event->x, event->y, &path, &column, NULL, NULL))
        {
          /* emit row-activated for the determined row */
          gtk_tree_view_row_activated (GTK_TREE_VIEW (view), path, column);

          /* cleanup */
          gtk_tree_path_free (path);
        }
    }

  return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->button_release_event) (widget, event);
}



static gboolean
thunar_shortcuts_view_motion_notify_event (GtkWidget      *widget,
                                           GdkEventMotion *event)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkCursor           *cursor;
  gboolean             single_click;

  /* check if we're in single-click mode */
  g_object_get (G_OBJECT (view->preferences), "misc-single-click", &single_click, NULL);

  /* check if we are in single-click mode and have a path for the mouse pointer position */
  if (G_UNLIKELY (single_click && gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), event->x, event->y, NULL, NULL, NULL, NULL)))
    {
      /* setup hand cursor */
      cursor = gdk_cursor_new (GDK_HAND2);
      gdk_window_set_cursor (widget->window, cursor);
      gdk_cursor_unref (cursor);
    }
  else
    {
      /* reset the cursor to its default */
      gdk_window_set_cursor (widget->window, NULL);
    }

  return (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->motion_notify_event) (widget, event);
}



static void
thunar_shortcuts_view_drag_data_received (GtkWidget        *widget,
                                          GdkDragContext   *context,
                                          gint              x,
                                          gint              y,
                                          GtkSelectionData *selection_data,
                                          guint             info,
                                          guint             time)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);
  GtkTreeSelection     *selection;
  GtkTreeModel         *model;
  GtkTreePath          *dst_path;
  GtkTreePath          *src_path;
  GtkTreeIter           iter;

  g_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* compute the drop position */
  dst_path = thunar_shortcuts_view_compute_drop_position (view, x, y);

  if (selection_data->target == gdk_atom_intern ("text/uri-list", FALSE))
    {
      thunar_shortcuts_view_drop_uri_list (view, (gchar *) selection_data->data, dst_path);
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
          if (!thunar_shortcuts_model_drop_possible (THUNAR_SHORTCUTS_MODEL (model), dst_path))
            gtk_tree_path_next (dst_path);

          /* perform the move */
          src_path = gtk_tree_model_get_path (model, &iter);
          thunar_shortcuts_model_move (THUNAR_SHORTCUTS_MODEL (model), src_path, dst_path);
          gtk_tree_path_free (src_path);
        }
    }

  gtk_tree_path_free (dst_path);
}



static gboolean
thunar_shortcuts_view_drag_drop (GtkWidget      *widget,
                                 GdkDragContext *context,
                                 gint            x,
                                 gint            y,
                                 guint           time)
{
  g_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (widget), FALSE);
  return TRUE;
}



static gboolean
thunar_shortcuts_view_drag_motion (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           time)
{
  GtkTreeViewDropPosition position = GTK_TREE_VIEW_DROP_BEFORE;
  ThunarShortcutsView    *view = THUNAR_SHORTCUTS_VIEW (widget);
  GdkDragAction           action;
  GtkTreeModel           *model;
  GtkTreePath            *path;

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

  /* highlight the appropriate row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (view), path, position);
  gtk_tree_path_free (path);

  gdk_drag_status (context, action, time);
  return TRUE;
}



static void
thunar_shortcuts_view_drag_begin (GtkWidget      *widget,
                                  GdkDragContext *context)
{
  ThunarShortcutsView *view = THUNAR_SHORTCUTS_VIEW (widget);

  /* Do not activate the selected row on the next button_release_event,
   * as the user started a drag operation with the mouse instead.
   */
  view->button_release_activates = FALSE;

  (*GTK_WIDGET_CLASS (thunar_shortcuts_view_parent_class)->drag_begin) (widget, context);
}



static void
thunar_shortcuts_view_row_activated (GtkTreeView       *tree_view,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column)
{
  /* call the row-activated method in the parent class */
  if (GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated != NULL)
    (*GTK_TREE_VIEW_CLASS (thunar_shortcuts_view_parent_class)->row_activated) (tree_view, path, column);

  /* open the folder referenced by the shortcut */
  thunar_shortcuts_view_open (THUNAR_SHORTCUTS_VIEW (tree_view));
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
  GtkCellRendererMode  mode;
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
      renderers = gtk_tree_view_column_get_cell_renderers (column);
      renderer = g_list_nth_data (renderers, 1);

      /* make sure the text renderer is editable */
      mode = renderer->mode;
      renderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;

      /* tell the tree view to start editing the given row */
      gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (view), path, column, renderer, TRUE);

      /* reset the text renderer mode */
      renderer->mode = mode;

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

  /* perform the rename */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (gtk_tree_model_get_iter_from_string (model, &iter, path_string))
    thunar_shortcuts_model_rename (THUNAR_SHORTCUTS_MODEL (model), &iter, text);
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

  g_return_val_if_fail (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) != NULL, NULL);
  g_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), NULL);

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
                                     const gchar         *uri_list,
                                     GtkTreePath         *dst_path)
{
  GtkTreeModel *model;
  ThunarFile   *file;
  GError       *error = NULL;
  gchar        *display_string;
  gchar        *uri_string;
  GList        *path_list;
  GList        *lp;

  path_list = thunar_vfs_path_list_from_string (uri_list, &error);
  if (G_LIKELY (error == NULL))
    {
      /* process the URIs one-by-one and stop on error */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          file = thunar_file_get_for_path (lp->data, &error);
          if (G_UNLIKELY (file == NULL))
            break;

          /* make sure, that only directories gets added to the shortcuts list */
          if (G_UNLIKELY (!thunar_file_is_directory (file)))
            {
              uri_string = thunar_vfs_path_dup_string (lp->data);
              display_string = g_filename_display_name (uri_string);
              g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR,
                           _("The path \"%s\" does not refer to a directory"),
                           display_string);
              g_object_unref (G_OBJECT (file));
              g_free (display_string);
              g_free (uri_string);
              break;
            }

          thunar_shortcuts_model_add (THUNAR_SHORTCUTS_MODEL (model), dst_path, file);
          g_object_unref (G_OBJECT (file));
          gtk_tree_path_next (dst_path);
        }

      thunar_vfs_path_list_free (path_list);
    }

  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (GTK_WIDGET (view), error, _("Failed to add new shortcut"));

      /* release the error */
      g_error_free (error);
    }
}



static void
thunar_shortcuts_view_open (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  ThunarFile       *file;

  g_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* make sure to mount the volume first (if it's a volume) */
  if (!thunar_shortcuts_view_mount (view))
    return;

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the file for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
      if (G_LIKELY (file != NULL))
        {
          /* invoke the signal to change to that folder */
          g_signal_emit (G_OBJECT (view), view_signals[SHORTCUT_ACTIVATED], 0, file);
          g_object_unref (G_OBJECT (file));
        }
    }
}



static void
thunar_shortcuts_view_open_in_new_window (ThunarShortcutsView *view)
{
  ThunarApplication *application;
  GtkTreeSelection  *selection;
  GtkTreeModel      *model;
  GtkTreeIter        iter;
  ThunarFile        *file;

  g_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));

  /* make sure to mount the volume first (if it's a volume) */
  if (!thunar_shortcuts_view_mount (view))
    return;

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the file for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_FILE, &file, -1);
      if (G_LIKELY (file != NULL))
        {
          /* open a new window for the shortcut target folder */
          application = thunar_application_get ();
          thunar_application_open_window (application, file, gtk_widget_get_screen (GTK_WIDGET (view)));
          g_object_unref (G_OBJECT (application));
          g_object_unref (G_OBJECT (file));
        }
    }
}



static gboolean
thunar_shortcuts_view_eject (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  ThunarVfsVolume  *volume;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *window;
  gboolean          result = TRUE;
  GError           *error = NULL;

  g_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), FALSE);

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the volume for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, -1);
      if (G_UNLIKELY (volume != NULL))
        {
          /* determine the toplevel window */
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* try to eject the volume */
          result = thunar_vfs_volume_eject (volume, window, &error);
          if (G_UNLIKELY (!result))
            {
              /* display an error dialog to inform the user */
              thunar_dialogs_show_error (window, error, _("Failed to eject \"%s\""), thunar_vfs_volume_get_name (volume));
              g_error_free (error);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (volume));
        }
    }

  return result;
}



static gboolean
thunar_shortcuts_view_mount (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  ThunarVfsVolume  *volume;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *window;
  gboolean          result = TRUE;
  GError           *error = NULL;

  g_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), FALSE);

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the volume for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, -1);
      if (G_UNLIKELY (volume != NULL))
        {
          /* check if the volume isn't already mounted */
          if (G_LIKELY (!thunar_vfs_volume_is_mounted (volume)))
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

          /* cleanup */
          g_object_unref (G_OBJECT (volume));
        }
    }

  return result;
}



static gboolean
thunar_shortcuts_view_unmount (ThunarShortcutsView *view)
{
  GtkTreeSelection *selection;
  ThunarVfsVolume  *volume;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *window;
  gboolean          result = TRUE;
  GError           *error = NULL;

  g_return_val_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view), FALSE);

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the volume for the shortcut at the given tree iterator */
      gtk_tree_model_get (model, &iter, THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME, &volume, -1);
      if (G_UNLIKELY (volume != NULL))
        {
          /* determine the toplevel window */
          window = gtk_widget_get_toplevel (GTK_WIDGET (view));

          /* try to unmount the volume */
          result = thunar_vfs_volume_unmount (volume, window, &error);
          if (G_UNLIKELY (!result))
            {
              /* display an error dialog to inform the user */
              thunar_dialogs_show_error (window, error, _("Failed to unmount \"%s\""), thunar_vfs_volume_get_name (volume));
              g_error_free (error);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (volume));
        }
    }

  return result;
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

  g_return_if_fail (THUNAR_IS_SHORTCUTS_VIEW (view));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* clear the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_unselect_all (selection);

  /* try to lookup a tree iter for the given file */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (thunar_shortcuts_model_iter_for_file (THUNAR_SHORTCUTS_MODEL (model), file, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


