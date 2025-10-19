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

#include "thunar/thunar-toolbar-order-model.h"

#include "thunar/thunar-application.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-window.h"



struct _ThunarToolbarOrderModelClass
{
  XfceItemListModelClass __parent__;
};

struct _ThunarToolbarOrderModel
{
  XfceItemListModel __parent__;

  ThunarPreferences *preferences;
  GtkWidget         *toolbar;
  GList             *children;
};



static void
thunar_toolbar_order_model_finalize (GObject *object);

static gint
thunar_toolbar_order_model_get_n_items (XfceItemListModel *item_model);

static void
thunar_toolbar_order_model_get_value (XfceItemListModel *item_model,
                                      gint               position,
                                      gint               column,
                                      GValue            *value);

static void
thunar_toolbar_order_model_set_activity (XfceItemListModel *item_model,
                                         gint               position,
                                         gboolean           activity);


static void
thunar_toolbar_order_model_move (XfceItemListModel *item_model,
                                 gint               a_position,
                                 gint               b_position);

static void
thunar_toolbar_order_model_swap (ThunarToolbarOrderModel *toolbar_model,
                                 gint                     a_position,
                                 gint                     b_position);

static void
thunar_toolbar_order_model_reset (XfceItemListModel *item_model);

static void
thunar_toolbar_order_model_set_toolbar (ThunarToolbarOrderModel *toolbar_model,
                                        GtkWidget               *toolbar);

static void
thunar_toolbar_order_model_save (ThunarToolbarOrderModel *toolbar_model);



G_DEFINE_TYPE (ThunarToolbarOrderModel, thunar_toolbar_order_model, XFCE_TYPE_ITEM_LIST_MODEL)



static void
thunar_toolbar_order_model_class_init (ThunarToolbarOrderModelClass *klass)
{
  GObjectClass           *object_class = G_OBJECT_CLASS (klass);
  XfceItemListModelClass *item_model_class = XFCE_ITEM_LIST_MODEL_CLASS (klass);

  object_class->finalize = thunar_toolbar_order_model_finalize;

  item_model_class->get_n_items = thunar_toolbar_order_model_get_n_items;
  item_model_class->get_item_value = thunar_toolbar_order_model_get_value;
  item_model_class->set_activity = thunar_toolbar_order_model_set_activity;
  item_model_class->move = thunar_toolbar_order_model_move;
  item_model_class->reset = thunar_toolbar_order_model_reset;
}



static void
thunar_toolbar_order_model_init (ThunarToolbarOrderModel *toolbar_model)
{
  g_object_set (toolbar_model, "list-flags", XFCE_ITEM_LIST_MODEL_REORDERABLE | XFCE_ITEM_LIST_MODEL_RESETTABLE, NULL);
}



static void
thunar_toolbar_order_model_finalize (GObject *object)
{
  ThunarToolbarOrderModel *toolbar_model = THUNAR_TOOLBAR_ORDER_MODEL (object);

  thunar_toolbar_order_model_save (toolbar_model);

  g_signal_handlers_disconnect_by_data (toolbar_model->preferences, toolbar_model);
  g_clear_object (&toolbar_model->preferences);

  g_list_free (toolbar_model->children);
  g_object_unref (toolbar_model->toolbar);

  G_OBJECT_CLASS (thunar_toolbar_order_model_parent_class)->finalize (object);
}



static gint
thunar_toolbar_order_model_get_n_items (XfceItemListModel *item_model)
{
  ThunarToolbarOrderModel *toolbar_model = THUNAR_TOOLBAR_ORDER_MODEL (item_model);

  return g_list_length (toolbar_model->children);
}



static void
thunar_toolbar_order_model_get_value (XfceItemListModel *item_model,
                                      gint               position,
                                      gint               column,
                                      GValue            *value)
{
  ThunarToolbarOrderModel *toolbar_model = THUNAR_TOOLBAR_ORDER_MODEL (item_model);
  GtkWidget               *item = g_list_nth_data (toolbar_model->children, position);
  const gchar             *id;
  const gchar             *icon;
  const gchar             *name;
  GtkWidget               *label;

  switch (column)
    {
    case XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE:
      g_value_set_boolean (value, gtk_widget_is_visible (item));
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE:
      id = g_object_get_data (G_OBJECT (item), "id");
      g_value_set_boolean (value, g_strcmp0 (id, "menu") != 0);
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_ICON:
      icon = g_object_get_data (G_OBJECT (item), "icon");
      if (icon != NULL && *icon != '\0')
        g_value_set_object (value, g_themed_icon_new (icon));
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_NAME:
      name = g_object_get_data (G_OBJECT (item), "label");
      label = gtk_label_new_with_mnemonic (name);
      g_object_ref_sink (label);
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (label)));
      g_object_unref (label);
      break;

    case XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP:
      id = g_object_get_data (G_OBJECT (item), "id");
      if (g_strcmp0 (id, "menu") == 0)
        g_value_set_string (value, _("Only visible when the menubar is hidden"));
      else
        g_value_set_string (value, NULL);
      break;

    default:
      g_warn_if_reached ();
    }
}



