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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
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
  gchar        *path;

  g_return_val_if_fail (identifier != NULL, NULL);

  /* try to parse the path */
  path = g_filename_from_uri (identifier, NULL, error);
  if (G_UNLIKELY (path == NULL))
    return NULL;

  /* allocate the URI instance */
  uri = g_object_new (THUNAR_VFS_TYPE_URI, NULL);
  uri->path = path;

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
 * Return value: the #ThunarVfsURI object referring to the parent folder
 *               or %NULL.
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
 * thunar_vfs_uri_to_string:
 * @uri : a #ThunarVfsURI.
 *
 * Returns the string representation of @uri. The
 * caller is responsible for freeing the returned
 * string using #g_free().
 *
 * Return value: the string representation of @uri.
 **/
gchar*
thunar_vfs_uri_to_string (ThunarVfsURI *uri)
{
  gchar *string;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* transform the path into a file:// uri */
  string = g_new (gchar, 7 + strlen (uri->path) + 1);
  strcpy (string, "file://");
  strcpy (string + 7, uri->path);

  return string;
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



/**
 * thunar_vfs_uri_list_from_string:
 * @string : string representation of an URI list.
 * @error  : return location for errors.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace.
 *
 * If all URIs were successfully parsed into #ThunarVfsURI
 * objects, the list of parsed URIs will be returned, and
 * you'll need to call #thunar_vfs_uri_list_free() to
 * release the list resources. Else if the parsing fails
 * at some point, %NULL will be returned and @error will
 * be set to describe the cause.
 *
 * Note, that if @string contains no URIs, this function
 * will also return %NULL, but @error won't be set. So
 * take care when checking for an error condition!
 *
 * Return value: the list of #ThunarVfsURI's or %NULL.
 **/
GList*
thunar_vfs_uri_list_from_string (const gchar *string,
                                 GError     **error)
{
  ThunarVfsURI *uri;
  const gchar  *s;
  const gchar  *t;
  GList        *uri_list = NULL;
  gchar        *identifier;

  g_return_val_if_fail (string != NULL, NULL);

  for (s = string; s != NULL; )
    {
      if (*s != '#')
        {
          while (g_ascii_isspace (*s))
            ++s;

          for (t = s; *t != '\0' && *t != '\n' && *t != '\r'; ++t)
            ;

          if (t > s)
            {
              for (t--; t > s && g_ascii_isspace (*t); t--)
                ;

              if (t > s)
                {
                  /* try to parse the URI */
                  identifier = g_strndup (s, t - s + 1);
                  uri = thunar_vfs_uri_new (identifier, error);
                  g_free (identifier);

                  /* check if we succeed */
                  if (G_UNLIKELY (uri == NULL))
                    {
                      thunar_vfs_uri_list_free (uri_list);
                      return NULL;
                    }
                  else
                    {
                      /* append the newly parsed uri */
                      uri_list = g_list_append (uri_list, uri);
                    }
                }
            }
        }

      for (; *s != '\0' && *s != '\n'; ++s)
        ;

      if (*s++ == '\0')
        break;
    }

  return uri_list;
}



/**
 * thunar_vfs_uri_list_to_string:
 * @uri_list : a list of #ThunarVfsURI<!---->s.
 *
 * Free the returned value using #g_free() when you
 * are done with it.
 *
 * Return value: the string representation of @uri_list conforming to the
 *               text/uri-list mime type defined in RFC 2483.
 **/
gchar*
thunar_vfs_uri_list_to_string (GList *uri_list)
{
  gchar *string_list;
  gchar *uri_string;
  gchar *new_list;
  GList *lp;

  string_list = g_strdup ("");

  /* This way of building up the string representation is pretty
   * slow and can lead to serious heap fragmentation if called
   * too often. But this doesn't matter, as this function is not
   * called very often (in fact it should only be used in Thunar
   * when a file drag is started from within Thunar). If you can
   * think of any easy way to avoid calling malloc that often,
   * let me know (of course without depending too much on the
   * exact internal representation and the type of URIs). Else
   * don't waste any time or thought on this.
   */
  for (lp = uri_list; lp != NULL; lp = lp->next)
    {
      g_assert (THUNAR_VFS_IS_URI (lp->data));

      uri_string = thunar_vfs_uri_to_string (THUNAR_VFS_URI (lp->data));
      new_list = g_strconcat (string_list, uri_string, "\r\n", NULL);
      g_free (string_list);
      g_free (uri_string);
      string_list = new_list;
    }

  return string_list;
}



/**
 * thunar_vfs_uri_list_free:
 * @uri_list : a list of #ThunarVfsURI<!---->s.
 *
 * Frees the #ThunarVfsURI<!---->s contained in @uri_list and
 * the @uri_list itself.
 **/
void
thunar_vfs_uri_list_free (GList *uri_list)
{
  GList *lp;

  for (lp = uri_list; lp != NULL; lp = lp->next)
    {
      g_assert (THUNAR_VFS_IS_URI (lp->data));
      g_object_unref (G_OBJECT (lp->data));
    }

  g_list_free (uri_list);
}



