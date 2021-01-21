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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-enum-types.h>



static void                thunar_icon_size_from_zoom_level     (const GValue   *src_value,
                                                                 GValue         *dst_value);
static void                thunar_thumbnail_size_from_icon_size (const GValue   *src_value,
                                                                 GValue         *dst_value);
static ThunarIconSize      thunar_zoom_level_to_icon_size       (ThunarZoomLevel zoom_level);
static ThunarThumbnailSize thunar_icon_size_to_thumbnail_size   (ThunarIconSize  icon_size);



GType
thunar_renamer_mode_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_RENAMER_MODE_NAME,   "THUNAR_RENAMER_MODE_NAME",   N_ ("Name only"),       },
        { THUNAR_RENAMER_MODE_SUFFIX, "THUNAR_RENAMER_MODE_SUFFIX", N_ ("Suffix only"),     },
        { THUNAR_RENAMER_MODE_BOTH,   "THUNAR_RENAMER_MODE_BOTH",   N_ ("Name and Suffix"), },
        { 0,                          NULL,                         NULL,                   },
      };

      type = g_enum_register_static (I_("ThunarRenamerMode"), values);
    }

  return type;
}



GType
thunar_date_style_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_DATE_STYLE_SIMPLE,   "THUNAR_DATE_STYLE_SIMPLE",   "simple",   },
        { THUNAR_DATE_STYLE_SHORT,    "THUNAR_DATE_STYLE_SHORT",    "short",    },
        { THUNAR_DATE_STYLE_LONG,     "THUNAR_DATE_STYLE_LONG",     "long",     },
        { THUNAR_DATE_STYLE_YYYYMMDD, "THUNAR_DATE_STYLE_YYYYMMDD", "yyyymmdd", },
        { THUNAR_DATE_STYLE_MMDDYYYY, "THUNAR_DATE_STYLE_MMDDYYYY", "mmddyyyy", },
        { THUNAR_DATE_STYLE_DDMMYYYY, "THUNAR_DATE_STYLE_DDMMYYYY", "ddmmyyyy", },
        { THUNAR_DATE_STYLE_CUSTOM,   "THUNAR_DATE_STYLE_CUSTOM",   "custom",   },
        /* to stay backward compartible*/
        { THUNAR_DATE_STYLE_YYYYMMDD, "THUNAR_DATE_STYLE_ISO",      "iso",      },
        { 0,                          NULL,                         NULL,       },
      };

      type = g_enum_register_static (I_("ThunarDateStyle"), values);
    }

  return type;
}



GType
thunar_column_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_COLUMN_DATE_CREATED,  "THUNAR_COLUMN_DATE_CREATED",  N_ ("Date Created"),  },
        { THUNAR_COLUMN_DATE_ACCESSED, "THUNAR_COLUMN_DATE_ACCESSED", N_ ("Date Accessed"), },
        { THUNAR_COLUMN_DATE_MODIFIED, "THUNAR_COLUMN_DATE_MODIFIED", N_ ("Date Modified"), },
        { THUNAR_COLUMN_GROUP,         "THUNAR_COLUMN_GROUP",         N_ ("Group"),         },
        { THUNAR_COLUMN_MIME_TYPE,     "THUNAR_COLUMN_MIME_TYPE",     N_ ("MIME Type"),     },
        { THUNAR_COLUMN_NAME,          "THUNAR_COLUMN_NAME",          N_ ("Name"),          },
        { THUNAR_COLUMN_OWNER,         "THUNAR_COLUMN_OWNER",         N_ ("Owner"),         },
        { THUNAR_COLUMN_PERMISSIONS,   "THUNAR_COLUMN_PERMISSIONS",   N_ ("Permissions"),   },
        { THUNAR_COLUMN_SIZE,          "THUNAR_COLUMN_SIZE",          N_ ("Size"),          },
        { THUNAR_COLUMN_SIZE_IN_BYTES, "THUNAR_COLUMN_SIZE_IN_BYTES", N_ ("Size in Bytes"), },
        { THUNAR_COLUMN_TYPE,          "THUNAR_COLUMN_TYPE",          N_ ("Type"),          },
        { THUNAR_COLUMN_FILE,          "THUNAR_COLUMN_FILE",          N_ ("File"),          },
        { THUNAR_COLUMN_FILE_NAME,     "THUNAR_COLUMN_FILE_NAME",     N_ ("File Name"),     },
        { 0,                           NULL,                          NULL,                 },
      };

      type = g_enum_register_static (I_("ThunarColumn"), values);
    }

  return type;
}



