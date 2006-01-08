/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-metafile.h>
#include <thunarx/thunarx.h>

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
 *
 * The various file icon states that are used within the file manager
 * views.
 **/
typedef enum /*< flags >*/
{
  THUNAR_FILE_ICON_STATE_DEFAULT,
  THUNAR_FILE_ICON_STATE_DROP,
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
  void (*changed) (ThunarFile *file);
  void (*destroy) (ThunarFile *file);
  void (*renamed) (ThunarFile *file);
};

struct _ThunarFile
{
  GObject __parent__;

  /*< private >*/
  ThunarVfsInfo *info;
  guint flags;
};

GType                            thunar_file_get_type             (void) G_GNUC_CONST;

ThunarFile                      *thunar_file_get_for_info         (ThunarVfsInfo          *info);
ThunarFile                      *thunar_file_get_for_path         (ThunarVfsPath          *path,
                                                                   GError                **error);

static inline gboolean           thunar_file_has_parent           (const ThunarFile       *file);
ThunarFile                      *thunar_file_get_parent           (const ThunarFile       *file,
                                                                   GError                **error);

gboolean                         thunar_file_execute              (ThunarFile             *file,
                                                                   GdkScreen              *screen,
                                                                   GList                  *path_list,
                                                                   GError                **error);

gboolean                         thunar_file_rename               (ThunarFile             *file,
                                                                   const gchar            *name,
                                                                   GError                **error);

GdkDragAction                    thunar_file_accepts_drop         (ThunarFile             *file,
                                                                   GList                  *path_list,
                                                                   GdkDragAction           actions);

const gchar                     *thunar_file_get_display_name     (const ThunarFile       *file);
const gchar                     *thunar_file_get_special_name     (const ThunarFile       *file);

static inline ThunarVfsInfo     *thunar_file_get_info             (const ThunarFile       *file);
static inline ThunarVfsPath     *thunar_file_get_path             (const ThunarFile       *file);
static inline ThunarVfsMimeInfo *thunar_file_get_mime_info        (const ThunarFile       *file);
ThunarVfsFileTime                thunar_file_get_date             (const ThunarFile       *file,
                                                                   ThunarFileDateType      date_type);
static inline ThunarVfsFileType  thunar_file_get_kind             (const ThunarFile       *file);
static inline ThunarVfsFileMode  thunar_file_get_mode             (const ThunarFile       *file);
static inline ThunarVfsFileSize  thunar_file_get_size             (const ThunarFile       *file);

gchar                           *thunar_file_get_date_string      (const ThunarFile       *file,
                                                                   ThunarFileDateType      date_type);
gchar                           *thunar_file_get_mode_string      (const ThunarFile       *file);
gchar                           *thunar_file_get_size_string      (const ThunarFile       *file);

ThunarVfsVolume                 *thunar_file_get_volume           (const ThunarFile       *file,
                                                                   ThunarVfsVolumeManager *volume_manager);

ThunarVfsGroup                  *thunar_file_get_group            (const ThunarFile       *file);
ThunarVfsUser                   *thunar_file_get_user             (const ThunarFile       *file);

gboolean                         thunar_file_is_chmodable         (const ThunarFile       *file);
gboolean                         thunar_file_is_executable        (const ThunarFile       *file);
gboolean                         thunar_file_is_readable          (const ThunarFile       *file);
gboolean                         thunar_file_is_renameable        (const ThunarFile       *file);
gboolean                         thunar_file_is_writable          (const ThunarFile       *file);

GList                           *thunar_file_get_actions          (ThunarFile             *file,
                                                                   GtkWidget              *window);

GList                           *thunar_file_get_emblem_names     (ThunarFile              *file);
void                             thunar_file_set_emblem_names     (ThunarFile              *file,
                                                                   GList                   *emblem_names);

static inline const gchar       *thunar_file_get_custom_icon      (const ThunarFile       *file);
const gchar                     *thunar_file_get_icon_name        (const ThunarFile        *file,
                                                                   ThunarFileIconState     icon_state,
                                                                   GtkIconTheme           *icon_theme);

