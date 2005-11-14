/* $Id$ */
/*-
 * Copyright (c) 1999-2004 Andrew Tridgell <tridge@linuxcare.com>
 * Copyright (c) 2005      Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_h
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <tdb/tdb.h>

/* this tests tdb by doing lots of ops from several simultaneous
   writers - that stresses the locking code. Build with TDB_DEBUG=1
   for best effect */



#define REOPEN_PROB 30
#define DELETE_PROB 8
#define STORE_PROB 4
#define APPEND_PROB 6
#define LOCKSTORE_PROB 0
#define TRAVERSE_PROB 20
#define CULL_PROB 100
#define KEYLEN 3
#define DATALEN 100
#define LOCKLEN 20

static TDB_CONTEXT *db;

static void tdb_log(TDB_CONTEXT *tdb, int level, const char *format, ...)
{
	va_list ap;
    
	va_start(ap, format);
	vfprintf(stdout, format, ap);
	va_end(ap);
	fflush(stdout);
#if 0
	{
		char *ptr;
		asprintf(&ptr,"xterm -e gdb /proc/%d/exe %d", getpid(), getpid());
		system(ptr);
		free(ptr);
	}
#endif	
}

static void fatal(char *why)
{
	perror(why);
	exit(1);
}

static char *randbuf(int len)
{
	char *buf;
	int i;
	buf = (char *)malloc(len+1);

	for (i=0;i<len;i++) {
		buf[i] = 'a' + (rand() % 26);
	}
	buf[i] = 0;
	return buf;
}

static int cull_traverse(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf,
			 void *state)
{
	if (random() % CULL_PROB == 0) {
		tdb_delete(tdb, key);
	}
	return 0;
}

static void addrec_db(void)
{
	int klen, dlen, slen;
	char *k, *d, *s;
	TDB_DATA key, data, lockkey;

	klen = 1 + (rand() % KEYLEN);
	dlen = 1 + (rand() % DATALEN);
	slen = 1 + (rand() % LOCKLEN);

	k = randbuf(klen);
	d = randbuf(dlen);
	s = randbuf(slen);

	key.dptr = k;
	key.dsize = klen+1;

	data.dptr = d;
	data.dsize = dlen+1;

	lockkey.dptr = s;
	lockkey.dsize = slen+1;

#if REOPEN_PROB
	if (random() % REOPEN_PROB == 0) {
		tdb_reopen_all();
		goto next;
	} 
#endif

#if DELETE_PROB
	if (random() % DELETE_PROB == 0) {
		tdb_delete(db, key);
		goto next;
	}
#endif

#if STORE_PROB
	if (random() % STORE_PROB == 0) {
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		goto next;
	}
#endif

#if APPEND_PROB
	if (random() % APPEND_PROB == 0) {
		if (tdb_append(db, key, data) != 0) {
			fatal("tdb_append failed");
		}
		goto next;
	}
#endif

#if LOCKSTORE_PROB
	if (random() % LOCKSTORE_PROB == 0) {
		tdb_chainlock(db, lockkey);
		data = tdb_fetch(db, key);
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		if (data.dptr) free(data.dptr);
		tdb_chainunlock(db, lockkey);
		goto next;
	} 
#endif

#if TRAVERSE_PROB
	if (random() % TRAVERSE_PROB == 0) {
		tdb_traverse(db, cull_traverse, NULL);
		goto next;
	}
#endif

	data = tdb_fetch(db, key);
	if (data.dptr) free(data.dptr);

next:
	free(k);
	free(d);
	free(s);
}

static int traverse_fn(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf,
                       void *state)
{
	tdb_delete(tdb, key);
	return 0;
}

#ifndef NPROC
#define NPROC 6
#endif

#ifndef NLOOPS
#define NLOOPS 200000
#endif

int main(int argc, char *argv[])
{
	int i, seed=0;
	int loops = NLOOPS;
	pid_t pids[NPROC];

	pids[0] = getpid();

	for (i=0;i<NPROC-1;i++) {
		if ((pids[i+1]=fork()) == 0) break;
	}

	db = tdb_open("torture.tdb", 2, TDB_CLEAR_IF_FIRST, 
		      O_RDWR | O_CREAT, 0600);
	if (!db) {
		fatal("db open failed");
	}
	tdb_logging_function(db, tdb_log);

	srand(seed + getpid());
	srandom(seed + getpid() + time(NULL));
	for (i=0;i<loops;i++) addrec_db();

	tdb_traverse(db, NULL, NULL);
	tdb_traverse(db, traverse_fn, NULL);
	tdb_traverse(db, traverse_fn, NULL);

	tdb_close(db);

	if (getpid() == pids[0]) {
		for (i=0;i<NPROC-1;i++) {
			int status;
			if (waitpid(pids[i+1], &status, 0) != pids[i+1]) {
				printf("failed to wait for %d\n",
				       (int)pids[i+1]);
				exit(1);
			}
			if (WEXITSTATUS(status) != 0) {
				printf("child %d exited with status %d\n",
				       (int)pids[i+1], WEXITSTATUS(status));
				exit(1);
			}
		}
		printf("OK\n");
	}

	return 0;
}
