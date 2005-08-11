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
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
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
#include <thunar-vfs/thunar-vfs-alias.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#endif



typedef struct _ThunarVfsMimeDesktopStore ThunarVfsMimeDesktopStore;
typedef struct _ThunarVfsMimeProviderData ThunarVfsMimeProviderData;



#define THUNAR_VFS_MIME_DESKTOP_STORE(store) ((ThunarVfsMimeDesktopStore *) (store))
#define THUNAR_VFS_MIME_PROVIDER_DATA(data)  ((ThunarVfsMimeProviderData *) (data))



static void                      thunar_vfs_mime_database_class_init                   (ThunarVfsMimeDatabaseClass *klass);
static void                      thunar_vfs_mime_database_init                         (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_finalize                     (ExoObject                  *object);
static ThunarVfsMimeApplication *thunar_vfs_mime_database_get_application_locked       (ThunarVfsMimeDatabase      *database,
                                                                                        const gchar                *desktop_id);
static ThunarVfsMimeInfo        *thunar_vfs_mime_database_get_info_locked              (ThunarVfsMimeDatabase      *database,
                                                                                        const gchar                *mime_type);
static ThunarVfsMimeInfo        *thunar_vfs_mime_database_get_info_for_data_locked     (ThunarVfsMimeDatabase      *database,
                                                                                        gconstpointer               data,
                                                                                        gsize                       length);
static ThunarVfsMimeInfo        *thunar_vfs_mime_database_get_info_for_name_locked     (ThunarVfsMimeDatabase      *database,
                                                                                        const gchar                *name);
static GList                    *thunar_vfs_mime_database_get_infos_for_info_locked    (ThunarVfsMimeDatabase      *database,
                                                                                        ThunarVfsMimeInfo          *info);
static void                      thunar_vfs_mime_database_initialize_providers         (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_shutdown_providers           (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_initialize_stores            (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_shutdown_stores              (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_store_changed                (ThunarVfsMonitor           *monitor,
                                                                                        ThunarVfsMonitorHandle     *handle,
                                                                                        ThunarVfsMonitorEvent       event,
                                                                                        ThunarVfsURI               *handle_uri,
                                                                                        ThunarVfsURI               *event_uri,
                                                                                        gpointer                    user_data);
static void                      thunar_vfs_mime_database_store_parse_file             (ThunarVfsMimeDatabase      *database,
                                                                                        ThunarVfsURI               *uri,
                                                                                        GHashTable                 *table);



struct _ThunarVfsMimeDatabaseClass
{
  ExoObjectClass __parent__;
};

struct _ThunarVfsMimeDatabase
{
  ExoObject __parent__;

  GMutex           *lock;

  ThunarVfsMonitor *monitor;

  /* mime detection support */
  GHashTable                *infos;
  GList                     *providers;
  gsize                      max_buffer_extents;
  gchar                     *stopchars;

  /* commonly used mime types */
  ThunarVfsMimeInfo         *application_octet_stream;
  ThunarVfsMimeInfo         *text_plain;

  /* mime applications support */
  ThunarVfsMimeDesktopStore *stores;
  guint                      n_stores;
  GHashTable                *applications;
};

struct _ThunarVfsMimeDesktopStore
{
  ThunarVfsMonitorHandle *defaults_list_handle;
  ThunarVfsURI           *defaults_list_uri;
  GHashTable             *defaults_list;

  ThunarVfsMonitorHandle *mimeinfo_cache_handle;
  ThunarVfsURI           *mimeinfo_cache_uri;
  GHashTable             *mimeinfo_cache;
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
  /* create the lock for this object */
  database->lock = g_mutex_new ();

  /* acquire a reference on the file alteration monitor */
  database->monitor = thunar_vfs_monitor_get_default ();

  /* allocate the hash table for the mime infos */
  database->infos = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, exo_object_unref);

  /* grab references on commonly used mime infos */
  database->application_octet_stream = thunar_vfs_mime_database_get_info_locked (database, "application/octet-stream");
  database->text_plain = thunar_vfs_mime_database_get_info_locked (database, "text/plain");

  /* allocate the applications cache */
  database->applications = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, exo_object_unref);

  /* initialize the MIME providers */
  thunar_vfs_mime_database_initialize_providers (database);

  /* initialize the MIME desktop stores */
  thunar_vfs_mime_database_initialize_stores (database);
}



static void
thunar_vfs_mime_database_finalize (ExoObject *object)
{
  ThunarVfsMimeDatabase *database = THUNAR_VFS_MIME_DATABASE (object);

  /* shutdown the MIME desktop stores */
  thunar_vfs_mime_database_shutdown_stores (database);

  /* release the MIME providers */
  thunar_vfs_mime_database_shutdown_providers (database);

  /* free all cached applications */
  g_hash_table_destroy (database->applications);

  /* free commonly used mime infos */
  exo_object_unref (database->application_octet_stream);
  exo_object_unref (database->text_plain);

  /* free all mime infos */
  g_hash_table_destroy (database->infos);

  /* release the reference on the file alteration monitor */
  g_object_unref (G_OBJECT (database->monitor));

  /* reset the shared instance pointer, as we're the last one to release it */
  g_atomic_pointer_compare_and_exchange ((gpointer) &thunar_vfs_mime_database_shared_instance,
                                         (gpointer) database, (gpointer) NULL);

  /* release the mutex for this object */
  g_mutex_free (database->lock);

  /* call the parent's finalize method */
  (*EXO_OBJECT_CLASS (thunar_vfs_mime_database_parent_class)->finalize) (object);
}



static ThunarVfsMimeApplication*
thunar_vfs_mime_database_get_application_locked (ThunarVfsMimeDatabase *database,
                                                 const gchar           *desktop_id)
{
  ThunarVfsMimeApplication *application;

  /* check if we have that application cached */
  application = g_hash_table_lookup (database->applications, desktop_id);
  if (G_UNLIKELY (application == NULL))
    {
      /* try to load the application from disk */
      application = thunar_vfs_mime_application_new_from_desktop_id (desktop_id);
      if (G_LIKELY (application != NULL))
        {
          /* add the application to the cache (we don't take a copy of the desktop-id here) */
          g_hash_table_insert (database->applications, (gchar *) thunar_vfs_mime_application_get_desktop_id (application), application);
        }
    }

  /* take an additional reference for the caller */
  if (G_LIKELY (application != NULL))
    exo_object_ref (EXO_OBJECT (application));

  return application;
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_locked (ThunarVfsMimeDatabase *database,
                                          const gchar           *mime_type)
{
  ThunarVfsMimeInfo *info;
  const gchar       *s;
  const gchar       *t;
  const gchar       *u;
  gchar             *v;
  guint              n;

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
        return exo_object_ref (database->application_octet_stream);

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
thunar_vfs_mime_database_get_info_for_data_locked (ThunarVfsMimeDatabase *database,
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
          info = thunar_vfs_mime_database_get_info_locked (database, type_best);
        }
      else if (g_utf8_validate (data, length, NULL))
        {
          /* we have valid UTF-8 text here! */
          info = exo_object_ref (database->text_plain);
        }
    }

  return info;
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_for_name_locked (ThunarVfsMimeDatabase *database,
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

  return (type != NULL) ? thunar_vfs_mime_database_get_info_locked (database, type) : NULL;
}



static GList*
thunar_vfs_mime_database_get_infos_for_info_locked (ThunarVfsMimeDatabase *database,
                                                    ThunarVfsMimeInfo     *info)
{
  ThunarVfsMimeProvider *provider;
  ThunarVfsMimeInfo     *parent_info;
  const gchar           *name = thunar_vfs_mime_info_get_name (info);
  const gchar           *type;
  gchar                 *parents[128];
  GList                 *infos;
  GList                 *lp;
  guint                  n, m;

  /* the info itself is of course on the list */
  infos = g_list_prepend (NULL, thunar_vfs_mime_info_ref (info));

  /* check if the info is an alias */
  for (lp = database->providers; lp != NULL; lp = lp->next)
    {
      provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
      if (G_LIKELY (provider != NULL))
        {
          type = thunar_vfs_mime_provider_lookup_alias (provider, name);
          if (G_UNLIKELY (type != NULL && strcmp (name, type) != 0))
            {
              /* it is indeed an alias, so we use the unaliased type instead */
              info = thunar_vfs_mime_database_get_info_locked (database, type);
              name = thunar_vfs_mime_info_get_name (info);
              infos = g_list_append (infos, info);
            }
        }
    }

  /* lookup all parents on every provider */
  for (lp = database->providers; lp != NULL; lp = lp->next)
    {
      provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
      if (G_LIKELY (provider != NULL))
        {
          /* ask this provider for the parents list */
          n = thunar_vfs_mime_provider_lookup_parents (provider, name, parents, G_N_ELEMENTS (parents));
          if (G_UNLIKELY (n == 0))
            continue;

          /* process the parents */
          for (m = 0; m < n; ++m)
            {
              /* append the type if we don't have it already */
              parent_info = thunar_vfs_mime_database_get_info_locked (database, parents[m]);
              if (G_LIKELY (g_list_find (infos, parent_info) == NULL))
                infos = g_list_append (infos, parent_info);
              else
                thunar_vfs_mime_info_unref (parent_info);
            }
        }
    }

  /* all text/xxxx types are subtype of text/plain */
  if (G_UNLIKELY (strcmp (thunar_vfs_mime_info_get_media (info), "text") == 0))
    {
      /* append text/plain if we don't have it already */
      if (g_list_find (infos, database->text_plain) == NULL)
        infos = g_list_append (infos, thunar_vfs_mime_info_ref (database->text_plain));
    }

  /* everything is subtype of application/octet-stream */
  if (g_list_find (infos, database->application_octet_stream) == NULL)
    infos = g_list_append (infos, thunar_vfs_mime_info_ref (database->application_octet_stream));

  return infos;
}



static void
thunar_vfs_mime_database_initialize_providers (ThunarVfsMimeDatabase *database)
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

  /* process all "mime" directories */
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

  /* clamp the max buffer extents to [64..256] to make
   * sure we don't try insane values (everything below
   * 64 bytes would be useless for the UTF-8 check).
   */
  database->max_buffer_extents = CLAMP (database->max_buffer_extents, 64, 256);

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
thunar_vfs_mime_database_shutdown_providers (ThunarVfsMimeDatabase *database)
{
  GList *lp;

  /* free the providers */
  for (lp = database->providers; lp != NULL; lp = lp->next)
    {
      if (G_LIKELY (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider != NULL))
        exo_object_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider);
      exo_object_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->uri);
      g_free (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data));
    }
  g_list_free (database->providers);

  /* free the stopchars */
  g_free (database->stopchars);
}



static void
thunar_vfs_mime_database_initialize_stores (ThunarVfsMimeDatabase *database)
{
  ThunarVfsMimeDesktopStore *store;
  gchar                     *path;
  gchar                    **basedirs;
  guint                      n;

  /* count all base data directories (with possible "applications" subdirs) */
  basedirs = xfce_resource_dirs (XFCE_RESOURCE_DATA);
  for (n = 0; basedirs[n] != NULL; ++n)
    ;

  /* allocate memory for the stores */
  database->n_stores = n;
  database->stores = g_new (ThunarVfsMimeDesktopStore, n);

  /* initialize the stores */
  for (n = 0, store = database->stores; basedirs[n] != NULL; ++n, ++store)
    {
      /* setup the defaults.list */
      path = g_build_filename (basedirs[n], "applications" G_DIR_SEPARATOR_S "defaults.list", NULL);
      store->defaults_list = g_hash_table_new_full (thunar_vfs_mime_info_hash,
                                                    thunar_vfs_mime_info_equal,
                                                    thunar_vfs_mime_info_unref,
                                                    (GDestroyNotify) g_strfreev);
      store->defaults_list_uri = thunar_vfs_uri_new_for_path (path);
      store->defaults_list_handle = thunar_vfs_monitor_add_file (database->monitor, store->defaults_list_uri,
                                                                 thunar_vfs_mime_database_store_changed, database);
      thunar_vfs_mime_database_store_parse_file (database, store->defaults_list_uri, store->defaults_list);
      g_free (path);

      /* setup the mimeinfo.cache */
      path = g_build_filename (basedirs[n], "applications" G_DIR_SEPARATOR_S "mimeinfo.cache", NULL);
      store->mimeinfo_cache = g_hash_table_new_full (thunar_vfs_mime_info_hash,
                                                     thunar_vfs_mime_info_equal,
                                                     thunar_vfs_mime_info_unref,
                                                     (GDestroyNotify) g_strfreev);
      store->mimeinfo_cache_uri = thunar_vfs_uri_new_for_path (path);
      store->mimeinfo_cache_handle = thunar_vfs_monitor_add_file (database->monitor, store->mimeinfo_cache_uri,
                                                                  thunar_vfs_mime_database_store_changed, database);
      thunar_vfs_mime_database_store_parse_file (database, store->mimeinfo_cache_uri, store->mimeinfo_cache);
      g_free (path);
    }
  g_strfreev (basedirs);
}



static void
thunar_vfs_mime_database_shutdown_stores (ThunarVfsMimeDatabase *database)
{
  ThunarVfsMimeDesktopStore *store;
  guint                      n;

  /* release all stores */
  for (n = 0, store = database->stores; n < database->n_stores; ++n, ++store)
    {
      /* release the defaults.list part */
      thunar_vfs_monitor_remove (database->monitor, store->defaults_list_handle);
      thunar_vfs_uri_unref (store->defaults_list_uri);
      g_hash_table_destroy (store->defaults_list);

      /* release the mimeinfo.cache part */
      thunar_vfs_monitor_remove (database->monitor, store->mimeinfo_cache_handle);
      thunar_vfs_uri_unref (store->mimeinfo_cache_uri);
      g_hash_table_destroy (store->mimeinfo_cache);
    }

#ifndef G_DISABLE_CHECKS
  memset (database->stores, 0xaa, database->n_stores * sizeof (*store));
#endif

  /* free the memory allocated to the stores */
  g_free (database->stores);
}



static void
thunar_vfs_mime_database_store_changed (ThunarVfsMonitor       *monitor,
                                        ThunarVfsMonitorHandle *handle,
                                        ThunarVfsMonitorEvent   event,
                                        ThunarVfsURI           *handle_uri,
                                        ThunarVfsURI           *event_uri,
                                        gpointer                user_data)
{
  ThunarVfsMimeDesktopStore *store;
  ThunarVfsMimeDatabase     *database = THUNAR_VFS_MIME_DATABASE (user_data);
  guint                      n;

  /* lookup the store that changed */
  g_mutex_lock (database->lock);
  for (n = 0, store = database->stores; n < database->n_stores; ++n, ++store)
    {
      /* lookup a store that has this handle */
      if (store->defaults_list_handle == handle)
        {
          /* clear any cached values */
          g_hash_table_foreach_remove (store->defaults_list, (GHRFunc) exo_noop_true, NULL);

          /* reload the store's defaults.list */
          thunar_vfs_mime_database_store_parse_file (database, store->defaults_list_uri, store->defaults_list);

          /* and now we're done */
          break;
        }
      else if (store->mimeinfo_cache_handle == handle)
        {
          /* clear the applications cache (as running update-desktop-database usually
           * means that new applications are available)
           */
          g_hash_table_foreach_remove (database->applications, (GHRFunc) exo_noop_true, NULL);

          /* clear any cached values */
          g_hash_table_foreach_remove (store->mimeinfo_cache, (GHRFunc) exo_noop_true, NULL);

          /* reload the store's mimeinfo.cache */
          thunar_vfs_mime_database_store_parse_file (database, store->mimeinfo_cache_uri, store->mimeinfo_cache);

          /* and now we're done */
          break;
        }
    }
  g_mutex_unlock (database->lock);
}



static void
thunar_vfs_mime_database_store_parse_file (ThunarVfsMimeDatabase *database,
                                           ThunarVfsURI          *uri,
                                           GHashTable            *table)
{
  ThunarVfsMimeInfo *info;
  const gchar       *type;
  const gchar       *ids;
  gchar            **id_list;
  gchar              line[4096];
  gchar             *lp;
  FILE              *fp;
  gint               n;
  gint               m;

  /* try to open the file */
  fp = fopen (thunar_vfs_uri_get_path (uri), "r");
  if (G_UNLIKELY (fp == NULL))
    return;

  /* process the file */
  while (fgets (line, sizeof (line), fp) != NULL)
    {
      /* skip everything that doesn't look like a mime type line */
      for (lp = line; g_ascii_isspace (*lp); ++lp) ;
      if (G_UNLIKELY (*lp < 'a' || *lp > 'z'))
        continue;

      /* lookup the '=' */
      for (type = lp++; *lp != '\0' && *lp != '='; ++lp) ;
      if (G_UNLIKELY (*lp != '='))
        continue;
      *lp++ = '\0';

      /* extract the application id list */
      for (ids = lp; g_ascii_isgraph (*lp); ++lp) ;
      if (G_UNLIKELY (ids == lp))
        continue;
      *lp = '\0';

      /* parse the id list */
      id_list = g_strsplit (ids, ";", -1);
      for (m = n = 0; id_list[m] != NULL; ++m)
        {
          if (G_UNLIKELY (id_list[m][0] == '\0'))
            g_free (id_list[m]);
          else
            id_list[n++] = id_list[m];
        }
      id_list[n] = NULL;

      /* verify that the id list is not empty */
      if (G_UNLIKELY (id_list[0] == NULL))
        {
          g_free (id_list);
          continue;
        }

      /* lookup the mime info for the type */
      info = thunar_vfs_mime_database_get_info_locked (database, type);
      if (G_UNLIKELY (info == NULL))
        {
          g_strfreev (id_list);
          continue;
        }

      /* insert the association for the desktop store */
      g_hash_table_insert (table, info, id_list);
    }

  /* close the file */
  fclose (fp);
}



/**
 * thunar_vfs_mime_database_get_default:
 *
 * Returns a reference on the shared #ThunarVfsMimeDatabase
 * instance. The caller is responsible to call #exo_object_unref()
 * on the returned object when no longer needed.
 *
 * Return value: the shared #ThunarVfsMimeDatabase.
 **/
ThunarVfsMimeDatabase*
thunar_vfs_mime_database_get_default (void)
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
  info = thunar_vfs_mime_database_get_info_locked (database, mime_type);
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
  ThunarVfsMimeInfo *info;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);

  /* try to determine a MIME type based on the data */
  g_mutex_lock (database->lock);
  info = thunar_vfs_mime_database_get_info_for_data_locked (database, data, length);
  g_mutex_unlock (database->lock);

  /* fallback to 'application/octet-stream' if we could not determine any type */
  if (G_UNLIKELY (info == NULL))
    info = exo_object_ref (database->application_octet_stream);

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
  info = thunar_vfs_mime_database_get_info_for_name_locked (database, name);
  g_mutex_unlock (database->lock);

  /* fallback to 'application/octet-stream' */
  if (G_UNLIKELY (info == NULL))
    info = exo_object_ref (database->application_octet_stream);

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
  const gchar       *p;
  gssize             nbytes;
  gchar             *basename;
  gchar             *buffer;
  gchar             *type;
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
  info = thunar_vfs_mime_database_get_info_for_name_locked (database, name);
  g_mutex_unlock (database->lock);

  /* try to determine the type from the file content */
  if (G_UNLIKELY (info == NULL))
    {
      fd = g_open (path, O_RDONLY, 0);
      if (G_LIKELY (fd >= 0))
        {
#ifdef HAVE_EXTATTR_GET_FD
          /* check if we have a valid mime type stored in the extattr */
          nbytes = extattr_get_fd (fd, EXTATTR_NAMESPACE_USER, "mime_type", NULL, 0);
          if (G_UNLIKELY (nbytes >= 3))
            {
              buffer = g_new (gchar, nbytes + 1);
              nbytes = extattr_get_fd (fd, EXTATTR_NAMESPACE_USER, "mime_type", buffer, nbytes);
              if (G_LIKELY (nbytes >= 3))
                {
                  buffer[nbytes] = '\0';
                  info = thunar_vfs_mime_database_get_info (database, buffer);
                }
              g_free (buffer);
            }
#endif

          /* stat the file and verify that we have a regular file, which is not empty */
          if (G_LIKELY (info == NULL && fstat (fd, &stat) == 0 && S_ISREG (stat.st_mode) && stat.st_size > 0))
            {
              /* read the beginning from the file */
              buflen = MIN (stat.st_size, database->max_buffer_extents);
              buffer = g_new (gchar, buflen);
              nbytes = read (fd, buffer, buflen);

              /* try to determine a type from the buffer contents */
              if (G_LIKELY (nbytes > 0))
                {
                  g_mutex_lock (database->lock);

                  /* the regular magic check first */
                  info = thunar_vfs_mime_database_get_info_for_data_locked (database, buffer, nbytes);

                  /* then if magic doesn't tell us anything and the file is marked,
                   * as executable, we just guess "application/x-executable", which
                   * is atleast more precise than "application/octet-stream".
                   */
                  if (G_UNLIKELY (info == NULL && (stat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0))
                    info = thunar_vfs_mime_database_get_info_locked (database, "application/x-executable");

                  g_mutex_unlock (database->lock);
                }

              /* cleanup */
              g_free (buffer);
            }

          /* cleanup */
          close (fd);
        }

      /* if we have exactly a dot and a non-empty extension, we
       * can generate a 'application/x-extension-<EXT>' on the fly.
       */
      if (G_UNLIKELY (info == NULL))
        {
          p = strrchr (name, '.');
          if (G_UNLIKELY (p != NULL && *++p != '\0'))
            {
              buffer = g_utf8_strdown (p, -1);
              type = g_strconcat ("application/x-extension-", buffer, NULL);
              info = thunar_vfs_mime_database_get_info (database, type);
              g_free (buffer);
              g_free (type);
            }
        }

      /* fallback to 'application/octet-stream' */
      if (G_UNLIKELY (info == NULL))
        info = exo_object_ref (database->application_octet_stream);
    }

  /* cleanup */
  if (G_UNLIKELY (basename != NULL))
    g_free (basename);

  /* we got it */
  return info;
}



