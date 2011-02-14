/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
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

#include <gio/gio.h>
#include <libxfce4ui/libxfce4ui.h>

#include <thunarx/thunarx.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-exec.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-user.h>
#include <thunar/thunar-util.h>



/* Additional flags associated with a ThunarFile */
#define THUNAR_FILE_IN_DESTRUCTION          0x04
#define THUNAR_FILE_OWNS_METAFILE_REFERENCE 0x08
#define THUNAR_FILE_OWNS_EMBLEM_NAMES       0x10


/* the watch count is stored in the GObject data
 * list, as it is needed only for a very few
 * files.
 */
#define THUNAR_FILE_GET_WATCH_COUNT(file)        (GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT ((file)), thunar_file_watch_count_quark)))
#define THUNAR_FILE_SET_WATCH_COUNT(file, count) (g_object_set_qdata (G_OBJECT ((file)), thunar_file_watch_count_quark, GINT_TO_POINTER ((count))))



/* Signal identifiers */
enum
{
  DESTROY,
  LAST_SIGNAL,
};



static void               thunar_file_info_init                (ThunarxFileInfoIface   *iface);
static void               thunar_file_dispose                  (GObject                *object);
static void               thunar_file_finalize                 (GObject                *object);
static gchar             *thunar_file_info_get_name            (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_uri             (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_parent_uri      (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_uri_scheme      (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_mime_type       (ThunarxFileInfo        *file_info);
static gboolean           thunar_file_info_has_mime_type       (ThunarxFileInfo        *file_info,
                                                                const gchar            *mime_type);
static gboolean           thunar_file_info_is_directory        (ThunarxFileInfo        *file_info);
static GFileInfo         *thunar_file_info_get_file_info       (ThunarxFileInfo        *file_info);
static GFileInfo         *thunar_file_info_get_filesystem_info (ThunarxFileInfo        *file_info);
static GFile             *thunar_file_info_get_location        (ThunarxFileInfo        *file_info);
static void               thunar_file_info_changed             (ThunarxFileInfo        *file_info);
static gboolean           thunar_file_denies_access_permission (const ThunarFile       *file,
                                                                ThunarFileMode          usr_permissions,
                                                                ThunarFileMode          grp_permissions,
                                                                ThunarFileMode          oth_permissions);
static ThunarMetafile    *thunar_file_get_metafile             (ThunarFile             *file);
static void               thunar_file_monitor                  (GFileMonitor           *monitor,
                                                                GFile                  *path,
                                                                GFile                  *other_path,
                                                                GFileMonitorEvent       event_type,
                                                                gpointer                user_data);



G_LOCK_DEFINE_STATIC (file_cache_mutex);



static ThunarUserManager *user_manager;
static ThunarMetafile    *metafile;
static GHashTable        *file_cache;
static guint32            effective_user_id;
static GQuark             thunar_file_thumb_path_quark;
static GQuark             thunar_file_watch_count_quark;
static GQuark             thunar_file_emblem_names_quark;
static guint              file_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (ThunarFile, thunar_file, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (THUNARX_TYPE_FILE_INFO, thunar_file_info_init))



#ifdef G_ENABLE_DEBUG
static gboolean thunar_file_atexit_registered = FALSE;

static void
thunar_file_atexit_foreach (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  gchar *uri;

  uri = g_file_get_uri (key);
  g_print ("--> %s (%u)\n", uri, G_OBJECT (value)->ref_count);
  g_free (uri);
}

static void
thunar_file_atexit (void)
{
  G_LOCK (file_cache_mutex);

  if (file_cache == NULL || g_hash_table_size (file_cache) == 0)
    {
      G_UNLOCK (file_cache_mutex);
      return;
    }

  g_print ("--- Leaked a total of %u ThunarFile objects:\n",
           g_hash_table_size (file_cache));

  g_hash_table_foreach (file_cache, thunar_file_atexit_foreach, NULL);

  g_print ("\n");

  G_UNLOCK (file_cache_mutex);
}
#endif



static void
thunar_file_class_init (ThunarFileClass *klass)
{
  GObjectClass *gobject_class;

#ifdef G_ENABLE_DEBUG
  if (G_UNLIKELY (!thunar_file_atexit_registered))
    {
      g_atexit (thunar_file_atexit);
      thunar_file_atexit_registered = TRUE;
    }
#endif

  /* pre-allocate the required quarks */
  thunar_file_thumb_path_quark = g_quark_from_static_string ("thunar-file-thumb-path");
  thunar_file_watch_count_quark = g_quark_from_static_string ("thunar-file-watch-count");
  thunar_file_emblem_names_quark = g_quark_from_static_string ("thunar-file-emblem-names");

  /* grab a reference on the user manager */
  user_manager = thunar_user_manager_get_default ();

  /* determine the effective user id of the process */
  effective_user_id = geteuid ();

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_file_dispose;
  gobject_class->finalize = thunar_file_finalize;

  /**
   * ThunarFile::destroy:
   * @file : the #ThunarFile instance.
   *
   * Emitted when the system notices that the @file
   * was destroyed.
   **/
  file_signals[DESTROY] =
    g_signal_new (I_("destroy"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ThunarFileClass, destroy),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_file_init (ThunarFile *file)
{
}



static void
thunar_file_info_init (ThunarxFileInfoIface *iface)
{
  iface->get_name = thunar_file_info_get_name;
  iface->get_uri = thunar_file_info_get_uri;
  iface->get_parent_uri = thunar_file_info_get_parent_uri;
  iface->get_uri_scheme = thunar_file_info_get_uri_scheme;
  iface->get_mime_type = thunar_file_info_get_mime_type;
  iface->has_mime_type = thunar_file_info_has_mime_type;
  iface->is_directory = thunar_file_info_is_directory;
  iface->get_file_info = thunar_file_info_get_file_info;
  iface->get_filesystem_info = thunar_file_info_get_filesystem_info;
  iface->get_location = thunar_file_info_get_location;
  iface->changed = thunar_file_info_changed;
}



static void
thunar_file_dispose (GObject *object)
{
  ThunarFile *file = THUNAR_FILE (object);

  /* check that we don't recurse here */
  if (G_LIKELY ((file->flags & THUNAR_FILE_IN_DESTRUCTION) == 0))
    {
      /* emit the "destroy" signal */
      file->flags |= THUNAR_FILE_IN_DESTRUCTION;
      g_signal_emit (object, file_signals[DESTROY], 0);
      file->flags &= ~THUNAR_FILE_IN_DESTRUCTION;
    }

  (*G_OBJECT_CLASS (thunar_file_parent_class)->dispose) (object);
}



static void
thunar_file_finalize (GObject *object)
{
  ThunarFile *file = THUNAR_FILE (object);

  /* verify that nobody's watching the file anymore */
#ifdef G_ENABLE_DEBUG
  if (G_UNLIKELY (THUNAR_FILE_GET_WATCH_COUNT (file) != 0))
    {
      g_error ("Attempt to finalize a ThunarFile, which has an active "
               "watch count of %d", THUNAR_FILE_GET_WATCH_COUNT (file));
    }
#endif

  /* drop the entry from the cache */
  G_LOCK (file_cache_mutex);
  g_hash_table_remove (file_cache, file->gfile);
  G_UNLOCK (file_cache_mutex);

  /* drop a reference on the metadata if we own one */
  if ((file->flags & THUNAR_FILE_OWNS_METAFILE_REFERENCE) != 0)
    g_object_unref (G_OBJECT (metafile));

  /* release file info */
  if (file->info != NULL)
    g_object_unref (file->info);

  /* free the custom icon name */
  g_free (file->custom_icon_name);
  
  /* free display name and basename */
  g_free (file->display_name);
  g_free (file->basename);

  /* free the thumbnail path */
  g_free (file->thumbnail_path);

  /* release file */
  g_object_unref (file->gfile);

  (*G_OBJECT_CLASS (thunar_file_parent_class)->finalize) (object);
}



static gchar *
thunar_file_info_get_name (ThunarxFileInfo *file_info)
{
  return g_strdup (thunar_file_get_basename (THUNAR_FILE (file_info)));
}



static gchar*
thunar_file_info_get_uri (ThunarxFileInfo *file_info)
{
  return g_file_get_uri (THUNAR_FILE (file_info)->gfile);
}



static gchar*
thunar_file_info_get_parent_uri (ThunarxFileInfo *file_info)
{
  GFile *parent;
  gchar *uri = NULL;

  parent = g_file_get_parent (THUNAR_FILE (file_info)->gfile);
  if (G_LIKELY (parent != NULL))
    {
      uri = g_file_get_uri (parent);
      g_object_unref (parent);
    }

  return uri;
}



static gchar*
thunar_file_info_get_uri_scheme (ThunarxFileInfo *file_info)
{
  return g_file_get_uri_scheme (THUNAR_FILE (file_info)->gfile);
}



static gchar*
thunar_file_info_get_mime_type (ThunarxFileInfo *file_info)
{
  if (THUNAR_FILE (file_info)->info == NULL)
    return NULL;

  return g_strdup (g_file_info_get_content_type (THUNAR_FILE (file_info)->info));
}



static gboolean
thunar_file_info_has_mime_type (ThunarxFileInfo *file_info,
                                const gchar     *mime_type)
{
  if (THUNAR_FILE (file_info)->info == NULL)
    return FALSE;

  return g_content_type_is_a (g_file_info_get_content_type (THUNAR_FILE (file_info)->info), mime_type);
}



static gboolean
thunar_file_info_is_directory (ThunarxFileInfo *file_info)
{
  return thunar_file_is_directory (THUNAR_FILE (file_info));
}



static GFileInfo *
thunar_file_info_get_file_info (ThunarxFileInfo *file_info)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_info), NULL);
  
  if (THUNAR_FILE (file_info)->info != NULL)
    return g_object_ref (THUNAR_FILE (file_info)->info);
  else
    return NULL;
}



static GFileInfo *
thunar_file_info_get_filesystem_info (ThunarxFileInfo *file_info)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_info), NULL);

  return g_file_query_filesystem_info (THUNAR_FILE (file_info)->gfile, 
                                       THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                       NULL, NULL);
}



static GFile *
thunar_file_info_get_location (ThunarxFileInfo *file_info)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_info), NULL);
  return g_object_ref (THUNAR_FILE (file_info)->gfile);
}



static void
thunar_file_info_changed (ThunarxFileInfo *file_info)
{
  thunar_file_set_thumb_state (THUNAR_FILE (file_info), THUNAR_FILE_THUMB_STATE_UNKNOWN);

  /* tell the file monitor that this file changed */
  thunar_file_monitor_file_changed (THUNAR_FILE (file_info));
}



