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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-mime.h>
#include <thunar-vfs/thunar-vfs-mime-parser.h>

#include <xdgmime/xdgmime.h>



static const struct
{
  const gchar *const type;
  const gchar *const icon;
} GNOME_ICONNAMES[] =
{
  { "inode/directory", "gnome-fs-directory" },
  { "inode/blockdevice", "gnome-fs-blockdev" },
  { "inode/chardevice", "gnome-fs-chardev" },
  { "inode/fifo", "gnome-fs-fifo" },
  { "inode/socket", "gnome-fs-socket" },
};



static void               thunar_vfs_mime_info_register_type      (GType                  *type);
static void               thunar_vfs_mime_info_class_init         (ThunarVfsMimeInfoClass *klass);
static void               thunar_vfs_mime_info_finalize           (ExoObject              *object);
static ThunarVfsMimeInfo *thunar_vfs_mime_info_get_unlocked       (const gchar            *name);
static void               thunar_vfs_mime_info_icon_theme_changed (GtkIconTheme           *icon_theme,
                                                                   ThunarVfsMimeInfo      *info);



struct _ThunarVfsMimeInfoClass
{
  ExoObjectClass __parent__;
};

struct _ThunarVfsMimeInfo
{
  ExoObject __parent__;

  gchar        *comment;
  gchar        *name;

  const gchar  *media;
  const gchar  *subtype;

  gchar        *icon_name;
  gboolean      icon_name_static : 1;
  GtkIconTheme *icon_theme;
};



static ExoObjectClass *thunar_vfs_mime_info_parent_class;
static GHashTable     *mime_infos = NULL;
G_LOCK_DEFINE_STATIC (mime_infos);



GType
thunar_vfs_mime_info_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_mime_info_register_type, &type);

  return type;
}



static void
thunar_vfs_mime_info_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsMimeInfoClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_mime_info_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsMimeInfo),
    128u,
    NULL,
    NULL,
  };

  *type = g_type_register_static (EXO_TYPE_OBJECT, "ThunarVfsMimeInfo", &info, 0);
}



static void
thunar_vfs_mime_info_class_init (ThunarVfsMimeInfoClass *klass)
{
  ExoObjectClass *exoobject_class;

  thunar_vfs_mime_info_parent_class = g_type_class_peek_parent (klass);

  exoobject_class = EXO_OBJECT_CLASS (klass);
  exoobject_class->finalize = thunar_vfs_mime_info_finalize;
}