/**
 * thunar_vfs_mime_database_get_applications:
 * @database : a #ThunarVfsMimeDatabase.
 * @info     : a #ThunarVfsMimeInfo.
 *
 * Looks up all #ThunarVfsMimeApplication<!---->s in @database, which
 * claim to be able to open files whose MIME-type is represented by
 * @info.
 *
 * The caller is responsible to free the returned list using
 * #thunar_vfs_mime_application_list_free().
 *
 * Return value: the list of #ThunarVfsMimeApplication<!---->s, that
 *               can handle @info.
 **/
GList*
thunar_vfs_mime_database_get_applications (ThunarVfsMimeDatabase *database,
                                           ThunarVfsMimeInfo     *info)
{
  ThunarVfsMimeDesktopStore *store;
  ThunarVfsMimeApplication  *application;
  const gchar              **id;
  GList                     *applications = NULL;
  GList                     *infos;
  GList                     *lp;
  guint                      n;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);

  g_mutex_lock (database->lock);

  infos = thunar_vfs_mime_database_get_infos_for_info_locked (database, info);
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      for (n = database->n_stores, store = database->stores; n-- > 0; ++store)
        {
          /* lookup the application ids */
          id = g_hash_table_lookup (store->mimeinfo_cache, lp->data);
          if (G_UNLIKELY (id == NULL))
            continue;

          /* merge the applications */
          for (; *id != NULL; ++id)
            {
              /* lookup the application for the id */
              application = thunar_vfs_mime_database_get_application_locked (database, *id);
              if (G_UNLIKELY (application == NULL))
                continue;

              /* merge the application */
              if (g_list_find (applications, application) == NULL)
                applications = g_list_append (applications, application);
              else
                exo_object_unref (EXO_OBJECT (application));
            }
        }
    }

  g_mutex_unlock (database->lock);

  /* free the temporary list of mime infos */
  thunar_vfs_mime_info_list_free (infos);

  return applications;
}