static gboolean
thunar_file_denies_access_permission (const ThunarFile *file,
                                      ThunarFileMode    usr_permissions,
                                      ThunarFileMode    grp_permissions,
                                      ThunarFileMode    oth_permissions)
{
  ThunarFileMode mode;
  ThunarGroup   *group;
  ThunarUser    *user;
  gboolean       result;
  GList         *groups;
  GList         *lp;

  /* query the file mode */
  mode = thunar_file_get_mode (file);

  /* query the owner of the file, if we cannot determine
   * the owner, we can't tell if we're denied to access
   * the file, so we simply return FALSE then.
   */
  user = thunar_file_get_user (file);
  if (G_UNLIKELY (user == NULL))
    return FALSE;

  /* root is allowed to do everything */
  if (G_UNLIKELY (effective_user_id == 0))
    return FALSE;

  if (thunar_user_is_me (user))
    {
      /* we're the owner, so the usr permissions must be granted */
      result = ((mode & usr_permissions) == 0);
  
      /* release the user */
      g_object_unref (G_OBJECT (user));
    }
  else
    {
      group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          /* release the file owner */
          g_object_unref (G_OBJECT (user));

          /* determine the effective user */
          user = thunar_user_manager_get_user_by_id (user_manager, effective_user_id);
          if (G_LIKELY (user != NULL))
            {
              /* check the group permissions */
              groups = thunar_user_get_groups (user);
              for (lp = groups; lp != NULL; lp = lp->next)
                if (THUNAR_GROUP (lp->data) == group)
                  {
                    g_object_unref (G_OBJECT (user));
                    g_object_unref (G_OBJECT (group));
                    return ((mode & grp_permissions) == 0);
                  }
          
              /* release the effective user */
              g_object_unref (G_OBJECT (user));
            }

          /* release the file group */
          g_object_unref (G_OBJECT (group));
        }

      /* check other permissions */
      result = ((mode & oth_permissions) == 0);
    }

  return result;
}



static ThunarMetafile*
thunar_file_get_metafile (ThunarFile *file)
{
  if ((file->flags & THUNAR_FILE_OWNS_METAFILE_REFERENCE) == 0)
    {
      /* take a reference on the metafile for this file */
      if (G_UNLIKELY (metafile == NULL))
        {
          metafile = thunar_metafile_get_default ();
          g_object_add_weak_pointer (G_OBJECT (metafile), (gpointer) &metafile);
        }
      else
        {
          g_object_ref (G_OBJECT (metafile));
        }

      /* remember that we own a reference now */
      file->flags |= THUNAR_FILE_OWNS_METAFILE_REFERENCE;
    }

  return metafile;
}



static void
thunar_file_monitor_update (GFile             *path,
                            GFileMonitorEvent  event_type)
{
  ThunarFile *file;

  _thunar_return_if_fail (G_IS_FILE (path));
  
  file = thunar_file_cache_lookup (path);
  if (G_LIKELY (file != NULL))
    {
      switch (event_type)
        {
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
          thunar_file_reload (file);
          break;

        case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        case G_FILE_MONITOR_EVENT_DELETED:
          thunar_file_reload (file);
          break;

        default:
          break;
        }
    }
}



static void
thunar_file_monitor (GFileMonitor     *monitor,
                     GFile            *path,
                     GFile            *other_path,
                     GFileMonitorEvent event_type,
                     gpointer          user_data)
{
  ThunarFile *file = THUNAR_FILE (user_data);

  _thunar_return_if_fail (G_IS_FILE_MONITOR (monitor));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (G_UNLIKELY (!G_IS_FILE (path) || !g_file_equal (path, file->gfile)))
    return;

  if (G_LIKELY (G_IS_FILE (path)))
    thunar_file_monitor_update (path, event_type);

  if (G_UNLIKELY (G_IS_FILE (other_path)))
    thunar_file_monitor_update (other_path, event_type);
}



/**
 * thunar_file_get:
 * @file  : a #GFile.
 * @error : return location for errors.
 *
 * Looks up the #ThunarFile referred to by @file. This function may return a
 * ThunarFile even though the file doesn't actually exist. This is the case
 * with remote URIs (like SFTP) for instance, if they are not mounted.
 *
 * The caller is responsible to call g_object_unref()
 * when done with the returned object.
 *
 * Return value: the #ThunarFile for @file or %NULL on errors.
 **/
ThunarFile*
thunar_file_get (GFile   *gfile,
                 GError **error)
{
  ThunarFile *file;

  _thunar_return_val_if_fail (G_IS_FILE (gfile), NULL);

  /* check if we already have a cached version of that file */
  file = thunar_file_cache_lookup (gfile);
  if (G_UNLIKELY (file != NULL))
    {
      /* take a reference for the caller */
      g_object_ref (file);
    }
  else
    {
      /* allocate a new object */
      file = g_object_new (THUNAR_TYPE_FILE, NULL);
      file->gfile = g_object_ref (gfile);
      file->info = NULL;
      file->custom_icon_name = NULL;
      file->display_name = NULL;
      file->basename = NULL;

      if (thunar_file_load (file, NULL, error))
        {
          /* insert the file into the cache */
          G_LOCK (file_cache_mutex);
          g_hash_table_insert (file_cache, g_object_ref (file->gfile), file);
          G_UNLOCK (file_cache_mutex);
        }
      else
        {
          /* failed loading, destroy the file */
          g_object_unref (file);

          /* make sure we return NULL */
          file = NULL;
        }
    }

  return file;
}



/**
 * thunar_file_get_for_uri:
 * @uri   : an URI or an absolute filename.
 * @error : return location for errors or %NULL.
 *
 * Convenience wrapper function for thunar_file_get_for_path(), as its
 * often required to determine a #ThunarFile for a given @uri.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the #ThunarFile for the given @uri or %NULL if
 *               unable to determine.
 **/
ThunarFile*
thunar_file_get_for_uri (const gchar *uri,
                         GError     **error)
{
  ThunarFile *file;
  GFile      *path;

  _thunar_return_val_if_fail (uri != NULL, NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  path = g_file_new_for_commandline_arg (uri);
  file = thunar_file_get (path, error);
  g_object_unref (path);

  return file;
}



/**
 * thunar_file_load:
 * @file        : a #ThunarFile.
 * @cancellable : a #GCancellable.
 * @error       : return location for errors or %NULL.
 *
 * Loads all information about the file. As this is a possibly
 * blocking call, it can be cancelled using @cancellable. 
 *
 * If loading the file fails or the operation is cancelled,
 * @error will be set.
 *
 * Return value: %TRUE on success, %FALSE on error or interruption.
 **/
gboolean
thunar_file_load (ThunarFile   *file,
                  GCancellable *cancellable,
                  GError      **error)
{
  GKeyFile *key_file;
  GError   *err = NULL;
  GFile    *thumbnail_dir;
  gchar    *base_name;
  gchar    *md5_hash;
  gchar    *p;
  gchar    *thumbnail_dir_path;
  gchar    *uri = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);

  /* release the current file info */
  if (file->info != NULL)
    {
      g_object_unref (file->info);
      file->info = NULL;
    }

  /* free the custom icon name */
  g_free (file->custom_icon_name);
  file->custom_icon_name = NULL;

  /* free display name and basename */
  g_free (file->display_name);
  file->display_name = NULL;

  g_free (file->basename);
  file->basename = NULL;

  /* free thumbnail path */
  g_free (file->thumbnail_path);
  file->thumbnail_path = NULL;

  /* assume the file is mounted by default */
  file->is_mounted = TRUE;

  /* query a new file info */
  file->info = g_file_query_info (file->gfile,
                                  THUNARX_FILE_INFO_NAMESPACE,
                                  G_FILE_QUERY_INFO_NONE,
                                  cancellable, &err);

  if (err == NULL)
    {
      if (g_file_info_get_file_type (file->info) == G_FILE_TYPE_MOUNTABLE)
        {
          file->is_mounted = 
            !g_file_info_get_attribute_boolean (file->info,
                                                G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT);
        }
    }
  else
    {
      if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_NOT_MOUNTED)
        {
          file->is_mounted = FALSE;
          g_clear_error (&err);
        }
    }

  /* determine the basename */
  file->basename = g_file_get_basename (file->gfile);
  _thunar_assert (file->basename != NULL);

  /* assume all files are not thumbnails themselves */
  file->is_thumbnail = FALSE;

  /* create a GFile for the $HOME/.thumbnails/ directory */
  thumbnail_dir_path = g_build_filename (xfce_get_homedir (), ".thumbnails", NULL);
  thumbnail_dir = g_file_new_for_path (thumbnail_dir_path);

  /* check if this file is a thumbnail */
  if (g_file_has_prefix (file->gfile, thumbnail_dir))
    {
      /* remember that this file is a thumbnail */
      file->is_thumbnail = TRUE;

      /* use the filename as custom icon name for thumbnails */
      file->custom_icon_name = g_file_get_path (file->gfile);
    }

  /* check if this file is a desktop entry */
  if (thunar_file_is_desktop_file (file))
    {
      /* determine the custom icon and display name for .desktop files */

      /* query a key file for the .desktop file */
      key_file = thunar_g_file_query_key_file (file->gfile, cancellable, NULL);
      if (key_file != NULL)
        {
          /* read the icon name from the .desktop file */
          file->custom_icon_name = g_key_file_get_string (key_file, 
                                                          G_KEY_FILE_DESKTOP_GROUP,
                                                          G_KEY_FILE_DESKTOP_KEY_ICON,
                                                          NULL);

          if (G_UNLIKELY (exo_str_is_empty (file->custom_icon_name)))
            {
              /* make sure we set null if the string is empty else the assertion in 
               * thunar_icon_factory_lookup_icon() will fail */
              g_free (file->custom_icon_name);
              file->custom_icon_name = NULL;
            }
          else
            {
              /* drop any suffix (e.g. '.png') from themed icons */
              if (!g_path_is_absolute (file->custom_icon_name))
                {
                  p = strrchr (file->custom_icon_name, '.');
                  if (p != NULL)
                    *p = '\0';
                }
            }

          /* read the display name from the .desktop file (will be overwritten later
           * if it's undefined here) */
          file->display_name = g_key_file_get_string (key_file,
                                                      G_KEY_FILE_DESKTOP_GROUP,
                                                      G_KEY_FILE_DESKTOP_KEY_NAME,
                                                      NULL);
          
          /* check if we have a display name now */
          if (file->display_name != NULL)
            {
              /* drop the name if it's empty or has invalid encoding */
              if (*file->display_name == '\0' 
                  || !g_utf8_validate (file->display_name, -1, NULL))
                {
                  g_free (file->display_name);
                  file->display_name = NULL;
                }
            }

          /* free the key file */
          g_key_file_free (key_file);
        }
      else
        {
          /* cannot parse the key file, no custom icon */
          file->custom_icon_name = NULL;
        }
    }
  else
    {
      /* not a .desktop file, no custom icon */
      file->custom_icon_name = NULL;
    }

  /* free $HOME/.thumbnails/ GFile and path */
  g_object_unref (thumbnail_dir);
  g_free (thumbnail_dir_path);

  /* determine the display name */
  if (file->display_name == NULL)
    {
      if (file->info != NULL)
        {
          if (g_strcmp0 (g_file_info_get_display_name (file->info), "/") == 0)
            file->display_name = g_strdup (_("File System"));
          else
            file->display_name = g_strdup (g_file_info_get_display_name (file->info));
        }
      else
        {
          if (g_file_is_native (file->gfile))
            {
              uri = g_file_get_path (file->gfile);
              if (uri == NULL)
                uri = g_file_get_uri (file->gfile);
            }
          else
            {
              uri = g_file_get_uri (file->gfile);
            }

          file->display_name = g_filename_display_name (uri);
          g_free (uri);
        }
    }

  /* set thumb state to unknown */
  file->flags = 
    (file->flags & ~THUNAR_FILE_THUMB_STATE_MASK) | THUNAR_FILE_THUMB_STATE_UNKNOWN;

  /* determine thumbnail path */
  uri = g_file_get_uri (file->gfile);
  md5_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  base_name = g_strdup_printf ("%s.png", md5_hash);
  file->thumbnail_path = g_build_filename (xfce_get_homedir (), ".thumbnails", 
                                           "normal", base_name, NULL);
  g_free (base_name);
  g_free (md5_hash);
  g_free (uri);

  /* TODO monitor the thumbnail file for changes */

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

