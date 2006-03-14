/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#if !defined (THUNAR_VFS_INSIDE_THUNAR_VFS_H) && !defined (THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __THUNAR_VFS_PATH_H__
#define __THUNAR_VFS_PATH_H__

#include <thunar-vfs/thunar-vfs-config.h>

G_BEGIN_DECLS;

/**
 * THUNAR_VFS_PATH_MAXSTRLEN:
 *
 * The maximum length of a path string in bytes that
 * can be handled by #ThunarVfsPath objects.
 **/
#if defined(PATH_MAX)
#define THUNAR_VFS_PATH_MAXSTRLEN (PATH_MAX + 1)
#else
#define THUNAR_VFS_PATH_MAXSTRLEN (4 * 1024 + 1)
#endif

/**
 * THUNAR_VFS_PATH_MAXURILEN:
 *
 * The maximum length of an URI string in bytes that
 * will be produced by thunar_vfs_path_to_uri() and
 * friends.
 **/
#define THUNAR_VFS_PATH_MAXURILEN (THUNAR_VFS_PATH_MAXSTRLEN * 3 - 2)

#define THUNAR_VFS_TYPE_PATH (thunar_vfs_path_get_type ())

typedef struct _ThunarVfsPath ThunarVfsPath;
struct _ThunarVfsPath
{
  /*< private >*/
  gint           ref_count;
  ThunarVfsPath *parent;
};

GType                        thunar_vfs_path_get_type     (void) G_GNUC_CONST;

