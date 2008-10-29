/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
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
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-cache.h>
#include <thunar-vfs/thunar-vfs-mime-database-private.h>
#include <thunar-vfs/thunar-vfs-mime-legacy.h>
#include <thunar-vfs/thunar-vfs-mime-sniffer.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_open(), g_rename() and g_unlink() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_unlink(path) (unlink ((path)))
#endif



/* the interval in which thunar-vfs-mime-cleaner is invoked (in ms) */
#define THUNAR_VFS_MIME_DATABASE_CLEANUP_INTERVAL (5 * 60 * 1000)



typedef struct _ThunarVfsMimeDesktopStore ThunarVfsMimeDesktopStore;
typedef struct _ThunarVfsMimeProviderData ThunarVfsMimeProviderData;



#define THUNAR_VFS_MIME_DESKTOP_STORE(store) ((ThunarVfsMimeDesktopStore *) (store))
#define THUNAR_VFS_MIME_PROVIDER_DATA(data)  ((ThunarVfsMimeProviderData *) (data))



static void                      thunar_vfs_mime_database_class_init                   (ThunarVfsMimeDatabaseClass *klass);
static void                      thunar_vfs_mime_database_init                         (ThunarVfsMimeDatabase      *database);
static void                      thunar_vfs_mime_database_finalize                     (GObject                    *object);
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
                                                                                        ThunarVfsPath              *handle_path,
                                                                                        ThunarVfsPath              *event_path,
                                                                                        gpointer                    user_data);
static void                      thunar_vfs_mime_database_store_parse_file             (ThunarVfsMimeDatabase      *database,
                                                                                        ThunarVfsPath              *path,
                                                                                        GHashTable                 *table);
static gboolean                  thunar_vfs_mime_database_icon_theme_changed           (GSignalInvocationHint      *ihint,
                                                                                        guint                       n_param_values,
                                                                                        const GValue               *param_values,
                                                                                        gpointer                    user_data);
static gboolean                  thunar_vfs_mime_database_cleanup_timer                (gpointer                    user_data);
static void                      thunar_vfs_mime_database_cleanup_timer_destroy        (gpointer                    user_data);



struct _ThunarVfsMimeDatabaseClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsMimeDatabase
{
  GObject __parent__;

  GMutex                   *lock;

  /* GtkIconTheme changed hook id */
  gulong                    changed_hook_id;

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

  /* the thunar-vfs-mime-cleaner timer id */
  gint                       cleanup_timer_id;
};

struct _ThunarVfsMimeDesktopStore
{
  ThunarVfsMonitorHandle *defaults_list_handle;
  GHashTable             *defaults_list;
  ThunarVfsMonitorHandle *mimeinfo_cache_handle;
  GHashTable             *mimeinfo_cache;
};

struct _ThunarVfsMimeProviderData
{
  ThunarVfsPath         *path;
  ThunarVfsMimeProvider *provider;
};



static GObjectClass *thunar_vfs_mime_database_parent_class;



GType
thunar_vfs_mime_database_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsMimeDatabase",
                                                 sizeof (ThunarVfsMimeDatabaseClass),
                                                 thunar_vfs_mime_database_class_init,
                                                 sizeof (ThunarVfsMimeDatabase),
                                                 thunar_vfs_mime_database_init,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_mime_database_class_init (ThunarVfsMimeDatabaseClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_mime_database_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_mime_database_finalize;
}



static void
thunar_vfs_mime_database_init (ThunarVfsMimeDatabase *database)
{
  gpointer klass;

  /* create the lock for this object */
  database->lock = g_mutex_new ();

  /* allocate the hash table for the mime infos */
  database->infos = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) thunar_vfs_mime_info_unref);

  /* grab references on commonly used mime infos */
  database->application_octet_stream = thunar_vfs_mime_database_get_info_locked (database, "application/octet-stream");
  database->text_plain = thunar_vfs_mime_database_get_info_locked (database, "text/plain");

  /* allocate the applications cache */
  database->applications = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  /* initialize the MIME providers */
  thunar_vfs_mime_database_initialize_providers (database);

  /* connect emission hook for the "changed" signal on the GtkIconTheme class. We use the emission
   * hook to reset the icon names of all mime infos at once.
   */
  klass = g_type_class_ref (GTK_TYPE_ICON_THEME);
  database->changed_hook_id = g_signal_add_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME),
                                                          0, thunar_vfs_mime_database_icon_theme_changed,
                                                          database, NULL);
  g_type_class_unref (klass);
}