/**
 * thunar_file_get_parent:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors.
 *
 * Determines the parent #ThunarFile for @file. If @file has no parent or
 * the user is not allowed to open the parent folder of @file, %NULL will
 * be returned and @error will be set to point to a #GError that
 * describes the cause. Else, the #ThunarFile will be returned, and
 * the caller must call g_object_unref() on it.
 *
 * You may want to call thunar_file_has_parent() first to
 * determine whether @file has a parent.
 *
 * Return value: the parent #ThunarFile or %NULL.
 **/
ThunarFile*
thunar_file_get_parent (const ThunarFile *file,
                        GError          **error)
{
  ThunarFile *parent = NULL;
  GFile      *parent_file;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  parent_file = g_file_get_parent (file->gfile);

  if (parent_file == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, _("The root folder has no parent"));
      return NULL;
    }

  parent = thunar_file_get (parent_file, error);
  g_object_unref (parent_file);

  return parent;
}



/**
 * thunar_file_execute:
 * @file              : a #ThunarFile instance.
 * @working_directory : the working directory used to resolve relative filenames 
 *                      in @file_list.
 * @screen            : a #GdkScreen.
 * @file_list         : the list of #GFile<!---->s to supply to @file on execution.
 * @error             : return location for errors or %NULL.
 *
 * Tries to execute @file on the specified @screen. If @file is executable
 * and could have been spawned successfully, %TRUE is returned, else %FALSE
 * will be returned and @error will be set to point to the error location.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_file_execute (ThunarFile  *file,
                     GFile       *working_directory,
                     GdkScreen   *screen,
                     GList       *file_list,
                     GError     **error)
{
  gboolean  snotify = FALSE;
  gboolean  terminal;
  gboolean  result = FALSE;
  GKeyFile *key_file;
  GError   *err = NULL;
  GFile    *parent;
  gchar    *icon = NULL;
  gchar    *name;
  gchar    *type;
  gchar    *url;
  gchar    *location;
  gchar    *escaped_location;
  gchar   **argv = NULL;
  gchar    *exec;
  gchar    *directory;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  location = thunar_g_file_get_location (file->gfile);

  if (thunar_file_is_desktop_file (file))
    {
      key_file = thunar_g_file_query_key_file (file->gfile, NULL, &err);

      if (key_file == NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Failed to parse the desktop file: %s"), err->message);
          g_error_free (err);
          return FALSE;
        }

      type = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_TYPE, NULL);

      if (G_LIKELY (exo_str_is_equal (type, "Application")))
        {
          exec = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
          if (G_LIKELY (exec != NULL))
            {
              /* parse other fields */
              name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                                   G_KEY_FILE_DESKTOP_KEY_NAME, NULL,
                                                   NULL);
              icon = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
              terminal = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                                 G_KEY_FILE_DESKTOP_KEY_TERMINAL, NULL);
              snotify = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, 
                                                NULL);

              result = thunar_exec_parse (exec, file_list, icon, name, location, 
                                          terminal, NULL, &argv, error);
              
              g_free (name);
              g_free (icon);
              g_free (exec);
            }
          else
            {
              /* TRANSLATORS: `Exec' is a field name in a .desktop file. 
               * Don't translate it. */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, 
                           _("No Exec field specified"));
            }
        }
      else if (exo_str_is_equal (type, "Link"))
        {
          url = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                       G_KEY_FILE_DESKTOP_KEY_URL, NULL);
          if (G_LIKELY (url != NULL))
            {
              /* pass the URL to exo-open which will fire up the appropriate viewer */
              argv = g_new (gchar *, 3);
              argv[0] = g_strdup ("exo-open");
              argv[1] = url;
              argv[2] = NULL;
              result = TRUE;
            }
          else
            {
              /* TRANSLATORS: `URL' is a field name in a .desktop file.
               * Don't translate it. */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, 
                           _("No URL field specified"));
            }
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, 
                       _("Invalid desktop file"));
        }

      g_free (type);
      g_key_file_free (key_file);
    }
  else
    {
      /* fake the Exec line */
      escaped_location = g_shell_quote (location);
      exec = g_strconcat (escaped_location, " %F", NULL);
      result = thunar_exec_parse (exec, file_list, NULL, NULL, NULL, FALSE, NULL, &argv,
                                  error);
      g_free (escaped_location);
      g_free (exec);
    }

  if (G_LIKELY (result))
    {
      /* determine the working directory */
      if (G_LIKELY (working_directory != NULL))
        {
          /* copy the working directory provided to this method */
          directory = g_file_get_path (working_directory);
        }
      else if (file_list != NULL)
        {
          /* use the directory of the first list item */
          parent = g_file_get_parent (file_list->data);
          directory = (parent != NULL) ? thunar_g_file_get_location (parent) : NULL;
          g_object_unref (parent);
        }
      else
        {
          /* use the directory of the executable file */
          parent = g_file_get_parent (file->gfile);
          directory = (parent != NULL) ? thunar_g_file_get_location (parent) : NULL;
          g_object_unref (parent);
        }

      /* execute the command */
      result = xfce_spawn_on_screen (screen, directory, argv, NULL, G_SPAWN_SEARCH_PATH,
                                     snotify, gtk_get_current_event_time (), icon, error);

      /* release the working directory */
      g_free (directory);
    }

  /* clean up */
  g_strfreev (argv);
  g_free (location);

  return result;
}



/**
 * thunar_file_launch:
 * @file       : a #ThunarFile instance.
 * @parent     : a #GtkWidget or a #GdkScreen on which to launch the @file.
 *               May also be %NULL in which case the default #GdkScreen will
 *               be used.
 * @startup_id : startup id for the new window (send over for dbus) or %NULL.
 * @error      : return location for errors or %NULL.
 *
 * If @file is an executable file, tries to execute it. Else if @file is
 * a directory, opens a new #ThunarWindow to display the directory. Else,
 * the default handler for @file is determined and run.
 *
 * The @parent can be either a #GtkWidget or a #GdkScreen, on which to
 * launch the @file. If @parent is a #GtkWidget, the chooser dialog (if
 * no default application is available for @file) will be transient for
 * @parent. Else if @parent is a #GdkScreen it specifies the screen on
 * which to launch @file.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_file_launch (ThunarFile  *file,
                    gpointer     parent,
                    const gchar *startup_id,
                    GError     **error)
{
  GdkAppLaunchContext *context;
  ThunarApplication   *application;
  GdkScreen           *screen;
  GAppInfo            *app_info;
  gboolean             succeed;
  GList                path_list;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), FALSE);
  
  /* determine the screen for the parent */
  if (G_UNLIKELY (parent == NULL))
    screen = gdk_screen_get_default ();
  else if (GTK_IS_WIDGET (parent))
    screen = gtk_widget_get_screen (parent);
  else
    screen = GDK_SCREEN (parent);

  /* check if we have a folder here */
  if (thunar_file_is_directory (file))
    {
      application = thunar_application_get ();
      thunar_application_open_window (application, file, screen, startup_id);
      g_object_unref (G_OBJECT (application));
      return TRUE;
    }

  /* check if we should execute the file */
  if (thunar_file_is_executable (file))
    return thunar_file_execute (file, NULL, screen, NULL, error);

  /* determine the default application to open the file */
  /* TODO We should probably add a cancellable argument to thunar_file_launch() */
  app_info = thunar_file_get_default_handler (file);

  /* display the application chooser if no application is defined for this file
   * type yet */
  if (G_UNLIKELY (app_info == NULL))
    {
      thunar_show_chooser_dialog (parent, file, TRUE);
      return TRUE;
    }

  /* HACK: check if we're not trying to launch another file manager again, possibly
   * ourselfs which will end in a loop */
  if (g_strcmp0 (g_app_info_get_id (app_info), "exo-file-manager.desktop") == 0
      || g_strcmp0 (g_app_info_get_id (app_info), "Thunar.desktop") == 0
      || g_strcmp0 (g_app_info_get_name (app_info), "exo-file-manager") == 0)
    {
      g_object_unref (G_OBJECT (app_info));
      thunar_show_chooser_dialog (parent, file, TRUE);
      return TRUE;
    }

  /* fake a path list */
  path_list.data = file->gfile;
  path_list.next = path_list.prev = NULL;

  /* create a launch context */
  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_screen (context, screen);

  /* otherwise try to execute the application */
  succeed = g_app_info_launch (app_info, &path_list, G_APP_LAUNCH_CONTEXT (context), error);

  /* destroy the launch context */
  g_object_unref (context);

  /* release the handler reference */
  g_object_unref (G_OBJECT (app_info));

  return succeed;
}



