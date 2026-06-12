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

#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"

#include <libxfce4util/libxfce4util.h>



struct _ThunarContextMenuOrderModelClass
{
  GObjectClass __parent__;
};

struct _ThunarContextMenuOrderModel
{
  GObject __parent__;

  ThunarPreferences *preferences;
  GList             *items;
};

enum
{
  CHANGED,
  N_SIGNALS,
};



static gint signals[N_SIGNALS];



static void
thunar_context_menu_order_model_finalize (GObject *object);

static void
thunar_context_menu_order_model_save (ThunarContextMenuOrderModel *order_model);

static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model);

static GList *
thunar_context_menu_order_model_get_custom_actions (void);

static GList *
thunar_context_menu_order_model_get_default_items (void);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new (const gchar *id,
                                          gboolean     hidden);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new_from_enum_value (ThunarContextMenuItem id,
                                                          gboolean              hidden);

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
  thunar_context_menu_order_model_load (order_model);

  g_signal_connect (order_model, "changed", G_CALLBACK (thunar_context_menu_order_model_save), NULL);
}



static void
thunar_context_menu_order_model_finalize (GObject *object)
{
  ThunarContextMenuOrderModel *order_model = THUNAR_CONTEXT_MENU_ORDER_MODEL (object);

  g_object_unref (order_model->preferences);

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

      g_string_append_printf (content, "%s:%d", item->id, (gint) !item->hidden);
    }

  g_object_set (order_model->preferences, "last-context-menu-items", content->str, NULL);
  g_string_free (content, TRUE);
}



static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model)
{
  GList *default_items = thunar_context_menu_order_model_get_default_items ();
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
          gboolean hidden = g_strcmp0 (item_data[1], "0") == 0;

          if (g_strcmp0 ("THUNAR_CONTEXT_MENU_ITEM_SEPARATOR", item_data[0]) == 0)
            {
              ThunarContextMenuOrderModelItem *item = thunar_context_menu_order_model_item_new_from_enum_value (THUNAR_CONTEXT_MENU_ITEM_SEPARATOR, hidden);

              order_model->items = g_list_append (order_model->items, item);
            }
          else
            {
              for (GList *l = default_items; l != NULL; l = l->next)
                {
                  ThunarContextMenuOrderModelItem *item = l->data;

                  if (g_strcmp0 (item->id, item_data[0]) == 0)
                    {
                      item->hidden = hidden;
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
          gchar                           *id = NULL;
          gchar                           *name = NULL;
          gchar                           *icon = NULL;
          gchar                           *tooltip = NULL;
          ThunarContextMenuOrderModelItem *item = NULL;

          g_object_get (provider_menu_item,
                        "name", &id,
                        "label", &name,
                        "icon", &icon,
                        "tooltip", &tooltip,
                        NULL);

          item = thunar_context_menu_order_model_item_new (id, FALSE);
          item->name = g_strdup (name);
          item->icon = g_strdup (icon);
          item->tooltip = g_strdup (tooltip);

          g_free (id);
          g_free (name);
          g_free (icon);
          g_free (tooltip);

          custom_actions = g_list_append (custom_actions, item);
        }
      g_list_free_full (provider_items, g_object_unref);
    }
  g_list_free_full (providers, g_object_unref);

  return custom_actions;
}



static GList *
thunar_context_menu_order_model_get_default_items (void)
{
  GList *default_items = NULL;

#define ITEM(id) default_items = g_list_append (default_items, thunar_context_menu_order_model_item_new_from_enum_value (id, FALSE))
#define SEPARATOR default_items = g_list_append (default_items, thunar_context_menu_order_model_item_new_from_enum_value (THUNAR_CONTEXT_MENU_ITEM_SEPARATOR, FALSE));

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CREATE_FOLDER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_CREATE_DOCUMENT);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_EXECUTE);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_EDIT_LAUNCHER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_TAB);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_WINDOW);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_WITH_OTHER);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_SET_DEFAULT_APP);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_LOCATION);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_SENDTO_MENU);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CUT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_COPY);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE_INTO_FOLDER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE_LINK);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_MOVE_TO_TRASH);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_DELETE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_EMPTY_TRASH);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_DUPLICATE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_MAKE_LINK);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_RENAME);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_RESTORE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_RESTORE_SHOW);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_REMOVE_FROM_RECENT);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CUSTOM_ACTION);
  default_items = g_list_concat (default_items, thunar_context_menu_order_model_get_custom_actions ());

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_ARRANGE_ITEMS);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_CONFIGURE_COLUMNS);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_TOGGLE_EXPANDABLE_FOLDERS);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_MOUNT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_UNMOUNT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_EJECT);

  SEPARATOR;

  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_IN);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_OUT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_RESET);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PROPERTIES);

