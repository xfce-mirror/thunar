/* $Id$ */
/*-
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
 * Based on code initially written by Matthias Clasen <mclasen@redhat.com>
 * for the xdgmime library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-legacy.h>



#define THUNAR_VFS_MIME_LEGACY_GLOB(obj)   ((ThunarVfsMimeLegacyGlob *) (obj))
#define THUNAR_VFS_MIME_LEGACY_SUFFIX(obj) ((ThunarVfsMimeLegacySuffix *) (obj))



typedef struct _ThunarVfsMimeLegacyGlob   ThunarVfsMimeLegacyGlob;
typedef struct _ThunarVfsMimeLegacySuffix ThunarVfsMimeLegacySuffix;



static void         thunar_vfs_mime_legacy_class_init             (ThunarVfsMimeLegacyClass *klass);
static void         thunar_vfs_mime_legacy_init                   (ThunarVfsMimeLegacy      *legacy);
static void         thunar_vfs_mime_legacy_finalize               (GObject                  *object);
static const gchar *thunar_vfs_mime_legacy_lookup_data            (ThunarVfsMimeProvider    *provider,
                                                                   gconstpointer             data,
                                                                   gsize                     length,
                                                                   gint                     *priority);
static const gchar *thunar_vfs_mime_legacy_lookup_literal         (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *filename);
static const gchar *thunar_vfs_mime_legacy_lookup_suffix          (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *suffix,
                                                                   gboolean                  ignore_case);
static const gchar *thunar_vfs_mime_legacy_lookup_glob            (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *filename);
static const gchar *thunar_vfs_mime_legacy_lookup_alias           (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *alias);
static guint        thunar_vfs_mime_legacy_lookup_parents         (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *mime_type,
                                                                   gchar                   **parents,
                                                                   guint                     max_parents);
static GList       *thunar_vfs_mime_legacy_get_stop_characters    (ThunarVfsMimeProvider    *provider);
static gsize        thunar_vfs_mime_legacy_get_max_buffer_extents (ThunarVfsMimeProvider    *provider);
static void         thunar_vfs_mime_legacy_parse_aliases          (ThunarVfsMimeLegacy      *legacy,
                                                                   const gchar              *directory);
static gboolean     thunar_vfs_mime_legacy_parse_globs            (ThunarVfsMimeLegacy      *legacy,
                                                                   const gchar              *directory);
static void         thunar_vfs_mime_legacy_parse_subclasses       (ThunarVfsMimeLegacy      *legacy,
                                                                   const gchar              *directory);



struct _ThunarVfsMimeLegacyClass
{
  ThunarVfsMimeProviderClass __parent__;
};

struct _ThunarVfsMimeLegacy
{
  ThunarVfsMimeProvider __parent__;

  GStringChunk              *string_chunk;
  GMemChunk                 *suffix_chunk;
  GMemChunk                 *glob_chunk;

  GHashTable                *literals;
  ThunarVfsMimeLegacySuffix *suffixes;
  GList                     *globs;

  GHashTable                *aliases;
  GHashTable                *parents;
};

struct _ThunarVfsMimeLegacyGlob
{
  const gchar *pattern;
  const gchar *mime_type;
};

struct _ThunarVfsMimeLegacySuffix
{
  ThunarVfsMimeLegacySuffix *child;
  ThunarVfsMimeLegacySuffix *next;
  const gchar               *mime_type;
  gunichar                   character;
};



static GObjectClass *thunar_vfs_mime_legacy_parent_class;



GType
thunar_vfs_mime_legacy_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsMimeLegacyClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_mime_legacy_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsMimeLegacy),
        0,
        (GInstanceInitFunc) thunar_vfs_mime_legacy_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_MIME_PROVIDER,
                                     "ThunarVfsMimeLegacy", &info, 0);
    }

  return type;
}



static void
thunar_vfs_mime_legacy_class_init (ThunarVfsMimeLegacyClass *klass)
{
  ThunarVfsMimeProviderClass *thunarvfs_mime_provider_class;
  GObjectClass               *gobject_class;

  thunar_vfs_mime_legacy_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_mime_legacy_finalize;

  thunarvfs_mime_provider_class = THUNAR_VFS_MIME_PROVIDER_CLASS (klass);
  thunarvfs_mime_provider_class->lookup_data = thunar_vfs_mime_legacy_lookup_data;
  thunarvfs_mime_provider_class->lookup_literal = thunar_vfs_mime_legacy_lookup_literal;
  thunarvfs_mime_provider_class->lookup_suffix = thunar_vfs_mime_legacy_lookup_suffix;
  thunarvfs_mime_provider_class->lookup_glob = thunar_vfs_mime_legacy_lookup_glob;
  thunarvfs_mime_provider_class->lookup_alias = thunar_vfs_mime_legacy_lookup_alias;
  thunarvfs_mime_provider_class->lookup_parents = thunar_vfs_mime_legacy_lookup_parents;
  thunarvfs_mime_provider_class->get_stop_characters = thunar_vfs_mime_legacy_get_stop_characters;
  thunarvfs_mime_provider_class->get_max_buffer_extents = thunar_vfs_mime_legacy_get_max_buffer_extents;
}



static void
thunar_vfs_mime_legacy_init (ThunarVfsMimeLegacy *legacy)
{
  legacy->string_chunk = g_string_chunk_new (1024);
  legacy->glob_chunk = g_mem_chunk_create (ThunarVfsMimeLegacyGlob, 32, G_ALLOC_ONLY);
  legacy->suffix_chunk = g_mem_chunk_create (ThunarVfsMimeLegacySuffix, 128, G_ALLOC_ONLY);

  legacy->literals = g_hash_table_new (g_str_hash, g_str_equal);

  legacy->aliases = g_hash_table_new (g_str_hash, g_str_equal);
  legacy->parents = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) g_list_free);
}



static void
thunar_vfs_mime_legacy_finalize (GObject *object)
{
  ThunarVfsMimeLegacy *legacy = THUNAR_VFS_MIME_LEGACY (object);

  /* free parents hash table */
  g_hash_table_destroy (legacy->parents);

  /* free aliases hash table */
  g_hash_table_destroy (legacy->aliases);

  /* free the list of globs */
  g_list_free (legacy->globs);

  /* free literals hash table */
  g_hash_table_destroy (legacy->literals);

  /* free chunks */
  g_string_chunk_free (legacy->string_chunk);
  g_mem_chunk_destroy (legacy->suffix_chunk);
  g_mem_chunk_destroy (legacy->glob_chunk);

  (*G_OBJECT_CLASS (thunar_vfs_mime_legacy_parent_class)->finalize) (object);
}



