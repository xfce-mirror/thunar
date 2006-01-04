/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-advanced-permissions-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-permissions-model.h>
#include <thunar/thunar-permissions-view.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void     thunar_permissions_view_class_init          (ThunarPermissionsViewClass *klass);
static void     thunar_permissions_view_init                (ThunarPermissionsView      *view);
static void     thunar_permissions_view_finalize            (GObject                    *object);
static void     thunar_permissions_view_get_property        (GObject                    *object,
                                                             guint                       prop_id,
                                                             GValue                     *value,
                                                             GParamSpec                 *pspec);
static void     thunar_permissions_view_set_property        (GObject                    *object,
                                                             guint                       prop_id,
                                                             const GValue               *value,
                                                             GParamSpec                 *pspec);
static gboolean thunar_permissions_view_context_menu        (ThunarPermissionsView      *view,
                                                             GtkTreeIter                *iter,
                                                             guint                       button,
                                                             guint32                     time);
static void     thunar_permissions_view_advanced_clicked    (GtkWidget                  *button,
                                                             ThunarPermissionsView      *view);
static gboolean thunar_permissions_view_button_press_event  (GtkWidget                  *tree_view,
                                                             GdkEventButton             *event,
                                                             ThunarPermissionsView      *view);
static gboolean thunar_permissions_view_popup_menu          (GtkWidget                  *tree_view,
                                                             ThunarPermissionsView      *view);
static void     thunar_permissions_view_row_activated       (GtkWidget                  *tree_view,
                                                             GtkTreePath                *path,
                                                             GtkTreeViewColumn          *column,
                                                             ThunarPermissionsView      *view);
static void     thunar_permissions_view_toggled             (GtkCellRendererToggle      *renderer,
                                                             const gchar                *path_string,
                                                             ThunarPermissionsView      *view);



struct _ThunarPermissionsViewClass
{
  GtkVBoxClass __parent__;
};

struct _ThunarPermissionsView
{
  GtkVBox __parent__;

  ThunarFile  *file;

  GtkTooltips *tooltips;
  GtkWidget   *tree_view;
};



static GObjectClass *thunar_permissions_view_parent_class;



GType
thunar_permissions_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPermissionsViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_permissions_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarPermissionsView),
        0,
        (GInstanceInitFunc) thunar_permissions_view_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_VBOX, I_("ThunarPermissionsView"), &info, 0);
    }

  return type;
}