static void
thunar_vfs_mime_database_finalize (GObject *object)
{
  ThunarVfsMimeDatabase *database = THUNAR_VFS_MIME_DATABASE (object);

  /* cancel any pending cleanup timer */
  if (G_LIKELY (database->cleanup_timer_id > 0))
    g_source_remove (database->cleanup_timer_id);

  /* remove the "changed" emission hook from the GtkIconTheme class */
  g_signal_remove_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME), database->changed_hook_id);

  /* shutdown the MIME desktop stores (if initialized) */
  if (G_LIKELY (database->stores != NULL))
    thunar_vfs_mime_database_shutdown_stores (database);

  /* release the MIME providers */
  thunar_vfs_mime_database_shutdown_providers (database);

  /* free all cached applications */
  g_hash_table_destroy (database->applications);

  /* free commonly used mime infos */
  thunar_vfs_mime_info_unref (database->application_octet_stream);
  thunar_vfs_mime_info_unref (database->text_plain);

  /* free all mime infos */
  g_hash_table_destroy (database->infos);

  /* release the mutex for this object */
  g_mutex_free (database->lock);

  /* call the parent's finalize method */
  (*G_OBJECT_CLASS (thunar_vfs_mime_database_parent_class)->finalize) (object);
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
    g_object_ref (G_OBJECT (application));

  return application;
}



static ThunarVfsMimeInfo*
thunar_vfs_mime_database_get_info_locked (ThunarVfsMimeDatabase *database,
                                          const gchar           *mime_type)
{
  ThunarVfsMimeProvider *provider;
  ThunarVfsMimeInfo     *info;
  const gchar           *p;
  GList                 *lp;
  guint                  n;

  /* unalias the mime type */
  for (lp = database->providers; lp != NULL; lp = lp->next)
    {
      provider = THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider;
      if (G_LIKELY (provider != NULL))
        {
          p = thunar_vfs_mime_provider_lookup_alias (provider, mime_type);
          if (G_UNLIKELY (p != NULL && strcmp (mime_type, p) != 0))
            {
              mime_type = p;
              break;
            }
        }
    }

  /* check if we have a cached version of the mime type */
  info = g_hash_table_lookup (database->infos, mime_type);
  if (G_UNLIKELY (info == NULL))
    {
      /* count the number of slashes in the mime_type */
      for (n = 0, p = mime_type; *p != '\0'; ++p)
        if (G_UNLIKELY (*p == '/'))
          ++n;

      /* fallback to 'application/octet-stream' if the type is invalid */
      if (G_UNLIKELY (n != 1))
        return thunar_vfs_mime_info_ref (database->application_octet_stream);

      /* allocate the MIME info instance */
      info = thunar_vfs_mime_info_new (mime_type, (p - mime_type));

      /* insert the mime type into the cache (w/o taking a copy on the name) */
      g_hash_table_insert (database->infos, (gpointer) thunar_vfs_mime_info_get_name (info), info);
    }

  /* take a reference for the caller */
  return thunar_vfs_mime_info_ref (info);
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

  if (G_UNLIKELY (length == 0))
    {
      /* empty files are considered to be textfiles, so
       * newly created (empty) files can be opened in
       * a text editor.
       */
      info = thunar_vfs_mime_info_ref (database->text_plain);
    }
  else
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
      else if (thunar_vfs_mime_sniffer_looks_like_text (data, length))
        {
          /* we have valid UTF-8 text here! */
          info = thunar_vfs_mime_info_ref (database->text_plain);
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
  gchar                 *parents[128];
  GList                 *infos;
  GList                 *lp;
  guint                  n, m;

  /* the info itself is of course on the list */
  infos = g_list_prepend (NULL, thunar_vfs_mime_info_ref (info));

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
  if (G_UNLIKELY (strncmp ("text/", thunar_vfs_mime_info_get_name (info), 5) == 0))
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
      /* determine the path for the MIME directory */
      directory = g_build_filename (basedirs[n], "mime", NULL);

      /* allocate the provider data for the directory */
      data = _thunar_vfs_slice_new (ThunarVfsMimeProviderData);
      data->path = thunar_vfs_path_new (directory, NULL);

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
        g_object_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->provider);
      thunar_vfs_path_unref (THUNAR_VFS_MIME_PROVIDER_DATA (lp->data)->path);
      _thunar_vfs_slice_free (ThunarVfsMimeProviderData, lp->data);
    }
  g_list_free (database->providers);

  /* free the stopchars */
  g_free (database->stopchars);
}



