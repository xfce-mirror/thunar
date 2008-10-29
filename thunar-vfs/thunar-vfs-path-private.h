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

#if !defined(THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __THUNAR_VFS_PATH_PRIVATE_H__
#define __THUNAR_VFS_PATH_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-path.h>

G_BEGIN_DECLS;

/* Support macros for compilers that don't support proper inlining */
#if !defined(G_CAN_INLINE) && !defined(__THUNAR_VFS_PATH_C__) && !defined(__THUNAR_VFS_INFO_C__)

#define thunar_vfs_path_ref(path)         (exo_atomic_inc (&(THUNAR_VFS_PATH ((path))->ref_count)), path)
#define thunar_vfs_path_is_root(path)     (THUNAR_VFS_PATH ((path))->parent == NULL)
#define thunar_vfs_path_get_name(path)    (((const gchar *) path) + sizeof (ThunarVfsPath))
#define thunar_vfs_path_get_parent(path)  (THUNAR_VFS_PATH ((path))->parent)
#define thunar_vfs_path_get_scheme(path)  (THUNAR_VFS_PATH ((path))->ref_count & THUNAR_VFS_PATH_SCHEME_MASK)

#endif /* !defined(G_CAN_INLINE) && !defined(__THUNAR_VFS_PATH_C__) && !defined(__THUNAR_VFS_INFO_C__) */

/* global shared variables */
extern ThunarVfsPath *_thunar_vfs_path_trash_root G_GNUC_INTERNAL;

/* initialization/shutdown routines */
void _thunar_vfs_path_init     (void) G_GNUC_INTERNAL;
void _thunar_vfs_path_shutdown (void) G_GNUC_INTERNAL;

/* internal support methods */
ThunarVfsPath *_thunar_vfs_path_new_relative          (ThunarVfsPath        *parent,
                                                       const gchar          *relative_path) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarVfsPath *_thunar_vfs_path_child                 (ThunarVfsPath        *parent,
                                                       const gchar          *name) G_GNUC_INTERNAL G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_path_dup_display_name      (const ThunarVfsPath  *path) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarVfsPath *_thunar_vfs_path_translate             (ThunarVfsPath        *src_path,
                                                       ThunarVfsPathScheme   dst_scheme,
                                                       GError              **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar         *_thunar_vfs_path_translate_dup_string  (ThunarVfsPath        *src_path,
                                                       ThunarVfsPathScheme   dst_scheme,
                                                       GError              **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

/**
 * _thunar_vfs_path_unref_nofree:
 * @path : a #ThunarVfsPath.
 *
 * Decrements the reference count on @path without checking whether the
 * reference count reached zero and thereby freeing the @path resources.
 * Use this method with care and only if you are absolutely sure that
 * the reference count is larger than one. Otherwise you'll leak the
 * @path memory.
 **/
#define _thunar_vfs_path_unref_nofree(path)                 \
G_STMT_START{                                               \
  (void)exo_atomic_dec (&(THUNAR_VFS_PATH ((path))->ref_count));  \
}G_STMT_END

/**
 * _thunar_vfs_path_is_local:
 * @path : a #ThunarVfsPath.
 *
 * Returns %TRUE if the @path<!---->s scheme is %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if @path is %THUNAR_VFS_PATH_SCHEME_FILE.
 **/
#define _thunar_vfs_path_is_local(path) (thunar_vfs_path_get_scheme ((path)) == THUNAR_VFS_PATH_SCHEME_FILE)

/**
 * _thunar_vfs_path_is_trash:
 * @path : a #ThunarVfsPath.
 *
 * Returns %TRUE if the @path<!---->s scheme is %THUNAR_VFS_PATH_SCHEME_TRASH.
 *
 * Return value: %TRUE if @path is %THUNAR_VFS_PATH_SCHEME_TRASH.
 **/
#define _thunar_vfs_path_is_trash(path) (thunar_vfs_path_get_scheme ((path)) == THUNAR_VFS_PATH_SCHEME_TRASH)

G_END_DECLS;

#endif /* !__THUNAR_VFS_PATH_PRIVATE_H__ */

