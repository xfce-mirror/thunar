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
thunar_context_menu_order_model_load_property (ThunarContextMenuOrderModel *order_model,
                                               const gchar                 *property_name);

static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model);

static gint
thunar_context_menu_order_model_get_item_index_by_id (ThunarContextMenuOrderModel *order_model,
                                                      ThunarContextMenuItem        id,
                                                      const gchar                 *secondary_id);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_append_item (ThunarContextMenuOrderModel *order_model,
                                             ThunarContextMenuItem        id,
                                             gboolean                     visibility);

static void
thunar_context_menu_order_model_append_custom_actions (ThunarContextMenuOrderModel *order_model);

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
  GEnumClass *enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  GString    *order_content = g_string_new (NULL);
  GString    *visibility_content = g_string_new (NULL);
  gint        order = 0;

  for (GList *l = order_model->items; l != NULL; l = l->next, ++order)
    {
      ThunarContextMenuOrderModelItem *item = l->data;
      GEnumValue                      *enum_value = g_enum_get_value (enum_class, item->id);

      if (item->default_order != order)
        g_string_append_printf (order_content, "%s:%s=%d;",
                                enum_value->value_name,
                                item->secondary_id != NULL ? item->secondary_id : "",
                                order);

      if (item->default_visibility != item->visibility)
        g_string_append_printf (visibility_content, "%s:%s=%d;",
                                enum_value->value_name,
                                item->secondary_id != NULL ? item->secondary_id : "",
                                item->visibility);
    }

  g_object_set (order_model->preferences,
                "context-menu-item-order", order_content->str,
                "context-menu-item-visibility", visibility_content->str,
                NULL);

  g_string_free (order_content, TRUE);
  g_string_free (visibility_content, TRUE);
  g_type_class_unref (enum_class);
}



static void
thunar_context_menu_order_model_load_property (ThunarContextMenuOrderModel *order_model,
                                               const gchar                 *property_name)
{
  GEnumClass *enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  gchar      *content = NULL;
  gchar     **pairs;

  g_signal_handlers_block_by_func (order_model, thunar_context_menu_order_model_save, NULL);

  g_object_get (order_model->preferences, property_name, &content, NULL);
  if (content != NULL)
    {
      pairs = g_strsplit (content, ";", -1);
      for (gint i = 0; pairs[i] != NULL; ++i)
        {
          gchar    name[128];
          gchar    secondary_id[128];
          gint     value;
          gboolean parsed;

          if (xfce_str_is_empty (pairs[i]))
            continue;

          parsed = sscanf (pairs[i], "%127[^:]:%127[^=]=%d", name, secondary_id, &value) == 3;
          if (!parsed)
            {
              secondary_id[0] = 0;
              parsed = sscanf (pairs[i], "%127[^:]:=%d", name, &value) == 2;
            }

          if (parsed)
            {
              GEnumValue *enum_value = g_enum_get_value_by_name (enum_class, name);

              if (enum_value != NULL)
                {
                  gint index = thunar_context_menu_order_model_get_item_index_by_id (order_model, enum_value->value, !xfce_str_is_empty (secondary_id) ? secondary_id : NULL);

                  if (index != -1)
                    {
                      if (g_strcmp0 (property_name, "context-menu-item-order") == 0)
                        thunar_context_menu_order_model_move (order_model, index, value);
                      else if (g_strcmp0 (property_name, "context-menu-item-visibility") == 0)
                        thunar_context_menu_order_model_set_visibility (order_model, index, value);
                      else
                        _thunar_assert_not_reached ();
                    }
                  else
                    {
                      g_warning ("Unknown context menu item: %s:%s", name, secondary_id);
                    }
                }
              else
                {
                  g_warning ("Unknown context menu item: %s", name);
                }
            }
          else
            {
              g_warning ("Option parsing error: %s", property_name);
            }
        }
      g_free (content);
      g_strfreev (pairs);
    }

  g_signal_handlers_unblock_by_func (order_model, thunar_context_menu_order_model_save, NULL);

  g_type_class_unref (enum_class);
}



static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model)
{
  thunar_context_menu_order_model_reset (order_model);
  thunar_context_menu_order_model_load_property (order_model, "context-menu-item-order");
  thunar_context_menu_order_model_load_property (order_model, "context-menu-item-visibility");
}



static gint
thunar_context_menu_order_model_get_item_index_by_id (ThunarContextMenuOrderModel *order_model,
                                                      ThunarContextMenuItem        id,
                                                      const gchar                 *secondary_id)
{
  gint index = 0;

  for (GList *l = order_model->items; l != NULL; l = l->next, ++index)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (item->id == id && g_strcmp0 (item->secondary_id, secondary_id) == 0)
        return index;
    }

  g_return_val_if_reached (-1);
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_append_item (ThunarContextMenuOrderModel *order_model,
                                             ThunarContextMenuItem        id,
                                             gboolean                     visibility)
{
  ThunarContextMenuOrderModelItem *item = g_new0 (ThunarContextMenuOrderModelItem, 1);
  gint                             current_order = g_list_length (order_model->items);
  GEnumClass                      *enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  GEnumValue                      *enum_value = g_enum_get_value (enum_class, id);

  g_assert (enum_value->value_nick != NULL);

  item->id = id;
  item->name = g_strdup (enum_value->value_nick);
  item->visibility = visibility;
  item->default_visibility = visibility;
  item->default_order = current_order;

  order_model->items = g_list_append (order_model->items, item);

  g_type_class_unref (enum_class);

  return item;
}