static void
thunar_permissions_view_class_init (ThunarPermissionsViewClass *klass)
{
  GObjectClass *gobject_class;

  thunar_permissions_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_permissions_view_finalize;
  gobject_class->get_property = thunar_permissions_view_get_property;
  gobject_class->set_property = thunar_permissions_view_set_property;

  /**
   * ThunarPermissionsView:file:
   *
   * The #ThunarFile whose permissions is displayed by this view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_permissions_view_init (ThunarPermissionsView *view)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkWidget         *align;
  GtkWidget         *button;
  GtkWidget         *hbox;
  GtkWidget         *image;
  GtkWidget         *label;
  GtkWidget         *scrolled_window;
  GtkWidget         *separator;

  /* allocate tooltips for the permissions view */
  view->tooltips = gtk_tooltips_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (view->tooltips));

  gtk_box_set_spacing (GTK_BOX (view), 6);
  gtk_container_set_border_width (GTK_CONTAINER (view), 12);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (view), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  view->tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view->tree_view), FALSE);
  g_signal_connect (G_OBJECT (view->tree_view), "button-press-event", G_CALLBACK (thunar_permissions_view_button_press_event), view);
  g_signal_connect (G_OBJECT (view->tree_view), "popup-menu", G_CALLBACK (thunar_permissions_view_popup_menu), view);
  g_signal_connect (G_OBJECT (view->tree_view), "row-activated", G_CALLBACK (thunar_permissions_view_row_activated), view);
  gtk_container_add (GTK_CONTAINER (scrolled_window), view->tree_view);
  gtk_widget_show (view->tree_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view->tree_view), column);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_add_attribute (column, renderer, "stock-id", THUNAR_PERMISSIONS_MODEL_COLUMN_ICON);
  gtk_tree_view_column_add_attribute (column, renderer, "visible", THUNAR_PERMISSIONS_MODEL_COLUMN_ICON_VISIBLE);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (thunar_permissions_view_toggled), view);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_add_attribute (column, renderer, "active", THUNAR_PERMISSIONS_MODEL_COLUMN_STATE);
  gtk_tree_view_column_add_attribute (column, renderer, "activatable", THUNAR_PERMISSIONS_MODEL_COLUMN_MUTABLE);
  gtk_tree_view_column_add_attribute (column, renderer, "visible", THUNAR_PERMISSIONS_MODEL_COLUMN_STATE_VISIBLE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer, "text", THUNAR_PERMISSIONS_MODEL_COLUMN_TITLE);

  /* setup the permissions model */
  model = thunar_permissions_model_new ();
  exo_binding_new (G_OBJECT (view), "file", G_OBJECT (model), "file");
  gtk_tree_view_set_model (GTK_TREE_VIEW (view->tree_view), model);
  g_object_unref (G_OBJECT (model));

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (view), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("For special permissions and for\nadvanced settings, click Advanced."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  align = gtk_alignment_new (0.5f, 0.5f, 0.0f, 0.0f);
  gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
  gtk_widget_show (align);

  button = gtk_button_new_with_mnemonic (_("_Advanced..."));
  gtk_tooltips_set_tip (view->tooltips, button, _("Click here for special permissions and for advanced settings."), NULL);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_permissions_view_advanced_clicked), view);
  gtk_container_add (GTK_CONTAINER (align), button);
  gtk_widget_show (button);

  separator = gtk_hseparator_new ();
  exo_binding_new_with_negation (G_OBJECT (model), "mutable", G_OBJECT (separator), "visible");
  gtk_box_pack_start (GTK_BOX (view), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  hbox = gtk_hbox_new (FALSE, 6);
  exo_binding_new_with_negation (G_OBJECT (model), "mutable", G_OBJECT (hbox), "visible");
  gtk_box_pack_start (GTK_BOX (view), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DND);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (_("You are not the owner of this file, so\nyou cannot change these permissions."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
}



static void
thunar_permissions_view_finalize (GObject *object)
{
  ThunarPermissionsView *view = THUNAR_PERMISSIONS_VIEW (object);

  /* release the file */
  thunar_permissions_view_set_file (view, NULL);

  /* release the tooltips */
  g_object_unref (G_OBJECT (view->tooltips));

  (*G_OBJECT_CLASS (thunar_permissions_view_parent_class)->finalize) (object);
}



static void
thunar_permissions_view_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ThunarPermissionsView *view = THUNAR_PERMISSIONS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_permissions_view_get_file (view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_permissions_view_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ThunarPermissionsView *view = THUNAR_PERMISSIONS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_permissions_view_set_file (view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_permissions_view_context_menu (ThunarPermissionsView *view,
                                      GtkTreeIter           *iter,
                                      guint                  button,
                                      guint32                time)
{
  GtkTreeModel *model;
  GMainLoop    *loop;
  GtkWidget    *item;
  GtkWidget    *menu;
  GList        *actions;
  GList        *lp;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view), FALSE);

  /* determine the model for the tree view */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view->tree_view));
  if (G_UNLIKELY (model == NULL))
    return FALSE;

  /* determine the menu actions for the specified iterator */
  actions = thunar_permissions_model_get_actions (THUNAR_PERMISSIONS_MODEL (model), GTK_WIDGET (view), iter);
  if (G_UNLIKELY (actions == NULL))
    return FALSE;

  /* prepare the internal loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* prepare the popup menu */
  menu = gtk_menu_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (menu));
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (view)));
  g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);

  /* append menu items for all actions */
  for (lp = actions; lp != NULL; lp = lp->next)
    {
      /* create the menu item */
      item = gtk_action_create_menu_item (GTK_ACTION (lp->data));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* release the action reference */
      g_object_unref (G_OBJECT (lp->data));
    }

  /* release the action list */
  g_list_free (actions);

  /* run the internal loop */
  gtk_grab_add (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
  g_main_loop_run (loop);
  gtk_grab_remove (menu);

  /* clean up */
  g_object_unref (G_OBJECT (menu));
  g_main_loop_unref (loop);

  return TRUE;
}