/**
 * thunar_vfs_mime_database_get_default_application:
 * @database : a #ThunarVfsMimeDatabase.
 * @info     : a #ThunarVfsMimeInfo.
 *
 * Returns the default #ThunarVfsMimeApplication to handle
 * files of type @info or %NULL if no default application
 * is set for @info.
 *
 * The caller is responsible to free the returned instance
 * using #exo_object_unref().
 *
 * Return value: the default #ThunarVfsMimeApplication for
 *               @info or %NULL.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_database_get_default_application (ThunarVfsMimeDatabase *database,
                                                  ThunarVfsMimeInfo     *info)
{
  ThunarVfsMimeDesktopStore *store;
  ThunarVfsMimeApplication  *application = NULL;
  const gchar              **id;
  GList                     *applications;
  GList                     *infos;
  GList                     *lp;
  guint                      n;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (info), NULL);

  g_mutex_lock (database->lock);

  infos = thunar_vfs_mime_database_get_infos_for_info_locked (database, info);
  for (lp = infos; application == NULL && lp != NULL; lp = lp->next)
    {
      for (n = database->n_stores, store = database->stores; application == NULL && n-- > 0; ++store)
        {
          id = g_hash_table_lookup (store->defaults_list, lp->data);
          if (G_LIKELY (id == NULL))
            continue;

          /* test all applications (use first match) */
          for (; application == NULL && *id != NULL; ++id)
            application = thunar_vfs_mime_database_get_application_locked (database, *id);
        }
    }

  g_mutex_unlock (database->lock);

  /* free the temporary list of mime infos */
  thunar_vfs_mime_info_list_free (infos);

  /* if we don't have a default application set, we simply
   * use the first available application here.
   */
  if (G_LIKELY (application == NULL))
    {
      /* query all apps for this mime type */
      applications = thunar_vfs_mime_database_get_applications (database, info);
      if (G_LIKELY (applications != NULL))
        {
          /* use the first available application */
          application = THUNAR_VFS_MIME_APPLICATION (applications->data);
          g_list_foreach (applications->next, (GFunc) exo_object_unref, NULL);
          g_list_free (applications);
        }
    }

  return application;
}



#define __THUNAR_VFS_MIME_DATABASE_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
