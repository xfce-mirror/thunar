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

#include "thunar/thunar-toolbar-order-editor.h"

#include "thunar/thunar-application.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-window.h"

#include <libxfce4ui/libxfce4ui.h>



struct _ThunarToolbarOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarToolbarOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarApplication *application;
  ThunarPreferences *preferences;
  GtkWidget         *toolbar;
  GList             *children;
  XfceItemListStore *store;
};



static void
thunar_toolbar_order_editor_finalize (GObject *object);

static void
thunar_toolbar_order_editor_help (ThunarOrderEditor *order_editor);

static void
thunar_toolbar_order_editor_populate (ThunarToolbarOrderEditor *toolbar_editor);

static void
thunar_toolbar_order_editor_move (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      source_index,
                                  gint                      dest_index);

static void
thunar_toolbar_order_editor_set_activity (ThunarToolbarOrderEditor *toolbar_editor,
                                          gint                      index,
                                          gboolean                  value);

static gint
thunar_toolbar_order_editor_compare_order (GObject *a,
                                           GObject *b);

static void
thunar_toolbar_order_editor_reset (ThunarToolbarOrderEditor *toolbar_editor);

static void
thunar_toolbar_order_editor_save (ThunarToolbarOrderEditor *toolbar_editor);


G_DEFINE_TYPE (ThunarToolbarOrderEditor, thunar_toolbar_order_editor, THUNAR_TYPE_ORDER_EDITOR)



static void
thunar_toolbar_order_editor_class_init (ThunarToolbarOrderEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = thunar_toolbar_order_editor_finalize;
}



static void
thunar_toolbar_order_editor_init (ThunarToolbarOrderEditor *toolbar_editor)
{
  ThunarOrderEditor *order_editor = THUNAR_ORDER_EDITOR (toolbar_editor);
  GtkWidget         *label;
  GtkWidget         *button;

  toolbar_editor->application = thunar_application_get ();
  toolbar_editor->preferences = thunar_preferences_get ();

  label = gtk_label_new (_("Configure the order and visibility of toolbar items.\n"
                           "Note that toolbar items are always executed for the current directory."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, FALSE);
  gtk_container_add (thunar_order_editor_get_description_area (order_editor), label);
  gtk_widget_show (label);

  button = gtk_check_button_new_with_mnemonic (_("_Symbolic Icons"));
  g_object_bind_property (G_OBJECT (toolbar_editor->preferences),
                          "misc-symbolic-icons-in-toolbar",
                          G_OBJECT (button),
                          "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("Use symbolic icons instead of regular ones (if available)."));
  gtk_container_add (thunar_order_editor_get_settings_area (order_editor), button);
  gtk_widget_show (button);

  g_signal_connect (toolbar_editor, "help", G_CALLBACK (thunar_toolbar_order_editor_help), NULL);
  g_object_set (toolbar_editor,
                "title", _("Configure the Toolbar"),
                "help-enabled", TRUE,
                NULL);
}



static void
thunar_toolbar_order_editor_finalize (GObject *object)
{
  ThunarToolbarOrderEditor *toolbar_editor = THUNAR_TOOLBAR_ORDER_EDITOR (object);

  g_signal_handlers_disconnect_by_data (toolbar_editor->preferences, toolbar_editor);
  g_clear_object (&toolbar_editor->application);
  g_clear_object (&toolbar_editor->preferences);
  g_clear_pointer (&toolbar_editor->children, g_list_free);

  G_OBJECT_CLASS (thunar_toolbar_order_editor_parent_class)->finalize (object);
}



static void
thunar_toolbar_order_editor_help (ThunarOrderEditor *order_editor)
{
  xfce_dialog_show_help (GTK_WINDOW (order_editor),
                         "thunar",
                         "the-file-manager-window",
                         "toolbar_customization");
}



static void
thunar_toolbar_order_editor_populate (ThunarToolbarOrderEditor *toolbar_editor)

{
  xfce_item_list_store_clear (toolbar_editor->store);

  g_clear_pointer (&toolbar_editor->children, g_list_free);
  toolbar_editor->children = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));

  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
    {
      GtkWidget   *item = GTK_WIDGET (l->data);
      const gchar *id = g_object_get_data (G_OBJECT (item), "id");
      gboolean     is_menu = g_strcmp0 (id, "menu") == 0;
      const gchar *icon_name = g_object_get_data (G_OBJECT (item), "icon");
      GIcon       *icon = !xfce_str_is_empty (icon_name) ? g_themed_icon_new (icon_name) : NULL;
      const gchar *name = g_object_get_data (G_OBJECT (item), "label");
      GtkWidget   *label = gtk_label_new_with_mnemonic (name);
      const gchar *tooltip = is_menu ? _("Only visible when the menubar is hidden") : NULL;

      xfce_item_list_store_insert_with_values (toolbar_editor->store, -1,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVE, is_menu || gtk_widget_is_visible (item),
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ACTIVABLE, !is_menu,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_ICON, icon,
                                               XFCE_ITEM_LIST_MODEL_COLUMN_NAME, gtk_label_get_text (GTK_LABEL (label)),
                                               XFCE_ITEM_LIST_MODEL_COLUMN_TOOLTIP, tooltip,
                                               -1);

      g_clear_object (&icon);
      g_object_ref_sink (label);
      g_object_unref (label);
    }
}



