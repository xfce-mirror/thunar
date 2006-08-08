/* $Id$ */
/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <png.h>

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-thumb-jpeg.h>
#include <thunar-vfs/thunar-vfs-thumb-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_open(), g_rename() and g_unlink() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_open(filename, flags, mode) (open ((filename), (flags), (mode)))
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_unlink(filename) (unlink ((filename)))
#endif



/* thumbnailers cache support */
#define CACHE_MAJOR_VERSION (1)
#define CACHE_MINOR_VERSION (0)
#define CACHE_READ32(cache, offset) (GUINT32_FROM_BE (*((guint32 *) ((cache) + (offset)))))

/* fallback cache if the loading fails */
static const guint32 CACHE_FALLBACK[4] =
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
  CACHE_MAJOR_VERSION,
  CACHE_MINOR_VERSION,
  0,
  0,
#else
  GUINT32_SWAP_LE_BE_CONSTANT (CACHE_MAJOR_VERSION),
  GUINT32_SWAP_LE_BE_CONSTANT (CACHE_MINOR_VERSION),
  GUINT32_SWAP_LE_BE_CONSTANT (0),
  GUINT32_SWAP_LE_BE_CONSTANT (0),
#endif
};



/* Property identifiers */
enum
{
  PROP_0,
  PROP_SIZE,
};



static void     thunar_vfs_thumb_factory_class_init           (ThunarVfsThumbFactoryClass *klass);
static void     thunar_vfs_thumb_factory_init                 (ThunarVfsThumbFactory      *factory);
static void     thunar_vfs_thumb_factory_finalize             (GObject                    *object);
static void     thunar_vfs_thumb_factory_get_property         (GObject                    *object,
                                                               guint                       prop_id,
                                                               GValue                     *value,
                                                               GParamSpec                 *pspec);
static void     thunar_vfs_thumb_factory_set_property         (GObject                    *object,
                                                               guint                       prop_id,
                                                               const GValue               *value,
                                                               GParamSpec                 *pspec);
static gboolean thunar_vfs_thumb_factory_cache_lookup         (ThunarVfsThumbFactory      *factory,
                                                               const gchar                *mime_type,
                                                               gint                        mime_type_len,
                                                               gchar                     **script_return) G_GNUC_WARN_UNUSED_RESULT;
static void     thunar_vfs_thumb_factory_cache_load           (ThunarVfsThumbFactory      *factory,
                                                               const gchar                *cache_file);
static void     thunar_vfs_thumb_factory_cache_unload         (ThunarVfsThumbFactory      *factory);
static void     thunar_vfs_thumb_factory_cache_monitor        (ThunarVfsMonitor           *monitor,
                                                               ThunarVfsMonitorHandle     *handle,
                                                               ThunarVfsMonitorEvent       event,
                                                               ThunarVfsPath              *handle_path,
                                                               ThunarVfsPath              *event_path,
                                                               gpointer                    user_data);
static void     thunar_vfs_thumb_factory_cache_update         (ThunarVfsThumbFactory      *factory);
static gboolean thunar_vfs_thumb_factory_cache_timer          (gpointer                    user_data);
static void     thunar_vfs_thumb_factory_cache_timer_destroy  (gpointer                    user_data);
static void     thunar_vfs_thumb_factory_cache_watch          (GPid                        pid,
                                                               gint                        status,
                                                               gpointer                    user_data);
static void     thunar_vfs_thumb_factory_cache_watch_destroy  (gpointer                    user_data);



struct _ThunarVfsThumbFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsThumbFactory
{
  GObject __parent__;

  gchar                  *base_path;
  gchar                  *fail_path;
  ThunarVfsThumbSize      size;

  gchar                  *cache;
  guint                   cache_size;
  GMutex                 *cache_lock;
  guint                   cache_timer_id;
  guint                   cache_watch_id;
  guint                   cache_was_mapped : 1;
  ThunarVfsMonitorHandle *cache_handle;
};



static GObjectClass *thunar_vfs_thumb_factory_parent_class;



