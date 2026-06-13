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

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

/*
 * A class for a model that stores the order and visibility of context menu items.
 *
 * Each element has a unique identifier in the form of a string, except for separators, they have one common id.
 * Custom action items have an ID starting with "custom-action-" and an arbitrary suffix. The IDs of standard Thunar
 * items (such as cut, paste, etc.) are specified by the ThunarContextMenuItem enumeration.
 *
 * To map a ThunarContextMenuOrderModelItem to a GtkMenuItem, each GtkMenuItem must have an "id" set
 * via g_object_set_data().
 *
 * A menu item with an ID equal to "custom-actions" has a special meaning; custom actions that are not present in the
 * model are inserted in its place, for example because the plugin does not support unique IDs for menu items.
 */
typedef struct _ThunarContextMenuOrderModel      ThunarContextMenuOrderModel;
typedef struct _ThunarContextMenuOrderModelClass ThunarContextMenuOrderModelClass;
typedef struct _ThunarContextMenuOrderModelItem  ThunarContextMenuOrderModelItem;

#define THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL (thunar_context_menu_order_model_get_type ())
#define THUNAR_CONTEXT_MENU_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModel))
#define THUNAR_CONTEXT_MENU_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModelClass))
#define THUNAR_IS_CONTEXT_MENU_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL))
#define THUNAR_IS_CONTEXT_MENU_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL))
#define THUNAR_CONTEXT_MENU_ORDER_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_CONTEXT_MENU_ORDER_MODEL, ThunarContextMenuOrderModelClass))

/* Items stored in ThunarContextMenuOrderModel */
struct _ThunarContextMenuOrderModelItem
{
  char    *id;
  gchar   *icon;
  gchar   *name;
  gchar   *tooltip;
  gboolean visibility;
  gboolean removable;
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
thunar_context_menu_order_model_remove (ThunarContextMenuOrderModel *order_model,
                                        const gint                  *indexes,
                                        gint                         n_indexes);

gint
thunar_context_menu_order_model_insert_separator (ThunarContextMenuOrderModel *order_model,
                                                  gint                         index);

ThunarContextMenuOrderModelItem *
thunar_context_menu_order_model_item_new_from_entry (const XfceGtkActionEntry *entry);

GList *
thunar_context_menu_order_model_item_new_list_from_entries (const XfceGtkActionEntry *entries,
                                                            guint                     n_entries,
                                                            const guint              *ids_of_entries,
                                                            guint                     n_ids_of_entries);

void
thunar_context_menu_item_set_id_by_entry (GtkWidget                *menu_item,
                                          const XfceGtkActionEntry *entry);

G_END_DECLS

#endif /* !__THUNAR_CONTEXT_MENU_ORDER_MODEL_H__ */