static const gchar*
thunar_vfs_mime_legacy_lookup_data (ThunarVfsMimeProvider *provider,
                                    gconstpointer          data,
                                    gsize                  length,
                                    gint                  *priority)
{
  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_literal (ThunarVfsMimeProvider *provider,
                                       const gchar           *filename)
{
  return g_hash_table_lookup (THUNAR_VFS_MIME_LEGACY (provider)->literals, filename);
}



static ThunarVfsMimeLegacySuffix*
suffix_insert (ThunarVfsMimeLegacy       *legacy,
               ThunarVfsMimeLegacySuffix *suffix_node,
               const gchar               *pattern,
               const gchar               *mime_type)
{
  ThunarVfsMimeLegacySuffix *previous;
  ThunarVfsMimeLegacySuffix *node;
  gboolean                   found_node = FALSE;
  gunichar                   character;

  character = g_utf8_get_char (pattern);

  if (suffix_node == NULL || character < suffix_node->character)
    {
      node = g_chunk_new0 (ThunarVfsMimeLegacySuffix, legacy->suffix_chunk);
      node->next = suffix_node;
      node->character = character;
      suffix_node = node;
    }
  else if (character == suffix_node->character)
    {
      node = suffix_node;
    }
  else
    {
      for (previous = suffix_node, node = previous->next; node != NULL; previous = node, node = node->next)
        {
          if (character < node->character)
            {
              node = g_chunk_new0 (ThunarVfsMimeLegacySuffix, legacy->suffix_chunk);
              node->next = previous->next;
              node->character = character;
              previous->next = node;
              found_node = TRUE;
              break;
            }
          else if (character == node->character)
            {
              found_node = TRUE;
              break;
            }
        }

      if (!found_node)
        {
          node = g_chunk_new0 (ThunarVfsMimeLegacySuffix, legacy->suffix_chunk);
          node->next = previous->next;
          node->character = character;
          previous->next = node;
        }
    }

  pattern = g_utf8_next_char (pattern);
  if (G_UNLIKELY (*pattern == '\0'))
    node->mime_type = mime_type;
  else
    node->child = suffix_insert (legacy, node->child, pattern, mime_type);

  return suffix_node;
}



static const gchar*
suffix_lookup (ThunarVfsMimeLegacySuffix *suffix_node,
               const gchar               *filename,
               gboolean                   ignore_case)
{
  ThunarVfsMimeLegacySuffix *node;
  gunichar                   character;

  if (G_UNLIKELY (suffix_node == NULL))
    return NULL;

  character = g_utf8_get_char (filename);
  if (G_UNLIKELY (ignore_case))
    character = g_unichar_tolower (character);

  for (node = suffix_node; node != NULL && character >= node->character; node = node->next)
    if (character == node->character)
      {
        filename = g_utf8_next_char (filename);
        if (*filename == '\0')
          return node->mime_type;
        else
          return suffix_lookup (node->child, filename, ignore_case);
      }

  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_suffix (ThunarVfsMimeProvider *provider,
                                      const gchar           *suffix,
                                      gboolean               ignore_case)
{
  return suffix_lookup (THUNAR_VFS_MIME_LEGACY (provider)->suffixes, suffix, ignore_case);
}



static const gchar*
thunar_vfs_mime_legacy_lookup_glob (ThunarVfsMimeProvider *provider,
                                    const gchar           *filename)
{
  GList *lp;

  for (lp = THUNAR_VFS_MIME_LEGACY (provider)->globs; lp != NULL; lp = lp->next)
    if (fnmatch (THUNAR_VFS_MIME_LEGACY_GLOB (lp->data)->pattern, filename, 0) == 0)
      return THUNAR_VFS_MIME_LEGACY_GLOB (lp->data)->mime_type;

  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_alias (ThunarVfsMimeProvider *provider,
                                     const gchar           *alias)
{
  return g_hash_table_lookup (THUNAR_VFS_MIME_LEGACY (provider)->aliases, alias);
}



static guint
thunar_vfs_mime_legacy_lookup_parents (ThunarVfsMimeProvider *provider,
                                       const gchar           *mime_type,
                                       gchar                **parents,
                                       guint                  max_parents)
{
  GList *lp;
  guint  n = 0;

  /* determine the known parents for the MIME-type */
  lp = g_hash_table_lookup (THUNAR_VFS_MIME_LEGACY (provider)->parents, mime_type);
  for (; lp != NULL && n < max_parents; lp = lp->next, ++n, ++parents)
    *parents = lp->data;

  return n;
}



static GList*
thunar_vfs_mime_legacy_get_stop_characters (ThunarVfsMimeProvider *provider)
{
  ThunarVfsMimeLegacySuffix *node;
  GList                     *stopchars = NULL;

  for (node = THUNAR_VFS_MIME_LEGACY (provider)->suffixes; node != NULL; node = node->next)
    if (node->character < 128u)
      stopchars = g_list_prepend (stopchars, GUINT_TO_POINTER (node->character));

  return stopchars;
}



static gsize
thunar_vfs_mime_legacy_get_max_buffer_extents (ThunarVfsMimeProvider *provider)
{
  return 0;
}



static void
thunar_vfs_mime_legacy_parse_aliases (ThunarVfsMimeLegacy *legacy,
                                      const gchar         *directory)
{
  gchar  line[2048];
  gchar *alias;
  gchar *name;
  gchar *path;
  gchar *lp;
  FILE  *fp;

  /* try to open the "aliases" file */
  path = g_build_filename (directory, "aliases", NULL);
  fp = fopen (path, "r");
  g_free (path);

  /* check if we succeed */
  if (G_UNLIKELY (fp == NULL))
    return;

  /* parse all aliases */
  while (fgets (line, sizeof (line), fp) != NULL)
    {
      /* skip whitespace/comments */
      for (lp = line; g_ascii_isspace (*lp); ++lp);
      if (G_UNLIKELY (*lp == '\0' || *lp == '#'))
        continue;

      /* extract the alias name */
      for (alias = lp; *lp != '\0' && !g_ascii_isspace (*lp); ++lp);
      if (G_UNLIKELY (*lp == '\0' || alias == lp))
        continue;
      *lp++ = '\0';

      /* skip whitespace */
      for (; G_UNLIKELY (g_ascii_isspace (*lp)); ++lp);
      if (G_UNLIKELY (*lp == '\0'))
        continue;

      /* extract the MIME-type name */
      for (name = lp; *lp != '\0' && *lp != '\n' && *lp != '\r'; ++lp);
      if (G_UNLIKELY (name == lp))
        continue;
      *lp = '\0';

      /* insert the alias into the string chunk */
      alias = g_string_chunk_insert_const (legacy->string_chunk, alias);

      /* insert the MIME-type name into the string chunk */
      name = g_string_chunk_insert_const (legacy->string_chunk, name);

      /* insert the association into the aliases hash table */
      g_hash_table_insert (legacy->aliases, alias, name);
    }

  fclose (fp);
}



static gboolean
thunar_vfs_mime_legacy_parse_globs (ThunarVfsMimeLegacy *legacy,
                                    const gchar         *directory)
{
  ThunarVfsMimeLegacyGlob *glob;
  gchar                    line[2048];
  gchar                   *pattern;
  gchar                   *path;
  gchar                   *name;
  gchar                   *lp;
  FILE                    *fp;

  /* try to open the "globs" file */
  path = g_build_filename (directory, "globs", NULL);
  fp = fopen (path, "r");
  g_free (path);

  /* cannot continue */
  if (G_UNLIKELY (fp == NULL))
    return FALSE;

  /* parse all globs */
  while (fgets (line, sizeof (line), fp) != NULL)
    {
      /* skip whitespace/comments */
      for (lp = line; g_ascii_isspace (*lp); ++lp);
      if (*lp == '\0' || *lp == '#')
        continue;

      /* extract the MIME-type name */
      for (name = lp; *lp != '\0' && *lp != ':'; ++lp);
      if (*lp == '\0' || name == lp)
        continue;

      /* extract the pattern */
      for (*lp = '\0', pattern = ++lp; *lp != '\0' && *lp != '\n' && *lp != '\r'; ++lp);
      *lp = '\0';
      if (*pattern == '\0')
        continue;

      /* insert the name into the string chunk */
      name = g_string_chunk_insert_const (legacy->string_chunk, name);

      /* determine the type of the pattern */
      if (strpbrk (pattern, "*?[") == NULL)
        {
          g_hash_table_insert (legacy->literals, g_string_chunk_insert (legacy->string_chunk, pattern), name);
        }
      else if (pattern[0] == '*' && pattern[1] == '.' && strpbrk (pattern + 2, "*?[") == NULL)
        {
          legacy->suffixes = suffix_insert (legacy, legacy->suffixes, pattern + 1, name);
        }
      else
        {
          glob = g_chunk_new (ThunarVfsMimeLegacyGlob, legacy->glob_chunk);
          glob->pattern = g_string_chunk_insert (legacy->string_chunk, pattern);
          glob->mime_type = name;
          legacy->globs = g_list_append (legacy->globs, glob);
        }
    }

  fclose (fp);

  return TRUE;
}



static void
thunar_vfs_mime_legacy_parse_subclasses (ThunarVfsMimeLegacy *legacy,
                                         const gchar         *directory)
{
  gchar  line[2048];
  GList *parents;
  gchar *subclass;
  gchar *name;
  gchar *path;
  gchar *lp;
  FILE  *fp;

  /* try to open the "subclasses" file */
  path = g_build_filename (directory, "subclasses", NULL);
  fp = fopen (path, "r");
  g_free (path);

  /* check if we succeed */
  if (G_UNLIKELY (fp == NULL))
    return;

  /* parse all subclasses */
  while (fgets (line, sizeof (line), fp) != NULL)
    {
      /* skip whitespace/comments */
      for (lp = line; g_ascii_isspace (*lp); ++lp);
      if (G_UNLIKELY (*lp == '\0' || *lp == '#'))
        continue;

      /* extract the subclass name */
      for (subclass = lp; *lp != '\0' && !g_ascii_isspace (*lp); ++lp);
      if (G_UNLIKELY (*lp == '\0' || subclass == lp))
        continue;
      *lp++ = '\0';

      /* skip whitespace */
      for (; G_UNLIKELY (g_ascii_isspace (*lp)); ++lp);
      if (G_UNLIKELY (*lp == '\0'))
        continue;

      /* extract the MIME-type name */
      for (name = lp; *lp != '\0' && *lp != '\n' && *lp != '\r'; ++lp);
      if (G_UNLIKELY (name == lp))
        continue;
      *lp = '\0';

      /* insert the subclass into the string chunk */
      subclass = g_string_chunk_insert_const (legacy->string_chunk, subclass);

      /* insert the MIME-type name into the string chunk */
      name = g_string_chunk_insert_const (legacy->string_chunk, name);

      /* add the MIME-type name to the list of parents for the subclass */
      parents = g_hash_table_lookup (legacy->parents, subclass);
      if (G_UNLIKELY (parents != NULL))
        parents = g_list_copy (parents);
      parents = g_list_append (parents, name);
      g_hash_table_insert (legacy->parents, subclass, parents);
    }

  fclose (fp);
}



/**
 * thunar_vfs_mime_legacy_new:
 * @directory : an XDG mime base directory.
 *
 * Allocates a new #ThunarVfsMimeLegacy for @directory and
 * returns the instance on success, or %NULL on error.
 *
 * The caller is responsible to free the returned instance
 * using g_object_unref().
 *
 * Return value: the newly allocated #ThunarVfsMimeLegacy
 *               instance or %NULL on error.
 **/
ThunarVfsMimeProvider*
thunar_vfs_mime_legacy_new (const gchar *directory)
{
  ThunarVfsMimeLegacy *legacy;

  /* allocate the new object */
  legacy = g_object_new (THUNAR_VFS_TYPE_MIME_LEGACY, NULL);

  /* try to parse the globs file */
  if (!thunar_vfs_mime_legacy_parse_globs (legacy, directory))
    {
      g_object_unref (legacy);
      return NULL;
    }

  /* parse the aliases file (optional) */
  thunar_vfs_mime_legacy_parse_aliases (legacy, directory);

  /* parse the subclasses file (optional) */
  thunar_vfs_mime_legacy_parse_subclasses (legacy, directory);

  /* we got it */
  return THUNAR_VFS_MIME_PROVIDER (legacy);
}



