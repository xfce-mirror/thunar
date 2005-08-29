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

#include <thunar/thunar-details-view.h>
#include <thunar/thunar-text-renderer.h>



static void       thunar_details_view_class_init          (ThunarDetailsViewClass *klass);
static void       thunar_details_view_init                (ThunarDetailsView      *details_view);
static AtkObject *thunar_details_view_get_accessible      (GtkWidget              *widget);
static GList     *thunar_details_view_get_selected_items  (ThunarStandardView     *standard_view);
static void       thunar_details_view_select_all          (ThunarStandardView     *standard_view);
static void       thunar_details_view_unselect_all        (ThunarStandardView     *standard_view);
static void       thunar_details_view_select_path         (ThunarStandardView     *standard_view,
                                                           GtkTreePath            *path);
static void       thunar_details_view_notify_model        (GtkTreeView            *tree_view,
                                                           GParamSpec             *pspec,
                                                           ThunarDetailsView      *details_view);
static gboolean   thunar_details_view_button_press_event  (GtkTreeView            *tree_view,
                                                           GdkEventButton         *event,
                                                           ThunarDetailsView      *details_view);
static gboolean   thunar_details_view_key_press_event     (GtkTreeView            *tree_view,
                                                           GdkEventKey            *event,
                                                           ThunarDetailsView      *details_view);
static void       thunar_details_view_row_activated       (GtkTreeView            *tree_view,
                                                           GtkTreePath            *path,
                                                           GtkTreeViewColumn      *column,
                                                           ThunarDetailsView      *details_view);



struct _ThunarDetailsViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarDetailsView
{
  ThunarStandardView __parent__;
};



G_DEFINE_TYPE (ThunarDetailsView, thunar_details_view, THUNAR_TYPE_STANDARD_VIEW);



static void
thunar_details_view_class_init (ThunarDetailsViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_details_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->get_selected_items = thunar_details_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_details_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_details_view_unselect_all;
  thunarstandard_view_class->select_path = thunar_details_view_select_path;
}



static void
thunar_details_view_init (ThunarDetailsView *details_view)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkWidget         *tree_view;

  /* create the tree view to embed */
  tree_view = gtk_tree_view_new ();
  g_signal_connect (G_OBJECT (tree_view), "notify::model",
                    G_CALLBACK (thunar_details_view_notify_model), details_view);
  g_signal_connect (G_OBJECT (tree_view), "button-press-event",
                    G_CALLBACK (thunar_details_view_button_press_event), details_view);
  g_signal_connect (G_OBJECT (tree_view), "key-press-event",
                    G_CALLBACK (thunar_details_view_key_press_event), details_view);
  g_signal_connect (G_OBJECT (tree_view), "row-activated",
                    G_CALLBACK (thunar_details_view_row_activated), details_view);
  gtk_container_add (GTK_CONTAINER (details_view), tree_view);
  gtk_widget_show (tree_view);

  /* configure general aspects of the details view */
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree_view), TRUE);

  /* first column (icon, name) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "expand", TRUE,
                         "resizable", TRUE,
                         "title", _("Name"),
                         NULL);
  gtk_tree_view_column_pack_start (column, THUNAR_STANDARD_VIEW (details_view)->icon_renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, THUNAR_STANDARD_VIEW (details_view)->icon_renderer,
                                       "file", THUNAR_LIST_MODEL_COLUMN_FILE,
                                       NULL);
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->name_renderer),
                "xalign", 0.0,
                NULL);
  gtk_tree_view_column_pack_start (column, THUNAR_STANDARD_VIEW (details_view)->name_renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, THUNAR_STANDARD_VIEW (details_view)->name_renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* second column (size) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "resizable", TRUE,
                         "title", _("Size"),
                         NULL);
  renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER,
                           "xalign", 1.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_SIZE,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_SIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* third column (permissions) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "resizable", TRUE,
                         "title", _("Permissions"),
                         NULL);
  renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER,
                           "xalign", 0.0,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_PERMISSIONS,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_PERMISSIONS);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* fourth column (type) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "resizable", TRUE,
                         "title", _("Type"),
                         NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_TYPE,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_TYPE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* fifth column (modification date) */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "resizable", TRUE,
                         "title", _("Date modified"),
                         NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_LIST_MODEL_COLUMN_DATE_MODIFIED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* configure the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
                            G_CALLBACK (thunar_standard_view_selection_changed), details_view);
}



