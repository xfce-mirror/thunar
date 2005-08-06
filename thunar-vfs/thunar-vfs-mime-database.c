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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
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
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-mime-legacy.h>
#include <thunar-vfs/thunar-vfs-monitor.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#endif



typedef struct _ThunarVfsMimeProviderData ThunarVfsMimeProviderData;



#define THUNAR_VFS_MIME_PROVIDER_DATA(data) ((ThunarVfsMimeProviderData *) (data))



static void               thunar_vfs_mime_database_class_init                 (ThunarVfsMimeDatabaseClass *klass);
static void               thunar_vfs_mime_database_init                       (ThunarVfsMimeDatabase      *database);
static void               thunar_vfs_mime_database_finalize                   (ExoObject                  *object);
static ThunarVfsMimeInfo *thunar_vfs_mime_database_get_info_unlocked          (ThunarVfsMimeDatabase      *database,
                                                                               const gchar                *mime_type);
static ThunarVfsMimeInfo *thunar_vfs_mime_database_get_info_for_name_unlocked (ThunarVfsMimeDatabase      *database,
                                                                               const gchar                *name);



struct _ThunarVfsMimeDatabaseClass
{
  ExoObjectClass __parent__;
};

struct _ThunarVfsMimeDatabase
{
  ExoObject __parent__;

  GMutex           *lock;

  ThunarVfsMonitor *monitor;

  GHashTable       *infos;
  GList            *providers;

  gsize             max_buffer_extents;
  gchar            *stopchars;
};

struct _ThunarVfsMimeProviderData
{
  ThunarVfsMonitorHandle *handle;
  ThunarVfsMimeProvider  *provider;
  ThunarVfsURI           *uri;
};



static ThunarVfsMimeDatabase *thunar_vfs_mime_database_shared_instance = NULL;



G_DEFINE_TYPE (ThunarVfsMimeDatabase, thunar_vfs_mime_database, EXO_TYPE_OBJECT);



static void
thunar_vfs_mime_database_class_init (ThunarVfsMimeDatabaseClass *klass)
{
  ExoObjectClass *exoobject_class;

  exoobject_class = EXO_OBJECT_CLASS (klass);
  exoobject_class->finalize = thunar_vfs_mime_database_finalize;
}



static void
thunar_vfs_mime_database_init (ThunarVfsMimeDatabase *database)
{
  ThunarVfsMimeProviderData *data;
  const gchar               *s;
  GList                     *stopchars = NULL;
  GList                     *lp;
  gchar                     *directory;
  gchar                    **basedirs;
  gchar                     *p;
  gchar                      c;
  gint                       n;

  /* create the lock for this object */
  database->lock = g_mutex_new ();

  /* acquire a reference on the file alteration monitor */
  database->monitor = thunar_vfs_monitor_get ();

  /* allocate the hash table for the mime infos */
  database->infos = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) exo_object_unref);

  /* lookup all MIME directories */
  basedirs = xfce_resource_dirs (XFCE_RESOURCE_DATA);
  for (n = 0; basedirs[n] != NULL; ++n)
    {
      /* determine the URI for the MIME directory */
      directory = g_build_filename (basedirs[n], "mime", NULL);

      /* allocate the provider data for the directory */
      data = g_new (ThunarVfsMimeProviderData, 1);
      data->handle = NULL;
      data->uri = thunar_vfs_uri_new_for_path (directory);

      /* try to allocate a provider for the directory */
      data->provider = thunar_vfs_mime_cache_new (directory);
      if (G_UNLIKELY (data->provider == NULL))
        data->provider = thunar_vfs_mime_legacy_new (directory);

      /* add the provider to our list */
      database->providers = g_list_append (database->providers, data);

      /* collect the stopchars for this provider */
      if (G_LIKELY (data->provider != NULL))
        stopchars = g_list_concat (stopchars, thunar_vfs_mime_provider_get_stop_characters (data->provider));

      /* collect the max buffer extents for this provider */
      if (G_LIKELY (data->provider != NULL))
        database->max_buffer_extents = MAX (database->max_buffer_extents, thunar_vfs_mime_provider_get_max_buffer_extents (data->provider));

      /* cleanup */
      g_free (directory);
    }
  g_strfreev (basedirs);

  /* clamp the max buffer extents to [1..256] to make
   * sure we don't try insane values.
   */
  database->max_buffer_extents = CLAMP (database->max_buffer_extents, 1, 256);

  /* collect the stop characters */
  database->stopchars = g_new (gchar, g_list_length (stopchars) + 1);
  for (lp = stopchars, p = database->stopchars; lp != NULL; lp = lp->next)
    {
      /* check if we have that character already */
      c = (gchar) GPOINTER_TO_UINT (lp->data);
      for (s = database->stopchars; s < p; ++s)
        if (G_UNLIKELY (*s == c))
          break;

      /* insert the stopchar */
      if (G_LIKELY (s == p))
        *p++ = c;
    }
  g_list_free (stopchars);
  *p = '\0';
}



