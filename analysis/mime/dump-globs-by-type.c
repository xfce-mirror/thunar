/* $Id$ */
/*-
 * dump-globs-by-type - Simple analysis tool used to dump the
 *                      globs from the Shared MIME Database by
 *                      their type (Literal pattern, Simple
 *                      pattern or Complex pattern).
 *
 * Compile with:
 *  cc -o dump-globs-by-type dump-globs-by-type.c \
 *    `pkg-config --cflags --libs libxfce4util-1.0`
 *
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxfce4util/libxfce4util.h>


typedef enum
{
  PATTERN_LITERAL,
  PATTERN_SIMPLE,
  PATTERN_COMPLEX,
  NUM_PATTERNS
} PatternType;

struct ParsedPattern
{
  gchar *pattern;
  gchar *type;
};

typedef struct
{
  GSList *parsed_patterns[NUM_PATTERNS];
  GMemChunk *pp_chunk;
} GlobsLoader;


static const gchar *names[NUM_PATTERNS] =
{
  "Literal",
  "Simple",
  "Complex"
};


static void
globs_loader_parse_file (GlobsLoader *loader,
                         const gchar *path)
{
  gchar line[1024];
  FILE *fp;
  gchar *p, *pattern, *mime_type;
  PatternType pattern_type;
  struct ParsedPattern *pp;
  GSList *lp;

  fp = fopen (path, "r");
  if (G_UNLIKELY (fp == NULL))
    return;

  for (;;)
    {
next:
      if (fgets (line, 1024, fp) == NULL)
        break;

      /* skip space on the beginning of the line */
      for (p = line; isspace (*p); ++p)
        ;

      /* skip empty or commented lines */
      if (*p == '#' || *p == '\0')
        continue;

      /* skip over to ':' */
      for (mime_type = p, ++p; *p != ':' && *p != '\0'; ++p)
        ;

      /* ignore broken lines */
      if (*p == '\0')
        continue;

      /* terminate the type string */
      *p++ = '\0';

      /* determine the pattern type */
      pattern_type = (*p == '*') ? PATTERN_SIMPLE : PATTERN_LITERAL;
      for (pattern = p, ++p; !isspace (*p) && *p != '\0'; ++p)
        if (*p == '*' || *p == '?' || *p == ']')
          pattern_type = PATTERN_COMPLEX;

      *p = '\0';

      /* check if this pattern is already listed for our type */
      for (lp = loader->parsed_patterns[pattern_type]; lp != NULL; lp = lp->next)
        {
          pp = lp->data;
          if (strcmp (pp->pattern, pattern) == 0)
            goto next;
        }

      /* prepend the new pattern */
      pp = g_chunk_new (struct ParsedPattern, loader->pp_chunk);
      pp->pattern = g_strdup (pattern);
      pp->type = g_strdup (mime_type);
      loader->parsed_patterns[pattern_type] = g_slist_prepend (loader->parsed_patterns[pattern_type], pp);
    }

  fclose (fp);
}


static gint
ppcmp (gconstpointer a, gconstpointer b)
{
  return strlen (((struct ParsedPattern *) b)->pattern)
       - strlen (((struct ParsedPattern *) a)->pattern);
}


static void
globs_loader_init (GlobsLoader *loader)
{
  gchar **files;
  guint   n;

  loader->pp_chunk = g_mem_chunk_create (struct ParsedPattern, 512, G_ALLOC_ONLY);

  for (n = 0; n < NUM_PATTERNS; ++n)
    loader->parsed_patterns[n] = NULL;

  files = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, "mime/globs");
  for (n = 0; files[n] != NULL; ++n)
    globs_loader_parse_file (loader, files[n]);
  g_strfreev (files);

  for (n = 0; n < NUM_PATTERNS; ++n)
    loader->parsed_patterns[n] = g_slist_sort (loader->parsed_patterns[n], ppcmp);
}


int
main (int argc, char **argv)
{
  struct ParsedPattern *pp;
  GlobsLoader           loader;
  GSList               *lp;
  guint                 n;

  globs_loader_init (&loader);

  for (n = 0; n < NUM_PATTERNS; ++n)
    {
      g_print ("%s patterns (%d):\n", names[n], g_slist_length (loader.parsed_patterns[n]));
      for (lp = loader.parsed_patterns[n]; lp != NULL; lp = lp->next)
        {
          pp = lp->data;
          g_print (" %s -> %s\n", pp->pattern, pp->type);
        }
      g_print ("\n");
    }

  return 0;
}