static void
thunar_vfs_mime_info_finalize (ExoObject *object)
{
  ThunarVfsMimeInfo *info = THUNAR_VFS_MIME_INFO (object);

  g_return_if_fail (THUNAR_VFS_IS_MIME_INFO (info));

  /* free the comment */
  if (info->comment != NULL && info->comment != info->name)
    {
#ifndef G_DISABLE_CHECKS
      memset (info->comment, 0xaa, strlen (info->comment) + 1);
#endif
      g_free (info->comment);
    }

  /* free the name */
#ifndef G_DISABLE_CHECKS
  if (G_LIKELY (info->name != NULL))
    memset (info->name, 0xaa, strlen (info->name) + 1);
#endif
  g_free (info->name);

  /* disconnect from the icon theme (if any) */
  if (G_LIKELY (info->icon_theme != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (info->icon_theme), thunar_vfs_mime_info_icon_theme_changed, info);
      g_object_unref (G_OBJECT (info->icon_theme));
    }

  /* free the icon name if it isn't one of the statics */
  if (G_LIKELY (!info->icon_name_static && info->icon_name != NULL))
    {
#ifndef G_DISABLE_CHECKS
      memset (info->icon_name, 0xaa, strlen (info->icon_name) + 1);
#endif
      g_free (info->icon_name);
    }

  /* invoke the parent's finalize method */
  (*EXO_OBJECT_CLASS (thunar_vfs_mime_info_parent_class)->finalize) (object);
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_info_get_unlocked (const gchar *name)
{
  ThunarVfsMimeInfo *info;
  const gchar       *s;
  const gchar       *t;
  const gchar       *u;
  gchar             *v;

  /* check if we have a cached version of the mime type */
  info = g_hash_table_lookup (mime_infos, name);
  if (info == NULL)
    {
      for (s = NULL, t = name; *t != '\0'; ++t)
        if (G_UNLIKELY (*t == '/'))
          s = t;

      /* we could do better here... */
      if (G_UNLIKELY (s == NULL))
        g_error ("Invalid MIME type '%s'", name);

      /* allocate the MIME info instance */
      info = exo_object_new (THUNAR_VFS_TYPE_MIME_INFO);

      /* allocate memory to store both the full name,
       * as well as the media type alone.
       */
      info->name = g_new (gchar, (t - name) + (s - name) + 2);

      /* copy full name (including the terminator) */
      for (u = name, v = info->name; u <= t; ++u, ++v)
        *v = *u;

      /* set the subtype portion */
      info->subtype = info->name + (s - name) + 1;

      /* copy the media portion */
      info->media = v;
      for (u = name; u < s; ++u, ++v)
        *v = *u;

      /* terminate the media portion */
      *v = '\0';

      /* insert the mime type into the cache */
      g_hash_table_insert (mime_infos, info->name, info);
    }

  /* take a reference for the caller */
  return thunar_vfs_mime_info_ref (info);
}



static void
thunar_vfs_mime_info_icon_theme_changed (GtkIconTheme      *icon_theme,
                                         ThunarVfsMimeInfo *info)
{
  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  g_return_if_fail (THUNAR_VFS_IS_MIME_INFO (info));
  g_return_if_fail (info->icon_theme == icon_theme);

  /* drop the cached icon name, so the next lookup
   * call will perform a lookup again.
   */
  if (G_LIKELY (!info->icon_name_static))
    {
      g_free (info->icon_name);
      info->icon_name = NULL;
    }
}



/**
 * thunar_vfs_mime_info_get_comment:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Determines the description for the given @info.
 *
 * Note that this method MUST NOT be called from threads other than
 * the main thread, because it's not thread-safe!
 *
 * Return value: the comment associated with the @info or the empty string
 *               if no comment was provided.
 */
const gchar*
thunar_vfs_mime_info_get_comment (ThunarVfsMimeInfo *info)
{
  gchar *path;
  gchar *spec;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);

  if (G_UNLIKELY (info->comment == NULL))
    {
      spec = g_strdup_printf ("mime/%s.xml", info->name);
      path = xfce_resource_lookup (XFCE_RESOURCE_DATA, spec);
      g_free (spec);

      if (G_LIKELY (path != NULL))
        {
          info->comment = _thunar_vfs_mime_parser_load_comment_from_file (path, NULL);
          g_free (path);
        }

      if (G_UNLIKELY (info->comment == NULL))
        info->comment = info->name;
    }

  return info->comment;
}



/**
 * thunar_vfs_mime_info_get_name:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the full qualified name of the MIME type
 * described by the @info object.
 *
 * Return value: the name of @info.
 **/
const gchar*
thunar_vfs_mime_info_get_name (const ThunarVfsMimeInfo *info)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);
  return info->name;
}



/**
 * thunar_vfs_mime_info_get_media:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the media portion of the MIME type, e.g. if your
 * #ThunarVfsMimeInfo instance refers to "text/plain", invoking
 * this method will return "text".
 *
 * Return value: the media portion of the MIME type.
 **/
const gchar*
thunar_vfs_mime_info_get_media (const ThunarVfsMimeInfo *info)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);
  return info->media;
}



/**
 * thunar_vfs_mime_info_get_subtype:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the subtype portion of the MIME type, e.g. if @info
 * refers to "application/octect-stream", this method will
 * return "octect-stream".
 *
 * Return value: the subtype portion of @info.
 **/
const gchar*
thunar_vfs_mime_info_get_subtype (const ThunarVfsMimeInfo *info)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);
  return info->subtype;
}



/**
 * thunar_vfs_mime_info_lookup_icon_name:
 * @info       : a #ThunarVfsMimeInfo.
 * @icon_theme : the #GtkIconTheme on which to perform the lookup.
 *
 * Tries to determine the name of a suitable icon for @info
 * in @icon_theme. The returned icon name can then be used
 * in calls to #gtk_icon_theme_lookup_icon() or
 * #gtk_icon_theme_load_icon().
 *
 * Note that this method MUST NOT be called from threads other than
 * the main thread, because it's not thread-safe!
 *
 * Return value: a suitable icon name for @info in @icon_theme.
 **/
