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

#include "thunar/thunar-order-model.h"

#include "thunar/thunar-private.h"



G_DEFINE_TYPE (ThunarOrderModel, thunar_order_model, XFCE_TYPE_ITEM_LIST_MODEL)



static void
thunar_order_model_class_init (ThunarOrderModelClass *klass)
{
}



static void
thunar_order_model_init (ThunarOrderModel *order_model)
{
}

void
thunar_order_model_reset (ThunarOrderModel *order_model)
{
  ThunarOrderModelClass *order_model_class;

  _thunar_return_if_fail (THUNAR_IS_ORDER_MODEL (order_model));

  order_model_class = THUNAR_ORDER_MODEL_GET_CLASS (order_model);
  _thunar_return_if_fail (order_model_class->reset != NULL);
  order_model_class->reset (order_model);
  xfce_item_list_model_changed (XFCE_ITEM_LIST_MODEL (order_model));
}
