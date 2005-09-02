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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-cache.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#endif



#define CACHE_MAJOR_VERSION (1)
#define CACHE_MINOR_VERSION (0)

#define CACHE_READ16(cache, offset) (GUINT16_FROM_BE (*((guint16 *) ((cache) + (offset)))))
#define CACHE_READ32(cache, offset) (GUINT32_FROM_BE (*((guint32 *) ((cache) + (offset)))))



static void         thunar_vfs_mime_cache_class_init             (ThunarVfsMimeCacheClass *klass);
static void         thunar_vfs_mime_cache_finalize               (ExoObject               *object);
static const gchar *thunar_vfs_mime_cache_lookup_data            (ThunarVfsMimeProvider   *provider,
                                                                  gconstpointer            data,
                                                                  gsize                    length,
                                                                  gint                    *priority);
static const gchar *thunar_vfs_mime_cache_lookup_literal         (ThunarVfsMimeProvider   *provider,
                                                                  const gchar             *filename);
static const gchar *thunar_vfs_mime_cache_lookup_suffix          (ThunarVfsMimeProvider   *provider,
                                                                  const gchar             *suffix,
                                                                  gboolean                 ignore_case);
static const gchar *thunar_vfs_mime_cache_lookup_glob            (ThunarVfsMimeProvider   *provider,
                                                                  const gchar             *filename);
static const gchar *thunar_vfs_mime_cache_lookup_alias           (ThunarVfsMimeProvider   *provider,
                                                                  const gchar             *alias);
static guint        thunar_vfs_mime_cache_lookup_parents         (ThunarVfsMimeProvider   *provider,
                                                                  const gchar             *mime_type,
                                                                  gchar                  **parents,
                                                                  guint                    max_parents);
static GList       *thunar_vfs_mime_cache_get_stop_characters    (ThunarVfsMimeProvider   *provider);
static gsize        thunar_vfs_mime_cache_get_max_buffer_extents (ThunarVfsMimeProvider   *provider);



struct _ThunarVfsMimeCacheClass
{
  ThunarVfsMimeProviderClass __parent__;
};

struct _ThunarVfsMimeCache
{
  ThunarVfsMimeProvider __parent__;

  gchar *buffer;
  gsize  bufsize;
};



static ExoObjectClass *thunar_vfs_mime_cache_parent_class;



GType
thunar_vfs_mime_cache_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsMimeCacheClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_mime_cache_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsMimeCache),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_MIME_PROVIDER,
                                     "ThunarVfsMimeCache", &info, 0);
    }

  return type;
}



static void
thunar_vfs_mime_cache_class_init (ThunarVfsMimeCacheClass *klass)
{
  ThunarVfsMimeProviderClass *thunarvfs_mime_provider_class;
  ExoObjectClass             *exoobject_class;

  thunar_vfs_mime_cache_parent_class = g_type_class_peek_parent (klass);

  exoobject_class = EXO_OBJECT_CLASS (klass);
  exoobject_class->finalize = thunar_vfs_mime_cache_finalize;

  thunarvfs_mime_provider_class = THUNAR_VFS_MIME_PROVIDER_CLASS (klass);
  thunarvfs_mime_provider_class->lookup_data = thunar_vfs_mime_cache_lookup_data;
  thunarvfs_mime_provider_class->lookup_literal = thunar_vfs_mime_cache_lookup_literal;
  thunarvfs_mime_provider_class->lookup_suffix = thunar_vfs_mime_cache_lookup_suffix;
  thunarvfs_mime_provider_class->lookup_glob = thunar_vfs_mime_cache_lookup_glob;
  thunarvfs_mime_provider_class->lookup_alias = thunar_vfs_mime_cache_lookup_alias;
  thunarvfs_mime_provider_class->lookup_parents = thunar_vfs_mime_cache_lookup_parents;
  thunarvfs_mime_provider_class->get_stop_characters = thunar_vfs_mime_cache_get_stop_characters;
  thunarvfs_mime_provider_class->get_max_buffer_extents = thunar_vfs_mime_cache_get_max_buffer_extents;
}



