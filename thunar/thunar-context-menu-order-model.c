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

  /* list of remote separators that were created by default by Thunar. It is necessary to remember
   * such deleted separators, otherwise it is impossible to determine which separators were
   * deleted by the user and which appeared in the new version of Thunar */
  GList *deleted_items;

  /* counter for assigning each user separator a unique ID */
  gint n_user_separators;
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
thunar_context_menu_order_model_item_new (ThunarContextMenuItem id,
                                          const gchar          *secondary_id,
                                          gboolean              visibility);

static void
thunar_context_menu_order_model_item_free (ThunarContextMenuOrderModelItem *item);

static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_copy (const ThunarContextMenuOrderModelItem *source);



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

  if (order_model->deleted_items != NULL)
    g_list_free_full (order_model->deleted_items, (GDestroyNotify) thunar_context_menu_order_model_item_free);

  if (order_model->items != NULL)
    g_list_free_full (order_model->items, (GDestroyNotify) thunar_context_menu_order_model_item_free);

  G_OBJECT_CLASS (thunar_context_menu_order_model_parent_class)->finalize (object);
}



static void
thunar_context_menu_order_model_save (ThunarContextMenuOrderModel *order_model)
{
  GString *order_content = g_string_new (NULL);
  GString *visibility_content = g_string_new (NULL);
  GString *deleted_content = g_string_new (NULL);

  /* context-menu-item-order & context-menu-item-visibility */
  for (GList *l = order_model->items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      g_string_append_printf (order_content, "%s;", item->config_id);
      g_string_append_printf (visibility_content, "%s=%d;", item->config_id, item->visibility);
    }

  /* context-menu-item-deleted */
  for (GList *l = order_model->deleted_items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      g_string_append_printf (deleted_content, "%s;", item->config_id);
    }

  g_object_set (order_model->preferences,
                "context-menu-item-order", order_content->str,
                "context-menu-item-visibility", visibility_content->str,
                "context-menu-item-deleted", deleted_content->str,
                NULL);

  g_string_free (order_content, TRUE);
  g_string_free (visibility_content, TRUE);
  g_string_free (deleted_content, TRUE);
}



