/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-view.h>



static void thunar_view_class_init  (gpointer klass);



GType
thunar_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarViewIface),
        NULL,
        NULL,
        (GClassInitFunc) thunar_view_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE, I_("ThunarView"), &info, 0);
      g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
      g_type_interface_add_prerequisite (type, THUNAR_TYPE_NAVIGATOR);
    }

  return type;
}



static void
thunar_view_class_init (gpointer klass)
{
  /**
   * ThunarView:loading:
   *
   * Indicates whether the given #ThunarView is currently loading or
   * layouting its contents. Implementations should invoke
   * #g_object_notify() on this property whenever they start to load
   * the contents and then once they have finished loading.
   *
   * Other modules can use this property to display some kind of
   * user visible notification about the loading state, e.g. a
   * progress bar or an animated image.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("loading",
                                                             _("Loading"),
                                                             _("Whether the view is currently being loaded"),
                                                             FALSE,
                                                             EXO_PARAM_READABLE));

  /**
   * ThunarView:statusbar-text:
   *
   * The text to be displayed in the status bar, which is associated
   * with this #ThunarView instance. Implementations should invoke
   * #g_object_notify() on this property, whenever they have a new
   * text to be display in the status bar (e.g. the selection changed
   * or similar).
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_string ("statusbar-text",
                                                            _("Statusbar text"),
                                                            _("Text to be displayed in the statusbar associated with this view"),
                                                            NULL,
                                                            EXO_PARAM_READABLE));

  /**
   * ThunarView:show-hidden:
   *
   * Tells whether to display hidden and backup files in the
   * #ThunarView or whether to hide them.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("show-hidden",
                                                             _("Show hidden"),
                                                             _("Whether to display hidden files"),
                                                             FALSE,
                                                             EXO_PARAM_READWRITE));

  /**
   * ThunarView:ui-manager:
   *
   * The UI manager used by the surrounding #ThunarWindow. The
   * #ThunarView implementations may connect additional actions
   * to the UI manager.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("ui-manager",
                                                            _("UI manager"),
                                                            _("UI manager of the surrounding window"),
                                                            GTK_TYPE_UI_MANAGER,
                                                            EXO_PARAM_READWRITE));
}



/**
 * thunar_view_get_loading:
 * @view : a #ThunarView instance.
 *
 * Tells whether the given #ThunarView is currently loading or
 * layouting its contents.
 *
 * Return value: %TRUE if @view is currently being loaded, else %FALSE.
 **/
gboolean
thunar_view_get_loading (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_loading) (view);
}



/**
 * thunar_view_get_statusbar_text:
 * @view : a #ThunarView instance.
 *
 * Queries the text that should be displayed in the status bar
 * associated with @view.
 *
 * Return value: the text to be displayed in the status bar
 *               asssociated with @view.
 **/
const gchar*
thunar_view_get_statusbar_text (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_statusbar_text) (view);
}



/**
 * thunar_view_get_show_hidden:
 * @view : a #ThunarView instance.
 *
 * Returns %TRUE if hidden and backup files are shown
 * in @view. Else %FALSE is returned.
 *
 * Return value: whether @view displays hidden files.
 **/
gboolean
thunar_view_get_show_hidden (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), FALSE);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_show_hidden) (view);
}



/**
 * thunar_view_set_show_hidden:
 * @view        : a #ThunarView instance.
 * @show_hidden : &TRUE to display hidden files, else %FALSE.
 *
 * If @show_hidden is %TRUE then @view will display hidden and
 * backup files, else those files will be hidden from the user
 * interface.
 **/
void
thunar_view_set_show_hidden (ThunarView *view,
                             gboolean    show_hidden)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));
  (*THUNAR_VIEW_GET_IFACE (view)->set_show_hidden) (view, show_hidden);
}



/**
 * thunar_view_get_ui_manager:
 * @view : a #ThunarView instance.
 *
 * Returns the #GtkUIManager associated with @view or
 * %NULL if @view has no #GtkUIManager associated with
 * it.
 *
 * Return value: the #GtkUIManager associated with @view
 *               or %NULL.
 **/
GtkUIManager*
thunar_view_get_ui_manager (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return (*THUNAR_VIEW_GET_IFACE (view)->get_ui_manager) (view);
}



/**
 * thunar_view_set_ui_manager:
 * @view       : a #ThunarView instance.
 * @ui_manager : a #GtkUIManager or %NULL.
 *
 * Installs a new #GtkUIManager for @view or resets the ::ui-manager
 * property.
 *
 * Implementations of the #ThunarView interface must first disconnect
 * from any previously set #GtkUIManager and then connect to the
 * @ui_manager if not %NULL.
 **/
void
thunar_view_set_ui_manager (ThunarView   *view,
                            GtkUIManager *ui_manager)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (ui_manager == NULL || GTK_IS_UI_MANAGER (ui_manager));
  (*THUNAR_VIEW_GET_IFACE (view)->set_ui_manager) (view, ui_manager);
}



