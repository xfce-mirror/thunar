/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* implement thunar-vfs-path's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __THUNAR_VFS_PATH_C__
#include <thunar-vfs/thunar-vfs-path.h>

#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* Masks to handle the 4-byte aligned path names */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define THUNAR_VFS_PATH_MASK (0xffu << ((sizeof (gsize) - 1) * 8))
#define THUNAR_VFS_PATH_ROOT (0x2fu)
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define THUNAR_VFS_PATH_MASK (0xffu)
#define THUNAR_VFS_PATH_ROOT (0x2fu << ((sizeof (gsize) - 1) * 8))
#else
#error "Unsupported endianess"
#endif



static guint thunar_vfs_path_escape_uri_length  (const ThunarVfsPath *path);
static guint thunar_vfs_path_escape_uri         (const ThunarVfsPath *path,
                                                 gchar               *buffer);



/* A table of the ASCII chars from space (32) to DEL (127) */
static const guchar ACCEPTABLE_URI_CHARS[96] = {
  /*      !    "    #    $    %    &    '    (    )    *    +    ,    -    .    / */ 
  0x00,0x3F,0x20,0x20,0x28,0x00,0x2C,0x3F,0x3F,0x3F,0x3F,0x2A,0x28,0x3F,0x3F,0x1C,
  /* 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ? */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x38,0x20,0x20,0x2C,0x20,0x20,
  /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
  0x38,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
  /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x20,0x3F,
  /* `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o */
  0x20,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
  /* p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~  DEL */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x3F,0x20
};

/* List of hexadecimal characters */
static const gchar HEX_CHARS[16] = "0123456789ABCDEF";

#define ACCEPTABLE_URI_CHAR(c) ((c) >= 32 && (c) < 128 && (ACCEPTABLE_URI_CHARS[(c) - 32] & 0x08))



/* Debugging support in ThunarVfsPath */
#ifdef G_ENABLE_DEBUG

G_LOCK_DEFINE_STATIC (debug_paths);
static GList *debug_paths = NULL;

#define THUNAR_VFS_PATH_DEBUG_INSERT(path)                                                        \
G_STMT_START{                                                                                     \
  G_LOCK (debug_paths);                                                                           \
  debug_paths = g_list_prepend (debug_paths, (path));                                             \
  G_UNLOCK (debug_paths);                                                                         \
}G_STMT_END

#define THUNAR_VFS_PATH_DEBUG_REMOVE(path)                                                        \
G_STMT_START{                                                                                     \
  G_LOCK (debug_paths);                                                                           \
  debug_paths = g_list_remove (debug_paths, (path));                                              \
  G_UNLOCK (debug_paths);                                                                         \
}G_STMT_END

#define THUNAR_VFS_PATH_DEBUG_SHUTDOWN()                                                          \
G_STMT_START{                                                                                     \
  if (G_UNLIKELY (debug_paths != NULL))                                                           \
    {                                                                                             \
      GList *lp;                                                                                  \
      gchar *uri;                                                                                 \
      gint   n;                                                                                   \
      G_LOCK (debug_paths);                                                                       \
      g_print ("--- Leaked a total of %u ThunarVfsPath objects:\n", g_list_length (debug_paths)); \
      for (lp = debug_paths; lp != NULL; lp = lp->next)                                           \
        {                                                                                         \
          uri = thunar_vfs_path_dup_string (lp->data);                                            \
          n = ((ThunarVfsPath *) lp->data)->ref_count;                                            \
          g_print ("--> %s (%d)\n", uri, (n & ~THUNAR_VFS_PATH_SCHEME_MASK));                     \
          g_free (uri);                                                                           \
        }                                                                                         \
      G_UNLOCK (debug_paths);                                                                     \
    }                                                                                             \
}G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define THUNAR_VFS_PATH_DEBUG_INSERT(path)  G_STMT_START{ (void)0; }G_STMT_END
#define THUNAR_VFS_PATH_DEBUG_REMOVE(path)  G_STMT_START{ (void)0; }G_STMT_END
#define THUNAR_VFS_PATH_DEBUG_SHUTDOWN()    G_STMT_START{ (void)0; }G_STMT_END

#endif /* !G_ENABLE_DEBUG */



/* components of the path to the users home folder */
static ThunarVfsPath **home_components;
static guint           n_home_components;

/**
 * _thunar_vfs_path_trash_root:
 *
 * The shared instance of the #ThunarVfsPath that points to the
 * trash root folder.
 **/
ThunarVfsPath *_thunar_vfs_path_trash_root = NULL;



static guint
thunar_vfs_path_escape_uri_length (const ThunarVfsPath *path)
{
  const guchar *s;
  guint         base_length;
  guint         length;

  /* determine the base length for the scheme (file:/// or trash:///) */
  length = base_length = _thunar_vfs_path_is_local (path) ? 8 : 9;

  /* determine the length for the path part */
  for (; path->parent != NULL; path = path->parent)
    {
      /* prepend a path separator */
      if (length > base_length)
        length += 1;

      for (s = (const guchar *) thunar_vfs_path_get_name (path); *s != '\0'; ++s)
        length += ACCEPTABLE_URI_CHAR (*s) ? 1 : 3;
    }

  return length;
}



