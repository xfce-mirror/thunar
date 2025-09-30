#ifndef __THUNAR_TOOLBAR_ORDER_MODEL_H__
#define __THUNAR_TOOLBAR_ORDER_MODEL_H__

#include "thunar/thunar-order-model.h"

G_BEGIN_DECLS

typedef struct _ThunarToolbarOrderModel      ThunarToolbarOrderModel;
typedef struct _ThunarToolbarOrderModelClass ThunarToolbarOrderModelClass;

#define THUNAR_TYPE_TOOLBAR_ORDER_MODEL (thunar_toolbar_order_model_get_type ())
#define THUNAR_TOOLBAR_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TOOLBAR_ORDER_MODEL, ThunarToolbarOrderModel))
#define THUNAR_TOOLBAR_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TOOLBAR_ORDER_MODEL, ThunarToolbarOrderModelClass))
#define THUNAR_IS_TOOLBAR_ORDER_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TOOLBAR_ORDER_MODEL))
#define THUNAR_IS_TOOLBAR_ORDER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TOOLBAR_ORDER_MODEL))
#define THUNAR_TOOLBAR_ORDER_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TOOLBAR_ORDER_MODEL, ThunarToolbarOrderModelClass))

GType
thunar_toolbar_order_model_get_type (void) G_GNUC_CONST;

ThunarOrderModel *
thunar_toolbar_order_model_new (GtkWidget *toolbar);

G_END_DECLS

#endif /* !__THUNAR_TOOLBAR_ORDER_MODEL_H__ */
