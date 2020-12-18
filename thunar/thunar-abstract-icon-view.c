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
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-window.h>



static void         thunar_abstract_icon_view_style_set             (GtkWidget                    *widget,
                                                                     GtkStyle                     *previous_style);
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
static void         thunar_abstract_icon_view_connect_accelerators  (ThunarStandardView           *standard_view,
                                                                     GtkAccelGroup                *accel_group);
static void         thunar_abstract_icon_view_disconnect_accelerators(ThunarStandardView          *standard_view,
                                                                     GtkAccelGroup                *accel_group);
static void         thunar_abstract_icon_view_append_menu_items     (ThunarStandardView           *standard_view,
                                                                     GtkMenu                      *menu,
                                                                     GtkAccelGroup                *accel_group);
static void         thunar_abstract_icon_view_notify_model          (ExoIconView                  *view,
                                                                     GParamSpec                   *pspec,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_button_press_event    (ExoIconView                  *view,
                                                                     GdkEventButton               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_button_release_event  (ExoIconView                  *view,
                                                                     GdkEventButton               *event,
                                                                     ThunarAbstractIconView       *abstract_icon_view);
static gboolean     thunar_abstract_icon_view_draw                  (ExoIconView                  *view,
                                                                     cairo_t                      *cr,
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
static void         thunar_abstract_icon_view_action_sort_by_name   (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_action_sort_by_size   (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_action_sort_by_type   (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_action_sort_by_date   (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_action_sort_ascending (ThunarStandardView           *standard_view);
static void         thunar_abstract_icon_view_action_sort_descending(ThunarStandardView           *standard_view);

struct _ThunarAbstractIconViewPrivate
{
  GtkSortType sort_order;
  gint        sort_column;

  /* mouse gesture support */
  gint   gesture_start_x;
  gint   gesture_start_y;
  gint   gesture_current_x;
  gint   gesture_current_y;
  gulong gesture_expose_id;
  gulong gesture_motion_id;
  gulong gesture_release_id;

  gboolean button_pressed;
};


static XfceGtkActionEntry thunar_abstract_icon_view_action_entries[] =
{
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_ARRANGE_ITEMS_MENU, "<Actions>/ThunarStandardView/arrange-items-menu",    "", XFCE_GTK_MENU_ITEM,       N_ ("Arran_ge Items"),        NULL,                                                NULL, G_CALLBACK (NULL),                                             },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_NAME,       "<Actions>/ThunarStandardView/sort-by-name",          "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Name"),              N_ ("Keep items sorted by their name"),              NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_by_name),    },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_SIZE,       "<Actions>/ThunarStandardView/sort-by-size",          "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Size"),              N_ ("Keep items sorted by their size"),              NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_by_size),    },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_TYPE,       "<Actions>/ThunarStandardView/sort-by-type",          "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("By _Type"),              N_ ("Keep items sorted by their type"),              NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_by_type),    },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_MTIME,      "<Actions>/ThunarStandardView/sort-by-mtime",         "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("By Modification _Date"), N_ ("Keep items sorted by their modification date"), NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_by_date),    },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_ASCENDING,     "<Actions>/ThunarStandardView/sort-ascending",        "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Ascending"),            N_ ("Sort items in ascending order"),                NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_ascending),  },
    { THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_DESCENDING,    "<Actions>/ThunarStandardView/sort-descending",       "", XFCE_GTK_RADIO_MENU_ITEM, N_ ("_Descending"),           N_ ("Sort items in descending order"),               NULL, G_CALLBACK (thunar_abstract_icon_view_action_sort_descending), },
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id(thunar_abstract_icon_view_action_entries,G_N_ELEMENTS(thunar_abstract_icon_view_action_entries),id)



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ThunarAbstractIconView, thunar_abstract_icon_view, THUNAR_TYPE_STANDARD_VIEW)



