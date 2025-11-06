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

#ifndef __THUNAR_CONTEXT_MENU_ORDER_MODEL_H__
#define __THUNAR_CONTEXT_MENU_ORDER_MODEL_H__

#include "thunar/thunar-action-manager.h"
#include "thunar/thunar-enum-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ThunarContextMenuOrderModel      ThunarContextMenuOrderModel;
typedef struct _ThunarContextMenuOrderModelClass ThunarContextMenuOrderModelClass;
typedef struct _ThunarContextMenuOrderModelItem  ThunarContextMenuOrderModelItem;

#define THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL (thunar_context_menu_order_model_get_type ())
#define THUNAR_CONTEXT_MENU_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModel))
#define THUNAR_CONTEXT_MENU_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModelClass))
#define THUNAR_IS_CONTEXT_MENU_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL))
#define THUNAR_IS_CONTEXT_MENU_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL))
#define THUNAR_CONTEXT_MENU_ORDER_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModelClass))

struct _ThunarContextMenuOrderModelItem
{
  ThunarContextMenuItem id;
  gchar                *secondary_id;
  gchar                *icon;
  gchar                *name;
  gchar                *tooltip;
  gboolean              visibility;
  gboolean              default_visibility;
  gint                  default_order;
};

GType
thunar_context_menu_order_model_get_type (void) G_GNUC_CONST;

ThunarContextMenuOrderModel *
thunar_context_menu_order_model_get_default (void);

GList *
thunar_context_menu_order_model_get_items (ThunarContextMenuOrderModel *order_model);

void
thunar_context_menu_order_model_move (ThunarContextMenuOrderModel *order_model,
                                      gint                         source_index,
                                      gint                         dest_index);

void
thunar_context_menu_order_model_set_visibility (ThunarContextMenuOrderModel *order_model,
                                                gint                         index,
                                                gboolean                     visibility);

void
thunar_context_menu_order_model_reset (ThunarContextMenuOrderModel *order_model);

void
thunar_context_menu_item_set_id (GtkWidget            *menu_item,
                                 ThunarContextMenuItem menu_item_id);

ThunarContextMenuItem
thunar_context_menu_item_get_id (GtkWidget *menu_item);

G_END_DECLS

#endif /* !__THUNAR_CONTEXT_MENU_ORDER_MODEL_H__ */