static void
thunar_vfs_mime_database_initialize_stores (ThunarVfsMimeDatabase *database)
{
  ThunarVfsMimeDesktopStore *store;
  ThunarVfsPath             *base_path;
  ThunarVfsPath             *path;
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
      /* determine the base path */
      path = thunar_vfs_path_new (basedirs[n], NULL);
      base_path = _thunar_vfs_path_child (path, "applications");
      _thunar_vfs_path_unref_nofree (path);

      /* setup the defaults.list */
      path = _thunar_vfs_path_child (base_path, "defaults.list");
      store->defaults_list = g_hash_table_new_full (thunar_vfs_mime_info_hash,
                                                    thunar_vfs_mime_info_equal,
                                                    (GDestroyNotify) thunar_vfs_mime_info_unref,
                                                    (GDestroyNotify) g_strfreev);
      store->defaults_list_handle = thunar_vfs_monitor_add_file (_thunar_vfs_monitor, path, thunar_vfs_mime_database_store_changed, database);
      thunar_vfs_mime_database_store_parse_file (database, path, store->defaults_list);
      _thunar_vfs_path_unref_nofree (path);

      /* setup the mimeinfo.cache */
      path = _thunar_vfs_path_child (base_path, "mimeinfo.cache");
      store->mimeinfo_cache = g_hash_table_new_full (thunar_vfs_mime_info_hash,
                                                     thunar_vfs_mime_info_equal,
                                                     (GDestroyNotify) thunar_vfs_mime_info_unref,
                                                     (GDestroyNotify) g_strfreev);
      store->mimeinfo_cache_handle = thunar_vfs_monitor_add_file (_thunar_vfs_monitor, path, thunar_vfs_mime_database_store_changed, database);
      thunar_vfs_mime_database_store_parse_file (database, path, store->mimeinfo_cache);
      _thunar_vfs_path_unref_nofree (path);

      /* release the base path */
      _thunar_vfs_path_unref_nofree (base_path);
    }
  g_strfreev (basedirs);

  /* launch the clean up timer if it's not already running */
  if (G_LIKELY (database->cleanup_timer_id == 0))
    {
      /* start the timer, which invokes thunar-vfs-mime-cleaner from time to time to cleanup the
       * user's mime database directories, so we don't keep old junk around for too long.
       */
      database->cleanup_timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_VFS_MIME_DATABASE_CLEANUP_INTERVAL,
                                                       thunar_vfs_mime_database_cleanup_timer, database,
                                                       thunar_vfs_mime_database_cleanup_timer_destroy);
    }
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
      thunar_vfs_monitor_remove (_thunar_vfs_monitor, store->defaults_list_handle);
      g_hash_table_destroy (store->defaults_list);

      /* release the mimeinfo.cache part */
      thunar_vfs_monitor_remove (_thunar_vfs_monitor, store->mimeinfo_cache_handle);
      g_hash_table_destroy (store->mimeinfo_cache);
    }

#ifdef G_ENABLE_DEBUG
  memset (database->stores, 0xaa, database->n_stores * sizeof (*store));
#endif

  /* free the memory allocated to the stores */
  g_free (database->stores);
}



static void
thunar_vfs_mime_database_store_changed (ThunarVfsMonitor       *monitor,
                                        ThunarVfsMonitorHandle *handle,
                                        ThunarVfsMonitorEvent   event,
                                        ThunarVfsPath          *handle_path,
                                        ThunarVfsPath          *event_path,
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
          thunar_vfs_mime_database_store_parse_file (database, _thunar_vfs_monitor_handle_get_path (store->defaults_list_handle), store->defaults_list);

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
          thunar_vfs_mime_database_store_parse_file (database, _thunar_vfs_monitor_handle_get_path (store->mimeinfo_cache_handle), store->mimeinfo_cache);

          /* and now we're done */
          break;
        }
    }
  g_mutex_unlock (database->lock);
}



