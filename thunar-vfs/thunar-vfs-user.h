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

#ifndef __THUNAR_VFS_USER_H__
#define __THUNAR_VFS_USER_H__

#include <thunar-vfs/thunar-vfs-info.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsGroupClass ThunarVfsGroupClass;
typedef struct _ThunarVfsGroup      ThunarVfsGroup;

#define THUNAR_VFS_TYPE_GROUP             (thunar_vfs_group_get_type ())
#define THUNAR_VFS_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_GROUP, ThunarVfsGroup))
#define THUNAR_VFS_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_GROUP, ThunarVfsGroupClass))
#define THUNAR_VFS_IS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_GROUP))
#define THUNAR_VFS_IS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_GROUP))
#define THUNAR_VFS_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_GROUP, ThunarVfsGroupClass))

GType            thunar_vfs_group_get_type  (void) G_GNUC_CONST;

ThunarVfsGroupId thunar_vfs_group_get_id    (ThunarVfsGroup *group);
const gchar     *thunar_vfs_group_get_name  (ThunarVfsGroup *group);


typedef struct _ThunarVfsUserClass ThunarVfsUserClass;
typedef struct _ThunarVfsUser      ThunarVfsUser;

#define THUNAR_VFS_TYPE_USER            (thunar_vfs_user_get_type ())
#define THUNAR_VFS_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_USER, ThunarVfsUser))
#define THUNAR_VFS_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_USER, ThunarVfsUserClass))
#define THUNAR_VFS_IS_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_USER))
#define THUNAR_VFS_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_USER))
#define THUNAR_VFS_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_USER, ThunarVfsUserClass))

GType            thunar_vfs_user_get_type          (void) G_GNUC_CONST;

GList           *thunar_vfs_user_get_groups        (ThunarVfsUser *user);
ThunarVfsGroup  *thunar_vfs_user_get_primary_group (ThunarVfsUser *user);
ThunarVfsUserId  thunar_vfs_user_get_id            (ThunarVfsUser *user);
const gchar     *thunar_vfs_user_get_name          (ThunarVfsUser *user);
const gchar     *thunar_vfs_user_get_real_name     (ThunarVfsUser *user);
gboolean         thunar_vfs_user_is_me             (ThunarVfsUser *user);


typedef struct _ThunarVfsUserManagerClass ThunarVfsUserManagerClass;
typedef struct _ThunarVfsUserManager      ThunarVfsUserManager;

#define THUNAR_VFS_TYPE_USER_MANAGER            (thunar_vfs_user_manager_get_type ())
#define THUNAR_VFS_USER_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_USER_MANAGER, ThunarVfsUserManager))
#define THUNAR_VFS_USER_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_USER_MANAGER, ThunarVfsUserManagerClass))
#define THUNAR_VFS_IS_USER_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_USER_MANAGER))
#define THUNAR_VFS_IS_USER_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_USER_MANAGER))
#define THUNAR_VFS_USER_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_USER_MANAGER, ThunarVfsUserManagerClass))

GType                 thunar_vfs_user_manager_get_type        (void) G_GNUC_CONST;

ThunarVfsUserManager *thunar_vfs_user_manager_get_default     (void);

ThunarVfsGroup       *thunar_vfs_user_manager_get_group_by_id (ThunarVfsUserManager *manager,
                                                               ThunarVfsGroupId      id);
ThunarVfsUser        *thunar_vfs_user_manager_get_user_by_id  (ThunarVfsUserManager *manager,
                                                               ThunarVfsUserId       id);

GList                *thunar_vfs_user_manager_get_all_groups  (ThunarVfsUserManager *manager) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_VFS_USER_H__ */