const gchar*
thunar_vfs_mime_info_lookup_icon_name (ThunarVfsMimeInfo *info,
                                       GtkIconTheme      *icon_theme)
{
  gsize n;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* check if our cached name will suffice */
  if (G_LIKELY (info->icon_theme == icon_theme && info->icon_name != NULL))
    return info->icon_name;

  /* if we have a new icon theme, connect to the new one */
  if (G_UNLIKELY (info->icon_theme != icon_theme))
    {
      /* disconnect from the previous one */
      if (G_LIKELY (info->icon_theme != NULL))
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (info->icon_theme), thunar_vfs_mime_info_icon_theme_changed, info);
          g_object_unref (G_OBJECT (info->icon_theme));
        }

      /* connect to the new one */
      info->icon_theme = icon_theme;
      g_object_ref (G_OBJECT (icon_theme));
      g_signal_connect (G_OBJECT (icon_theme), "changed", G_CALLBACK (thunar_vfs_mime_info_icon_theme_changed), info);
    }

  /* free the previously set icon name */
  if (!info->icon_name_static)
    g_free (info->icon_name);

  /* start out with the full name (assuming a non-static icon_name) */
  info->icon_name = g_strdup_printf ("gnome-mime-%s-%s", info->media, info->subtype);
  info->icon_name_static = FALSE;
  if (!gtk_icon_theme_has_icon (icon_theme, info->icon_name))
    {
      /* only the media portion */
      info->icon_name[11 + ((info->subtype - 1) - info->name)] = '\0';
      if (!gtk_icon_theme_has_icon (icon_theme, info->icon_name))
        {
          /* if we get here, we'll use a static icon name */
          info->icon_name_static = TRUE;
          g_free (info->icon_name);

          /* GNOME uses non-standard names for special MIME types */
          for (n = 0; n < G_N_ELEMENTS (GNOME_ICONNAMES); ++n)
            if (exo_str_is_equal (info->name, GNOME_ICONNAMES[n].type))
              if (gtk_icon_theme_has_icon (icon_theme, GNOME_ICONNAMES[n].icon))
                {
                  info->icon_name = (gchar *) GNOME_ICONNAMES[n].icon;
                  break;
                }

          /* fallback is always application/octect-stream */
          if (n == G_N_ELEMENTS (GNOME_ICONNAMES))
            info->icon_name = (gchar *) "gnome-mime-application-octect-stream";
        }
    }

  g_assert (info->icon_name != NULL);
  g_assert (info->icon_name[0] != '\0');

  return info->icon_name;
}





/**
 * _thunar_vfs_mime_init:
 *
 * Initializes the MIME component of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_mime_init (void)
{
  g_return_if_fail (mime_infos == NULL);

  /* initialize the XDG mime library */
  xdg_mime_init ();

  /* allocate a string chunk for the mime types */
  mime_infos = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) thunar_vfs_mime_info_unref);
}



/**
 * thunar_vfs_mime_info_get:
 * @mime_type : the string representation of a mime type.
 *
 * Returns the #ThunarVfsMimeInfo corresponding to 
 * @mime_type.
 *
 * The caller is responsible to free the returned object
 * using #thunar_vfs_mime_info_unref().
 *
 * Return value: the #ThunarVfsMimeInfo for @mime_type.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_info_get (const gchar *mime_type)
{
  ThunarVfsMimeInfo *info;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (strchr (mime_type, '/') != NULL, NULL);
  g_return_val_if_fail (strchr (mime_type, '/') == strrchr (mime_type, '/'), NULL);

  G_LOCK (mime_infos);
  info = thunar_vfs_mime_info_get_unlocked (mime_type);
  G_UNLOCK (mime_infos);

  return info;
}


 
/**
 * thunar_vfs_mime_info_get_for_file:
 * @path : the path to a local file.
 *
 * Determines the #ThunarVfsMimeInfo of the file referred to
 * by @path.
 *
 * The caller is responsible to free the returned object
 * using #thunar_vfs_mime_info_unref().
 *
 * Return value: the #ThunarVfsMimeInfo for @path.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_info_get_for_file (const gchar *path)
{
  ThunarVfsMimeInfo *info;
  const gchar       *name;

  g_return_val_if_fail (g_path_is_absolute (path), NULL);
  g_return_val_if_fail (mime_infos != NULL, NULL);

  G_LOCK (mime_infos);
  name = xdg_mime_get_mime_type_for_file (path);
  info = thunar_vfs_mime_info_get_unlocked (name);
  G_UNLOCK (mime_infos);

  return info;
}