const gchar                     *thunar_file_get_metadata         (ThunarFile             *file,
                                                                   ThunarMetafileKey       key,
                                                                   const gchar            *default_value);
void                             thunar_file_set_metadata         (ThunarFile             *file,
                                                                   ThunarMetafileKey       key,
                                                                   const gchar            *value,
                                                                   const gchar            *default_value);
static inline gboolean           thunar_file_get_boolean_metadata (ThunarFile             *file,
                                                                   ThunarMetafileKey       key,
                                                                   gboolean                default_value);
static inline void               thunar_file_set_boolean_metadata (ThunarFile             *file,
                                                                   ThunarMetafileKey       key,
                                                                   gboolean                value,
                                                                   gboolean                default_value);

void                             thunar_file_watch                (ThunarFile             *file);
void                             thunar_file_unwatch              (ThunarFile             *file);

void                             thunar_file_reload               (ThunarFile             *file);

void                             thunar_file_changed              (ThunarFile             *file);
void                             thunar_file_destroy              (ThunarFile             *file);

static inline gboolean           thunar_file_is_directory         (const ThunarFile       *file);
static inline gboolean           thunar_file_is_home              (const ThunarFile       *file);
gboolean                         thunar_file_is_hidden            (const ThunarFile       *file);
static inline gboolean           thunar_file_is_regular           (const ThunarFile       *file);
static inline gboolean           thunar_file_is_root              (const ThunarFile       *file);
static inline gboolean           thunar_file_is_symlink           (const ThunarFile       *file);


ThunarFile                      *thunar_file_cache_lookup         (const ThunarVfsPath    *path);


/* these methods are provided for ThunarIconFactory */
static inline ThunarFileThumbState thunar_file_get_thumb_state (const ThunarFile    *file);
static inline void                 thunar_file_set_thumb_state (ThunarFile          *file,
                                                                ThunarFileThumbState thumb_state);


static inline GList *thunar_file_list_copy (GList *file_list);
static inline void   thunar_file_list_free (GList *file_list);


/**
 * thunar_file_has_parent:
 * @file : a #ThunarFile instance.
 *
 * Checks whether it is possible to determine the parent #ThunarFile
 * for @file.
 *
 * Return value: whether @file has a parent.
 **/
static inline gboolean
thunar_file_has_parent (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return !thunar_file_is_root (file);
}

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
static inline ThunarVfsInfo*
thunar_file_get_info (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->info;
}

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
static inline ThunarVfsPath*
thunar_file_get_path (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->info->path;
}

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
static inline ThunarVfsMimeInfo*
thunar_file_get_mime_info (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->info->mime_info;
}

/**
 * thunar_file_get_kind:
 * @file : a #ThunarFile instance.
 *
 * Returns the kind of @file.
 *
 * Return value: the kind of @file.
 **/
static inline ThunarVfsFileType
thunar_file_get_kind (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), THUNAR_VFS_FILE_TYPE_UNKNOWN);
  return file->info->type;
}

/**
 * thunar_file_get_mode:
 * @file : a #ThunarFile instance.
 *
 * Returns the permission bits of @file.
 *
 * Return value: the permission bits of @file.
 **/
static inline ThunarVfsFileMode
thunar_file_get_mode (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), 0);
  return file->info->mode;
}

/**
 * thunar_file_get_size:
 * @file : a #ThunarFile instance.
 *
 * Tries to determine the size of @file in bytes and
 * returns the size.
 *
 * Return value: the size of @file in bytes.
 **/
static inline ThunarVfsFileSize
thunar_file_get_size (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return file->info->size;
}

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
static inline const gchar*
thunar_file_get_custom_icon (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return thunar_vfs_info_get_custom_icon (file->info);
}

/**
 * thunar_file_get_boolean_metadata:
 * @file          : a #ThunarFile instance.
 * @key           : a #ThunarMetafileKey.
 * @default_value : the default value.
 *
 * Specialized version of thunar_file_get_metadata()
 * for boolean metadata values.
 *
 * Return value: the metadata for @key in @file if @key
 *               exists, or @default_value.
 **/
