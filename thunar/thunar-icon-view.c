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

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-icon-view-ui.h>



static void         thunar_icon_view_class_init             (ThunarIconViewClass *klass);
static void         thunar_icon_view_init                   (ThunarIconView      *icon_view);
static AtkObject   *thunar_icon_view_get_accessible         (GtkWidget           *widget);
static void         thunar_icon_view_connect_ui_manager     (ThunarStandardView  *standard_view,
                                                             GtkUIManager        *ui_manager);
static void         thunar_icon_view_disconnect_ui_manager  (ThunarStandardView  *standard_view,
                                                             GtkUIManager        *ui_manager);
static GList       *thunar_icon_view_get_selected_items     (ThunarStandardView  *standard_view);
static void         thunar_icon_view_select_all             (ThunarStandardView  *standard_view);
static void         thunar_icon_view_unselect_all           (ThunarStandardView  *standard_view);
static void         thunar_icon_view_select_path            (ThunarStandardView  *standard_view,
                                                             GtkTreePath         *path);
static void         thunar_icon_view_set_cursor             (ThunarStandardView  *standard_view,
                                                             GtkTreePath         *path,
                                                             gboolean             start_editing);
static void         thunar_icon_view_scroll_to_path         (ThunarStandardView  *standard_view,
                                                             GtkTreePath         *path);
static GtkTreePath *thunar_icon_view_get_path_at_pos        (ThunarStandardView  *standard_view,
                                                             gint                 x,
                                                             gint                 y);
static void         thunar_icon_view_highlight_path         (ThunarStandardView  *standard_view,
                                                             GtkTreePath         *path);
static void         thunar_icon_view_action_sort            (GtkAction           *action,
                                                             GtkAction           *current,
                                                             ThunarStandardView  *standard_view);
static gboolean     thunar_icon_view_button_press_event     (ExoIconView         *view,
                                                             GdkEventButton      *event,
                                                             ThunarIconView      *icon_view);
static gboolean     thunar_icon_view_key_press_event        (ExoIconView         *view,
                                                             GdkEventKey         *event,
                                                             ThunarIconView      *icon_view);
static void         thunar_icon_view_item_activated         (ExoIconView         *view,
                                                             GtkTreePath         *path,
                                                             ThunarIconView      *icon_view);
static void         thunar_icon_view_sort_column_changed    (GtkTreeSortable     *sortable,
                                                             ThunarIconView      *icon_view);



struct _ThunarIconViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarIconView
{
  ThunarStandardView __parent__;

  gint ui_merge_id;
};



static const GtkActionEntry action_entries[] =
{
  { "arrange-items-menu", NULL, N_ ("Arran_ge Items"), NULL, NULL, NULL, },
};

static const GtkRadioActionEntry column_action_entries[] =
{
  { "sort-by-name", NULL, N_ ("Sort By _Name"), NULL, N_ ("Keep items sorted by their name in rows"), THUNAR_LIST_MODEL_COLUMN_NAME, },
  { "sort-by-size", NULL, N_ ("Sort By _Size"), NULL, N_ ("Keep items sorted by their size in rows"), THUNAR_LIST_MODEL_COLUMN_SIZE, },
  { "sort-by-type", NULL, N_ ("Sort By _Type"), NULL, N_ ("Keep items sorted by their type in rows"), THUNAR_LIST_MODEL_COLUMN_TYPE, },
  { "sort-by-mtime", NULL, N_ ("Sort By Modification _Date"), NULL, N_ ("Keep items sorted by their modification date in rows"), THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED, },
};

static const GtkRadioActionEntry order_action_entries[] =
{
  { "sort-ascending", NULL, N_ ("_Ascending"), NULL, N_ ("Sort items in ascending order"), GTK_SORT_ASCENDING, },
  { "sort-descending", NULL, N_ ("_Descending"), NULL, N_ ("Sort items in descending order"), GTK_SORT_DESCENDING, },
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
  thunarstandard_view_class->connect_ui_manager = thunar_icon_view_connect_ui_manager;
  thunarstandard_view_class->disconnect_ui_manager = thunar_icon_view_disconnect_ui_manager;
  thunarstandard_view_class->get_selected_items = thunar_icon_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_icon_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_icon_view_unselect_all;
  thunarstandard_view_class->select_path = thunar_icon_view_select_path;
  thunarstandard_view_class->set_cursor = thunar_icon_view_set_cursor;
  thunarstandard_view_class->scroll_to_path = thunar_icon_view_scroll_to_path;
  thunarstandard_view_class->get_path_at_pos = thunar_icon_view_get_path_at_pos;
  thunarstandard_view_class->highlight_path = thunar_icon_view_highlight_path;
}



