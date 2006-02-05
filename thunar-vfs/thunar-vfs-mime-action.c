/* $Id$ */
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

#include <thunar-vfs/thunar-vfs-mime-action-private.h>
#include <thunar-vfs/thunar-vfs-mime-action.h>
#include <thunar-vfs/thunar-vfs-alias.h>



GType
thunar_vfs_mime_action_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsMimeActionClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof (ThunarVfsMimeAction),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_MIME_HANDLER, I_("ThunarVfsMimeAction"), &info, 0);
    }

  return type;
}



/**
 * _thunar_vfs_mime_action_new:
 * @command : the command for the mime action.
 * @name    : the name for the mime action.
 * @icon    : the icon for the mime action or %NULL.
 * @flags   : the #ThunarVfsMimeHandlerFlags for the mime action.
 *
 * Allocates a new #ThunarVfsMimeAction with the given
 * parameters.
 *
 * Return value: the newly allocated #ThunarVfsMimeAction.
 **/
ThunarVfsMimeAction*
_thunar_vfs_mime_action_new (const gchar              *command,
                             const gchar              *name,
                             const gchar              *icon,
                             ThunarVfsMimeHandlerFlags flags)
{
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), NULL);
  g_return_val_if_fail (command != NULL, NULL);

  return g_object_new (THUNAR_VFS_TYPE_MIME_ACTION,
                       "command", command,
                       "flags", flags,
                       "icon", icon,
                       "name", name,
                       NULL);
}



#define __THUNAR_VFS_MIME_ACTION_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