GType
thunar_vfs_thumb_factory_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsThumbFactory",
                                                 sizeof (ThunarVfsThumbFactoryClass),
                                                 thunar_vfs_thumb_factory_class_init,
                                                 sizeof (ThunarVfsThumbFactory),
                                                 thunar_vfs_thumb_factory_init,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_thumb_factory_class_init (ThunarVfsThumbFactoryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_thumb_factory_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_thumb_factory_finalize;
  gobject_class->get_property = thunar_vfs_thumb_factory_get_property;
  gobject_class->set_property = thunar_vfs_thumb_factory_set_property;

  /**
   * ThunarVfsThumbFactory::size:
   *
   * The #ThunarVfsThumbSize at which thumbnails should be
   * looked up and saved for this #ThunarVfsThumbFactory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size",
                                                      _("Size"),
                                                      _("The desired thumbnail size"),
                                                      THUNAR_VFS_TYPE_VFS_THUMB_SIZE,
                                                      THUNAR_VFS_THUMB_SIZE_NORMAL,
                                                      G_PARAM_CONSTRUCT_ONLY | EXO_PARAM_READWRITE));
}



static void
thunar_vfs_thumb_factory_init (ThunarVfsThumbFactory *factory)
{
  ThunarVfsPath *cache_path;
  gchar         *cache_file;

  /* pre-allocate the failed path, so we don't need to do that on every method call */
  factory->fail_path = g_strconcat (xfce_get_homedir (),
                                    G_DIR_SEPARATOR_S ".thumbnails"
                                    G_DIR_SEPARATOR_S "fail"
                                    G_DIR_SEPARATOR_S "thunar-vfs"
                                    G_DIR_SEPARATOR_S, NULL);

  /* we default to normal size here, so we don't need to override the constructor */
  factory->base_path = g_strconcat (xfce_get_homedir (), G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S "normal" G_DIR_SEPARATOR_S, NULL);
  factory->size = THUNAR_VFS_THUMB_SIZE_NORMAL;

  /* allocate the cache mutex */
  factory->cache_lock = g_mutex_new ();

  /* determine the thumbnailers.cache file */
  cache_file = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "Thunar/thumbnailers.cache", FALSE);

  /* monitor the thumbnailers.cache file for changes */
  cache_path = thunar_vfs_path_new (cache_file, NULL);
  factory->cache_handle = thunar_vfs_monitor_add_file (_thunar_vfs_monitor, cache_path, thunar_vfs_thumb_factory_cache_monitor, factory);
  _thunar_vfs_path_unref_nofree (cache_path);

  /* initially load the thumbnailers.cache file */
  thunar_vfs_thumb_factory_cache_load (factory, cache_file);

  /* schedule a timer to update the thumbnailers.cache every 5 minutes */
  factory->cache_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 5 * 60 * 1000, thunar_vfs_thumb_factory_cache_timer,
                                                factory, thunar_vfs_thumb_factory_cache_timer_destroy);

  /* cleanup */
  g_free (cache_file);
}



static void
thunar_vfs_thumb_factory_finalize (GObject *object)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  /* be sure to unload the cache file */
  thunar_vfs_thumb_factory_cache_unload (factory);

  /* stop any pending cache sources */
  if (G_LIKELY (factory->cache_timer_id != 0))
    g_source_remove (factory->cache_timer_id);
  if (G_UNLIKELY (factory->cache_watch_id != 0))
    g_source_remove (factory->cache_watch_id);

  /* unregister the monitor handle */
  thunar_vfs_monitor_remove (_thunar_vfs_monitor, factory->cache_handle);

  /* release the cache lock */
  g_mutex_free (factory->cache_lock);

  /* free the thumbnail paths */
  g_free (factory->base_path);
  g_free (factory->fail_path);

  (*G_OBJECT_CLASS (thunar_vfs_thumb_factory_parent_class)->finalize) (object);
}