const gchar*
thunar_column_string_from_value (ThunarColumn value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (THUNAR_TYPE_COLUMN);
  enum_value = g_enum_get_value (enum_class, value);

  g_type_class_unref (enum_class);

  if (enum_value == NULL)
    return NULL;

  return enum_value->value_name;
}



gboolean
thunar_column_value_from_string (const gchar *value_string,
                                 gint        *value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (THUNAR_TYPE_COLUMN);
  enum_value = g_enum_get_value_by_name (enum_class, value_string);

  g_type_class_unref (enum_class);

  if (enum_value == NULL)
    return FALSE;

  *value =  enum_value->value;
  return TRUE;
}



GType
thunar_icon_size_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_ICON_SIZE_16,   "THUNAR_ICON_SIZE_16",        "16px",   },
        { THUNAR_ICON_SIZE_24,   "THUNAR_ICON_SIZE_24",        "24px",   },
        { THUNAR_ICON_SIZE_32,   "THUNAR_ICON_SIZE_32",        "32px",   },
        { THUNAR_ICON_SIZE_48,   "THUNAR_ICON_SIZE_48",        "48px",   },
        { THUNAR_ICON_SIZE_64,   "THUNAR_ICON_SIZE_64",        "64px",   },
        { THUNAR_ICON_SIZE_96,   "THUNAR_ICON_SIZE_96",        "96px",   },
        { THUNAR_ICON_SIZE_128,  "THUNAR_ICON_SIZE_128",       "128px",  },
        { THUNAR_ICON_SIZE_160,  "THUNAR_ICON_SIZE_160",       "160px",  },
        { THUNAR_ICON_SIZE_192,  "THUNAR_ICON_SIZE_192",       "192px",  },
        { THUNAR_ICON_SIZE_256,  "THUNAR_ICON_SIZE_256",       "256px",  },
        /* Support of old type-strings for two thunar stable releases. Old strings will be transformed to new ones on write*/
        { THUNAR_ICON_SIZE_16,   "THUNAR_ICON_SIZE_SMALLEST",  "16px",   },
        { THUNAR_ICON_SIZE_24,   "THUNAR_ICON_SIZE_SMALLER",   "24px",   },
        { THUNAR_ICON_SIZE_32,   "THUNAR_ICON_SIZE_SMALL",     "32px",   },
        { THUNAR_ICON_SIZE_48,   "THUNAR_ICON_SIZE_NORMAL",    "48px",   },
        { THUNAR_ICON_SIZE_64,   "THUNAR_ICON_SIZE_LARGE",     "64px",   },
        { THUNAR_ICON_SIZE_96,   "THUNAR_ICON_SIZE_LARGER",    "96px",   },
        { THUNAR_ICON_SIZE_128,  "THUNAR_ICON_SIZE_LARGEST",   "128px",  },
        /* g_value_transform will pick the last value if nothing else matches. So we put the default there */
        /* this is required here, because the names of the enum values have changed since the previous thunar-version*/
        { THUNAR_ICON_SIZE_48,   "*",                          "*",      },
        { 0,                     NULL,                         NULL,     },
      };

      type = g_enum_register_static (I_("ThunarIconSize"), values);

      /* register transformation function for ThunarIconSize->ThunarThumbnailSize */
      g_value_register_transform_func (type, THUNAR_TYPE_THUMBNAIL_SIZE, thunar_thumbnail_size_from_icon_size);
    }

  return type;
}



GType
thunar_recursive_permissions_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_RECURSIVE_PERMISSIONS_ASK,    "THUNAR_RECURSIVE_PERMISSIONS_ASK",    "ask",    },
        { THUNAR_RECURSIVE_PERMISSIONS_ALWAYS, "THUNAR_RECURSIVE_PERMISSIONS_ALWAYS", "always", },
        { THUNAR_RECURSIVE_PERMISSIONS_NEVER,  "THUNAR_RECURSIVE_PERMISSIONS_NEVER",  "never",  },
        { 0,                                   NULL,                                  NULL,     },
      };

      type = g_enum_register_static (I_("ThunarRecursivePermissions"), values);
    }

  return type;
}