static guint
thunar_vfs_path_escape_uri (const ThunarVfsPath *path,
                            gchar               *buffer)
{
  typedef struct _ThunarVfsPathItem
  {
    const ThunarVfsPath       *path;
    struct _ThunarVfsPathItem *next;
  } ThunarVfsPathItem;

  ThunarVfsPathItem *item;
  ThunarVfsPathItem *root = NULL;
  const gchar       *s;
  guchar             c;
  gchar             *t;

  /* prepend 'trash:///' or 'file:///' string (using a simple optimization on i386/ppc) */
  if (G_LIKELY (thunar_vfs_path_get_scheme (path) == THUNAR_VFS_PATH_SCHEME_FILE))
    {
#if defined(__GNUC__) && (defined(__i386__) || defined(__ppc__))
      ((guint32 *) buffer)[0] = ((const guint32 *) "file:///")[0];
      ((guint32 *) buffer)[1] = ((const guint32 *) "file:///")[1];
#else
      /* hopefully the compiler will be able to optimize this */
      buffer[0] = 'f'; buffer[1] = 'i';
      buffer[2] = 'l'; buffer[3] = 'e';
      buffer[4] = ':'; buffer[5] = '/';
      buffer[6] = '/'; buffer[7] = '/';
#endif
      t = buffer + 8;
    }
  else
    {
#if defined(__GNUC__) && (defined(__i386__) || defined(__ppc__))
      ((guint32 *) buffer)[0] = ((const guint32 *) "trash://")[0];
      ((guint32 *) buffer)[1] = ((const guint32 *) "trash://")[1];
#else
      /* hopefully the compiler will be able to optimize this */
      buffer[0] = 't'; buffer[1] = 'r';
      buffer[2] = 'a'; buffer[3] = 's';
      buffer[4] = 'h'; buffer[5] = ':';
      buffer[6] = '/'; buffer[7] = '/';
#endif
      buffer[8] = '/';
      t = buffer + 9;
    }

  /* generate the path item list (reverse parent relation) */
  for (; path->parent != NULL; path = path->parent)
    {
      item = g_newa (ThunarVfsPathItem, 1);
      item->path = path;
      item->next = root;
      root = item;
    }

  /* generate the uri */
  for (item = root; item != NULL; item = item->next)
    {
      /* append a '/' character */
      if (G_LIKELY (item != root))
        *t++ = '/';

      /* copy the path component name */
      for (s = thunar_vfs_path_get_name (item->path); *s != '\0'; ++s)
        {
          c = *((const guchar *) s);
          if (G_UNLIKELY (!ACCEPTABLE_URI_CHAR (c)))
            {
              *t++ = '%';
              *t++ = HEX_CHARS[c >> 4];
              *t++ = HEX_CHARS[c & 15];
            }
          else
            {
              *t++ = *s;
            }
        }
    }

  /* zero-terminate the URI */
  *t = '\0';

  return (t - buffer) + 1;
}



GType
thunar_vfs_path_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarVfsPath"),
                                           (GBoxedCopyFunc) thunar_vfs_path_ref,
                                           (GBoxedFreeFunc) thunar_vfs_path_unref);
    }

  return type;
}



GType
thunar_vfs_path_list_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarVfsPathList"),
                                           (GBoxedCopyFunc) thunar_vfs_path_list_copy,
                                           (GBoxedFreeFunc) thunar_vfs_path_list_free);
    }

  return type;
}



/**
 * thunar_vfs_path_new:
 * @identifier : an URI identifier or an absolute path.
 * @error      : return location for errors or %NULL.
 *
 * Returns a #ThunarVfsPath that represents the given
 * @identifier or %NULL on error. In the latter case
 * @error will be set to point to an #GError describing
 * the problem.
 * 
 * The caller is responsible to free the returned
 * object using thunar_vfs_path_unref() when no
 * longer needed.
 *
 * Return value: the #ThunarVfsPath for @identifier
 *               or %NULL on error.
 **/
ThunarVfsPath*
thunar_vfs_path_new (const gchar *identifier,
                     GError     **error)
{
  ThunarVfsPath *path = home_components[0]; /* default to file system root */
  const gchar   *s;
  const gchar   *s1;
  const gchar   *s2;
  gchar         *filename;
  gchar         *t;
  guint          n;

  /* check if we have an absolute path or an URI */
  if (G_UNLIKELY (*identifier != G_DIR_SEPARATOR))
    {
      /* treat the identifier as URI */
      filename = g_filename_from_uri (identifier, NULL, NULL);
      if (G_UNLIKELY (filename == NULL))
        {
          /* hey, but maybe it's a trash:-URI */
          if (G_LIKELY (identifier[0] == 't' && identifier[1] == 'r' && identifier[2] == 'a'
                     && identifier[3] == 's' && identifier[4] == 'h' && identifier[5] == ':'))
            {
              /* skip slashes (yes, not dir separators) */
              for (s = identifier + 6; *s == '/'; ++s)
                ;

              /* start at the trash root folder */
              path = _thunar_vfs_path_trash_root;

              /* check if it's the trash root folder */
              if (G_LIKELY (*s == '\0'))
                return thunar_vfs_path_ref (path);

              /* try to interpret the file part */
              t = g_strconcat ("file:/", s, NULL);
              filename = g_filename_from_uri (t, NULL, NULL);
              g_free (t);
            }
        }

      /* check if the URI is invalid */
      if (G_UNLIKELY (filename == NULL))
        {
          g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI, _("The URI \"%s\" is invalid"), identifier);
          return NULL;
        }
    }
  else
    {
      /* canonicalize the absolute path, to remove additional slashes and dots */
      filename = thunar_vfs_canonicalize_filename (identifier);
    }

  /* parse the filename to a ThunarVfsPath */
  s = filename + 1;

  /* local paths may be relative to a home component */
  if (G_LIKELY (path == home_components[0]))
    {
      /* start at the root path */
      for (n = 1; n < n_home_components; ++n)
        {
          /* skip additional slashes */
          for (; G_UNLIKELY (*s == G_DIR_SEPARATOR); ++s)
            ;

          /* check if we have reached the end of the filename */
          if (G_UNLIKELY (*s == '\0'))
            break;

          /* check if the path component equals the next home path component */
          for (s1 = thunar_vfs_path_get_name (home_components[n]), s2 = s; *s1 != '\0' && *s1 == *s2; ++s1, ++s2)
            ;
          if (*s1 != '\0' || (*s2 != '\0' && *s2 != G_DIR_SEPARATOR))
            break;

          /* go on with the next home path component */
          path = home_components[n];
          s = s2;
        }
    }

  /* determine the subpath (takes appropriate references) */
  path = _thunar_vfs_path_new_relative (path, s);

  /* cleanup */
  g_free (filename);

  return path;
}