#undef ITEM
#undef SEPARATOR

  return default_items;
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new (const gchar *id,
                                          gboolean     hidden)
{
  ThunarContextMenuOrderModelItem *item = g_new0 (ThunarContextMenuOrderModelItem, 1);
  item->id = g_strdup (id);
  item->hidden = hidden;
  return item;
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new_from_enum_value (ThunarContextMenuItem id,
                                                          gboolean              hidden)
{
  GEnumClass                      *enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  GEnumValue                      *enum_value = g_enum_get_value (enum_class, id);
  ThunarContextMenuOrderModelItem *item = NULL;

  g_assert (enum_value->value_name != NULL);
  g_assert (enum_value->value_nick != NULL);

  item = thunar_context_menu_order_model_item_new (enum_value->value_name, hidden);
  item->name = g_strdup (enum_value->value_nick);
  item->icon = g_strdup (thunar_context_menu_item_get_icon (id));

  if (id == THUNAR_CONTEXT_MENU_ITEM_SEPARATOR)
    item->removable = TRUE;

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



GList *
thunar_context_menu_order_model_get_visible_items (ThunarContextMenuOrderModel *order_model)
{
  GList *list = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model), NULL);

  for (GList *l = order_model->items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (!item->hidden)
        list = g_list_append (list, l->data);
    }

  return list;
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
thunar_context_menu_order_model_set_hidden (ThunarContextMenuOrderModel *order_model,
                                            gint                         index,
                                            gboolean                     hidden)
{
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model));
  _thunar_return_if_fail (index >= 0 && index < (gint) g_list_length (order_model->items));

  item = g_list_nth_data (order_model->items, index);
  item->hidden = hidden;

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

  order_model->items = thunar_context_menu_order_model_get_default_items ();
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
          g_warning ("attempt to delete an item at a non-existent index");
          continue;
        }
#endif /* !NDEBUG */

      link = g_list_nth (order_model->items, index);
      item = link->data;

      if (!item->removable)
        {
          g_warning ("attempt to remove an unremovable item");
          continue;
        }

      if (thunar_context_menu_order_model_item_is (item, THUNAR_CONTEXT_MENU_ITEM_SEPARATOR))
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

  item = thunar_context_menu_order_model_item_new_from_enum_value (THUNAR_CONTEXT_MENU_ITEM_SEPARATOR, FALSE);

  order_model->items = g_list_insert (order_model->items, item, index);

  g_signal_emit (order_model, signals[CHANGED], 0);

  return index;
}



void
thunar_context_menu_item_set_id (GtkWidget            *menu_item,
                                 ThunarContextMenuItem id)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  _thunar_return_if_fail (menu_item != NULL);
  _thunar_return_if_fail (GTK_IS_WIDGET (menu_item));

  enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  enum_value = g_enum_get_value (enum_class, id);

  if (enum_value != NULL)
    {
      if (enum_value->value_name != NULL)
        g_object_set_data (G_OBJECT (menu_item), "id", (gpointer) enum_value->value_name);
      else
        g_warning ("Missing name for ThunarContextMenuItem constant: %d", id);
    }
  else
    {
      g_warning ("Missing GEnumValue for ThunarContextMenuItem constant: %d", id);
    }

  g_type_class_unref (enum_class);
}



void
thunar_context_menu_item_set_custom_action_id (GtkWidget   *menu_item,
                                               const gchar *custom_action_id)
{
  gchar *id;

  _thunar_return_if_fail (menu_item != NULL);
  _thunar_return_if_fail (custom_action_id != NULL);

  id = g_strdup_printf ("custom-action-%s", custom_action_id);
  g_object_set_data_full (G_OBJECT (menu_item), "id", id, (GDestroyNotify) g_free);
}



gboolean
thunar_context_menu_item_is_custom_action (GtkWidget *menu_item)
{
  const gchar *id;

  _thunar_return_val_if_fail (menu_item != NULL, FALSE);

  id = g_object_get_data (G_OBJECT (menu_item), "id");
  return id != NULL && g_str_has_prefix (id, "custom-action-");
}



gboolean
thunar_context_menu_order_model_item_is (ThunarContextMenuOrderModelItem *item,
                                         ThunarContextMenuItem            id)

{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  gboolean    status;

  _thunar_return_val_if_fail (item != NULL, FALSE);

  enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  enum_value = g_enum_get_value (enum_class, id);

  _thunar_return_val_if_fail (enum_value != NULL, FALSE);

  status = g_strcmp0 (item->id, enum_value->value_name) == 0;
  g_type_class_unref (enum_class);
  return status;
}