GType
thunar_zoom_level_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_ZOOM_LEVEL_25_PERCENT,  "THUNAR_ZOOM_LEVEL_25_PERCENT",    "25%",  },
        { THUNAR_ZOOM_LEVEL_38_PERCENT,  "THUNAR_ZOOM_LEVEL_38_PERCENT",    "38%",  },
        { THUNAR_ZOOM_LEVEL_50_PERCENT,  "THUNAR_ZOOM_LEVEL_50_PERCENT",    "50%",  },
        { THUNAR_ZOOM_LEVEL_75_PERCENT,  "THUNAR_ZOOM_LEVEL_75_PERCENT",    "75%",  },
        { THUNAR_ZOOM_LEVEL_100_PERCENT, "THUNAR_ZOOM_LEVEL_100_PERCENT",   "100%", },
        { THUNAR_ZOOM_LEVEL_150_PERCENT, "THUNAR_ZOOM_LEVEL_150_PERCENT",   "150%", },
        { THUNAR_ZOOM_LEVEL_200_PERCENT, "THUNAR_ZOOM_LEVEL_200_PERCENT",   "200%", },
        { THUNAR_ZOOM_LEVEL_250_PERCENT, "THUNAR_ZOOM_LEVEL_250_PERCENT",   "250%", },
        { THUNAR_ZOOM_LEVEL_300_PERCENT, "THUNAR_ZOOM_LEVEL_300_PERCENT",   "300%", },
        { THUNAR_ZOOM_LEVEL_400_PERCENT, "THUNAR_ZOOM_LEVEL_400_PERCENT",   "400%", },
        /* Support of old type-strings for two thunar stable releases. Old strings will be transformed to new ones on write*/
        { THUNAR_ZOOM_LEVEL_25_PERCENT,  "THUNAR_ZOOM_LEVEL_SMALLEST",      "25%",  },
        { THUNAR_ZOOM_LEVEL_38_PERCENT,  "THUNAR_ZOOM_LEVEL_SMALLER",       "38%",  },
        { THUNAR_ZOOM_LEVEL_50_PERCENT,  "THUNAR_ZOOM_LEVEL_SMALL",         "50%",  },
        { THUNAR_ZOOM_LEVEL_75_PERCENT,  "THUNAR_ZOOM_LEVEL_NORMAL",        "75%",  },
        { THUNAR_ZOOM_LEVEL_100_PERCENT, "THUNAR_ZOOM_LEVEL_LARGE",         "100%", },
        { THUNAR_ZOOM_LEVEL_150_PERCENT, "THUNAR_ZOOM_LEVEL_LARGER",        "150%", },
        { THUNAR_ZOOM_LEVEL_200_PERCENT, "THUNAR_ZOOM_LEVEL_LARGEST",       "200%", },
        /* g_value_transform will pick the last value if nothing else matches. So we put the default there */
        /* this is required here, because the names of the enum values have changed since the previous thunar-version*/
        { THUNAR_ZOOM_LEVEL_100_PERCENT, "*",                               "*",    },
        { 0,                             NULL,                              NULL,   },
      };

      type = g_enum_register_static (I_("ThunarZoomLevel"), values);

      /* register transformation function for ThunarZoomLevel->ThunarIconSize */
      g_value_register_transform_func (type, THUNAR_TYPE_ICON_SIZE, thunar_icon_size_from_zoom_level);
    }

  return type;
}



ThunarThumbnailSize
thunar_zoom_level_to_thumbnail_size (ThunarZoomLevel zoom_level)
{
  ThunarIconSize icon_size = thunar_zoom_level_to_icon_size (zoom_level);
  return thunar_icon_size_to_thumbnail_size (icon_size);
}



GType
thunar_thumbnail_mode_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_THUMBNAIL_MODE_NEVER,      "THUNAR_THUMBNAIL_MODE_NEVER",      "never",      },
        { THUNAR_THUMBNAIL_MODE_ONLY_LOCAL, "THUNAR_THUMBNAIL_MODE_ONLY_LOCAL", "only-local", },
        { THUNAR_THUMBNAIL_MODE_ALWAYS,     "THUNAR_THUMBNAIL_MODE_ALWAYS",     "always",     },
        { 0,                                NULL,                               NULL,         },
      };

      type = g_enum_register_static (I_("ThunarThumbnailMode"), values);
    }

  return type;
}