static void
thunar_vfs_thumb_factory_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      g_value_set_enum (value, factory->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_vfs_thumb_factory_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      if (G_UNLIKELY (g_value_get_enum (value) == THUNAR_VFS_THUMB_SIZE_LARGE))
        {
          /* free the previous base path */
          g_free (factory->base_path);

          /* setup the new base path */
          factory->base_path = g_strconcat (xfce_get_homedir (), G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S "large" G_DIR_SEPARATOR_S, NULL);
          factory->size = THUNAR_VFS_THUMB_SIZE_LARGE;
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_vfs_thumb_factory_cache_lookup (ThunarVfsThumbFactory *factory,
                                       const gchar           *mime_type,
                                       gint                   mime_type_len,
                                       gchar                **script_return)
{
  const gchar *cache;
  gint         max;
  gint         mid;
  gint         min;
  gint         n;

  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  _thunar_vfs_return_val_if_fail (mime_type_len > 0, FALSE);
  _thunar_vfs_return_val_if_fail (mime_type != NULL, FALSE);

  /* acquire the cache lock */
  g_mutex_lock (factory->cache_lock);

  /* binary search on the mime types */
  for (cache = factory->cache, max = ((gint) CACHE_READ32 (cache, 8)) - 1, min = 0; max >= min; )
    {
      mid = (min + max) / 2;

      /* compare the string length */
      n = (gint) (CACHE_READ32 (cache, 16 + 8 * mid)) - mime_type_len;
      if (G_UNLIKELY (n == 0))
        {
          /* compare the strings themselves */
          n = strcmp (cache + CACHE_READ32 (cache, 16 + 8 * mid + 4), mime_type);
        }

      /* where to search next */
      if (n < 0)
        min = mid + 1;
      else if (n > 0)
        max = mid - 1;
      else
        {
          /* check if we should return the script */
          if (G_UNLIKELY (script_return != NULL))
            *script_return = g_strdup (cache + CACHE_READ32 (cache + CACHE_READ32 (cache, 12) + mid * 4, 0));

          /* we found it */
          break;
        }
    }

  /* release the cache lock */
  g_mutex_unlock (factory->cache_lock);

  return (max >= min);
}



static void
thunar_vfs_thumb_factory_cache_load (ThunarVfsThumbFactory *factory,
                                     const gchar           *cache_file)
{
  struct stat statb;
  gssize      m;
  gsize       n;
  gint        fd;

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));
  _thunar_vfs_return_if_fail (g_path_is_absolute (cache_file));
  _thunar_vfs_return_if_fail (factory->cache == NULL);

  /* try to open the thumbnailers.cache file */
  fd = g_open (cache_file, O_RDONLY, 0000);
  if (G_LIKELY (fd >= 0))
    {
      /* stat the file to get proper size info */
      if (fstat (fd, &statb) == 0 && statb.st_size >= 16)
        {
          /* remember the size of the cache */
          factory->cache_size = statb.st_size;

#ifdef HAVE_MMAP
          /* try to mmap() the file into memory */
          factory->cache = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
          if (G_LIKELY (factory->cache != MAP_FAILED))
            {
              /* remember that mmap() succeed */
              factory->cache_was_mapped = TRUE;

#ifdef HAVE_POSIX_MADVISE
              /* tell the system that we'll use this buffer quite often */
              posix_madvise (factory->cache, statb.st_size, POSIX_MADV_WILLNEED);
#endif
            }
          else
#endif
            {
              /* remember that mmap() failed */
              factory->cache_was_mapped = FALSE;

              /* allocate memory for the cache */
              factory->cache = g_malloc (statb.st_size);

              /* read the cache file */
              for (n = 0; n < statb.st_size; n += m)
                {
                  /* read the next chunk */
                  m = read (fd, factory->cache + n, statb.st_size - n);
                  if (G_UNLIKELY (m <= 0))
                    {
                      /* reset the cache */
                      g_free (factory->cache);
                      factory->cache = NULL;
                      break;
                    }
                }
            }
        }

      /* close the file handle */
      close (fd);
    }

  /* check if the cache was loaded successfully */
  if (G_LIKELY (factory->cache != NULL))
    {
      /* check that we actually support the file version */
      if (CACHE_READ32 (factory->cache, 0) != CACHE_MAJOR_VERSION || CACHE_READ32 (factory->cache, 4) != CACHE_MINOR_VERSION)
        thunar_vfs_thumb_factory_cache_unload (factory);
    }
  else
    {
      /* run the thunar-vfs-update-thumbnailers-cache-1 utility */
      thunar_vfs_thumb_factory_cache_update (factory);
    }

  /* use the fallback cache if the loading failed */
  if (G_UNLIKELY (factory->cache == NULL))
    {
      factory->cache = (gchar *) CACHE_FALLBACK;
      factory->cache_size = 16;
    }
}



