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

#ifndef __THUNAR_FOLDER_H__
#define __THUNAR_FOLDER_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarFolderIface ThunarFolderIface;

#define THUNAR_TYPE_FOLDER            (thunar_folder_get_type ())
#define THUNAR_FOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FOLDER, ThunarFolder))
#define THUNAR_IS_FOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FOLDER))
#define THUNAR_FOLDER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_FOLDER, ThunarFolderIface))

struct _ThunarFolderIface
{
  GTypeInterface __parent__;

  /* methods */
  ThunarFile *(*get_corresponding_file) (ThunarFolder *folder);
  GSList     *(*get_files)              (ThunarFolder *folder);

  /* signals */
  void (*files_added) (ThunarFolder *folder,
                       GSList       *files);
};

GType       thunar_folder_get_type               (void) G_GNUC_CONST;

ThunarFile *thunar_folder_get_corresponding_file (ThunarFolder *folder);
GSList     *thunar_folder_get_files              (ThunarFolder *folder);

void        thunar_folder_files_added            (ThunarFolder *folder,
                                                  GSList       *files);

G_END_DECLS;

#endif /* !__THUNAR_FOLDER_H__ */