static void
thunar_icon_view_init (ThunarIconView *icon_view)
{
  GtkWidget *view;

  /* create the real view */
  view = exo_icon_view_new ();
  g_signal_connect (G_OBJECT (view), "button-press-event", G_CALLBACK (thunar_icon_view_button_press_event), icon_view);
  g_signal_connect (G_OBJECT (view), "key-press-event", G_CALLBACK (thunar_icon_view_key_press_event), icon_view);
  g_signal_connect (G_OBJECT (view), "item-activated", G_CALLBACK (thunar_icon_view_item_activated), icon_view);
  g_signal_connect_swapped (G_OBJECT (view), "selection-changed", G_CALLBACK (thunar_standard_view_selection_changed), icon_view);
  gtk_container_add (GTK_CONTAINER (icon_view), view);
  gtk_widget_show (view);

  /* initialize the icon view properties */
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (view), GTK_SELECTION_MULTIPLE);

  /* add the icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (icon_view)->icon_renderer),
                "follow-state", TRUE,
                "size", 48,
                "ypad", 3u,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (icon_view)->icon_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (icon_view)->icon_renderer,
                                 "file", THUNAR_LIST_MODEL_COLUMN_FILE);

  /* add the name renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (icon_view)->name_renderer),
                "follow-state", TRUE,
                "wrap-mode", PANGO_WRAP_WORD_CHAR,
                "wrap-width", 128,
                "yalign", 0.0f,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (icon_view)->name_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (icon_view)->name_renderer,
                                 "text", THUNAR_LIST_MODEL_COLUMN_NAME);

  /* setup the icon view actions */
  gtk_action_group_add_actions (THUNAR_STANDARD_VIEW (icon_view)->action_group,
                                action_entries, G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (icon_view));
  gtk_action_group_add_radio_actions (THUNAR_STANDARD_VIEW (icon_view)->action_group, column_action_entries,
                                      G_N_ELEMENTS (column_action_entries), THUNAR_LIST_MODEL_COLUMN_NAME,
                                      G_CALLBACK (thunar_icon_view_action_sort), icon_view);
  gtk_action_group_add_radio_actions (THUNAR_STANDARD_VIEW (icon_view)->action_group, order_action_entries,
                                      G_N_ELEMENTS (order_action_entries), GTK_SORT_ASCENDING,
                                      G_CALLBACK (thunar_icon_view_action_sort), icon_view);

  /* we need to listen to sort column changes to sync the menu items */
  g_signal_connect (G_OBJECT (THUNAR_STANDARD_VIEW (icon_view)->model), "sort-column-changed",
                    G_CALLBACK (thunar_icon_view_sort_column_changed), icon_view);
  thunar_icon_view_sort_column_changed (GTK_TREE_SORTABLE (THUNAR_STANDARD_VIEW (icon_view)->model), icon_view);
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



static void
thunar_icon_view_connect_ui_manager (ThunarStandardView *standard_view,
                                     GtkUIManager       *ui_manager)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (standard_view);
  GError         *error = NULL;

  icon_view->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_icon_view_ui,
                                                              thunar_icon_view_ui_length, &error);
  if (G_UNLIKELY (icon_view->ui_merge_id == 0))
    {
      g_error ("Failed to merge ThunarIconView menus: %s", error->message);
      g_error_free (error);
    }
}



static void
thunar_icon_view_disconnect_ui_manager (ThunarStandardView *standard_view,
                                        GtkUIManager       *ui_manager)
{
  gtk_ui_manager_remove_ui (ui_manager, THUNAR_ICON_VIEW (standard_view)->ui_merge_id);
}