static void
thunar_vfs_mime_database_finalize (ExoObject *object)
{
  ThunarVfsMimeDatabase *database = THUNAR_VFS_MIME_DATABASE (object);
  GList                 *lp;

  /* free the providers */
  for (lp = database->providers; lp != NULL; lp = lp->next)
    {
      if (G_LIKELY (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider != NULL))
        exo_object_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider);
      exo_object_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->uri);
      g_free (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data));
    }
  g_list_free (database->providers);

  /* free all mime infos */
  g_hash_table_destroy (database->infos);

  /* release the reference on the file alteration monitor */
  g_object_unref (G_OBJECT (database->monitor));

  /* reset the shared instance pointer, as we're the last one to release it */
  g_atomic_pointer_compare_and_exchange ((gpointer) &thunar_vfs_mime_database_shared_instance,
                                         (gpointer) database, (gpointer) NULL);

  /* release the mutex for this object */
  g_mutex_free (database->lock);

  /* free the stopchars */
  g_free (database->stopchars);

  /* call the parent's finalize method */
  (*EXO_OBJECT_CLASS (thunar_vfs_mime_database_parent_class)->finalize) (object);
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_unlocked (ThunarVfsMimeDatabase *database,
                                            const gchar           *mime_type)
{
  ThunarVfsMimeInfo *info;
  const gchar       *s;
  const gchar       *t;
  const gchar       *u;
  gchar             *v;
  guint              n;

again:
  /* check if we have a cached version of the mime type */
  info = g_hash_table_lookup (database->infos, mime_type);
  if (G_UNLIKELY (info == NULL))
    {
      /* count the number of slashes in the mime_type */
      for (n = 0, s = NULL, t = mime_type; *t != '\0'; ++t)
        if (G_UNLIKELY (*t == '/'))
          {
            s = t;
            ++n;
          }

      /* fallback to 'application/octet-stream' if the type is invalid */
      if (G_UNLIKELY (n != 1))
        {
          mime_type = "application/octet-stream";
          goto again;
        }

      /* allocate the MIME info instance */
      info = exo_object_new (THUNAR_VFS_TYPE_MIME_INFO);

      /* allocate memory to store both the full name,
       * as well as the media type alone.
       */
      info->name = g_new (gchar, (t - mime_type) + (s - mime_type) + 2);

      /* copy full name (including the terminator) */
      for (u = mime_type, v = info->name; u <= t; ++u, ++v)
        *v = *u;

      /* set the subtype portion */
      info->subtype = info->name + (s - mime_type) + 1;

      /* copy the media portion */
      info->media = v;
      for (u = mime_type; u < s; ++u, ++v)
        *v = *u;

      /* terminate the media portion */
      *v = '\0';

      /* insert the mime type into the cache */
      g_hash_table_insert (database->infos, info->name, info);
    }

  /* take a reference for the caller */
  return exo_object_ref (info);
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_for_name_unlocked (ThunarVfsMimeDatabase *database,
                                                     const gchar           *name)
{
  ThunarVfsMimeProvider *provider;
  const gchar           *type = NULL;
  const gchar           *ptr;
  GList                 *lp;

  /* try the literals first */
  for (lp = database->providers; lp != NULL && type == NULL; lp = lp->next)
    {
      provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
      if (G_LIKELY (provider != NULL))
        type = thunar_vfs_mime_provider_lookup_literal (provider, name);
    }

  /* next the suffixes */
  if (G_LIKELY (type == NULL))
    {
      ptr = strpbrk (name, database->stopchars);
      while (ptr != NULL)
        {
          /* case-sensitive first */
          for (lp = database->providers; lp != NULL && type == NULL; lp = lp->next)
            {
              provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
              if (G_LIKELY (provider != NULL))
                type = thunar_vfs_mime_provider_lookup_suffix (provider, ptr, FALSE);
            }

          /* case-insensitive next */
          if (G_UNLIKELY (type == NULL))
            {
              for (lp = database->providers; lp != NULL && type == NULL; lp = lp->next)
                {
                  provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
                  if (G_LIKELY (provider != NULL))
                    type = thunar_vfs_mime_provider_lookup_suffix (provider, ptr, TRUE);
                }
            }

          /* check if we got a type */
          if (G_LIKELY (type != NULL))
            break;

          ptr = strpbrk (ptr + 1, database->stopchars);
        }
    }

  /* the glob match comes last */
  if (G_UNLIKELY (type == NULL))
    {
      for (lp = database->providers; lp != NULL && type == NULL; lp = lp->next)
        {
          provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
          if (G_LIKELY (provider != NULL))
            type = thunar_vfs_mime_provider_lookup_glob (provider, name);
        }
    }

  return (type != NULL) ? thunar_vfs_mime_database_get_info_unlocked (database, type) : NULL;
}



/**
 * thunar_vfs_mime_database_get:
 *
 * Returns a reference on the shared #ThunarVfsMimeDatabase
 * instance. The caller is responsible to call #exo_object_unref()
 * on the returned object when no longer needed.
 *
 * Return value: the shared #ThunarVfsMimeDatabase.
 **/
ThunarVfsMimeDatabase*
thunar_vfs_mime_database_get (void)
{
  if (G_UNLIKELY (thunar_vfs_mime_database_shared_instance == NULL))
    thunar_vfs_mime_database_shared_instance = exo_object_new (THUNAR_VFS_TYPE_MIME_DATABASE);
  else
    exo_object_ref (EXO_OBJECT (thunar_vfs_mime_database_shared_instance));

  return thunar_vfs_mime_database_shared_instance;
}



/**
 * thunar_vfs_mime_database_get_info:
 * @database  : a #ThunarVfsMimeDatabase.
 * @mime_type : the string representation of the mime type.
 *
 * Determines the #ThunarVfsMimeInfo which corresponds to @mime_type
 * in database. The caller is responsible to call #exo_object_unref()
 * on the returned instance.
 *
 * Return value: the #ThunarVfsMimeInfo corresponding to @mime_type in @database.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info (ThunarVfsMimeDatabase *database,
                                   const gchar           *mime_type)
{
  ThunarVfsMimeInfo *info;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (mime_type != NULL, NULL);

  g_mutex_lock (database->lock);
  info = thunar_vfs_mime_database_get_info_unlocked (database, mime_type);
  g_mutex_unlock (database->lock);

  return info;
}



/**
 * thunar_vfs_mime_database_get_info_for_data:
 * @database : a #ThunarVfsMimeDatabase.
 * @data     : the data to check.
 * @length   : the length of @data in bytes.
 *
 * Determines the #ThunarVfsMimeInfo for @data in @database. The
 * caller is responsible to call #exo_object_unref() on the
 * returned instance.
 *
 * Return value: the #ThunarVfsMimeInfo determined for @data.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_for_data (ThunarVfsMimeDatabase *database,
                                            gconstpointer          data,
                                            gsize                  length)
{
  ThunarVfsMimeProvider *provider;
  ThunarVfsMimeInfo     *info = NULL;
  const gchar           *type_best;
  const gchar           *type;
  GList                 *lp;
  gint                   prio_best;
  gint                   prio;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);

  g_mutex_lock (database->lock);

  if (G_LIKELY (data != NULL && length > 0))
    {
      /* perform a 'best fit' lookup on all active providers */
      for (type_best = NULL, prio_best = -1, lp = database->providers; lp != NULL; lp = lp->next)
        {
          provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
          if (G_LIKELY (provider != NULL))
            {
              type = thunar_vfs_mime_provider_lookup_data (provider, data, length, &prio);
              if (G_LIKELY (type != NULL && prio > prio_best))
                {
                  prio_best = prio;
                  type_best = type;
                }
            }
        }

      if (G_LIKELY (type_best != NULL))
        {
          /* lookup the info for the best type (if any) */
          info = thunar_vfs_mime_database_get_info_unlocked (database, type_best);
        }
      else if (g_utf8_validate (data, length, NULL))
        {
          /* we have valid UTF-8 text here! */
          info = thunar_vfs_mime_database_get_info_unlocked (database, "text/plain");
        }
    }

  /* fallback to 'application/octet-stream' if we could not determine any type */
  if (G_UNLIKELY (info == NULL))
    info = thunar_vfs_mime_database_get_info_unlocked (database, "application/octet-stream");

  g_mutex_unlock (database->lock);

  return info;
}



