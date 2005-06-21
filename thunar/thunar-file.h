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

#ifndef __THUNAR_FILE_H__
#define __THUNAR_FILE_H__

#include <thunar-vfs/thunar-vfs.h>

G_BEGIN_DECLS;

/* we define the folder type here, as thunar-file.h will
 * be included by thunar-folder.h in any case.
 */
typedef struct _ThunarFolder ThunarFolder;

typedef struct _ThunarFileClass ThunarFileClass;
typedef struct _ThunarFile      ThunarFile;

#define THUNAR_TYPE_FILE            (thunar_file_get_type ())
#define THUNAR_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FILE, ThunarFile))
#define THUNAR_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FILE, ThunarFileClass))
#define THUNAR_IS_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FILE))
#define THUNAR_IS_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FILE))
#define THUNAR_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FILE, ThunarFileClass))

struct _ThunarFileClass
{
  GtkObjectClass __parent__;

  /* virtual methods */
  ThunarFolder        *(*open_as_folder)      (ThunarFile   *file,
                                               GError      **error);

  ThunarVfsURI        *(*get_uri)             (ThunarFile   *file);

  ExoMimeInfo         *(*get_mime_info)       (ThunarFile   *file);

  const gchar         *(*get_display_name)    (ThunarFile   *file);
  const gchar         *(*get_special_name)    (ThunarFile   *file);

  ThunarVfsFileType    (*get_kind)            (ThunarFile   *file);
  ThunarVfsFileMode    (*get_mode)            (ThunarFile   *file);
  ThunarVfsFileSize    (*get_size)            (ThunarFile   *file);

  const gchar         *(*get_icon_name)       (ThunarFile   *file,
                                               GtkIconTheme *icon_theme);


  /* signals */
  void (*changed) (ThunarFile *file);
};

struct _ThunarFile
{
  GtkObject __parent__;

  /*< private >*/
  GdkPixbuf *cached_icon;
  gint       cached_size;
};

GType              thunar_file_get_type         (void) G_GNUC_CONST;

ThunarFile        *thunar_file_get_for_uri      (ThunarVfsURI *uri,
                                                 GError      **error);
ThunarFile        *thunar_file_get_parent       (ThunarFile   *file,
                                                 GError      **error);

ThunarFolder      *thunar_file_open_as_folder   (ThunarFile   *file,
                                                 GError      **error);

ThunarVfsURI      *thunar_file_get_uri          (ThunarFile   *file);

ExoMimeInfo       *thunar_file_get_mime_info    (ThunarFile   *file);

const gchar       *thunar_file_get_display_name (ThunarFile   *file);
const gchar       *thunar_file_get_special_name (ThunarFile   *file);

ThunarVfsFileType  thunar_file_get_kind         (ThunarFile   *file);
ThunarVfsFileMode  thunar_file_get_mode         (ThunarFile   *file);
ThunarVfsFileSize  thunar_file_get_size         (ThunarFile   *file);

gchar             *thunar_file_get_mode_string  (ThunarFile   *file);
gchar             *thunar_file_get_size_string  (ThunarFile   *file);

GdkPixbuf         *thunar_file_load_icon        (ThunarFile   *file,
                                                 gint          size);

void               thunar_file_changed          (ThunarFile   *file);

gboolean           thunar_file_is_hidden        (ThunarFile   *file);

#define thunar_file_get_name(file)      (thunar_vfs_uri_get_name (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))

#define thunar_file_is_directory(file)  (THUNAR_FILE_GET_CLASS ((file))->get_kind ((file)) == THUNAR_VFS_FILE_TYPE_DIRECTORY)
#define thunar_file_is_home(file)       (thunar_vfs_uri_is_home (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))
#define thunar_file_is_root(file)       (thunar_vfs_uri_is_root (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))

G_END_DECLS;

#endif /* !__THUNAR_FILE_H__ */
