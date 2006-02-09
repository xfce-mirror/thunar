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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-icon-view-ui.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_TEXT_BESIDE_ICONS,
};



static void         thunar_icon_view_class_init             (ThunarIconViewClass *klass);
static void         thunar_icon_view_init                   (ThunarIconView      *icon_view);
static void         thunar_icon_view_set_property           (GObject             *object,
                                                             guint                prop_id,
                                                             const GValue        *value,
                                                             GParamSpec          *pspec);
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
static GtkAction   *thunar_icon_view_gesture_action         (ThunarIconView      *icon_view);
static void         thunar_icon_view_action_sort            (GtkAction           *action,
                                                             GtkAction           *current,
                                                             ThunarStandardView  *standard_view);
static gboolean     thunar_icon_view_button_press_event     (ExoIconView         *view,
                                                             GdkEventButton      *event,
                                                             ThunarIconView      *icon_view);
static gboolean     thunar_icon_view_button_release_event   (ExoIconView         *view,
                                                             GdkEventButton      *event,
                                                             ThunarIconView      *icon_view);
static gboolean     thunar_icon_view_expose_event           (ExoIconView         *view,
                                                             GdkEventExpose      *event,
                                                             ThunarIconView      *icon_view);
static gboolean     thunar_icon_view_key_press_event        (ExoIconView         *view,
                                                             GdkEventKey         *event,
                                                             ThunarIconView      *icon_view);
static gboolean     thunar_icon_view_motion_notify_event    (ExoIconView         *view,
                                                             GdkEventMotion      *event,
                                                             ThunarIconView      *icon_view);
static void         thunar_icon_view_item_activated         (ExoIconView         *view,
                                                             GtkTreePath         *path,
                                                             ThunarIconView      *icon_view);
static void         thunar_icon_view_sort_column_changed    (GtkTreeSortable     *sortable,
                                                             ThunarIconView      *icon_view);
static void         thunar_icon_view_zoom_level_changed     (ThunarIconView      *icon_view);



struct _ThunarIconViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarIconView
{
  ThunarStandardView __parent__;

  /* the UI manager merge id for the icon view */
  gint ui_merge_id;

  /* mouse gesture support */
  gint gesture_start_x;
  gint gesture_start_y;
  gint gesture_current_x;
  gint gesture_current_y;
  gint gesture_expose_id;
  gint gesture_motion_id;
  gint gesture_release_id;
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



static GObjectClass *thunar_icon_view_parent_class;



GType
thunar_icon_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarIconViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_icon_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarIconView),
        0,
        (GInstanceInitFunc) thunar_icon_view_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_TYPE_STANDARD_VIEW, I_("ThunarIconView"), &info, 0);
    }

  return type;
}



static void
thunar_icon_view_class_init (ThunarIconViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;
  GObjectClass            *gobject_class;

  /* determine the parent type class */
  thunar_icon_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = thunar_icon_view_set_property;

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
  thunarstandard_view_class->zoom_level_property_name = "last-icon-view-zoom-level";

  /**
   * ThunarIconView::text-beside-icons:
   *
   * Write-only property to specify whether text should be
   * display besides the icon rather than below.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT_BESIDE_ICONS,
                                   g_param_spec_boolean ("text-beside-icons",
                                                         "text-beside-icons",
                                                         "text-beside-icons",
                                                         FALSE,
                                                         EXO_PARAM_WRITABLE));
}



static void
thunar_icon_view_init (ThunarIconView *icon_view)
{
  GtkWidget *view;

  /* stay informed about zoom-level changes, so we can force a re-layout on the icon view */
  g_signal_connect (G_OBJECT (icon_view), "notify::zoom-level", G_CALLBACK (thunar_icon_view_zoom_level_changed), NULL);

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

  /* synchronize the "text-beside-icons" property with the global preference */
  exo_binding_new (G_OBJECT (THUNAR_STANDARD_VIEW (icon_view)->preferences), "misc-text-beside-icons", G_OBJECT (icon_view), "text-beside-icons");
}



