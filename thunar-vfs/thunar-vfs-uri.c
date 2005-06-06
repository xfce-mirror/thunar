/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
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

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-uri.h>



static void thunar_vfs_uri_class_init   (ThunarVfsURIClass  *klass);
static void thunar_vfs_uri_init         (ThunarVfsURI       *uri);
static void thunar_vfs_uri_finalize     (GObject            *object);



struct _ThunarVfsURIClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsURI
{
  GObject __parent__;

  gchar       *path;
  const gchar *name;
};



G_DEFINE_TYPE (ThunarVfsURI, thunar_vfs_uri, G_TYPE_OBJECT);



static void
thunar_vfs_uri_class_init (ThunarVfsURIClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_uri_finalize;
}



static void
thunar_vfs_uri_init (ThunarVfsURI *uri)
{
}



static void
thunar_vfs_uri_finalize (GObject *object)
{
  ThunarVfsURI *uri = THUNAR_VFS_URI (object);

  g_return_if_fail (THUNAR_VFS_IS_URI (uri));

  g_free (uri->path);

  G_OBJECT_CLASS (thunar_vfs_uri_parent_class)->finalize (object);
}



/**
 * thunar_vfs_uri_new:
 * @identifier : the resource identifier encoded as string.
 * @error      : return location for errors.
 *
 * Return value: the #ThunarVfsURI or %NULL on error.
 **/
ThunarVfsURI*
thunar_vfs_uri_new (const gchar *identifier,
                    GError     **error)
{
  ThunarVfsURI *uri;
  const gchar  *p;

  g_return_val_if_fail (identifier != NULL, NULL);

  /* allocate the URI instance */
  uri = g_object_new (THUNAR_VFS_TYPE_URI, NULL);
  uri->path = g_filename_from_uri (identifier, NULL, error);

  /* determine the basename of the path */
  for (p = uri->name = uri->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      uri->name = p + 1;

  return uri;
}



/**
 * thunar_vfs_uri_new_for_path:
 * @path  : an absolute path referring to a file accessible via the local
 *          file system implementation.
 *
 * Allocates a new #ThunarVfsURI instance that refers to the local file
 * identified by @path.
 *
 * Return value: the newly allocated #ThunarVfsURI instance.
 **/
ThunarVfsURI*
thunar_vfs_uri_new_for_path (const gchar *path)
{
  ThunarVfsURI *uri;
  const gchar  *p;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (path), NULL);

  /* allocate the URI instance */
  uri = g_object_new (THUNAR_VFS_TYPE_URI, NULL);
  uri->path = g_strdup (path);

  /* determine the basename of the path */
  for (p = uri->name = uri->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      uri->name = p + 1;

  return uri;
}



/**
 * thunar_vfs_uri_is_home:
 * @uri : an #ThunarVfsURI instance.
 *
 * Checks whether @uri refers to the home directory
 * of the current user.
 *
 * Return value: %TRUE if @uri refers to the home directory.
 **/
gboolean
thunar_vfs_uri_is_home (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);
  return exo_str_is_equal (xfce_get_homedir (), uri->path);
}



/**
 * thunar_vfs_uri_is_local:
 * @uri : an #ThunarVfsURI instance.
 *
 * Checks whether @uri refers to a local resource.
 *
 * Return value: %TRUE if the @uri refers to a local resource.
 **/
gboolean
thunar_vfs_uri_is_local (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);
  return TRUE;
}



/**
 * thunar_vfs_uri_is_root:
 * @uri : an #ThunarVfsURI instance.
 *
 * Checks whether @uri refers to the root of the file system
 * scheme described by @uri.
 *
 * Return value: %TRUE if @uri is a root element, else %FALSE.
 **/
gboolean
thunar_vfs_uri_is_root (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);
  return (uri->path[0] == '/' && uri->path[1] == '\0');
}



/**
 * thunar_vfs_uri_get_display_name:
 * @uri : an #ThunarVfsURI instance.
 *
 * Returns a displayable version of the @uri's base
 * name in UTF-8 encoding, which is suitable for
 * display in the user interface.
 *
 * The returned string must be freed using #g_free()
 * when no longer needed.
 *
 * Return value: a displayable version of the @uri's base name.
 **/
