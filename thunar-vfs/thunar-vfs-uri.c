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



static void thunar_vfs_uri_register_type  (GType              *type);
static void thunar_vfs_uri_base_init      (ThunarVfsURIClass  *klass);
static void thunar_vfs_uri_base_finalize  (ThunarVfsURIClass  *klass);
#ifndef G_DISABLE_CHECKS
static void thunar_vfs_uri_init           (ThunarVfsURI       *uri);
#endif



struct _ThunarVfsURIClass
{
  GTypeClass __parent__;

  const gchar *home_path;
  gchar       *localhost;
};

struct _ThunarVfsURI
{
  GTypeInstance __parent__;

  guint              ref_count;
  gchar             *host;
  gchar             *path;
  const gchar       *name;
  ThunarVfsURIScheme scheme;
};



static const gchar *scheme_names[] =
{
  "computer://",
  "file://",
  "trash://",
};



#ifndef G_DISABLE_CHECKS
G_LOCK_DEFINE_STATIC (debug_uris);
static GList *debug_uris = NULL;
#endif



GType
thunar_vfs_uri_get_type (void)
{
  static GType type = G_TYPE_INVALID;;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_uri_register_type, &type);

  return type;
}



static void
thunar_vfs_uri_register_type (GType *type)
{
  static const GTypeFundamentalInfo finfo =
  {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE
  };

  static const GTypeInfo info =
  {
    sizeof (ThunarVfsURIClass),
    (GBaseInitFunc) thunar_vfs_uri_base_init,
    (GBaseFinalizeFunc) thunar_vfs_uri_base_finalize,
    NULL,
    NULL,
    NULL,
    sizeof (ThunarVfsURI),
    256u,
#ifndef G_DISABLE_CHECKS
    (GInstanceInitFunc) thunar_vfs_uri_init,
#else
    NULL,
#endif
    NULL,
  };

  *type = g_type_register_fundamental (g_type_fundamental_next (), "ThunarVfsURI", &info, &finfo, 0);
}



static void
thunar_vfs_uri_base_init (ThunarVfsURIClass *klass)
{
  /* determine the path to the current user's homedir */
  klass->home_path = xfce_get_homedir ();

  /* determine the local host's name */
  klass->localhost = xfce_gethostname ();
}



static void
thunar_vfs_uri_base_finalize (ThunarVfsURIClass *klass)
{
  /* free class data */
  g_free (klass->localhost);
}



#ifndef G_DISABLE_CHECKS
static void
thunar_vfs_uri_atexit (void)
{
  ThunarVfsURI *uri;
  GList        *lp;
  gchar        *s;

  G_LOCK (debug_uris);

  if (G_UNLIKELY (debug_uris != NULL))
    {
      g_print ("--- Leaked a total of %u ThunarVfsURI objects:\n", g_list_length (debug_uris));

      for (lp = debug_uris; lp != NULL; lp = lp->next)
        {
          uri = THUNAR_VFS_URI (lp->data);
          s = thunar_vfs_uri_to_string (uri, 0);
          g_print ("--> %s (%u)\n", s, uri->ref_count);
          g_free (s);
        }

      g_print ("\n");
    }

  G_UNLOCK (debug_uris);
}

static void
thunar_vfs_uri_init (ThunarVfsURI *uri)
{
  static gboolean registered = FALSE;

  G_LOCK (debug_uris);

  /* register the check function to be called at program exit */
  if (!registered)
    {
      g_atexit (thunar_vfs_uri_atexit);
      registered = TRUE;
    }

  debug_uris = g_list_prepend (debug_uris, uri);

  G_UNLOCK (debug_uris);
}
#endif



/**
 * thunar_vfs_uri_new:
 * @identifier : the resource identifier encoded as string.
 * @error      : return location for errors.
 *
 * Parses the URI given in @identifier and returns a
 * corresponding #ThunarVfsURI object or %NULL if
 * @identifier is not a valid URI.
 *
 * As a special case, @identifier may also include an
 * absolute path, in which case the 'file://' scheme is
 * assumed.
 *
 * Return value: the #ThunarVfsURI or %NULL on error.
 **/