/**
 * thunar_vfs_path_get_for_home:
 *
 * Returns the #ThunarVfsPath that represents
 * the current users home directory.
 *
 * The caller is responsible to free the
 * returned object using thunar_vfs_path_unref()
 * when no longer needed.
 *
 * Return value: the #ThunarVfsPath for the 
 *               current users home directory.
 **/
ThunarVfsPath*
thunar_vfs_path_get_for_home (void)
{
  return thunar_vfs_path_ref (home_components[n_home_components - 1]);
}



/**
 * thunar_vfs_path_get_for_root:
 *
 * Returns the #ThunarVfsPath that represents the
 * file systems root folder.
 *
 * The caller is responsible to free the returned
 * object using thunar_vfs_path_unref() when no
 * longer needed.
 *
 * Return value: the #ThunarVfsPath for the file
 *               systems root directory.
 **/
ThunarVfsPath*
thunar_vfs_path_get_for_root (void)
{
  return thunar_vfs_path_ref (home_components[0]);
}



/**
 * thunar_vfs_path_get_for_trash:
 *
 * Returns the #ThunarVfsPath that represents the
 * trash root folder.
 *
 * The caller is responsible to free the returned
 * object using thunar_vfs_path_unref() when no
 * longer needed.
 *
 * Return value: the #ThunarVfsPath for the trash
 *               root folder.
 **/
ThunarVfsPath*
thunar_vfs_path_get_for_trash (void)
{
  return thunar_vfs_path_ref (_thunar_vfs_path_trash_root);
}



/**
 * thunar_vfs_path_unref:
 * @path : a #ThunarVfsPath.
 *
 * Decreases the reference count on @path and
 * frees the resources allocated for @path
 * once the reference count drops to zero.
 **/
void
thunar_vfs_path_unref (ThunarVfsPath *path)
{
  ThunarVfsPath *parent;
  const gsize   *p;

  while (path != NULL && (g_atomic_int_exchange_and_add (&path->ref_count, -1) & ~THUNAR_VFS_PATH_SCHEME_MASK) == 1)
    {
      /* verify that we don't free the paths for trash root or home components */
#ifdef G_ENABLE_DEBUG
      if (G_UNLIKELY (path == _thunar_vfs_path_trash_root))
        {
          /* the trash root path may not be freed */
          g_error (G_STRLOC ": Attempt to free the trash root path detected");
        }
      else
        {
          /* same for the home component paths */
          guint n;
          for (n = 0; n < n_home_components; ++n)
            if (G_UNLIKELY (path == home_components[n]))
              break;

          /* check if one of the home components matched */
          if (G_UNLIKELY (n < n_home_components))
            {
              /* none of the home component paths can be freed this way */
              g_error (G_STRLOC ": Attempt to free the home component path \"%s\" detected", thunar_vfs_path_get_name (path));
            }
        }
#endif

      /* remember the parent path */
      parent = path->parent;

      /* remove the path from the debug list */
      THUNAR_VFS_PATH_DEBUG_REMOVE (path);

      /* release the path resources (we need to determine the size for the slice allocator) */
      for (p = (const gsize *) thunar_vfs_path_get_name (path); (*p & THUNAR_VFS_PATH_MASK) != 0u; ++p)
        ;
      _thunar_vfs_slice_free1 (((const guint8 *) (p + 1)) - ((const guint8 *) path), path);

      /* continue with the parent */
      path = parent;
    }
}



/**
 * thunar_vfs_path_hash:
 * @path_ptr : a #ThunarVfsPath.
 *
 * Generates a hash value for the given @path_ptr.
 *
 * Return value: the hash value for @path_ptr.
 **/
guint
thunar_vfs_path_hash (gconstpointer path_ptr)
{
  const gchar *p = thunar_vfs_path_get_name (path_ptr);
  guint        h = *p + thunar_vfs_path_get_scheme (path_ptr);

  /* hash the last path component (which cannot be empty) */
  while (*++p != '\0')
    h = (h << 5) - h + *p;

  return h;
}