static void
thunar_abstract_icon_view_class_init (ThunarAbstractIconViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_set = thunar_abstract_icon_view_style_set;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
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
  thunarstandard_view_class->append_menu_items = thunar_abstract_icon_view_append_menu_items;
  thunarstandard_view_class->connect_accelerators = thunar_abstract_icon_view_connect_accelerators;
  thunarstandard_view_class->disconnect_accelerators = thunar_abstract_icon_view_disconnect_accelerators;

  xfce_gtk_translate_action_entries (thunar_abstract_icon_view_action_entries, G_N_ELEMENTS (thunar_abstract_icon_view_action_entries));

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
  abstract_icon_view->priv = thunar_abstract_icon_view_get_instance_private (abstract_icon_view);

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
  /*FIXME text prelit*/
  /*g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer), "follow-state", TRUE, NULL);*/
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_icon_view)->name_renderer,
                                 "text", THUNAR_COLUMN_NAME);

  /* we need to listen to sort column changes to sync the menu items */
  g_signal_connect (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_icon_view)->model), "sort-column-changed",
                    G_CALLBACK (thunar_abstract_icon_view_sort_column_changed), abstract_icon_view);
  thunar_abstract_icon_view_sort_column_changed (GTK_TREE_SORTABLE (THUNAR_STANDARD_VIEW (abstract_icon_view)->model), abstract_icon_view);

  /* update the icon view on size-allocate events */
  /* TODO: issue not reproducible anymore as of gtk 3.24.18
   * we can probably remove this in the future. */
  g_signal_connect_swapped (G_OBJECT (abstract_icon_view), "size-allocate",
                            G_CALLBACK (gtk_widget_queue_resize), view);
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
  exo_icon_view_set_column_spacing (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (widget))), column_spacing);
  exo_icon_view_set_row_spacing (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (widget))), row_spacing);

  /* call the parent handler */
  (*GTK_WIDGET_CLASS (thunar_abstract_icon_view_parent_class)->style_set) (widget, previous_style);
}



static GList*
thunar_abstract_icon_view_get_selected_items (ThunarStandardView *standard_view)
{
  return exo_icon_view_get_selected_items (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_icon_view_select_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_select_all (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_icon_view_unselect_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_unselect_all (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_icon_view_selection_invert (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_selection_invert (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_icon_view_select_path (ThunarStandardView *standard_view,
                                       GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_select_path (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path);
}



static void
thunar_abstract_icon_view_set_cursor (ThunarStandardView *standard_view,
                                      GtkTreePath        *path,
                                      gboolean            start_editing)
{
  GtkCellRendererMode mode;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));

  /* make sure the name renderer is editable */
  g_object_get ( G_OBJECT (standard_view->name_renderer), "mode", &mode, NULL);
  g_object_set ( G_OBJECT (standard_view->name_renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);

  /* tell the abstract_icon view to start editing the given item */
  exo_icon_view_set_cursor (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, standard_view->name_renderer, start_editing);

  /* reset the name renderer mode */
  g_object_set (G_OBJECT (standard_view->name_renderer), "mode", mode, NULL);
}



static void
thunar_abstract_icon_view_scroll_to_path (ThunarStandardView *standard_view,
                                          GtkTreePath        *path,
                                          gboolean            use_align,
                                          gfloat              row_align,
                                          gfloat              col_align)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_scroll_to_path (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, use_align, row_align, col_align);
}



static GtkTreePath*
thunar_abstract_icon_view_get_path_at_pos (ThunarStandardView *standard_view,
                                           gint                x,
                                           gint                y)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view), NULL);
  return exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), x, y);
}



static gboolean
thunar_abstract_icon_view_get_visible_range (ThunarStandardView *standard_view,
                                             GtkTreePath       **start_path,
                                             GtkTreePath       **end_path)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view), FALSE);
  return exo_icon_view_get_visible_range (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), start_path, end_path);
}



static void
thunar_abstract_icon_view_highlight_path (ThunarStandardView *standard_view,
                                          GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (standard_view));
  exo_icon_view_set_drag_dest_item (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, EXO_ICON_VIEW_DROP_INTO);
}



/**
 * thunar_abstract_icon_view_connect_accelerators:
 * @standard_view : a #ThunarStandardView
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
static void
thunar_abstract_icon_view_connect_accelerators (ThunarStandardView *standard_view,
                                                GtkAccelGroup      *accel_group)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  xfce_gtk_accel_map_add_entries (thunar_abstract_icon_view_action_entries,
                                  G_N_ELEMENTS (thunar_abstract_icon_view_action_entries));
  xfce_gtk_accel_group_connect_action_entries (accel_group,
                                               thunar_abstract_icon_view_action_entries,
                                               G_N_ELEMENTS (thunar_abstract_icon_view_action_entries),
                                               standard_view);
}



/**
 * thunar_abstract_icon_view_disconnect_accelerators:
 * @standard_view : a #ThunarStandardView
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Disconnects all accelerators from the passed #GtkAccelGroup
 **/
static void
thunar_abstract_icon_view_disconnect_accelerators (ThunarStandardView *standard_view,
                                                   GtkAccelGroup      *accel_group)
{
  /* Dont listen to the accel keys defined by the action entries any more */
  xfce_gtk_accel_group_disconnect_action_entries (accel_group,
                                                  thunar_abstract_icon_view_action_entries,
                                                  G_N_ELEMENTS (thunar_abstract_icon_view_action_entries));
}



