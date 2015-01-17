/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_ENUM_TYPES_H__
#define __THUNAR_ENUM_TYPES_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

#define THUNAR_TYPE_RENAMER_MODE (thunar_renamer_mode_get_type ())

/**
 * ThunarRenamerMode:
 * @THUNAR_RENAMER_MODE_NAME   : only the name should be renamed.
 * @THUNAR_RENAMER_MODE_SUFFIX : only the suffix should be renamed.
 * @THUNAR_RENAMER_MODE_BOTH   : the name and the suffix should be renamed.
 *
 * The rename mode for a #ThunarRenamerModel instance.
 **/
typedef enum
{
  THUNAR_RENAMER_MODE_NAME,
  THUNAR_RENAMER_MODE_SUFFIX,
  THUNAR_RENAMER_MODE_BOTH,
} ThunarRenamerMode;

GType thunar_renamer_mode_get_type (void) G_GNUC_CONST;



#define THUNAR_TYPE_DATE_STYLE (thunar_date_style_get_type ())

/**
 * ThunarDateStyle:
 * @THUNAR_DATE_STYLE_SIMPLE : display only the date.
 * @THUNAR_DATE_STYLE_SHORT  : display date and time in a short manner.
 * @THUNAR_DATE_STYLE_LONG   : display date and time in a long manner.
 * @THUNAR_DATE_STYLE_ISO    : display date and time in ISO standard form.
 *
 * The style used to display dates (i.e. modification dates) to the user.
 **/
typedef enum
{
  THUNAR_DATE_STYLE_SIMPLE,
  THUNAR_DATE_STYLE_SHORT,
  THUNAR_DATE_STYLE_LONG,
  THUNAR_DATE_STYLE_ISO,
} ThunarDateStyle;

GType thunar_date_style_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_COLUMN (thunar_column_get_type ())

/**
 * ThunarColumn:
 * @THUNAR_COLUMN_DATE_ACCESSED : last access time.
 * @THUNAR_COLUMN_DATE_MODIFIED : last modification time.
 * @THUNAR_COLUMN_GROUP         : group's name.
 * @THUNAR_COLUMN_MIME_TYPE     : mime type (i.e. "text/plain").
 * @THUNAR_COLUMN_NAME          : display name.
 * @THUNAR_COLUMN_OWNER         : owner's name.
 * @THUNAR_COLUMN_PERMISSIONS   : permissions bits.
 * @THUNAR_COLUMN_SIZE          : file size.
 * @THUNAR_COLUMN_TYPE          : file type (i.e. 'plain text document').
 * @THUNAR_COLUMN_FILE          : #ThunarFile object.
 * @THUNAR_COLUMN_FILE_NAME     : real file name.
 *
 * Columns exported by #ThunarListModel using the #GtkTreeModel
 * interface.
 **/
typedef enum
{
  /* visible columns */
  THUNAR_COLUMN_DATE_ACCESSED,
  THUNAR_COLUMN_DATE_MODIFIED,
  THUNAR_COLUMN_GROUP,
  THUNAR_COLUMN_MIME_TYPE,
  THUNAR_COLUMN_NAME,
  THUNAR_COLUMN_OWNER,
  THUNAR_COLUMN_PERMISSIONS,
  THUNAR_COLUMN_SIZE,
  THUNAR_COLUMN_TYPE,

  /* special internal columns */
  THUNAR_COLUMN_FILE,
  THUNAR_COLUMN_FILE_NAME,

  /* number of columns */
  THUNAR_N_COLUMNS,

  /* number of visible columns */
  THUNAR_N_VISIBLE_COLUMNS = THUNAR_COLUMN_FILE,
} ThunarColumn;

GType thunar_column_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_ICON_SIZE (thunar_icon_size_get_type ())

/**
 * ThunarIconSize:
 * @THUNAR_ICON_SIZE_SMALLEST : icon size for #THUNAR_ZOOM_LEVEL_SMALLEST.
 * @THUNAR_ICON_SIZE_SMALLER  : icon size for #THUNAR_ZOOM_LEVEL_SMALLER.
 * @THUNAR_ICON_SIZE_SMALL    : icon size for #THUNAR_ZOOM_LEVEL_SMALL.
 * @THUNAR_ICON_SIZE_NORMAL   : icon size for #THUNAR_ZOOM_LEVEL_NORMAL.
 * @THUNAR_ICON_SIZE_LARGE    : icon size for #THUNAR_ZOOM_LEVEL_LARGE.
 * @THUNAR_ICON_SIZE_LARGER   : icon size for #THUNAR_ZOOM_LEVEL_LARGER.
 * @THUNAR_ICON_SIZE_LARGEST  : icon size for #THUNAR_ZOOM_LEVEL_LARGEST.
 *
 * Icon sizes matching the various #ThunarZoomLevel<!---->s.
 **/