static void
thunar_vfs_mime_database_store_parse_file (ThunarVfsMimeDatabase *database,
                                           ThunarVfsPath         *path,
                                           GHashTable            *table)
{
  ThunarVfsMimeInfo *info;
  const gchar       *type;
  const gchar       *ids;
  gchar              absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar            **id_list;
  gchar              line[4096];
  gchar             *lp;
  FILE              *fp;
  gint               n;
  gint               m;

  /* determine the absolute path */
  if (thunar_vfs_path_to_string (path, absolute_path, sizeof (absolute_path), NULL) < 0)
    return;

  /* try to open the file */
  fp = fopen (absolute_path, "r");
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



static void
invalidate_icon_name (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
  _thunar_vfs_mime_info_invalidate_icon_name (value);
}



static gboolean
thunar_vfs_mime_database_icon_theme_changed (GSignalInvocationHint *ihint,
                                             guint                  n_param_values,
                                             const GValue          *param_values,
                                             gpointer               user_data)
{
  ThunarVfsMimeDatabase *database = THUNAR_VFS_MIME_DATABASE (user_data);

  /* invalidate all cached icon names */
  g_mutex_lock (database->lock);
  g_hash_table_foreach (database->infos, invalidate_icon_name, NULL);
  g_mutex_unlock (database->lock);

  /* keep the emission hook alive */
  return TRUE;
}



static gboolean
thunar_vfs_mime_database_cleanup_timer (gpointer user_data)
{
  /* invoke thunar-vfs-mime-cleaner */
  g_spawn_command_line_async (LIBEXECDIR "/thunar-vfs-mime-cleaner-" THUNAR_VFS_VERSION_API, NULL);

  /* keep the timer alive */
  return TRUE;
}



static void
thunar_vfs_mime_database_cleanup_timer_destroy (gpointer user_data)
{
  THUNAR_VFS_MIME_DATABASE (user_data)->cleanup_timer_id = 0;
}



/**
 * thunar_vfs_mime_database_get_default:
 *
 * Returns a reference on the shared #ThunarVfsMimeDatabase
 * instance. The caller is responsible to call g_object_unref()
 * on the returned object when no longer needed.
 *
 * Return value: the shared #ThunarVfsMimeDatabase.
 **/
ThunarVfsMimeDatabase*
thunar_vfs_mime_database_get_default (void)
{
  return g_object_ref (G_OBJECT (_thunar_vfs_mime_database));
}



/**
 * thunar_vfs_mime_database_get_info:
 * @database  : a #ThunarVfsMimeDatabase.
 * @mime_type : the string representation of the mime type.
 *
 * Determines the #ThunarVfsMimeInfo which corresponds to @mime_type
 * in database. The caller is responsible to call thunar_vfs_mime_info_unref()
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
 * caller is responsible to call thunar_vfs_mime_info_unref() on
 * the returned instance.
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
    info = thunar_vfs_mime_info_ref (database->application_octet_stream);

  return info;
}



/**
 * thunar_vfs_mime_database_get_info_for_name:
 * @database : a #ThunarVfsMimeDatabase.
 * @name     : a filename (must be valid UTF-8!).
 *
 * Determines the #ThunarVfsMimeInfo for the filename given
 * in @name from @database. The caller is responsible to
 * call thunar_vfs_mime_info_unref() on the returned instance.
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
    info = thunar_vfs_mime_info_ref (database->application_octet_stream);

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
 * thunar_vfs_mime_info_unref().
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

#ifdef HAVE_ATTROPEN
  /* check if we have a valid mime type stored in the extattr (SunOS) */
  if (G_UNLIKELY (info == NULL))
    {
      fd = attropen (path, "user.mime_type", O_RDONLY);
      if (G_UNLIKELY (fd >= 0))
        {
          if (fstat (fd, &stat) == 0)
            {
              buffer = g_newa (gchar, stat.st_size + 1);
              nbytes = read (fd, buffer, stat.st_size);
              if (G_LIKELY (nbytes >= 3))
                {
                  buffer[nbytes] = '\0';
                  info = thunar_vfs_mime_database_get_info (database, buffer);
                }
            }
          close (fd);
        }
    }
#endif

  /* try to determine the type from the file content */
  if (G_UNLIKELY (info == NULL))
    {
      fd = g_open (path, O_RDONLY, 0);
      if (G_LIKELY (fd >= 0))
        {
#ifdef HAVE_EXTATTR_GET_FD
          /* check if we have a valid mime type stored in the extattr (TrustedBSD) */
          nbytes = extattr_get_fd (fd, EXTATTR_NAMESPACE_USER, "mime_type", NULL, 0);
          if (G_UNLIKELY (nbytes >= 3))
            {
              buffer = g_newa (gchar, nbytes + 1);
              nbytes = extattr_get_fd (fd, EXTATTR_NAMESPACE_USER, "mime_type", buffer, nbytes);
              if (G_LIKELY (nbytes >= 3))
                {
                  buffer[nbytes] = '\0';
                  info = thunar_vfs_mime_database_get_info (database, buffer);
                }
            }
#elif defined(HAVE_FGETXATTR)
          /* check for valid mime type stored in the extattr (Linux) */
#if defined(__APPLE__) && defined(__MACH__)
          nbytes = fgetxattr (fd, "user.mime_type", NULL, 0, 0, 0);
#else
          nbytes = fgetxattr (fd, "user.mime_type", NULL, 0);
#endif
          if (G_UNLIKELY (nbytes >= 3))
            {
              buffer = g_newa (gchar, nbytes + 1);
#if defined(__APPLE__) && defined(__MACH__)
              nbytes = fgetxattr (fd, "user.mime_type", buffer, nbytes, 0, 0);
#else
              nbytes = fgetxattr (fd, "user.mime_type", buffer, nbytes);
#endif

              if (G_LIKELY (nbytes >= 3))
                {
                  buffer[nbytes] = '\0';
                  info = thunar_vfs_mime_database_get_info (database, buffer);
                }
            }
#endif

          /* stat the file and verify that we have a regular file */
          if (G_LIKELY (info == NULL && fstat (fd, &stat) == 0 && S_ISREG (stat.st_mode)))
            {
              /* read the beginning from the file */
              buflen = MIN (stat.st_size, database->max_buffer_extents);
              buffer = g_newa (gchar, buflen);
              nbytes = read (fd, buffer, buflen);

              /* try to determine a type from the buffer contents */
              if (G_LIKELY (nbytes >= 0))
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
            }

          /* cleanup */
          close (fd);
        }

      /* if we have exactly a dot and a non-empty extension, we
       * can generate a 'application/x-extension-<EXT>' on the fly
       * or if no extension is found, we'll use the whole filename
       * for '<EXT>'.
       */
      if (G_UNLIKELY (info == NULL))
        {
          /* check if the filename has an extension */
          p = strrchr (name, '.');
          if (G_UNLIKELY (p != NULL && *++p != '\0'))
            {
              /* use the file extension for the type */
              buffer = g_utf8_strdown (p, -1);
            }
          else
            {
              /* use the whole filename for the type as suggested by jrb */
              buffer = g_utf8_strdown (name, -1);
            }

          /* generate a new mime type */
          type = g_strconcat ("application/x-extension-", buffer, NULL);
          info = thunar_vfs_mime_database_get_info (database, type);
          g_free (buffer);
          g_free (type);
        }
    }

  /* cleanup */
  if (G_UNLIKELY (basename != NULL))
    g_free (basename);

  /* we got it */
  return info;
}