/**
 * thunar_abstract_icon_view_append_menu_items:
 * @standard_view : a #ThunarStandardView
 * @menu : the #GtkMenu to add the menu items
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Appends widget-specific menu items to a #GtkMenu and connects them to the passed #GtkAccelGroup
 * Implements method 'append_menu_items' of #ThunarStandardView
 **/
static void
thunar_abstract_icon_view_append_menu_items (ThunarStandardView *standard_view,
                                             GtkMenu            *menu,
                                             GtkAccelGroup      *accel_group)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);
  GtkWidget              *submenu;
  GtkWidget              *item;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_ARRANGE_ITEMS_MENU), NULL, GTK_MENU_SHELL (menu));
  submenu =  gtk_menu_new();
  if (accel_group != NULL)
    gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_NAME), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_column == THUNAR_COLUMN_NAME, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_SIZE), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_column == THUNAR_COLUMN_SIZE, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_TYPE), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_column == THUNAR_COLUMN_TYPE, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_BY_MTIME), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_column == THUNAR_COLUMN_DATE_MODIFIED, GTK_MENU_SHELL (submenu));
  xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_ASCENDING), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_order == GTK_SORT_ASCENDING, GTK_MENU_SHELL (submenu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_ABSTRACT_ICON_VIEW_ACTION_SORT_DESCENDING), G_OBJECT (standard_view),
                                                   abstract_icon_view->priv->sort_order == GTK_SORT_DESCENDING, GTK_MENU_SHELL (submenu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
  gtk_widget_show (item);
}



static const XfceGtkActionEntry*
thunar_abstract_icon_view_gesture_action (ThunarAbstractIconView *abstract_icon_view, GtkTextDirection direction)
{
  GtkWidget *window;

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_icon_view));
  if (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y > 40
      && ABS (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x) < 40)
    {
      return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_OPEN_PARENT);
    }
  else if (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x > 40
      && ABS (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y) < 40)
    {
      if (direction == GTK_TEXT_DIR_LTR || direction == GTK_TEXT_DIR_NONE)
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_BACK);
      else
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_FORWARD);
    }
  else if (abstract_icon_view->priv->gesture_current_x - abstract_icon_view->priv->gesture_start_x > 40
      && ABS (abstract_icon_view->priv->gesture_start_y - abstract_icon_view->priv->gesture_current_y) < 40)
    {
      if (direction == GTK_TEXT_DIR_LTR || direction == GTK_TEXT_DIR_NONE)
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_FORWARD);
      else
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_BACK);
    }
  else if (abstract_icon_view->priv->gesture_current_y - abstract_icon_view->priv->gesture_start_y > 40
      && ABS (abstract_icon_view->priv->gesture_start_x - abstract_icon_view->priv->gesture_current_x) < 40)
    {
      return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_RELOAD);
    }
  return NULL;
}



static void
thunar_abstract_icon_view_action_sort_by_name (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), THUNAR_COLUMN_NAME, abstract_icon_view->priv->sort_order);
}



static void
thunar_abstract_icon_view_action_sort_by_size (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), THUNAR_COLUMN_SIZE, abstract_icon_view->priv->sort_order);
}



static void
thunar_abstract_icon_view_action_sort_by_type (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), THUNAR_COLUMN_TYPE, abstract_icon_view->priv->sort_order);
}



static void
thunar_abstract_icon_view_action_sort_by_date (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), THUNAR_COLUMN_DATE_MODIFIED, abstract_icon_view->priv->sort_order);
}



static void
thunar_abstract_icon_view_action_sort_ascending (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), abstract_icon_view->priv->sort_column, GTK_SORT_ASCENDING);
}



