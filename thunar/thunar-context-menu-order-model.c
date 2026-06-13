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

#include "thunar/thunar-context-menu-order-model.h"

#include "thunar/thunar-action-manager.h"
#include "thunar/thunar-details-view.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-standard-view.h"
#include "thunar/thunar-util.h"
#include "thunar/thunar-window.h"

#include <libxfce4util/libxfce4util.h>



struct _ThunarContextMenuOrderModelClass
{
  GObjectClass __parent__;
};

struct _ThunarContextMenuOrderModel
{
  GObject __parent__;

  ThunarPreferences *preferences;

  /* list of ThunarContextMenuOrderModelItem* in the order set by the user */
  GList *items;

  /* table(gchar* => ThunarContextMenuOrderModelItem*) for storing menu item prototypes, from which items are then
   * created in the thunar_context_menu_order_model_new_item_from_prototype() */
  GHashTable *prototypes_table;
};

enum
{
  CHANGED,
  N_SIGNALS,
};



static gint signals[N_SIGNALS];


static void
thunar_context_menu_order_model_init_prototypes_table (ThunarContextMenuOrderModel *order_model);

static void
thunar_context_menu_order_model_finalize (GObject *object);

static void
thunar_context_menu_order_model_save (ThunarContextMenuOrderModel *order_model);

static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model);

static GList *
thunar_context_menu_order_model_get_custom_actions (void);

static GList *
thunar_context_menu_order_model_get_default_items (ThunarContextMenuOrderModel *order_model);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_new_item_from_prototype (ThunarContextMenuOrderModel *order_model,
                                                         const gchar                 *id);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new (const gchar *id,
                                          gboolean     visibility);

static void
thunar_context_menu_order_model_item_free (ThunarContextMenuOrderModelItem *item);



G_DEFINE_TYPE (ThunarContextMenuOrderModel, thunar_context_menu_order_model, G_TYPE_OBJECT)



static void
thunar_context_menu_order_model_class_init (ThunarContextMenuOrderModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_context_menu_order_model_finalize;

  signals[CHANGED] = g_signal_new ("changed",
                                   G_TYPE_FROM_CLASS (object_class),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   NULL,
                                   G_TYPE_NONE, 0);
}



static void
thunar_context_menu_order_model_init (ThunarContextMenuOrderModel *order_model)
{
  order_model->preferences = thunar_preferences_get ();

  thunar_context_menu_order_model_init_prototypes_table (order_model);

  thunar_context_menu_order_model_load (order_model);

  g_signal_connect (order_model, "changed", G_CALLBACK (thunar_context_menu_order_model_save), NULL);
}



static void
thunar_context_menu_order_model_init_prototypes_table (ThunarContextMenuOrderModel *order_model)
{
  GList                           *items = NULL;
  ThunarContextMenuOrderModelItem *item;

  /* separator */
  item = thunar_context_menu_order_model_item_new ("separator", TRUE);
  item->name = g_strdup (_("--- Separator ---"));
  item->removable = TRUE;
  items = g_list_append (items, item);

  /* custom actions */
  item = thunar_context_menu_order_model_item_new ("custom-actions", TRUE);
  item->name = g_strdup (_("*** Custom actions ***"));
  item->tooltip = g_strdup (_("Custom action menu items will appear in place of this item"));
  items = g_list_append (items, item);

  /* Thunar menu items */
  items = g_list_concat (items, thunar_action_manager_get_right_click_context_menu_items ());
  items = g_list_concat (items, thunar_standard_view_get_right_click_context_menu_items ());
  items = g_list_concat (items, thunar_details_view_get_right_click_context_menu_items ());
  items = g_list_concat (items, thunar_window_get_right_click_context_menu_items ());

  /* filling the table */
  order_model->prototypes_table = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         g_free,
                                                         (GDestroyNotify) thunar_context_menu_order_model_item_free);
  for (GList *l = items; l != NULL; l = l->next)
    {
      item = l->data;

#ifndef NDEBUG
      if (g_hash_table_contains (order_model->prototypes_table, item->id))
        g_warning ("Rewriting id=\"%s\" in the menu item prototype table", item->id);
#endif /* !NDEBUG */

      g_hash_table_insert (order_model->prototypes_table, g_strdup (item->id), item);
    }
  g_list_free (items);
}