/**
 * thunar_vfs_mime_database_get_infos_for_info:
 * @database : a #ThunarVfsMimeDatabase.
 * @info     : a #ThunarVfsMimeInfo.
 *
 * Returns a list of all #ThunarVfsMimeInfo<!---->s,
 * that are related to @info in @database. Currently
 * this is the list of parent MIME-types for @info,
 * as defined in the Shared Mime Database.
 *
 * Note that the returned list will also include
 * a reference @info itself. In addition, this
 * method also handles details specified by the
 * Shared Mime Database Specification like the
 * fact that every "text/xxxx" MIME-type is a
 * subclass of "text/plain" and every MIME-type
 * is a subclass of "application/octet-stream".
 *
 * The caller is responsible to free the returned
 * list using #thunar_vfs_mime_info_list_free()
 * when done with it.
 *
 * Return value: the list of #ThunarVfsMimeInfo<!---->s
 *               related to @info.
 **/
GList*
thunar_vfs_mime_database_get_infos_for_info (ThunarVfsMimeDatabase *database,
                                             ThunarVfsMimeInfo     *info)
{
  GList *infos;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);

  g_mutex_lock (database->lock);
  infos = thunar_vfs_mime_database_get_infos_for_info_locked (database, info);
  g_mutex_unlock (database->lock);

  return infos;
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
 * something like:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
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
  const gchar               *command;
  const gchar              **id;
  GList                     *applications = NULL;
  GList                     *infos;
  GList                     *lp;
  GList                     *p;
  guint                      n;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);

  g_mutex_lock (database->lock);

  /* determine the mime infos for info */
  infos = thunar_vfs_mime_database_get_infos_for_info_locked (database, info);

  /* be sure to initialize the MIME desktop stores first */
  if (G_UNLIKELY (database->stores == NULL))
    thunar_vfs_mime_database_initialize_stores (database);

  /* lookup the default applications for the infos */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      for (n = database->n_stores, store = database->stores; n-- > 0; ++store)
        {
          /* lookup the application ids */
          id = g_hash_table_lookup (store->defaults_list, lp->data);
          if (G_LIKELY (id == NULL))
            continue;

          /* merge the applications */
          for (; *id != NULL; ++id)
            {
              /* lookup the application for the id */
              application = thunar_vfs_mime_database_get_application_locked (database, *id);
              if (G_UNLIKELY (application == NULL))
                continue;

              /* check if we already have an application with an equal command
               * (thanks again Nautilus for the .desktop file mess).
               */
              command = thunar_vfs_mime_application_get_command (application);
              for (p = applications; p != NULL; p = p->next)
                if (g_str_equal (thunar_vfs_mime_application_get_command (p->data), command))
                  break;

              /* merge the application */
              if (G_LIKELY (p == NULL))
                applications = g_list_append (applications, application);
              else
                g_object_unref (G_OBJECT (application));
            }
        }
    }

  /* lookup the other applications */
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

              /* check if we already have an application with an equal command
               * (thanks again Nautilus for the .desktop file mess).
               */
              command = thunar_vfs_mime_application_get_command (application);
              for (p = applications; p != NULL; p = p->next)
                if (g_str_equal (thunar_vfs_mime_application_get_command (p->data), command))
                  break;

              /* merge the application */
              if (G_LIKELY (p == NULL))
                applications = g_list_append (applications, application);
              else
                g_object_unref (G_OBJECT (application));
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
 * using g_object_unref() when no longer needed.
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

  g_mutex_lock (database->lock);

  /* be sure to initialize the MIME desktop stores first */
  if (G_UNLIKELY (database->stores == NULL))
    thunar_vfs_mime_database_initialize_stores (database);

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
          application = applications->data;
          g_list_foreach (applications->next, (GFunc) g_object_unref, NULL);
          g_list_free (applications);
        }
    }

  return application;
}