static void
thunar_abstract_icon_view_action_sort_descending (ThunarStandardView *standard_view)
{
  ThunarAbstractIconView *abstract_icon_view = THUNAR_ABSTRACT_ICON_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), abstract_icon_view->priv->sort_column, GTK_SORT_DESCENDING);
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

  abstract_icon_view->priv->button_pressed = TRUE;

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
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_icon_view));
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

          /* try to open the path as new window/tab, if possible */
          _thunar_standard_view_open_on_middle_click (THUNAR_STANDARD_VIEW (abstract_icon_view), path, event->state);

          /* cleanup */
          gtk_tree_path_free (path);
        }
      else if (event->type == GDK_BUTTON_PRESS)
        {
          abstract_icon_view->priv->gesture_start_x = abstract_icon_view->priv->gesture_current_x = event->x;
          abstract_icon_view->priv->gesture_start_y = abstract_icon_view->priv->gesture_current_y = event->y;
          abstract_icon_view->priv->gesture_expose_id = g_signal_connect_after (G_OBJECT (view), "draw",
                                                                                G_CALLBACK (thunar_abstract_icon_view_draw),
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
  const XfceGtkActionEntry *action_entry;
  GtkWidget                *window;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view), FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_release_id > 0, FALSE);

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_icon_view));

  /* unregister the "expose-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_expose_id);
  abstract_icon_view->priv->gesture_expose_id = 0;

  /* unregister the "motion-notify-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_motion_id);
  abstract_icon_view->priv->gesture_motion_id = 0;

  /* unregister the "button-release-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_icon_view->priv->gesture_release_id);
  abstract_icon_view->priv->gesture_release_id = 0;

  /* execute the related callback  (if any) */
  action_entry = thunar_abstract_icon_view_gesture_action (abstract_icon_view, gtk_widget_get_direction (window));
  if (G_LIKELY (action_entry != NULL))
    ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW (window));

  /* redraw the abstract_icon view */
  gtk_widget_queue_draw (GTK_WIDGET (view));

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_draw (ExoIconView            *view,
                                cairo_t                *cr,
                                ThunarAbstractIconView *abstract_icon_view)
{
  const XfceGtkActionEntry *action_entry = NULL;
  GdkPixbuf                *gesture_icon = NULL;
  gint                      x, y;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view), FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_icon_view->priv->gesture_release_id > 0, FALSE);

  /* shade the abstract_icon view content while performing mouse gestures */
  cairo_set_source_rgba (cr, 1, 1, 1, 0.7);
  cairo_paint (cr);

  /* determine the gesture action.
   * GtkTextDirection needs to be fixed, so that e.g. the left arrow will always be shown on the left.
   * Even if the window uses GTK_TEXT_DIR_RTL.
   * */
  action_entry = thunar_abstract_icon_view_gesture_action (abstract_icon_view, GTK_TEXT_DIR_NONE);
  if (G_LIKELY (action_entry != NULL))
    {
      gesture_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default(),
                                               action_entry->menu_item_icon_name,
                                               32,
                                               GTK_ICON_LOOKUP_FORCE_SIZE,
                                               NULL);
      /* draw the rendered icon */
      if (G_LIKELY (gesture_icon != NULL))
        {
          /* x/y position of the icon */
          x = abstract_icon_view->priv->gesture_start_x - gdk_pixbuf_get_width (gesture_icon) / 2;
          y = abstract_icon_view->priv->gesture_start_y - gdk_pixbuf_get_height (gesture_icon) / 2;

          /* render the icon into the abstract_icon view window */
          gdk_cairo_set_source_pixbuf (cr, gesture_icon, x, y);
          cairo_rectangle (cr, x, y,
                           gdk_pixbuf_get_width (gesture_icon),
                           gdk_pixbuf_get_height (gesture_icon));
          cairo_fill (cr);

          /* release the stock abstract_icon */
          g_object_unref (G_OBJECT (gesture_icon));
        }
    }

  return FALSE;
}



static gboolean
thunar_abstract_icon_view_key_press_event (ExoIconView            *view,
                                           GdkEventKey            *event,
                                           ThunarAbstractIconView *abstract_icon_view)
{
  abstract_icon_view->priv->button_pressed = FALSE;

  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_KEY_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_KEY_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_icon_view));
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
  GtkWidget *window;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  /* be sure to have only the clicked item selected */
  if (abstract_icon_view->priv->button_pressed)
    {
      exo_icon_view_unselect_all (view);
      exo_icon_view_select_path (view, path);
    }

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_icon_view));
  thunar_launcher_activate_selected_files (thunar_window_get_launcher (THUNAR_WINDOW (window)), THUNAR_LAUNCHER_CHANGE_DIRECTORY, NULL);
}



static void
thunar_abstract_icon_view_sort_column_changed (GtkTreeSortable        *sortable,
                                               ThunarAbstractIconView *abstract_icon_view)
{
  GtkSortType order;
  gint        column;

  if (gtk_tree_sortable_get_sort_column_id (sortable, &column, &order))
    {
      abstract_icon_view->priv->sort_column = column;
      abstract_icon_view->priv->sort_order = order;
    }
}



static void
thunar_abstract_icon_view_zoom_level_changed (ThunarAbstractIconView *abstract_icon_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_ICON_VIEW (abstract_icon_view));

  /* we use the same trick as with ThunarDetailsView here, simply because its simple :-) */
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (gtk_bin_get_child (GTK_BIN (abstract_icon_view))),
                                      THUNAR_STANDARD_VIEW (abstract_icon_view)->icon_renderer,
                                      NULL, NULL, NULL);
}