static gboolean
thunar_context_menu_order_model_item_deleted (ThunarContextMenuOrderModel *order_model,
                                              const gchar                 *config_id)
{
  for (GList *l = order_model->deleted_items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (g_strcmp0 (item->config_id, config_id) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}



static void
thunar_context_menu_order_model_load (ThunarContextMenuOrderModel *order_model)
{
  GList *default_items = thunar_context_menu_order_model_get_default_items ();
  gchar *content = NULL;
  gint   index;

  /* clearing previous data */
  if (order_model->items != NULL)
    {
      g_list_free_full (order_model->items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
      order_model->items = NULL;
    }

  if (order_model->deleted_items != NULL)
    {
      g_list_free_full (order_model->deleted_items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
      order_model->deleted_items = NULL;
    }

  /* loading a list of deleted default elements */
  g_object_get (order_model->preferences, "context-menu-item-deleted", &content, NULL);
  if (content != NULL)
    {
      gchar **names = g_strsplit (content, ";", -1);

      for (gint i = 0; names[i] != NULL; ++i)
        {
          for (GList *l = default_items; l != NULL; l = l->next)
            {
              ThunarContextMenuOrderModelItem *item = l->data;

              if (g_strcmp0 (item->config_id, names[i]) == 0)
                {
                  order_model->deleted_items = g_list_append (order_model->deleted_items,
                                                              thunar_context_menu_order_model_item_copy (item));
                  break;
                }
            }
        }

      g_free (content);
      g_strfreev (names);
    }

  /* loading a list of custom item order */
  g_object_get (order_model->preferences, "context-menu-item-order", &content, NULL);
  if (content != NULL)
    {
      gchar **names = g_strsplit (content, ";", -1);

      for (gint i = 0; names[i] != NULL; ++i)
        {
          if (g_str_has_prefix (names[i], "THUNAR_CONTEXT_MENU_ITEM_SEPARATOR\\user-"))
            {
              thunar_context_menu_order_model_insert_separator (order_model, -1);
              continue;
            }

          if (!thunar_context_menu_order_model_item_deleted (order_model, names[i]))
            {
              for (GList *l = default_items; l != NULL; l = l->next)
                {
                  ThunarContextMenuOrderModelItem *item = l->data;

                  if (g_strcmp0 (item->config_id, names[i]) == 0)
                    {
                      order_model->items = g_list_append (order_model->items,
                                                          thunar_context_menu_order_model_item_copy (item));
                      break;
                    }
                }
            }
        }

      g_free (content);
      g_strfreev (names);
    }

  /* inserting default elements that appeared in the new version of Thunar. The insertion is
   * carried out using the same indexes that the elements had in the default order. */
  index = 0;
  for (GList *li = default_items; li != NULL; li = li->next, ++index)
    {
      ThunarContextMenuOrderModelItem *li_item = li->data;
      gboolean                         inserted = FALSE;

      for (GList *lj = order_model->items; lj != NULL; lj = lj->next)
        {
          ThunarContextMenuOrderModelItem *lj_item = lj->data;

          if (g_strcmp0 (li_item->config_id, lj_item->config_id) == 0)
            {
              inserted = TRUE;
              break;
            }
        }

      if (!inserted && !thunar_context_menu_order_model_item_deleted (order_model, li_item->config_id))
        {
          order_model->items = g_list_insert (order_model->items,
                                              thunar_context_menu_order_model_item_copy (li_item),
                                              index);
        }
    }

  /* loading custom element visibility */
  g_object_get (order_model->preferences, "context-menu-item-visibility", &content, NULL);
  if (content != NULL)
    {
      gchar **names = g_strsplit (content, ";", -1);

      for (gint i = 0; names[i] != NULL; ++i)
        {
          gchar name[256];
          gint  is_visible;

          if (sscanf (names[i], "%255[^=]=%d", name, &is_visible) == 2)
            {
              for (GList *l = order_model->items; l != NULL; l = l->next)
                {
                  ThunarContextMenuOrderModelItem *item = l->data;

                  if (g_strcmp0 (item->config_id, name) == 0)
                    {
                      item->visibility = is_visible;
                      break;
                    }
                }
            }
          else
            {
              g_warn_if_reached ();
            }
        }

      g_free (content);
      g_strfreev (names);
    }

  /* signal */
  g_signal_handlers_block_by_func (order_model, thunar_context_menu_order_model_save, NULL);
  g_signal_emit (order_model, signals[CHANGED], 0);
  g_signal_handlers_unblock_by_func (order_model, thunar_context_menu_order_model_save, NULL);

  /* cleanup */
  g_list_free_full (default_items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
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
          gchar                           *secondary_id = NULL;
          gchar                           *name = NULL;
          gchar                           *icon = NULL;
          gchar                           *tooltip = NULL;
          ThunarContextMenuOrderModelItem *item = NULL;

          g_object_get (provider_menu_item,
                        "name", &secondary_id,
                        "label", &name,
                        "icon", &icon,
                        "tooltip", &tooltip,
                        NULL);

          item = thunar_context_menu_order_model_item_new (THUNAR_CONTEXT_MENU_ITEM_CUSTOM_ACTION, secondary_id, TRUE);
          g_free (secondary_id);
          g_free (item->name);
          item->name = name;
          item->icon = icon;
          item->tooltip = tooltip;

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

#define ITEM(id) default_items = g_list_append (default_items, thunar_context_menu_order_model_item_new (id, NULL, TRUE))
#define SEPARATOR(id) default_items = g_list_append (default_items, thunar_context_menu_order_model_item_new (THUNAR_CONTEXT_MENU_ITEM_SEPARATOR, "thunar-" #id, TRUE));

  /* you can add a new element anywhere, but do not change the IDs of the previous separators; if you
   * want to add a new separator, select an ID that is greater than all the others. */

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CREATE_FOLDER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_CREATE_DOCUMENT);

  SEPARATOR (1);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_EXECUTE);

  SEPARATOR (2);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_EDIT_LAUNCHER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_TAB);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_IN_WINDOW);

  SEPARATOR (3);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_WITH_OTHER);

  SEPARATOR (4);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_SET_DEFAULT_APP);

  SEPARATOR (5);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_OPEN_LOCATION);

  SEPARATOR (6);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_SENDTO_MENU);

  SEPARATOR (7);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CUT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_COPY);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE_INTO_FOLDER);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PASTE_LINK);

  SEPARATOR (8);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_MOVE_TO_TRASH);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_DELETE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_EMPTY_TRASH);

  SEPARATOR (9);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_DUPLICATE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_MAKE_LINK);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_RENAME);

  SEPARATOR (10);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_RESTORE);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_RESTORE_SHOW);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_REMOVE_FROM_RECENT);

  SEPARATOR (11);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_CUSTOM_ACTION);
  default_items = g_list_concat (default_items, thunar_context_menu_order_model_get_custom_actions ());

  SEPARATOR (12);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_ARRANGE_ITEMS);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_CONFIGURE_COLUMNS);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_TOGGLE_EXPANDABLE_FOLDERS);

  SEPARATOR (13);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_MOUNT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_UNMOUNT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_EJECT);

  SEPARATOR (14);

  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_IN);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_OUT);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_ZOOM_RESET);
  ITEM (THUNAR_CONTEXT_MENU_ITEM_PROPERTIES);