static void
defaults_list_write (ThunarVfsMimeInfo *info,
                     gchar            **ids,
                     FILE              *fp)
{
  guint n;

  fprintf (fp, "%s=%s", thunar_vfs_mime_info_get_name (info), ids[0]);
  for (n = 1; ids[n] != NULL; ++n)
    fprintf (fp, ";%s", ids[n]);
  fprintf (fp, "\n");
}



/**
 * thunar_vfs_mime_database_set_default_application:
 * @database    : a #ThunarVfsMimeDatabase.
 * @info        : a valid #ThunarVfsMimeInfo for @database.
 * @application : a #ThunarVfsMimeApplication.
 * @error       : return location for errors or %NULL.
 *
 * Sets @application to be the default #ThunarVfsMimeApplication to open files
 * of type @info in @database.
 *
 * Return value: %TRUE if the operation was successfull, else %FALSE.
 **/
gboolean
thunar_vfs_mime_database_set_default_application (ThunarVfsMimeDatabase    *database,
                                                  ThunarVfsMimeInfo        *info,
                                                  ThunarVfsMimeApplication *application,
                                                  GError                  **error)
{
  ThunarVfsMimeDesktopStore *store;
  ThunarVfsPath             *parent;
  gboolean                   succeed = FALSE;
  gchar                      absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar                    **pids;
  gchar                    **nids;
  gchar                     *path;
  guint                      n, m;
  FILE                      *fp;
  gint                       fd;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* acquire the lock on the database */
  g_mutex_lock (database->lock);

  /* be sure to initialize the MIME desktop stores first */
  if (G_UNLIKELY (database->stores == NULL))
    thunar_vfs_mime_database_initialize_stores (database);

  /* grab the user's desktop applications store */
  store = database->stores;

  /* verify that the applications/ directory exists */
  parent = thunar_vfs_path_get_parent (_thunar_vfs_monitor_handle_get_path (store->defaults_list_handle));
  if (thunar_vfs_path_to_string (parent, absolute_path, sizeof (absolute_path), NULL) > 0)
    succeed = xfce_mkdirhier (absolute_path, 0700, error);

  /* associate the application with the info */
  if (G_LIKELY (succeed))
    {
      /* generate the new application id list */
      pids = g_hash_table_lookup (store->defaults_list, info);
      if (G_UNLIKELY (pids == NULL))
        {
          /* generate a new list that contains only the application */
          nids = g_new (gchar *, 2);
          nids[0] = g_strdup (thunar_vfs_mime_application_get_desktop_id (application));
          nids[1] = NULL;
        }
      else
        {
          /* count the number of previously associated application ids */
          for (n = 0; pids[n] != NULL; ++n)
            ;

          /* allocate a new list and prepend the application */
          nids = g_new (gchar *, n + 2);
          nids[0] = g_strdup (thunar_vfs_mime_application_get_desktop_id (application));

          /* append the previously associated application ids */
          for (m = 0, n = 1; pids[m] != NULL; ++m)
            if (strcmp (pids[m], nids[0]) != 0)
              nids[n++] = g_strdup (pids[m]);

          /* null-terminate the new application id list */
          nids[n] = NULL;
        }

      /* activate the new application id list */
      g_hash_table_replace (store->defaults_list, thunar_vfs_mime_info_ref (info), nids);

      /* determine the absolute path to the defaults.list file */
      if (thunar_vfs_path_to_string (_thunar_vfs_monitor_handle_get_path (store->defaults_list_handle), absolute_path, sizeof (absolute_path), error) < 0)
        {
          succeed = FALSE;
          goto done;
        }

      /* write the default applications list to a temporary file */
      path = g_strdup_printf ("%s.XXXXXX", absolute_path);
      fd = g_mkstemp (path);
      if (G_LIKELY (fd >= 0))
        {
          /* wrap the descriptor in a file pointer */
          fp = fdopen (fd, "w");

          /* write the default application list content */
          fprintf (fp, "[Default Applications]\n");
          g_hash_table_foreach (store->defaults_list, (GHFunc) defaults_list_write, fp);
          fclose (fp);

          /* try to atomically rename the file */
          if (G_UNLIKELY (g_rename (path, absolute_path) < 0))
            {
              /* tell the caller that we failed */
              g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
              succeed = FALSE;

              /* be sure to remove the temporary file */
              g_unlink (path);
            }
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          succeed = FALSE;
        }
      g_free (path);
    }

done:
  /* release the lock on the database */
  g_mutex_unlock (database->lock);

  return succeed;
}



