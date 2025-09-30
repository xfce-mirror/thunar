#ifndef __THUNAR_TOOLBAR_ORDER_EDITOR_H__
#define __THUNAR_TOOLBAR_ORDER_EDITOR_H__

#include "thunar/thunar-order-editor.h"

G_BEGIN_DECLS

typedef struct _ThunarToolbarOrderEditor      ThunarToolbarOrderEditor;
typedef struct _ThunarToolbarOrderEditorClass ThunarToolbarOrderEditorClass;

#define THUNAR_TYPE_TOOLBAR_ORDER_EDITOR (thunar_toolbar_order_editor_get_type ())
#define THUNAR_TOOLBAR_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TOOLBAR_ORDER_EDITOR, ThunarToolbarOrderEditor))
#define THUNAR_TOOLBAR_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TOOLBAR_ORDER_EDITOR, ThunarToolbarOrderEditorClass))
#define THUNAR_IS_TOOLBAR_ORDER_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TOOLBAR_ORDER_EDITOR))
#define THUNAR_IS_TOOLBAR_ORDER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TOOLBAR_ORDER_EDITOR))
#define THUNAR_TOOLBAR_ORDER_EDITOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TOOLBAR_ORDER_EDITOR, ThunarToolbarOrderEditorClass))

GType
thunar_toolbar_order_editor_get_type (void) G_GNUC_CONST;

void
thunar_toolbar_order_editor_show (GtkWidget *window,
                                  GtkWidget *window_toolbar);

G_END_DECLS

#endif /* !__THUNAR_TOOLBAR_ORDER_EDITOR_H__ */
