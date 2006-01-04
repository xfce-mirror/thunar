/* $Id$ */
/*-
 * Copyright (c) 1999-2000 Andrew Tridgell
 * Copyright (c) 2000      Paul `Rusty' Russell
 * Copyright (c) 2000      Jeremy Allison
 * Copyright (c) 2001      Andrew Esh
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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
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

/* a tdb tool for manipulating a tdb database */

#define FSTRING_LEN 256
typedef char fstring[FSTRING_LEN];

typedef struct connections_key {
  pid_t pid;
  int cnum;
  fstring name;
} connections_key;

typedef struct connections_data {
  int magic;
  pid_t pid;
  int cnum;
  uid_t uid;
  gid_t gid;
  char name[24];
  char addr[24];
  char machine[128];
  time_t start;
} connections_data;

static TDB_CONTEXT *tdb;
FILE *pDumpFile;

static int print_rec(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state);

static char *get_token(int startover)
{
        static char tmp[1024];
  static char *cont = NULL;
  char *insert, *start;
  char *k = strtok(NULL, " ");

  if (!k)
    return NULL;

  if (startover)
    start = tmp;
  else
    start = cont;

  strcpy(start, k);
  insert = start + strlen(start) - 1;
  while (*insert == '\\') {
    *insert++ = ' ';
    k = strtok(NULL, " ");
    if (!k)
      break;
    strcpy(insert, k);
    insert = start + strlen(start) - 1;
  }

  /* Get ready for next call */
  cont = start + strlen(start) + 1;
  return start;
}

static int open_dump_file()
{
  int retval = 0;
  char *tok = get_token(0);
  if (!tok) {
    pDumpFile = stdout;
  } else {
    pDumpFile = fopen(tok, "w");
    
    if (pDumpFile == NULL) {
      printf("File Open Failed! --  %s", tok);
      retval = 1;
    } else {
      printf("Writing to file: %s\n", tok);
    }
  }
    return(retval);
}

static void close_dump_file()
{
  if(pDumpFile != NULL && pDumpFile != stdout) {
    fclose(pDumpFile);
  }
}

 static void print_asc(unsigned char *buf,int len)
{
  int i;

  /* We're probably printing ASCII strings so don't try to display
     the trailing NULL character. */

  if (buf[len - 1] == 0)
          len--;

  for (i=0;i<len;i++)
    fprintf(pDumpFile,"%c",isprint(buf[i])?buf[i]:'.');
}

static void print_data(unsigned char *buf,int len)
{
  int i=0;
  if (len<=0) return;
  fprintf(pDumpFile,"[%03X] ",i);
  for (i=0;i<len;) {
    fprintf(pDumpFile,"%02X ",(int)buf[i]);
    i++;
    if (i%8 == 0) fprintf(pDumpFile," ");
    if (i%16 == 0) {      
      print_asc(&buf[i-16],8); fprintf(pDumpFile," ");
      print_asc(&buf[i-8],8); fprintf(pDumpFile,"\n");
      if (i<len) fprintf(pDumpFile,"[%03X] ",i);
    }
  }
  if (i%16) {
    int n;
    
    n = 16 - (i%16);
    fprintf(pDumpFile," ");
    if (n>8) fprintf(pDumpFile," ");
    while (n--) fprintf(pDumpFile,"   ");
    
    n = i%16;
    if (n > 8) n = 8;
    print_asc(&buf[i-(i%16)],n); fprintf(pDumpFile," ");
    n = (i%16) - n;
    if (n>0) print_asc(&buf[i-n],n); 
    fprintf(pDumpFile,"\n");    
  }
}

static void help(void)
{
  printf(
"tdbtool:\n"
"  create    dbname     : create a database\n"
"  open      dbname     : open an existing database\n"
"  erase                : erase the database\n"
"  dump      dumpname   : dump the database as strings\n"
"  insert    key  data  : insert a record\n"
"  store     key  data  : store a record (replace)\n"
"  show      key        : show a record by key\n"
"  delete    key        : delete a record by key\n"
"  list                 : print the database hash table and freelist\n"
"  free                 : print the database freelist\n"
"  1 | first            : print the first record\n"
"  n | next             : print the next record\n"
"  q | quit             : terminate\n"
"  \\n                   : repeat 'next' command\n");
}