/**
 * thunar_vfs_mime_database_add_application:
 * @database : a #ThunarVfsMimeDatabase.
 * @info     : a #ThunarVfsMimeInfo.
 * @name     : the name for the application.
 * @exec     : the command for the application.
 * @error    : return location for errors or %NULL.
 *
 * Adds a new #ThunarVfsMimeApplication to the @database, whose
 * name is @name and command is @exec, and which can be used to
 * open files of type @info.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the newly created #ThunarVfsMimeApplication
 *               or %NULL on error.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_database_add_application (ThunarVfsMimeDatabase *database,
                                          ThunarVfsMimeInfo     *info,
                                          const gchar           *name,
                                          const gchar           *exec,
                                          GError               **error)
{
  ThunarVfsMimeApplication *application = NULL;
  gboolean                  succeed;
  gchar                    *desktop_id;
  gchar                    *directory;
  gchar                    *command;
  gchar                    *file;
  guint                     n;
  FILE                     *fp;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), NULL);
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), NULL);
  g_return_val_if_fail (exec != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* determine a file name for the new applications .desktop file */
  directory = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", TRUE);
  file = g_strconcat (directory, G_DIR_SEPARATOR_S, name, "-usercreated.desktop", NULL);
  for (n = 1; g_file_test (file, G_FILE_TEST_EXISTS); ++n)
    {
      g_free (file);
      file = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s-usercreated-%u.desktop", directory, name, n);
    }

  /* open the .desktop file for writing */
  fp = fopen (file, "w");
  if (G_UNLIKELY (fp == NULL))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
    }
  else
    {
      /* write the content and close the file */
      fprintf (fp, "[Desktop Entry]\n");
      fprintf (fp, "Encoding=UTF-8\n");
      fprintf (fp, "Type=Application\n");
      fprintf (fp, "NoDisplay=true\n");
      fprintf (fp, "Name=%s\n", name);
      fprintf (fp, "Exec=%s\n", exec);
      fprintf (fp, "MimeType=%s\n", thunar_vfs_mime_info_get_name (info));
      fclose (fp);

      /* update the mimeinfo.cache file for the directory */
      command = g_strdup_printf ("update-desktop-database \"%s\"", directory);
      succeed = g_spawn_command_line_sync (command, NULL, NULL, NULL, error);
      g_free (command);

      /* check if the update was successfull */
      if (G_LIKELY (succeed))
        {
          /* load the application from the .desktop file */
          desktop_id = g_path_get_basename (file);
          application = thunar_vfs_mime_application_new_from_file (file, desktop_id);
          g_free (desktop_id);

          /* this shouldn't happen, but you never know */
          if (G_UNLIKELY (application == NULL))
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to load application from file %s"), file);
        }
    }

  /* cleanup */
  g_free (directory);
  g_free (file);

  return application;
}



