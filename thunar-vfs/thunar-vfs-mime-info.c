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

/* implement thunar-vfs-mime-info's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __THUNAR_VFS_MIME_INFO_C__
#include <thunar-vfs/thunar-vfs-mime-info.h>

#include <thunar-vfs/thunar-vfs-mime-parser.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* special mime type to gnome icon mapping */
static const struct
{
  const gchar *const type;
  const gchar *const icon;
} GNOME_ICONNAMES[] =
{
  { "inode/blockdevice", "gnome-fs-blockdev" },
  { "inode/chardevice", "gnome-fs-chardev" },
  { "inode/directory", "gnome-fs-directory" },
  { "inode/fifo", "gnome-fs-fifo" },
  { "inode/socket", "gnome-fs-socket" },
};

/* static constant media-only gnome icon names
 * (shared by all mime types that don't have
 * a more specific icon).
 */
static const gchar * const GNOME_MEDIAICONS[] =
{
  "gnome-mime-application",
  "gnome-mime-audio",
  "gnome-mime-image",
  "gnome-mime-text",
  "gnome-mime-video",
};

/* fallback gnome icon name */
static const gchar GNOME_MIME_APPLICATION_OCTET_STREAM[] = "gnome-mime-application-octet-stream";



/* Checks whether info has a static constant icon name, that doesn't
 * need to be freed using g_free().
 */
static inline gboolean
thunar_vfs_mime_info_is_static_icon_name (const ThunarVfsMimeInfo *info)
{
  guint n;
  for (n = 0; n < G_N_ELEMENTS (GNOME_ICONNAMES); ++n)
    if (info->icon_name == GNOME_ICONNAMES[n].icon)
      return TRUE;
  for (n = 0; n < G_N_ELEMENTS (GNOME_MEDIAICONS); ++n)
    if (info->icon_name == GNOME_MEDIAICONS[n])
      return TRUE;
  return (info->icon_name == GNOME_MIME_APPLICATION_OCTET_STREAM);
}



GType
thunar_vfs_mime_info_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarVfsMimeInfo"),
                                           (GBoxedCopyFunc) thunar_vfs_mime_info_ref,
                                           (GBoxedFreeFunc) thunar_vfs_mime_info_unref);
    }

  return type;
}



/**
 * thunar_vfs_mime_info_new:
 * @name : the mime type name.
 * @len  : the length of @name or %-1 if zero-terminated.
 *
 * Allocates a new #ThunarVfsMimeInfo object with an
 * initial reference count of one and sets it to the
 * given @name.
 *
 * Note that no checking is performed on the given @name.
 * You should not normally use this function, but use
 * thunar_vfs_mime_database_get_info() instead.
 *
 * In addition, if you allocate #ThunarVfsMimeInfo<!---->s
 * using this function, you cannot mix them with the objects
 * allocated in a #ThunarVfsMimeDatabase, because the
 * #ThunarVfsMimeDatabase and associated functions assume
 * that #ThunarVfsMimeInfo objects are unique.
 *
 * Return value: the newly allocated #ThunarVfsMimeInfo.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_info_new (const gchar *name,
                          gssize       len)
{
  ThunarVfsMimeInfo *info;

  if (G_UNLIKELY (len < 0))
    len = strlen (name);

  /* allocate the new object */
  info = _thunar_vfs_slice_alloc (sizeof (*info) + len + 1);
  info->ref_count = 1;
  info->comment = NULL;
  info->icon_name = NULL;

  /* set the name */
  memcpy (((gchar *) info) + sizeof (*info), name, len + 1);

  return info;
}



/**
 * thunar_vfs_mime_info_unref:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Decrements the reference count on @info and releases
 * the resources allocated for @info once the reference
 * count drops to zero.
 **/
void
thunar_vfs_mime_info_unref (ThunarVfsMimeInfo *info)
{
  if (exo_atomic_dec (&info->ref_count))
    {
      /* free the comment */
      if (info->comment != NULL && info->comment != thunar_vfs_mime_info_get_name (info))
        {
#ifdef G_ENABLE_DEBUG
          memset (info->comment, 0xaa, strlen (info->comment) + 1);
#endif
          g_free (info->comment);
        }

      /* free the icon name if it isn't one of the statics */
      if (G_LIKELY (!thunar_vfs_mime_info_is_static_icon_name (info)))
        {
#ifdef G_ENABLE_DEBUG
          if (G_LIKELY (info->icon_name != NULL))
            memset (info->icon_name, 0xaa, strlen (info->icon_name) + 1);
#endif
          g_free (info->icon_name);
        }

      /* free the info struct (need to determine the block size for the slice allocator) */
      _thunar_vfs_slice_free1 (sizeof (*info) + strlen (((const gchar *) info) + sizeof (*info)) + 1, info);
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
  const gchar *name;
  gchar       *path;
  gchar       *spec;

  if (G_UNLIKELY (info->comment == NULL))
    {
      name = thunar_vfs_mime_info_get_name (info);
      spec = g_strdup_printf ("mime/%s.xml", name);
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
          if (G_UNLIKELY (strncmp (name, "application/x-extension-", 24) == 0))
            info->comment = g_strdup_printf (_("%s document"), name + 24);
          else
            info->comment = (gchar *) name;
        }
    }

  return info->comment;
}



