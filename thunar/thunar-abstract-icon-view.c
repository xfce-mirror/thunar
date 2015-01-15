/* vi:set et ai sw=2 sts=2 ts=2: */
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-abstract-icon-view.h>
#include <thunar/thunar-abstract-icon-view-ui.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>



#define THUNAR_ABSTRACT_ICON_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_ABSTRACT_ICON_VIEW, ThunarAbstractIconViewPrivate))



static void         thunar_abstract_icon_view_style_set             (GtkWidget                    *widget,
                                                                     GtkStyle                     *previous_style);
static void         thunar_abstract_icon_view_connect_ui_manager    (ThunarStandardView           *standard_view,
                                                                     GtkUIManager                 *ui_manager);
static void         thunar_abstract_icon_view_disconnect_ui_manager (ThunarStandardView           *standard_view,
                                                                     GtkUIManager                 *ui_manager);
static GList       *thunar_abstract_icon_view_get_selected_items    (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_select_all            (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_unselect_all          (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_selection_invert      (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_select_path           (ThunarStandardView           *standard_view,
                                                                     GtkTreePath                  *path);
static void         thunar_abstract_icon_view_set_cursor            (ThunarStandardView           *standard_view,
                                                                     GtkTreePath                  *path,
                                                                     gboolean                      start_editing);
static void         thunar_abstract_icon_view_scroll_to_path        (ThunarStandardView           *standard_view,
                                                                     GtkTreePath                  *path,
                                                                     gboolean                      use_align,
                                                                     gfloat                        row_align,
                                                                     gfloat                        col_align);
static GtkTreePath *thunar_abstract_icon_view_get_path_at_pos       (ThunarStandardView           *standard_view,
                                                                     gint                          x,
                                                                     gint                          y);
static gboolean     thunar_abstract_icon_view_get_visible_range     (ThunarStandardView           *standard_view,
                                                                     GtkTreePath                 **start_path,
                                                                     GtkTreePath                 **end_path);
static void         thunar_abstract_icon_view_highlight_path        (ThunarStandardView           *standard_view,
                                                                     GtkTreePath                  *path);
static GtkAction   *thunar_abstract_icon_view_gesture_action        (ThunarAbstractIconView       *abstract_icon_view);
static void         thunar_abstract_icon_view_action_sort           (GtkAction                    *action,
                                                                     GtkAction                    *current,
                                                                     ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_notify_model          (ExoIconView                  *view,
                                                                     GParamSpec                   *pspec,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_button_press_event    (ExoIconView                  *view,
                                                                     GdkEventButton               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_button_release_event  (ExoIconView                  *view,
                                                                     GdkEventButton               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_expose_event          (ExoIconView                  *view,
                                                                     GdkEventExpose               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_key_press_event       (ExoIconView                  *view,
                                                                     GdkEventKey                  *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_motion_notify_event   (ExoIconView                  *view,
                                                                     GdkEventMotion               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static void         thunar_abstract_icon_view_item_activated        (ExoIconView                  *view,
                                                                     GtkTreePath                  *path,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static void         thunar_abstract_icon_view_sort_column_changed   (GtkTreeSortable              *sortable,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static void         thunar_abstract_icon_view_zoom_level_changed    (ThunarAbstractIconView       *abstract_icon_view);



struct _ThunarAbstractIconViewPrivate
{
  /* the UI manager merge id for the abstract icon view */
  gint ui_merge_id;

  /* mouse gesture support */
  gint   gesture_start_x;
  gint   gesture_start_y;
  gint   gesture_current_x;
  gint   gesture_current_y;
  gulong gesture_expose_id;
  gulong gesture_motion_id;
  gulong gesture_release_id;
};



static const GtkActionEntry action_entries[] =
{
  { "arrange-items-menu", NULL, N_ ("Arran_ge Items"), NULL, NULL, NULL, },
};

static const GtkRadioActionEntry column_action_entries[] =
{
  { "sort-by-name", NULL, N_ ("Sort By _Name"), NULL, N_ ("Keep items sorted by their name"), THUNAR_COLUMN_NAME, },
  { "sort-by-size", NULL, N_ ("Sort By _Size"), NULL, N_ ("Keep items sorted by their size"), THUNAR_COLUMN_SIZE, },
  { "sort-by-type", NULL, N_ ("Sort By _Type"), NULL, N_ ("Keep items sorted by their type"), THUNAR_COLUMN_TYPE, },
  { "sort-by-mtime", NULL, N_ ("Sort By Modification _Date"), NULL, N_ ("Keep items sorted by their modification date"), THUNAR_COLUMN_DATE_MODIFIED, },
};

static const GtkRadioActionEntry order_action_entries[] =
{
  { "sort-ascending", NULL, N_ ("_Ascending"), NULL, N_ ("Sort items in ascending order"), GTK_SORT_ASCENDING, },
  { "sort-descending", NULL, N_ ("_Descending"), NULL, N_ ("Sort items in descending order"), GTK_SORT_DESCENDING, },
};



G_DEFINE_ABSTRACT_TYPE (ThunarAbstractIconView, thunar_abstract_icon_view, THUNAR_TYPE_STANDARD_VIEW)



static void
thunar_abstract_icon_view_class_init (ThunarAbstractIconViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  /* add private data to the instance type */
  g_type_class_add_private (klass, sizeof (ThunarAbstractIconViewPrivate));

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_set = thunar_abstract_icon_view_style_set;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->connect_ui_manager = thunar_abstract_icon_view_connect_ui_manager;
  thunarstandard_view_class->disconnect_ui_manager = thunar_abstract_icon_view_disconnect_ui_manager;
  thunarstandard_view_class->get_selected_items = thunar_abstract_icon_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_abstract_icon_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_abstract_icon_view_unselect_all;
  thunarstandard_view_class->selection_invert = thunar_abstract_icon_view_selection_invert;
  thunarstandard_view_class->select_path = thunar_abstract_icon_view_select_path;
  thunarstandard_view_class->set_cursor = thunar_abstract_icon_view_set_cursor;
  thunarstandard_view_class->scroll_to_path = thunar_abstract_icon_view_scroll_to_path;
  thunarstandard_view_class->get_path_at_pos = thunar_abstract_icon_view_get_path_at_pos;
  thunarstandard_view_class->get_visible_range = thunar_abstract_icon_view_get_visible_range;
  thunarstandard_view_class->highlight_path = thunar_abstract_icon_view_highlight_path;

  /**
   * ThunarAbstractIconView:column-spacing:
   *
   * The additional space inserted between columns in the
   * icon views.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("column-spacing",
                                                             "column-spacing",
                                                             "column-spacing",
                                                             0, G_MAXINT, 6,
                                                             EXO_PARAM_READABLE));

  /**
   * ThunarAbstractIconView:row-spacing:
   *
   * The additional space inserted between rows in the
   * icon views.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("row-spacing",
                                                             "row-spacing",
                                                             "row-spacing",
                                                             0, G_MAXINT, 6,
                                                             EXO_PARAM_READABLE));
}



static void
thunar_abstract_icon_view_init (ThunarAbstractIconView *abstract_icon_view)
{
  GtkWidget *view;

  /* connect private instance data */
  abstract_icon_view->priv = THUNAR_ABSTRACT_ICON_VIEW_GET_PRIVATE (abstract_icon_view);

  /* stay informed about zoom-level changes, so we can force a re-layout on the abstract_icon view */
  g_signal_connect (G_OBJECT (abstract_icon_view), "notify::zoom-level", G_CALLBACK (thunar_abstract_icon_view_zoom_level_changed), NULL);

  /* create the real view */
  view = exo_icon_view_new ();
  g_signal_connect (G_OBJECT (view), "notify::model", G_CALLBACK (thunar_abstract_icon_view_notify_model), abstract_icon_view);
  g_signal_connect (G_OBJECT (view), "button-press-event", G_CALLBACK (thunar_abstract_icon_view_button_press_event), abstract_icon_view);
  g_signal_connect (G_OBJECT (view), "key-press-event", G_CALLBACK (thunar_abstract_icon_view_key_press_event), abstract_icon_view);
  g_signal_connect (G_OBJECT (view), "item-activated", G_CALLBACK (thunar_abstract_icon_view_item_activated), abstract_icon_view);
  g_signal_connect_swapped (G_OBJECT (view), "selection-changed", G_CALLBACK (thunar_standard_view_selection_changed), abstract_icon_view);
  gtk_container_add (GTK_CONTAINER (abstract_icon_view), view);
  gtk_widget_show (view);

  /* initialize the abstract icon view properties */
  exo_icon_view_set_enable_search (EXO_ICON_VIEW (view), TRUE);
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (view), GTK_SELECTION_MULTIPLE);

  /* add the abstract icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_icon_view)->icon_renderer), "follow-state", TRUE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->icon_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->icon_renderer,
                                 "file", THUNAR_COLUMN_FILE);

  /* add the name renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer), "follow-state", TRUE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer,
                                 "text", THUNAR_COLUMN_NAME);

  /* setup the abstract icon view actions */
  gtk_action_group_add_actions (THUNAR_STANDARD_VIEW (abstract_icon_view)->action_group,
                                action_entries, G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (abstract_icon_view));
  gtk_action_group_add_radio_actions (THUNAR_STANDARD_VIEW (abstract_icon_view)->action_group, column_action_entries,
                                      G_N_ELEMENTS (column_action_entries), THUNAR_COLUMN_NAME,
                                      G_CALLBACK (thunar_abstract_icon_view_action_sort), abstract_icon_view);
  gtk_action_group_add_radio_actions (THUNAR_STANDARD_VIEW (abstract_icon_view)->action_group, order_action_entries,
                                      G_N_ELEMENTS (order_action_entries), GTK_SORT_ASCENDING,
                                      G_CALLBACK (thunar_abstract_icon_view_action_sort), abstract_icon_view);

  /* we need to listen to sort column changes to sync the menu items */
  g_signal_connect (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_icon_view)->model), "sort-column-changed",
                    G_CALLBACK (thunar_abstract_icon_view_sort_column_changed), abstract_icon_view);
  thunar_abstract_icon_view_sort_column_changed (GTK_TREE_SORTABLE (THUNAR_STANDARD_VIEW (abstract_icon_view)->model), abstract_icon_view);
}



static void
thunar_abstract_icon_view_style_set (GtkWidget *widget,
                                     GtkStyle  *previous_style)
{
  gint column_spacing;
  gint row_spacing;

  /* determine the column/row spacing from the style */
  gtk_widget_style_get (widget, "column-spacing", &column_spacing, "row-spacing", &row_spacing, NULL);

  /* apply the column/row spacing to the icon view */
  exo_icon_view_set_column_spacing (EXO_ICON_VIEW (GTK_BIN (widget)->child), column_spacing);
  exo_icon_view_set_row_spacing (EXO_ICON_VIEW (GTK_BIN (widget)->child), row_spacing);

  /* call the parent handler */
  (*GTK_WIDGET_CLASS (thunar_abstract_icon_view_parent_class)->style_set) (widget, previous_style);
}



static void
thunar_abstract_icon_view_connect_ui_manager (ThunarStandardView *standard_view,
                                              GtkUIManager       *ui_manager)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);
  GError                 *error = NULL;

  abstract_icon_view->priv->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_abstract_icon_view_ui,
                                                                             thunar_abstract_icon_view_ui_length, &error);
  if (G_UNLIKELY (abstract_icon_view->priv->ui_merge_id == 0))
    {
      g_error ("Failed to merge ThunarAbstractIconView menus: %s", error->message);
      g_error_free (error);
    }
}



static void
thunar_abstract_icon_view_disconnect_ui_manager (ThunarStandardView *standard_view,
                                                 GtkUIManager       *ui_manager)
{
  gtk_ui_manager_remove_ui (ui_manager, THUNAR_ABSTRACT_ICON_VIEW (standard_view)->priv->ui_merge_id);
}



static GList*
thunar_abstract_icon_view_get_selected_items (ThunarStandardView *standard_view)
{
  return exo_icon_view_get_selected_items (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_abstract_icon_view_select_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_select_all (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_abstract_icon_view_unselect_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_unselect_all (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_abstract_icon_view_selection_invert (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_selection_invert (EXO_ICON_VIEW (GTK_BIN (standard_view)->child));
}



static void
thunar_abstract_icon_view_select_path (ThunarStandardView *standard_view,
                                       GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_select_path (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path);
}



static void
thunar_abstract_icon_view_set_cursor (ThunarStandardView *standard_view,
                                      GtkTreePath        *path,
                                      gboolean            start_editing)
{
  GtkCellRendererMode mode;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));

  /* make sure the name renderer is editable */
  mode = standard_view->name_renderer->mode;
  standard_view->name_renderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;

  /* tell the abstract_icon view to start editing the given item */
  exo_icon_view_set_cursor (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, standard_view->name_renderer, start_editing);

  /* reset the name renderer mode */
  standard_view->name_renderer->mode = mode;
}



static void
thunar_abstract_icon_view_scroll_to_path (ThunarStandardView *standard_view,
                                          GtkTreePath        *path,
                                          gboolean            use_align,
                                          gfloat              row_align,
                                          gfloat              col_align)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_scroll_to_path (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, use_align, row_align, col_align);
}



static GtkTreePath*
thunar_abstract_icon_view_get_path_at_pos (ThunarStandardView *standard_view,
                                           gint                x,
                                           gint                y)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view), NULL);
  return exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), x, y);
}



static gboolean
thunar_abstract_icon_view_get_visible_range (ThunarStandardView *standard_view,
                                             GtkTreePath       **start_path,
                                             GtkTreePath       **end_path)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view), FALSE);
  return exo_icon_view_get_visible_range (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), start_path, end_path);
}



