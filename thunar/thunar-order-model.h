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
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_ORDER_MODEL (thunar_order_model_get_type ())
G_DECLARE_DERIVABLE_TYPE (ThunarOrderModel, thunar_order_model, THUNAR, ORDER_MODEL, GObject)

struct _ThunarOrderModelClass
{
  XfceItemListModelClass __parent__;

  void (*reset) (ThunarOrderModel *order_model);
};

void
thunar_order_model_reset (ThunarOrderModel *order_model);

G_END_DECLS

#endif /* !__THUNAR_ORDER_MODEL_H__ */
