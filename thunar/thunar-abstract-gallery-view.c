/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
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

#include <thunar/thunar-abstract-gallery-view.h>
#include <thunar/thunar-action-manager.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-window.h>



static void         thunar_abstract_gallery_view_style_set               (GtkWidget                    *widget,
                                                                          GtkStyle                     *previous_style);
static GList       *thunar_abstract_gallery_view_get_selected_items      (ThunarStandardView           *standard_view);
static void         thunar_abstract_gallery_view_select_all              (ThunarStandardView           *standard_view);
static void         thunar_abstract_gallery_view_unselect_all            (ThunarStandardView           *standard_view);
static void         thunar_abstract_gallery_view_selection_invert        (ThunarStandardView           *standard_view);
static void         thunar_abstract_gallery_view_select_path             (ThunarStandardView           *standard_view,
                                                                          GtkTreePath                  *path);
static void         thunar_abstract_gallery_view_set_cursor              (ThunarStandardView           *standard_view,
                                                                          GtkTreePath                  *path,
                                                                          gboolean                      start_editing);
static void         thunar_abstract_gallery_view_scroll_to_path          (ThunarStandardView           *standard_view,
                                                                          GtkTreePath                  *path,
                                                                          gboolean                      use_align,
                                                                          gfloat                        row_align,
                                                                          gfloat                        col_align);
static GtkTreePath *thunar_abstract_gallery_view_get_path_at_pos         (ThunarStandardView           *standard_view,
                                                                          gint                          x,
                                                                          gint                          y);
static gboolean     thunar_abstract_gallery_view_get_visible_range       (ThunarStandardView           *standard_view,
                                                                          GtkTreePath                 **start_path,
                                                                          GtkTreePath                 **end_path);
static void         thunar_abstract_gallery_view_highlight_path          (ThunarStandardView           *standard_view,
                                                                          GtkTreePath                  *path);