/**
 * thunar_vfs_path_equal:
 * @path_ptr_a : first #ThunarVfsPath.
 * @path_ptr_b : second #ThunarVfsPath.
 *
 * Checks whether @path_ptr_a and @path_ptr_b refer
 * to the same local path.
 *
 * Return value: %TRUE if @path_ptr_a and @path_ptr_b
 *               are equal.
 **/
gboolean
thunar_vfs_path_equal (gconstpointer path_ptr_a,
                       gconstpointer path_ptr_b)
{
  const ThunarVfsPath *path_a = path_ptr_a;
  const ThunarVfsPath *path_b = path_ptr_b;
  const gsize         *a;
  const gsize         *b;

  /* compare the schemes */
  if (thunar_vfs_path_get_scheme (path_a) != thunar_vfs_path_get_scheme (path_b))
    return FALSE;

again:
  /* check if the paths are the same object */
  if (G_UNLIKELY (path_a == path_b))
    return TRUE;

  /* compare the last path component */
  a = (const gsize *) thunar_vfs_path_get_name (path_a);
  b = (const gsize *) thunar_vfs_path_get_name (path_b);
  for (;;)
    {
      if (*a != *b)
        return FALSE;
      else if ((*a & THUNAR_VFS_PATH_MASK) == 0u)
        break;

      ++a;
      ++b;
    }

  /* compare the parent path components */
  if (G_LIKELY (path_a->parent != NULL && path_b->parent != NULL))
    {
      path_a = path_a->parent;
      path_b = path_b->parent;
      goto again;
    }

  /* verify that both paths have no parents then */
  return (path_a->parent == NULL && path_b->parent == NULL);
}



/**
 * thunar_vfs_path_relative:
 * @parent : a #ThunarVfsPath.
 * @name   : a valid filename in the local file system encoding.
 *
 * Returns a #ThunarVfsPath for the file @name relative to
 * @parent. @name must be a valid filename in the local file
 * system encoding and it may not contain any slashes.
 *
 * The caller is responsible to free the returned object
 * using thunar_vfs_path_unref() when no longer needed.
 *
 * Return value: the path to @name relative to @parent.
 **/
ThunarVfsPath*
thunar_vfs_path_relative (ThunarVfsPath *parent,
                          const gchar   *name)
{
  g_return_val_if_fail (parent != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (*name != '\0', NULL);
  g_return_val_if_fail (strchr (name, '/') == NULL, NULL);

  /* let _thunar_vfs_path_child() do it's work */
  return _thunar_vfs_path_child (parent, name);
}



/**
 * thunar_vfs_path_is_ancestor:
 * @path     : a #ThunarVfsPath.
 * @ancestor : another #ThunarVfsPath.
 *
 * Determines whether @path is somewhere below @ancestor,
 * possible with intermediate folders.
 *
 * Return value: %TRUE if @ancestor contains @path as a
 *               child, grandchild, great grandchild, etc.
 **/
gboolean
thunar_vfs_path_is_ancestor (const ThunarVfsPath *path,
                             const ThunarVfsPath *ancestor)
{
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  for (path = path->parent; path != NULL; path = path->parent)
    if (thunar_vfs_path_equal (path, ancestor))
      return TRUE;

  return FALSE;
}



/**
 * thunar_vfs_path_is_home:
 * @path : a #ThunarVfsPath.
 *
 * Checks whether @path refers to the users home
 * directory.
 *
 * Return value: %TRUE if @path refers to the users
 *               home directory.
 **/
gboolean
thunar_vfs_path_is_home (const ThunarVfsPath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);
  return (path == home_components[n_home_components - 1]);
}



/**
 * thunar_vfs_path_dup_string:
 * @path : a #ThunarVfsPath.
 *
 * Like thunar_vfs_path_to_string(), this function transform
 * the @path to its string representation, but unlike
 * thunar_vfs_path_to_string(), this function automatically
 * allocates the required amount of memory from the heap.
 * The returned string must be freed by the caller when
 * no longer needed.
 *
 * Return value: the string representation of @path.
 **/
gchar*
thunar_vfs_path_dup_string (const ThunarVfsPath *path)
{
  const ThunarVfsPath *p;
  gchar               *s;
  guint                n;

  /* determine the number of bytes required to
   * store the path's string representation.
   */
  for (n = 0, p = path; p != NULL; p = p->parent)
    n += strlen (thunar_vfs_path_get_name (p)) + 2;

  /* allocate the buffer to store the string */
  s = g_malloc (n);

  /* store the path string to the buffer */
  thunar_vfs_path_to_string (path, s, n, NULL);

  /* return the string buffer */
  return s;
}



/**
 * thunar_vfs_path_to_string:
 * @path    : a #ThunarVfsPath.
 * @buffer  : the buffer to store the path string to.
 * @bufsize : the size of @buffer in bytes.
 * @error   : return location for errors or %NULL.
 *
 * Stores the @path into the string pointed to by @buffer,
 * so it can be used for system path operations. Returns
 * the number of bytes stored to @buffer or a negative
 * value if @bufsize is too small to store the whole @path.
 * In the latter case @error will be set to point to an
 * error describing the problem.
 *
 * If @buffer is allocated on the stack, it is suggested
 * to use #THUNAR_VFS_PATH_MAXSTRLEN for the buffer size
 * in most cases. The stack should never be used in recursive
 * functions; use thunar_vfs_path_dup_string() instead there.
 *
 * Return value: the number of bytes (including the null
 *               byte) stored to @buffer or a negative
 *               value if @buffer cannot hold the whole
 *               @path.
 **/