static AtkObject*
thunar_details_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the tree view class */
  object = GTK_WIDGET_CLASS (thunar_details_view_parent_class)->get_accessible (widget);

  /* set custom Atk properties for the details view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_name (object, _("Details view"));
      atk_object_set_description (object, _("Detailed directory listing"));
    }

  return object;
}



static GList*
thunar_details_view_get_selected_items (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  g_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  return gtk_tree_selection_get_selected_rows (selection, NULL);
}



static void
thunar_details_view_select_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  g_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  gtk_tree_selection_select_all (selection);
}



static void
thunar_details_view_unselect_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  g_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  gtk_tree_selection_unselect_all (selection);
}



static void
thunar_details_view_select_path (ThunarStandardView *standard_view,
                                 GtkTreePath        *path)
{
  GtkTreeSelection *selection;

  g_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  gtk_tree_selection_select_path (selection, path);
}



static void
thunar_details_view_notify_model (GtkTreeView       *tree_view,
                                  GParamSpec        *pspec,
                                  ThunarDetailsView *details_view)
{
  /* We need to set the search column here, as GtkTreeView resets it
   * whenever a new model is set.
   */
  gtk_tree_view_set_search_column (tree_view, THUNAR_LIST_MODEL_COLUMN_NAME);
}



static gboolean
thunar_details_view_button_press_event (GtkTreeView       *tree_view,
                                        GdkEventButton    *event,
                                        ThunarDetailsView *details_view)
{
  GtkTreeSelection *selection;
  GtkTreePath      *path;

  /* we unselect all selected items if the user clicks on an empty
   * area of the treeview and no modifier key is active.
   */
  if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0
      && !gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, NULL, NULL, NULL, NULL))
    {
      selection = gtk_tree_view_get_selection (tree_view);
      gtk_tree_selection_unselect_all (selection);
    }

  /* open the context menu on right clicks */
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      selection = gtk_tree_view_get_selection (tree_view);
      if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path, NULL, NULL, NULL))
        {
          /* select the path on which the user clicked if not selected yet */
          if (!gtk_tree_selection_path_is_selected (selection, path))
            {
              /* we don't unselect all other items if Control is active */
              if ((event->state & GDK_CONTROL_MASK) == 0)
                gtk_tree_selection_unselect_all (selection);
              gtk_tree_selection_select_path (selection, path);
            }
          gtk_tree_path_free (path);
        }

      /* open the context menu */
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view), event->button, event->time);

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_details_view_key_press_event (GtkTreeView       *tree_view,
                                     GdkEventKey       *event,
                                     ThunarDetailsView *details_view)
{
  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view), 0, event->time);
      return TRUE;
    }

  return FALSE;
}



static void
thunar_details_view_row_activated (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column,
                                   ThunarDetailsView *details_view)
{
  GtkTreeSelection *selection;
  GtkAction        *action;

  g_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  /* be sure to have only the double clicked item selected */
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_path (selection, path);

  /* emit the "open" action */
  action = gtk_action_group_get_action (THUNAR_STANDARD_VIEW (details_view)->action_group, "open");
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);
}



/**
 * thunar_details_view_new:
 *
 * Allocates a new #ThunarDetailsView instance, which is not
 * associated with any #ThunarListModel yet.
 *
 * Return value: the newly allocated #ThunarDetailsView instance.
 **/
GtkWidget*
thunar_details_view_new (void)
{
  return g_object_new (THUNAR_TYPE_DETAILS_VIEW, NULL);
}



