/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * _thunar_vfs_g_type_register_simple:
 * @type_parent      : the parent #GType.
 * @type_name_static : the name of the new type, allocated on static storage,
 *                     must not change during the lifetime of the process.
 * @class_size       : the size of the class structure in bytes.
 * @class_init       : the class initialization function or %NULL.
 * @instance_size    : the size of the instance structure in bytes.
 * @instance_init    : the constructor function or %NULL.
 * @flags            : the #GTypeFlags or %0.
 *
 * Simple wrapper for g_type_register_static(), used to reduce the
 * number of relocations required for the various #GTypeInfo<!---->s.
 *
 * Return value: the newly registered #GType.
 **/
GType
_thunar_vfs_g_type_register_simple (GType        type_parent,
                                    const gchar *type_name_static,
                                    guint        class_size,
                                    gpointer     class_init,
                                    guint        instance_size,
                                    gpointer     instance_init,
                                    GTypeFlags   flags)
{
  /* setup the type info on the stack */
  GTypeInfo info =
  {
    class_size,
    NULL,
    NULL,
    (GClassInitFunc) class_init,
    NULL,
    NULL,
    instance_size,
    0,
    (GInstanceInitFunc) instance_init,
    NULL,
  };

  /* register the new type */
  return g_type_register_static (type_parent, I_(type_name_static), &info, 0);
}



#define __THUNAR_VFS_PRIVATE_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