gchar*
thunar_vfs_uri_get_display_name (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  // FIXME: This should be optimized (and GLib 2.4 compatible)!
  return g_filename_display_basename (uri->path);
}



/**
 * thunar_vfs_uri_get_name:
 * @uri : an #ThunarVfsURI instance.
 *
 * Returns the basename of @uri.
 *
 * Return value: the basename of @uri.
 **/
const gchar*
thunar_vfs_uri_get_name (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  return uri->name;
}



/**
 * thunar_vfs_uri_get_path:
 * @uri : an #ThunarVfsURI instance.
 *
 * Returns the path component of the given @uri.
 *
 * Return value: the path component of @uri.
 **/
const gchar*
thunar_vfs_uri_get_path (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  return uri->path;
}



/**
 * thunar_vfs_uri_parent:
 * @uri : an #ThunarVfsURI instance.
 *
 * Returns the #ThunarVfsURI object that refers to the parent 
 * folder of @uri or %NULL if @uri has no parent.
 *
 * Return value:
 **/
ThunarVfsURI*
thunar_vfs_uri_parent (ThunarVfsURI *uri)
{
  ThunarVfsURI *parent;
  const gchar  *p;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  if (thunar_vfs_uri_is_root (uri))
    return NULL;

  /* allocate the new object */
  parent = g_object_new (THUNAR_VFS_TYPE_URI, NULL);
  parent->path = g_path_get_dirname (uri->path);

  /* determine the basename of the path */
  for (p = parent->name = parent->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      parent->name = p + 1;

  return parent;
}



/**
 * thunar_vfs_uri_relative:
 * @uri   : an #ThunarVfsURI instance.
 * @name  : the relative name.
 *
 * Return value:
 **/
ThunarVfsURI*
thunar_vfs_uri_relative (ThunarVfsURI *uri,
                         const gchar  *name)
{
  ThunarVfsURI *relative;
  const gchar  *p;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* allocate the new object */
  relative = g_object_new (THUNAR_VFS_TYPE_URI, NULL);
  relative->path = g_build_filename (uri->path, name, NULL);

  /* determine the basename of the path */
  for (p = relative->name = relative->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      relative->name = p + 1;

  return relative;
}



/**
 * thunar_vfs_uri_hash:
 * @uri : a #ThunarVfsURI.
 *
 * Calculates a hash value for the given @uri.
 *
 * Return value: a hash value for @uri.
 **/
guint
thunar_vfs_uri_hash (gconstpointer uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), 0);
  return g_str_hash (THUNAR_VFS_URI (uri)->path);
}



/**
 * thunar_vfs_uri_equal:
 * @a : first #ThunarVfsURI.
 * @b : second #ThunarVfsURI.
 *
 * Checks whether the two URIs @a and @b refer to the same
 * resource.
 *
 * Return value: %TRUE if @a equals @b.
 **/
gboolean
thunar_vfs_uri_equal (gconstpointer a,
                      gconstpointer b)
{
  const gchar *a_name;
  const gchar *b_name;
  const gchar *a_path;
  const gchar *b_path;
  const gchar *ap;
  const gchar *bp;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (a), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (b), FALSE);

  a_name = THUNAR_VFS_URI (a)->name;
  b_name = THUNAR_VFS_URI (b)->name;

  /* compare the names first */
  for (ap = a_name, bp = b_name; *ap == *bp && *ap != '\0'; ++ap, ++bp)
    ;
  if (G_LIKELY (*ap != '\0' || *bp != '\0'))
    return FALSE;

  a_path = THUNAR_VFS_URI (a)->path;
  b_path = THUNAR_VFS_URI (b)->path;

  /* check if the dirnames have the same length */
  if (G_LIKELY ((a_name - a_path) != (b_name - b_path)))
    return FALSE;

  /* fallback to a full path comparison (excluding
   * the already compared basename)
   */
  for (ap = a_path, bp = b_path; *ap == *bp && ap < a_name; ++ap, ++bp)
    ;

  g_assert (ap != a_name || bp == b_name);
  g_assert (bp != b_name || ap == a_name);

  return (ap == a_name);
}




