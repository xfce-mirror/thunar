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

#ifndef __THUNAR_ORDER_MODEL_H__
#define __THUNAR_ORDER_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  THUNAR_ORDER_MODEL_COLUMN_ACTIVE,
  THUNAR_ORDER_MODEL_COLUMN_MUTABLE,
  THUNAR_ORDER_MODEL_COLUMN_ICON,
  THUNAR_ORDER_MODEL_COLUMN_NAME,
  THUNAR_ORDER_MODEL_COLUMN_TOOLTIP,
  THUNAR_ORDER_MODEL_N_COLUMNS,
} ThunarOrderModelColumn;

#define THUNAR_TYPE_ORDER_MODEL (thunar_order_model_get_type ())
G_DECLARE_DERIVABLE_TYPE (ThunarOrderModel, thunar_order_model, THUNAR, ORDER_MODEL, GObject)

struct _ThunarOrderModelClass
{
  GObjectClass __parent__;

  gint (*get_n_items) (ThunarOrderModel *order_model);

  void (*get_value) (ThunarOrderModel      *order_model,
                     gint                   position,
                     ThunarOrderModelColumn column,
                     GValue                *value);

  void (*set_activity) (ThunarOrderModel *order_model,
                        gint              position,
                        gboolean          activity);

  void (*move_before) (ThunarOrderModel *order_model,
                       gint              a_position,
                       gint              b_position);

  void (*reset) (ThunarOrderModel *order_model);
};

void
thunar_order_model_set_activity (ThunarOrderModel *order_model,
                                 GtkTreeIter      *iter,
                                 gboolean          activity);

void
thunar_order_model_swap_items (ThunarOrderModel *order_model,
                               GtkTreeIter      *a_iter,
                               GtkTreeIter      *b_iter);

void
thunar_order_model_move_before (ThunarOrderModel *order_model,
                                GtkTreeIter      *a_iter,
                                GtkTreeIter      *b_iter);

void
thunar_order_model_reset (ThunarOrderModel *order_model);

void
thunar_order_model_reload (ThunarOrderModel *order_model);

G_END_DECLS

#endif /* !__THUNAR_ORDER_MODEL_H__ */
