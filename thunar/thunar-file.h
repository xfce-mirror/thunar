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
#include <thunar/thunar-icon-factory.h>

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

/**
 * ThunarFileDateType:
 * @THUNAR_FILE_DATE_ACCESSED : date of last access to the file.
 * @THUNAR_FILE_DATE_CHANGED  : date of last change to the file meta data or the content.
 * @THUNAR_FILE_DATE_MODIFIED : date of last modification of the file's content.
 *
 * The various dates that can be queried about a #ThunarFile. Note, that not all
 * #ThunarFile implementations support all types listed above. See the documentation
 * of the #thunar_file_get_date() method for details.
 **/
typedef enum
{
  THUNAR_FILE_DATE_ACCESSED,
  THUNAR_FILE_DATE_CHANGED,
  THUNAR_FILE_DATE_MODIFIED,
} ThunarFileDateType;

/**
 * ThunarFileIconState:
 * @THUNAR_FILE_ICON_STATE_DEFAULT : the default icon for the file.
 * @THUNAR_FILE_ICON_STATE_DROP    : the drop accept icon for the file.
 *
 * The various file icon states that are used within the file manager
 * views.
 **/
typedef enum
{
  THUNAR_FILE_ICON_STATE_DEFAULT,
  THUNAR_FILE_ICON_STATE_DROP,
} ThunarFileIconState;

#define THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK "emblem-symbolic-link"
#define THUNAR_FILE_EMBLEM_NAME_CANT_READ "emblem-noread"
#define THUNAR_FILE_EMBLEM_NAME_CANT_WRITE "emblem-nowrite"
#define THUNAR_FILE_EMBLEM_NAME_DESKTOP "emblem-desktop"

struct _ThunarFileClass
{
  GObjectClass __parent__;

  /* virtual methods */
  gboolean             (*has_parent)          (ThunarFile             *file);
  ThunarFile          *(*get_parent)          (ThunarFile             *file,
                                               GError                **error);

  gboolean             (*execute)             (ThunarFile             *file,
                                               GdkScreen              *screen,
                                               GList                  *uris,
                                               GError                **error);

  gboolean             (*rename)              (ThunarFile             *file,
                                               const gchar            *name,
                                               GError                **error);

  ThunarFolder        *(*open_as_folder)      (ThunarFile             *file,
                                               GError                **error);

  GdkDragAction        (*accepts_uri_drop)    (ThunarFile             *file,
                                               const ThunarVfsURI     *uri,
                                               GdkDragAction           actions);

  ThunarVfsURI        *(*get_uri)             (ThunarFile             *file);

  ThunarVfsMimeInfo   *(*get_mime_info)       (ThunarFile             *file);

  const gchar         *(*get_display_name)    (ThunarFile             *file);
  const gchar         *(*get_special_name)    (ThunarFile             *file);

  gboolean             (*get_date)            (ThunarFile             *file,
                                               ThunarFileDateType      date_type,
                                               ThunarVfsFileTime      *date_return);
  ThunarVfsFileType    (*get_kind)            (ThunarFile             *file);
  ThunarVfsFileMode    (*get_mode)            (ThunarFile             *file);
  gboolean             (*get_size)            (ThunarFile             *file,
                                               ThunarVfsFileSize      *size_return);

  ThunarVfsVolume     *(*get_volume)          (ThunarFile             *file,
                                               ThunarVfsVolumeManager *volume_manager);

  ThunarVfsGroup      *(*get_group)           (ThunarFile             *file);
  ThunarVfsUser       *(*get_user)            (ThunarFile             *file);

  gboolean             (*is_executable)       (ThunarFile             *file);
  gboolean             (*is_readable)         (ThunarFile             *file);
  gboolean             (*is_renameable)       (ThunarFile             *file);
  gboolean             (*is_writable)         (ThunarFile             *file);

  GList               *(*get_emblem_names)    (ThunarFile             *file);
  const gchar         *(*get_icon_name)       (ThunarFile             *file,
                                               ThunarFileIconState     icon_state,
                                               GtkIconTheme           *icon_theme);

  void                 (*watch)               (ThunarFile             *file);
  void                 (*unwatch)             (ThunarFile             *file);

  void                 (*reload)              (ThunarFile             *file);


  /* signals */
  void (*changed) (ThunarFile *file);
  void (*destroy) (ThunarFile *file);
};