static void
thunar_context_menu_order_model_finalize (GObject *object)
{
  ThunarContextMenuOrderModel *order_model = THUNAR_CONTEXT_MENU_ORDER_MODEL (object);

  g_object_unref (order_model->preferences);

  g_hash_table_unref (order_model->prototypes_table);

  if (order_model->items != NULL)
    g_list_free_full (order_model->items, (GDestroyNotify) thunar_context_menu_order_model_item_free);

  G_OBJECT_CLASS (thunar_context_menu_order_model_parent_class)->finalize (object);
}



static void
thunar_context_menu_order_model_save (ThunarContextMenuOrderModel *order_model)
{
  GString *content = g_string_new (NULL);

  for (GList *l = order_model->items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (l != order_model->items)
        g_string_append_c (content, ',');

      g_string_append_printf (content, "%s:%d", item->id, (gint) item->visibility);
    }

  g_object_set (order_model->preferences, "last-context-menu-items", content->str, NULL);
  g_string_free (content, TRUE);
}



static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model)
{
  GList *default_items = thunar_context_menu_order_model_get_default_items (order_model);
  gchar *content = NULL;

  /* clearing previous data */
  if (order_model->items != NULL)
    {
      g_list_free_full (order_model->items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
      order_model->items = NULL;
    }

  g_object_get (order_model->preferences, "last-context-menu-items", &content, NULL);
  if (content != NULL)
    {
      gchar **items = g_strsplit (content, ",", -1);

      for (gint i = 0; items[i] != NULL; ++i)
        {
          gchar  **item_data = g_strsplit (items[i], ":", -1);
          gboolean visibility = g_strcmp0 (item_data[1], "0") != 0;

          if (g_strcmp0 ("separator", item_data[0]) == 0)
            {
              /* if a separator is encountered, then simply create it */
              ThunarContextMenuOrderModelItem *item = thunar_context_menu_order_model_new_item_from_prototype (order_model, "separator");

              item->visibility = visibility;
              order_model->items = g_list_append (order_model->items, item);
            }
          else
            {
              /* if a menu item is encountered, it must be removed from default_items and placed in the specified position */
              for (GList *l = default_items; l != NULL; l = l->next)
                {
                  ThunarContextMenuOrderModelItem *item = l->data;

                  if (g_strcmp0 (item->id, item_data[0]) == 0)
                    {
                      item->visibility = visibility;
                      order_model->items = g_list_append (order_model->items, l->data);
                      default_items = g_list_remove (default_items, l->data);
                      break;
                    }
                }
            }

          g_strfreev (item_data);
        }

      g_strfreev (items);
      g_free (content);
    }
  else
    {
      /* if the configuration value is not specified, the standard order of items is used */
      order_model->items = default_items;
      default_items = NULL;
    }

  if (default_items != NULL)
    {
      GList *new_custom_actions = NULL;
      gint   index = 0;

      /* search for menu items of custom actions that were not specified in the config but have appeared now */
      for (GList *l = default_items, *lnext; l != NULL; l = lnext)
        {
          ThunarContextMenuOrderModelItem *item = l->data;

          lnext = l->next;
          if (g_str_has_prefix (item->id, "custom-action-"))
            {
              new_custom_actions = g_list_append (new_custom_actions, item);
              default_items = g_list_remove (default_items, item);
            }
        }

      if (new_custom_actions != NULL)
        {
          /*if there are new custom actions, then put them at the end of the list of current custom actions */
          for (GList *li = order_model->items; li != NULL; li = li->next, ++index)
            {
              ThunarContextMenuOrderModelItem *item = li->data;

              if (g_strcmp0 (item->id, "custom-actions") == 0)
                {
                  /* new custom actions are added to the end after those that were already set in the config
                   *
                   * Visualization of the context menu:
                   * - New File
                   * - New Directory
                   * - Custom action <- now the variable "item" refers to this item
                   * - UCA action1
                   * - UCA action2 <- the new_custom_actions list is inserted after this item
                   * - Properties...
                   */

                  for (li = li->next; li != NULL; li = li->next, ++index)
                    {
                      item = li->data;

                      if (!g_str_has_prefix (item->id, "custom-action-"))
                        break;
                    }
                  while (new_custom_actions != NULL)
                    {
                      item = new_custom_actions->data;
                      order_model->items = g_list_insert (order_model->items, item, ++index);
                      new_custom_actions = g_list_remove (new_custom_actions, item);
                    }
                  break;
                }
            }
        }

      g_list_free_full (new_custom_actions, (GDestroyNotify) thunar_context_menu_order_model_item_free);
    }

  g_list_free_full (default_items, (GDestroyNotify) thunar_context_menu_order_model_item_free);

  /* signal */
  g_signal_handlers_block_by_func (order_model, thunar_context_menu_order_model_save, NULL);
  g_signal_emit (order_model, signals[CHANGED], 0);
  g_signal_handlers_unblock_by_func (order_model, thunar_context_menu_order_model_save, NULL);
}



static GList *
thunar_context_menu_order_model_get_custom_actions (void)
{
  ThunarxProviderFactory *provider_factory = thunarx_provider_factory_get_default ();
  GList                  *providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  GList                  *custom_actions = NULL;

  g_object_unref (provider_factory);
  for (GList *provider = providers; provider != NULL; provider = provider->next)
    {
      GList *provider_items = thunarx_menu_provider_get_all_right_click_menu_items (provider->data);
      for (GList *provider_item = provider_items; provider_item != NULL; provider_item = provider_item->next)
        {
          ThunarxMenuItem                 *provider_menu_item = provider_item->data;
          gchar                           *custom_action_id = NULL;
          gchar                           *name = NULL;
          gchar                           *icon = NULL;
          gchar                           *tooltip = NULL;
          gchar                           *id = NULL;
          ThunarContextMenuOrderModelItem *item = NULL;

          g_object_get (provider_menu_item,
                        "name", &custom_action_id,
                        "label", &name,
                        "icon", &icon,
                        "tooltip", &tooltip,
                        NULL);

          if (custom_action_id != NULL)
            {
              id = g_strdup_printf ("custom-action-%s", custom_action_id);
              item = thunar_context_menu_order_model_item_new (id, TRUE);
              item->name = g_strdup (name);
              item->icon = g_strdup (icon);
              item->tooltip = g_strdup (tooltip);
            }

          g_free (custom_action_id);
          g_free (name);
          g_free (icon);
          g_free (tooltip);
          g_free (id);

          custom_actions = g_list_append (custom_actions, item);
        }
      g_list_free_full (provider_items, g_object_unref);
    }
  g_list_free_full (providers, g_object_unref);

  return custom_actions;
}



static GList *
thunar_context_menu_order_model_get_default_items (ThunarContextMenuOrderModel *order_model)
{
  GList *default_items = NULL;

#define ITEM(id) default_items = g_list_append (default_items, thunar_context_menu_order_model_new_item_from_prototype (order_model, id))
#define SEPARATOR ITEM ("separator");

  ITEM ("create-folder");
  ITEM ("create-document");

  SEPARATOR;

  ITEM ("execute");

  SEPARATOR;

  ITEM ("edit-launcher");
  ITEM ("open");
  ITEM ("open-in-new-tab");
  ITEM ("open-in-new-window");

  SEPARATOR;

  ITEM ("open-with-other");

  SEPARATOR;

  ITEM ("set-default-app");

  SEPARATOR;

  ITEM ("open-location");

  SEPARATOR;

  ITEM ("sendto-menu");

  SEPARATOR;

  ITEM ("cut");
  ITEM ("copy");
  ITEM ("paste-into-folder");
  ITEM ("paste");
  ITEM ("paste-link");

  SEPARATOR;

  ITEM ("move-to-trash");
  ITEM ("delete");
  ITEM ("empty-trash");

  SEPARATOR;

  ITEM ("duplicate");
  ITEM ("make-link");
  ITEM ("rename");

  SEPARATOR;

  ITEM ("restore");
  ITEM ("restore-show");
  ITEM ("remove-from-recent");

  SEPARATOR;

  ITEM ("custom-actions");
  default_items = g_list_concat (default_items, thunar_context_menu_order_model_get_custom_actions ());

  SEPARATOR;

  ITEM ("arrange-items-menu");
  ITEM ("configure-columns");
  ITEM ("expandable-folders");

  SEPARATOR;

  ITEM ("mount");
  ITEM ("unmount");
  ITEM ("eject");

  SEPARATOR;

  ITEM ("zoom-in");
  ITEM ("zoom-out");
  ITEM ("zoom-reset");

  SEPARATOR;

  ITEM ("properties");

#undef ITEM
#undef SEPARATOR

  /* null in the list can only appear because of a bug, but a broken menu is better than a segfault */
  default_items = g_list_remove_all (default_items, NULL);

  return default_items;
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_new_item_from_prototype (ThunarContextMenuOrderModel *order_model,
                                                         const gchar                 *id)
{
  ThunarContextMenuOrderModelItem *prototype, *item;

  _thunar_return_val_if_fail (id != NULL, NULL);

  prototype = g_hash_table_lookup (order_model->prototypes_table, id);
  if (prototype == NULL)
    {
      g_warning ("Prototype of menu item with id=%s not found", id);
      return NULL;
    }

  item = thunar_context_menu_order_model_item_new (prototype->id, prototype->visibility);
  item->icon = g_strdup (prototype->icon);
  item->name = g_strdup (prototype->name);
  item->tooltip = g_strdup (prototype->tooltip);
  item->removable = prototype->removable;
  return item;
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new (const gchar *id,
                                          gboolean     visibility)
{
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_val_if_fail (id != NULL, NULL);

  item = g_new0 (ThunarContextMenuOrderModelItem, 1);
  item->id = g_strdup (id);
  item->visibility = visibility;

  return item;
}



static void
thunar_context_menu_order_model_item_free (ThunarContextMenuOrderModelItem *item)
{
  if (item == NULL)
    return;

  g_free (item->id);
  g_free (item->name);
  g_free (item->icon);
  g_free (item->tooltip);
  g_free (item);
}



ThunarContextMenuOrderModel *
thunar_context_menu_order_model_get_default (void)
{
  static ThunarContextMenuOrderModel *order_model = NULL;

  if (G_UNLIKELY (order_model == NULL))
    {
      order_model = g_object_new (THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (order_model), (gpointer) &order_model);
    }
  else
    {
      g_object_ref (G_OBJECT (order_model));
    }

  return order_model;
}



GList *
thunar_context_menu_order_model_get_items (ThunarContextMenuOrderModel *order_model)
{
  _thunar_return_val_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model), NULL);

  return g_list_copy (order_model->items);
}



void
thunar_context_menu_order_model_move (ThunarContextMenuOrderModel *order_model,
                                      gint                         source_index,
                                      gint                         dest_index)
{
  GList *link, *dest_sibling;

  _thunar_return_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model));
  _thunar_return_if_fail (source_index >= 0 && source_index < (gint) g_list_length (order_model->items));
  _thunar_return_if_fail (dest_index >= 0 && dest_index < (gint) g_list_length (order_model->items));

  if (source_index == dest_index)
    return;

  link = g_list_nth (order_model->items, source_index);
  order_model->items = g_list_remove_link (order_model->items, link);
  dest_sibling = g_list_nth (order_model->items, dest_index);
  order_model->items = g_list_insert_before_link (order_model->items, dest_sibling, link);

  g_signal_emit (order_model, signals[CHANGED], 0);
}



