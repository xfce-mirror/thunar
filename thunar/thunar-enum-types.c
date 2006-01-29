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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-enum-types.h>



static void   thunar_enum_from_string           (const GValue     *src_value,
                                                 GValue           *dst_value);
static GType  thunar_enum_register_type         (const gchar      *enum_name,
                                                 const GEnumValue *enum_values);
static void   thunar_icon_size_from_zoom_level  (const GValue     *src_value,
                                                 GValue           *dst_value);



GType
thunar_color_style_get_type (void)
{
  static const GEnumValue values[] =
  {
    { THUNAR_COLOR_STYLE_SOLID,     "THUNAR_COLOR_STYLE_SOLID",     "solid",     },
    { THUNAR_COLOR_STYLE_HGRADIENT, "THUNAR_COLOR_STYLE_HGRADIENT", "hgradient", },
    { THUNAR_COLOR_STYLE_VGRADIENT, "THUNAR_COLOR_STYLE_VGRADIENT", "vgradient", },
    { 0,                            NULL,                           NULL,        },
  };

  return thunar_enum_register_type ("ThunarColorStyle", values);
}



GType
thunar_icon_size_get_type (void)
{
  static const GEnumValue values[] =
  {
    { THUNAR_ICON_SIZE_SMALLEST, "THUNAR_ICON_SIZE_SMALLEST", "smallest", },
    { THUNAR_ICON_SIZE_SMALLER,  "THUNAR_ICON_SIZE_SMALLER",  "smaller",  },
    { THUNAR_ICON_SIZE_SMALL,    "THUNAR_ICON_SIZE_SMALL",    "small",    },
    { THUNAR_ICON_SIZE_NORMAL,   "THUNAR_ICON_SIZE_NORMAL",   "normal",   },
    { THUNAR_ICON_SIZE_LARGE,    "THUNAR_ICON_SIZE_LARGE",    "large",    },
    { THUNAR_ICON_SIZE_LARGER,   "THUNAR_ICON_SIZE_LARGER",   "larger",   },
    { THUNAR_ICON_SIZE_LARGEST,  "THUNAR_ICON_SIZE_LARGEST",  "largest",  },
    { 0,                         NULL,                        NULL,       },
  };

  return thunar_enum_register_type ("ThunarIconSize", values);
}



GType
thunar_recursive_permissions_get_type (void)
{
  static const GEnumValue values[] =
  {
    { THUNAR_RECURSIVE_PERMISSIONS_ASK,    "THUNAR_RECURSIVE_PERMISSIONS_ASK",    "ask",    },
    { THUNAR_RECURSIVE_PERMISSIONS_ALWAYS, "THUNAR_RECURSIVE_PERMISSIONS_ALWAYS", "always", },
    { THUNAR_RECURSIVE_PERMISSIONS_NEVER,  "THUNAR_RECURSIVE_PERMISSIONS_NEVER",  "never",  },
    { 0,                                   NULL,                                  NULL,     },
  };

  return thunar_enum_register_type ("ThunarRecursivePermissions", values);
}



GType
thunar_wallpaper_style_get_type (void)
{
  static const GEnumValue values[] =
  {
    { THUNAR_WALLPAPER_STYLE_CENTERED,  "THUNAR_WALLPAPER_STYLE_CENTERED",  "centered",  },
    { THUNAR_WALLPAPER_STYLE_SCALED,    "THUNAR_WALLPAPER_STYLE_SCALED",    "scaled",    },
    { THUNAR_WALLPAPER_STYLE_STRETCHED, "THUNAR_WALLPAPER_STYLE_STRETCHED", "stretched", },
    { THUNAR_WALLPAPER_STYLE_TILED,     "THUNAR_WALLPAPER_STYLE_TILED",     "tiled",     },
    { 0,                                NULL,                               NULL,        },
  };

  return thunar_enum_register_type ("ThunarWallpaperStyle", values);
}



