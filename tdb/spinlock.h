/* $Id$ */
/*-
 * Copyright (c) 2001 Anton Blanchard <anton@samba.org>
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
 *
 * This file was originally part of the tdb library, which in turn is
 * part of the Samba suite, a Unix SMB/CIFS implementation.
 */

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <tdb/tdb.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_SPINLOCKS

#define RWLOCK_BIAS 0x1000UL

/* OS SPECIFIC */
#define MAX_BUSY_LOOPS 1000

/* ARCH SPECIFIC */
/* We should make sure these are padded to a cache line */
#if defined(SPARC_SPINLOCKS)
typedef volatile char spinlock_t;
#elif defined(POWERPC_SPINLOCKS)
typedef volatile unsigned long spinlock_t;
#elif defined(INTEL_SPINLOCKS)
typedef volatile int spinlock_t;
#elif defined(MIPS_SPINLOCKS)
typedef volatile unsigned long spinlock_t;
#else
#error Need to implement spinlock code in spinlock.h
#endif

typedef struct {
	spinlock_t lock;
	volatile int count;
} tdb_rwlock_t;

#define TDB_SPINLOCK_SIZE(hash_size) (((hash_size) + 1) * sizeof(tdb_rwlock_t))

#else /* !USE_SPINLOCKS */

#define TDB_SPINLOCK_SIZE(hash_size) 0

#endif /* !USE_SPINLOCKS */

int tdb_spinlock(TDB_CONTEXT *tdb, int list, int rw_type);
int tdb_spinunlock(TDB_CONTEXT *tdb, int list, int rw_type);
int tdb_create_rwlocks(int fd, unsigned int hash_size);
int tdb_clear_spinlocks(TDB_CONTEXT *tdb);

#ifdef __cplusplus
}
#endif

#endif /* !__SPINLOCK_H__ */