static void
thunar_abstract_icon_view_highlight_path (ThunarStandardView *standard_view,
                                          GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_set_drag_dest_item (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), path, EXO_ICON_VIEW_DROP_INTO);
}



static GtkAction*
thunar_abstract_icon_view_gesture_action (ThunarAbstractIconView *abstract_icon_view)
{
  if (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y > 40
      && ABS (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, "/main-menu/go-menu/open-parent");
    }
  else if (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x > 40
      && ABS (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, "/main-menu/go-menu/placeholder-go-history-actions/back");
    }
  else if (abstract_icon_view->priv->gesture_current_x - abstract_icon_view->priv->gesture_start_x > 40
      && ABS (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, "/main-menu/go-menu/placeholder-go-history-actions/forward");
    }
  else if (abstract_icon_view->priv->gesture_current_y - abstract_icon_view->priv->gesture_start_y > 40
      && ABS (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, "/main-menu/view-menu/reload");
    }

  return NULL;
}



static void
thunar_abstract_icon_view_action_sort (GtkAction          *action,
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



static void
thunar_abstract_icon_view_notify_model (ExoIconView            *view,
                                        GParamSpec             *pspec,
                                        ThunarAbstractIconView *abstract_icon_view)
{
  /* We need to set the search column here, as ExoIconView resets it
   * whenever a new model is set.
   */
  exo_icon_view_set_search_column (view, THUNAR_COLUMN_NAME);
}



static gboolean
thunar_abstract_icon_view_button_press_event (ExoIconView            *view,
                                              GdkEventButton         *event,
                                              ThunarAbstractIconView *abstract_icon_view)
{
  GtkTreePath       *path;
  GtkTreeIter        iter;
  ThunarFile        *file;
  GtkAction         *action;
  ThunarPreferences *preferences;
  gboolean           in_tab;
  const gchar       *action_name;

  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      /* open the context menu on right clicks */
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
          thunar_standard_view_queue_popup (THUNAR_STANDARD_VIEW (abstract_icon_view), event);
        }
      else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
          /* user clicked on an empty area, so we unselect everything
           * to make sure that the folder context menu is opened.
           */
          exo_icon_view_unselect_all (view);
      
          /* open the context menu */
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_icon_view), event->button, event->time);
        }

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS && event->button == 2)
    {
      /* unselect all currently selected items */
      exo_icon_view_unselect_all (view);

      /* determine the path to the item that was middle-clicked */
      if (exo_icon_view_get_item_at_pos (view, event->x, event->y, &path, NULL))
        {
          /* select only the path to the item on which the user clicked */
          exo_icon_view_select_path (view, path);

          /* determine the file for the path */
          gtk_tree_model_get_iter (GTK_TREE_MODEL (THUNAR_STANDARD_VIEW (abstract_icon_view)->model), &iter, path);
          file = thunar_list_model_get_file (THUNAR_STANDARD_VIEW (abstract_icon_view)->model, &iter);
          if (G_LIKELY (file != NULL) && thunar_file_is_directory (file))
            {
              /* lookup setting if we should open in a tab or a window */
              preferences = thunar_preferences_get ();
              g_object_get (preferences, "misc-middle-click-in-tab", &in_tab, NULL);
              g_object_unref (preferences);

              /* holding ctrl inverts the action */
              if ((event->state & GDK_CONTROL_MASK) != 0)
                  in_tab = !in_tab;
              action_name = in_tab ? "open-in-new-tab" : "open-in-new-window";

              /* emit the action */
              action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, action_name);
              if (G_LIKELY (action != NULL))
                  gtk_action_activate (action);

              /* release the file reference */
              g_object_unref (G_OBJECT (file));
            }

          /* cleanup */
          gtk_tree_path_free (path);
        }
      else if (event->type == GDK_BUTTON_PRESS)
        {
          abstract_icon_view->priv->gesture_start_x = abstract_icon_view->priv->gesture_current_x = event->x;
          abstract_icon_view->priv->gesture_start_y = abstract_icon_view->priv->gesture_current_y = event->y;
          abstract_icon_view->priv->gesture_expose_id = g_signal_connect_after (G_OBJECT (view), "expose-event",
                                                                                G_CALLBACK (thunar_abstract_icon_view_expose_event),
                                                                                G_OBJECT (abstract_icon_view));
          abstract_icon_view->priv->gesture_motion_id = g_signal_connect (G_OBJECT (view), "motion-notify-event",
                                                                          G_CALLBACK (thunar_abstract_icon_view_motion_notify_event),
                                                                          G_OBJECT (abstract_icon_view));
          abstract_icon_view->priv->gesture_release_id = g_signal_connect (G_OBJECT (view), "button-release-event",
                                                                           G_CALLBACK (thunar_abstract_icon_view_button_release_event),
                                                                           G_OBJECT (abstract_icon_view));
        }

      /* don't run the default handler here */
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_button_release_event (ExoIconView            *view,
                                                GdkEventButton         *event,
                                                ThunarAbstractIconView *abstract_icon_view)
{
  GtkAction *action;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view), FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_release_id > 0, FALSE);

  /* run the selected action (if any) */
  action = thunar_abstract_icon_view_gesture_action (abstract_icon_view);
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);

  /* unregister the "expose-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_expose_id);
  abstract_icon_view->priv->gesture_expose_id = 0;

  /* unregister the "motion-notify-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_motion_id);
  abstract_icon_view->priv->gesture_motion_id = 0;

  /* unregister the "button-release-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_release_id);
  abstract_icon_view->priv->gesture_release_id = 0;

  /* redraw the abstract_icon view */
  gtk_widget_queue_draw (GTK_WIDGET (view));

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_expose_event (ExoIconView            *view,
                                        GdkEventExpose         *event,
                                        ThunarAbstractIconView *abstract_icon_view)
{
  GtkIconSet *stock_icon_set;
  GtkAction  *action = NULL;
  GdkPixbuf  *stock_icon = NULL;
  gchar      *stock_id;
  GdkColor    bg;
  cairo_t    *cr;
  gint        x, y;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view), FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_release_id > 0, FALSE);

  /* create the cairo context (is already clipped) */
  cr = gdk_cairo_create (event->window);

  /* shade the abstract_icon view content while performing mouse gestures */
  bg = GTK_WIDGET (view)->style->base[GTK_STATE_NORMAL];
  cairo_set_source_rgba (cr, bg.red / 65535.0, bg.green / 65535.0, bg.blue / 65535.0, 0.7);
  cairo_paint (cr);

  /* determine the gesture action */
  action = thunar_abstract_icon_view_gesture_action (abstract_icon_view);
  if (G_LIKELY (action != NULL))
    {
      /* determine the stock abstract_icon for the action */
      g_object_get (G_OBJECT (action), "stock-id", &stock_id, NULL);

      /* lookup the abstract_icon set for the stock abstract_icon */
      stock_icon_set = gtk_style_lookup_icon_set (GTK_WIDGET (view)->style, stock_id);
      if (G_LIKELY (stock_icon_set != NULL))
        {
          stock_icon = gtk_icon_set_render_icon (stock_icon_set, GTK_WIDGET (view)->style,
                                                 gtk_widget_get_direction (GTK_WIDGET (view)),
                                                 gtk_action_is_sensitive (action) ? 0 : GTK_STATE_INSENSITIVE,
                                                 GTK_ICON_SIZE_DND, GTK_WIDGET (view), NULL);
        }

      /* draw the rendered icon */
      if (G_LIKELY (stock_icon != NULL))
        {
          /* x/y position of the icon */
          x = abstract_icon_view->priv->gesture_start_x - gdk_pixbuf_get_width (stock_icon) / 2;
          y = abstract_icon_view->priv->gesture_start_y - gdk_pixbuf_get_height (stock_icon) / 2;

          /* render the stock abstract_icon into the abstract_icon view window */
          gdk_cairo_set_source_pixbuf (cr, stock_icon, x, y);
          cairo_rectangle (cr, x, y,
                           gdk_pixbuf_get_width (stock_icon),
                           gdk_pixbuf_get_height (stock_icon));
          cairo_fill (cr);

          /* release the stock abstract_icon */
          g_object_unref (G_OBJECT (stock_icon));
        }

      /* release the stock id */
      g_free (stock_id);
    }

  /* destroy context */
  cairo_destroy (cr);

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_key_press_event (ExoIconView            *view,
                                           GdkEventKey            *event,
                                           ThunarAbstractIconView *abstract_icon_view)
{
  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_icon_view), 0, event->time);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_motion_notify_event (ExoIconView            *view,
                                               GdkEventMotion         *event,
                                               ThunarAbstractIconView *abstract_icon_view)
{
  GdkRectangle area;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view), FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_release_id > 0, FALSE);

  /* schedule a complete redraw on the first motion event */
  if (abstract_icon_view->priv->gesture_current_x == abstract_icon_view->priv->gesture_start_x
      && abstract_icon_view->priv->gesture_current_y == abstract_icon_view->priv->gesture_start_y)
    {
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
  else
    {
      /* otherwise, just redraw the action abstract_icon area */
      gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &area.width, &area.height);
      area.x = abstract_icon_view->priv->gesture_start_x - area.width / 2;
      area.y = abstract_icon_view->priv->gesture_start_y - area.height / 2;
      gdk_window_invalidate_rect (event->window, &area, TRUE);
    }

  /* update the current gesture position */
  abstract_icon_view->priv->gesture_current_x = event->x;
  abstract_icon_view->priv->gesture_current_y = event->y;

  /* don't execute the default motion notify handler */
  return TRUE;
}