struct _ThunarFile
{
  GObject __parent__;

  /*< private >*/
  guint flags;
};

GType              thunar_file_get_type         (void) G_GNUC_CONST;

ThunarFile        *_thunar_file_cache_lookup    (ThunarVfsURI           *uri);
void               _thunar_file_cache_insert    (ThunarFile             *file);
void               _thunar_file_cache_rename    (ThunarFile             *file,
                                                 ThunarVfsURI           *uri);

ThunarFile        *thunar_file_get_for_uri      (ThunarVfsURI           *uri,
                                                 GError                **error);

gboolean           thunar_file_has_parent       (ThunarFile             *file);
ThunarFile        *thunar_file_get_parent       (ThunarFile             *file,
                                                 GError                **error);

gboolean           thunar_file_execute          (ThunarFile             *file,
                                                 GdkScreen              *screen,
                                                 GList                  *uris,
                                                 GError                **error);

gboolean           thunar_file_rename           (ThunarFile             *file,
                                                 const gchar            *name,
                                                 GError                **error);

ThunarFolder      *thunar_file_open_as_folder   (ThunarFile             *file,
                                                 GError                **error);

GdkDragAction      thunar_file_accepts_uri_drop (ThunarFile             *file,
                                                 GList                  *uri_list,
                                                 GdkDragAction           actions);

ThunarVfsURI      *thunar_file_get_uri          (ThunarFile             *file);

ThunarVfsMimeInfo *thunar_file_get_mime_info    (ThunarFile             *file);

const gchar       *thunar_file_get_display_name (ThunarFile             *file);
const gchar       *thunar_file_get_special_name (ThunarFile             *file);

gboolean           thunar_file_get_date         (ThunarFile             *file,
                                                 ThunarFileDateType      date_type,
                                                 ThunarVfsFileTime      *date_return);
ThunarVfsFileType  thunar_file_get_kind         (ThunarFile             *file);
ThunarVfsFileMode  thunar_file_get_mode         (ThunarFile             *file);
gboolean           thunar_file_get_size         (ThunarFile             *file,
                                                 ThunarVfsFileSize      *size_return);

gchar             *thunar_file_get_date_string  (ThunarFile             *file,
                                                 ThunarFileDateType      date_type);
gchar             *thunar_file_get_mode_string  (ThunarFile             *file);
gchar             *thunar_file_get_size_string  (ThunarFile             *file);

ThunarVfsVolume   *thunar_file_get_volume       (ThunarFile             *file,
                                                 ThunarVfsVolumeManager *volume_manager);

ThunarVfsGroup    *thunar_file_get_group        (ThunarFile             *file);
ThunarVfsUser     *thunar_file_get_user         (ThunarFile             *file);

gboolean          thunar_file_is_executable     (ThunarFile             *file);
gboolean          thunar_file_is_readable       (ThunarFile             *file);
gboolean          thunar_file_is_renameable     (ThunarFile             *file);
gboolean          thunar_file_is_writable       (ThunarFile             *file);

GList             *thunar_file_get_emblem_names (ThunarFile             *file);
GdkPixbuf         *thunar_file_load_icon        (ThunarFile             *file,
                                                 ThunarFileIconState     icon_state,
                                                 ThunarIconFactory      *icon_factory,
                                                 gint                    size);

void               thunar_file_watch            (ThunarFile             *file);
void               thunar_file_unwatch          (ThunarFile             *file);

void               thunar_file_reload           (ThunarFile             *file);

void               thunar_file_changed          (ThunarFile             *file);
void               thunar_file_destroy          (ThunarFile             *file);

gboolean           thunar_file_is_hidden        (ThunarFile             *file);

#define thunar_file_get_name(file)      (thunar_vfs_uri_get_name (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))

#define thunar_file_is_directory(file)  (THUNAR_FILE_GET_CLASS ((file))->get_kind ((file)) == THUNAR_VFS_FILE_TYPE_DIRECTORY)
#define thunar_file_is_home(file)       (thunar_vfs_uri_is_home (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))
#define thunar_file_is_root(file)       (thunar_vfs_uri_is_root (THUNAR_FILE_GET_CLASS ((file))->get_uri ((file))))


GList             *thunar_file_list_copy        (const GList            *file_list);
void               thunar_file_list_free        (GList                  *file_list);

G_END_DECLS;

#endif /* !__THUNAR_FILE_H__ */