gssize
thunar_vfs_path_to_string (const ThunarVfsPath *path,
                           gchar               *buffer,
                           gsize                bufsize,
                           GError             **error)
{
  typedef struct _ThunarVfsPathItem
  {
    const gchar               *name;
    struct _ThunarVfsPathItem *next;
  } ThunarVfsPathItem;

  ThunarVfsPathItem *items = NULL;
  ThunarVfsPathItem *item;
  const gchar       *name;
  gchar             *bp;
  guint              n;

  g_return_val_if_fail (buffer != NULL, -1);
  g_return_val_if_fail (bufsize > 0, -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  /* the root element is a special case to ease the processing */
  if (G_UNLIKELY (path->parent == NULL))
    {
      if (G_UNLIKELY (bufsize < 2))
        goto error;
      buffer[0] = G_DIR_SEPARATOR;
      buffer[1] = '\0';
      return 2;
    }

  /* determine the buffer size required for the path buffer */
  for (n = 1; path->parent != NULL; path = path->parent)
    {
      /* add the path to the item list */
      item = g_newa (ThunarVfsPathItem, 1);
      item->name = thunar_vfs_path_get_name (path);
      item->next = items;
      items = item;

      /* add the size constraint (including the '/') */
      n += strlen (item->name) + 1;
    }

  /* verify the buffer size */
  if (G_UNLIKELY (bufsize < n))
    {
error:
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NAMETOOLONG,
                   _("Path too long to fit into buffer"));
      return -1;
    }

  /* generate the path string */
  for (bp = buffer, item = items; item != NULL; item = item->next)
    {
      /* prepend the path separator */
      *bp++ = G_DIR_SEPARATOR;

      /* append the component name */
      for (name = item->name; *name != '\0'; )
        *bp++ = *name++;
    }

  /* append the string terminator */
  *bp = '\0';

   /* return the number of bytes written to the buffer */
  return n;
}



/**
 * thunar_vfs_path_dup_uri:
 * @path : a #ThunarVfsPath.
 *
 * Similar to thunar_vfs_path_to_uri(), but automatically
 * allocates memory on the heap instead of using a user
 * supplied buffer for the URI.
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the escaped URI for @path.
 **/
gchar*
thunar_vfs_path_dup_uri (const ThunarVfsPath *path)
{
  gchar *s;
  guint  m;
  guint  n;

  g_return_val_if_fail (path != NULL, NULL);

  /* calculate the length of the uri string */
  n = thunar_vfs_path_escape_uri_length (path) + 1;

  /* escape the path to an uri string */
  s = g_malloc (sizeof (gchar) * n);
  m = thunar_vfs_path_escape_uri (path, s);

  /* verify the result */
  _thunar_vfs_assert (strlen (s) == m - 1);
  _thunar_vfs_assert (m == n);

  return s;
}



/**
 * thunar_vfs_path_to_uri:
 * @path    : a #ThunarVfsPath.
 * @buffer  : the buffer to store the URI string to.
 * @bufsize : the size of @buffer in bytes.
 * @error   : return location for errors or %NULL.
 *
 * Escapes @path according to the rules of the file URI
 * specification and stores the escaped URI to @buffer.
 * Returns the number of bytes stored to @buffer or a
 * negative value if @bufsize is too small to store the
 * escaped URI. In the latter case @error will be set to
 * point to an #GError describing the problem.
 *
 * When using the stack for @buffer, it is suggested to
 * use #THUNAR_VFS_PATH_MAXURILEN for the buffer size in
 * most cases. The stack should never be used in recursive
 * functions; use thunar_vfs_path_dup_uri() instead there.
 *
 * Return value: the number of bytes (including the null
 *               byte) stored to @buffer or a negative
 *               value if @buffer cannot hold the URI.
 **/
gssize
thunar_vfs_path_to_uri (const ThunarVfsPath *path,
                        gchar               *buffer,
                        gsize                bufsize,
                        GError             **error)
{
  guint n;
  guint m;

  g_return_val_if_fail (path != NULL, -1);
  g_return_val_if_fail (buffer != NULL, -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  /* verify that the buffer is large enough */
  n = thunar_vfs_path_escape_uri_length (path) + 1;
  if (G_UNLIKELY (bufsize < n))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NAMETOOLONG,
                   _("URI too long to fit into buffer"));
      return -1;
    }

  /* copy the URI to the buffer */
  m = thunar_vfs_path_escape_uri (path, buffer);

  /* verify the result */
  _thunar_vfs_assert (strlen (buffer) == m - 1);
  _thunar_vfs_assert (m == n);

  return n;
}



/**
 * thunar_vfs_path_list_from_string:
 * @uri_string : a string representation of an URI list.
 * @error      : return location for errors.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace.
 *
 * If all URIs were successfully parsed into #ThunarVfsPath
 * objects, the list of parsed URIs will be returned, and
 * you'll need to call thunar_vfs_path_list_free() to
 * release the list resources. Else if the parsing fails
 * at some point, %NULL will be returned and @error will
 * be set to describe the cause.
 *
 * Note, that if @string contains no URIs, this function
 * will also return %NULL, but @error won't be set. So
 * take care when checking for an error condition!
 *
 * Return value: the list of #ThunarVfsPath's or %NULL.
 **/
