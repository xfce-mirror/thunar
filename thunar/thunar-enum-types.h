/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_ENUM_TYPES_H__
#define __THUNAR_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS;

#define THUNAR_TYPE_RECURSIVE_PERMISSIONS (thunar_recursive_permissions_get_type ())

/**
 * ThunarRecursivePermissionsMode:
 * @THUNAR_RECURSIVE_PERMISSIONS_ASK    : ask the user everytime permissions are changed.
 * @THUNAR_RECURSIVE_PERMISSIONS_ALWAYS : always apply the change recursively.
 * @THUNAR_RECURSIVE_PERMISSIONS_NEVER  : never apply the change recursively.
 *
 * Modus operandi when changing permissions.
 **/
typedef enum
{
  THUNAR_RECURSIVE_PERMISSIONS_ASK,
  THUNAR_RECURSIVE_PERMISSIONS_ALWAYS,
  THUNAR_RECURSIVE_PERMISSIONS_NEVER,
} ThunarRecursivePermissionsMode;

GType thunar_recursive_permissions_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_ENUM_TYPES_H__ */