static void
thunar_vfs_thumb_factory_cache_unload (ThunarVfsThumbFactory *factory)
{
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));
  _thunar_vfs_return_if_fail (factory->cache != NULL);

  /* check if any cache is loaded (and not using the fallback cache) */
  if (factory->cache != (gchar *) CACHE_FALLBACK)
    {
#ifdef HAVE_MMAP
      /* check if mmap() succeed */
      if (factory->cache_was_mapped)
        {
          /* just unmap the memory */
          munmap (factory->cache, factory->cache_size);
        }
      else
#endif
        {
          /* need to release the memory */
          g_free (factory->cache);
        }
    }

  /* reset the cache pointer */
  factory->cache = NULL;
}



static void
thunar_vfs_thumb_factory_cache_monitor (ThunarVfsMonitor       *monitor,
                                        ThunarVfsMonitorHandle *handle,
                                        ThunarVfsMonitorEvent   event,
                                        ThunarVfsPath          *handle_path,
                                        ThunarVfsPath          *event_path,
                                        gpointer                user_data)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (user_data);
  gchar                 *cache_file;

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));
  _thunar_vfs_return_if_fail (factory->cache_handle == handle);
  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));

  /* check the event */
  if (G_UNLIKELY (event == THUNAR_VFS_MONITOR_EVENT_DELETED))
    {
      /* some idiot deleted the cache file, regenerate it */
      thunar_vfs_thumb_factory_cache_update (factory);
    }
  else
    {
      /* determine the thumbnailers.cache file */
      cache_file = thunar_vfs_path_dup_string (handle_path);
      
      /* the thumbnailers.cache file was changed, reload it */
      g_mutex_lock (factory->cache_lock);
      thunar_vfs_thumb_factory_cache_unload (factory);
      thunar_vfs_thumb_factory_cache_load (factory, cache_file);
      g_mutex_unlock (factory->cache_lock);

      /* release the file path */
      g_free (cache_file);
    }
}



static void
thunar_vfs_thumb_factory_cache_update (ThunarVfsThumbFactory *factory)
{
  gchar *argv[2] = { LIBEXECDIR G_DIR_SEPARATOR_S "/thunar-vfs-update-thumbnailers-cache-1", NULL };
  GPid   pid;

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));

  /* check if we're already updating the cache */
  if (G_LIKELY (factory->cache_watch_id == 0))
    {
      /* try to spawn the command */
      if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL))
        {
          /* add a child watch for the updater process */
          factory->cache_watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid, thunar_vfs_thumb_factory_cache_watch,
                                                            factory, thunar_vfs_thumb_factory_cache_watch_destroy);

#ifdef HAVE_SETPRIORITY
          /* decrease the priority of the updater process */
          setpriority (PRIO_PROCESS, pid, 10);
#endif
        }
    }
}



static gboolean
thunar_vfs_thumb_factory_cache_timer (gpointer user_data)
{
  /* run the thunar-vfs-update-thumbnailers-cache-1 utility... */
  thunar_vfs_thumb_factory_cache_update (THUNAR_VFS_THUMB_FACTORY (user_data));

  /* ...and keep the timer running */
  return TRUE;
}



static void
thunar_vfs_thumb_factory_cache_timer_destroy (gpointer user_data)
{
  THUNAR_VFS_THUMB_FACTORY (user_data)->cache_timer_id = 0;
}



static void
thunar_vfs_thumb_factory_cache_watch (GPid     pid,
                                      gint     status,
                                      gpointer user_data)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (user_data);

  _thunar_vfs_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));

  /* a return value of 33 means that the thumbnailers.cache was updated by the
   * thunar-vfs-update-thumbnailers-cache-1 utilitiy and must be reloaded.
   */
  if (WIFEXITED (status) && WEXITSTATUS (status) == 33)
    {
      /* schedule a "changed" event for the thumbnailers.cache */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED,
                               _thunar_vfs_monitor_handle_get_path (factory->cache_handle));
    }

  /* close the PID (win32) */
  g_spawn_close_pid (pid);
}



static void
thunar_vfs_thumb_factory_cache_watch_destroy (gpointer user_data)
{
  THUNAR_VFS_THUMB_FACTORY (user_data)->cache_watch_id = 0;
}