static void
thunar_vfs_mime_cache_finalize (ExoObject *object)
{
  ThunarVfsMimeCache *cache = THUNAR_VFS_MIME_CACHE (object);

#ifdef HAVE_MMAP
  if (G_LIKELY (cache->buffer != NULL))
    munmap (cache->buffer, cache->bufsize);
#endif

  (*EXO_OBJECT_CLASS (thunar_vfs_mime_cache_parent_class)->finalize) (object);
}



static gboolean
cache_magic_matchlet_compare_to_data (const gchar  *buffer,
                                      guint32       offset,
                                      gconstpointer data,
                                      gsize         length)
{
  gboolean valid_matchlet;
  guint32  range_start = CACHE_READ32 (buffer, offset);
  guint32  range_length = CACHE_READ32 (buffer, offset + 4);
  guint32  data_length = CACHE_READ32 (buffer, offset + 12);
  guint32  data_offset = CACHE_READ32 (buffer, offset + 16);
  guint32  mask_offset = CACHE_READ32 (buffer, offset + 20);
  guint32  i, j;

  for (i = range_start; i <= range_start + range_length; i++)
    {
      valid_matchlet = TRUE;
      
      if (i + data_length > length)
        return FALSE;

      if (mask_offset)
        {
          for (j = 0; j < data_length; j++)
            if ((buffer[data_offset + j] & buffer[mask_offset + j]) != ((((gchar *) data)[j + i]) & buffer[mask_offset + j]))
              {
                valid_matchlet = FALSE;
                break;
              }
        }
      else
        {
          for (j = 0; j < data_length; j++)
            if (buffer[data_offset + j] != ((gchar *) data)[j + i])
              {
                valid_matchlet = FALSE;
                break;
              }
        }
          
      if (valid_matchlet)
        return TRUE;
    }
  
  return FALSE;  
}



static gboolean
cache_magic_matchlet_compare (const gchar  *buffer,
                              guint32       offset,
                              gconstpointer data,
                              gsize         length)
{
  guint32 n_children = CACHE_READ32 (buffer, offset + 24);
  guint32 child_offset = CACHE_READ32 (buffer, offset + 28);
  guint32 i;
  
  if (cache_magic_matchlet_compare_to_data (buffer, offset, data, length))
    {
      if (n_children == 0)
        return TRUE;
      
      for (i = 0; i < n_children; i++)
        if (cache_magic_matchlet_compare (buffer, child_offset + 32 * i, data, length))
          return TRUE;
    }
  
  return FALSE;  
}



static const gchar*
thunar_vfs_mime_cache_lookup_data (ThunarVfsMimeProvider *provider,
                                   gconstpointer          data,
                                   gsize                  length,
                                   gint                  *priority)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      matchlet_offset;
  guint32      offset;
  guint32      n, m;

  offset = CACHE_READ32 (buffer, 24);
  n = CACHE_READ32 (buffer, offset);
  offset = CACHE_READ32 (buffer, offset + 8);

  for (; n-- > 0; offset += 16)
    {
      matchlet_offset = CACHE_READ32 (buffer, offset + 12);
      for (m = CACHE_READ32 (buffer, offset + 8); m-- > 0; matchlet_offset += 32)
        if (cache_magic_matchlet_compare (buffer, matchlet_offset, data, length))
          {
            if (G_LIKELY (priority != NULL))
              *priority = (gint) CACHE_READ32 (buffer, offset);
            return buffer + CACHE_READ32 (buffer, offset + 4);
          }
    }

  return NULL;
}



static const gchar*
thunar_vfs_mime_cache_lookup_literal (ThunarVfsMimeProvider *provider,
                                      const gchar           *filename)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      list_offset = CACHE_READ32 (buffer, 12);
  guint32      n_entries = CACHE_READ32 (buffer, list_offset);
  guint32      offset;
  gint         min;
  gint         mid;
  gint         max;
  gint         cmp;

  for (min = 0, max = (gint) n_entries - 1; max >= min; )
    {
      mid = (min + max) / 2;

      offset = CACHE_READ32 (buffer, list_offset + 4 + 8 * mid);
      cmp = strcmp (buffer + offset, filename);

      if (cmp < 0)
        min = mid + 1;
      else if (cmp > 0)
        max = mid - 1;
      else
        return buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * mid + 4);
    }

  return NULL;
}



