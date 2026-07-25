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

#include "thunar/thunar-toolbar-order-editor.h"

#include "thunar/thunar-application.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-uca-editor.h"
#include "thunar/thunar-uca-model.h"
#include "thunar/thunar-window.h"

#include <libxfce4ui/libxfce4ui.h>



struct _ThunarToolbarOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarToolbarOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarApplication *application;
  ThunarPreferences *preferences;
  GtkWidget         *toolbar;
  GList             *children;
  XfceItemListStore *store;

  /* If TRUE, thunar_toolbar_order_editor_refresh() will only update pointers and the children list, without completely
   * rebuilding the store via clear-and-refill. Used to prevent interface jitter when editing list items. */
  gboolean block_store_refill;
};

enum
{
  PROP_0,
  PROP_TOOLBAR,
};



static void
thunar_toolbar_order_editor_finalize (GObject *object);

static void
thunar_toolbar_order_editor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec);

static void
thunar_toolbar_order_editor_help (ThunarOrderEditor *order_editor);

static void
thunar_toolbar_order_editor_refresh (ThunarToolbarOrderEditor *toolbar_editor);

static void
thunar_toolbar_order_editor_move (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      source_index,
                                  gint                      dest_index);

static void
thunar_toolbar_order_editor_set_item_visibility (ThunarToolbarOrderEditor *toolbar_editor,
                                                 gint                      index,
                                                 gboolean                  value);

static gint
thunar_toolbar_order_editor_compare_order (GObject *a,
                                           GObject *b);

static void
thunar_toolbar_order_editor_reset (ThunarToolbarOrderEditor *toolbar_editor);

static gboolean
thunar_toolbar_order_editor_remove (ThunarToolbarOrderEditor *toolbar_editor,
                                    gint                     *indexes,
                                    gint                      n_items);

static gboolean
thunar_toolbar_order_editor_edit (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      index);

static void
thunar_toolbar_order_editor_add_uca (ThunarToolbarOrderEditor *toolbar_editor);

static void
thunar_toolbar_order_editor_save (ThunarToolbarOrderEditor *toolbar_editor);


G_DEFINE_TYPE (ThunarToolbarOrderEditor, thunar_toolbar_order_editor, THUNAR_TYPE_ORDER_EDITOR)