static void
thunar_icon_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  switch (prop_id)
    {
    case PROP_TEXT_BESIDE_ICONS:
      if (G_UNLIKELY (g_value_get_boolean (value)))
        {
          exo_icon_view_set_orientation (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), GTK_ORIENTATION_HORIZONTAL);
          g_object_set (G_OBJECT (standard_view->name_renderer), "yalign", 0.5f, NULL);
        }
      else
        {
          exo_icon_view_set_orientation (EXO_ICON_VIEW (GTK_BIN (standard_view)->child), GTK_ORIENTATION_VERTICAL);
          g_object_set (G_OBJECT (standard_view->name_renderer), "yalign", 0.0f, NULL);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static AtkObject*
thunar_icon_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the icon view class */
  object = (*GTK_WIDGET_CLASS (thunar_icon_view_parent_class)->get_accessible) (widget);

  /* set custom Atk properties for the icon view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_description (object, _("Icon based directory listing"));
      atk_object_set_name (object, _("Icon view"));
      atk_object_set_role (object, ATK_ROLE_DIRECTORY_PANE);
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



static GtkAction*
thunar_icon_view_gesture_action (ThunarIconView *icon_view)
{
  if (icon_view->gesture_start_y - icon_view->gesture_current_y > 40
      && ABS (icon_view->gesture_start_x - icon_view->gesture_current_x) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (icon_view)->ui_manager, "/main-menu/go-menu/open-parent");
    }
  else if (icon_view->gesture_start_x - icon_view->gesture_current_x > 40
      && ABS (icon_view->gesture_start_y - icon_view->gesture_current_y) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (icon_view)->ui_manager, "/main-menu/go-menu/back");
    }
  else if (icon_view->gesture_current_x - icon_view->gesture_start_x > 40
      && ABS (icon_view->gesture_start_y - icon_view->gesture_current_y) < 40)
    {
      return gtk_ui_manager_get_action (THUNAR_STANDARD_VIEW (icon_view)->ui_manager, "/main-menu/go-menu/forward");
    }

  return NULL;
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
  GtkTreeIter  iter;
  ThunarFile  *file;
  GtkAction   *action;

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
  else if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS) && event->button == 2)
    {
      /* unselect all currently selected items */
      exo_icon_view_unselect_all (view);

      /* determine the path to the item that was middle-clicked */
      if (exo_icon_view_get_item_at_pos (view, event->x, event->y, &path, NULL))
        {
          /* select only the path to the item on which the user clicked */
          exo_icon_view_select_path (view, path);

          /* if the event was a double-click, then we'll open the file or folder (folder's are opened in new windows) */
          if (G_LIKELY (event->type == GDK_2BUTTON_PRESS))
            {
              /* determine the file for the path */
              gtk_tree_model_get_iter (GTK_TREE_MODEL (THUNAR_STANDARD_VIEW (icon_view)->model), &iter, path);
              file = thunar_list_model_get_file (THUNAR_STANDARD_VIEW (icon_view)->model, &iter);
              if (G_LIKELY (file != NULL))
                {
                  /* determine the action to perform depending on the type of the file */
                  action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (icon_view)->ui_manager,
                      thunar_file_is_directory (file) ? "open-in-new-window" : "open");
      
                  /* emit the action */
                  if (G_LIKELY (action != NULL))
                    gtk_action_activate (action);

                  /* release the file reference */
                  g_object_unref (G_OBJECT (file));
                }
            }

          /* cleanup */
          gtk_tree_path_free (path);
        }
      else if (event->type == GDK_BUTTON_PRESS)
        {
          icon_view->gesture_start_x = icon_view->gesture_current_x = event->x;
          icon_view->gesture_start_y = icon_view->gesture_current_y = event->y;
          icon_view->gesture_expose_id = g_signal_connect_after (G_OBJECT (view), "expose-event",
                                                                 G_CALLBACK (thunar_icon_view_expose_event),
                                                                 G_OBJECT (icon_view));
          icon_view->gesture_motion_id = g_signal_connect (G_OBJECT (view), "motion-notify-event",
                                                           G_CALLBACK (thunar_icon_view_motion_notify_event),
                                                           G_OBJECT (icon_view));
          icon_view->gesture_release_id = g_signal_connect (G_OBJECT (view), "button-release-event",
                                                            G_CALLBACK (thunar_icon_view_button_release_event),
                                                            G_OBJECT (icon_view));
        }

      /* don't run the default handler here */
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_icon_view_button_release_event (ExoIconView    *view,
                                       GdkEventButton *event,
                                       ThunarIconView *icon_view)
{
  GtkAction *action;

  g_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (icon_view->gesture_expose_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_motion_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_release_id > 0, FALSE);

  /* run the selected action (if any) */
  action = thunar_icon_view_gesture_action (icon_view);
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);

  /* unregister the "expose-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), icon_view->gesture_expose_id);
  icon_view->gesture_expose_id = 0;

  /* unregister the "motion-notify-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), icon_view->gesture_motion_id);
  icon_view->gesture_motion_id = 0;

  /* unregister the "button-release-event" handler */
  g_signal_handler_disconnect (G_OBJECT (view), icon_view->gesture_release_id);
  icon_view->gesture_release_id = 0;

  /* redraw the icon view */
  gtk_widget_queue_draw (GTK_WIDGET (view));

  return FALSE;
}