static void
thunar_permissions_view_advanced_clicked (GtkWidget             *button,
                                          ThunarPermissionsView *view)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  /* determine the toplevel widget for the view */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  if (G_UNLIKELY (toplevel == NULL))
    return;

  /* display the advanced permissions dialog */
  dialog = thunar_advanced_permissions_dialog_new ();
  exo_binding_new (G_OBJECT (view), "file", G_OBJECT (dialog), "file");
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  gtk_widget_show (dialog);
}



static gboolean
thunar_permissions_view_button_press_event (GtkWidget             *tree_view,
                                            GdkEventButton        *event,
                                            ThunarPermissionsView *view)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (view->tree_view == tree_view, FALSE);

  /* determine the path for the click */
  if (event->button == 3 && gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
    {
      /* select the specified row */
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);

      /* determine the iterator for the tree path */
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);

      /* popup the context menu */
      return thunar_permissions_view_context_menu (view, &iter, event->button, event->time);
    }

  return FALSE;
}



static gboolean
thunar_permissions_view_popup_menu (GtkWidget             *tree_view,
                                    ThunarPermissionsView *view)
{
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  g_return_val_if_fail (view->tree_view == tree_view, FALSE);

  /* determine the selected item */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    return thunar_permissions_view_context_menu (view, &iter, 0, gtk_get_current_event_time ());

  return FALSE;
}



static void
thunar_permissions_view_row_activated (GtkWidget             *tree_view,
                                       GtkTreePath           *path,
                                       GtkTreeViewColumn     *column,
                                       ThunarPermissionsView *view)
{
  g_return_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (view->tree_view == tree_view);

  if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (tree_view), path))
    gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);
  else
    gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);
}



static void
thunar_permissions_view_toggled (GtkCellRendererToggle *renderer,
                                 const gchar           *path_string,
                                 ThunarPermissionsView *view)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;
  GError       *error = NULL;

  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (renderer));
  g_return_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view));
  g_return_if_fail (path_string != NULL);

  /* determine the path from the string */
  path = gtk_tree_path_new_from_string (path_string);
  if (G_UNLIKELY (path == NULL))
    return;

  /* determine the model and the iter */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view->tree_view));
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      /* try to toggle the flag identified by the path */
      if (!thunar_permissions_model_toggle (THUNAR_PERMISSIONS_MODEL (model), &iter, &error))
        {
          thunar_dialogs_show_error (GTK_WIDGET (view), error, _("Failed to change permissions of `%s'"), thunar_file_get_display_name (view->file));
          g_error_free (error);
        }
    }

  /* cleanup */
  gtk_tree_path_free (path);
}



/**
 * thunar_permissions_view_new:
 *
 * Allocates a new #ThunarPermissionsView instance.
 *
 * Return value: the newly allocated #ThunarPermissionsView instance.
 **/
GtkWidget*
thunar_permissions_view_new (void)
{
  return g_object_new (THUNAR_TYPE_PERMISSIONS_VIEW, NULL);
}



/**
 * thunar_permissions_view_get_file:
 * @view : a #ThunarPermissionsView instance.
 *
 * Returns the #ThunarFile associated with the specified @view.
 *
 * Return value: the #ThunarFile associated with @view.
 **/
ThunarFile*
thunar_permissions_view_get_file (ThunarPermissionsView *view)
{
  g_return_val_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view), NULL);
  return view->file;
}



/**
 * thunar_permissions_view_set_file:
 * @view : a #ThunarPermissionsView instance.
 * @file : a #ThunarFile instance or %NULL.
 *
 * Associates the given @view with the specified @file.
 **/
void
thunar_permissions_view_set_file (ThunarPermissionsView *view,
                                  ThunarFile            *file)
{
  g_return_if_fail (THUNAR_IS_PERMISSIONS_VIEW (view));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (view->file == file))
    return;

  /* disconnect from the previous file */
  if (G_LIKELY (view->file != NULL))
    g_object_unref (G_OBJECT (view->file));

  /* activate the new file */
  view->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    g_object_ref (G_OBJECT (file));

  /* notify listeners */
  g_object_notify (G_OBJECT (view), "file");
}


