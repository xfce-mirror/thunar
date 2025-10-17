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

#ifndef __THUNAR_ORDER_EDITOR_H__
#define __THUNAR_ORDER_EDITOR_H__

#include "thunar/thunar-abstract-dialog.h"

#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

typedef struct _ThunarOrderEditor      ThunarOrderEditor;
typedef struct _ThunarOrderEditorClass ThunarOrderEditorClass;

#define THUNAR_TYPE_ORDER_EDITOR (thunar_order_editor_get_type ())
#define THUNAR_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ORDER_EDITOR, ThunarOrderEditor))
#define THUNAR_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ORDER_EDITOR, ThunarOrderEditorClass))
#define THUNAR_IS_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ORDER_EDITOR))
#define THUNAR_IS_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ORDER_EDITOR))
#define THUNAR_ORDER_EDITOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ORDER_EDITOR, ThunarOrderEditorClass))

struct _ThunarOrderEditorClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarOrderEditor
{
  ThunarAbstractDialog __parent__;
};

GType
thunar_order_editor_get_type (void) G_GNUC_CONST;

GtkContainer *
thunar_order_editor_get_description_area (ThunarOrderEditor *order_editor);

GtkContainer *
thunar_order_editor_get_settings_area (ThunarOrderEditor *order_editor);

void
thunar_order_editor_set_model (ThunarOrderEditor *order_editor,
                               XfceItemListModel *model);

void
thunar_order_editor_show (ThunarOrderEditor *order_editor,
                          GtkWidget         *window);

G_END_DECLS

#endif /* !__THUNAR_ORDER_EDITOR_H__ */