/**
 * thunar_file_rename:
 * @file  : a #ThunarFile instance.
 * @name  : the new file name in UTF-8 encoding.
 * @error : return location for errors or %NULL.
 *
 * Tries to rename @file to the new @name. If @file cannot be renamed,
 * %FALSE will be returned and @error will be set accordingly. Else, if
 * the operation succeeds, %TRUE will be returned, and @file will have
 * a new URI and a new display name.
 *
 * When offering a rename action in the user interface, the implementation
 * should first check whether the file is available, using the
 * thunar_file_is_renameable() method.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_file_rename (ThunarFile   *file,
                    const gchar  *name,
                    GCancellable *cancellable,
                    gboolean      called_from_job,
                    GError      **error)
{
  ThunarApplication    *application;
  ThunarThumbnailCache *thumbnail_cache;
  GKeyFile             *key_file;
  GError               *err = NULL;
  GFile                *previous_file;
  GFile                *renamed_file;
  gint                  watch_count;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if this file is a desktop entry */
  if (thunar_file_is_desktop_file (file))
    {
      /* try to load the desktop entry into a key file */
      key_file = thunar_g_file_query_key_file (file->gfile, cancellable, &err);
      if (key_file == NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("Failed to parse the desktop file: %s"), err->message);
          g_error_free (err);
          return FALSE;
        }

      /* change the Name field of the desktop entry */
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_NAME, name);

      /* write the changes back to the file */
      if (thunar_g_file_write_key_file (file->gfile, key_file, cancellable, &err))
        {
          /* reload file information */
          thunar_file_load (file, NULL, NULL);

          if (!called_from_job)
            {
              /* tell the associated folder that the file was renamed */
              thunarx_file_info_renamed (THUNARX_FILE_INFO (file));

              /* notify everybody that the file has changed */
              thunar_file_changed (file);
            }

          /* release the key file and return with success */
          g_key_file_free (key_file);
          return TRUE;
        }
      else
        {
          /* propagate the error message and return with failure */
          g_propagate_error (error, err);
          g_key_file_free (key_file);
          return FALSE;
        }
    }
  else
    {
      /* remember the previous file */
      previous_file = g_object_ref (file->gfile);
      
      /* try to rename the file */
      renamed_file = g_file_set_display_name (file->gfile, name, cancellable, error);

      /* notify the thumbnail cache that we can now also move the thumbnail */
      application = thunar_application_get ();
      thumbnail_cache = thunar_application_get_thumbnail_cache (application);
      thunar_thumbnail_cache_move_file (thumbnail_cache, previous_file, renamed_file);
      g_object_unref (thumbnail_cache);
      g_object_unref (application);

      /* check if we succeeded */
      if (renamed_file != NULL)
        {
          /* set the new file */
          file->gfile = renamed_file;

          /* reload file information */
          thunar_file_load (file, NULL, NULL);

          /* need to re-register the monitor handle for the new uri */
          watch_count = THUNAR_FILE_GET_WATCH_COUNT (file);
          if (G_LIKELY (watch_count > 0))
            {
              /* drop the watch_count temporary */
              THUNAR_FILE_SET_WATCH_COUNT (file, 1);

              /* drop the previous handle (with the old path) */
              thunar_file_unwatch (file);

              /* register the new handle (with the new path) */
              thunar_file_watch (file);

              /* reset the watch count */
              THUNAR_FILE_SET_WATCH_COUNT (file, watch_count);
            }

          G_LOCK (file_cache_mutex);

          /* drop the previous entry from the cache */
          g_hash_table_remove (file_cache, previous_file);

          /* drop the reference on the previous file */
          g_object_unref (previous_file);

          /* insert the new entry */
          g_hash_table_insert (file_cache, g_object_ref (file->gfile), file);

          G_UNLOCK (file_cache_mutex);

          if (!called_from_job)
            {
              /* tell the associated folder that the file was renamed */
              thunarx_file_info_renamed (THUNARX_FILE_INFO (file));

              /* emit the file changed signal */
              thunar_file_changed (file);
            }

          return TRUE;
        }
      else
        {
          g_object_unref (previous_file);

          return FALSE;
        }
    }
}



/**
 * thunar_file_accepts_drop:
 * @file                    : a #ThunarFile instance.
 * @file_list               : the list of #GFile<!---->s that will be droppped.
 * @context                 : the current #GdkDragContext, which is used for the drop.
 * @suggested_action_return : return location for the suggested #GdkDragAction or %NULL.
 *
 * Checks whether @file can accept @path_list for the given @context and
 * returns the #GdkDragAction<!---->s that can be used or 0 if no actions
 * apply.
 *
 * If any #GdkDragAction<!---->s apply and @suggested_action_return is not
 * %NULL, the suggested #GdkDragAction for this drop will be stored to the
 * location pointed to by @suggested_action_return.
 *
 * Return value: the #GdkDragAction<!---->s supported for the drop or
 *               0 if no drop is possible.
 **/
GdkDragAction
thunar_file_accepts_drop (ThunarFile     *file,
                          GList          *file_list,
                          GdkDragContext *context,
                          GdkDragAction  *suggested_action_return)
{
  GdkDragAction suggested_action;
  GdkDragAction actions;
  ThunarFile   *ofile;
  GFile        *parent_file;
  GList        *lp;
  guint         n;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), 0);
  _thunar_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  /* we can never drop an empty list */
  if (G_UNLIKELY (file_list == NULL))
    return 0;

  /* default to whatever GTK+ thinks for the suggested action */
  suggested_action = context->suggested_action;

  /* check if we have a writable directory here or an executable file */
  if (thunar_file_is_directory (file) && thunar_file_is_writable (file))
    {
      /* determine the possible actions */
      actions = context->actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

      /* cannot create symbolic links in the trash or copy to the trash */
      if (thunar_file_is_trashed (file))
        actions &= ~(GDK_ACTION_COPY | GDK_ACTION_LINK);

      /* check up to 100 of the paths (just in case somebody tries to
       * drag around his music collection with 5000 files).
       */
      for (lp = file_list, n = 0; lp != NULL && n < 100; lp = lp->next, ++n)
        {
          /* we cannot drop a file on itself */
          if (G_UNLIKELY (g_file_equal (file->gfile, lp->data)))
            return 0;

          /* check whether source and destination are the same */
          parent_file = g_file_get_parent (lp->data);
          if (G_LIKELY (parent_file != NULL))
            {
              if (g_file_equal (file->gfile, parent_file))
                {
                  g_object_unref (parent_file);
                  return 0;
                }
              else
                g_object_unref (parent_file);
            }

          /* copy/move/link within the trash not possible */
          if (G_UNLIKELY (thunar_g_file_is_trashed (lp->data) && thunar_file_is_trashed (file)))
            return 0;
        }

      /* if the source offers both copy and move and the GTK+ suggested action is copy, try to be smart telling whether
       * we should copy or move by default by checking whether the source and target are on the same disk.
       */
      if ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE)) != 0 
          && (suggested_action == GDK_ACTION_COPY))
        {
          /* default to move as suggested action */
          suggested_action = GDK_ACTION_MOVE;

          /* check for up to 100 files, for the reason state above */
          for (lp = file_list, n = 0; lp != NULL && n < 100; lp = lp->next, ++n)
            {
              /* dropping from the trash always suggests move */
              if (G_UNLIKELY (thunar_g_file_is_trashed (lp->data)))
                break;

              /* determine the cached version of the source file */
              ofile = thunar_file_cache_lookup (lp->data);

              /* we have only move if we know the source and both the source and the target
               * are on the same disk, and the source file is owned by the current user.
               */
              if (ofile == NULL 
                  || !thunar_file_same_filesystem (file, ofile)
                  || (ofile->info != NULL 
                      && g_file_info_get_attribute_uint32 (ofile->info, 
                                                           G_FILE_ATTRIBUTE_UNIX_UID) != effective_user_id))
                {
                  /* default to copy and get outa here */
                  suggested_action = GDK_ACTION_COPY;
                  break;
                }
            }
        }
    }
  else if (thunar_file_is_executable (file))
    {
      /* determine the possible actions */
      actions = context->actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_PRIVATE);
    }
  else
    return 0;

  /* determine the preferred action based on the context */
  if (G_LIKELY (suggested_action_return != NULL))
    {
      /* determine a working action */
      if (G_LIKELY ((suggested_action & actions) != 0))
        *suggested_action_return = suggested_action;
      else if ((actions & GDK_ACTION_ASK) != 0)
        *suggested_action_return = GDK_ACTION_ASK;
      else if ((actions & GDK_ACTION_COPY) != 0)
        *suggested_action_return = GDK_ACTION_COPY;
      else if ((actions & GDK_ACTION_LINK) != 0)
        *suggested_action_return = GDK_ACTION_LINK;
      else if ((actions & GDK_ACTION_MOVE) != 0)
        *suggested_action_return = GDK_ACTION_MOVE;
      else
        *suggested_action_return = GDK_ACTION_PRIVATE;
    }

  /* yeppa, we can drop here */
  return actions;
}



/**
 * thunar_file_get_date:
 * @file        : a #ThunarFile instance.
 * @date_type   : the kind of date you are interested in.
 *
 * Queries the given @date_type from @file and returns the result.
 *
 * Return value: the time for @file of the given @date_type.
 **/
guint64
thunar_file_get_date (const ThunarFile  *file,
                      ThunarFileDateType date_type)
{
  const gchar *attribute;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), 0);

  if (file->info == NULL)
    return 0;
  
  switch (date_type)
    {
    case THUNAR_FILE_DATE_ACCESSED: 
      attribute = G_FILE_ATTRIBUTE_TIME_ACCESS;
      break;
    case THUNAR_FILE_DATE_CHANGED:
      attribute = G_FILE_ATTRIBUTE_TIME_CHANGED;
      break;
    case THUNAR_FILE_DATE_MODIFIED: 
      attribute = G_FILE_ATTRIBUTE_TIME_MODIFIED;
      break;
    default:
      _thunar_assert_not_reached ();
    }

  return g_file_info_get_attribute_uint64 (file->info, attribute);
}



/**
 * thunar_file_get_date_string:
 * @file       : a #ThunarFile instance.
 * @date_type  : the kind of date you are interested to know about @file.
 * @date_style : the style used to format the date.
 *
 * Tries to determine the @date_type of @file, and if @file supports the
 * given @date_type, it'll be formatted as string and returned. The
 * caller is responsible for freeing the string using the g_free()
 * function.
 *
 * Return value: the @date_type of @file formatted as string.
 **/
gchar*
thunar_file_get_date_string (const ThunarFile  *file,
                             ThunarFileDateType date_type,
                             ThunarDateStyle    date_style)
{
  return thunar_util_humanize_file_time (thunar_file_get_date (file, date_type), date_style);
}



/**
 * thunar_file_get_mode_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the mode of @file as text. You'll need to free
 * the result using g_free() when you're done with it.
 *
 * Return value: the mode of @file as string.
 **/
gchar*
thunar_file_get_mode_string (const ThunarFile *file)
{
  ThunarFileMode mode;
  GFileType      kind;
  gchar         *text;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  kind = thunar_file_get_kind (file);
  mode = thunar_file_get_mode (file);
  text = g_new (gchar, 11);

  /* file type */
  /* TODO earlier versions of Thunar had 'P' for ports and
   * 'D' for doors. Do we still need those? */
  switch (kind)
    {
    case G_FILE_TYPE_SYMBOLIC_LINK: text[0] = 'l'; break;
    case G_FILE_TYPE_REGULAR:       text[0] = '-'; break;
    case G_FILE_TYPE_DIRECTORY:     text[0] = 'd'; break;
    case G_FILE_TYPE_SPECIAL:
    case G_FILE_TYPE_UNKNOWN:
    default:
      if (S_ISCHR (mode))
        text[0] = 'c';
      else if (S_ISSOCK (mode))
        text[0] = 's';
      else if (S_ISFIFO (mode))
        text[0] = 'f';
      else if (S_ISBLK (mode))
        text[0] = 'b';
      else
        text[0] = ' ';
    }

  /* permission flags */
  text[1] = (mode & THUNAR_FILE_MODE_USR_READ)  ? 'r' : '-';
  text[2] = (mode & THUNAR_FILE_MODE_USR_WRITE) ? 'w' : '-';
  text[3] = (mode & THUNAR_FILE_MODE_USR_EXEC)  ? 'x' : '-';
  text[4] = (mode & THUNAR_FILE_MODE_GRP_READ)  ? 'r' : '-';
  text[5] = (mode & THUNAR_FILE_MODE_GRP_WRITE) ? 'w' : '-';
  text[6] = (mode & THUNAR_FILE_MODE_GRP_EXEC)  ? 'x' : '-';
  text[7] = (mode & THUNAR_FILE_MODE_OTH_READ)  ? 'r' : '-';
  text[8] = (mode & THUNAR_FILE_MODE_OTH_WRITE) ? 'w' : '-';
  text[9] = (mode & THUNAR_FILE_MODE_OTH_EXEC)  ? 'x' : '-';

  /* special flags */
  if (G_UNLIKELY (mode & THUNAR_FILE_MODE_SUID))
    text[3] = 's';
  if (G_UNLIKELY (mode & THUNAR_FILE_MODE_SGID))
    text[6] = 's';
  if (G_UNLIKELY (mode & THUNAR_FILE_MODE_STICKY))
    text[9] = 't';

  text[10] = '\0';

  return text;
}



