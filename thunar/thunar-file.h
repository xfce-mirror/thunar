/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-metafile.h>
#include <thunar/thunar-user.h>
#include <thunarx/thunarx.h>

#include <glib.h>

G_BEGIN_DECLS;

typedef struct _ThunarFileClass ThunarFileClass;
typedef struct _ThunarFile      ThunarFile;

#define THUNAR_TYPE_FILE            (thunar_file_get_type ())
#define THUNAR_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FILE, ThunarFile))
#define THUNAR_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FILE, ThunarFileClass))
#define THUNAR_IS_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FILE))
#define THUNAR_IS_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FILE))
#define THUNAR_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FILE, ThunarFileClass))

/**
 * ThunarFileMode:
 *
 * Special flags and permissions of a filesystem entity.
 **/
typedef enum /*< flags >*/
{
  THUNAR_FILE_MODE_SUID       = 04000,
  THUNAR_FILE_MODE_SGID       = 02000,
  THUNAR_FILE_MODE_STICKY     = 01000,
  THUNAR_FILE_MODE_USR_ALL    = 00700,
  THUNAR_FILE_MODE_USR_READ   = 00400,
  THUNAR_FILE_MODE_USR_WRITE  = 00200,
  THUNAR_FILE_MODE_USR_EXEC   = 00100,
  THUNAR_FILE_MODE_GRP_ALL    = 00070,
  THUNAR_FILE_MODE_GRP_READ   = 00040,
  THUNAR_FILE_MODE_GRP_WRITE  = 00020,
  THUNAR_FILE_MODE_GRP_EXEC   = 00010,
  THUNAR_FILE_MODE_OTH_ALL    = 00007,
  THUNAR_FILE_MODE_OTH_READ   = 00004,
  THUNAR_FILE_MODE_OTH_WRITE  = 00002,
  THUNAR_FILE_MODE_OTH_EXEC   = 00001,
} ThunarFileMode;

/**
 * ThunarFileDateType:
 * @THUNAR_FILE_DATE_ACCESSED : date of last access to the file.
 * @THUNAR_FILE_DATE_CHANGED  : date of last change to the file meta data or the content.
 * @THUNAR_FILE_DATE_MODIFIED : date of last modification of the file's content.
 *
 * The various dates that can be queried about a #ThunarFile. Note, that not all
 * #ThunarFile implementations support all types listed above. See the documentation
 * of the thunar_file_get_date() method for details.
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
 * @THUNAR_FILE_ICON_STATE_OPEN    : the folder is expanded.
 *
 * The various file icon states that are used within the file manager
 * views.
 **/
typedef enum /*< enum >*/
{
  THUNAR_FILE_ICON_STATE_DEFAULT,
  THUNAR_FILE_ICON_STATE_DROP,
  THUNAR_FILE_ICON_STATE_OPEN,
} ThunarFileIconState;

/**
 * ThunarFileThumbState:
 * @THUNAR_FILE_THUMB_STATE_MASK    : the mask to extract the thumbnail state.
 * @THUNAR_FILE_THUMB_STATE_UNKNOWN : unknown whether there's a thumbnail.
 * @THUNAR_FILE_THUMB_STATE_NONE    : no thumbnail is available.
 * @THUNAR_FILE_THUMB_STATE_READY   : a thumbnail is available.
 * @THUNAR_FILE_THUMB_STATE_LOADING : a thumbnail is being generated.
 *
 * The state of the thumbnailing for a given #ThunarFile.
 **/
typedef enum /*< flags >*/
{
  THUNAR_FILE_THUMB_STATE_MASK    = 0x03,
  THUNAR_FILE_THUMB_STATE_UNKNOWN = 0x00,
  THUNAR_FILE_THUMB_STATE_NONE    = 0x01,
  THUNAR_FILE_THUMB_STATE_READY   = 0x02,
  THUNAR_FILE_THUMB_STATE_LOADING = 0x03,
} ThunarFileThumbState;

#define THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK "emblem-symbolic-link"
#define THUNAR_FILE_EMBLEM_NAME_CANT_READ "emblem-noread"
#define THUNAR_FILE_EMBLEM_NAME_CANT_WRITE "emblem-nowrite"
#define THUNAR_FILE_EMBLEM_NAME_DESKTOP "emblem-desktop"

struct _ThunarFileClass
{
  GObjectClass __parent__;