/**
 * thunar_vfs_mime_database_remove_application:
 * @database    : a #ThunarVfsMimeDatabase.
 * @application : a #ThunarVfsMimeApplication.
 * @error       : return location for errors or %NULL.
 *
 * Undoes the effect of thunar_vfs_mime_database_add_application() by removing
 * the user created @application from the @database. The @application must be
 * user created, and the removal will fail if its not (for example if its a
 * system-wide installed application launcher). See the documentation for
 * thunar_vfs_mime_application_is_usercreated() for details.
 *
 * Return value: %TRUE if the removal was successfull, %FALSE otherwise and
 *               @error will be set.
 **/
gboolean
thunar_vfs_mime_database_remove_application (ThunarVfsMimeDatabase    *database,
                                             ThunarVfsMimeApplication *application,
                                             GError                  **error)
{
  const gchar *desktop_id;
  gboolean     succeed = FALSE;
  gchar       *directory;
  gchar       *command;
  gchar       *file;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_DATABASE (database), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), FALSE);

  /* verify that the application is usercreated */
  if (!thunar_vfs_mime_application_is_usercreated (application))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, g_strerror (EINVAL));
      return FALSE;
    }

  /* determine the desktop-id of the application */
  desktop_id = thunar_vfs_mime_application_get_desktop_id (application);
  
  /* determine the save location for applications */
  directory = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", TRUE);
  if (G_UNLIKELY (directory == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR, g_strerror (ENOTDIR));
      return FALSE;
    }

  /* try to delete the desktop file */
  file = g_build_filename (directory, desktop_id, NULL);
  if (G_UNLIKELY (g_unlink (file) < 0))
    {
      /* tell the user that we failed to delete the application launcher */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Failed to remove \"%s\": %s"), file, g_strerror (errno));
    }
  else
    {
      /* update the mimeinfo.cache file for the directory */
      command = g_strdup_printf ("update-desktop-database \"%s\"", directory);
      succeed = g_spawn_command_line_sync (command, NULL, NULL, NULL, error);
      g_free (command);

      /* check if we succeed */
      if (G_LIKELY (succeed))
        {
          /* clear the application cache */
          g_mutex_lock (database->lock);
          g_hash_table_foreach_remove (database->applications, (GHRFunc) exo_noop_true, NULL);
          g_mutex_unlock (database->lock);
        }
    }

  /* cleanup */
  g_free (directory);
  g_free (file);

  return succeed;
}



/**
 * _thunar_vfs_mime_database:
 *
 * The shared reference to the global mime database. Only valid between
 * calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeDatabase *_thunar_vfs_mime_database = NULL;



/**
 * _thunar_vfs_mime_inode_directory:
 *
 * The shared #ThunarVfsMimeInfo instance for "inode/directory". Only valid
 * between calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeInfo *_thunar_vfs_mime_inode_directory = NULL;



/**
 * _thunar_vfs_mime_application_x_desktop:
 *
 * The shared #ThunarVfsMimeInfo instance for "application/x-desktop". Only valid
 * between calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeInfo *_thunar_vfs_mime_application_x_desktop = NULL;



/**
 * _thunar_vfs_mime_application_x_executable:
 *
 * The shared #ThunarVfsMimeInfo instance for "application/x-executable". Only valid
 * between calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeInfo *_thunar_vfs_mime_application_x_executable = NULL;



/**
 * _thunar_vfs_mime_application_x_shellscript:
 *
 * The shared #ThunarVfsMimeInfo instance for "application/x-shellscript". Only valid
 * between calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeInfo *_thunar_vfs_mime_application_x_shellscript = NULL;



/**
 * _thunar_vfs_mime_application_octet_stream:
 *
 * The shared #ThunarVfsMimeInfo instance for "application/octet-stream". Only valid
 * between calls to thunar_vfs_init() and thunar_vfs_shutdown().
 **/
ThunarVfsMimeInfo *_thunar_vfs_mime_application_octet_stream = NULL;



#define __THUNAR_VFS_MIME_DATABASE_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