/**
 * thunar_file_get_size_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the size of the file as text in a human readable
 * format. You'll need to free the result using g_free()
 * if you're done with it.
 *
 * Return value: the size of @file in a human readable
 *               format.
 **/
gchar *
thunar_file_get_size_string (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return g_format_size_for_display (thunar_file_get_size (file));
}



/**
 * thunar_file_get_volume:
 * @file           : a #ThunarFile instance.
 *
 * Attempts to determine the #GVolume on which @file is located. If @file cannot 
 * determine it's volume, then %NULL will be returned. Else a #GVolume instance 
 * is returned which has to be released by the caller using g_object_unref().
 *
 * Return value: the #GVolume for @file or %NULL.
 **/
GVolume*
thunar_file_get_volume (const ThunarFile *file)
{
  GVolume *volume = NULL;
  GMount  *mount;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* TODO make this function call asynchronous */
  mount = g_file_find_enclosing_mount (file->gfile, NULL, NULL);
  if (mount != NULL)
    {
      volume = g_mount_get_volume (mount);
      g_object_unref (mount);
    }

  return volume;
}



/**
 * thunar_file_get_group:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarGroup for @file. If there's no
 * group associated with @file or if the system is unable to
 * determine the group, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarGroup for @file or %NULL.
 **/
ThunarGroup *
thunar_file_get_group (const ThunarFile *file)
{
  guint32 gid;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* TODO what are we going to do on non-UNIX systems? */
  gid = g_file_info_get_attribute_uint32 (file->info,
                                          G_FILE_ATTRIBUTE_UNIX_GID);

  return thunar_user_manager_get_group_by_id (user_manager, gid);
}



/**
 * thunar_file_get_user:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarUser for @file. If there's no
 * user associated with @file or if the system is unable
 * to determine the user, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarUser for @file or %NULL.
 **/
ThunarUser*
thunar_file_get_user (const ThunarFile *file)
{
  guint32 uid;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* TODO what are we going to do on non-UNIX systems? */
  uid = g_file_info_get_attribute_uint32 (file->info,
                                          G_FILE_ATTRIBUTE_UNIX_UID);

  return thunar_user_manager_get_user_by_id (user_manager, uid);
}



/**
 * thunar_file_get_content_type:
 * @file : a #ThunarFile.
 *
 * Returns the content type of @file.
 *
 * Return value: content type of @file.
 **/
const gchar *
thunar_file_get_content_type (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (file->info == NULL)
    return NULL;

  return g_file_info_get_content_type (file->info);
}



/**
 * thunar_file_get_symlink_target:
 * @file : a #ThunarFile.
 *
 * Returns the path of the symlink target or %NULL if the @file
 * is not a symlink.
 *
 * Return value: path of the symlink target or %NULL.
 **/
const gchar *
thunar_file_get_symlink_target (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (file->info == NULL)
    return NULL;

  return g_file_info_get_symlink_target (file->info);
}



/**
 * thunar_file_get_basename:
 * @file : a #ThunarFile.
 *
 * Returns the basename of the @file in UTF-8 encoding.
 *
 * Return value: UTF-8 encoded basename of the @file.
 **/
const gchar *
thunar_file_get_basename (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->basename;
}



/**
 * thunar_file_is_symlink:
 * @file : a #ThunarFile.
 *
 * Returns %TRUE if @file is a symbolic link.
 *
 * Return value: %TRUE if @file is a symbolic link.
 **/
gboolean 
thunar_file_is_symlink (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  return g_file_info_get_is_symlink (file->info);
}



/**
 * thunar_file_get_size:
 * @file : a #ThunarFile instance.
 *
 * Tries to determine the size of @file in bytes and
 * returns the size.
 *
 * Return value: the size of @file in bytes.
 **/
guint64
thunar_file_get_size (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), 0);

  if (file->info == NULL)
    return 0;

  return g_file_info_get_size (file->info);
}



/**
 * thunar_file_get_default_handler:
 * @file : a #ThunarFile instance.
 *
 * Returns the default #GAppInfo for @file or %NULL if there is none.
 * 
 * The caller is responsible to free the returned #GAppInfo using
 * g_object_unref().
 *
 * Return value: Default #GAppInfo for @file or %NULL if there is none.
 **/
GAppInfo *
thunar_file_get_default_handler (const ThunarFile *file) 
{
  const gchar *content_type;
  GAppInfo    *app_info = NULL;
  gboolean     must_support_uris = FALSE;
  gchar       *path;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  content_type = thunar_file_get_content_type (file);
  if (content_type != NULL)
    {
      path = g_file_get_path (file->gfile);
      must_support_uris = (path == NULL);
      g_free (path);

      app_info = g_app_info_get_default_for_type (content_type, must_support_uris);
    }

  if (app_info == NULL)
    app_info = g_file_query_default_handler (file->gfile, NULL, NULL);

  return app_info;
}



/**
 * thunar_file_get_kind:
 * @file : a #ThunarFile instance.
 *
 * Returns the kind of @file.
 *
 * Return value: the kind of @file.
 **/
GFileType
thunar_file_get_kind (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), G_FILE_TYPE_UNKNOWN);

  if (file->info == NULL)
    return G_FILE_TYPE_UNKNOWN;

  return g_file_info_get_file_type (file->info);
}



GFile *
thunar_file_get_target_location (const ThunarFile *file)
{
  const gchar *uri;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (file->info == NULL)
    return g_object_ref (file->gfile);
  
  uri = g_file_info_get_attribute_string (file->info,
                                          G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);

  return (uri != NULL) ? g_file_new_for_uri (uri) : NULL;
}



/**
 * thunar_file_get_mode:
 * @file : a #ThunarFile instance.
 *
 * Returns the permission bits of @file.
 *
 * Return value: the permission bits of @file.
 **/
ThunarFileMode
thunar_file_get_mode (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), 0);

  if (file->info == NULL)
    return 0;

  if (g_file_info_has_attribute (file->info, G_FILE_ATTRIBUTE_UNIX_MODE))
    return g_file_info_get_attribute_uint32 (file->info, G_FILE_ATTRIBUTE_UNIX_MODE);
  else
    return thunar_file_is_directory (file) ? 0777 : 0666;
}



/**
 * thunar_file_get_free_space:
 * @file              : a #ThunarFile instance.
 * @free_space_return : return location for the amount of
 *                      free space or %NULL.
 *
 * Determines the amount of free space of the volume on
 * which @file resides. Returns %TRUE if the amount of
 * free space was determined successfully and placed into
 * @free_space_return, else %FALSE will be returned.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
thunar_file_get_free_space (const ThunarFile *file, 
                            guint64          *free_space_return) 
{
  GFileInfo *filesystem_info;
  gboolean   success = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  filesystem_info = g_file_query_filesystem_info (file->gfile, 
                                                  THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                                  NULL, NULL);

  if (filesystem_info != NULL)
    {
      *free_space_return = 
        g_file_info_get_attribute_uint64 (filesystem_info,
                                          G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

      success = g_file_info_has_attribute (filesystem_info, 
                                           G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

      g_object_unref (filesystem_info);
    }

  return success;
}



gboolean
thunar_file_is_mounted (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return file->is_mounted;
}



gboolean
thunar_file_exists (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return g_file_query_exists (file->gfile, NULL);
}



/**
 * thunar_file_is_directory:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file refers to a directory.
 *
 * Return value: %TRUE if @file is a directory.
 **/
gboolean
thunar_file_is_directory (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  return thunar_file_get_kind (file) == G_FILE_TYPE_DIRECTORY;
}



/**
 * thunar_file_is_shortcut:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file refers to a shortcut to something else.
 *
 * Return value: %TRUE if @file is a shortcut.
 **/
gboolean
thunar_file_is_shortcut (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  return thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT;
}



/**
 * thunar_file_is_mountable:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file refers to a mountable file/directory.
 *
 * Return value: %TRUE if @file is a mountable file/directory.
 **/
gboolean
thunar_file_is_mountable (const ThunarFile *file) 
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  return thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE;
}



/**
 * thunar_file_is_local:
 * @file : a #ThunarFile instance.
 *
 * Returns %TRUE if @file is a local file with the
 * file:// URI scheme.
 *
 * Return value: %TRUE if @file is local.
 **/
gboolean
thunar_file_is_local (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file->gfile, "file");
}



/**
 * thunar_file_is_parent:
 * @file  : a #ThunarFile instance.
 * @child : another #ThunarFile instance.
 *
 * Determines whether @file is the parent directory of @child.
 *
 * Return value: %TRUE if @file is the parent of @child.
 **/
gboolean
thunar_file_is_parent (const ThunarFile *file,
                       const ThunarFile *child)
{
  gboolean is_parent = FALSE;
  GFile   *parent;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (child), FALSE);

  parent = g_file_get_parent (child->gfile);
  if (parent != NULL)
    {
      is_parent = g_file_equal (file->gfile, parent);
      g_object_unref (parent);
    }

  return is_parent;
}



/**
 * thunar_file_is_ancestor:
 * @file     : a #ThunarFile instance.
 * @ancestor : another #ThunarFile instance.
 *
 * Determines whether @file is somewhere inside @ancestor,
 * possibly with intermediate folders.
 *
 * Return value: %TRUE if @ancestor contains @file as a
 *               child, grandchild, great grandchild, etc.
 **/
gboolean
thunar_file_is_ancestor (const ThunarFile *file, 
                         const ThunarFile *ancestor)
{
  gboolean is_ancestor = FALSE;
  GFile   *current = NULL;
  GFile   *tmp;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (ancestor), FALSE);

  for (current = g_object_ref (file->gfile);
       is_ancestor == FALSE && current != NULL;
       tmp = g_file_get_parent (current), g_object_unref (current), current = tmp)
    {
      if (G_UNLIKELY (g_file_equal (current, ancestor->gfile)))
        is_ancestor = TRUE;
    }

  if (current != NULL)
    g_object_unref (current);

  return is_ancestor;
}



/**
 * thunar_file_is_executable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to execute the @file (or enter the directory refered to by
 * @file). On UNIX it also returns %TRUE if @file refers to a 
 * desktop entry.
 *
 * Return value: %TRUE if @file can be executed.
 **/