  /* signals */
  void (*destroy) (ThunarFile *file);
};

struct _ThunarFile
{
  GObject __parent__;

  /*< private >*/
  ThunarVfsInfo *info;
  GFileInfo     *ginfo;
  GFileInfo     *filesystem_info;
  GFile         *gfile;
  guint          flags;
};

GType             thunar_file_get_type             (void) G_GNUC_CONST;

ThunarFile       *thunar_file_get                  (GFile                  *file,
                                                    GError                **error);
ThunarFile       *thunar_file_get_for_info         (ThunarVfsInfo          *info);
ThunarFile       *thunar_file_get_for_path         (ThunarVfsPath          *path,
                                                    GError                **error);
ThunarFile       *thunar_file_get_for_uri          (const gchar            *uri,
                                                    GError                **error);

gboolean          thunar_file_load                 (ThunarFile             *file,
                                                    GCancellable           *cancellable,
                                                    GError                **error);

ThunarFile       *thunar_file_get_parent           (const ThunarFile       *file,
                                                    GError                **error);

gboolean          thunar_file_execute              (ThunarFile             *file,
                                                    GdkScreen              *screen,
                                                    GList                  *path_list,
                                                    GError                **error);

gboolean          thunar_file_launch               (ThunarFile             *file,
                                                    gpointer                parent,
                                                    GError                **error);

gboolean          thunar_file_rename               (ThunarFile             *file,
                                                    const gchar            *name,
                                                    GError                **error);

GdkDragAction     thunar_file_accepts_drop         (ThunarFile             *file,
                                                    GList                  *path_list,
                                                    GdkDragContext         *context,
                                                    GdkDragAction          *suggested_action_return);

const gchar      *thunar_file_get_display_name     (const ThunarFile       *file);

ThunarVfsFileTime thunar_file_get_date             (const ThunarFile       *file,
                                                    ThunarFileDateType      date_type);