static void
thunar_context_menu_order_model_append_custom_actions (ThunarContextMenuOrderModel *order_model)
{
  ThunarxProviderFactory *provider_factory = thunarx_provider_factory_get_default ();
  GList                  *providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_MENU_PROVIDER);

  g_object_unref (provider_factory);
  for (GList *provider = providers; provider != NULL; provider = provider->next)
    {
      GList *provider_items = thunarx_menu_provider_get_all_menu_items (provider->data);
      for (GList *provider_item = provider_items; provider_item != NULL; provider_item = provider_item->next)
        {
          ThunarxMenuItem                 *provider_menu_item = provider_item->data;
          ThunarContextMenuOrderModelItem *item = thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_CUSTOM_ACTION, TRUE);

          g_clear_pointer (&item->name, g_free);
          g_object_get (provider_menu_item,
                        "name", &item->secondary_id,
                        "label", &item->name,
                        "icon", &item->icon,
                        "tooltip", &item->tooltip,
                        NULL);
        }
      g_list_free_full (provider_items, g_object_unref);
    }
  g_list_free_full (providers, g_object_unref);
}



static void
thunar_context_menu_order_model_item_free (ThunarContextMenuOrderModelItem *item)
{
  if (item == NULL)
    return;

  g_free (item->secondary_id);
  g_free (item->icon);
  g_free (item->name);
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

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_CREATE_FOLDER, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_CREATE_DOCUMENT, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_EXECUTE, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_EDIT_LAUNCHER, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_OPEN, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_TAB, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_WINDOW, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_OPEN_WITH_OTHER, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_SET_DEFAULT_APP, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_OPEN_LOCATION, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_SENDTO_MENU, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_CUT, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_COPY, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_PASTE_INTO_FOLDER, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_PASTE, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_PASTE_LINK, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_MOVE_TO_TRASH, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_DELETE, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_EMPTY_TRASH, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_DUPLICATE, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_MAKE_LINK, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_RENAME, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_RESTORE, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_RESTORE_SHOW, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_REMOVE_FROM_RECENT, TRUE);

  thunar_context_menu_order_model_append_custom_actions (order_model);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_ARRANGE_ITEMS, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_CONFIGURE_COLUMNS, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_TOGGLE_EXPANDABLE_FOLDERS, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_MOUNT, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_UNMOUNT, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_EJECT, TRUE);

  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_ZOOM_IN, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_ZOOM_OUT, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_ZOOM_RESET, TRUE);
  thunar_context_menu_order_model_append_item (order_model, THUNAR_CONTEXT_MENU_ITEM_PROPERTIES, TRUE);

  g_signal_emit (order_model, signals[CHANGED], 0);
}



void
thunar_context_menu_item_set_id (GtkWidget            *menu_item,
                                 ThunarContextMenuItem menu_item_id)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  _thunar_return_if_fail (menu_item != NULL);
  _thunar_return_if_fail (GTK_IS_WIDGET (menu_item));

  enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  enum_value = g_enum_get_value (enum_class, menu_item_id);

  if (enum_value != NULL)
    {
      if (enum_value->value_name != NULL)
        g_object_set_data (G_OBJECT (menu_item), "id", (gpointer) enum_value->value_name);
      else
        g_warning ("Missing name for ThunarContextMenuItemId constant: %d", menu_item_id);
    }
  else
    {
      g_warning ("Missing GEnumValue for ThunarContextMenuItemId constant: %d", menu_item_id);
    }

  g_type_class_unref (enum_class);
}



ThunarContextMenuItem
thunar_context_menu_item_get_id (GtkWidget *menu_item)
{
  const gchar          *id_name;
  GEnumClass           *enum_class;
  GEnumValue           *enum_value;
  ThunarContextMenuItem item_id = -1;

  _thunar_return_val_if_fail (menu_item != NULL, -1);
  _thunar_return_val_if_fail (GTK_IS_WIDGET (menu_item), -1);

  id_name = g_object_get_data (G_OBJECT (menu_item), "id");
  if (id_name != NULL)
    {
      enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
      enum_value = g_enum_get_value_by_name (enum_class, id_name);

      if (enum_value != NULL)
        item_id = enum_value->value;
      else
        g_warning ("Missing constant for name: %s", id_name);

      g_type_class_unref (enum_class);
    }
  else
    {
      g_warning ("menu_item has no id");
    }

  return item_id;
}