gboolean
thunar_file_is_executable (const ThunarFile *file)
{
  gboolean     can_execute = FALSE;
  const gchar *content_type;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  if (g_file_info_get_attribute_boolean (file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    {
      /* get the content type of the file */
      content_type = g_file_info_get_content_type (file->info);
      if (G_LIKELY (content_type != NULL))
        {
#ifdef G_OS_WIN32
          /* check for .exe, .bar or .com */
          can_execute = g_content_type_can_be_executable (content_type);
#else
          /* check if the content type is save to execute, we don't use
           * g_content_type_can_be_executable() for unix because it also returns
           * true for "text/plain" and we don't want that */
          if (g_content_type_is_a (content_type, "application/x-executable")
              || g_content_type_is_a (content_type, "application/x-shellscript"))
            {
              can_execute = TRUE;
            }
#endif
        }
    }

  return can_execute || thunar_file_is_desktop_file (file);
}



/**
 * thunar_file_is_readable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to read the @file.
 *
 * Return value: %TRUE if @file can be read.
 **/
gboolean
thunar_file_is_readable (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  if (!g_file_info_has_attribute (file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    return TRUE;
      
  return g_file_info_get_attribute_boolean (file->info, 
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
}



/**
 * thunar_file_is_writable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to write the @file.
 *
 * Return value: %TRUE if @file can be read.
 **/
gboolean
thunar_file_is_writable (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  
  if (file->info == NULL)
    return FALSE;

  if (!g_file_info_has_attribute (file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    return TRUE;

  return g_file_info_get_attribute_boolean (file->info, 
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
}



/**
 * thunar_file_is_hidden:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file can be considered a hidden file.
 *
 * Return value: %TRUE if @file is a hidden file, else %FALSE.
 **/
gboolean
thunar_file_is_hidden (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  
  if (file->info == NULL)
    return FALSE;

  return g_file_info_get_is_hidden (file->info);
}



/**
 * thunar_file_is_home:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the users home directory.
 *
 * Return value: %TRUE if @file is the users home directory.
 **/
gboolean
thunar_file_is_home (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return thunar_g_file_is_home (file->gfile);
}



/**
 * thunar_file_is_regular:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to a regular file.
 *
 * Return value: %TRUE if @file is a regular file.
 **/
gboolean
thunar_file_is_regular (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return thunar_file_get_kind (file) == G_FILE_TYPE_REGULAR;
}



/**
 * thunar_file_is_trashed:
 * @file : a #ThunarFile instance.
 *
 * Returns %TRUE if @file is a local file that resides in 
 * the trash bin.
 *
 * Return value: %TRUE if @file is in the trash, or
 *               the trash folder itself.
 **/
gboolean
thunar_file_is_trashed (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return thunar_g_file_is_trashed (file->gfile);
}



/**
 * thunar_file_is_desktop_file:
 * @file : a #ThunarFile.
 *
 * Returns %TRUE if @file is a .desktop file, but not a .directory file.
 *
 * Return value: %TRUE if @file is a .desktop file.
 **/
gboolean
thunar_file_is_desktop_file (const ThunarFile *file)
{
  const gchar *content_type;
  gboolean     is_desktop_file = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  content_type = g_file_info_get_content_type (file->info);

  if (content_type != NULL)
    is_desktop_file = g_content_type_equals (content_type, "application/x-desktop");

  return is_desktop_file 
    && !g_str_has_suffix (thunar_file_get_basename (file), ".directory");
}



/**
 * thunar_file_get_display_name:
 * @file : a #ThunarFile instance.
 *
 * Returns the @file name in the UTF-8 encoding, which is
 * suitable for displaying the file name in the GUI.
 *
 * Return value: the @file name suitable for display.
 **/
const gchar *
thunar_file_get_display_name (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return file->display_name;
}



/**
 * thunar_file_get_deletion_date:
 * @file       : a #ThunarFile instance.
 * @date_style : the style used to format the date.
 *
 * Returns the deletion date of the @file if the @file
 * is located in the trash. Otherwise %NULL will be
 * returned.
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the deletion date of @file if @file is
 *               in the trash, %NULL otherwise.
 **/
gchar*
thunar_file_get_deletion_date (const ThunarFile *file,
                               ThunarDateStyle   date_style)
{
  const gchar *date;
  time_t       deletion_time;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (G_IS_FILE_INFO (file->info), NULL);

  date = g_file_info_get_attribute_string (file->info, "trash::deletion-date");
  if (G_UNLIKELY (date == NULL))
    return NULL;

  /* try to parse the DeletionDate (RFC 3339 string) */
  deletion_time = thunar_util_time_from_rfc3339 (date);

  /* humanize the time value */
  return thunar_util_humanize_file_time (deletion_time, date_style);
}



/**
 * thunar_file_get_original_path:
 * @file : a #ThunarFile instance.
 *
 * Returns the original path of the @file if the @file
 * is located in the trash. Otherwise %NULL will be
 * returned.
 *
 * Return value: the original path of @file if @file is
 *               in the trash, %NULL otherwise.
 **/
const gchar *
thunar_file_get_original_path (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (file->info == NULL)
    return NULL;

  return g_file_info_get_attribute_byte_string (file->info, "trash::orig-path");
}



/**
 * thunar_file_get_item_count:
 * @file : a #ThunarFile instance.
 *
 * Returns the number of items in the trash, if @file refers to the
 * trash root directory. Otherwise returns 0.
 *
 * Return value: number of files in the trash if @file is the trash 
 *               root dir, 0 otherwise.
 **/
guint32
thunar_file_get_item_count (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), 0);

  if (file->info == NULL)
    return 0;

  return g_file_info_get_attribute_uint32 (file->info, 
                                           G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
}



/**
 * thunar_file_is_chmodable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to changed the file mode of @file.
 *
 * Return value: %TRUE if the mode of @file can be changed.
 **/
gboolean
thunar_file_is_chmodable (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* we can only change the mode if we the euid is
   *   a) equal to the file owner id
   * or
   *   b) the super-user id
   * and the file is not in the trash.
   */
  if (file->info == NULL)
    {
      return (effective_user_id == 0 && !thunar_file_is_trashed (file));
    }
  else
    {
      return ((effective_user_id == 0 
               || effective_user_id == g_file_info_get_attribute_uint32 (file->info,
                                                                         G_FILE_ATTRIBUTE_UNIX_UID))
              && !thunar_file_is_trashed (file));
    }
}



/**
 * thunar_file_is_renameable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether @file can be renamed using
 * #thunar_file_rename(). Note that the return
 * value is just a guess and #thunar_file_rename()
 * may fail even if this method returns %TRUE.
 *
 * Return value: %TRUE if @file can be renamed.
 **/
gboolean
thunar_file_is_renameable (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  
  if (file->info == NULL)
    return FALSE;

  return g_file_info_get_attribute_boolean (file->info,
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
}



gboolean
thunar_file_can_be_trashed (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  if (file->info == NULL)
    return FALSE;

  return g_file_info_get_attribute_boolean (file->info, 
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
}



/**
 * thunar_file_get_emblem_names:
 * @file : a #ThunarFile instance.
 *
 * Determines the names of the emblems that should be displayed for
 * @file. The returned list is owned by the caller, but the list
 * items - the name strings - are owned by @file. So the caller
 * must call g_list_free(), but don't g_free() the list items.
 *
 * Note that the strings contained in the returned list are
 * not garantied to exist over the next iteration of the main
 * loop. So in case you need the list of emblem names for
 * a longer time, you'll need to take a copy of the strings.
 *
 * Return value: the names of the emblems for @file.
 **/
GList*
thunar_file_get_emblem_names (ThunarFile *file)
{
  const gchar *emblem_string;
  guint32      uid;
  gchar      **emblem_names;
  GList       *emblems = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* check if we need to load the emblems_list from the metafile */
  if (G_UNLIKELY ((file->flags & THUNAR_FILE_OWNS_EMBLEM_NAMES) == 0))
    {
      emblem_string = thunar_file_get_metadata (file, THUNAR_METAFILE_KEY_EMBLEMS, "");
      if (G_UNLIKELY (*emblem_string != '\0'))
        {
          emblem_names = g_strsplit (emblem_string, ";", -1);
          g_object_set_qdata_full (G_OBJECT (file), thunar_file_emblem_names_quark,
                                   emblem_names, (GDestroyNotify) g_strfreev);
        }
      file->flags |= THUNAR_FILE_OWNS_EMBLEM_NAMES;
    }

  /* determine the custom emblems */
  emblem_names = g_object_get_qdata (G_OBJECT (file), thunar_file_emblem_names_quark);
  if (G_UNLIKELY (emblem_names != NULL))
    {
      for (; *emblem_names != NULL; ++emblem_names)
        emblems = g_list_append (emblems, *emblem_names);
    }

  if (thunar_file_is_symlink (file))
    emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK);

  /* determine the user ID of the file owner */
  /* TODO what are we going to do here on non-UNIX systems? */
  uid = file->info != NULL 
        ? g_file_info_get_attribute_uint32 (file->info, G_FILE_ATTRIBUTE_UNIX_UID) 
        : 0;

  /* we add "cant-read" if either (a) the file is not readable or (b) a directory, that lacks the
   * x-bit, see http://bugzilla.xfce.org/show_bug.cgi?id=1408 for the details about this change.
   */
  if (!thunar_file_is_readable (file)
      || (thunar_file_is_directory (file)
          && thunar_file_denies_access_permission (file, THUNAR_FILE_MODE_USR_EXEC,
                                                         THUNAR_FILE_MODE_GRP_EXEC,
                                                         THUNAR_FILE_MODE_OTH_EXEC)))
    {
      emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_CANT_READ);
    }
  else if (G_UNLIKELY (uid == effective_user_id && !thunar_file_is_writable (file)))
    {
      /* we own the file, but we cannot write to it, that's why we mark it as "cant-write", so
       * users won't be surprised when opening the file in a text editor, but are unable to save.
       */
      emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_CANT_WRITE);
    }

  return emblems;
}



/**
 * thunar_file_set_emblem_names:
 * @file         : a #ThunarFile instance.
 * @emblem_names : a #GList of emblem names.
 *
 * Sets the custom emblem name list of @file to @emblem_names
 * and stores them in the @file<!---->s metadata.
 **/
void
thunar_file_set_emblem_names (ThunarFile *file,
                              GList      *emblem_names)
{
  GList  *lp;
  gchar **emblems;
  gchar  *emblems_string;
  gint    n;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* allocate a zero-terminated array for the emblem names */
  emblems = g_new0 (gchar *, g_list_length (emblem_names) + 1);

  /* turn the emblem_names list into a zero terminated array */
  for (lp = emblem_names, n = 0; lp != NULL; lp = lp->next)
    {
      /* skip special emblems */
      if (strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK) == 0
          || strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_READ) == 0
          || strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_WRITE) == 0
          || strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_DESKTOP) == 0)
        continue;

      /* add the emblem to our list */
      emblems[n++] = g_strdup (lp->data);
    }

  /* associate the emblems with the file */
  file->flags |= THUNAR_FILE_OWNS_EMBLEM_NAMES;
  g_object_set_qdata_full (G_OBJECT (file), thunar_file_emblem_names_quark,
                           emblems, (GDestroyNotify) g_strfreev);

  /* store the emblem list in the file's metadata */
  emblems_string = g_strjoinv (";", emblems);
  thunar_file_set_metadata (file, THUNAR_METAFILE_KEY_EMBLEMS, emblems_string, "");
  g_free (emblems_string);

  /* tell everybody that we have changed */
  thunar_file_changed (file);
}



/**
 * thunar_file_set_custom_icon:
 * @file        : a #ThunarFile instance.
 * @custom_icon : the new custom icon for the @file.
 * @error       : return location for errors or %NULL.
 *
 * Tries to change the custom icon of the .desktop file referred
 * to by @file. If that fails, %FALSE is returned and the
 * @error is set accordingly.
 *
 * Return value: %TRUE if the icon of @file was changed, %FALSE otherwise.
 **/
