/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
#include <thunarx/thunarx-alias.h>



static GQuark thunarx_menu_provider_action_quark;



GType
thunarx_menu_provider_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarxMenuProviderIface),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        NULL,
        NULL,
      };

      /* register the menu provider interface */
      type = g_type_register_static (G_TYPE_INTERFACE, I_("ThunarxMenuProvider"), &info, 0);
      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      /* allocate the thunarx-menu-provider-action quark */
      thunarx_menu_provider_action_quark = g_quark_from_static_string ("thunarx-menu-provider-action");
    }

  return type;
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
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
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
  GList *lp;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_actions != NULL)
    {
      /* query the actions and take a reference on the provider for each action */
      actions = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_actions) (provider, window, files);
      for (lp = actions; lp != NULL; lp = lp->next)
        {
          g_object_set_qdata_full (G_OBJECT (lp->data), thunarx_menu_provider_action_quark, provider, g_object_unref);
          g_object_ref (G_OBJECT (provider));
        }
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
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
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
  GList *lp;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (THUNARX_IS_FILE_INFO (folder), NULL);
  g_return_val_if_fail (thunarx_file_info_is_directory (folder), NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_actions != NULL)
    {
      /* query the actions and take a reference on the provider for each action */
      actions = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_actions) (provider, window, folder);
      for (lp = actions; lp != NULL; lp = lp->next)
        {
          g_object_set_qdata_full (G_OBJECT (lp->data), thunarx_menu_provider_action_quark, provider, g_object_unref);
          g_object_ref (G_OBJECT (provider));
        }
    }
  else
    {
      actions = NULL;
    }

  return actions;
}



#define __THUNARX_MENU_PROVIDER_C__
#include <thunarx/thunarx-aliasdef.c>
