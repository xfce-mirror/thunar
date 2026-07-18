/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 The Xfce Development Team
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

#include "thunar/thunar-context-menu-order-editor.h"

#include "thunar/thunar-context-menu-order-model.h"
#include "thunar/thunar-order-editor.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-uca-editor.h"

#include <libxfce4ui/libxfce4ui.h>



struct _ThunarContextMenuOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarContextMenuOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarContextMenuOrderModel *order_model;
};



static void
thunar_context_menu_order_editor_finalize (GObject *object);

static gboolean
thunar_context_menu_order_editor_remove (ThunarContextMenuOrderEditor *menu_editor,
                                         gint                         *indexes,
                                         gint                          n_indexes);


G_DEFINE_TYPE (ThunarContextMenuOrderEditor, thunar_context_menu_order_editor, THUNAR_TYPE_ORDER_EDITOR)



static void
thunar_context_menu_order_editor_class_init (ThunarContextMenuOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_context_menu_order_editor_finalize;
}



static void
thunar_context_menu_order_editor_init (ThunarContextMenuOrderEditor *menu_editor)
{
  GtkWidget *label;

  menu_editor->order_model = thunar_context_menu_order_model_get_default ();

  label = gtk_label_new (_("Setting the order and visibility of context menu elements. Menu items are grouped into sections. Note that most context menu items only will be shown conditionally, based on the file type, the number of selected items and other criteria."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_container_add (thunar_order_editor_get_description_area (THUNAR_ORDER_EDITOR (menu_editor)), label);
  gtk_widget_show (label);

  g_object_set (menu_editor, "title", _("Configure the context menu"), NULL);
}



static void
thunar_context_menu_order_editor_finalize (GObject *object)
{
  ThunarContextMenuOrderEditor *menu_editor = THUNAR_CONTEXT_MENU_ORDER_EDITOR (object);

  g_signal_handlers_disconnect_by_data (menu_editor->order_model, menu_editor);
  g_object_unref (menu_editor->order_model);

  G_OBJECT_CLASS (thunar_context_menu_order_editor_parent_class)->finalize (object);
}



static void
thunar_context_menu_order_editor_insert_item (ThunarContextMenuOrderEditor          *menu_editor,
                                              gint                                   index,
                                              const ThunarContextMenuOrderModelItem *item)
{
  XfceItemListModel *item_view_model = thunar_order_editor_get_model (THUNAR_ORDER_EDITOR (menu_editor));
  GIcon             *icon = item->icon != NULL ? g_themed_icon_new (item->icon) : NULL;
  gboolean           editable = g_str_has_prefix (item->id, "custom-action-uca-");

  xfce_item_list_store_insert_with_values (XFCE_ITEM_LIST_STORE (item_view_model), index,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE, item->visibility,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE, TRUE,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ICON, icon,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_NAME, gettext (item->name),
                                           XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP, item->tooltip,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_REMOVABLE, item->removable,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_EDITABLE, editable,
                                           -1);
  g_clear_object (&icon);
}



static void
thunar_context_menu_order_editor_populate (ThunarContextMenuOrderEditor *menu_editor)
{
  XfceItemListModel *item_view_model = thunar_order_editor_get_model (THUNAR_ORDER_EDITOR (menu_editor));
  GList             *items = thunar_context_menu_order_model_get_items (menu_editor->order_model);

  g_signal_handlers_block_by_func (item_view_model, thunar_context_menu_order_editor_remove, menu_editor);
  xfce_item_list_store_clear (XFCE_ITEM_LIST_STORE (item_view_model));
  g_signal_handlers_unblock_by_func (item_view_model, thunar_context_menu_order_editor_remove, menu_editor);

  for (GList *l = items; l != NULL; l = l->next)
    {
      /* insert an item at the end */
      thunar_context_menu_order_editor_insert_item (menu_editor, -1, l->data);
    }

  g_list_free (items);
}



static void
thunar_context_menu_order_editor_move (ThunarContextMenuOrderEditor *menu_editor,
                                       gint                          source_index,
                                       gint                          dest_index)
{
  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  thunar_context_menu_order_model_move (menu_editor->order_model, source_index, dest_index);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
}



static void
thunar_context_menu_order_editor_set_visibility (ThunarContextMenuOrderEditor *menu_editor,
                                                 gint                          index,
                                                 gboolean                      visibility)
{
  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  thunar_context_menu_order_model_set_visibility (menu_editor->order_model, index, visibility);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
}


static gboolean
thunar_context_menu_order_editor_remove (ThunarContextMenuOrderEditor *menu_editor,
                                         gint                         *indexes,
                                         gint                          n_items)
{
  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  thunar_context_menu_order_model_remove (menu_editor->order_model, indexes, n_items);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);

  return FALSE;
}



static gboolean
thunar_context_menu_order_editor_edit (ThunarContextMenuOrderEditor *menu_editor,
                                       gint                          index)
{
  GList                           *items = thunar_context_menu_order_model_get_items (menu_editor->order_model);
  ThunarContextMenuOrderModelItem *item = g_list_nth_data (items, index);
  const gchar                     *unique_id = thunar_context_menu_order_model_item_get_uca_unique_id (item);
  XfceItemListView                *item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (menu_editor));
  GtkWidget                       *tree_view = xfce_item_list_view_get_tree_view (item_view);
  XfceItemListModel               *model = xfce_item_list_view_get_model (item_view);
  GtkTreeIter                      iter;
  GtkTreePath                     *path;

  /* show dialog */
  thunar_uca_editor_show (GTK_WINDOW (menu_editor), unique_id, NULL);

  /* refresh */
  thunar_context_menu_order_model_load (menu_editor->order_model);
  thunar_context_menu_order_editor_populate (menu_editor);

  /* set cursor */
  xfce_item_list_model_set_index (model, &iter, index);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);

  g_list_free (items);
  gtk_tree_path_free (path);

  return FALSE;
}