gboolean
thunar_file_set_custom_icon (ThunarFile  *file,
                             const gchar *custom_icon,
                             GError     **error)
{
  GKeyFile *key_file;

  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (custom_icon != NULL, FALSE);

  key_file = thunar_g_file_query_key_file (file->gfile, NULL, error);

  if (key_file == NULL)
    return FALSE;

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_ICON, custom_icon);

  if (thunar_g_file_write_key_file (file->gfile, key_file, NULL, error))
    {
      /* tell everybody that we have changed */
      thunar_file_changed (file);

      g_key_file_free (key_file);
      return TRUE;
    }
  else
    {
      g_key_file_free (key_file);
      return FALSE;
    }
}


/**
 * thunar_file_is_desktop:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to the users desktop directory.
 *
 * Return value: %TRUE if @file is the users desktop directory.
 **/
gboolean
thunar_file_is_desktop (const ThunarFile *file)
{
  GFile   *desktop;
  gboolean is_desktop = FALSE;

  desktop = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
  is_desktop = g_file_equal (file->gfile, desktop);
  g_object_unref (desktop);

  return is_desktop;
}



const gchar *
thunar_file_get_thumbnail_path (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->thumbnail_path;
}



gboolean
thunar_file_is_thumbnail (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return file->is_thumbnail;
}



/**
 * thunar_file_set_thumb_state:
 * @file        : a #ThunarFile.
 * @thumb_state : the new #ThunarFileThumbState.
 *
 * Sets the #ThunarFileThumbState for @file to @thumb_state. 
 * This will cause a "file-changed" signal to be emitted from
 * #ThunarFileMonitor. 
 **/ 
void
thunar_file_set_thumb_state (ThunarFile          *file, 
                             ThunarFileThumbState state)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* set the new thumbnail state */
  file->flags = (file->flags & ~THUNAR_FILE_THUMB_STATE_MASK) | (state);
  
  /* notify others of this change, so that all components can update
   * their file information */
  thunar_file_monitor_file_changed (file);
}



/**
 * thunar_file_get_custom_icon:
 * @file : a #ThunarFile instance.
 *
 * Queries the custom icon from @file if any, else %NULL is returned. 
 * The custom icon can be either a themed icon name or an absolute path
 * to an icon file in the local file system.
 *
 * Return value: the custom icon for @file or %NULL.
 **/
gchar *
thunar_file_get_custom_icon (const ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return g_strdup (file->custom_icon_name);
}



/**
 * thunar_file_get_preview_icon:
 * @file : a #ThunarFile instance.
 *
 * Returns the preview icon for @file if any, else %NULL is returned.
 *
 * Return value: the custom icon for @file or %NULL, the GIcon is owner
 * by the file, so do not unref it.
 **/
GIcon *
thunar_file_get_preview_icon (const ThunarFile *file)
{
  GObject *icon;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (G_IS_FILE_INFO (file->info), NULL);

  icon = g_file_info_get_attribute_object (file->info, "preview::icon");
  if (G_LIKELY (icon != NULL))
    return G_ICON (icon);

  return NULL;
}



/**
 * thunar_file_get_icon_name:
 * @file       : a #ThunarFile instance.
 * @icon_state : the state of the @file<!---->s icon we are interested in.
 * @icon_theme : the #GtkIconTheme on which to lookup up the icon name.
 *
 * Returns the name of the icon that can be used to present @file, based
 * on the given @icon_state and @icon_theme. The returned string has to
 * be freed using g_free().
 *
 * Return value: the icon name for @file in @icon_theme.
 **/
gchar *
thunar_file_get_icon_name (const ThunarFile   *file,
                           ThunarFileIconState icon_state,
                           GtkIconTheme       *icon_theme)
{
  GFile  *icon_file;
  GIcon  *icon;
  gchar **themed_icon_names;
  gchar  *icon_name = NULL;
  gint    i;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* the system root folder has a special icon */
  if (thunar_file_is_root (file) 
      && thunar_file_is_local (file)
      && thunar_file_is_directory (file))
    {
      return g_strdup ("drive-harddisk");
    }

  if (file->info == NULL)
    return NULL;

  icon = g_file_info_get_icon (file->info);

  if (icon != NULL)
    {
      if (G_IS_THEMED_ICON (icon))
        {
          g_object_get (icon, "names", &themed_icon_names, NULL);

          for (i = 0; icon_name == NULL && themed_icon_names[i] != NULL; ++i)
            if (gtk_icon_theme_has_icon (icon_theme, themed_icon_names[i]))
              icon_name = g_strdup (themed_icon_names[i]);

          g_strfreev (themed_icon_names);
        }
      else if (G_IS_FILE_ICON (icon))
        {
          icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
          icon_name = g_file_get_path (icon_file);
          g_object_unref (icon_file);
        }
    }
  
  if (icon_name == NULL)
    {
      /* try to be smart when determining icons for executable files
       * in that we use the name of the file as icon name (which will
       * work for quite a lot of binaries, e.g. 'Terminal', 'mousepad',
       * 'Thunar', 'xfmedia', etc.).
       */
      if (G_UNLIKELY (thunar_file_is_executable (file)))
        {
          icon_name = g_file_get_basename (file->gfile);
          if (G_LIKELY (!gtk_icon_theme_has_icon (icon_theme, icon_name)))
            {
              g_free (icon_name);
              icon_name = NULL;
            }
        }
    }

  /* check if we have an accept icon for the icon we found */
  if (icon_name != NULL 
      && (g_str_equal (icon_name, "inode-directory") 
          || g_str_equal (icon_name, "folder")))
    {
      if (icon_state == THUNAR_FILE_ICON_STATE_DROP)
        {
          g_free (icon_name);
          icon_name = g_strdup ("folder-drag-accept");
        }
      else if (icon_state == THUNAR_FILE_ICON_STATE_OPEN)
        {
          g_free (icon_name);
          icon_name = g_strdup ("folder-open");
        }
    }

  return icon_name;
}



/**
 * thunar_file_get_metadata:
 * @file          : a #ThunarFile instance.
 * @key           : a #ThunarMetaFileKey.
 * @default_value : the default value for @key in @file
 *                  which is returned when @key isn't
 *                  explicitly set for @file (may be
 *                  %NULL).
 *
 * Returns the metadata available for @key in @file.
 *
 * The returned string is owned by the @file and uses
 * an internal buffer that will be overridden on the
 * next call to any of the metadata retrieval methods.
 *
 * Return value: the metadata available for @key in @file
 *               or @default_value if @key is not set for
 *               @file.
 **/
const gchar*
thunar_file_get_metadata (ThunarFile       *file,
                          ThunarMetafileKey key,
                          const gchar      *default_value)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (key < THUNAR_METAFILE_N_KEYS, NULL);

  return thunar_metafile_fetch (thunar_file_get_metafile (file),
                                file->gfile, key,
                                default_value);
}



/**
 * thunar_file_set_metadata:
 * @file          : a #ThunarFile instance.
 * @key           : a #ThunarMetafileKey.
 * @value         : the new value for @key on @file.
 * @default_value : the default for @key on @file.
 *
 * Sets the metadata available for @key in @file to
 * the given @value.
 **/
void
thunar_file_set_metadata (ThunarFile       *file,
                          ThunarMetafileKey key,
                          const gchar      *value,
                          const gchar      *default_value)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (key < THUNAR_METAFILE_N_KEYS);
  _thunar_return_if_fail (default_value != NULL);
  _thunar_return_if_fail (value != NULL);

  thunar_metafile_store (thunar_file_get_metafile (file),
                         file->gfile, key, value,
                         default_value);
}



/**
 * thunar_file_watch:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to watch itself for changes. Not all #ThunarFile
 * implementations must support this, but if a #ThunarFile
 * implementation implements the thunar_file_watch() method,
 * it must also implement the thunar_file_unwatch() method.
 *
 * The #ThunarFile base class implements automatic "ref
 * counting" for watches, that says, you can call thunar_file_watch()
 * multiple times, but the virtual method will be invoked only
 * once. This also means that you MUST call thunar_file_unwatch()
 * for every thunar_file_watch() invokation, else the application
 * will abort.
 **/
void
thunar_file_watch (ThunarFile *file)
{
  gint watch_count;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_FILE_GET_WATCH_COUNT (file) >= 0);

  watch_count = THUNAR_FILE_GET_WATCH_COUNT (file);

  if (++watch_count == 1)
    {
      /* create a file or directory monitor */
      file->monitor = g_file_monitor (file->gfile, G_FILE_MONITOR_WATCH_MOUNTS, NULL, NULL);
      if (G_LIKELY (file->monitor != NULL))
        {
          /* make sure the pointer is set to NULL once the monitor is destroyed */
          g_object_add_weak_pointer (G_OBJECT (file->monitor), (gpointer) &(file->monitor));

          /* watch monitor for file changes */
          g_signal_connect (file->monitor, "changed", G_CALLBACK (thunar_file_monitor), file);
        }
    }

  THUNAR_FILE_SET_WATCH_COUNT (file, watch_count);
}



/**
 * thunar_file_unwatch:
 * @file : a #ThunarFile instance.
 *
 * See thunar_file_watch() for a description of how watching
 * #ThunarFile<!---->s works.
 **/
void
thunar_file_unwatch (ThunarFile *file)
{
  gint watch_count;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_FILE_GET_WATCH_COUNT (file) > 0);

  watch_count = THUNAR_FILE_GET_WATCH_COUNT (file);

  if (--watch_count == 0)
    {
      if (G_LIKELY (file->monitor != NULL))
        {
          /* cancel monitoring */
          g_file_monitor_cancel (file->monitor);

          /* destroy the monitor */
          g_object_unref (file->monitor);
        }
    }

  THUNAR_FILE_SET_WATCH_COUNT (file, watch_count);
}



/**
 * thunar_file_reload:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to reload its internal state, e.g. by reacquiring
 * the file info from the underlying media.
 *
 * You must be able to handle the case that @file is
 * destroyed during the reload call.
 **/
void
thunar_file_reload (ThunarFile *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (!thunar_file_load (file, NULL, NULL))
    {
      /* destroy the file if we cannot query any file information */
      thunar_file_destroy (file);
      return;
    }

  /* ... and tell others */
  thunar_file_changed (file);
  
}


 
/**
 * thunar_file_destroy:
 * @file : a #ThunarFile instance.
 *
 * Emits the ::destroy signal notifying all reference holders
 * that they should release their references to the @file.
 *
 * This method is very similar to what gtk_object_destroy()
 * does for #GtkObject<!---->s.
 **/
void
thunar_file_destroy (ThunarFile *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (G_LIKELY ((file->flags & THUNAR_FILE_IN_DESTRUCTION) == 0))
    {
      /* take an additional reference on the file, as the file-destroyed
       * invocation may already release the last reference.
       */
      g_object_ref (G_OBJECT (file));

      /* tell the file monitor that this file was destroyed */
      thunar_file_monitor_file_destroyed (file);

      /* run the dispose handler */
      g_object_run_dispose (G_OBJECT (file));

      /* release our reference */
      g_object_unref (G_OBJECT (file));
    }
}