void
thunar_context_menu_order_model_set_visibility (ThunarContextMenuOrderModel *order_model,
                                                gint                         index,
                                                gboolean                     visibility)
{
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model));
  _thunar_return_if_fail (index >= 0 && index < (gint) g_list_length (order_model->items));

  item = g_list_nth_data (order_model->items, index);
  item->visibility = visibility;

  g_signal_emit (order_model, signals[CHANGED], 0);
}



void
thunar_context_menu_order_model_reset (ThunarContextMenuOrderModel *order_model)
{
  if (order_model->items != NULL)
    {
      g_list_free_full (order_model->items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
      order_model->items = NULL;
    }

  order_model->items = thunar_context_menu_order_model_get_default_items (order_model);
  g_signal_emit (order_model, signals[CHANGED], 0);
}



void
thunar_context_menu_order_model_remove (ThunarContextMenuOrderModel *order_model,
                                        const gint                  *indexes,
                                        gint                         n_indexes)
{
  _thunar_return_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model));
  _thunar_return_if_fail (n_indexes >= 0);
  _thunar_return_if_fail (n_indexes == 0 || (n_indexes > 0 && indexes != NULL));

  if (n_indexes == 0)
    return;

  for (gint i = n_indexes - 1; i >= 0; --i)
    {
      gint                             index = indexes[i];
      GList                           *link;
      ThunarContextMenuOrderModelItem *item;

#ifndef NDEBUG
      if (index < 0 || index >= (gint) g_list_length (order_model->items))
        {
          g_warning ("Attempt to delete an item at a non-existent index");
          continue;
        }
#endif /* !NDEBUG */

      link = g_list_nth (order_model->items, index);
      item = link->data;

      if (!item->removable)
        {
          g_warning ("Attempt to remove an unremovable item");
          continue;
        }

      if (g_strcmp0 (item->id, "separator") == 0)
        {
          thunar_context_menu_order_model_item_free (item);
          order_model->items = g_list_delete_link (order_model->items, link);
        }
    }

  g_signal_emit (order_model, signals[CHANGED], 0);
}