static void
thunar_toolbar_order_editor_move (ThunarToolbarOrderEditor *toolbar_editor,
                                  gint                      source_index,
                                  gint                      dest_index)
{
  GList *windows = thunar_application_get_windows (toolbar_editor->application);

  /* Changes the order for all windows */
  for (GList *l = windows; l != NULL; l = l->next)
    thunar_window_toolbar_move_item (THUNAR_WINDOW (l->data), source_index, dest_index);

  g_list_free (windows);

  g_clear_pointer (&toolbar_editor->children, g_list_free);
  toolbar_editor->children = gtk_container_get_children (GTK_CONTAINER (toolbar_editor->toolbar));
}



static void
thunar_toolbar_order_editor_set_activity (ThunarToolbarOrderEditor *toolbar_editor,
                                          gint                      index,
                                          gboolean                  value)
{
  GList *windows = thunar_application_get_windows (toolbar_editor->application);

  for (GList *l = windows; l != NULL; l = l->next)
    thunar_window_toolbar_toggle_item_visibility (THUNAR_WINDOW (l->data), index);

  g_list_free (windows);
}



static gint
thunar_toolbar_order_editor_compare_order (GObject *a,
                                           GObject *b)
{
  gint a_order = *(const gint *) g_object_get_data (a, "default-order");
  gint b_order = *(const gint *) g_object_get_data (b, "default-order");

  return (a_order > b_order) - (a_order < b_order);
}



static void
thunar_toolbar_order_editor_reset (ThunarToolbarOrderEditor *toolbar_editor)
{
  GList *new_order = NULL;
  gint   index = 0;

  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
    new_order = g_list_insert_sorted (new_order, l->data, (GCompareFunc) thunar_toolbar_order_editor_compare_order);

  for (GList *l = new_order; l != NULL; l = l->next, ++index)
    xfce_item_list_model_move (XFCE_ITEM_LIST_MODEL (toolbar_editor->store), g_list_index (toolbar_editor->children, l->data), index);

  g_list_free (new_order);

  thunar_toolbar_order_editor_populate (toolbar_editor);
}



static void
thunar_toolbar_order_editor_save (ThunarToolbarOrderEditor *toolbar_editor)
{
  GString *items = g_string_sized_new (1024);

  /* block signal */
  g_signal_handlers_block_by_func (toolbar_editor->preferences, thunar_toolbar_order_editor_save, toolbar_editor);

  /* read the internal id and visibility column values and store them */
  for (GList *l = toolbar_editor->children; l != NULL; l = l->next)
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
  g_object_set (toolbar_editor->preferences, "last-toolbar-items", items->str, NULL);

  /* release the string */
  g_string_free (items, TRUE);

  /* unblock signal */
  g_signal_handlers_unblock_by_func (toolbar_editor->preferences, thunar_toolbar_order_editor_save, toolbar_editor);
}



void
thunar_toolbar_order_editor_show (GtkWidget *window,
                                  GtkWidget *window_toolbar)
{
  XfceItemListStore        *store = xfce_item_list_store_new (-1);
  ThunarToolbarOrderEditor *toolbar_editor = g_object_new (THUNAR_TYPE_TOOLBAR_ORDER_EDITOR, "model", store, NULL);

  toolbar_editor->toolbar = window_toolbar;
  toolbar_editor->store = store;

  g_object_set (store, "list-flags", XFCE_ITEM_LIST_MODEL_REORDERABLE | XFCE_ITEM_LIST_MODEL_RESETTABLE, NULL);
  g_signal_connect_swapped (store, "before-move-item", G_CALLBACK (thunar_toolbar_order_editor_move), toolbar_editor);
  g_signal_connect_swapped (store, "before-set-activity", G_CALLBACK (thunar_toolbar_order_editor_set_activity), toolbar_editor);
  g_signal_connect_swapped (store, "reset", G_CALLBACK (thunar_toolbar_order_editor_reset), toolbar_editor);
  g_signal_connect_swapped (toolbar_editor->preferences, "notify::last-toolbar-items", G_CALLBACK (thunar_toolbar_order_editor_populate), toolbar_editor);
  thunar_toolbar_order_editor_populate (toolbar_editor);
  g_object_unref (store);

  g_signal_connect (toolbar_editor, "close", G_CALLBACK (thunar_toolbar_order_editor_save), NULL);

  thunar_order_editor_show (THUNAR_ORDER_EDITOR (toolbar_editor), window);
}