/**
 * thunar_vfs_mime_info_get_media:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the media portion of the MIME type, e.g. if your
 * #ThunarVfsMimeInfo instance refers to "text/plain", invoking
 * this method will return "text".
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the media portion of the MIME type.
 **/
gchar*
thunar_vfs_mime_info_get_media (const ThunarVfsMimeInfo *info)
{
  const gchar *name;
  const gchar *p;

  /* lookup the slash character */
  name = thunar_vfs_mime_info_get_name (info);
  for (p = name; *p != '/' && *p != '\0'; ++p)
    ;

  return g_strndup (name, p - name);
}



/**
 * thunar_vfs_mime_info_get_subtype:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Returns the subtype portion of the MIME type, e.g. if @info
 * refers to "application/octect-stream", this method will
 * return "octect-stream".
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the subtype portion of @info.
 **/
gchar*
thunar_vfs_mime_info_get_subtype (const ThunarVfsMimeInfo *info)
{
  const gchar *name;
  const gchar *p;

  /* lookup the slash character */
  name = thunar_vfs_mime_info_get_name (info);
  for (p = name; *p != '/' && *p != '\0'; ++p)
    ;

  /* skip the slash character */
  if (G_LIKELY (*p == '/'))
    ++p;

  return g_strdup (p);
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
  const gchar *name;
  guint        h;

  name = thunar_vfs_mime_info_get_name (info);
  for (h = *name; *++name != '\0'; )
    h = (h << 5) - h + *name;

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
  const gchar *a_name = thunar_vfs_mime_info_get_name (a);
  const gchar *b_name = thunar_vfs_mime_info_get_name (b);

  return (a == b) || G_UNLIKELY (strcmp (a_name, b_name) == 0);
}



/**
 * thunar_vfs_mime_info_lookup_icon_name:
 * @info       : a #ThunarVfsMimeInfo.
 * @icon_theme : the #GtkIconTheme on which to perform the lookup.
 *
 * Tries to determine the name of a suitable icon for @info
 * in @icon_theme. The returned icon name can then be used
 * in calls to gtk_icon_theme_lookup_icon() or
 * gtk_icon_theme_load_icon().
 *
 * The returned icon name is owned by @info and MUST NOT be freed
 * by the caller.
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
  const gchar *subtype;
  const gchar *name;
  const gchar *p;
  gchar       *media;
  gsize        n;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* determine the icon name if we don't already have it cached */
  if (G_UNLIKELY (info->icon_name == NULL))
    {
      /* determine media and subtype */
      name = thunar_vfs_mime_info_get_name (info);
      for (p = name + 1; *p != '/' && *p != '\0'; ++p);
      media = g_newa (gchar, p - name + 1);
      memcpy (media, name, p - name);
      media[p - name] = '\0';
      subtype = G_LIKELY (*p == '/') ? p + 1 : p;

      /* start out with the full name */
      info->icon_name = g_strdup_printf ("gnome-mime-%s-%s", media, subtype);
      if (!gtk_icon_theme_has_icon (icon_theme, info->icon_name))
        {
          /* only the media portion */
          info->icon_name[11 + ((subtype - 1) - name)] = '\0';
          if (!gtk_icon_theme_has_icon (icon_theme, info->icon_name))
            {
              /* if we get here, we'll use a static icon name */
              g_free (info->icon_name);

              /* GNOME uses non-standard names for special MIME types */
              for (n = 0; n < G_N_ELEMENTS (GNOME_ICONNAMES); ++n)
                if (strcmp (name, GNOME_ICONNAMES[n].type) == 0)
                  if (gtk_icon_theme_has_icon (icon_theme, GNOME_ICONNAMES[n].icon))
                    {
                      info->icon_name = (gchar *) GNOME_ICONNAMES[n].icon;
                      break;
                    }

              /* fallback is always application/octet-stream */
              if (n == G_N_ELEMENTS (GNOME_ICONNAMES))
                info->icon_name = (gchar *) GNOME_MIME_APPLICATION_OCTET_STREAM;
            }
          else
            {
              /* check if we can use one of the static media icon names */
              for (n = 0; n < G_N_ELEMENTS (GNOME_MEDIAICONS); ++n)
                if (strcmp (info->icon_name, GNOME_MEDIAICONS[n]) == 0)
                  {
                    g_free (info->icon_name);
                    info->icon_name = (gchar *) GNOME_MEDIAICONS[n];
                    break;
                  }
            }
        }

      /* verify the icon name */
      g_assert (info->icon_name != NULL);
      g_assert (info->icon_name[0] != '\0');
    }

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



/**
 * _thunar_vfs_mime_info_invalidate_icon_name:
 * @info : a #ThunarVfsMimeInfo.
 *
 * Invalidates the cached icon name for @info.
 *
 * Note that this method MUST NOT be called from threads other than
 * the main thread, because it's not thread-safe!
 **/
void
_thunar_vfs_mime_info_invalidate_icon_name (ThunarVfsMimeInfo *info)
{
  if (!thunar_vfs_mime_info_is_static_icon_name (info))
    {
#ifdef G_ENABLE_DEBUG
      if (G_LIKELY (info->icon_name != NULL))
        memset (info->icon_name, 0xaa, strlen (info->icon_name));
#endif
      g_free (info->icon_name);
    }
  info->icon_name = NULL;
}



#define __THUNAR_VFS_MIME_INFO_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