GType
thunar_zoom_level_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_ZOOM_LEVEL_SMALLEST, "THUNAR_ZOOM_LEVEL_SMALLEST", "smallest", },
        { THUNAR_ZOOM_LEVEL_SMALLER,  "THUNAR_ZOOM_LEVEL_SMALLER",  "smaller",  },
        { THUNAR_ZOOM_LEVEL_SMALL,    "THUNAR_ZOOM_LEVEL_SMALL",    "small",    },
        { THUNAR_ZOOM_LEVEL_NORMAL,   "THUNAR_ZOOM_LEVEL_NORMAL",   "normal",   },
        { THUNAR_ZOOM_LEVEL_LARGE,    "THUNAR_ZOOM_LEVEL_LARGE",    "large",    },
        { THUNAR_ZOOM_LEVEL_LARGER,   "THUNAR_ZOOM_LEVEL_LARGER",   "larger",   },
        { THUNAR_ZOOM_LEVEL_LARGEST,  "THUNAR_ZOOM_LEVEL_LARGEST",  "largest",  },
        { 0,                          NULL,                         NULL,       },
      };

      type = g_enum_register_static (I_("ThunarZoomLevel"), values);

      /* register transformation function for ThunarZoomLevel->ThunarIconSize */
      g_value_register_transform_func (type, THUNAR_TYPE_ICON_SIZE, thunar_icon_size_from_zoom_level);

      /* register transformation function for string->ThunarZoomLevel */
      g_value_register_transform_func (G_TYPE_STRING, type, thunar_enum_from_string);
    }

  return type;
}



/**
 * thunar_zoom_level_to_icon_size:
 * @zoom_level : a #ThunarZoomLevel.
 *
 * Returns the #ThunarIconSize corresponding to the @zoom_level.
 *
 * Return value: the #ThunarIconSize for @zoom_level.
 **/
ThunarIconSize
thunar_zoom_level_to_icon_size (ThunarZoomLevel zoom_level)
{
  switch (zoom_level)
    {
    case THUNAR_ZOOM_LEVEL_SMALLEST: return THUNAR_ICON_SIZE_SMALLEST;
    case THUNAR_ZOOM_LEVEL_SMALLER:  return THUNAR_ICON_SIZE_SMALLER;
    case THUNAR_ZOOM_LEVEL_SMALL:    return THUNAR_ICON_SIZE_SMALL;
    case THUNAR_ZOOM_LEVEL_NORMAL:   return THUNAR_ICON_SIZE_NORMAL;
    case THUNAR_ZOOM_LEVEL_LARGE:    return THUNAR_ICON_SIZE_LARGE;
    case THUNAR_ZOOM_LEVEL_LARGER:   return THUNAR_ICON_SIZE_LARGER;
    default:                         return THUNAR_ICON_SIZE_LARGEST;
    }
}



static void
thunar_enum_from_string (const GValue *src_value,
                         GValue       *dst_value)
{
  GEnumClass *klass;
  gint        value = 0;
  gint        n;

  /* determine the enum value matching the src_value... */
  klass = g_type_class_ref (G_VALUE_TYPE (dst_value));
  for (n = 0; n < klass->n_values; ++n)
    {
      value = klass->values[n].value;
      if (exo_str_is_equal (klass->values[n].value_name, g_value_get_string (src_value)))
        break;
    }
  g_type_class_unref (klass);

  /* ...and return that value */
  g_value_set_enum (dst_value, value);
}



static GType
thunar_enum_register_type (const gchar      *enum_name,
                           const GEnumValue *enum_values)
{
  GType type;

  /* check if we already have the type */
  type = g_type_from_name (enum_name);
  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      /* register the type using the specified enum values */
      type = g_enum_register_static (I_(enum_name), enum_values);

      /* register transformation function, that transforms strings to the enum type */
      g_value_register_transform_func (G_TYPE_STRING, type, thunar_enum_from_string);
    }

  return type;
}



static void
thunar_icon_size_from_zoom_level (const GValue *src_value,
                                  GValue       *dst_value)
{
  g_value_set_enum (dst_value, thunar_zoom_level_to_icon_size (g_value_get_enum (src_value)));
}