static const gchar*
cache_node_lookup_suffix (const gchar *buffer,
                          guint32      n_entries,
                          guint32      offset,
                          const gchar *suffix, 
                          gboolean     ignore_case)
{
  gunichar  character;
  gunichar  match_char;
  gint      min, max, mid;

next:
  character = g_utf8_get_char (suffix);
  if (ignore_case)
    character = g_unichar_tolower (character);

  for (min = 0, max = (gint) n_entries - 1; max >= min; )
    {
      mid = (min + max) /  2;

      match_char = (gunichar) CACHE_READ32 (buffer, offset + 16 * mid);

      if (match_char < character)
        min = mid + 1;
      else if (match_char > character)
        max = mid - 1;
      else 
        {
          suffix = g_utf8_next_char (suffix);
          if (*suffix == '\0')
            {
              return buffer + CACHE_READ32 (buffer, offset + 16 * mid + 4);
            }
          else
            {
              /* We emulate a recursive call to cache_node_lookup_suffix()
               * here. This optimization works because the algorithm is
               * tail-recursive. The goto is not necessarily nice, but it
               * works for our purpose and doesn't decrease readability.
               * If we'd use a recursive call here, the code would look
               * like this:
               *
               * return cache_node_lookup_suffix (buffer, CACHE_READ32 (buffer, offset + 16 * mid + 8),
               *                                  CACHE_READ32 (buffer, offset + 16 * mid + 12),
               *                                  suffix, ignore_case);
               */
              n_entries = CACHE_READ32 (buffer, offset + 16 * mid + 8);
              offset = CACHE_READ32 (buffer, offset + 16 * mid + 12);
              goto next;
            }
        }
    }

  return NULL;
}



static const gchar*
thunar_vfs_mime_cache_lookup_suffix (ThunarVfsMimeProvider *provider,
                                     const gchar           *suffix,
                                     gboolean               ignore_case)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      offset = CACHE_READ32 (buffer, 16);

  g_return_val_if_fail (g_utf8_validate (suffix, -1, NULL), NULL);

  return cache_node_lookup_suffix (buffer, CACHE_READ32 (buffer, offset),
                                   CACHE_READ32 (buffer, offset + 4),
                                   suffix, ignore_case);
}



static const gchar*
thunar_vfs_mime_cache_lookup_glob (ThunarVfsMimeProvider *provider,
                                   const gchar           *filename)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      list_offset = CACHE_READ32 (buffer, 20);
  guint32      n_entries = CACHE_READ32 (buffer, list_offset);
  guint32      n;

  for (n = 0; n < n_entries; ++n)
    if (fnmatch (buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * n), filename, 0) == 0)
      return buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * n + 4);

  return NULL;
}



static const gchar*
thunar_vfs_mime_cache_lookup_alias (ThunarVfsMimeProvider *provider,
                                    const gchar           *alias)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      list_offset = CACHE_READ32 (buffer, 4);
  gint         max;
  gint         mid;
  gint         min;
  gint         n;

  for (max = ((gint) CACHE_READ32 (buffer, list_offset)) - 1, min = 0; max >= min; )
    {
      mid = (min + max) / 2;

      n = strcmp (buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * mid), alias);
      if (n < 0)
        min = mid + 1;
      else if (n > 0)
        max = mid - 1;
      else
        return buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * mid + 4);
    }

  return NULL;
}



