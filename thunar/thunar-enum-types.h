/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#define THUNAR_TYPE_COLOR_STYLE (thunar_color_style_get_type ())

/**
 * ThunarColorStyle:
 * @THUNAR_COLOR_STYLE_SOLID      : solid desktop background color.
 * @THUNAR_COLOR_STYLE_HGRADIENT  : horizontal gradient from color0 to color1.
 * @THUNAR_COLOR_STYLE_VGRADIENT  : vertical gradient from color0 to color1.
 *
 * Desktop background color style.
 **/
typedef enum
{
  THUNAR_COLOR_STYLE_SOLID,
  THUNAR_COLOR_STYLE_HGRADIENT,
  THUNAR_COLOR_STYLE_VGRADIENT,
} ThunarColorStyle;

GType thunar_color_style_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


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

GType thunar_column_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


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
  THUNAR_ICON_SIZE_SMALL    = 36,
  THUNAR_ICON_SIZE_NORMAL   = 48,
  THUNAR_ICON_SIZE_LARGE    = 64,
  THUNAR_ICON_SIZE_LARGER   = 96,
  THUNAR_ICON_SIZE_LARGEST  = 128,
} ThunarIconSize;

GType thunar_icon_size_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


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

GType thunar_recursive_permissions_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


#define THUNAR_TYPE_WALLPAPER_STYLE (thunar_wallpaper_style_get_type ())

/**
 * ThunarWallpaperStyle:
 * @THUNAR_WALLPAPER_STYLE_CENTERED   : center the wallpaper.
 * @THUNAR_WALLPAPER_STYLE_SCALED     : scale the wallpaper.
 * @THUNAR_WALLPAPER_STYLE_STRETCHED  : stretch the wallpaper.
 * @THUNAR_WALLPAPER_STYLE_TILED      : tile the wallpaper.
 *
 * Desktop background wallpaper style.
 **/
typedef enum
{
  THUNAR_WALLPAPER_STYLE_CENTERED,
  THUNAR_WALLPAPER_STYLE_SCALED,
  THUNAR_WALLPAPER_STYLE_STRETCHED,
  THUNAR_WALLPAPER_STYLE_TILED,
} ThunarWallpaperStyle;

GType thunar_wallpaper_style_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


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

GType          thunar_zoom_level_get_type     (void) G_GNUC_CONST G_GNUC_INTERNAL;
ThunarIconSize thunar_zoom_level_to_icon_size (ThunarZoomLevel zoom_level) G_GNUC_CONST G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_ENUM_TYPES_H__ */
