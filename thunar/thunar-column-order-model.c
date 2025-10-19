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

#include "thunar/thunar-column-order-model.h"

#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"

#include <libxfce4util/libxfce4util.h>



struct _ThunarColumnOrderModelClass
{
  XfceItemListModelClass __parent__;
};

struct _ThunarColumnOrderModel
{
  XfceItemListModel __parent__;

  ThunarPreferences *preferences;
  ThunarColumnModel *model;
};



static void
thunar_column_order_model_finalize (GObject *object);

static gint
thunar_column_order_model_get_n_items (XfceItemListModel *item_model);

static void
thunar_column_order_model_get_value (XfceItemListModel *item_model,
                                     gint               position,
                                     gint               column,
                                     GValue            *value);

static void
thunar_column_order_model_set_activity (XfceItemListModel *item_model,
                                        gint               position,
                                        gboolean           activity);

static void
thunar_column_order_model_move (XfceItemListModel *item_model,
                                gint               a_position,
                                gint               b_position);

static void
thunar_column_order_model_reset (XfceItemListModel *item_model);

static ThunarColumn
thunar_column_order_model_get_model_column_nth (ThunarColumnOrderModel *column_model,
                                                gint                    position);



G_DEFINE_TYPE (ThunarColumnOrderModel, thunar_column_order_model, XFCE_TYPE_ITEM_LIST_MODEL)



static void
thunar_column_order_model_class_init (ThunarColumnOrderModelClass *klass)
{
  GObjectClass           *object_class = G_OBJECT_CLASS (klass);
  XfceItemListModelClass *item_model_class = XFCE_ITEM_LIST_MODEL_CLASS (klass);

  object_class->finalize = thunar_column_order_model_finalize;

  item_model_class->get_n_items = thunar_column_order_model_get_n_items;
  item_model_class->get_item_value = thunar_column_order_model_get_value;
  item_model_class->set_activity = thunar_column_order_model_set_activity;
  item_model_class->move = thunar_column_order_model_move;
  item_model_class->reset = thunar_column_order_model_reset;
}



static void
thunar_column_order_model_init (ThunarColumnOrderModel *column_model)
{
  g_object_set (column_model, "list-flags", XFCE_ITEM_LIST_MODEL_REORDERABLE | XFCE_ITEM_LIST_MODEL_RESETTABLE, NULL);

  column_model->preferences = thunar_preferences_get ();
  column_model->model = thunar_column_model_get_default ();

  g_signal_connect_swapped (G_OBJECT (column_model->preferences), "notify::last-details-view-column-order",
                            G_CALLBACK (xfce_item_list_model_changed), XFCE_ITEM_LIST_MODEL (column_model));
  g_signal_connect_swapped (G_OBJECT (column_model->preferences), "notify::last-details-view-visible-columns",
                            G_CALLBACK (xfce_item_list_model_changed), XFCE_ITEM_LIST_MODEL (column_model));
}



static void
thunar_column_order_model_finalize (GObject *object)
{
  ThunarColumnOrderModel *column_model = THUNAR_COLUMN_ORDER_MODEL (object);

  g_signal_handlers_disconnect_by_data (column_model->preferences, XFCE_ITEM_LIST_MODEL (column_model));
  g_clear_object (&column_model->preferences);
  g_clear_object (&column_model->model);

  G_OBJECT_CLASS (thunar_column_order_model_parent_class)->finalize (object);
}



static gint
thunar_column_order_model_get_n_items (XfceItemListModel *item_model)
{
  return THUNAR_N_VISIBLE_COLUMNS;
}



static void
thunar_column_order_model_get_value (XfceItemListModel *item_model,
                                     gint               position,
                                     gint               column,
                                     GValue            *value)
{
  ThunarColumnOrderModel *column_model = THUNAR_COLUMN_ORDER_MODEL (item_model);
  ThunarColumn            model_column = thunar_column_order_model_get_model_column_nth (column_model, position);

  switch (column)
    {
    case XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE:
      g_value_set_boolean (value, thunar_column_model_get_column_visible (column_model->model, model_column)
                                  || model_column == THUNAR_COLUMN_NAME
                                  || thunar_column_is_special (model_column));
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE:
      g_value_set_boolean (value, model_column != THUNAR_COLUMN_NAME && !thunar_column_is_special (model_column));
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_ICON:
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_NAME:
      g_value_set_string (value, thunar_column_model_get_column_name (column_model->model, model_column));
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP:
      if (thunar_column_is_special (model_column))
        g_value_set_string (value, _("This column is reserved for special locations"));
      break;

    default:
      g_warn_if_reached ();
    }
}



static void
thunar_column_order_model_set_activity (XfceItemListModel *item_model,
                                        gint               position,
                                        gboolean           activity)
{
  ThunarColumnOrderModel *column_model = THUNAR_COLUMN_ORDER_MODEL (item_model);
  ThunarColumn            model_column = thunar_column_order_model_get_model_column_nth (column_model, position);

  g_signal_handlers_block_by_func (G_OBJECT (column_model->preferences), G_CALLBACK (xfce_item_list_model_changed), item_model);
  thunar_column_model_set_column_visible (column_model->model, model_column, activity);
  g_signal_handlers_unblock_by_func (G_OBJECT (column_model->preferences), G_CALLBACK (xfce_item_list_model_changed), item_model);
}



static void
thunar_column_order_model_move (XfceItemListModel *item_model,
                                gint               a_position,
                                gint               b_position)
{
  ThunarColumnOrderModel *column_model = THUNAR_COLUMN_ORDER_MODEL (item_model);

  g_signal_handlers_block_by_func (G_OBJECT (column_model->preferences), G_CALLBACK (xfce_item_list_model_changed), item_model);
  thunar_column_model_move (column_model->model, a_position, b_position);
  g_signal_handlers_unblock_by_func (G_OBJECT (column_model->preferences), G_CALLBACK (xfce_item_list_model_changed), item_model);
}



static void
thunar_column_order_model_reset (XfceItemListModel *item_model)
{
  ThunarColumnOrderModel *column_model = THUNAR_COLUMN_ORDER_MODEL (item_model);

  thunar_column_model_reset (column_model->model);
}



static ThunarColumn
thunar_column_order_model_get_model_column_nth (ThunarColumnOrderModel *column_model,
                                                gint                    position)
{
  const ThunarColumn *order = thunar_column_model_get_column_order (column_model->model);

  g_assert (position >= 0 && position < THUNAR_N_VISIBLE_COLUMNS);
  return order[position];
}



XfceItemListModel *
thunar_column_order_model_new (void)
{
  return g_object_new (THUNAR_TYPE_COLUMN_ORDER_MODEL, NULL);
}