ThunarVfsURI*
thunar_vfs_uri_new (const gchar *identifier,
                    GError     **error)
{
  ThunarVfsURI *uri;
  const gchar  *p;
  const gchar  *q;
  gchar        *host;
  gchar        *path;

  g_return_val_if_fail (identifier != NULL, NULL);

  /* check if identifier includes an absolute path */
  if (G_UNLIKELY (*identifier == '/'))
    return thunar_vfs_uri_new_for_path (identifier);

  if (G_LIKELY (identifier[0] == 'f' && identifier[1] == 'i'
            && identifier[2] == 'l' && identifier[3] == 'e'
            && identifier[4] == ':'))
    {
      /* try to parse the path and hostname */
      path = g_filename_from_uri (identifier, &host, error);
      if (G_UNLIKELY (path == NULL))
        return NULL;

      /* allocate the URI instance */
      uri = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
      uri->ref_count = 1;
      uri->scheme = THUNAR_VFS_URI_SCHEME_FILE;
      uri->path = path;

      /* check the host name */
      if (host != NULL)
        {
          if (strcmp (host, THUNAR_VFS_URI_GET_CLASS (uri)->localhost) == 0
              || strcmp (host, "localhost") == 0)
            {
              /* no need to remember the host name */
              g_free (host);
            }
          else
            {
              /* non-local host name, need to remember */
              uri->host = host;
            }
        }
    }
  else if (identifier[0] == 't' && identifier[1] == 'r'
        && identifier[2] == 'a' && identifier[3] == 's'
        && identifier[4] == 'h' && identifier[5] == ':')
    {
      /* skip to the path */
      p = (identifier[6] == '/' && identifier[7] == '/')
        ? (identifier + 8) : (identifier + 6);

      /* verify that the remainder is a path */
      if (G_UNLIKELY (*p != '\0' && *p != '/'))
        {
          g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI,
                       _("The URI '%s' is invalid"), identifier);
          return NULL;
        }

      /* verify that the path doesn't include a '#' */
      for (q = p; *q != '\0'; ++q)
        if (G_UNLIKELY (*q == '#'))
          {
            g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI,
                         _("The trash file URI '%s' may not include a '#'"),
                         identifier);
            return NULL;
          }

      /* allocate the URI instance */
      uri = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
      uri->ref_count = 1;
      uri->scheme = THUNAR_VFS_URI_SCHEME_TRASH;
      uri->path = g_strdup ((*p == '/') ? p : "/");
    }
  else if (identifier[0] == 'c' && identifier[1] == 'o'
        && identifier[2] == 'm' && identifier[3] == 'p'
        && identifier[4] == 'u' && identifier[5] == 't'
        && identifier[6] == 'e' && identifier[7] == 'r'
        && identifier[8] == ':')
    {
      /* skip to the path */
      p = (identifier[9] == '/' && identifier[10] == '/')
        ? (identifier + 11) : (identifier + 9);

      /* verify that the remainder is a path */
      if (G_UNLIKELY (*p != '\0' && *p != '/'))
        {
          g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI,
                       _("The URI '%s' is invalid"), identifier);
          return NULL;
        }

      /* verify that the path doesn't include a '#' */
      for (q = p; *q != '\0'; ++q)
        if (G_UNLIKELY (*q == '#'))
          {
            g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI,
                         _("The trash file URI '%s' may not include a '#'"),
                         identifier);
            return NULL;
          }

      /* allocate the URI instance */
      uri = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
      uri->ref_count = 1;
      uri->scheme = THUNAR_VFS_URI_SCHEME_COMPUTER;
      uri->path = g_strdup ((*p == '/') ? p : "/");
    }
  else
    {
      g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI,
                   _("The URI '%s' uses an unsupported scheme"),
                   identifier);
      return NULL;
    }

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
  gchar        *p;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (path), NULL);

  /* allocate the URI instance */
  uri = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
  uri->ref_count = 1;
  uri->scheme = THUNAR_VFS_URI_SCHEME_FILE;
  uri->path = g_strdup (path);

  /* walk to the end of the path */
  for (p = uri->path; *p != '\0'; ++p)
    ;

  g_assert (*p == '\0');

  /* strip trailing slashes */
  for (--p; p > uri->path && *p == '/'; --p)
    *p = '\0';

  g_assert (*p != '\0');
  g_assert (p >= uri->path);

  /* determine the basename of the path */
  for (uri->name = uri->path; p > uri->path && *p != '/'; --p)
    uri->name = p;

  g_assert (p >= uri->path);
  g_assert (uri->name >= uri->path);
  g_assert (uri->name < uri->path + strlen (uri->path));

  return uri;
}