static gint
compare_by_name_using_number (const gchar *ap,
                              const gchar *bp)
{
  guint anum;
  guint bnum;

  /* determine the numbers in ap and bp */
  anum = strtoul (ap, NULL, 10);
  bnum = strtoul (bp, NULL, 10);

  /* compare the numbers */
  if (anum < bnum)
    return -1;
  else if (anum > bnum)
    return 1;

  /* the numbers are equal, and so the higher first digit should
   * be sorted first, i.e. 'file10' before 'file010', since we
   * also sort 'file10' before 'file011'.
   */
  return (*bp - *ap);
}



/**
 * thunar_file_compare_by_name:
 * @file_a         : the first #ThunarFile.
 * @file_b         : the second #ThunarFile.
 * @case_sensitive : whether the comparison should be case-sensitive.
 *
 * Compares @file_a and @file_b by their display names. If @case_sensitive
 * is %TRUE the comparison will be case-sensitive.
 *
 * Return value: -1 if @file_a should be sorted before @file_b, 1 if
 *               @file_b should be sorted before @file_a, 0 if equal.
 **/
gint
thunar_file_compare_by_name (const ThunarFile *file_a,
                             const ThunarFile *file_b,
                             gboolean          case_sensitive)
{
  const gchar *ap;
  const gchar *bp;
  guint        ac;
  guint        bc;

#ifdef G_ENABLE_DEBUG
  /* probably too expensive to do the instance check every time
   * this function is called, so only for debugging builds.
   */
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_a), 0);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_b), 0);
#endif

  /* we compare only the display names (UTF-8!) */
  ap = thunar_file_get_display_name (file_a);
  bp = thunar_file_get_display_name (file_b);

  /* check if we should ignore case */
  if (G_LIKELY (case_sensitive))
    {
      /* try simple (fast) ASCII comparison first */
      for (;; ++ap, ++bp)
        {
          /* check if the characters differ or we have a non-ASCII char */
          ac = *((const guchar *) ap);
          bc = *((const guchar *) bp);
          if (ac != bc || ac == 0 || ac > 127)
            break;
        }

      /* fallback to Unicode comparison */
      if (G_UNLIKELY (ac > 127 || bc > 127))
        {
          for (;; ap = g_utf8_next_char (ap), bp = g_utf8_next_char (bp))
            {
              /* check if characters differ or end of string */
              ac = g_utf8_get_char (ap);
              bc = g_utf8_get_char (bp);
              if (ac != bc || ac == 0)
                break;
            }
        }
    }
  else
    {
      /* try simple (fast) ASCII comparison first (case-insensitive!) */
      for (;; ++ap, ++bp)
        {
          /* check if the characters differ or we have a non-ASCII char */
          ac = *((const guchar *) ap);
          bc = *((const guchar *) bp);
          if (g_ascii_tolower (ac) != g_ascii_tolower (bc) || ac == 0 || ac > 127)
            break;
        }

      /* fallback to Unicode comparison (case-insensitive!) */
      if (G_UNLIKELY (ac > 127 || bc > 127))
        {
          for (;; ap = g_utf8_next_char (ap), bp = g_utf8_next_char (bp))
            {
              /* check if characters differ or end of string */
              ac = g_utf8_get_char (ap);
              bc = g_utf8_get_char (bp);
              if (g_unichar_tolower (ac) != g_unichar_tolower (bc) || ac == 0)
                break;
            }
        }
    }

  /* if both strings are equal, we're done */
  if (G_UNLIKELY (ac == bc || (!case_sensitive && g_unichar_tolower (ac) == g_unichar_tolower (bc))))
    return 0;

  /* check if one of the characters that differ is a digit */
  if (G_UNLIKELY (g_ascii_isdigit (ac) || g_ascii_isdigit (bc)))
    {
      /* if both strings differ in a digit, we use a smarter comparison
       * to get sorting 'file1', 'file5', 'file10' done the right way.
       */
      if (g_ascii_isdigit (ac) && g_ascii_isdigit (bc))
        return compare_by_name_using_number (ap, bp);

      /* a second case is '20 file' and '2file', where comparison by number
       * makes sense, if the previous char for both strings is a digit.
       */
      if (ap > thunar_file_get_display_name (file_a) 
          && bp > thunar_file_get_display_name (file_b)
          && g_ascii_isdigit (*(ap - 1)) && g_ascii_isdigit (*(bp - 1)))
        {
          return compare_by_name_using_number (ap - 1, bp - 1);
        }
    }

  /* otherwise, if they differ in a unicode char, use the
   * appropriate collate function for the current locale (only
   * if charset is UTF-8, else the required transformations
   * would be too expensive)
   */
#ifdef HAVE_STRCOLL
  if ((ac > 127 || bc > 127) && g_get_charset (NULL))
    {
      /* case-sensitive is easy, case-insensitive is expensive,
       * but we use a simple optimization to make it fast.
       */
      if (G_LIKELY (case_sensitive))
        {
          return strcoll (ap, bp);
        }
      else
        {
          /* we use a trick here, so we don't need to allocate
           * and transform the two strings completely first (8
           * byte for each buffer, so all compilers should align
           * them properly)
           */
          gchar abuf[8];
          gchar bbuf[8];

          /* transform the unicode chars to strings and
           * make sure the strings are nul-terminated.
           */
          abuf[g_unichar_to_utf8 (ac, abuf)] = '\0';
          bbuf[g_unichar_to_utf8 (bc, bbuf)] = '\0';

          /* compare the unicode chars (as strings) */
          return strcoll (abuf, bbuf);
        }
    }
#endif

  /* else, they differ in an ASCII character */
  if (G_UNLIKELY (!case_sensitive))
    return (g_unichar_tolower (ac) > g_unichar_tolower (bc)) ? 1 : -1;
  else
    return (ac > bc) ? 1 : -1;
}



gboolean
thunar_file_same_filesystem (const ThunarFile *file_a,
                             const ThunarFile *file_b)
{
  const gchar *filesystem_id_a;
  const gchar *filesystem_id_b;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_a), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file_b), FALSE);

  /* return false if we have no information about one of the files */
  if (file_a->info == NULL || file_b->info == NULL)
    return FALSE;

  /* determine the filesystem IDs */
  filesystem_id_a = g_file_info_get_attribute_string (file_a->info, 
                                                      G_FILE_ATTRIBUTE_ID_FILESYSTEM);

  filesystem_id_b = g_file_info_get_attribute_string (file_b->info, 
                                                      G_FILE_ATTRIBUTE_ID_FILESYSTEM);

  /* compare the filesystem IDs */
  return exo_str_is_equal (filesystem_id_a, filesystem_id_b);
}



/**
 * thunar_file_cache_lookup:
 * @file : a #GFile.
 *
 * Looks up the #ThunarFile for @file in the internal file
 * cache and returns the file present for @file in the
 * cache or %NULL if no #ThunarFile is cached for @file.
 *
 * Note that no reference is taken for the caller.
 *
 * This method should not be used but in very rare cases.
 * Consider using thunar_file_get() instead.
 *
 * Return value: the #ThunarFile for @file in the internal
 *               cache, or %NULL.
 **/
ThunarFile *
thunar_file_cache_lookup (const GFile *file)
{
  ThunarFile *cached_file;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  G_LOCK (file_cache_mutex);

  /* allocate the ThunarFile cache on-demand */
  if (G_UNLIKELY (file_cache == NULL))
    {
      file_cache = g_hash_table_new_full (g_file_hash, 
                                          (GEqualFunc) g_file_equal, 
                                          (GDestroyNotify) g_object_unref, 
                                          NULL);
    }

  cached_file = g_hash_table_lookup (file_cache, file);

  G_UNLOCK (file_cache_mutex);

  return cached_file;
}



gchar *
thunar_file_cached_display_name (const GFile *file)
{
  ThunarFile *cached_file;
  gchar      *base_name;
  gchar      *display_name;
      
  /* check if we have a ThunarFile for it in the cache (usually is the case) */
  cached_file = thunar_file_cache_lookup (file);
  if (cached_file != NULL)
    {
      /* determine the display name of the file */
      display_name = g_strdup (thunar_file_get_display_name (cached_file));
    }
  else
    {
      /* determine something a hopefully good approximation of the display name */
      base_name = g_file_get_basename (G_FILE (file));
      display_name = g_filename_display_name (base_name);
      g_free (base_name);
    }

  return display_name;
}



/**
 * thunar_file_list_get_applications:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Returns the #GList of #GAppInfo<!---->s that can be used to open 
 * all #ThunarFile<!---->s in the given @file_list.
 *
 * The caller is responsible to free the returned list using something like:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GAppInfo<!---->s that can be used to open all 
 *               items in the @file_list.
 **/
GList*
thunar_file_list_get_applications (GList *file_list)
{
  GList       *applications = NULL;
  GList       *list;
  GList       *next;
  GList       *ap;
  GList       *lp;
  const gchar *previous_type;
  const gchar *current_type;

  /* determine the set of applications that can open all files */
  for (lp = file_list; lp != NULL; lp = lp->next)
    {
      current_type = thunar_file_get_content_type (lp->data);

      /* no need to check anything if this file has the same mime type as the previous file */
      if (current_type != NULL && lp->prev != NULL)
        {
          previous_type = thunar_file_get_content_type (lp->prev->data);
          if (G_LIKELY (g_content_type_equals (previous_type, current_type)))
            continue;
        }

      /* determine the list of applications that can open this file */
      list = current_type == NULL ? NULL : g_app_info_get_all_for_type (current_type);
      if (G_UNLIKELY (applications == NULL))
        {
          /* first file, so just use the applications list */
          applications = list;
        }
      else
        {
          /* keep only the applications that are also present in list */
          for (ap = applications; ap != NULL; ap = next)
            {
              /* grab a pointer on the next application */
              next = ap->next;

              /* check if the application is present in list */
              if (g_list_find (list, ap->data) == NULL)
                {
                  /* drop our reference on the application */
                  g_object_unref (G_OBJECT (ap->data));

                  /* drop this application from the list */
                  applications = g_list_delete_link (applications, ap);
                }
            }

          /* release the list of applications for this file */
          g_list_foreach (list, (GFunc) g_object_unref, NULL);
          g_list_free (list);
        }

      /* check if the set is still not empty */
      if (G_LIKELY (applications == NULL))
        break;
    }

  return applications;
}



/**
 * thunar_file_list_to_thunar_g_file_list:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Transforms the @file_list to a #GList of #GFile<!---->s for
 * the #ThunarFile<!---->s contained within @file_list.
 *
 * The caller is responsible to free the returned list using
 * thunar_g_file_list_free() when no longer needed.
 *
 * Return value: the list of #GFile<!---->s for @file_list.
 **/
GList*
thunar_file_list_to_thunar_g_file_list (GList *file_list)
{
  GList *list = NULL;
  GList *lp;

  for (lp = g_list_last (file_list); lp != NULL; lp = lp->prev)
    list = g_list_prepend (list, g_object_ref (THUNAR_FILE (lp->data)->gfile));

  return list;
}
