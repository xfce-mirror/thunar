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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_INFO_PROVIDER_H__
#define __THUNARX_INFO_PROVIDER_H__

#include "thunarx/thunarx-file-info.h"

G_BEGIN_DECLS

typedef struct _ThunarxInfoProviderIface ThunarxInfoProviderIface;
typedef struct _ThunarxInfoProvider      ThunarxInfoProvider;

#define THUNARX_TYPE_INFO_PROVIDER (thunarx_info_provider_get_type ())
#define THUNARX_INFO_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_INFO_PROVIDER, ThunarxInfoProvider))
#define THUNARX_IS_INFO_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_INFO_PROVIDER))
#define THUNARX_INFO_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNARX_TYPE_INFO_PROVIDER, ThunarxInfoProviderIface))

/**
 * ThunarxInfoProviderIface:
 * @get_infos: see thunarx_info_provider_get_infos().
 *
 * Interface used by extensions that provides signals on creation/destruction of ThunarxFileInfo objects
 */

struct _ThunarxInfoProviderIface
{
  /*< private >*/
  GTypeInterface __parent__;

  /* signals */
  void (*file_created) (ThunarxInfoProvider *provider, GHashTable *files);
  void (*file_destroyed) (ThunarxInfoProvider *provider, GHashTable *files);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
};

GType
thunarx_info_provider_get_type (void) G_GNUC_CONST;

void
thunarx_info_provider_notify_file_creation (ThunarxInfoProvider *provider, GHashTable *files);
void
thunarx_info_provider_notify_file_destruction (ThunarxInfoProvider *provider, GHashTable *files);

G_END_DECLS

#endif /* !__THUNARX_INFO_PROVIDER_H__ */