/**
 * thunar_vfs_thumb_factory_new:
 * @size : the desired #ThunarVfsThumbSize.
 *
 * Allocates a new #ThunarVfsThumbFactory, that is able to
 * load and store thumbnails in the given @size.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsThumbFactory.
 **/
ThunarVfsThumbFactory*
thunar_vfs_thumb_factory_new (ThunarVfsThumbSize size)
{
  return g_object_new (THUNAR_VFS_TYPE_THUMB_FACTORY, "size", size, NULL);
}



/**
 * thunar_vfs_thumb_factory_lookup:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of the original file.
 *
 * Looks up the path to a thumbnail for @info in @factory and returns
 * the absolute path to the thumbnail file if a valid thumbnail is
 * found. Else %NULL is returned.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the path to a valid thumbnail for @info in @factory
 *               or %NULL if no valid thumbnail was found.
 **/
gchar*
thunar_vfs_thumb_factory_lookup_thumbnail (ThunarVfsThumbFactory *factory,
                                           const ThunarVfsInfo   *info)
{
  gchar  uri[THUNAR_VFS_PATH_MAXURILEN];
  gchar *path;
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  /* determine the URI for the path */
  if (thunar_vfs_path_to_uri (info->path, uri, sizeof (uri), NULL) >= 0)
    {
      /* determine the path to the thumbnail for the factory */
      md5 = exo_str_get_md5_str (uri);
      path = g_strconcat (factory->base_path, md5, ".png", NULL);
      g_free (md5);

      /* check if the path contains a valid thumbnail */
      if (thunar_vfs_thumbnail_is_valid (path, uri, info->mtime))
        return path;

      /* no valid thumbnail in the global repository */
      g_free (path);
    }

  return NULL;
}



/**
 * thunar_vfs_thumb_factory_can_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of a file.
 *
 * Checks if @factory can generate a thumbnail for
 * the file specified by @info.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory can generate a thumbnail for @info.
 **/
gboolean
thunar_vfs_thumb_factory_can_thumbnail (ThunarVfsThumbFactory *factory,
                                        const ThunarVfsInfo   *info)
{
  ThunarVfsPath *path;
  const gchar   *name;
  gint           name_len;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  /* we can only handle thumbnails for regular files */
  if (G_UNLIKELY (info->type != THUNAR_VFS_FILE_TYPE_REGULAR))
    return FALSE;

  /* we really don't want to generate a thumbnail for a thumbnail */
  for (path = info->path; path != NULL; path = thunar_vfs_path_get_parent (path))
    {
      name = thunar_vfs_path_get_name (path);
      if (*name == '.' && (strcmp (name + 1, "thumbnails") == 0 || strcmp (name + 1, "thumblocal") == 0))
        return FALSE;
    }

  /* determine the length of the mime type */
  name = thunar_vfs_mime_info_get_name (info->mime_info);
  name_len = strlen (name);

  /* image/jpeg should never be a problem, otherwise check if we have a thumbnailer in the cache */
  if (G_LIKELY (name_len == 10 && memcmp (name, "image/jpeg", 10) == 0)
      || thunar_vfs_thumb_factory_cache_lookup (factory, name, name_len, NULL))
    return !thunar_vfs_thumb_factory_has_failed_thumbnail (factory, info);

  /* we cannot handle the mime type */
  return FALSE;
}



/**
 * thunar_vfs_thumb_factory_has_failed_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of a file.
 *
 * Checks whether we know that @factory won't be able to generate
 * a thumbnail for the file referred to by @info.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory knows that it cannot generate
 *               a thumbnail for @info, else %TRUE.
 **/
gboolean
thunar_vfs_thumb_factory_has_failed_thumbnail (ThunarVfsThumbFactory *factory,
                                               const ThunarVfsInfo   *info)
{
  gchar  uri[THUNAR_VFS_PATH_MAXURILEN];
  gchar  path[4096];
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  /* determine the URI of the info */
  if (thunar_vfs_path_to_uri (info->path, uri, sizeof (uri), NULL) < 0)
    return FALSE;

  /* determine the path to the thumbnail */
  md5 = exo_str_get_md5_str (uri);
  g_snprintf (path, sizeof (path), "%s%s.png", factory->fail_path, md5);
  g_free (md5);

  return thunar_vfs_thumbnail_is_valid (path, uri, info->mtime);
}