static void
thunar_toolbar_order_editor_class_init (ThunarToolbarOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_toolbar_order_editor_finalize;
  object_class->set_property = thunar_toolbar_order_editor_set_property;

  g_object_class_install_property (object_class,
                                   PROP_TOOLBAR,
                                   g_param_spec_object ("toolbar", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}



static void
thunar_toolbar_order_editor_init (ThunarToolbarOrderEditor *toolbar_editor)
{
  ThunarOrderEditor *order_editor = THUNAR_ORDER_EDITOR (toolbar_editor);
  GtkWidget         *label;
  GtkWidget         *button;
  XfceItemListView  *item_view;
  GMenu             *menu;
  GMenuItem         *menu_item;
  GIcon             *icon;
  GActionGroup      *group;
  GSimpleAction     *action;
  GtkWidget         *tree_view;
  GtkTreeSelection  *selection;

  toolbar_editor->application = thunar_application_get ();
  toolbar_editor->preferences = thunar_preferences_get ();

  label = gtk_label_new (_("Configure the order and visibility of toolbar items.\n"
                           "Note that toolbar items are always executed for the current directory."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_container_add (thunar_order_editor_get_description_area (order_editor), label);
  gtk_widget_show (label);

  button = gtk_check_button_new_with_mnemonic (_("_Symbolic Icons"));
  g_object_bind_property (G_OBJECT (toolbar_editor->preferences),
                          "misc-symbolic-icons-in-toolbar",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Use symbolic icons instead of regular ones (if available)."));
  gtk_container_add (thunar_order_editor_get_settings_area (order_editor), button);
  gtk_widget_show (button);

  g_signal_connect (toolbar_editor, "help", G_CALLBACK (thunar_toolbar_order_editor_help), NULL);
  g_object_set (toolbar_editor,
                "title", _("Configure the Toolbar"),
                "help-enabled", TRUE,
                NULL);

  /* item view*/
  item_view = thunar_order_editor_get_item_view (order_editor);
  xfce_item_list_view_set_label_visibility (item_view, FALSE);
  g_signal_connect_swapped (item_view, "remove-items", G_CALLBACK (thunar_toolbar_order_editor_remove), toolbar_editor);
  g_signal_connect_swapped (item_view, "edit-item", G_CALLBACK (thunar_toolbar_order_editor_edit), toolbar_editor);

  /* store */
  toolbar_editor->store = xfce_item_list_store_new (-1);
  g_object_set (toolbar_editor, "model", toolbar_editor->store, NULL);
  g_object_set (toolbar_editor->store, "list-flags",
                XFCE_ITEM_LIST_MODEL_REORDERABLE
                | XFCE_ITEM_LIST_MODEL_RESETTABLE
                | XFCE_ITEM_LIST_MODEL_REMOVABLE
                | XFCE_ITEM_LIST_MODEL_EDITABLE,
                NULL);
  g_signal_connect_swapped (toolbar_editor->store, "before-move-item", G_CALLBACK (thunar_toolbar_order_editor_move), toolbar_editor);
  g_signal_connect_swapped (toolbar_editor->store, "activity-changed", G_CALLBACK (thunar_toolbar_order_editor_set_item_visibility), toolbar_editor);
  g_signal_connect_swapped (toolbar_editor->store, "reset", G_CALLBACK (thunar_toolbar_order_editor_reset), toolbar_editor);
  g_signal_connect_swapped (toolbar_editor->preferences, "notify::last-toolbar-items", G_CALLBACK (thunar_toolbar_order_editor_refresh), toolbar_editor);
  g_object_unref (toolbar_editor->store);

  /* menu items */
  menu = xfce_item_list_view_get_menu (item_view);

  menu_item = g_menu_item_new (_("Add custom action"), "xfce-item-list-view.add-user-custom-action");
  g_menu_item_set_attribute (menu_item, XFCE_MENU_ATTRIBUTE_TOOLTIP, "s", _("Add user custom action"));
  icon = g_themed_icon_new ("application-add-symbolic");
  g_menu_item_set_icon (menu_item, icon);
  g_clear_object (&icon);
  g_menu_insert_item (menu, 1, menu_item);
  g_object_unref (menu_item);

  /* actions */
  group = gtk_widget_get_action_group (GTK_WIDGET (item_view), "xfce-item-list-view");

  action = g_simple_action_new ("add-user-custom-action", NULL);
  g_signal_connect_swapped (action, "activate", G_CALLBACK (thunar_toolbar_order_editor_add_uca), toolbar_editor);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
  g_object_unref (action);

  /* tree view */
  tree_view = xfce_item_list_view_get_tree_view (item_view);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
}



static void
thunar_toolbar_order_editor_finalize (GObject *object)
{
  ThunarToolbarOrderEditor *toolbar_editor = THUNAR_TOOLBAR_ORDER_EDITOR (object);

  g_signal_handlers_disconnect_by_data (toolbar_editor->preferences, toolbar_editor);
  g_clear_object (&toolbar_editor->application);
  g_clear_object (&toolbar_editor->preferences);
  g_clear_object (&toolbar_editor->toolbar);
  g_clear_pointer (&toolbar_editor->children, g_list_free);

  G_OBJECT_CLASS (thunar_toolbar_order_editor_parent_class)->finalize (object);
}



static void
thunar_toolbar_order_editor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ThunarToolbarOrderEditor *toolbar_editor = THUNAR_TOOLBAR_ORDER_EDITOR (object);

  switch (prop_id)
    {
    case PROP_TOOLBAR:
      g_clear_object (&toolbar_editor->toolbar);
      toolbar_editor->toolbar = g_value_dup_object (value);
      thunar_toolbar_order_editor_refresh (toolbar_editor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_toolbar_order_editor_help (ThunarOrderEditor *order_editor)
{
  xfce_dialog_show_help (GTK_WINDOW (order_editor),
                         "thunar",
                         "the-file-manager-window",
                         "toolbar_customization");
}



static void
thunar_toolbar_order_editor_insert_item (ThunarToolbarOrderEditor *toolbar_editor,
                                         gint                      index,
                                         GtkWidget                *item)
{
  const gchar *id = g_object_get_data (G_OBJECT (item), "id");
  gboolean     is_menu = g_strcmp0 (id, "menu") == 0;
  const gchar *icon_name = g_object_get_data (G_OBJECT (item), "icon");
  GIcon       *icon = !xfce_str_is_empty (icon_name) ? g_themed_icon_new (icon_name) : NULL;
  const gchar *name = g_object_get_data (G_OBJECT (item), "label");
  GtkWidget   *label = gtk_label_new_with_mnemonic (name);
  const gchar *tooltip = is_menu ? _("Only visible when the menubar is hidden") : NULL;
  gboolean     is_uca = g_str_has_prefix (id, "uca-action-");

  xfce_item_list_store_insert_with_values (toolbar_editor->store, index,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE, is_menu || gtk_widget_is_visible (item),
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE, !is_menu,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_ICON, icon,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_NAME, gtk_label_get_text (GTK_LABEL (label)),
                                           XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP, tooltip,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_REMOVABLE, is_uca,
                                           XFCE_ITEM_LIST_MODEL_COLUMN_EDITABLE, is_uca,
                                           -1);

  g_clear_object (&icon);
  g_object_ref_sink (label);
  g_object_unref (label);
}



static void
thunar_toolbar_order_editor_refresh (ThunarToolbarOrderEditor *toolbar_editor)

{
  _thunar_return_if_fail (toolbar_editor->toolbar != NULL);

  g_clear_pointer (&toolbar_editor->children, g_list_free);
  toolbar_editor->children = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));

  if (toolbar_editor->block_store_refill)
    return;

  xfce_item_list_store_clear (toolbar_editor->store);

  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
    thunar_toolbar_order_editor_insert_item (toolbar_editor, -1, GTK_WIDGET (l->data));
}



static void
thunar_toolbar_order_editor_move (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      source_index,
                                  gint                      dest_index)
{
  GList *windows = thunar_application_get_windows (toolbar_editor->application);

  /* Changes the order for all windows */
  for (GList *l = windows; l != NULL; l = l->next)
    thunar_window_toolbar_move_item (THUNAR_WINDOW (l->data), source_index, dest_index);

  g_list_free (windows);

  g_clear_pointer (&toolbar_editor->children, g_list_free);
  toolbar_editor->children = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));

  thunar_toolbar_order_editor_save (toolbar_editor);
}



static void
thunar_toolbar_order_editor_set_item_visibility (ThunarToolbarOrderEditor *toolbar_editor,
                                                 gint                      index,
                                                 gboolean                  value)
{
  GList *windows = thunar_application_get_windows (toolbar_editor->application);

  for (GList *l = windows; l != NULL; l = l->next)
    thunar_window_toolbar_toggle_item_visibility (THUNAR_WINDOW (l->data), index);

  g_list_free (windows);

  thunar_toolbar_order_editor_save (toolbar_editor);
}



static gint
thunar_toolbar_order_editor_compare_order (GObject *a,
                                           GObject *b)
{
  gint a_order = *(const gint *) g_object_get_data (a, "default-order");
  gint b_order = *(const gint *) g_object_get_data (b, "default-order");

  return (a_order > b_order) - (a_order < b_order);
}



static void
thunar_toolbar_order_editor_reset (ThunarToolbarOrderEditor *toolbar_editor)
{
  GList *new_order = NULL;
  gint   index = 0;

  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
    new_order = g_list_insert_sorted (new_order, l->data, (GCompareFunc) thunar_toolbar_order_editor_compare_order);

  for (GList *l = new_order; l != NULL; l = l->next, ++index)
    xfce_item_list_model_move (XFCE_ITEM_LIST_MODEL (toolbar_editor->store), g_list_index (toolbar_editor->children, l->data), index);

  g_list_free (new_order);

  thunar_toolbar_order_editor_save (toolbar_editor);
  thunar_toolbar_order_editor_refresh (toolbar_editor);
}



static gboolean
thunar_toolbar_order_editor_remove (ThunarToolbarOrderEditor *toolbar_editor,
                                    gint                     *indexes,
                                    gint                      n_items)
{
  ThunarUcaModel *uca_model = thunar_uca_model_get_default ();
  GtkWidget      *dialog;
  gint            response;
  gboolean        stop_event;

  /* create the question dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (toolbar_editor),
                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                   | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Are you sure you want to delete the selected actions?"));

  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete action"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("If you delete a custom action, it is permanently lost."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Delete"), GTK_RESPONSE_YES, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* if the user clicked cancel, stop event propagation; otherwise, allow XfceItemListView to remove items
   * from the store */
  stop_event = response != GTK_RESPONSE_YES;

  if (!stop_event)
    {
      /* lock uca_model for exclusive write access */
      if (thunar_uca_editor_lock_model (GTK_WINDOW (toolbar_editor), uca_model))
        {
          /* Block the full store update triggered by UCA changes; store deletions will be performed selectively when this
           * function returns FALSE */
          toolbar_editor->block_store_refill = TRUE;

          /* deletion in reverse order to maintain index validity */
          for (gint i = n_items - 1; i >= 0; --i)
            {
              GtkWidget   *item = g_list_nth_data (toolbar_editor->children, indexes[i]);
              const gchar *item_id = g_object_get_data (G_OBJECT (item), "id");
              const gchar *unique_id = item_id + strlen ("uca-action-");
              GtkTreeIter  iter;

              if (thunar_uca_model_get_iter_by_unique_id (uca_model, &iter, unique_id))
                {
                  gint index = xfce_item_list_model_get_index (XFCE_ITEM_LIST_MODEL (uca_model), &iter);

                  /* remove uca item */
                  xfce_item_list_model_remove (XFCE_ITEM_LIST_MODEL (uca_model), index);
                }
            }

          /* save uca model */
          thunar_uca_editor_save_persistently (GTK_WINDOW (toolbar_editor), uca_model);

          /* save toolbar */
          thunar_toolbar_order_editor_save (toolbar_editor);

          toolbar_editor->block_store_refill = FALSE;

          /* unlocking the uca_model */
          thunar_uca_model_unlock (uca_model);
        }
      else
        {
          stop_event = TRUE;
        }
    }

  /* cleanup */
  g_object_unref (uca_model);

  return stop_event;
}



gboolean
thunar_toolbar_order_editor_edit (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      index)
{
  ThunarUcaModel *uca_model = thunar_uca_model_get_default ();
  GtkWidget      *item = g_list_nth_data (toolbar_editor->children, index);
  const gchar    *item_id = g_object_get_data (G_OBJECT (item), "id");
  const gchar    *unique_id = item_id + strlen ("uca-action-");
  gboolean        stop_event = FALSE;

  /* lock uca_model for exclusive write access */
  if (thunar_uca_editor_lock_model (GTK_WINDOW (toolbar_editor), uca_model))
    {
      /* Show dialog. Block the store update triggered by the UCA update; instead, we will manually remove and recreate
       * the item being updated */
      toolbar_editor->block_store_refill = TRUE;
      if (thunar_uca_editor_show (GTK_WINDOW (toolbar_editor), unique_id, NULL, THUNAR_UCA_EDITOR_SHOW_FLAG_NONE))
        {
          XfceItemListView *item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (toolbar_editor));
          GtkWidget        *tree_view = xfce_item_list_view_get_tree_view (item_view);
          GtkTreeIter       iter;
          GtkTreePath      *path = NULL;

          /* update item (remove+insert) */
          item = GTK_WIDGET (g_list_nth_data (toolbar_editor->children, index));
          xfce_item_list_model_remove (XFCE_ITEM_LIST_MODEL (toolbar_editor->store), index);
          thunar_toolbar_order_editor_insert_item (toolbar_editor, index, item);

          /* set cursor */
          xfce_item_list_model_set_index (XFCE_ITEM_LIST_MODEL (toolbar_editor->store), &iter, index);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (toolbar_editor->store), &iter);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);

          /* cleanup */
          gtk_tree_path_free (path);
        }
      else
        {
          stop_event = TRUE;
        }
      toolbar_editor->block_store_refill = FALSE;

      /* unlocking the uca_model */
      thunar_uca_model_unlock (uca_model);
    }
  else
    {
      stop_event = TRUE;
    }

  /* cleanup */
  g_object_unref (uca_model);

  /* continue event propagation */
  return stop_event;
}



