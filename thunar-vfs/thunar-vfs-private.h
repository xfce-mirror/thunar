/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_VFS_PRIVATE_H__
#define __THUNAR_VFS_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-config.h>

G_BEGIN_DECLS;

/* support macros for debugging */
#ifndef NDEBUG
#define _thunar_vfs_assert(expr)                  g_assert (expr)
#define _thunar_vfs_assert_not_reached()          g_assert_not_reached ()
#define _thunar_vfs_return_if_fail(expr)          g_return_if_fail (expr)
#define _thunar_vfs_return_val_if_fail(expr, val) g_return_val_if_fail (expr, (val))
#else
#define _thunar_vfs_assert(expr)                  G_STMT_START{ (void)0; }G_STMT_END
#define _thunar_vfs_assert_not_reached()          G_STMT_START{ (void)0; }G_STMT_END
#define _thunar_vfs_return_if_fail(expr)          G_STMT_START{ (void)0; }G_STMT_END
#define _thunar_vfs_return_val_if_fail(expr, val) G_STMT_START{ (void)0; }G_STMT_END
#endif

/* support macros for the slice allocator */
#if GLIB_CHECK_VERSION(2,10,0)
#define _thunar_vfs_slice_alloc(block_size)             (g_slice_alloc ((block_size)))
#define _thunar_vfs_slice_alloc0(block_size)            (g_slice_alloc0 ((block_size)))
#define _thunar_vfs_slice_free1(block_size, mem_block)  G_STMT_START{ g_slice_free1 ((block_size), (mem_block)); }G_STMT_END
#define _thunar_vfs_slice_new(type)                     (g_slice_new (type))
#define _thunar_vfs_slice_new0(type)                    (g_slice_new0 (type))
#define _thunar_vfs_slice_free(type, ptr)               G_STMT_START{ g_slice_free (type, (ptr)); }G_STMT_END
#else
#define _thunar_vfs_slice_alloc(block_size)             (g_malloc ((block_size)))
#define _thunar_vfs_slice_alloc0(block_size)            (g_malloc0 ((block_size)))
#define _thunar_vfs_slice_free1(block_size, mem_block)  G_STMT_START{ g_free ((mem_block)); }G_STMT_END
#define _thunar_vfs_slice_new(type)                     (g_new (type, 1))
#define _thunar_vfs_slice_new0(type)                    (g_new0 (type, 1))
#define _thunar_vfs_slice_free(type, ptr)               G_STMT_START{ g_free ((ptr)); }G_STMT_END
#endif

/* avoid trivial g_value_get_*() function calls */
#ifdef NDEBUG
#define g_value_get_boolean(v)  (((const GValue *) (v))->data[0].v_int)
#define g_value_get_char(v)     (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uchar(v)    (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_int(v)      (((const GValue *) (v))->data[0].v_int)
#define g_value_get_uint(v)     (((const GValue *) (v))->data[0].v_uint)
#define g_value_get_long(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_ulong(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_int64(v)    (((const GValue *) (v))->data[0].v_int64)
#define g_value_get_uint64(v)   (((const GValue *) (v))->data[0].v_uint64)
#define g_value_get_enum(v)     (((const GValue *) (v))->data[0].v_long)
#define g_value_get_flags(v)    (((const GValue *) (v))->data[0].v_ulong)
#define g_value_get_float(v)    (((const GValue *) (v))->data[0].v_float)
#define g_value_get_double(v)   (((const GValue *) (v))->data[0].v_double)
#define g_value_get_string(v)   (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_param(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_boxed(v)    (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_pointer(v)  (((const GValue *) (v))->data[0].v_pointer)
#define g_value_get_object(v)   (((const GValue *) (v))->data[0].v_pointer)
#endif

/* GType registration routines */
GType     _thunar_vfs_g_type_register_simple    (GType        type_parent,
                                                 const gchar *type_name_static,
                                                 guint        class_size,
                                                 gpointer     class_init,
                                                 guint        instance_size,
                                                 gpointer     instance_init,
                                                 GTypeFlags   flags) G_GNUC_INTERNAL;

/* GType value routines */
void      _thunar_vfs_g_value_array_free        (GValue      *values,
                                                 guint        n_values) G_GNUC_INTERNAL;

/* path scheme checking routines */
gboolean  _thunar_vfs_check_only_local          (GList       *path_list,
                                                 GError     **error) G_GNUC_INTERNAL;

/* error reporting routines */
void      _thunar_vfs_set_g_error_from_errno    (GError     **error,
                                                 gint         serrno) G_GNUC_INTERNAL;
void      _thunar_vfs_set_g_error_from_errno2   (GError     **error,
                                                 gint         serrno,
                                                 const gchar *format,
                                                 ...) G_GNUC_INTERNAL G_GNUC_PRINTF (3, 4);
void      _thunar_vfs_set_g_error_from_errno3   (GError     **error) G_GNUC_INTERNAL;
void      _thunar_vfs_set_g_error_not_supported (GError     **error) G_GNUC_INTERNAL;

/* RFC 2396 support routines */
gchar    *_thunar_vfs_unescape_rfc2396_string   (const gchar *escaped_string,
                                                 gssize       escaped_len,
                                                 const gchar *illegal_escaped_characters,
                                                 gboolean     ascii_must_not_be_escaped,
                                                 GError     **error) G_GNUC_INTERNAL G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_VFS_PRIVATE_H__ */