static gchar*
thumbnailer_script_expand (const gchar *script,
                           const gchar *ipath,
                           const gchar *opath,
                           gint         size)
{
  const gchar *p;
  GString     *s;
  gchar       *quoted;
  gchar       *uri;

  s = g_string_new (NULL);
  for (p = script; *p != '\0'; ++p)
    {
      if (G_LIKELY (*p != '%'))
        {
          g_string_append_c (s, *p);
          continue;
        }

      switch (*++p)
        {
        case 'u':
          uri = g_filename_to_uri (ipath, NULL, NULL);
          if (G_LIKELY (uri != NULL))
            {
              quoted = g_shell_quote (uri);
              g_string_append (s, quoted);
              g_free (quoted);
              g_free (uri);
            }
          break;

        case 'i':
          quoted = g_shell_quote (ipath);
          g_string_append (s, quoted);
          g_free (quoted);
          break;

        case 'o':
          quoted = g_shell_quote (opath);
          g_string_append (s, quoted);
          g_free (quoted);
          break;

        case 's':
          g_string_append_printf (s, "%d", size);
          break;

        case '%':
          g_string_append_c (s, '%');
          break;

        case '\0':
          --p;
          break;

        default:
          break;
        }
    }

  return g_string_free (s, FALSE);
}



/**
 * thunar_vfs_thumb_factory_generate_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of the file for which a thumbnail
 *            should be generated.
 *
 * Tries to generate a thumbnail for the file referred to by @info in
 * @factory.
 *
 * The caller is responsible to free the returned #GdkPixbuf using
 * g_object_unref() when no longer needed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the thumbnail for the @uri or %NULL.
 **/
GdkPixbuf*
thunar_vfs_thumb_factory_generate_thumbnail (ThunarVfsThumbFactory *factory,
                                             const ThunarVfsInfo   *info)
{
  const gchar *name;
  GdkPixbuf   *pixbuf = NULL;
  GdkPixbuf   *scaled;
  gchar       *tmp_path;
  gchar       *command;
  gchar       *script;
  gchar       *path;
  gint         name_len;
  gint         status;
  gint         size;
  gint         fd;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  /* determine the pixel size of the thumbnail */
  size = G_LIKELY (factory->size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? 128 : 256;

  /* determine the absolute local path to the file */
  path = _thunar_vfs_path_translate_dup_string (info->path, THUNAR_VFS_PATH_SCHEME_FILE, NULL);
  if (G_UNLIKELY (path == NULL))
    return NULL;

  /* determine the length of the mime type */
  name = thunar_vfs_mime_info_get_name (info->mime_info);
  name_len = strlen (name);

  /* try the fast JPEG thumbnailer */
  if (G_LIKELY (name_len == 10 && memcmp (name, "image/jpeg", 10) == 0))
    pixbuf = thunar_vfs_thumb_jpeg_load (path, size);

  /* otherwise check if we have a thumbnailer script in the cache */
  if (pixbuf == NULL && thunar_vfs_thumb_factory_cache_lookup (factory, name, name_len, &script))
    {
      /* create a temporary file for the thumbnailer */
      fd = g_file_open_tmp (".thunar-vfs-thumbnail.XXXXXX", &tmp_path, NULL);
      if (G_LIKELY (fd >= 0))
        {
          /* determine the command for the script */
          command = thumbnailer_script_expand (script, path, tmp_path, size);

          /* run the thumbnailer script and load the generated file */
          if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && WIFEXITED (status) && WEXITSTATUS (status) == 0)
            {
              pixbuf = gdk_pixbuf_new_from_file (tmp_path, NULL);
            }

          /* unlink the temporary file */
          g_unlink (tmp_path);

          /* cleanup */
          g_free (tmp_path);
          g_free (command);
          close (fd);
        }

      /* cleanup */
      g_free (script);
    }

  /* check if we need to scale the image */
  if (pixbuf != NULL && (gdk_pixbuf_get_width (pixbuf) > size || gdk_pixbuf_get_height (pixbuf) > size))
    {
      scaled = exo_gdk_pixbuf_scale_ratio (pixbuf, size);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = scaled;
    }

  /* cleanup */
  g_free (path);

  return pixbuf;
}