static void
thunar_abstract_icon_view_item_activated (ExoIconView            *view,
                                          GtkTreePath            *path,
                                          ThunarAbstractIconView *abstract_icon_view)
{
  GtkAction *action;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  /* be sure to have only the double clicked item selected */
  exo_icon_view_unselect_all (view);
  exo_icon_view_select_path (view, path);

  /* emit the "open" action */
  action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (abstract_icon_view)->ui_manager, "open");
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);
}



static void
thunar_abstract_icon_view_sort_column_changed (GtkTreeSortable        *sortable,
                                               ThunarAbstractIconView *abstract_icon_view)
{
  GtkSortType order;
  GtkAction  *action;
  gint        column;

  if (gtk_tree_sortable_get_sort_column_id (sortable, &column, &order))
    {
      /* apply the new sort column */
      action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->action_group, "sort-by-name");
      gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), column);

      /* apply the new sort order */
      action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (abstract_icon_view)->action_group, "sort-ascending");
      gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), order);
    }
}



static void
thunar_abstract_icon_view_zoom_level_changed (ThunarAbstractIconView *abstract_icon_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  /* we use the same trick as with ThunarDetailsView here, simply because its simple :-) */
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (GTK_BIN (abstract_icon_view)->child),
                                      THUNAR_STANDARD_VIEW (abstract_icon_view)->icon_renderer,
                                      NULL, NULL, NULL);
}