static void terror(char *why)
{
  printf("%s\n", why);
}

static void create_tdb(void)
{
  char *tok = get_token(1);
  if (!tok) {
    help();
    return;
  }
  if (tdb) tdb_close(tdb);
  tdb = tdb_open(tok, 0, TDB_CLEAR_IF_FIRST,
           O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (!tdb) {
    printf("Could not create %s: %s\n", tok, strerror(errno));
  }
}

static void open_tdb(void)
{
  char *tok = get_token(1);
  if (!tok) {
    help();
    return;
  }
  if (tdb) tdb_close(tdb);
  tdb = tdb_open(tok, 0, 0, O_RDWR, 0600);
  if (!tdb) {
    printf("Could not open %s: %s\n", tok, strerror(errno));
  }
}

static void insert_tdb(void)
{
  char *k = get_token(1);
  char *d = get_token(0);
  TDB_DATA key, dbuf;

  if (!k || !d) {
    help();
    return;
  }

  key.dptr = k;
  key.dsize = strlen(k)+1;
  dbuf.dptr = d;
  dbuf.dsize = strlen(d)+1;

  if (tdb_store(tdb, key, dbuf, TDB_INSERT) == -1) {
    terror("insert failed");
  }
}

static void store_tdb(void)
{
  char *k = get_token(1);
  char *d = get_token(0);
  TDB_DATA key, dbuf;

  if (!k || !d) {
    help();
    return;
  }

  key.dptr = k;
  key.dsize = strlen(k)+1;
  dbuf.dptr = d;
  dbuf.dsize = strlen(d)+1;

  printf("Storing key:\n");
  print_rec(tdb, key, dbuf, NULL);

  if (tdb_store(tdb, key, dbuf, TDB_REPLACE) == -1) {
    terror("store failed");
  }
}

static void show_tdb(void)
{
  char *k = get_token(1);
  TDB_DATA key, dbuf;

  if (!k) {
    help();
    return;
  }

  key.dptr = k;
  key.dsize = strlen(k)+1;

  dbuf = tdb_fetch(tdb, key);
  if (!dbuf.dptr) {
    terror("fetch failed");
    return;
  }
  /* printf("%s : %*.*s\n", k, (int)dbuf.dsize, (int)dbuf.dsize, dbuf.dptr); */
  print_rec(tdb, key, dbuf, NULL);
}

static void delete_tdb(void)
{
  char *k = get_token(1);
  TDB_DATA key;

  if (!k) {
    help();
    return;
  }

  key.dptr = k;
  key.dsize = strlen(k)+1;

  if (tdb_delete(tdb, key) != 0) {
    terror("delete failed");
  }
}

static int print_rec(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
  fprintf(pDumpFile,"\nkey %u bytes\n", (unsigned) key.dsize);
  print_asc((unsigned char*)key.dptr, key.dsize);
  fprintf(pDumpFile,"\ndata %u bytes\n", (unsigned) dbuf.dsize);
  print_data((unsigned char*)dbuf.dptr, dbuf.dsize);
  return 0;
}

static int print_key(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
  print_asc((unsigned char*)key.dptr, key.dsize);
  printf("\n");
  return 0;
}

static int total_bytes;

static int traverse_fn(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
  total_bytes += dbuf.dsize;
  return 0;
}

static void info_tdb(void)
{
  int count;
  total_bytes = 0;
  if ((count = tdb_traverse(tdb, traverse_fn, NULL) == -1))
    printf("Error = %s\n", tdb_errorstr(tdb));
  else
    printf("%d records totalling %d bytes\n", count, total_bytes);
}

static char *tdb_getline(char *prompt)
{
  static char line[1024];
  char *p;
  fputs(prompt, stdout);
  line[0] = 0;
  p = fgets(line, sizeof(line)-1, stdin);
  if (p) p = strchr(p, '\n');
  if (p) *p = 0;
  pDumpFile = stdout;
  return p?line:NULL;
}

static int do_delete_fn(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf,
                     void *state)
{
    return tdb_delete(tdb, key);
}

static void first_record(TDB_CONTEXT *tdb, TDB_DATA *pkey)
{
  TDB_DATA dbuf;
  *pkey = tdb_firstkey(tdb);
  
  dbuf = tdb_fetch(tdb, *pkey);
  if (!dbuf.dptr) terror("fetch failed");
  /* printf("%s : %*.*s\n", k, (int)dbuf.dsize, (int)dbuf.dsize, dbuf.dptr); */
  print_rec(tdb, *pkey, dbuf, NULL);
}

static void next_record(TDB_CONTEXT *tdb, TDB_DATA *pkey)
{
  TDB_DATA dbuf;
  *pkey = tdb_nextkey(tdb, *pkey);
  
  dbuf = tdb_fetch(tdb, *pkey);
  if (!dbuf.dptr) 
    terror("fetch failed");
  else
    /* printf("%s : %*.*s\n", k, (int)dbuf.dsize, (int)dbuf.dsize, dbuf.dptr); */
    print_rec(tdb, *pkey, dbuf, NULL);
}

int main(int argc, char *argv[])
{
    int bIterate = 0;
    char *line;
    char *tok;
  TDB_DATA iterate_kbuf;

    if (argv[1]) {
  static char tmp[1024];
        sprintf(tmp, "open %s", argv[1]);
        tok=strtok(tmp," ");
        open_tdb();
    }

    while ((line = tdb_getline("tdb> "))) {

        /* Shell command */
        
        if (line[0] == '!') {
            system(line + 1);
            continue;
        }
        
        if ((tok = strtok(line," "))==NULL) {
           if (bIterate)
              next_record(tdb, &iterate_kbuf);
           continue;
        }
        if (strcmp(tok,"create") == 0) {
            bIterate = 0;
            create_tdb();
            continue;
        } else if (strcmp(tok,"open") == 0) {
            open_tdb();
            continue;
        } else if ((strcmp(tok, "q") == 0) ||
                   (strcmp(tok, "quit") == 0)) {
            break;
  }
            
        /* all the rest require a open database */
        if (!tdb) {
            bIterate = 0;
            terror("database not open");
            help();
            continue;
        }
            
        if (strcmp(tok,"insert") == 0) {
            bIterate = 0;
            insert_tdb();
        } else if (strcmp(tok,"store") == 0) {
            bIterate = 0;
            store_tdb();
        } else if (strcmp(tok,"show") == 0) {
            bIterate = 0;
            show_tdb();
        } else if (strcmp(tok,"erase") == 0) {
            bIterate = 0;
            tdb_traverse(tdb, do_delete_fn, NULL);
        } else if (strcmp(tok,"delete") == 0) {
            bIterate = 0;
            delete_tdb();
        } else if (strcmp(tok,"dump") == 0) {
            bIterate = 0;
      if(open_dump_file() == 0) { //open file
        tdb_traverse(tdb, print_rec, NULL);
        close_dump_file(); //close file
      }
      pDumpFile = stdout;
        } else if (strcmp(tok,"list") == 0) {
            tdb_dump_all(tdb);
        } else if (strcmp(tok, "free") == 0) {
            tdb_printfreelist(tdb);
        } else if (strcmp(tok,"info") == 0) {
            info_tdb();
        } else if ( (strcmp(tok, "1") == 0) ||
                    (strcmp(tok, "first") == 0)) {
            bIterate = 1;
            first_record(tdb, &iterate_kbuf);
        } else if ((strcmp(tok, "n") == 0) ||
                   (strcmp(tok, "next") == 0)) {
            next_record(tdb, &iterate_kbuf);
        } else if ((strcmp(tok, "keys") == 0)) {
                bIterate = 0;
                tdb_traverse(tdb, print_key, NULL);
        } else {
            help();
        }
    }

    if (tdb) tdb_close(tdb);

    return 0;
}