static void
thunar_toolbar_order_model_set_activity (XfceItemListModel *item_model,
                                         gint               position,
                                         gboolean           activity)
{
  ThunarApplication *application = thunar_application_get ();
  GList             *windows = thunar_application_get_windows (application);

  for (GList *lp = windows; lp != NULL; lp = lp->next)
    thunar_window_toolbar_toggle_item_visibility (THUNAR_WINDOW (lp->data), position);

  g_object_unref (application);
  g_list_free (windows);
}



static void
thunar_toolbar_order_model_move (XfceItemListModel *item_model,
                                 gint               a_position,
                                 gint               b_position)
{
  ThunarToolbarOrderModel *toolbar_model = THUNAR_TOOLBAR_ORDER_MODEL (item_model);
  ThunarApplication       *application = thunar_application_get ();
  GList                   *list_item = g_list_nth (toolbar_model->children, a_position);
  GList                   *windows;

  g_return_if_fail (list_item != NULL);

  /* Changing the local list */
  toolbar_model->children = g_list_remove_link (toolbar_model->children, list_item);
  toolbar_model->children = g_list_insert_before_link (toolbar_model->children,
                                                       g_list_nth (toolbar_model->children, b_position),
                                                       list_item);

  /* Changes the order for all windows */
  windows = thunar_application_get_windows (application);
  for (GList *lp = windows; lp != NULL; lp = lp->next)
    thunar_window_toolbar_move_item_before (THUNAR_WINDOW (lp->data), a_position, b_position);
}



static void
thunar_toolbar_order_model_swap (ThunarToolbarOrderModel *toolbar_model,
                                 gint                     a_position,
                                 gint                     b_position)
{
  if (a_position > b_position)
    thunar_toolbar_order_model_move (XFCE_ITEM_LIST_MODEL (toolbar_model), a_position, b_position);
  else
    thunar_toolbar_order_model_move (XFCE_ITEM_LIST_MODEL (toolbar_model), b_position, a_position);
}



static void
thunar_toolbar_order_model_reset (XfceItemListModel *item_model)
{
  ThunarToolbarOrderModel *toolbar_model = THUNAR_TOOLBAR_ORDER_MODEL (item_model);
  guint                    item_count = g_list_length (toolbar_model->children);
  guint                    target_order[item_count];
  guint                    current_order[item_count];
  guint                    index = 0;

  for (GList *lp = toolbar_model->children; lp != NULL; lp = lp->next)
    {
      GtkWidget *item = lp->data;
      gint      *order = NULL;

      order = g_object_get_data (G_OBJECT (item), "default-order");
      current_order[index] = *order;
      target_order[index] = index;
      index++;
    }

  /* now rearrange the toolbar items, the goal is to make the current_order like the target_order */
  for (guint i = 0; i < item_count; i++)
    {
      guint x = target_order[i];
      for (guint j = 0; j < item_count; j++)
        {
          guint y = current_order[j];
          if (x == y && i != j)
            {
              thunar_toolbar_order_model_swap (toolbar_model, i, j);
              y = current_order[i];
              current_order[i] = target_order[i];
              current_order[j] = y;
              break;
            }
        }
    }
}



static void
thunar_toolbar_order_model_set_toolbar (ThunarToolbarOrderModel *toolbar_model,
                                        GtkWidget               *toolbar)
{
  toolbar_model->toolbar = g_object_ref (toolbar);
  toolbar_model->children = gtk_container_get_children (GTK_CONTAINER (toolbar));
  toolbar_model->preferences = thunar_preferences_get ();
  g_signal_connect_swapped (toolbar_model->preferences, "notify::misc-symbolic-icons-in-toolbar",
                            G_CALLBACK (xfce_item_list_model_changed), XFCE_ITEM_LIST_MODEL (toolbar_model));
}



static void
thunar_toolbar_order_model_save (ThunarToolbarOrderModel *toolbar_model)
{
  GString *items = g_string_sized_new (1024);

  /* read the internal id and visibility column values and store them */
  for (GList *l = toolbar_model->children; l != NULL; l = l->next)
    {
      gchar   *id;
      gboolean visible;

      /* get the id value of the entry */
      id = g_object_get_data (l->data, "id");
      if (id == NULL)
        continue;

      /* append a comma if not empty */
      if (*items->str != '\0')
        g_string_append_c (items, ',');

      /* store the id value */
      g_string_append (items, id);

      /* append the separator character */
      g_string_append_c (items, ':');

      /* get the visibility value of the entry and store it */
      g_object_get (l->data, "visible", &visible, NULL);
      g_string_append_printf (items, "%i", visible);
    }

  /* save the toolbar configuration */
  g_object_set (toolbar_model->preferences, "last-toolbar-items", items->str, NULL);

  /* release the string */
  g_string_free (items, TRUE);
}



XfceItemListModel *
thunar_toolbar_order_model_new (GtkWidget *toolbar)
{
  ThunarToolbarOrderModel *toolbar_model = g_object_new (THUNAR_TYPE_TOOLBAR_ORDER_MODEL, NULL);

  thunar_toolbar_order_model_set_toolbar (toolbar_model, toolbar);
  return XFCE_ITEM_LIST_MODEL (toolbar_model);
}