gint
thunar_context_menu_order_model_insert_separator (ThunarContextMenuOrderModel *order_model,
                                                  gint                         index)
{
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_val_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model), -1);

  if (index < 0 || index >= (gint) g_list_length (order_model->items))
    index = g_list_length (order_model->items);

  item = thunar_context_menu_order_model_new_item_from_prototype (order_model, "separator");

  order_model->items = g_list_insert (order_model->items, item, index);

  g_signal_emit (order_model, signals[CHANGED], 0);

  return index;
}



ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new_from_entry (const XfceGtkActionEntry *entry)
{
  ThunarContextMenuOrderModelItem *item;
  GString                         *name;

  _thunar_return_val_if_fail (entry != NULL, NULL);

  if (entry->accel_path == NULL)
    {
      g_warning ("XfceGtkActionEntry with id=%u does not have an accel_path", entry->id);
      return NULL;
    }

  item = g_new0 (ThunarContextMenuOrderModelItem, 1);
  item->id = thunar_util_accel_path_to_id (entry->accel_path);
  item->icon = g_strdup (entry->menu_item_icon_name);

  name = g_string_new (entry->menu_item_label_text);
  g_string_replace (name, "_", "", 1);
  item->name = g_strdup (name->str);
  g_string_free (name, TRUE);

  item->tooltip = g_strdup (entry->menu_item_tooltip_text);
  item->visibility = TRUE;
  item->removable = FALSE;

  return item;
}



GList *
thunar_context_menu_order_model_item_new_list_from_entries (const XfceGtkActionEntry *entries,
                                                            guint                     n_entries,
                                                            const guint              *ids_of_entries,
                                                            guint                     n_ids_of_entries)
{
  GList *items = NULL;

  for (guint i = 0; i < n_ids_of_entries; ++i)
    {
      const XfceGtkActionEntry        *entry = xfce_gtk_get_action_entry_by_id (entries, n_entries, ids_of_entries[i]);
      ThunarContextMenuOrderModelItem *item = thunar_context_menu_order_model_item_new_from_entry (entry);

      if (item != NULL)
        items = g_list_append (items, item);
    }

  return items;
}



void
thunar_context_menu_item_set_id_by_entry (GtkWidget                *menu_item,
                                          const XfceGtkActionEntry *entry)
{
  _thunar_return_if_fail (menu_item != NULL);
  _thunar_return_if_fail (entry != NULL);

  if (entry->accel_path != NULL)
    {
      gchar *id = thunar_util_accel_path_to_id (entry->accel_path);

      if (id != NULL)
        g_object_set_data_full (G_OBJECT (menu_item), "id", g_strdup (id), g_free);

      g_free (id);
    }
}
