/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2026 Alexander Schwinn <alexxcons@xfce.org>
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

#include "thunarx/thunarx-private.h"
#include "thunarx/thunarx-info-provider.h"
#include "thunarx/thunarx-visibility.h"

#include <libxfce4util/libxfce4util.h>

/* Signal identifiers */
enum
{
  FILES_CREATED,
  FILES_DESTROYED,
  LAST_SIGNAL,
};



static guint info_provider_signals[LAST_SIGNAL];

/**
 * SECTION: thunarx-info-provider
 * @short_description: The interface for plugins which want to be signaled on creation/destruction of ThunarxFileInfo objects
 * @title: ThunarxInfoProvider
 * @include: thunarx/thunarx.h
 *
 * The <interface>ThunarxInfoProvider</interface> interface
 * provides signals for creation and destruction of ThunarFiles (ThunarxFileInfo objects)
 */

GType
thunarx_info_provider_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_ ("ThunarxInfoProvider"),
                                            sizeof (ThunarxInfoProviderIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      /**
       * ThunarxInfoProvider::changed:
       * @file_info : a #ThunarxFileInfo.
       *
       * Emitted by Thunar after creation of a new ThunarXFileInfo object.
       **/
      info_provider_signals[FILES_CREATED] =
      g_signal_new (I_ ("file-created"),
                    type,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarxInfoProviderIface, file_created),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

      /**
       * ThunarxInfoProvider::renamed:
       * @file_info : a #ThunarxFileInfo
       *
       * Emitted by Thunar before destruction of each ThunarXFileInfo object.
       **/
      info_provider_signals[FILES_DESTROYED] =
      g_signal_new (I_ ("file-destroyed"),
                    type,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarxInfoProviderIface, file_destroyed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

      g_once_init_leave (&type__static, type);
    }
  return type__static;
}

/**
 * thunarx_info_provider_notify_file_creation:
 * @provider : a #ThunarxInfoProvider.
 *
 * Emits the ::file_created signal on @provider. This method should not
 * be invoked by Thunar plugins, instead the file manager itself
 * will use this method to emit ::file_created whenever it created a new ThunarXFileInfo object
 **/
void
thunarx_info_provider_notify_file_creation (ThunarxInfoProvider *provider, GHashTable *files)
{
  g_return_if_fail (THUNARX_IS_INFO_PROVIDER (provider));
  g_signal_emit (G_OBJECT (provider), info_provider_signals[FILES_CREATED], 0, files);
}

/**
 * thunarx_info_provider_notify_file_destruction:
 * @provider : a #ThunarxInfoProvider.
 *
 * Emits the ::file_destroyed signal on @provider. This method should not
 * be invoked by Thunar plugins, instead the file manager itself
 * will use this method to emit ::file_destroyed whenever it is about to destroys a ThunarXFileInfo object
 **/
void
thunarx_info_provider_notify_file_destruction (ThunarxInfoProvider *provider, GHashTable *files)
{
  g_return_if_fail (THUNARX_IS_INFO_PROVIDER (provider));
  g_signal_emit (G_OBJECT (provider), info_provider_signals[FILES_DESTROYED], 0, files);
}

#define __THUNARX_INFO_PROVIDER_C__
#include "thunarx-visibility.c"