static void
thunar_toolbar_order_editor_add_uca (ThunarToolbarOrderEditor *toolbar_editor)
{
  ThunarUcaModel *uca_model = thunar_uca_model_get_default ();

  /* lock uca_model for exclusive write access */
  if (thunar_uca_editor_lock_model (GTK_WINDOW (toolbar_editor), uca_model))
    {
      gchar   *new_unique_id = NULL;
      gboolean added;

      /* Show dialog. Block the store update triggered by the UCA update; instead, we will create a new item and place it
       * in the appropriate position in the store. */
      toolbar_editor->block_store_refill = TRUE;
      added = thunar_uca_editor_show (GTK_WINDOW (toolbar_editor), NULL, &new_unique_id, THUNAR_UCA_EDITOR_SHOW_FLAG_FOR_TOOLBAR_ADD);
      toolbar_editor->block_store_refill = FALSE;

      /* place the cursor on the new item */
      if (added && new_unique_id != NULL)
        {
          gint     index = 0;
          gboolean found = FALSE;

          /* search for a new item in the toolbar */
          for (GList *l = toolbar_editor->children; l != NULL; l = l->next, ++index)
            {
              GtkWidget   *item = GTK_WIDGET (l->data);
              const gchar *item_id = g_object_get_data (G_OBJECT (item), "id");

              if (g_str_has_prefix (item_id, "uca-action-"))
                {
                  const gchar *item_unique_id = item_id + strlen ("uca-action-");

                  if (g_str_equal (item_unique_id, new_unique_id))
                    {
                      found = TRUE;
                      break;
                    }
                }
            }

          if (found)
            {
              XfceItemListView *item_view = thunar_order_editor_get_item_view (THUNAR_ORDER_EDITOR (toolbar_editor));
              GtkWidget        *tree_view = xfce_item_list_view_get_tree_view (item_view);
              gint              new_index = -1;
              gint             *selected = NULL;
              gint              n_selected;
              GtkWidget        *item;
              GtkTreeIter       iter;
              GtkTreePath      *path;

              /* show item */
              thunar_toolbar_order_editor_set_item_visibility (toolbar_editor, index, TRUE);

              /* moving the new item below the cursor */
              n_selected = xfce_item_list_view_get_selected_items (item_view, &selected);
              if (n_selected > 0)
                new_index = selected[n_selected - 1] + 1;

              if (new_index != -1)
                thunar_toolbar_order_editor_move (toolbar_editor, index, new_index);
              else
                /* if nothing is selected, the decision on where to place the item is left to the toolbar */
                new_index = index;

              /* inserting an item from the toolbar into the store */
              item = GTK_WIDGET (g_list_nth_data (toolbar_editor->children, new_index));
              thunar_toolbar_order_editor_insert_item (toolbar_editor, new_index, item);

              /* setting the cursor to the new item */
              xfce_item_list_model_set_index (XFCE_ITEM_LIST_MODEL (toolbar_editor->store), &iter, new_index);
              path = gtk_tree_model_get_path (GTK_TREE_MODEL (toolbar_editor->store), &iter);
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);

              /* cleanup */
              gtk_tree_path_free (path);
              g_free (selected);
            }
        }

      /* unlocking the uca_model */
      thunar_uca_model_unlock (uca_model);

      /* cleanup */
      g_free (new_unique_id);
    }

  /* cleanup */
  g_object_unref (uca_model);
}