GList*
thunar_vfs_path_list_from_string (const gchar *uri_string,
                                  GError     **error)
{
  ThunarVfsPath *path;
  const gchar   *s;
  const gchar   *t;
  GList         *path_list = NULL;
  gchar         *identifier;

  for (s = uri_string; s != NULL; )
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
                  path = thunar_vfs_path_new (identifier, error);
                  g_free (identifier);

                  /* check if we succeed */
                  if (G_UNLIKELY (path == NULL))
                    {
                      thunar_vfs_path_list_free (path_list);
                      return NULL;
                    }
                  else
                    {
                      /* append the newly parsed path */
                      path_list = g_list_append (path_list, path);
                    }
                }
            }
        }

      for (; *s != '\0' && *s != '\n'; ++s)
        ;

      if (*s++ == '\0')
        break;
    }

  return path_list;
}



/**
 * thunar_vfs_path_list_to_string:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 *
 * Free the returned value using g_free() when you
 * are done with it.
 *
 * Return value: the string representation of @path_list conforming to the
 *               text/uri-list mime type defined in RFC 2483.
 **/
gchar*
thunar_vfs_path_list_to_string (GList *path_list)
{
  gchar *buffer;
  gsize  bufsize = 512;
  gsize  bufpos = 0;
  GList *lp;
  gint   n;

  /* allocate initial buffer */
  buffer = g_malloc (bufsize + 1);

  for (lp = path_list; lp != NULL; lp = lp->next)
    {
      for (;;)
        {
          /* determine the size required to store the URI and the line break */
          n = thunar_vfs_path_escape_uri_length (lp->data) + 2;
          if (n > (bufsize - bufpos))
            {
              /* automatically increase the buffer */
              bufsize += 512;
              buffer = g_realloc (buffer, bufsize + 1);
              continue;
            }

          /* append the URI to the buffer */
          n = thunar_vfs_path_escape_uri (lp->data, buffer + bufpos);

          /* shift the buffer position */
          bufpos += (n - 1);

          /* append a line break */
          buffer[bufpos++] = '\r';
          buffer[bufpos++] = '\n';

          /* sanity checks */
          _thunar_vfs_assert (bufpos <= bufsize);
          break;
        }
    }

  /* zero terminate the string */
  buffer[bufpos] = '\0';

  return buffer;
}



/**
 * thunar_vfs_path_list_copy:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 *
 * Takes a deep copy of @path_list and returns the
 * result. The caller is responsible to free the
 * returned list using thunar_vfs_path_list_free().
 *
 * Return value: a deep copy of @path_list.
 **/
GList*
thunar_vfs_path_list_copy (GList *path_list)
{
  GList *list;
  GList *lp;

  for (list = NULL, lp = g_list_last (path_list); lp != NULL; lp = lp->prev)
    list = g_list_prepend (list, thunar_vfs_path_ref (lp->data));

  return list;
}



/**
 * thunar_vfs_path_list_free:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 *
 * Frees the #ThunarVfsPath<!---->s in @path_list and
 * the @path_list itself.
 **/
void
thunar_vfs_path_list_free (GList *path_list)
{
  GList *lp;
  for (lp = path_list; lp != NULL; lp = lp->next)
    thunar_vfs_path_unref (lp->data);
  g_list_free (path_list);
}



/**
 * _thunar_vfs_path_init:
 *
 * Intialize the #ThunarVfsPath module.
 **/
void
_thunar_vfs_path_init (void)
{
  ThunarVfsPath *path;
  const gchar   *s;
  gchar         *offset;
  gchar        **components;
  gchar        **component;
  guint          n_bytes;
  guint          n = 0;
  gchar         *t;

  _thunar_vfs_return_if_fail (home_components == NULL);
  _thunar_vfs_return_if_fail (n_home_components == 0);

  /* include the root element */
  n_bytes = sizeof (ThunarVfsPath) + sizeof (gsize);
  n_home_components = 1;

  /* split the home path into its components */
  components = g_strsplit (g_get_home_dir (), "/", -1);
  for (component = components; *component != NULL; ++component)
    if (G_LIKELY (**component != '\0'))
      {
        n_bytes += sizeof (ThunarVfsPath) + ((strlen (*component) + sizeof (gsize)) / sizeof (gsize)) * sizeof (gsize);
        n_home_components += 1;
      }

  /* allocate the memory (including the pointer table overhead) */
  home_components = g_malloc (n_bytes + n_home_components * sizeof (ThunarVfsPath *));
  offset = ((gchar *) home_components) + n_home_components * sizeof (ThunarVfsPath *);

  /* add the root node */
  path = (gpointer) offset;
  path->ref_count = 1;
  path->parent = NULL;
  home_components[0] = path;
  *((gsize *) thunar_vfs_path_get_name (path)) = THUNAR_VFS_PATH_ROOT;
  offset += sizeof (ThunarVfsPath) + sizeof (gsize);

  /* add the remaining path components */
  for (component = components; *component != NULL; ++component)
    if (G_LIKELY (**component != '\0'))
      {
        /* setup the path basics */
        path = (gpointer) offset;
        path->ref_count = 1;
        path->parent = home_components[n];
        home_components[++n] = path;

        /* calculate the offset for the next home path component */
        offset += sizeof (ThunarVfsPath) + ((strlen (*component) + sizeof (gsize)) / sizeof (gsize)) * sizeof (gsize);

        /* copy the path */
        for (s = *component, t = (gchar *) thunar_vfs_path_get_name (path); *s != '\0'; )
          *t++ = *s++;

        /* fill the path with zeros */
        while (t < offset)
          *t++ = '\0';
      }

  /* verify state */
  g_assert (n_home_components == n + 1);

  /* allocate the trash root path */
  _thunar_vfs_path_trash_root = g_malloc (sizeof (ThunarVfsPath) + sizeof (gsize));
  _thunar_vfs_path_trash_root->ref_count = 1 | THUNAR_VFS_PATH_SCHEME_TRASH;
  _thunar_vfs_path_trash_root->parent = NULL;
  *((gsize *) thunar_vfs_path_get_name (_thunar_vfs_path_trash_root)) = THUNAR_VFS_PATH_ROOT;

  /* cleanup */
  g_strfreev (components);
}