#undef ITEM
#undef SEPARATOR

  return default_items;
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new (ThunarContextMenuItem id,
                                          const gchar          *secondary_id,
                                          gboolean              visibility)
{
  ThunarContextMenuOrderModelItem *item = g_new0 (ThunarContextMenuOrderModelItem, 1);
  GEnumClass                      *enum_class = g_type_class_ref (THUNAR_TYPE_CONTEXT_MENU_ITEM);
  GEnumValue                      *enum_value = g_enum_get_value (enum_class, id);

  g_assert (enum_value->value_nick != NULL);

  item->id = id;
  item->secondary_id = g_strdup (secondary_id);
  item->config_id = g_strdup_printf ("%s\\%s", enum_value->value_name, secondary_id != NULL ? secondary_id : "");
  item->name = g_strdup (enum_value->value_nick);
  item->visibility = visibility;

  if (id == THUNAR_CONTEXT_MENU_ITEM_SEPARATOR)
    item->removable = TRUE;

  g_type_class_unref (enum_class);

  return item;
}



static void
thunar_context_menu_order_model_item_free (ThunarContextMenuOrderModelItem *item)
{
  if (item == NULL)
    return;

  g_free (item->secondary_id);
  g_free (item->config_id);
  g_free (item->icon);
  g_free (item->name);
  g_free (item->tooltip);
  g_free (item);
}



static ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_copy (const ThunarContextMenuOrderModelItem *source)
{
  ThunarContextMenuOrderModelItem *item = g_new0 (ThunarContextMenuOrderModelItem, 1);

  item->id = source->id;
  item->secondary_id = g_strdup (source->secondary_id);
  item->config_id = g_strdup (source->config_id);
  item->icon = g_strdup (source->icon);
  item->name = g_strdup (source->name);
  item->tooltip = g_strdup (source->tooltip);
  item->visibility = source->visibility;
  item->removable = source->removable;

  return item;
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

  if (order_model->deleted_items != NULL)
    {
      g_list_free_full (order_model->deleted_items, (GDestroyNotify) thunar_context_menu_order_model_item_free);
      order_model->deleted_items = NULL;
    }

  order_model->items = thunar_context_menu_order_model_get_default_items ();
  order_model->n_user_separators = 0;

  g_signal_emit (order_model, signals[CHANGED], 0);
}



void
thunar_context_menu_order_model_remove (ThunarContextMenuOrderModel *order_model,
                                        gint                         index)
{
  GList                           *link;
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model));
  _thunar_return_if_fail (index >= 0 && index < (gint) g_list_length (order_model->items));

  link = g_list_nth (order_model->items, index);
  item = link->data;
  _thunar_return_if_fail (item->removable);

  if (item->id == THUNAR_CONTEXT_MENU_ITEM_SEPARATOR)
    {
      /* custom separators can simply be removed, and default separators must be moved to a special list */
      if (g_str_has_prefix (item->secondary_id, "thunar-"))
        {
          order_model->items = g_list_remove_link (order_model->items, link);
          order_model->deleted_items = g_list_insert_before_link (order_model->deleted_items, NULL, link);
        }
      else
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
  gchar                           *name;
  ThunarContextMenuOrderModelItem *item;

  _thunar_return_val_if_fail (THUNAR_IS_CONTEXT_MENU_ORDER_MODEL (order_model), -1);

  if (index < 0 || index >= (gint) g_list_length (order_model->items))
    index = g_list_length (order_model->items);

  name = g_strdup_printf ("user-%d", order_model->n_user_separators++);
  item = thunar_context_menu_order_model_item_new (THUNAR_CONTEXT_MENU_ITEM_SEPARATOR, name, TRUE);
  g_free (name);

  order_model->items = g_list_insert (order_model->items, item, index);

  g_signal_emit (order_model, signals[CHANGED], 0);

  return index;
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



gboolean
thunar_context_menu_has_custom_action (const gchar *secondary_id)
{
  ThunarContextMenuOrderModel *order_model = thunar_context_menu_order_model_get_default ();
  gboolean                     has_action = FALSE;

  for (GList *l = order_model->items; l != NULL; l = l->next)
    {
      ThunarContextMenuOrderModelItem *item = l->data;

      if (item->id == THUNAR_CONTEXT_MENU_ITEM_CUSTOM_ACTION && g_strcmp0 (item->secondary_id, secondary_id) == 0)
        {
          has_action = TRUE;
          break;
        }
    }

  g_object_unref (order_model);

  return has_action;
}
