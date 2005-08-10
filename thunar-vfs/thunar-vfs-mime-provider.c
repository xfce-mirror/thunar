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

#include <thunar-vfs/thunar-vfs-mime-provider.h>



static void thunar_vfs_mime_provider_register_type (GType                      *type);
static void thunar_vfs_mime_provider_class_init    (ThunarVfsMimeProviderClass *klass);



GType
thunar_vfs_mime_provider_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_mime_provider_register_type, &type);

  return type;
}



static void
thunar_vfs_mime_provider_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsMimeProviderClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_mime_provider_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsMimeProvider),
    0,
    NULL,
    NULL,
  };

  *type = g_type_register_static (EXO_TYPE_OBJECT, "ThunarVfsMimeProvider", &info, G_TYPE_FLAG_ABSTRACT);
}



static void
thunar_vfs_mime_provider_class_init (ThunarVfsMimeProviderClass *klass)
{
  /* We install noops for every virtual method here,
   * so derived classes don't need to implement
   * each and every method if desired.
   */
  klass->lookup_data = (gpointer) exo_noop_null;
  klass->lookup_literal = (gpointer) exo_noop_null;
  klass->lookup_suffix = (gpointer) exo_noop_null;
  klass->lookup_glob = (gpointer) exo_noop_null;
  klass->lookup_alias = (gpointer) exo_noop_null;
  klass->lookup_parents = (gpointer) exo_noop_zero;
  klass->get_stop_characters = (gpointer) exo_noop_null;
  klass->get_max_buffer_extents = (gpointer) exo_noop_zero;
}