/**
 * _thunar_vfs_path_shutdown:
 *
 * Shutdown the #ThunarVfsPath module.
 **/
void
_thunar_vfs_path_shutdown (void)
{
  guint n;

  _thunar_vfs_return_if_fail (home_components != NULL);
  _thunar_vfs_return_if_fail (n_home_components != 0);

  /* print out the list of leaked paths */
  THUNAR_VFS_PATH_DEBUG_SHUTDOWN ();

  for (n = 0; n < n_home_components; ++n)
    g_assert (home_components[n]->ref_count == 1);

  g_free (home_components);
  home_components = NULL;
  n_home_components = 0;

  g_assert (_thunar_vfs_path_trash_root->ref_count == (1 | THUNAR_VFS_PATH_SCHEME_TRASH));
  g_free (_thunar_vfs_path_trash_root);
  _thunar_vfs_path_trash_root = NULL;
}



/**
 * _thunar_vfs_path_new_relative:
 * @parent        : the parent path.
 * @relative_path : a relative path, or the empty string.
 *
 * Creates a new #ThunarVfsPath, which represents the subpath of @parent,
 * identified by the @relative_path. If @relative_path is the empty string,
 * a new reference to @parent will be returned.
 *
 * The caller is responsible to free the returned #ThunarVfsPath object
 * using thunar_vfs_path_unref() when no longer needed.
 *
 * Return value: the #ThunarVfsPath object to the subpath of @parent
 *               identified by @relative_path.
 **/
ThunarVfsPath*
_thunar_vfs_path_new_relative (ThunarVfsPath *parent,
                               const gchar   *relative_path)
{
  ThunarVfsPath *path = parent;
  const gchar   *s1;
  const gchar   *s = relative_path;
  gchar         *t;
  guint          n;

  _thunar_vfs_return_val_if_fail (relative_path != NULL, NULL);
  _thunar_vfs_return_val_if_fail (parent != NULL, NULL);

  /* skip additional slashes */
  for (; G_UNLIKELY (*s == G_DIR_SEPARATOR); ++s)
    ;

  /* generate the additional path components (if any) */
  while (*s != '\0')
    {
      /* remember the current path as parent path */
      parent = path;

      /* determine the length of the path component in bytes */
      for (s1 = s + 1; *s1 != '\0' && *s1 != G_DIR_SEPARATOR; ++s1)
        ;
      n = (((s1 - s) + sizeof (gsize)) / sizeof (gsize)) * sizeof (gsize)
        + sizeof (ThunarVfsPath);

      /* allocate memory for the new path component */
      path = _thunar_vfs_slice_alloc (n);
      path->ref_count = thunar_vfs_path_get_scheme (parent);
      path->parent = thunar_vfs_path_ref (parent);

      /* insert the path into the debug list */
      THUNAR_VFS_PATH_DEBUG_INSERT (path);

      /* zero out the last word to have the name zero-terminated */
      *(((gsize *) (((gchar *) path) + n)) - 1) = 0;

      /* copy the path component name */
      for (t = (gchar *) thunar_vfs_path_get_name (path); *s != '\0' && *s != G_DIR_SEPARATOR; )
        *t++ = *s++;

      /* skip additional slashes */
      for (; G_UNLIKELY (*s == G_DIR_SEPARATOR); ++s)
        ;
    }

  /* return a reference to the path */
  return thunar_vfs_path_ref (path);
}



/**
 * _thunar_vfs_path_child:
 * @parent : a #ThunarVfsPath.
 * @name   : a valid filename in the local file system encoding.
 *
 * Internal implementation of thunar_vfs_path_relative(), that performs
 * no external sanity checking of it's parameters.
 *
 * Returns a #ThunarVfsPath for the file @name relative to
 * @parent. @name must be a valid filename in the local file
 * system encoding and it may not contain any slashes.
 *
 * The caller is responsible to free the returned object
 * using thunar_vfs_path_unref() when no longer needed.
 *
 * Return value: the child path to @name relative to @parent.
 **/