static void         thunar_abstract_gallery_view_notify_model            (ExoIconView                  *view,
                                                                          GParamSpec                   *pspec,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static gboolean     thunar_abstract_gallery_view_button_press_event      (ExoIconView                  *view,
                                                                          GdkEventButton               *event,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static gboolean     thunar_abstract_gallery_view_button_release_event    (ExoIconView                  *view,
                                                                          GdkEventButton               *event,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static gboolean     thunar_abstract_gallery_view_draw                    (ExoIconView                  *view,
                                                                          cairo_t                      *cr,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static gboolean     thunar_abstract_gallery_view_key_press_event         (ExoIconView                  *view,
                                                                          GdkEventKey                  *event,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static gboolean     thunar_abstract_gallery_view_motion_notify_event     (ExoIconView                  *view,
                                                                          GdkEventMotion               *event,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static void         thunar_abstract_gallery_view_item_activated          (ExoIconView                  *view,
                                                                          GtkTreePath                  *path,
                                                                          ThunarAbstractGalleryView    *abstract_gallery_view);
static void         thunar_abstract_gallery_view_zoom_level_changed      (ThunarAbstractGalleryView    *abstract_gallery_view);



struct _ThunarAbstractGalleryViewPrivate
{
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


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ThunarAbstractGalleryView, thunar_abstract_gallery_view, THUNAR_TYPE_STANDARD_VIEW)



static void
thunar_abstract_gallery_view_class_init (ThunarAbstractGalleryViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_set = thunar_abstract_gallery_view_style_set;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->get_selected_items = thunar_abstract_gallery_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_abstract_gallery_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_abstract_gallery_view_unselect_all;
  thunarstandard_view_class->selection_invert = thunar_abstract_gallery_view_selection_invert;
  thunarstandard_view_class->select_path = thunar_abstract_gallery_view_select_path;
  thunarstandard_view_class->set_cursor = thunar_abstract_gallery_view_set_cursor;
  thunarstandard_view_class->scroll_to_path = thunar_abstract_gallery_view_scroll_to_path;
  thunarstandard_view_class->get_path_at_pos = thunar_abstract_gallery_view_get_path_at_pos;
  thunarstandard_view_class->get_visible_range = thunar_abstract_gallery_view_get_visible_range;
  thunarstandard_view_class->highlight_path = thunar_abstract_gallery_view_highlight_path;

  /**
   * ThunarAbstractGalleryView:column-spacing:
   *
   * The additional space inserted between columns in the
   * gallery views.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("column-spacing",
                                                             "column-spacing",
                                                             "column-spacing",
                                                             0, G_MAXINT, 6,
                                                             EXO_PARAM_READABLE));

  /**
   * ThunarAbstractGalleryView:row-spacing:
   *
   * The additional space inserted between rows in the
   * gallery views.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("row-spacing",
                                                             "row-spacing",
                                                             "row-spacing",
                                                             0, G_MAXINT, 6,
                                                             EXO_PARAM_READABLE));
}



static void
thunar_abstract_gallery_view_init (ThunarAbstractGalleryView *abstract_gallery_view)
{
  GtkWidget *view;

  /* connect private instance data */
  abstract_gallery_view->priv = thunar_abstract_gallery_view_get_instance_private (abstract_gallery_view);

  /* stay informed about zoom-level changes, so we can force a re-layout on the abstract_icon view */
  g_signal_connect (G_OBJECT (abstract_gallery_view), "notify::zoom-level", G_CALLBACK (thunar_abstract_gallery_view_zoom_level_changed), NULL);

  /* create the real view */
  view = exo_icon_view_new ();
  g_signal_connect (G_OBJECT (view), "notify::model", G_CALLBACK (thunar_abstract_gallery_view_notify_model), abstract_gallery_view);
  g_signal_connect (G_OBJECT (view), "button-press-event", G_CALLBACK (thunar_abstract_gallery_view_button_press_event), abstract_gallery_view);
  g_signal_connect (G_OBJECT (view), "key-press-event", G_CALLBACK (thunar_abstract_gallery_view_key_press_event), abstract_gallery_view);
  g_signal_connect (G_OBJECT (view), "item-activated", G_CALLBACK (thunar_abstract_gallery_view_item_activated), abstract_gallery_view);
  g_signal_connect_swapped (G_OBJECT (view), "selection-changed", G_CALLBACK (thunar_standard_view_selection_changed), abstract_gallery_view);
  gtk_container_add (GTK_CONTAINER (abstract_gallery_view), view);
  gtk_widget_show (view);

  /* initialize the abstract icon view properties */
  exo_icon_view_set_enable_search (EXO_ICON_VIEW (view), TRUE);
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (view), GTK_SELECTION_MULTIPLE);

  /* add the abstract icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer), "square-icons", TRUE, NULL);
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer), "follow-state", TRUE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer,
                                 "file", THUNAR_COLUMN_FILE);

  /* add the name renderer, but make it invisible */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (abstract_gallery_view)->name_renderer), "visible", FALSE, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_gallery_view)->name_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), THUNAR_STANDARD_VIEW (abstract_gallery_view)->name_renderer,
                                 "text", THUNAR_COLUMN_NAME);

  /* update the icon view on size-allocate events */
  /* TODO: issue not reproducible anymore as of gtk 3.24.18
   * we can probably remove this in the future. */
  g_signal_connect_swapped (G_OBJECT (abstract_gallery_view), "size-allocate",
                            G_CALLBACK (gtk_widget_queue_resize), view);
}



static void
thunar_abstract_gallery_view_style_set (GtkWidget *widget,
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
  (*GTK_WIDGET_CLASS (thunar_abstract_gallery_view_parent_class)->style_set) (widget, previous_style);
}



static GList*
thunar_abstract_gallery_view_get_selected_items (ThunarStandardView *standard_view)
{
  return exo_icon_view_get_selected_items (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_gallery_view_select_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_select_all (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_gallery_view_unselect_all (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_unselect_all (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_gallery_view_selection_invert (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_selection_invert (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
}



static void
thunar_abstract_gallery_view_select_path (ThunarStandardView *standard_view,
                                          GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_select_path (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path);
}



static void
thunar_abstract_gallery_view_set_cursor (ThunarStandardView *standard_view,
                                         GtkTreePath        *path,
                                         gboolean            start_editing)
{
  GtkCellRendererMode mode;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));

  /* make sure the name renderer is editable */
  g_object_get ( G_OBJECT (standard_view->name_renderer), "mode", &mode, NULL);
  g_object_set ( G_OBJECT (standard_view->name_renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);

  /* tell the abstract_icon view to start editing the given item */
  exo_icon_view_set_cursor (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, standard_view->name_renderer, start_editing);

  /* reset the name renderer mode */
  g_object_set (G_OBJECT (standard_view->name_renderer), "mode", mode, NULL);
}



static void
thunar_abstract_gallery_view_scroll_to_path (ThunarStandardView *standard_view,
                                            GtkTreePath         *path,
                                            gboolean             use_align,
                                            gfloat               row_align,
                                            gfloat               col_align)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_scroll_to_path (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, use_align, row_align, col_align);
}



static GtkTreePath*
thunar_abstract_gallery_view_get_path_at_pos (ThunarStandardView *standard_view,
                                              gint                x,
                                              gint                y)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view), NULL);
  return exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), x, y);
}