static void
thunar_toolbar_order_editor_save (ThunarToolbarOrderEditor *toolbar_editor)
{
  GString *items = g_string_sized_new (1024);

  /* block signal */
  g_signal_handlers_block_by_func (toolbar_editor->preferences, thunar_toolbar_order_editor_refresh, toolbar_editor);

  /* read the internal id and visibility column values and store them */
  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
    {
      gchar   *id;
      gboolean visible;

      /* get the id value of the entry */
      id = g_object_get_data (l->data, "id");
      if (id == NULL)
        continue;

      /* append a comma if not empty */
      if (*items->str != '\0')
        g_string_append_c (items, ',');

      /* store the id value */
      g_string_append (items, id);

      /* append the separator character */
      g_string_append_c (items, ':');

      /* get the visibility value of the entry and store it */
      g_object_get (l->data, "visible", &visible, NULL);
      g_string_append_printf (items, "%i", visible);
    }

  /* save the toolbar configuration */
  g_object_set (toolbar_editor->preferences, "last-toolbar-items", items->str, NULL);

  /* release the string */
  g_string_free (items, TRUE);

  /* unblock signal */
  g_signal_handlers_unblock_by_func (toolbar_editor->preferences, thunar_toolbar_order_editor_refresh, toolbar_editor);
}