typedef enum
{
  THUNAR_ICON_SIZE_SMALLEST = 16,
  THUNAR_ICON_SIZE_SMALLER  = 24,
  THUNAR_ICON_SIZE_SMALL    = 32,
  THUNAR_ICON_SIZE_NORMAL   = 48,
  THUNAR_ICON_SIZE_LARGE    = 64,
  THUNAR_ICON_SIZE_LARGER   = 96,
  THUNAR_ICON_SIZE_LARGEST  = 128,
} ThunarIconSize;

GType thunar_icon_size_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_THUMBNAIL_MODE (thunar_thumbnail_mode_get_type ())

/**
 * ThunarThumbnailsShow:
 * @THUNAR_THUMBNAIL_MODE_NEVER      : never show thumbnails.
 * @THUNAR_THUMBNAIL_MODE_ONLY_LOCAL : only show thumbnails on local filesystems.
 * @THUNAR_THUMBNAIL_MODE_ALWAYS     : always show thumbnails (everywhere).
 **/
typedef enum
{
  THUNAR_THUMBNAIL_MODE_NEVER,
  THUNAR_THUMBNAIL_MODE_ONLY_LOCAL,
  THUNAR_THUMBNAIL_MODE_ALWAYS
} ThunarThumbnailMode;

GType thunar_thumbnail_mode_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_RECURSIVE_PERMISSIONS (thunar_recursive_permissions_get_type ())

/**
 * ThunarRecursivePermissionsMode:
 * @THUNAR_RECURSIVE_PERMISSIONS_ASK    : ask the user everytime permissions are changed.
 * @THUNAR_RECURSIVE_PERMISSIONS_ALWAYS : always apply the change recursively.
 * @THUNAR_RECURSIVE_PERMISSIONS_NEVER  : never apply the change recursively.
 *
 * Modus operandi when changing permissions.
 **/
typedef enum
{
  THUNAR_RECURSIVE_PERMISSIONS_ASK,
  THUNAR_RECURSIVE_PERMISSIONS_ALWAYS,
  THUNAR_RECURSIVE_PERMISSIONS_NEVER,
} ThunarRecursivePermissionsMode;

GType thunar_recursive_permissions_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_ZOOM_LEVEL (thunar_zoom_level_get_type ())

/**
 * ThunarZoomLevel:
 * @THUNAR_ZOOM_LEVEL_SMALLEST : smallest possible zoom level.
 * @THUNAR_ZOOM_LEVEL_SMALLER  : smaller zoom level.
 * @THUNAR_ZOOM_LEVEL_SMALL    : small zoom level.
 * @THUNAR_ZOOM_LEVEL_NORMAL   : the default zoom level.
 * @THUNAR_ZOOM_LEVEL_LARGE    : large zoom level.
 * @THUNAR_ZOOM_LEVEL_LARGER   : larger zoom level.
 * @THUNAR_ZOOM_LEVEL_LARGEST  : largest possible zoom level.
 *
 * Lists the various zoom levels supported by Thunar's
 * folder views.
 **/
typedef enum
{
  THUNAR_ZOOM_LEVEL_SMALLEST,
  THUNAR_ZOOM_LEVEL_SMALLER,
  THUNAR_ZOOM_LEVEL_SMALL,
  THUNAR_ZOOM_LEVEL_NORMAL,
  THUNAR_ZOOM_LEVEL_LARGE,
  THUNAR_ZOOM_LEVEL_LARGER,
  THUNAR_ZOOM_LEVEL_LARGEST,

  /*< private >*/
  THUNAR_ZOOM_N_LEVELS,
} ThunarZoomLevel;

GType          thunar_zoom_level_get_type     (void) G_GNUC_CONST;


#define THUNAR_TYPE_JOB_RESPONSE (thunar_job_response_get_type ())

/**
 * ThunarJobResponse:
 * @THUNAR_JOB_RESPONSE_YES     :
 * @THUNAR_JOB_RESPONSE_YES_ALL :
 * @THUNAR_JOB_RESPONSE_NO      :
 * @THUNAR_JOB_RESPONSE_NO_ALL  :
 * @THUNAR_JOB_RESPONSE_CANCEL  :
 * @THUNAR_JOB_RESPONSE_RETRY   :
 * @THUNAR_JOB_RESPONSE_FORCE   :
 *
 * Possible responses for the ThunarJob::ask signal.
 **/
typedef enum /*< flags >*/
{
  THUNAR_JOB_RESPONSE_YES     = 1 << 0,
  THUNAR_JOB_RESPONSE_YES_ALL = 1 << 1,
  THUNAR_JOB_RESPONSE_NO      = 1 << 2,
  THUNAR_JOB_RESPONSE_CANCEL  = 1 << 3,
  THUNAR_JOB_RESPONSE_NO_ALL  = 1 << 4,
  THUNAR_JOB_RESPONSE_RETRY   = 1 << 5,
  THUNAR_JOB_RESPONSE_FORCE   = 1 << 6,
} ThunarJobResponse;

GType thunar_job_response_get_type (void) G_GNUC_CONST;


#define THUNAR_TYPE_FILE_MODE (thunar_file_mode_get_type ())

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

GType thunar_file_mode_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_ENUM_TYPES_H__ */
