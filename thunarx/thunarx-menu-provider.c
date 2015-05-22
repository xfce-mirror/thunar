/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunarx/thunarx-menu-provider.h>
#include <thunarx/thunarx-private.h>



GType
thunarx_menu_provider_get_type (void)
{
  static volatile gsize type__volatile = 0;
  GType                 type;

  if (g_once_init_enter (&type__volatile))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarxMenuProvider"),
                                            sizeof (ThunarxMenuProviderIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&type__volatile, type);
    }

  return type__volatile;
}



/**
 * thunarx_menu_provider_get_file_actions:
 * @provider : a #ThunarxMenuProvider.
 * @window   : the #GtkWindow within which the actions will be used.
 * @files    : the list of #ThunarxFileInfo<!---->s to which the actions will be applied.
 *
 * Returns the list of #GtkAction<!---->s that @provider has to offer for
 * @files.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #GtkAction object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for atleast the time that the actions are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of actions using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkAction<!---->s that @provider has to offer
 *               for @files.
 **/
GList*
thunarx_menu_provider_get_file_actions (ThunarxMenuProvider *provider,
                                        GtkWidget           *window,
                                        GList               *files)
{
  GList *actions;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_actions != NULL)
    {
      /* query the actions from the implementation */
      actions = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_actions) (provider, window, files);

      /* take a reference on the provider for each action */
      thunarx_object_list_take_reference (actions, provider);
    }
  else
    {
      actions = NULL;
    }

  return actions;
}



/**
 * thunarx_menu_provider_get_folder_actions:
 * @provider : a #ThunarxMenuProvider.
 * @window   : the #GtkWindow within which the actions will be used.
 * @folder   : the folder to which the actions should will be applied.
 *
 * Returns the list of #GtkAction<!---->s that @provider has to offer for
 * @folder.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #GtkAction object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for atleast the time that the actions are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of actions using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkAction<!---->s that @provider has to offer
 *               for @folder.
 **/
GList*
thunarx_menu_provider_get_folder_actions (ThunarxMenuProvider *provider,
                                          GtkWidget           *window,
                                          ThunarxFileInfo     *folder)
{
  GList *actions;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (THUNARX_IS_FILE_INFO (folder), NULL);
  g_return_val_if_fail (thunarx_file_info_is_directory (folder), NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_actions != NULL)
    {
      /* query the actions from the implementation */
      actions = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_actions) (provider, window, folder);

      /* take a reference on the provider for each action */
      thunarx_object_list_take_reference (actions, provider);
    }
  else
    {
      actions = NULL;
    }

  return actions;
}



/**
 * thunarx_menu_provider_get_dnd_actions:
 * @provider : a #ThunarxMenuProvider.
 * @window   : the #GtkWindow within which the actions will be used.
 * @folder   : the folder into which the @files are being dropped
 * @files    : the list of #ThunarxFileInfo<!---->s for the files that are 
 *             being dropped to @folder in @window.
 *
 * Returns the list of #GtkAction<!---->s that @provider has to offer for
 * dropping the @files into the @folder. For example, the thunar-archive-plugin
 * provides <guilabel>Extract Here</guilabel> actions when dropping archive
 * files into a folder that is writable by the user.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #GtkAction object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for atleast the time that the actions are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of actions using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkAction<!---->s that @provider has to offer
 *               for dropping @files to @folder.
 *
 * Since: 0.4.1
 **/
GList*
thunarx_menu_provider_get_dnd_actions (ThunarxMenuProvider *provider,
                                       GtkWidget           *window,
                                       ThunarxFileInfo     *folder,
                                       GList               *files)
{
  GList *actions;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (THUNARX_IS_FILE_INFO (folder), NULL);
  g_return_val_if_fail (thunarx_file_info_is_directory (folder), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_dnd_actions != NULL)
    {
      /* query the actions from the implementation */
      actions = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_dnd_actions) (provider, window, folder, files);

      /* take a reference on the provider for each action */
      thunarx_object_list_take_reference (actions, provider);
    }
  else
    {
      actions = NULL;
    }

  return actions;
}