/**
 * thunar_vfs_uri_ref:
 * @uri : a #ThunarVfsURI instance.
 *
 * Increments the reference count on @uri by 1 and
 * returns @uri.
 *
 * Return value: pointer to @uri.
 **/
ThunarVfsURI*
thunar_vfs_uri_ref (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (uri->ref_count > 0, NULL);

  ++uri->ref_count;

  return uri;
}



/**
 * thunar_vfs_uri_unref:
 * @uri : a #ThunarVfsURI instance.
 *
 * Decreases the reference count on @uri by 1. If the
 * reference count drops to 0, the resources allocated
 * for @uri will be freed.
 **/
void
thunar_vfs_uri_unref (ThunarVfsURI *uri)
{
  g_return_if_fail (THUNAR_VFS_IS_URI (uri));
  g_return_if_fail (uri->ref_count > 0);

  if (--uri->ref_count == 0)
    {
#ifndef G_DISABLE_CHECKS
      G_LOCK (debug_uris);
      if (G_UNLIKELY (uri->host != NULL))
        memset (uri->host, 0xaa, strlen (uri->host));
      memset (uri->path, 0xaa, strlen (uri->path));
      debug_uris = g_list_remove (debug_uris, uri);
      G_UNLOCK (debug_uris);
#endif

      if (G_UNLIKELY (uri->host != NULL))
        g_free (uri->host);

      g_free (uri->path);

      g_type_free_instance ((GTypeInstance *) uri);
    }
}



/**
 * thunar_vfs_uri_is_home:
 * @uri : a #ThunarVfsURI instance.
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
  return (uri->scheme == THUNAR_VFS_URI_SCHEME_FILE
       && exo_str_is_equal (THUNAR_VFS_URI_GET_CLASS (uri)->home_path, uri->path));
}



/**
 * thunar_vfs_uri_is_local:
 * @uri : a #ThunarVfsURI instance.
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
 * @uri : a #ThunarVfsURI instance.
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
 * @uri : a #ThunarVfsURI instance.
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
 * thunar_vfs_uri_get_host:
 * @uri : a #ThunarVfsURI instance.
 *
 * Returns the host associated with the given @uri. This
 * function may return %NULL if either the scheme of @uri
 * does not support specifying a hostname or no hostname
 * was given on construction of the @uri.
 *
 * Return value: the name of the host set for @uri or %NULL.
 **/
const gchar*
thunar_vfs_uri_get_host (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* fallback to local host for 'file://' uris */
  if (uri->scheme == THUNAR_VFS_URI_SCHEME_FILE && uri->host == NULL)
    return THUNAR_VFS_URI_GET_CLASS (uri)->localhost;

  return uri->host;
}



/**
 * thunar_vfs_uri_get_name:
 * @uri : a #ThunarVfsURI instance.
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
 * @uri : a #ThunarVfsURI instance.
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
 * thunar_vfs_uri_get_scheme:
 * @uri : a #ThunarVfsURI instance.
 *
 * Returns the #ThunarVfsURIScheme of @uri.
 *
 * Return value: the scheme of @uri.
 **/
ThunarVfsURIScheme
thunar_vfs_uri_get_scheme (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), THUNAR_VFS_URI_SCHEME_FILE);
  return uri->scheme;
}



