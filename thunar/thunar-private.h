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

#ifndef __THUNAR_PRIVATE_H__
#define __THUNAR_PRIVATE_H__

#include <glib-object.h>

G_BEGIN_DECLS;

/* support macros for the slice allocator */
#if GLIB_CHECK_VERSION(2,10,0)
#define _thunar_slice_alloc(block_size)             (g_slice_alloc ((block_size)))
#define _thunar_slice_alloc0(block_size)            (g_slice_alloc0 ((block_size)))
#define _thunar_slice_free1(block_size, mem_block)  G_STMT_START{ g_slice_free1 ((block_size), (mem_block)); }G_STMT_END
#define _thunar_slice_new(type)                     (g_slice_new (type))
#define _thunar_slice_new0(type)                    (g_slice_new0 (type))
#define _thunar_slice_free(type, ptr)               G_STMT_START{ g_slice_free (type, (ptr)); }G_STMT_END
#else
#define _thunar_slice_alloc(block_size)             (g_malloc ((block_size)))
#define _thunar_slice_alloc0(block_size)            (g_malloc0 ((block_size)))
#define _thunar_slice_free1(block_size, mem_block)  G_STMT_START{ g_free ((mem_block)); }G_STMT_END
#define _thunar_slice_new(type)                     (g_new (type, 1))
#define _thunar_slice_new0(type)                    (g_new0 (type, 1))
#define _thunar_slice_free(type, ptr)               G_STMT_START{ g_free ((ptr)); }G_STMT_END
#endif

/* avoid trivial g_value_get_*() function calls */
#ifndef G_ENABLE_DEBUG
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

G_END_DECLS;

#endif /* !__THUNAR_PRIVATE_H__ */
