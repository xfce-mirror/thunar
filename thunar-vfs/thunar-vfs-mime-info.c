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

#include <thunar-vfs/thunar-vfs-mime-info.h>
#include <thunar-vfs/thunar-vfs-mime-parser.h>



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



static void thunar_vfs_mime_info_register_type      (GType                  *type);
static void thunar_vfs_mime_info_class_init         (ThunarVfsMimeInfoClass *klass);
static void thunar_vfs_mime_info_finalize           (ExoObject              *object);
static void thunar_vfs_mime_info_icon_theme_changed (GtkIconTheme           *icon_theme,
                                                     ThunarVfsMimeInfo      *info);





static ExoObjectClass *thunar_vfs_mime_info_parent_class;



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
        {
          /* we handle 'application/x-extension-<EXT>' special here */
          if (G_UNLIKELY (strncmp (info->name, "application/x-extension-", 24) == 0))
            info->comment = g_strdup_printf (_("%s document"), info->name + 24);
          else
            info->comment = info->name;
        }
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
 * thunar_vfs_mime_info_hash:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Calculates a hash value for @info.
 *
 * Return value: a hash value for @info.
 **/
guint
thunar_vfs_mime_info_hash (gconstpointer info)
{
  const gchar *p;
  guint        h;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), 0);

  for (h = THUNAR_VFS_MIME_INFO (info)->name[0], p = THUNAR_VFS_MIME_INFO (info)->name + 1; *p != '\0'; ++p)
    h = (h << 5) - h + *p;

  return h;
}



/**
 * thunar_vfs_mime_info_equal:
 * @a : a #ThunarVfsMimeInfo.
 * @b : a #ThunarVfsMimeInfo.
 *
 * Compares @a and @b and returns %TRUE if both
 * are equal. 
 *
 * Return value: %TRUE if @a and @b are equal.
 **/
gboolean
thunar_vfs_mime_info_equal (gconstpointer a,
                            gconstpointer b)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (a), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (b), FALSE);

  return (a == b) || G_UNLIKELY (strcmp (THUNAR_VFS_MIME_INFO (a)->name, THUNAR_VFS_MIME_INFO (b)->name) == 0);
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
 * thunar_vfs_mime_info_list_free:
 * @info_list : a #GList of #ThunarVfsMimeInfo<!---->s
 *
 * Frees the list and all #ThunarVfsMimeInfo<!---->s
 * contained within the list.
 **/
void
thunar_vfs_mime_info_list_free (GList *info_list)
{
  g_list_foreach (info_list, (GFunc) thunar_vfs_mime_info_unref, NULL);
  g_list_free (info_list);
}