static gboolean
thunar_abstract_gallery_view_get_visible_range (ThunarStandardView *standard_view,
                                                GtkTreePath       **start_path,
                                                GtkTreePath       **end_path)
{
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view), FALSE);
  return exo_icon_view_get_visible_range (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), start_path, end_path);
}



static void
thunar_abstract_gallery_view_highlight_path (ThunarStandardView *standard_view,
                                             GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (standard_view));
  exo_icon_view_set_drag_dest_item (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, EXO_ICON_VIEW_DROP_INTO);
}



static const XfceGtkActionEntry*
thunar_abstract_gallery_view_gesture_action (ThunarAbstractGalleryView *abstract_gallery_view, GtkTextDirection direction)
{
  GtkWidget *window;

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_gallery_view));
  if (abstract_gallery_view->priv->gesture_start_y - abstract_gallery_view->priv->gesture_current_y > 40
      && ABS (abstract_gallery_view->priv->gesture_start_x - abstract_gallery_view->priv->gesture_current_x) < 40)
    {
      return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_OPEN_PARENT);
    }
  else if (abstract_gallery_view->priv->gesture_start_x - abstract_gallery_view->priv->gesture_current_x > 40
      && ABS (abstract_gallery_view->priv->gesture_start_y - abstract_gallery_view->priv->gesture_current_y) < 40)
    {
      if (direction == GTK_TEXT_DIR_LTR || direction == GTK_TEXT_DIR_NONE)
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_BACK);
      else
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_FORWARD);
    }
  else if (abstract_gallery_view->priv->gesture_current_x - abstract_gallery_view->priv->gesture_start_x > 40
      && ABS (abstract_gallery_view->priv->gesture_start_y - abstract_gallery_view->priv->gesture_current_y) < 40)
    {
      if (direction == GTK_TEXT_DIR_LTR || direction == GTK_TEXT_DIR_NONE)
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_FORWARD);
      else
        return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_BACK);
    }
  else if (abstract_gallery_view->priv->gesture_current_y - abstract_gallery_view->priv->gesture_start_y > 40
      && ABS (abstract_gallery_view->priv->gesture_start_x - abstract_gallery_view->priv->gesture_current_x) < 40)
    {
      return thunar_window_get_action_entry (THUNAR_WINDOW (window), THUNAR_WINDOW_ACTION_RELOAD);
    }
  return NULL;
}