static void
thunar_context_menu_order_editor_add_separator (ThunarContextMenuOrderEditor *menu_editor)
{
  XfceItemListView                *item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (menu_editor));
  gint                            *sel_items = NULL;
  gint                             n_sel_items = xfce_item_list_view_get_selected_items (item_view, &sel_items);
  gint                             index;
  gint                             separator_index;
  GList                           *items;
  ThunarContextMenuOrderModelItem *item;
  GtkTreePath                     *path;
  GtkTreeView                     *tree_view;
  GtkTreeSelection                *selection;

  if (n_sel_items > 0)
    {
      /* insert an item after the last selected item */
      index = sel_items[n_sel_items - 1] + 1;
    }
  else
    {
      /* insert an item at the end */
      index = -1;
    }

  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  separator_index = thunar_context_menu_order_model_insert_separator (menu_editor->order_model, index);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);

  items = thunar_context_menu_order_model_get_items (menu_editor->order_model);
  item = g_list_nth_data (items, separator_index);
  thunar_context_menu_order_editor_insert_item (menu_editor, separator_index, item);
  g_list_free (items);

  path = gtk_tree_path_new_from_indices (separator_index, -1);
  tree_view = GTK_TREE_VIEW (xfce_item_list_view_get_tree_view (item_view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
  gtk_tree_path_free (path);

  g_free (sel_items);
}



static void
thunar_context_menu_order_editor_add_uca (ThunarContextMenuOrderEditor *menu_editor)
{
  XfceItemListView  *item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (menu_editor));
  GtkWidget         *tree_view = xfce_item_list_view_get_tree_view (item_view);
  XfceItemListModel *model = xfce_item_list_view_get_model (item_view);
  gchar             *new_unique_id = NULL;
  GList             *items;
  gint               index;
  GtkTreeIter        iter;
  GtkTreePath       *path;

  /* show dialog */
  thunar_uca_editor_show (GTK_WINDOW (menu_editor), NULL, &new_unique_id);

  /* refresh */
  thunar_context_menu_order_model_load (menu_editor->order_model);
  thunar_context_menu_order_editor_populate (menu_editor);

  /* place the cursor on the new item */
  items = thunar_context_menu_order_model_get_items (menu_editor->order_model);
  index = 0;
  for (GList *l = items; l != NULL; l = l->next, ++index)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (g_str_has_prefix (item->id, "custom-action-uca-"))
        {
          const gchar *item_unique_id = thunar_context_menu_order_model_item_get_uca_unique_id (item);

          if (g_str_equal (item_unique_id, new_unique_id))
            break;
        }
    }

  xfce_item_list_model_set_index (model, &iter, index);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);

  /* cleanup */
  g_free (new_unique_id);
  g_list_free (items);
  gtk_tree_path_free (path);
}