GType
thunar_thumbnail_size_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_THUMBNAIL_SIZE_NORMAL,  "THUNAR_THUMBNAIL_SIZE_NORMAL", "normal", },
        { THUNAR_THUMBNAIL_SIZE_LARGE,   "THUNAR_THUMBNAIL_SIZE_LARGE",  "large",  },
        { 0,                             NULL,                           NULL,     },
      };

      type = g_enum_register_static (I_("ThunarThumbnailSize"), values);
    }

  return type;
}

const char*
thunar_thumbnail_size_get_nick (ThunarThumbnailSize thumbnail_size)
{
  GEnumValue *thumbnail_size_enum_value;

  thumbnail_size_enum_value = g_enum_get_value (g_type_class_ref (THUNAR_TYPE_THUMBNAIL_SIZE), thumbnail_size);
  return thumbnail_size_enum_value->value_nick;
}



GType
thunar_parallel_copy_mode_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_PARALLEL_COPY_MODE_ALWAYS,                  "THUNAR_PARALLEL_COPY_MODE_ALWAYS",                  "always",                  },
        { THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL,              "THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL",              "only-local",              },
        { THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES, "THUNAR_PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES", "only-local-same-devices", },
        { THUNAR_PARALLEL_COPY_MODE_NEVER,                   "THUNAR_PARALLEL_COPY_MODE_NEVER",                   "never",                   },
        { 0,                                                 NULL,                                                NULL,                      },
      };

      type = g_enum_register_static (I_("ThunarParallelCopyMode"), values);
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
static ThunarIconSize
thunar_zoom_level_to_icon_size (ThunarZoomLevel zoom_level)
{
  switch (zoom_level)
    {
    case THUNAR_ZOOM_LEVEL_25_PERCENT:  return THUNAR_ICON_SIZE_16;
    case THUNAR_ZOOM_LEVEL_38_PERCENT:  return THUNAR_ICON_SIZE_24;
    case THUNAR_ZOOM_LEVEL_50_PERCENT:  return THUNAR_ICON_SIZE_32;
    case THUNAR_ZOOM_LEVEL_75_PERCENT:  return THUNAR_ICON_SIZE_48;
    case THUNAR_ZOOM_LEVEL_100_PERCENT: return THUNAR_ICON_SIZE_64;
    case THUNAR_ZOOM_LEVEL_150_PERCENT: return THUNAR_ICON_SIZE_96;
    case THUNAR_ZOOM_LEVEL_200_PERCENT: return THUNAR_ICON_SIZE_128;
    case THUNAR_ZOOM_LEVEL_250_PERCENT: return THUNAR_ICON_SIZE_160;
    case THUNAR_ZOOM_LEVEL_300_PERCENT: return THUNAR_ICON_SIZE_192;
    case THUNAR_ZOOM_LEVEL_400_PERCENT: return THUNAR_ICON_SIZE_256;
    default:                            return THUNAR_ICON_SIZE_64; // default = 100 %zoom
    }
}



static ThunarThumbnailSize
thunar_icon_size_to_thumbnail_size (ThunarIconSize icon_size)
{
  if (icon_size > THUNAR_ICON_SIZE_128)
    return THUNAR_THUMBNAIL_SIZE_LARGE;

  return THUNAR_THUMBNAIL_SIZE_NORMAL;
}



static void
thunar_icon_size_from_zoom_level (const GValue *src_value,
                                  GValue       *dst_value)
{
  g_value_set_enum (dst_value, thunar_zoom_level_to_icon_size (g_value_get_enum (src_value)));
}



static void
thunar_thumbnail_size_from_icon_size (const GValue *src_value,
                                      GValue       *dst_value)
{
  g_value_set_enum (dst_value, thunar_icon_size_to_thumbnail_size (g_value_get_enum (src_value)));
}



