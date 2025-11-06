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

#include "thunar/thunar-context-menu-order-editor.h"

#include "thunar/thunar-context-menu-order-model.h"
#include "thunar/thunar-order-editor.h"
#include "thunar/thunar-private.h"

#include <libxfce4ui/libxfce4ui.h>



struct _ThunarContextMenuOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarContextMenuOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarContextMenuOrderModel *order_model;
  XfceItemListStore           *store;
};



static void
thunar_context_menu_order_editor_finalize (GObject *object);



G_DEFINE_TYPE (ThunarContextMenuOrderEditor, thunar_context_menu_order_editor, THUNAR_TYPE_ORDER_EDITOR)



static void
thunar_context_menu_order_editor_class_init (ThunarContextMenuOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_context_menu_order_editor_finalize;
}



static void
thunar_context_menu_order_editor_init (ThunarContextMenuOrderEditor *menu_editor)
{
  GtkWidget *label;

  menu_editor->order_model = thunar_context_menu_order_model_get_default ();

  label = gtk_label_new (_("Setting the order and visibility of context menu elements. Menu items are grouped into sections."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_container_add (thunar_order_editor_get_description_area (THUNAR_ORDER_EDITOR (menu_editor)), label);
  gtk_widget_show (label);

  g_object_set (menu_editor, "title", _("Configure the context menu"), NULL);
}



static void
thunar_context_menu_order_editor_finalize (GObject *object)
{
  ThunarContextMenuOrderEditor *menu_editor = THUNAR_CONTEXT_MENU_ORDER_EDITOR (object);

  g_signal_handlers_disconnect_by_data (menu_editor->order_model, menu_editor);
  g_object_unref (menu_editor->order_model);

  G_OBJECT_CLASS (thunar_context_menu_order_editor_parent_class)->finalize (object);
}



static void
thunar_context_menu_order_editor_populate (ThunarContextMenuOrderEditor *menu_editor)
{
  GList *items = thunar_context_menu_order_model_get_items (menu_editor->order_model);

  xfce_item_list_store_clear (menu_editor->store);

  for (GList *l = items; l != NULL; l = l->next)
    {
      const ThunarContextMenuOrderModelItem *item = l->data;
      GIcon                                 *icon = item->icon != NULL ? g_themed_icon_new (item->icon) : NULL;

      xfce_item_list_store_insert_with_values (menu_editor->store, -1,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE, item->visibility,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE, TRUE,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ICON, icon,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_NAME, gettext (item->name),
                                               XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP, item->tooltip,
                                               -1);

      g_clear_object (&icon);
    }

  g_list_free (items);
}



static void
thunar_context_menu_order_editor_move (ThunarContextMenuOrderEditor *menu_editor,
                                       gint                          source_index,
                                       gint                          dest_index)
{
  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  thunar_context_menu_order_model_move (menu_editor->order_model, source_index, dest_index);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
}



static void
thunar_context_menu_order_editor_set_visibility (ThunarContextMenuOrderEditor *menu_editor,
                                                 gint                          index,
                                                 gboolean                      visibility)
{
  g_signal_handlers_block_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
  thunar_context_menu_order_model_set_visibility (menu_editor->order_model, index, visibility);
  g_signal_handlers_unblock_by_func (menu_editor->order_model, thunar_context_menu_order_editor_populate, menu_editor);
}



static void
thunar_context_menu_order_editor_reset (ThunarContextMenuOrderEditor *menu_editor)
{
  thunar_context_menu_order_model_reset (menu_editor->order_model);
}



void
thunar_context_menu_order_editor_show (GtkWidget *window)
{
  XfceItemListStore            *store = xfce_item_list_store_new (-1);
  ThunarContextMenuOrderEditor *menu_editor = g_object_new (THUNAR_TYPE_CONTEXT_MENU_ORDER_EDITOR, "model", store, NULL);

  menu_editor->store = store;

  g_object_set (store, "list-flags", XFCE_ITEM_LIST_MODEL_REORDERABLE | XFCE_ITEM_LIST_MODEL_RESETTABLE, NULL);
  g_signal_connect_swapped (store, "before-move-item", G_CALLBACK (thunar_context_menu_order_editor_move), menu_editor);
  g_signal_connect_swapped (store, "before-set-activity", G_CALLBACK (thunar_context_menu_order_editor_set_visibility), menu_editor);
  g_signal_connect_swapped (store, "reset", G_CALLBACK (thunar_context_menu_order_editor_reset), menu_editor);
  g_signal_connect_swapped (menu_editor->order_model, "changed", G_CALLBACK (thunar_context_menu_order_editor_populate), menu_editor);
  thunar_context_menu_order_editor_populate (menu_editor);
  g_object_unref (store);

  thunar_order_editor_show (THUNAR_ORDER_EDITOR (menu_editor), window);
}
