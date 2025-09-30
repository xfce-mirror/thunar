#ifndef __THUNAR_ORDER_EDITOR_H__
#define __THUNAR_ORDER_EDITOR_H__

#include "thunar/thunar-abstract-dialog.h"
#include "thunar/thunar-order-model.h"

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
                               ThunarOrderModel  *model);

void
thunar_order_editor_show (ThunarOrderEditor *order_editor,
                          GtkWidget         *window);

G_END_DECLS

#endif /* !__THUNAR_ORDER_EDITOR_H__ */
