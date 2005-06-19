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

#ifndef __THUNAR_VFS_TRASH_H__
#define __THUNAR_VFS_TRASH_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsTrashInfo ThunarVfsTrashInfo;

#define THUNAR_VFS_TRASH_INFO (thunar_vfs_trash_info_get_type ())

GType               thunar_vfs_trash_info_get_type          (void) G_GNUC_CONST;

ThunarVfsTrashInfo *thunar_vfs_trash_info_copy              (const ThunarVfsTrashInfo *info);
void                thunar_vfs_trash_info_free              (ThunarVfsTrashInfo       *info);

const gchar        *thunar_vfs_trash_info_get_path          (const ThunarVfsTrashInfo *info);
const gchar        *thunar_vfs_trash_info_get_deletion_date (const ThunarVfsTrashInfo *info);


typedef struct _ThunarVfsTrashClass ThunarVfsTrashClass;
typedef struct _ThunarVfsTrash      ThunarVfsTrash;

#define THUNAR_VFS_TYPE_TRASH             (thunar_vfs_trash_get_type ())
#define THUNAR_VFS_TRASH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_TRASH, ThunarVfsTrash))
#define THUNAR_VFS_TRASH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_TRASH, ThunarVfsTrashClass))
#define THUNAR_VFS_IS_TRASH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_TRASH))
#define THUNAR_VFS_IS_TRASH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_TRASH))
#define THUNAR_VFS_TRASH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_TRASH, ThunarVfsTrashClass))

GType               thunar_vfs_trash_get_type   (void) G_GNUC_CONST;

gint                thunar_vfs_trash_get_id     (ThunarVfsTrash *trash);
GList              *thunar_vfs_trash_get_files  (ThunarVfsTrash *trash);
ThunarVfsTrashInfo *thunar_vfs_trash_get_info   (ThunarVfsTrash *trash,
                                                 const gchar    *file);


typedef struct _ThunarVfsTrashManagerClass ThunarVfsTrashManagerClass;
typedef struct _ThunarVfsTrashManager      ThunarVfsTrashManager;

#define THUNAR_VFS_TYPE_TRASH_MANAGER             (thunar_vfs_trash_manager_get_type ())
#define THUNAR_VFS_TRASH_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_TRASH_MANAGER, ThunarVfsTrashManager))
#define THUNAR_VFS_TRASH_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_TRASH_MANAGER, ThunarVfsTrashManagerClass))
#define THUNAR_VFS_IS_TRASH_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_TRASH_MANAGER))
#define THUNAR_VFS_IS_TRASH_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_TRASH_MANAGER))
#define THUNAR_VFS_TRASH_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_TRASH_MANAGER, ThunarVfsTrashManagerClass))

GType                  thunar_vfs_trash_manager_get_type    (void) G_GNUC_CONST;

ThunarVfsTrashManager *thunar_vfs_trash_manager_get_default (void);

gboolean               thunar_vfs_trash_manager_is_empty    (ThunarVfsTrashManager *manager);

GList                 *thunar_vfs_trash_manager_get_trashes (ThunarVfsTrashManager *manager);

G_END_DECLS;

#endif /* !__THUNAR_VFS_TRASH_H__ */