static void
thunar_abstract_gallery_view_notify_model (ExoIconView            *view,
                                           GParamSpec             *pspec,
                                           ThunarAbstractGalleryView *abstract_gallery_view)
{
  /* We need to set the search column here, as ExoIconView resets it
   * whenever a new model is set.
   */
  exo_icon_view_set_search_column (view, THUNAR_COLUMN_NAME);
}



static gboolean
thunar_abstract_gallery_view_button_press_event (ExoIconView            *view,
                                                 GdkEventButton         *event,
                                                 ThunarAbstractGalleryView *abstract_gallery_view)
{
  GtkTreePath       *path;

  abstract_gallery_view->priv->button_pressed = TRUE;

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
          thunar_standard_view_queue_popup (THUNAR_STANDARD_VIEW (abstract_gallery_view), event);
        }
      else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
          /* user clicked on an empty area, so we unselect everything
           * to make sure that the folder context menu is opened.
           */
          exo_icon_view_unselect_all (view);

          /* open the context menu */
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_gallery_view));
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
          _thunar_standard_view_open_on_middle_click (THUNAR_STANDARD_VIEW (abstract_gallery_view), path, event->state);

          /* cleanup */
          gtk_tree_path_free (path);
        }
      else if (event->type == GDK_BUTTON_PRESS)
        {
          abstract_gallery_view->priv->gesture_start_x = abstract_gallery_view->priv->gesture_current_x = event->x;
          abstract_gallery_view->priv->gesture_start_y = abstract_gallery_view->priv->gesture_current_y = event->y;
          abstract_gallery_view->priv->gesture_expose_id = g_signal_connect_after (G_OBJECT (view), "draw",
                                                                                   G_CALLBACK (thunar_abstract_gallery_view_draw),
                                                                                   G_OBJECT (abstract_gallery_view));
          abstract_gallery_view->priv->gesture_motion_id = g_signal_connect (G_OBJECT (view), "motion-notify-event",
                                                                             G_CALLBACK (thunar_abstract_gallery_view_motion_notify_event),
                                                                             G_OBJECT (abstract_gallery_view));
          abstract_gallery_view->priv->gesture_release_id = g_signal_connect (G_OBJECT (view), "button-release-event",
                                                                              G_CALLBACK (thunar_abstract_gallery_view_button_release_event),
                                                                              G_OBJECT (abstract_gallery_view));
        }

      /* don't run the default handler here */
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_abstract_gallery_view_button_release_event (ExoIconView              *view,
                                                   GdkEventButton            *event,
                                                   ThunarAbstractGalleryView *abstract_gallery_view)
{
  const XfceGtkActionEntry *action_entry;
  GtkWidget                *window;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (abstract_gallery_view), FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_release_id > 0, FALSE);

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_gallery_view));

  /* unregister the "expose-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_gallery_view->priv->gesture_expose_id);
  abstract_gallery_view->priv->gesture_expose_id = 0;

  /* unregister the "motion-notify-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_gallery_view->priv->gesture_motion_id);
  abstract_gallery_view->priv->gesture_motion_id = 0;

  /* unregister the "button-release-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), abstract_gallery_view->priv->gesture_release_id);
  abstract_gallery_view->priv->gesture_release_id = 0;

  /* execute the related callback  (if any) */
  action_entry = thunar_abstract_gallery_view_gesture_action (abstract_gallery_view, gtk_widget_get_direction (window));
  if (G_LIKELY (action_entry != NULL))
    ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW (window));

  /* redraw the abstract_icon view */
  gtk_widget_queue_draw (GTK_WIDGET (view));

  return FALSE;
}