static inline gboolean
thunar_file_get_boolean_metadata (ThunarFile       *file,
                                  ThunarMetafileKey key,
                                  gboolean          default_value)
{
  const gchar *value;
  value = thunar_file_get_metadata (file, key, default_value ? "true" : "false");
  return (value[0] == 't' && value[1] == 'r'
       && value[2] == 'u' && value[3] == 'e'
       && value[4] == '\0');
}



/**
 * thunar_file_set_boolean_metadata:
 * @file          : a #ThunarFile instance.
 * @key           : a #ThunarMetafileKey.
 * @value         : the new value.
 * @default_value : the default value for @key.
 *
 * Specialized version of thunar_file_set_metadata()
 * for boolean metadata values.
 **/
static inline void
thunar_file_set_boolean_metadata (ThunarFile       *file,
                                  ThunarMetafileKey key,
                                  gboolean          value,
                                  gboolean          default_value)
{
  thunar_file_set_metadata (file, key, value ? "true" : "false",
                            default_value ? "true" : "false");
}



/**
 * thunar_file_is_directory:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file refers to a directory.
 *
 * Return value: %TRUE if @file is a directory.
 **/
static inline gboolean
thunar_file_is_directory (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return (file->info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY);
}

/**
 * thunar_file_is_home:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the users home directory.
 *
 * Return value: %TRUE if @file is the users home directory.
 **/
static inline gboolean
thunar_file_is_home (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return thunar_vfs_path_is_home (file->info->path);
}

/**
 * thunar_file_is_regular:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to a regular file.
 *
 * Return value: %TRUE if @file is a regular file.
 **/
static inline gboolean
thunar_file_is_regular (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return (file->info->type == THUNAR_VFS_FILE_TYPE_REGULAR);
}

/**
 * thunar_file_is_root:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the root directory.
 *
 * Return value: %TRUE if @file is the root directory.
 **/
static inline gboolean
thunar_file_is_root (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return thunar_vfs_path_is_root (file->info->path);
}

/**
 * thunar_file_is_symlink:
 * @file : a #ThunarFile.
 *
 * Returns %TRUE if @file is a symbolic link.
 *
 * Return value: %TRUE if @file is a symbolic link.
 **/
static inline gboolean
thunar_file_is_symlink (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return ((file->info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) != 0);
}



/**
 * thunar_file_get_thumb_state:
 * @file : a #ThunarFile.
 *
 * Returns the current #ThunarFileThumbState for @file.
 *
 * Return value: the #ThunarFileThumbState for @file.
 **/
static inline ThunarFileThumbState
thunar_file_get_thumb_state (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), THUNAR_FILE_THUMB_STATE_NONE);
  return (file->flags & THUNAR_FILE_THUMB_STATE_MASK);
}

/**
 * thunar_file_set_thumb_state:
 * @file        : a #ThunarFile.
 * @thumb_state : the new #ThunarFileThumbState.
 *
 * Sets the #ThunarFileThumbState for @file
 * to @thumb_state.
 **/ 
static inline void
thunar_file_set_thumb_state (ThunarFile          *file,
                             ThunarFileThumbState thumb_state)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  file->flags = (file->flags & ~THUNAR_FILE_THUMB_STATE_MASK) | thumb_state;
}



/**
 * thunar_file_list_copy:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Returns a deep-copy of @file_list, which must be
 * freed using thunar_file_list_free().
 *
 * Return value: a deep copy of @file_list.
 **/
static inline GList*
thunar_file_list_copy (GList *file_list)
{
  return thunarx_file_info_list_copy (file_list);
}

/**
 * thunar_file_list_free:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Unrefs the #ThunarFile<!---->s contained in @file_list
 * and frees the list itself.
 **/
static inline void
thunar_file_list_free (GList *file_list)
{
  thunarx_file_info_list_free (file_list);
}


G_END_DECLS;

#endif /* !__THUNAR_FILE_H__ */