static GList*
thunar_icon_view_get_selected_items (ThunarStandardView *standard_view)
{
  return exo_icon_view_get_selected_items (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_icon_view_select_all (ThunarStandardView *standard_view)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));
  exo_icon_view_select_all (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_icon_view_unselect_all (ThunarStandardView *standard_view)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));
  exo_icon_view_unselect_all (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_icon_view_select_path (ThunarStandardView *standard_view,
                              GtkTreePath        *path)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));
  exo_icon_view_select_path (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path);
}



static void
thunar_icon_view_set_cursor (ThunarStandardView *standard_view,
                             GtkTreePath        *path,
                             gboolean            start_editing)
{
  GtkCellRendererMode mode;

  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));

  /* make sure the name renderer is editable */
  mode = standard_view->name_renderer->mode;
  standard_view->name_renderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;

  /* tell the icon view to start editing the given item */
  exo_icon_view_set_cursor (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, standard_view->name_renderer, start_editing);

  /* reset the name renderer mode */
  standard_view->name_renderer->mode = mode;
}



static void
thunar_icon_view_scroll_to_path (ThunarStandardView *standard_view,
                                 GtkTreePath        *path)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));
  exo_icon_view_scroll_to_path (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, FALSE, 0.0f, 0.0f);
}



static GtkTreePath*
thunar_icon_view_get_path_at_pos (ThunarStandardView *standard_view,
                                  gint                x,
                                  gint                y)
{
  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (standard_view), NULL);
  return exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), x, y);
}



static void
thunar_icon_view_highlight_path (ThunarStandardView *standard_view,
                                 GtkTreePath        *path)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (standard_view));
  exo_icon_view_set_drag_dest_item (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, EXO_ICON_VIEW_DROP_INTO);
}



static void
thunar_icon_view_action_sort (GtkAction          *action,
                              GtkAction          *current,
                              ThunarStandardView *standard_view)
{
  GtkSortType order;
  gint        column;

  /* query the new sort column id */
  action = gtk_action_group_get_action (standard_view->action_group, "sort-by-name");
  column = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

  /* query the new sort order */
  action = gtk_action_group_get_action (standard_view->action_group, "sort-ascending");
  order = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

  /* apply the new settings */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), column, order);
}



static gboolean
thunar_icon_view_button_press_event (ExoIconView    *view,
                                     GdkEventButton *event,
                                     ThunarIconView *icon_view)
{
  GtkTreePath *path;

  /* open the context menu on right clicks */
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      if (exo_icon_view_get_item_at_pos (view, event->x, event->y, &path, NULL))
        {
          /* select the path on which the user clicked if not selected yet */
          if (!exo_icon_view_path_is_selected (view, path))
            {
              /* we don't unselect all other items if Control is active */
              if ((event->state & GDK_CONTROL_MASK) == 0)
                exo_icon_view_unselect_all (view);
              exo_icon_view_select_path (view, path);
            }
          gtk_tree_path_free (path);

          /* queue the menu popup */
          thunar_standard_view_queue_popup (THUNAR_STANDARD_VIEW (icon_view), event);
        }
      else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
          /* user clicked on an empty area, so we unselect everything
           * to make sure that the folder context menu is opened.
           */
          exo_icon_view_unselect_all (view);
      
          /* open the context menu */
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (icon_view), event->button, event->time);
        }

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_icon_view_key_press_event (ExoIconView    *view,
                                  GdkEventKey    *event,
                                  ThunarIconView *icon_view)
{
  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (icon_view), 0, event->time);
      return TRUE;
    }

  return FALSE;
}



static void
thunar_icon_view_item_activated (ExoIconView    *view,
                                 GtkTreePath    *path,
                                 ThunarIconView *icon_view)
{
  GtkAction *action;

  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));

  /* be sure to have only the double clicked item selected */
  exo_icon_view_unselect_all (view);
  exo_icon_view_select_path (view, path);

  /* emit the "open" action */
  action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (icon_view)->action_group, "open");
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);
}



static void
thunar_icon_view_sort_column_changed (GtkTreeSortable *sortable,
                                      ThunarIconView  *icon_view)
{
  GtkSortType order;
  GtkAction  *action;
  gint        column;

  if (gtk_tree_sortable_get_sort_column_id (sortable, &column, &order))
    {
      /* apply the new sort column */
      action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (icon_view)->action_group, "sort-by-name");
      exo_gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), column);

      /* apply the new sort order */
      action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (icon_view)->action_group, "sort-ascending");
      exo_gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), order);
    }
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


