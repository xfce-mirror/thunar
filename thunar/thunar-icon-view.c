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

#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-icon-view.h>
#include <thunar/thunar-text-renderer.h>



static void       thunar_icon_view_class_init         (ThunarIconViewClass *klass);
static void       thunar_icon_view_init               (ThunarIconView      *icon_view);
static AtkObject *thunar_icon_view_get_accessible     (GtkWidget           *widget);
static GList     *thunar_icon_view_get_selected_items (ThunarStandardView  *standard_view);
static void       thunar_icon_view_select_all         (ThunarStandardView  *standard_view);
static void       thunar_icon_view_unselect_all       (ThunarStandardView  *standard_view);
static void       thunar_icon_view_select_path        (ThunarStandardView  *standard_view,
                                                       GtkTreePath         *path);
static gboolean   thunar_icon_view_button_press_event (ExoIconView         *view,
                                                       GdkEventButton      *event,
                                                       ThunarIconView      *icon_view);
static gboolean   thunar_icon_view_key_press_event    (ExoIconView         *view,
                                                       GdkEventKey         *event,
                                                       ThunarIconView      *icon_view);
static void       thunar_icon_view_item_activated     (ExoIconView         *view,
                                                       GtkTreePath         *path,
                                                       ThunarIconView      *icon_view);



struct _ThunarIconViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarIconView
{
  ThunarStandardView __parent__;
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
  thunarstandard_view_class->get_selected_items = thunar_icon_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_icon_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_icon_view_unselect_all;
  thunarstandard_view_class->select_path = thunar_icon_view_select_path;
}



static void
thunar_icon_view_init (ThunarIconView *icon_view)
{
  GtkCellRenderer *renderer;
  GtkWidget       *view;

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
  renderer = g_object_new (THUNAR_TYPE_ICON_RENDERER,
                           "follow-state", TRUE,
                           "size", 48,
                           NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), renderer, "file", THUNAR_LIST_MODEL_COLUMN_FILE);

  /* add the text renderer */
  renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER,
                           "follow-state", TRUE,
                           "wrap-mode", PANGO_WRAP_WORD_CHAR,
                           "wrap-width", 128,
                           "yalign", 0.0f,
                           NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), renderer, "text", THUNAR_LIST_MODEL_COLUMN_NAME);
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



static gboolean
thunar_icon_view_button_press_event (ExoIconView    *view,
                                     GdkEventButton *event,
                                     ThunarIconView *icon_view)
{
  GtkTreePath *path;

  /* open the context menu on right clicks */
  if (event->type == GDK_BUTTON_PRESS && event->button == 3
      && exo_icon_view_get_item_at_pos (view, event->x, event->y, &path, NULL))
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

      /* open the context menu */
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (icon_view), event->button, event->time);

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