gchar            *thunar_file_get_date_string      (const ThunarFile       *file,
                                                    ThunarFileDateType      date_type,
                                                    ThunarDateStyle         date_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_mode_string      (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_size_string      (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarVfsVolume  *thunar_file_get_volume           (const ThunarFile       *file,
                                                    ThunarVfsVolumeManager *volume_manager);

ThunarGroup      *thunar_file_get_group            (const ThunarFile       *file);
ThunarUser       *thunar_file_get_user             (const ThunarFile       *file);

const gchar      *thunar_file_get_content_type     (const ThunarFile       *file);
const gchar      *thunar_file_get_symlink_target   (const ThunarFile       *file);
const gchar      *thunar_file_get_basename         (const ThunarFile       *file);
gboolean          thunar_file_is_symlink           (const ThunarFile       *file);
guint64           thunar_file_get_size             (const ThunarFile       *file);
GAppInfo         *thunar_file_get_default_handler  (const ThunarFile       *file);
GFileType         thunar_file_get_kind             (const ThunarFile       *file);
ThunarFileMode    thunar_file_get_mode             (const ThunarFile       *file);
gboolean          thunar_file_get_free_space       (const ThunarFile       *file, 
                                                    guint64                *free_space_return);
gboolean          thunar_file_is_directory         (const ThunarFile       *file);
gboolean          thunar_file_is_local             (const ThunarFile       *file);
gboolean          thunar_file_is_ancestor          (const ThunarFile       *file, 
                                                    const ThunarFile       *ancestor);
gboolean          thunar_file_is_executable        (const ThunarFile       *file);
gboolean          thunar_file_is_readable          (const ThunarFile       *file);
gboolean          thunar_file_is_writable          (const ThunarFile       *file);
gboolean          thunar_file_is_hidden            (const ThunarFile       *file);
gboolean          thunar_file_is_home              (const ThunarFile       *file);
gboolean          thunar_file_is_regular           (const ThunarFile       *file);

gchar            *thunar_file_get_deletion_date    (const ThunarFile       *file,
                                                    ThunarDateStyle         date_style) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar            *thunar_file_get_original_path    (const ThunarFile       *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean          thunar_file_is_chmodable         (const ThunarFile       *file);
gboolean          thunar_file_is_renameable        (const ThunarFile       *file);

GList            *thunar_file_get_emblem_names     (ThunarFile              *file);
void              thunar_file_set_emblem_names     (ThunarFile              *file,
                                                    GList                   *emblem_names);

gboolean          thunar_file_set_custom_icon      (ThunarFile              *file,
                                                    const gchar             *custom_icon,
                                                    GError                 **error) G_GNUC_WARN_UNUSED_RESULT;

gchar            *thunar_file_get_icon_name        (const ThunarFile        *file,
                                                    ThunarFileIconState     icon_state,
                                                    GtkIconTheme           *icon_theme);

const gchar      *thunar_file_get_metadata         (ThunarFile             *file,
                                                    ThunarMetafileKey       key,
                                                    const gchar            *default_value);
void              thunar_file_set_metadata         (ThunarFile             *file,
                                                    ThunarMetafileKey       key,
                                                    const gchar            *value,
                                                    const gchar            *default_value);

void              thunar_file_watch                (ThunarFile             *file);
void              thunar_file_unwatch              (ThunarFile             *file);

void              thunar_file_reload               (ThunarFile             *file);

void              thunar_file_destroy              (ThunarFile             *file);


gint              thunar_file_compare_by_name      (const ThunarFile       *file_a,
                                                    const ThunarFile       *file_b,
                                                    gboolean                case_sensitive);


ThunarFile       *thunar_file_cache_lookup         (const GFile            *file);
ThunarFile       *thunar_file_cache_lookup_path    (const ThunarVfsPath    *path);


GList            *thunar_file_list_get_applications  (GList *file_list);
GList            *thunar_file_list_to_g_file_list    (GList *file_list);
GList            *thunar_file_list_to_path_list      (GList *file_list);

gboolean         thunar_file_is_desktop              (const ThunarFile *file);

/**
 * thunar_file_is_root:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the root directory.
 *
 * Return value: %TRUE if @file is the root directory.
 **/
#define thunar_file_is_root(file) (g_file_is_root (THUNAR_FILE ((file))->gfile))

/**
 * thunar_file_has_parent:
 * @file : a #ThunarFile instance.
 *
 * Checks whether it is possible to determine the parent #ThunarFile
 * for @file.
 *
 * Return value: whether @file has a parent.
 **/
#define thunar_file_has_parent(file) (!thunar_file_is_root (THUNAR_FILE ((file))))

/**
 * thunar_file_get_info:
 * @file : a #ThunarFile instance.
 *
 * Returns the #ThunarVfsInfo for @file.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ThunarVfsInfo, so if you need the object for a longer
 * perioud, you'll need to take a reference yourself using the
 * thunar_vfs_info_ref() method.
 *
 * Return value: the #ThunarVfsInfo for @file.
 **/
#define thunar_file_get_info(file) (THUNAR_FILE ((file))->info)

/**
 * thunar_file_get_path:
 * @file  : a #ThunarFile instance.
 *
 * Returns the #ThunarVfsPath, that refers to the location of the @file.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ThunarVfsPath, so if you need the object for a longer
 * period, you'll need to take a reference yourself using the
 * thunar_vfs_path_ref() function.
 *
 * Return value: the path to the @file.
 **/
#define thunar_file_get_path(file) (THUNAR_FILE ((file))->info->path)

/**
 * thunar_file_get_file:
 * @file : a #ThunarFile instance.
 *
 * Returns the #GFile that refers to the location of @file.
 *
 * The returned #GFile is owned by @file and must not be released
 * with g_object_unref().
 * 
 * Return value: the #GFile corresponding to @file.
 **/
#define thunar_file_get_file(file) (THUNAR_FILE ((file))->gfile)

/**
 * thunar_file_get_mime_info:
 * @file : a #ThunarFile instance.
 *
 * Returns the MIME type information for the given @file object. This
 * function is garantied to always return a valid #ThunarVfsMimeInfo.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ThunarVfsMimeInfo, so if you need the object for a
 * longer period, you'll need to take a reference yourself using
 * the thunar_vfs_mime_info_ref() function.
 *
 * Return value: the MIME type.
 **/
#define thunar_file_get_mime_info(file) (THUNAR_FILE ((file))->info->mime_info)

/**
 * thunar_file_dup_uri:
 * @file : a #ThunarFile instance.
 *
 * Returns the URI for the @file. The caller is responsible
 * to free the returned string when no longer needed.
 *
 * Return value: the URI for @file.
 **/
#define thunar_file_dup_uri(file) (g_file_get_uri (THUNAR_FILE ((file))->gfile))


/**
 * thunar_file_get_custom_icon:
 * @file : a #ThunarFile instance.
 *
 * Queries the custom icon from @file if any,
 * else %NULL is returned. The custom icon
 * can be either a themed icon name or an
 * absolute path to an icon file in the local
 * file system.
 *
 * Return value: the custom icon for @file
 *               or %NULL.
 **/
#define thunar_file_get_custom_icon(file) (g_strdup (thunar_vfs_info_get_custom_icon (THUNAR_FILE ((file))->info)))



/**
 * thunar_file_changed:
 * @file : a #ThunarFile instance.
 *
 * Emits the ::changed signal on @file. This function is meant to be called
 * by derived classes whenever they notice changes to the @file.
 **/
#define thunar_file_changed(file)                         \
G_STMT_START{                                             \
  thunarx_file_info_changed (THUNARX_FILE_INFO ((file))); \
}G_STMT_END

/**
 * thunar_file_is_trashed:
 * @file : a #ThunarFile instance.
 *
 * Returns %TRUE if @file is a local file that resides in 
 * the trash bin.
 *
 * Return value: %TRUE if @file is in the trash, or
 *               the trash folder itself.
 **/
#define thunar_file_is_trashed(file) (g_file_is_trashed (THUNAR_FILE ((file))->gfile))

/**
 * thunar_file_is_desktop_file:
 * @file : a #ThunarFile.
 *
 * Returns %TRUE if @file is a .desktop file, but not a .directory file.
 *
 * Return value: %TRUE if @file is a .desktop file.
 **/
#define thunar_file_is_desktop_file(file) (exo_str_is_equal (thunar_vfs_mime_info_get_name (thunar_file_get_mime_info ((file))), "application/x-desktop") \
                                        && !exo_str_is_equal (thunar_vfs_path_get_name (thunar_file_get_path ((file))), ".directory"))

/**
 * thunar_file_get_display_name:
 * @file : a #ThunarFile instance.
 *
 * Returns the @file name in the UTF-8 encoding, which is
 * suitable for displaying the file name in the GUI.
 *
 * Return value: the @file name suitable for display.
 **/
#define thunar_file_get_display_name(file) (THUNAR_FILE ((file))->info->display_name)

/**
 * thunar_file_read_link:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors or %NULL.
 *
 * Simple wrapper to thunar_vfs_info_read_link().
 *
 * Return value: the link target of @file or %NULL
 *               if an error occurred.
 **/
#define thunar_file_read_link(file, error) (thunar_vfs_info_read_link (THUNAR_FILE ((file))->info, (error)))



/**
 * thunar_file_get_thumb_state:
 * @file : a #ThunarFile.
 *
 * Returns the current #ThunarFileThumbState for @file. This
 * method is intended to be used by #ThunarIconFactory only.
 *
 * Return value: the #ThunarFileThumbState for @file.
 **/
#define thunar_file_get_thumb_state(file) (THUNAR_FILE ((file))->flags & THUNAR_FILE_THUMB_STATE_MASK)

/**
 * thunar_file_set_thumb_state:
 * @file        : a #ThunarFile.
 * @thumb_state : the new #ThunarFileThumbState.
 *
 * Sets the #ThunarFileThumbState for @file
 * to @thumb_state. This method is intended
 * to be used by #ThunarIconFactory only.
 **/ 
#define thunar_file_set_thumb_state(file, thumb_state)                    \
G_STMT_START{                                                             \
  ThunarFile *f = THUNAR_FILE ((file));                                   \
  f->flags = (f->flags & ~THUNAR_FILE_THUMB_STATE_MASK) | (thumb_state);  \
}G_STMT_END



/**
 * thunar_file_list_copy:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Returns a deep-copy of @file_list, which must be
 * freed using thunar_file_list_free().
 *
 * Return value: a deep copy of @file_list.
 **/
#define thunar_file_list_copy(file_list) (thunarx_file_info_list_copy ((file_list)))

/**
 * thunar_file_list_free:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Unrefs the #ThunarFile<!---->s contained in @file_list
 * and frees the list itself.
 **/
#define thunar_file_list_free(file_list)      \
G_STMT_START{                                 \
  thunarx_file_info_list_free ((file_list));  \
}G_STMT_END


G_END_DECLS;

#endif /* !__THUNAR_FILE_H__ */
