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

#include "thunar/thunar-preferences.h"
#include "thunar/thunar-toolbar-order-model.h"

#include <libxfce4ui/libxfce4ui.h>


struct _ThunarToolbarOrderEditorClass
{
  ThunarOrderEditorClass __parent__;
};

struct _ThunarToolbarOrderEditor
{
  ThunarOrderEditor __parent__;

  ThunarPreferences *preferences;
};

static void
thunar_toolbar_order_editor_finalize (GObject *object);

static void
thunar_toolbar_order_editor_help (ThunarOrderEditor *order_editor);

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
                "reset-enabled", TRUE,
                NULL);
}

static void
thunar_toolbar_order_editor_finalize (GObject *object)
{
  ThunarToolbarOrderEditor *toolbar_editor = THUNAR_TOOLBAR_ORDER_EDITOR (object);

  g_clear_object (&toolbar_editor->preferences);

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

void
thunar_toolbar_order_editor_show (GtkWidget *window,
                                  GtkWidget *window_toolbar)
{
  ThunarToolbarOrderEditor *toolbar_editor;
  ThunarOrderModel         *model = thunar_toolbar_order_model_new (window_toolbar);

  toolbar_editor = g_object_new (THUNAR_TYPE_TOOLBAR_ORDER_EDITOR,
                                 "model", model,
                                 NULL);
  g_object_unref (model);
  thunar_order_editor_show (THUNAR_ORDER_EDITOR (toolbar_editor), window);
}