/**
 * thunar_vfs_thumb_factory_store_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @pixbuf  : the thumbnail #GdkPixbuf to store or %NULL
 *            to remember the thumbnail for @uri as failed.
 * @info    : the #ThunarVfsInfo of the original file.
 * @error   : return location for errors or %NULL.
 *
 * Stores @pixbuf as thumbnail for @info in the right place, according
 * to the size set for @factory.
 *
 * If you specify %NULL for @pixbuf, the @factory will remember that
 * the thumbnail generation for @info failed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if the thumbnail was stored successfully,
 *               else %FALSE.
 **/
gboolean
thunar_vfs_thumb_factory_store_thumbnail (ThunarVfsThumbFactory *factory,
                                          const GdkPixbuf       *pixbuf,
                                          const ThunarVfsInfo   *info,
                                          GError               **error)
{
  const gchar *base_path;
  GdkPixbuf   *thumbnail;
  gboolean     succeed;
  gchar       *dst_path;
  gchar       *tmp_path;
  gchar       *mtime;
  gchar       *size;
  gchar       *md5;
  gchar       *uri;
  gint         tmp_fd;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check whether we should save a thumbnail or remember failed generation */
  base_path = G_LIKELY (pixbuf != NULL) ? factory->base_path : factory->fail_path;

  /* verify that the target directory exists */
  if (!xfce_mkdirhier (base_path, 0700, error))
    return FALSE;

  /* determine the URI of the file */
  uri = thunar_vfs_path_dup_uri (info->path);

  /* determine the MD5 sum for the URI */
  md5 = exo_str_get_md5_str (uri);

  /* try to open a temporary file to write the thumbnail to */
  tmp_path = g_strconcat (base_path, md5, ".png.XXXXXX", NULL);
  tmp_fd = g_mkstemp (tmp_path);
  if (G_UNLIKELY (tmp_fd < 0))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      g_free (md5);
      g_free (uri);
      return FALSE;
    }

  /* close the temporary file as it exists now, and hence
   * we successfully avoided a race condition there.
   */
  close (tmp_fd);

  /* generate a 1x1 image if we're storing a failure */
  thumbnail = (pixbuf != NULL) ? GDK_PIXBUF (pixbuf) : gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);

  /* determine string representations for the modification time and size */
  mtime = g_strdup_printf ("%lu", (gulong) info->mtime);
  size = g_strdup_printf ("%" G_GUINT64_FORMAT, (guint64) info->size);

  /* write the thumbnail to the temporary location */
  succeed = gdk_pixbuf_save (thumbnail, tmp_path, "png", error,
                             "tEXt::Thumb::URI", uri,
                             "tEXt::Thumb::Size", size,
                             "tEXt::Thumb::MTime", mtime,
                             "tEXt::Thumb::Mimetype", thunar_vfs_mime_info_get_name (info->mime_info),
                             "tEXt::Software", "Thunar-VFS Thumbnail Factory",
                             NULL);

  /* drop the failed thumbnail pixbuf (if any) */
  if (G_UNLIKELY (pixbuf == NULL))
    g_object_unref (G_OBJECT (thumbnail));

  /* rename the file to the final location */
  if (G_LIKELY (succeed))
    {
      dst_path = g_strconcat (base_path, md5, ".png", NULL);
      if (G_UNLIKELY (g_rename (tmp_path, dst_path) < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          succeed = FALSE;
        }
      g_free (dst_path);
    }

  /* cleanup */
  g_free (tmp_path);
  g_free (mtime);
  g_free (size);
  g_free (md5);
  g_free (uri);

  return succeed;
}



/**
 * thunar_vfs_thumbnail_for_path:
 * @path : the #ThunarVfsPath to the original file.
 * @size : the desired #ThunarVfsThumbSize.
 *
 * Returns the absolute path to the thumbnail location
 * for the file described by @path, at the given @size.
 *
 * The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the absolute path to the thumbnail
 *               location for @path at @size.
 **/