ThunarVfsPath               *thunar_vfs_path_new          (const gchar          *identifier,
                                                           GError              **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarVfsPath               *thunar_vfs_path_get_for_home (void) G_GNUC_WARN_UNUSED_RESULT;
ThunarVfsPath               *thunar_vfs_path_get_for_root (void) G_GNUC_WARN_UNUSED_RESULT;

G_INLINE_FUNC ThunarVfsPath *thunar_vfs_path_ref          (ThunarVfsPath        *path);
void                         thunar_vfs_path_unref        (ThunarVfsPath        *path);

guint                        thunar_vfs_path_hash         (gconstpointer         path_ptr);
gboolean                     thunar_vfs_path_equal        (gconstpointer         path_ptr_a,
                                                           gconstpointer         path_ptr_b);

ThunarVfsPath               *thunar_vfs_path_relative     (ThunarVfsPath        *parent,
                                                           const gchar          *name) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean                     thunar_vfs_path_is_ancestor  (const ThunarVfsPath  *path,
                                                           const ThunarVfsPath  *ancestor);
gboolean                     thunar_vfs_path_is_home      (const ThunarVfsPath  *path);
G_INLINE_FUNC gboolean       thunar_vfs_path_is_root      (const ThunarVfsPath  *path);

G_INLINE_FUNC const gchar   *thunar_vfs_path_get_name     (const ThunarVfsPath  *path);
G_INLINE_FUNC ThunarVfsPath *thunar_vfs_path_get_parent   (const ThunarVfsPath  *path);

gchar                       *thunar_vfs_path_dup_string   (const ThunarVfsPath  *path) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gssize                       thunar_vfs_path_to_string    (const ThunarVfsPath  *path,
                                                           gchar                *buffer,
                                                           gsize                 bufsize,
                                                           GError              **error);

gchar                       *thunar_vfs_path_dup_uri      (const ThunarVfsPath  *path) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gssize                       thunar_vfs_path_to_uri       (const ThunarVfsPath  *path,
                                                           gchar                *buffer,
                                                           gsize                 bufsize,
                                                           GError              **error);


GList                       *thunar_vfs_path_list_from_string (const gchar          *uri_string,
                                                               GError              **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar                       *thunar_vfs_path_list_to_string   (GList                *path_list) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
G_INLINE_FUNC GList         *thunar_vfs_path_list_append      (GList                *path_list,
                                                               ThunarVfsPath        *path) G_GNUC_WARN_UNUSED_RESULT;
G_INLINE_FUNC GList         *thunar_vfs_path_list_prepend     (GList                *path_list,
                                                               ThunarVfsPath        *path) G_GNUC_WARN_UNUSED_RESULT;
G_INLINE_FUNC GList         *thunar_vfs_path_list_copy        (GList                *path_list) G_GNUC_WARN_UNUSED_RESULT;
G_INLINE_FUNC void           thunar_vfs_path_list_free        (GList                *path_list);


/* inline function implementations */
#if defined(G_CAN_INLINE) || defined(__THUNAR_VFS_PATH_C__)
/**
 * thunar_vfs_path_ref:
 * @path : a #ThunarVfsPath.
 *
 * Increments the reference count on @path
 * and returns a reference to @path.
 *
 * Return value: a reference to @path.
 **/
G_INLINE_FUNC ThunarVfsPath*
thunar_vfs_path_ref (ThunarVfsPath *path)
{
  exo_atomic_inc (&path->ref_count);
  return path;
}

/**
 * thunar_vfs_path_is_root:
 * @path : a #ThunarVfsPath.
 *
 * Checks whether path refers to the root directory.
 *
 * Return value: %TRUE if @path refers to the root
 *               directory.
 **/
G_INLINE_FUNC gboolean
thunar_vfs_path_is_root (const ThunarVfsPath *path)
{
  return (path->parent == NULL);
}

/**
 * thunar_vfs_path_get_name:
 * @path : a #ThunarVfsPath.
 *
 * Returns the base name of the @path in the local
 * file system encoding.
 *
 * Return value: the base name of @path.
 **/
G_INLINE_FUNC const gchar*
thunar_vfs_path_get_name (const ThunarVfsPath *path)
{
  return ((const gchar *) path) + sizeof (*path);
}

/**
 * thunar_vfs_path_get_parent:
 * @path : a #ThunarVfsPath.
 *
 * Returns the #ThunarVfsPath that refers to the parent
 * directory of @path or %NULL if @path refers to the
 * root file system node.
 *
 * No additional reference is taken on the parent, so
 * you'll need to call thunar_vfs_path_ref() yourself
 * if you need to keep a reference.
 *
 * Return value: the parent of @path or %NULL.
 **/
G_INLINE_FUNC ThunarVfsPath*
thunar_vfs_path_get_parent (const ThunarVfsPath *path)
{
  return path->parent;
}

/**
 * thunar_vfs_path_list_append:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 * @path      : a #ThunarVfsPath.
 *
 * Appends @path to the @path_list while taking
 * an additional reference for @path.
 *
 * Return value: pointer to the extended @path_list.
 **/
G_INLINE_FUNC GList*
thunar_vfs_path_list_append (GList         *path_list,
                             ThunarVfsPath *path)
{
  thunar_vfs_path_ref (path);
  return g_list_append (path_list, path);
}

/**
 * thunar_vfs_path_list_prepend:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 * @path      : a #ThunarVfsPath.
 *
 * Similar to thunar_vfs_path_list_append(), but
 * prepends the @path to the @path_list.
 *
 * Return value: pointer to the extended @path_list.
 **/
G_INLINE_FUNC GList*
thunar_vfs_path_list_prepend (GList         *path_list,
                              ThunarVfsPath *path)
{
  thunar_vfs_path_ref (path);
  return g_list_prepend (path_list, path);
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
G_INLINE_FUNC GList*
thunar_vfs_path_list_copy (GList *path_list)
{
  GList *list;
  GList *lp;

  for (list = NULL, lp = g_list_last (path_list); lp != NULL; lp = lp->prev)
    {
      list = g_list_prepend (list, lp->data);
      thunar_vfs_path_ref (lp->data);
    }

  return list;
}

/**
 * thunar_vfs_path_list_free:
 * @path_list : a list of #ThunarVfsPath<!---->s.
 *
 * Frees the #ThunarVfsPath<!---->s in @path_list and
 * the @path_list itself.
 **/
G_INLINE_FUNC void
thunar_vfs_path_list_free (GList *path_list)
{
  GList *lp;
  for (lp = path_list; lp != NULL; lp = lp->next)
    thunar_vfs_path_unref (lp->data);
  g_list_free (path_list);
}
#endif /* G_CAN_INLINE || __THUNAR_VFS_PATH_C__ */


#if defined(THUNAR_VFS_COMPILATION)
void _thunar_vfs_path_init     (void) G_GNUC_INTERNAL;
void _thunar_vfs_path_shutdown (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__THUNAR_VFS_PATH_H__ */