/**
 * thunar_vfs_uri_parent:
 * @uri : a #ThunarVfsURI instance.
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
  parent = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
  parent->ref_count = 1;
  parent->host = g_strdup (uri->host);
  parent->path = g_path_get_dirname (uri->path);
  parent->scheme = uri->scheme;

  /* determine the basename of the path */
  for (p = parent->name = parent->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      parent->name = p + 1;

  return parent;
}



/**
 * thunar_vfs_uri_relative:
 * @uri   : a #ThunarVfsURI instance.
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
  relative = (ThunarVfsURI *) g_type_create_instance (THUNAR_VFS_TYPE_URI);
  relative->ref_count = 1;
  relative->host = g_strdup (uri->host);
  relative->path = g_build_filename (uri->path, name, NULL);
  relative->scheme = uri->scheme;

  /* determine the basename of the path */
  for (p = relative->name = relative->path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      relative->name = p + 1;

  return relative;
}



/**
 * thunar_vfs_uri_to_string:
 * @uri          : a #ThunarVfsURI.
 * @hide_options : tells which parts of the uri should be hidden.
 *
 * Returns the string representation of @uri. The
 * caller is responsible for freeing the returned
 * string using #g_free().
 *
 * Return value: the string representation of @uri.
 **/
gchar*
thunar_vfs_uri_to_string (ThunarVfsURI           *uri,
                          ThunarVfsURIHideOptions hide_options)
{
  const gchar *host;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* determine the host name */
  if ((hide_options & THUNAR_VFS_URI_HIDE_HOST) != 0)
    {
      host = "";
    }
  else if (uri->host == NULL)
    {
      host = (uri->scheme == THUNAR_VFS_URI_SCHEME_FILE)
        ?  THUNAR_VFS_URI_GET_CLASS (uri)->localhost : "";
    }
  else
    {
      host = uri->host;
    }

  return g_strconcat (scheme_names[uri->scheme], host, uri->path, NULL);
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
  const gchar *host;
  const gchar *p;
  guint        h;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), 0);

  /* hash the scheme */
  h = THUNAR_VFS_URI (uri)->scheme;

  /* hash the path */
  for (p = THUNAR_VFS_URI (uri)->path; *p != '\0'; ++p)
    h = (h << 5) - h + *p;

  /* hash the host (if present) */
  host = thunar_vfs_uri_get_host (THUNAR_VFS_URI (uri));
  if (G_LIKELY (host != NULL))
    for (p = host; *p != '\0'; ++p)
      h = (h << 5) - h + *p;

  return h;
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
  const gchar *a_host;
  const gchar *b_host;
  const gchar *a_name;
  const gchar *b_name;
  const gchar *a_path;
  const gchar *b_path;
  const gchar *ap;
  const gchar *bp;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (a), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (b), FALSE);

  /* compare the schemes first */
  if (THUNAR_VFS_URI (a)->scheme != THUNAR_VFS_URI (b)->scheme)
    return FALSE;

  /* compare the host names (TODO: speedup?!) */
  a_host = thunar_vfs_uri_get_host (THUNAR_VFS_URI (a));
  b_host = thunar_vfs_uri_get_host (THUNAR_VFS_URI (b));
  if (!exo_str_is_equal (a_host, b_host))
    return FALSE;

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
 * @uri_list     : a list of #ThunarVfsURI<!---->s.
 * @hide_options : tells which parts of the uris should be hidden.
 *
 * Free the returned value using #g_free() when you
 * are done with it.
 *
 * Return value: the string representation of @uri_list conforming to the
 *               text/uri-list mime type defined in RFC 2483.
 **/
gchar*
thunar_vfs_uri_list_to_string (GList                  *uri_list,
                               ThunarVfsURIHideOptions hide_options)
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
      uri_string = thunar_vfs_uri_to_string (THUNAR_VFS_URI (lp->data), hide_options);
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
    thunar_vfs_uri_unref (lp->data);

  g_list_free (uri_list);
}