/**
 * thunar_vfs_mime_database_get_info_for_name:
 * @database : a #ThunarVfsMimeDatabase.
 * @name     : a filename (must be valid UTF-8!).
 *
 * Determines the #ThunarVfsMimeInfo for the filename given
 * in @name from @database. The caller is responsible to
 * call #exo_object_unref() on the returned instance.
 *
 * The @name must be a valid filename in UTF-8 encoding
 * and it may not contained any slashes!
 *
 * Return value: the #ThunarVfsMimeInfo for @name in @database.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_for_name (ThunarVfsMimeDatabase *database,
                                            const gchar           *name)
{
  ThunarVfsMimeInfo *info;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), NULL);
  g_return_val_if_fail (strchr (name, '/') == NULL, NULL);
  g_return_val_if_fail (*name != '\0', NULL);

  /* try to determine the info from the name */
  g_mutex_lock (database->lock);
  info = thunar_vfs_mime_database_get_info_for_name_unlocked (database, name);
  g_mutex_unlock (database->lock);

  /* fallback to 'application/octet-stream' */
  if (G_UNLIKELY (info == NULL))
    info = thunar_vfs_mime_database_get_info (database, "application/octet-stream");

  /* we got it */
  return info;
}



/**
 * thunar_vfs_mime_database_get_info_for_file:
 * @database : a #ThunarVfsMimeDatabase.
 * @path     : the path to a file in the local filesystem (in the filesystem encoding).
 * @name     : the basename of @path in UTF-8 encoding or %NULL.
 *
 * Determines the #ThunarVfsMimeInfo for @path in @database. The
 * caller is responsible to free the returned instance using
 * #exo_object_unref().
 *
 * The @name parameter is optional. If the caller already knows the
 * basename of @path in UTF-8 encoding, it should be specified here
 * to speed up the lookup process.
 *
 * Return value: the #ThunarVfsMimeInfo for @path in @database.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_for_file (ThunarVfsMimeDatabase *database,
                                            const gchar           *path,
                                            const gchar           *name)
{
  ThunarVfsMimeInfo *info;
  struct stat        stat;
  gssize             nbytes;
  gchar             *basename;
  gchar             *buffer;
  gsize              buflen;
  gint               fd;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (path != NULL && *path != '\0', NULL);
  g_return_val_if_fail (name == NULL || g_utf8_validate (name, -1, NULL), NULL);

  /* determine the basename in UTF-8 if not already given by the caller */
  if (G_UNLIKELY (name == NULL))
    {
      buffer = g_path_get_basename (path);
      name = basename = g_filename_display_name (buffer);
      g_free (buffer);
    }
  else
    {
      basename = NULL;
    }

  /* try to determine the type from the name first */
  g_mutex_lock (database->lock);
  info = thunar_vfs_mime_database_get_info_for_name_unlocked (database, name);
  g_mutex_unlock (database->lock);

  /* try to determine the type from the file content */
  if (G_UNLIKELY (info == NULL))
    {
      fd = g_open (path, O_RDONLY, 0);
      if (G_LIKELY (fd >= 0))
        {
          /* stat the file and verify that we have a regular file, which is not empty */
          if (G_LIKELY (fstat (fd, &stat) == 0 && S_ISREG (stat.st_mode) && stat.st_size > 0))
            {
              /* read the beginning from the file */
              buflen = MIN (stat.st_size, database->max_buffer_extents);
              buffer = g_new (gchar, buflen);
              nbytes = read (fd, buffer, buflen);

              /* try to determine a type from the buffer contents */
              if (G_LIKELY (nbytes > 0))
                info = thunar_vfs_mime_database_get_info_for_data (database, buffer, nbytes);

              /* cleanup */
              g_free (buffer);
            }

          /* cleanup */
          close (fd);
        }

      /* fallback to 'application/octet-stream' */
      if (G_UNLIKELY (info == NULL))
        info = thunar_vfs_mime_database_get_info (database, "application/octet-stream");
    }

  /* cleanup */
  if (G_UNLIKELY (basename != NULL))
    g_free (basename);

  /* we got it */
  return info;
}


