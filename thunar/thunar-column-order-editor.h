#ifndef __THUNAR_COLUMN_ORDER_EDITOR_H__
#define __THUNAR_COLUMN_ORDER_EDITOR_H__

#include "thunar/thunar-order-editor.h"

G_BEGIN_DECLS

typedef struct _ThunarColumnOrderEditorClass ThunarColumnOrderEditorClass;
typedef struct _ThunarColumnOrderEditor      ThunarColumnOrderEditor;

#define THUNAR_TYPE_COLUMN_ORDER_EDITOR (thunar_column_order_editor_get_type ())
#define THUNAR_COLUMN_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_COLUMN_ORDER_EDITOR, ThunarColumnOrderEditor))
#define THUNAR_COLUMN_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_COLUMN_ORDER_EDITOR, ThunarColumnOrderEditorClass))
#define THUNAR_IS_COLUMN_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_COLUMN_ORDER_EDITOR))
#define THUNAR_IS_COLUMN_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_COLUMN_ORDER_EDITOR))
#define THUNAR_COLUMN_ORDER_EDITOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_COLUMN_ORDER_EDITOR, ThunarColumnOrderEditorClass))

GType
thunar_column_order_editor_get_type (void) G_GNUC_CONST;

void
thunar_column_order_editor_show (GtkWidget *window);

G_END_DECLS

#endif /* !__THUNAR_COLUMN_ORDER_EDITOR_H__ */
