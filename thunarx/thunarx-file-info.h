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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_FILE_INFO_H__
#define __THUNARX_FILE_INFO_H__

#include <glib-object.h>

G_BEGIN_DECLS;

/* Used to avoid a dependency of thunarx on thunar-vfs */
#ifndef __THUNAR_VFS_INFO_DEFINED__
#define __THUNAR_VFS_INFO_DEFINED__
typedef struct _ThunarVfsInfo ThunarVfsInfo;
#endif

typedef struct _ThunarxFileInfoIface ThunarxFileInfoIface;
typedef struct _ThunarxFileInfo      ThunarxFileInfo;

#define THUNARX_TYPE_FILE_INFO            (thunarx_file_info_get_type ())
#define THUNARX_FILE_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_FILE_INFO, ThunarxFileInfo))
#define THUNARX_IS_FILE_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_FILE_INFO))
#define THUNARX_FILE_INFO_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNARX_TYPE_FILE_INFO, ThunarxFileInfoIface))

struct _ThunarxFileInfoIface
{
  /*< private >*/
  GTypeInterface __parent__;

  /*< public >*/
  gchar         *(*get_name)        (ThunarxFileInfo *file_info);

  gchar         *(*get_uri)         (ThunarxFileInfo *file_info);
  gchar         *(*get_parent_uri)  (ThunarxFileInfo *file_info);
  gchar         *(*get_uri_scheme)  (ThunarxFileInfo *file_info);

  gchar         *(*get_mime_type)   (ThunarxFileInfo *file_info);
  gboolean       (*has_mime_type)   (ThunarxFileInfo *file_info,
                                     const gchar     *mime_type);

  gboolean       (*is_directory)    (ThunarxFileInfo *file_info);

  ThunarVfsInfo *(*get_vfs_info)    (ThunarxFileInfo *file_info);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
  void (*reserved6) (void);
};

GType          thunarx_file_info_get_type       (void) G_GNUC_CONST;

gchar         *thunarx_file_info_get_name       (ThunarxFileInfo *file_info);
gchar         *thunarx_file_info_get_uri        (ThunarxFileInfo *file_info);
gchar         *thunarx_file_info_get_parent_uri (ThunarxFileInfo *file_info);
gchar         *thunarx_file_info_get_uri_scheme (ThunarxFileInfo *file_info);

gchar         *thunarx_file_info_get_mime_type  (ThunarxFileInfo *file_info);
gboolean       thunarx_file_info_has_mime_type  (ThunarxFileInfo *file_info,
                                                 const gchar     *mime_type);

gboolean       thunarx_file_info_is_directory   (ThunarxFileInfo *file_info);

ThunarVfsInfo *thunarx_file_info_get_vfs_info   (ThunarxFileInfo *file_info);

GList         *thunarx_file_info_list_copy      (GList           *file_infos);
void           thunarx_file_info_list_free      (GList           *file_infos);

G_END_DECLS;

#endif /* !__THUNARX_FILE_INFO_H__ */