GType
thunar_job_response_get_type (void)
{
	static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
	    static const GFlagsValue values[] =
      {
	      { THUNAR_JOB_RESPONSE_YES,         "THUNAR_JOB_RESPONSE_YES",         "yes"         },
	      { THUNAR_JOB_RESPONSE_YES_ALL,     "THUNAR_JOB_RESPONSE_YES_ALL",     "yes-all"     },
	      { THUNAR_JOB_RESPONSE_NO,          "THUNAR_JOB_RESPONSE_NO",          "no"          },
	      { THUNAR_JOB_RESPONSE_CANCEL,      "THUNAR_JOB_RESPONSE_CANCEL",      "cancel"      },
	      { THUNAR_JOB_RESPONSE_NO_ALL,      "THUNAR_JOB_RESPONSE_NO_ALL",      "no-all"      },
	      { THUNAR_JOB_RESPONSE_RETRY,       "THUNAR_JOB_RESPONSE_RETRY",       "retry"       },
	      { THUNAR_JOB_RESPONSE_FORCE,       "THUNAR_JOB_RESPONSE_FORCE",       "force"       },
	      { THUNAR_JOB_RESPONSE_REPLACE,     "THUNAR_JOB_RESPONSE_REPLACE",     "replace"     },
	      { THUNAR_JOB_RESPONSE_REPLACE_ALL, "THUNAR_JOB_RESPONSE_REPLACE_ALL", "replace-all" },
	      { THUNAR_JOB_RESPONSE_SKIP,        "THUNAR_JOB_RESPONSE_SKIP",        "skip"        },
	      { THUNAR_JOB_RESPONSE_SKIP_ALL,    "THUNAR_JOB_RESPONSE_SKIP_ALL",    "skip-all"    },
	      { THUNAR_JOB_RESPONSE_RENAME,      "THUNAR_JOB_RESPONSE_RENAME",      "rename"      },
	      { THUNAR_JOB_RESPONSE_RENAME_ALL,  "THUNAR_JOB_RESPONSE_RENAME_ALL",  "rename-all " },
	      { 0,                               NULL,                              NULL          }
	    };

	    type = g_flags_register_static (I_("ThunarJobResponse"), values);
    }

	return type;
}



GType
thunar_file_mode_get_type (void)
{
	static GType type = G_TYPE_INVALID;

	if (type == G_TYPE_INVALID)
    {
	    static const GFlagsValue values[] =
      {
	      { THUNAR_FILE_MODE_SUID,      "THUNAR_FILE_MODE_SUID",      "suid"      },
	      { THUNAR_FILE_MODE_SGID,      "THUNAR_FILE_MODE_SGID",      "sgid"      },
	      { THUNAR_FILE_MODE_STICKY,    "THUNAR_FILE_MODE_STICKY",    "sticky"    },
	      { THUNAR_FILE_MODE_USR_ALL,   "THUNAR_FILE_MODE_USR_ALL",   "usr-all"   },
	      { THUNAR_FILE_MODE_USR_READ,  "THUNAR_FILE_MODE_USR_READ",  "usr-read"  },
	      { THUNAR_FILE_MODE_USR_WRITE, "THUNAR_FILE_MODE_USR_WRITE", "usr-write" },
	      { THUNAR_FILE_MODE_USR_EXEC,  "THUNAR_FILE_MODE_USR_EXEC",  "usr-exec"  },
	      { THUNAR_FILE_MODE_GRP_ALL,   "THUNAR_FILE_MODE_GRP_ALL",   "grp-all"   },
	      { THUNAR_FILE_MODE_GRP_READ,  "THUNAR_FILE_MODE_GRP_READ",  "grp-read"  },
	      { THUNAR_FILE_MODE_GRP_WRITE, "THUNAR_FILE_MODE_GRP_WRITE", "grp-write" },
	      { THUNAR_FILE_MODE_GRP_EXEC,  "THUNAR_FILE_MODE_GRP_EXEC",  "grp-exec"  },
	      { THUNAR_FILE_MODE_OTH_ALL,   "THUNAR_FILE_MODE_OTH_ALL",   "oth-all"   },
	      { THUNAR_FILE_MODE_OTH_READ,  "THUNAR_FILE_MODE_OTH_READ",  "oth-read"  },
	      { THUNAR_FILE_MODE_OTH_WRITE, "THUNAR_FILE_MODE_OTH_WRITE", "oth-write" },
	      { THUNAR_FILE_MODE_OTH_EXEC,  "THUNAR_FILE_MODE_OTH_EXEC",  "oth-exec"  },
	      { 0,                          NULL,                         NULL        }
	    };

      type = g_flags_register_static ("ThunarFileMode", values);
    }
	return type;
}