static gboolean
thunar_abstract_gallery_view_draw (ExoIconView               *view,
                                   cairo_t                   *cr,
                                   ThunarAbstractGalleryView *abstract_gallery_view)
{
  const XfceGtkActionEntry *action_entry = NULL;
  GdkPixbuf                *gesture_icon = NULL;
  gint                      x, y;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (abstract_gallery_view), FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_release_id > 0, FALSE);

  /* shade the abstract_icon view content while performing mouse gestures */
  cairo_set_source_rgba (cr, 1, 1, 1, 0.7);
  cairo_paint (cr);

  /* determine the gesture action.
   * GtkTextDirection needs to be fixed, so that e.g. the left arrow will always be shown on the left.
   * Even if the window uses GTK_TEXT_DIR_RTL.
   * */
  action_entry = thunar_abstract_gallery_view_gesture_action (abstract_gallery_view, GTK_TEXT_DIR_NONE);
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
          x = abstract_gallery_view->priv->gesture_start_x - gdk_pixbuf_get_width (gesture_icon) / 2;
          y = abstract_gallery_view->priv->gesture_start_y - gdk_pixbuf_get_height (gesture_icon) / 2;

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
thunar_abstract_gallery_view_key_press_event (ExoIconView               *view,
                                              GdkEventKey               *event,
                                              ThunarAbstractGalleryView *abstract_gallery_view)
{
  abstract_gallery_view->priv->button_pressed = FALSE;

  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_KEY_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_KEY_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (abstract_gallery_view));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_abstract_gallery_view_motion_notify_event (ExoIconView               *view,
                                                  GdkEventMotion            *event,
                                                  ThunarAbstractGalleryView *abstract_gallery_view)
{
  GdkRectangle area;

  _thunar_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (abstract_gallery_view), FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_expose_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_motion_id > 0, FALSE);
  _thunar_return_val_if_fail (abstract_gallery_view->priv->gesture_release_id > 0, FALSE);

  /* schedule a complete redraw on the first motion event */
  if (abstract_gallery_view->priv->gesture_current_x == abstract_gallery_view->priv->gesture_start_x
      && abstract_gallery_view->priv->gesture_current_y == abstract_gallery_view->priv->gesture_start_y)
    {
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
  else
    {
      /* otherwise, just redraw the action abstract_icon area */
      gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &area.width, &area.height);
      area.x = abstract_gallery_view->priv->gesture_start_x - area.width / 2;
      area.y = abstract_gallery_view->priv->gesture_start_y - area.height / 2;
      gdk_window_invalidate_rect (event->window, &area, TRUE);
    }

  /* update the current gesture position */
  abstract_gallery_view->priv->gesture_current_x = event->x;
  abstract_gallery_view->priv->gesture_current_y = event->y;

  /* don't execute the default motion notify handler */
  return TRUE;
}



static void
thunar_abstract_gallery_view_item_activated (ExoIconView               *view,
                                             GtkTreePath               *path,
                                             ThunarAbstractGalleryView *abstract_gallery_view)
{
  GtkWidget *window;

  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (abstract_gallery_view));

  /* be sure to have only the clicked item selected */
  if (abstract_gallery_view->priv->button_pressed)
    {
      exo_icon_view_unselect_all (view);
      exo_icon_view_select_path (view, path);
    }

  window = gtk_widget_get_toplevel (GTK_WIDGET (abstract_gallery_view));
  thunar_action_manager_activate_selected_files (thunar_window_get_action_manager (THUNAR_WINDOW (window)), THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY, NULL);
}



static void
thunar_abstract_gallery_view_zoom_level_changed (ThunarAbstractGalleryView *abstract_gallery_view)
{
  _thunar_return_if_fail (THUNAR_IS_ABSTRACT_GALLERY_VIEW (abstract_gallery_view));

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (gtk_bin_get_child (GTK_BIN (abstract_gallery_view))),
                                      THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer,
                                      NULL, NULL, NULL);

  /* reset the cell_layout_data_func (required for highlighting) */
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (gtk_bin_get_child (GTK_BIN (abstract_gallery_view))),
                                      THUNAR_STANDARD_VIEW (abstract_gallery_view)->icon_renderer,
                                      THUNAR_STANDARD_VIEW_GET_CLASS (abstract_gallery_view)->cell_layout_data_func,
                                      NULL, NULL);
}