ThunarVfsPath*
_thunar_vfs_path_child (ThunarVfsPath *parent,
                        const gchar   *name)
{
  ThunarVfsPath *path;
  const gchar   *s;
  gchar         *t;
  gint           n;

  _thunar_vfs_return_val_if_fail (parent != NULL, NULL);
  _thunar_vfs_return_val_if_fail (name != NULL, NULL);
  _thunar_vfs_return_val_if_fail (*name != '\0', NULL);
  _thunar_vfs_return_val_if_fail (strchr (name, '/') == NULL, NULL);

  /* check if parent is one of the home path components */
  for (n = n_home_components - 2; n >= 0; --n)
    if (G_UNLIKELY (home_components[n] == parent))
      {
        /* check if the name equals the home path child component */
        if (strcmp (name, thunar_vfs_path_get_name (home_components[n + 1])) == 0)
          return thunar_vfs_path_ref (home_components[n + 1]);
        break;
      }

  /* determine the length of the name in bytes */
  for (s = name + 1; *s != '\0'; ++s)
    ;
  n = (((s - name) + sizeof (gsize)) / sizeof (gsize)) * sizeof (gsize)
    + sizeof (ThunarVfsPath);

  /* allocate memory for the new path component */
  path = _thunar_vfs_slice_alloc (n);
  path->ref_count = 1 | thunar_vfs_path_get_scheme (parent);
  path->parent = thunar_vfs_path_ref (parent);

  /* insert the path into the debug list */
  THUNAR_VFS_PATH_DEBUG_INSERT (path);

  /* zero out the last word to have the name zero-terminated */
  *(((gsize *) (((gchar *) path) + n)) - 1) = 0;

  /* copy the path component name */
  for (s = name, t = (gchar *) thunar_vfs_path_get_name (path); *s != '\0'; )
    *t++ = *s++;

  return path;
}



/**
 * _thunar_vfs_path_dup_display_name:
 * @path : a #ThunarVfsPath.
 *
 * Returns the display name for the @path<!---->s name. That said,
 * the method is similar to thunar_vfs_path_get_name(), but the
 * returned string is garantied to be valid UTF-8.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: a displayable variant of the @path<!---->s name.
 **/
gchar*
_thunar_vfs_path_dup_display_name (const ThunarVfsPath *path)
{
  return g_filename_display_name (thunar_vfs_path_get_name (path));
}



/**
 * _thunar_vfs_path_translate:
 * @src_path   : the source #ThunarVfsPath.
 * @dst_scheme : the destination #ThunarVfsPathScheme.
 * @error      : return location for errors or %NULL.
 *
 * Translates the @src_path to the specified @dst_scheme if possible.
 *
 * If the @src_path is already in the @dst_scheme, a new reference
 * on @src_path will be returned.
 *
 * The caller is responsible to free the returned #ThunarVfsPath using
 * thunar_vfs_path_unref() when no longer needed.
 *
 * Return value: the #ThunarVfsPath that corresponds to @src_path in the
 *               @dst_scheme, or %NULL on error.
 **/
ThunarVfsPath*
_thunar_vfs_path_translate (ThunarVfsPath      *src_path,
                            ThunarVfsPathScheme dst_scheme,
                            GError            **error)
{
  ThunarVfsPathScheme src_scheme;
  ThunarVfsPath      *dst_path = NULL;
  gchar              *absolute_path;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* check if the src_path is already in dst_scheme */
  src_scheme = thunar_vfs_path_get_scheme (src_path);
  if (G_LIKELY (dst_scheme == src_scheme))
    return thunar_vfs_path_ref (src_path);

  /* we can translate trash:-URIs to file:-URIs */
  if (src_scheme == THUNAR_VFS_PATH_SCHEME_TRASH && dst_scheme == THUNAR_VFS_PATH_SCHEME_FILE)
    {
      /* resolve the local path to the trash resource */
      absolute_path = _thunar_vfs_io_trash_path_resolve (src_path, error);
      if (G_LIKELY (absolute_path != NULL))
        {
          /* generate a file:-URI path for the trash resource */
          dst_path = thunar_vfs_path_new (absolute_path, error);
          g_free (absolute_path);
        }
    }
  else
    {
      /* cannot perform the translation */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, g_strerror (EINVAL));
    }

  return dst_path;
}



/**
 * _thunar_vfs_path_translate_dup_string:
 * @src_path   : the source #ThunarVfsPath.
 * @dst_scheme : the destination #ThunarVfsPathScheme.
 * @error      : return location for errors or %NULL.
 *
 * Uses _thunar_vfs_path_translate() and thunar_vfs_path_dup_string()
 * to generate the string representation of the @src_path in the
 * @dst_scheme.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the string representation of the @src_path in the
 *               @dst_scheme, or %NULL in case of an error.
 **/
gchar*
_thunar_vfs_path_translate_dup_string (ThunarVfsPath      *src_path,
                                       ThunarVfsPathScheme dst_scheme,
                                       GError            **error)
{
  ThunarVfsPath *dst_path;
  gchar         *dst_string;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* handle the common case first */
  if (G_LIKELY (dst_scheme == THUNAR_VFS_PATH_SCHEME_FILE))
    {
      if (_thunar_vfs_path_is_local (src_path))
        return thunar_vfs_path_dup_string (src_path);
      else if (_thunar_vfs_path_is_trash (src_path))
        return _thunar_vfs_io_trash_path_resolve (src_path, error);
    }

  /* translate the source path to the destination scheme */
  dst_path = _thunar_vfs_path_translate (src_path, dst_scheme, error);
  if (G_LIKELY (dst_path != NULL))
    {
      /* determine the string representation */
      dst_string = thunar_vfs_path_dup_string (dst_path);
      thunar_vfs_path_unref (dst_path);
    }
  else
    {
      /* we failed to translate */
      dst_string = NULL;
    }

  return dst_string;
}



#define __THUNAR_VFS_PATH_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