static void
thunar_context_menu_order_editor_reset (ThunarContextMenuOrderEditor *menu_editor)
{
  thunar_context_menu_order_model_reset (menu_editor->order_model);
}



void
thunar_context_menu_order_editor_show (GtkWidget *window)
{
  XfceItemListStore            *item_view_model = xfce_item_list_store_new (-1);
  XfceItemListView             *item_view;
  ThunarContextMenuOrderEditor *menu_editor;
  GMenu                        *menu;
  GMenuItem                    *menu_item;
  GIcon                        *icon;
  GActionGroup                 *group;
  GSimpleAction                *action;
  GtkTreeView                  *tree_view;
  GtkTreeSelection             *selection;

  menu_editor = g_object_new (THUNAR_TYPE_CONTEXT_MENU_ORDER_EDITOR, "model", item_view_model, NULL);
  g_object_unref (item_view_model); /* "menu_editor" now owns the "item_view_model" */

  item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (menu_editor));
  xfce_item_list_view_set_label_visibility (item_view, FALSE);

  g_object_set (item_view_model,
                "list-flags",
                XFCE_ITEM_LIST_MODEL_REORDERABLE
                | XFCE_ITEM_LIST_MODEL_REMOVABLE
                | XFCE_ITEM_LIST_MODEL_RESETTABLE
                | XFCE_ITEM_LIST_MODEL_EDITABLE,
                NULL);
  g_signal_connect_swapped (item_view_model, "before-move-item", G_CALLBACK (thunar_context_menu_order_editor_move), menu_editor);
  g_signal_connect_swapped (item_view_model, "activity-changed", G_CALLBACK (thunar_context_menu_order_editor_set_visibility), menu_editor);
  g_signal_connect_swapped (item_view, "remove-items", G_CALLBACK (thunar_context_menu_order_editor_remove), menu_editor);
  g_signal_connect_swapped (item_view, "edit-item", G_CALLBACK (thunar_context_menu_order_editor_edit), menu_editor);
  g_signal_connect_swapped (item_view_model, "reset", G_CALLBACK (thunar_context_menu_order_editor_reset), menu_editor);
  g_signal_connect_swapped (menu_editor->order_model, "changed", G_CALLBACK (thunar_context_menu_order_editor_populate), menu_editor);

  /* menu items */
  menu = xfce_item_list_view_get_menu (item_view);

  menu_item = g_menu_item_new (_("Add separator"), "xfce-item-list-view.add-separator");
  g_menu_item_set_attribute (menu_item, XFCE_MENU_ATTRIBUTE_TOOLTIP, "s", _("Add separator"));
  icon = g_themed_icon_new ("list-add-symbolic");
  g_menu_item_set_icon (menu_item, icon);
  g_clear_object (&icon);
  g_menu_insert_item (menu, 0, menu_item);
  g_object_unref (menu_item);

  menu_item = g_menu_item_new (_("Add custom action"), "xfce-item-list-view.add-user-custom-action");
  g_menu_item_set_attribute (menu_item, XFCE_MENU_ATTRIBUTE_TOOLTIP, "s", _("Add user custom action"));
  icon = g_themed_icon_new ("application-add-symbolic");
  g_menu_item_set_icon (menu_item, icon);
  g_clear_object (&icon);
  g_menu_insert_item (menu, 1, menu_item);
  g_object_unref (menu_item);

  /* actions */
  group = gtk_widget_get_action_group (GTK_WIDGET (item_view), "xfce-item-list-view");

  action = g_simple_action_new ("add-separator", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (thunar_context_menu_order_editor_add_separator), menu_editor);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);

  action = g_simple_action_new ("add-user-custom-action", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (thunar_context_menu_order_editor_add_uca), menu_editor);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);

  /* tree view */
  tree_view = GTK_TREE_VIEW (xfce_item_list_view_get_tree_view (item_view));
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  thunar_context_menu_order_editor_populate (menu_editor);

  thunar_order_editor_show (THUNAR_ORDER_EDITOR (menu_editor), window);
}
