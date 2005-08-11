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

#ifndef __THUNAR_VFS_MIME_LEGACY_H__
#define __THUNAR_VFS_MIME_LEGACY_H__

#include <thunar-vfs/thunar-vfs-mime-provider.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeLegacyClass ThunarVfsMimeLegacyClass;
typedef struct _ThunarVfsMimeLegacy      ThunarVfsMimeLegacy;

#define THUNAR_VFS_TYPE_MIME_LEGACY             (thunar_vfs_mime_legacy_get_type ())
#define THUNAR_VFS_MIME_LEGACY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_LEGACY, ThunarVfsMimeLegacy))
#define THUNAR_VFS_MIME_LEGACY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_LEGACY, ThunarVfsMimeLegacyClass))
#define THUNAR_VFS_IS_MIME_LEGACY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_LEGACY))
#define THUNAR_VFS_IS_MIME_LEGACY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_LEGACY))
#define THUNAR_VFS_MIME_LEGACY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_LEGACY, ThunarVfsMimeLegacyClass))

GType                  thunar_vfs_mime_legacy_get_type (void) G_GNUC_CONST;

ThunarVfsMimeProvider *thunar_vfs_mime_legacy_new      (const gchar *directory) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_LEGACY_H__ */
