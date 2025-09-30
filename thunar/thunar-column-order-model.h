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

#ifndef __THUNAR_COLUMN_ORDER_MODEL_H__
#define __THUNAR_COLUMN_ORDER_MODEL_H__

#include "thunar/thunar-column-model.h"
#include "thunar/thunar-order-model.h"

G_BEGIN_DECLS

typedef struct _ThunarColumnOrderModel      ThunarColumnOrderModel;
typedef struct _ThunarColumnOrderModelClass ThunarColumnOrderModelClass;

#define THUNAR_TYPE_COLUMN_ORDER_MODEL (thunar_column_order_model_get_type ())
#define THUNAR_COLUMN_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_COLUMN_ORDER_MODEL, ThunarColumnOrderModel))
#define THUNAR_COLUMN_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_COLUMN_ORDER_MODEL, ThunarColumnOrderModelClass))
#define THUNAR_IS_COLUMN_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_COLUMN_ORDER_MODEL))
#define THUNAR_IS_COLUMN_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_COLUMN_ORDER_MODEL))
#define THUNAR_COLUMN_ORDER_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_COLUMN_ORDER_MODEL, ThunarColumnOrderModelClass))

GType
thunar_column_order_model_get_type (void) G_GNUC_CONST;

ThunarOrderModel *
thunar_column_order_model_new (void);

G_END_DECLS

#endif