static guint
thunar_vfs_mime_cache_lookup_parents (ThunarVfsMimeProvider *provider,
                                      const gchar           *mime_type,
                                      gchar                **parents,
                                      guint                  max_parents)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  guint32      parents_offset;
  guint32      list_offset = CACHE_READ32 (buffer, 8);
  guint32      n_entries = CACHE_READ32 (buffer, list_offset);
  guint32      n_parents;
  guint32      i, j, l;

  for (i = j = 0; i < n_entries && j < max_parents; ++i)
    if (strcmp (buffer + CACHE_READ32 (buffer, list_offset + 4 + 8 * i), mime_type) == 0)
      {
        parents_offset = CACHE_READ32 (buffer, list_offset + 4 + 8 * i + 4);
        n_parents = CACHE_READ32 (buffer, parents_offset);

        for (l = 0; l < n_parents && j < max_parents; ++l, ++j)
          parents[j] = (gchar *) (buffer + CACHE_READ32 (buffer, parents_offset + 4 + 4 * l));
      }

  return j;
}



static GList*
thunar_vfs_mime_cache_get_stop_characters (ThunarVfsMimeProvider *provider)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;
  gunichar     character;
  guint32      offset = CACHE_READ32 (buffer, 16);
  guint32      n = CACHE_READ32 (buffer, offset);
  GList       *list = NULL;

  for (offset = CACHE_READ32 (buffer, offset + 4); n-- > 0; offset += 16)
    {
      /* query the suffix start character */
      character = (gunichar) CACHE_READ32 (buffer, offset);
      if (G_UNLIKELY (character >= 128))
        continue;

      /* prepend the character to the list (if not already present) */
      if (G_UNLIKELY (g_list_find (list, GUINT_TO_POINTER (character)) == NULL))
        list = g_list_prepend (list, GUINT_TO_POINTER (character));
    }

  return list;
}



static gsize
thunar_vfs_mime_cache_get_max_buffer_extents (ThunarVfsMimeProvider *provider)
{
  const gchar *buffer = THUNAR_VFS_MIME_CACHE (provider)->buffer;

  /* get the MAX_EXTENTS entry from the MagicList */
  return CACHE_READ32 (buffer, CACHE_READ32 (buffer, 24) + 4);
}



/**
 * thunar_vfs_mime_cache_new:
 * @directory : the mime base directory.
 *
 * Creates a new #ThunarVfsMimeCache for @directory. Returns
 * %NULL if for some reason, @directory could not be opened
 * as a #ThunarVfsMimeCache.
 *
 * The caller is responsible to call #exo_object_unref()
 * on the returned instance.
 *
 * Return value: a #ThunarVfsMimeCache for @directory or %NULL.
 **/
ThunarVfsMimeProvider*
thunar_vfs_mime_cache_new (const gchar *directory)
{
  ThunarVfsMimeCache *cache = NULL;

#ifdef HAVE_MMAP
  struct stat         stat;
  gchar              *buffer;
  gchar              *path;
  gint                fd;

  /* try to open the mime.cache file */
  path = g_build_filename (directory, "mime.cache", NULL);
  fd = g_open (path, O_RDONLY, 0);
  g_free (path);

  if (G_UNLIKELY (fd < 0))
    return NULL;

  /* stat the file to get proper size info */
  if (fstat (fd, &stat) < 0 || stat.st_size < 4)
    goto done;

  /* try to map the file into memory */
  buffer = (gchar *) mmap (NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (G_UNLIKELY (buffer == MAP_FAILED))
    goto done;

  /* check that we actually support the file version */
  if (CACHE_READ16 (buffer, 0) != CACHE_MAJOR_VERSION || CACHE_READ16 (buffer, 2) != CACHE_MINOR_VERSION)
    {
      munmap (buffer, stat.st_size);
      goto done;
    }

  /* allocate a new cache provider */
  cache = exo_object_new (THUNAR_VFS_TYPE_MIME_CACHE);
  cache->buffer = buffer;
  cache->bufsize = stat.st_size;

  /* tell the system that we'll use this buffer quite often */
#ifdef HAVE_POSIX_MADVISE
  posix_madvise (buffer, stat.st_size, POSIX_MADV_WILLNEED);
#endif

  /* cleanup */
done:
  if (G_LIKELY (fd >= 0))
    close (fd);
#endif /* !HAVE_MMAP */

  return (ThunarVfsMimeProvider *) cache;
}