gchar*
thunar_vfs_thumbnail_for_path (const ThunarVfsPath *path,
                               ThunarVfsThumbSize   size)
{
  gchar *thumbnail;
  gchar *md5;
  gchar *uri;

  uri = thunar_vfs_path_dup_uri (path);
  md5 = exo_str_get_md5_str (uri);
  thumbnail = g_strconcat (xfce_get_homedir (),
                           G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S,
                           (size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? "normal" : "large",
                           G_DIR_SEPARATOR_S, md5, ".png", NULL);
  g_free (md5);
  g_free (uri);

  return thumbnail;
}



/**
 * thunar_vfs_thumbnail_is_valid:
 * @thumbnail : the absolute path to a thumbnail file.
 * @uri       : the URI of the original file.
 * @mtime     : the modification time of the original file.
 *
 * Checks whether the file located at @thumbnail contains a
 * valid thumbnail for the file described by @uri and @mtime.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @thumbnail is a valid thumbnail for
 *               the file referred to by @uri, else %FALSE.
 **/
gboolean
thunar_vfs_thumbnail_is_valid (const gchar      *thumbnail,
                               const gchar      *uri,
                               ThunarVfsFileTime mtime)
{
  png_structp  png_ptr;
  png_infop    info_ptr;
  png_textp    text_ptr;
  gboolean     is_valid = FALSE;
  gchar        signature[4];
  FILE        *fp;
  gint         n_checked;
  gint         n_text;
  gint         n;

  g_return_val_if_fail (g_path_is_absolute (thumbnail), FALSE);
  g_return_val_if_fail (uri != NULL && *uri != '\0', FALSE);

  /* try to open the thumbnail file */
  fp = fopen (thumbnail, "r");
  if (G_UNLIKELY (fp == NULL))
    return FALSE;

  /* read the png signature */
  if (G_UNLIKELY (fread (signature, 1, sizeof (signature), fp) != sizeof (signature)))
    goto done0;

  /* verify the png signature */
  if (G_LIKELY (png_check_sig ((png_bytep) signature, sizeof (signature))))
    rewind (fp);
  else
    goto done0;

  /* allocate the png read structure */
  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (G_UNLIKELY (png_ptr == NULL))
    goto done0;

  /* allocate the png info structure */
  info_ptr = png_create_info_struct (png_ptr);
  if (G_UNLIKELY (info_ptr == NULL))
    {
      png_destroy_read_struct (&png_ptr, NULL, NULL);
      goto done0;
    }

  /* read the png info from the file */
  png_init_io (png_ptr, fp);
  png_read_info (png_ptr, info_ptr);

  /* verify the tEXt attributes defined by the thumbnail spec */
  n_text = png_get_text (png_ptr, info_ptr, &text_ptr, &n_text);
  for (n = 0, n_checked = 0; n_checked < 2 && n < n_text; ++n)
    {
      if (strcmp (text_ptr[n].key, "Thumb::MTime") == 0)
        {
          /* verify the modification time */
          if (G_UNLIKELY (strtol (text_ptr[n].text, NULL, 10) != mtime))
            goto done1;
          ++n_checked;
        }
      else if (strcmp (text_ptr[n].key, "Thumb::URI") == 0)
        {
          /* check if the URIs are equal */
          if (strcmp (text_ptr[n].text, uri) != 0)
            goto done1;
          ++n_checked;
        }
    }

  /* check if all required attributes were checked successfully */
  is_valid = (n_checked == 2);

done1:
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
done0:
  fclose (fp);
  return is_valid;
}



/**
 * _thunar_vfs_thumbnail_remove_for_path:
 * @path : the #ThunarVfsPath to the file whose thumbnails should be removed.
 *
 * Removes any existing thumbnails for the file at the specified @path.
 **/
void
_thunar_vfs_thumbnail_remove_for_path (const ThunarVfsPath *path)
{
  ThunarVfsThumbSize size;
  gchar             *thumbnail_path;

  /* drop the normal and large thumbnails for the path */
  for (size = THUNAR_VFS_THUMB_SIZE_NORMAL; size < THUNAR_VFS_THUMB_SIZE_LARGE; ++size)
    {
      /* the thumbnail may not exist, so simply ignore errors here */
      thumbnail_path = thunar_vfs_thumbnail_for_path (path, size);
      g_unlink (thumbnail_path);
      g_free (thumbnail_path);
    }
}



#define __THUNAR_VFS_THUMB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