static gboolean
thunar_icon_view_expose_event (ExoIconView    *view,
                               GdkEventExpose *event,
                               ThunarIconView *icon_view)
{
  GtkIconSet *stock_icon_set;
  GtkAction  *action = NULL;
  GdkPixbuf  *stock_icon = NULL;
  gchar      *stock_id;
#if GTK_CHECK_VERSION(2,7,1)
  GdkColor    bg;
  cairo_t    *cr;
#endif

  g_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (icon_view->gesture_expose_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_motion_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_release_id > 0, FALSE);

  /* shade the icon view content while performing mouse gestures */
#if GTK_CHECK_VERSION(2,7,1)
  cr = gdk_cairo_create (event->window);
  bg = GTK_WIDGET (view)->style->base[GTK_STATE_NORMAL];
  cairo_set_source_rgba (cr, bg.red / 65535.0, bg.green / 65535.0, bg.blue / 65535.0, 0.7);
  cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
  cairo_clip (cr);
  cairo_paint (cr);
  cairo_destroy (cr);
#endif

  /* determine the gesture action */
  action = thunar_icon_view_gesture_action (icon_view);
  if (G_LIKELY (action != NULL))
    {
      /* determine the stock icon for the action */
      g_object_get (G_OBJECT (action), "stock-id", &stock_id, NULL);

      /* lookup the icon set for the stock icon */
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
          /* render the stock icon into the icon view window */
          gdk_draw_pixbuf (event->window, NULL, stock_icon, 0, 0,
                           icon_view->gesture_start_x - gdk_pixbuf_get_width (stock_icon) / 2,
                           icon_view->gesture_start_y - gdk_pixbuf_get_height (stock_icon) / 2,
                           gdk_pixbuf_get_width (stock_icon), gdk_pixbuf_get_height (stock_icon),
                           GDK_RGB_DITHER_NONE, 0, 0);

          /* release the stock icon */
          g_object_unref (G_OBJECT (stock_icon));
        }

      /* release the stock id */
      g_free (stock_id);
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



static gboolean
thunar_icon_view_motion_notify_event (ExoIconView    *view,
                                      GdkEventMotion *event,
                                      ThunarIconView *icon_view)
{
  GdkRectangle area;

  g_return_val_if_fail (EXO_IS_ICON_VIEW (view), FALSE);
  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (icon_view->gesture_expose_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_motion_id > 0, FALSE);
  g_return_val_if_fail (icon_view->gesture_release_id > 0, FALSE);

  /* schedule a complete redraw on the first motion event */
  if (icon_view->gesture_current_x == icon_view->gesture_start_x
      && icon_view->gesture_current_y == icon_view->gesture_start_y)
    {
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
  else
    {
      /* otherwise, just redraw the action icon area */
      gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &area.width, &area.height);
      area.x = icon_view->gesture_start_x - area.width / 2;
      area.y = icon_view->gesture_start_y - area.height / 2;
      gdk_window_invalidate_rect (event->window, &area, TRUE);
    }

  /* update the current gesture position */
  icon_view->gesture_current_x = event->x;
  icon_view->gesture_current_y = event->y;

  /* don't execute the default motion notify handler */
  return TRUE;
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
  action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (icon_view)->ui_manager, "open");
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



static void
thunar_icon_view_zoom_level_changed (ThunarIconView *icon_view)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));

  /* we use the same trick as with ThunarDetailsView here, simply because its simple :-) */
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (GTK_BIN (icon_view)->child), THUNAR_STANDARD_VIEW (icon_view)->icon_renderer, NULL, NULL, NULL);
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


