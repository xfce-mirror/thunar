/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>



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



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_SPECIAL_NAME,
};

/* Signal identifiers */
enum
{
  DESTROY,
  LAST_SIGNAL,
};



static void               thunar_file_class_init               (ThunarFileClass        *klass);
static void               thunar_file_info_init                (ThunarxFileInfoIface   *iface);
static void               thunar_file_dispose                  (GObject                *object);
static void               thunar_file_finalize                 (GObject                *object);
static void               thunar_file_get_property             (GObject                *object,
                                                                guint                   prop_id,
                                                                GValue                 *value,
                                                                GParamSpec             *pspec);
static gchar             *thunar_file_info_get_name            (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_uri             (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_parent_uri      (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_uri_scheme      (ThunarxFileInfo        *file_info);
static gchar             *thunar_file_info_get_mime_type       (ThunarxFileInfo        *file_info);
static gboolean           thunar_file_info_has_mime_type       (ThunarxFileInfo        *file_info,
                                                                const gchar            *mime_type);
static gboolean           thunar_file_info_is_directory        (ThunarxFileInfo        *file_info);
static ThunarVfsInfo     *thunar_file_info_get_vfs_info        (ThunarxFileInfo        *file_info);
static void               thunar_file_info_changed             (ThunarxFileInfo        *file_info);
static gboolean           thunar_file_denies_access_permission (const ThunarFile       *file,
                                                                ThunarVfsFileMode       usr_permissions,
                                                                ThunarVfsFileMode       grp_permissions,
                                                                ThunarVfsFileMode       oth_permissions);
static ThunarMetafile    *thunar_file_get_metafile             (ThunarFile             *file);
static void               thunar_file_monitor                  (ThunarVfsMonitor       *monitor,
                                                                ThunarVfsMonitorHandle *handle,
                                                                ThunarVfsMonitorEvent   event,
                                                                ThunarVfsPath          *handle_path,
                                                                ThunarVfsPath          *event_path,
                                                                gpointer                user_data);
static void               thunar_file_watch_free               (gpointer                data);



static ThunarVfsUserManager *user_manager;
static ThunarVfsMonitor     *monitor;
static ThunarVfsUserId       effective_user_id;
static ThunarMetafile       *metafile;
static GObjectClass         *thunar_file_parent_class;
static GHashTable           *file_cache;
static GQuark                thunar_file_thumb_path_quark;
static GQuark                thunar_file_watch_count_quark;
static GQuark                thunar_file_watch_handle_quark;
static GQuark                thunar_file_emblem_names_quark;
static guint                 file_signals[LAST_SIGNAL];



GType
thunar_file_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarFileClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_file_class_init,
        NULL,
        NULL,
        sizeof (ThunarFile),
        0,
        NULL,
        NULL,
      };

      static const GInterfaceInfo file_info_info = 
      {
        (GInterfaceInitFunc) thunar_file_info_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarFile"), &info, 0);
      g_type_add_interface_static (type, THUNARX_TYPE_FILE_INFO, &file_info_info);
    }

  return type;
}



#ifdef G_ENABLE_DEBUG
static gboolean thunar_file_atexit_registered = FALSE;

static void
thunar_file_atexit_foreach (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  gchar *s;

  s = thunar_vfs_path_dup_string (key);
  g_print ("--> %s (%u)\n", s, G_OBJECT (value)->ref_count);
  g_free (s);
}

static void
thunar_file_atexit (void)
{
  if (file_cache == NULL || g_hash_table_size (file_cache) == 0)
    return;

  g_print ("--- Leaked a total of %u ThunarFile objects:\n",
           g_hash_table_size (file_cache));

  g_hash_table_foreach (file_cache, thunar_file_atexit_foreach, NULL);

  g_print ("\n");
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
  thunar_file_watch_handle_quark = g_quark_from_static_string ("thunar-file-watch-handle");
  thunar_file_emblem_names_quark = g_quark_from_static_string ("thunar-file-emblem-names");

  /* determine the parent class */
  thunar_file_parent_class = g_type_class_peek_parent (klass);

  /* grab a reference on the user manager */
  user_manager = thunar_vfs_user_manager_get_default ();

  /* determine the effective user id of the process */
  effective_user_id = geteuid ();

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_file_dispose;
  gobject_class->finalize = thunar_file_finalize;
  gobject_class->get_property = thunar_file_get_property;

  /**
   * ThunarFile::display-name:
   *
   * The file's display name, see thunar_file_get_display_name()
   * for details.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY_NAME,
                                   g_param_spec_string ("display-name",
                                                        "display-name",
                                                        "display-name",
                                                        NULL,
                                                        EXO_PARAM_READABLE));

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
thunar_file_info_init (ThunarxFileInfoIface *iface)
{
  iface->get_name = thunar_file_info_get_name;
  iface->get_uri = thunar_file_info_get_uri;
  iface->get_parent_uri = thunar_file_info_get_parent_uri;
  iface->get_uri_scheme = thunar_file_info_get_uri_scheme;
  iface->get_mime_type = thunar_file_info_get_mime_type;
  iface->has_mime_type = thunar_file_info_has_mime_type;
  iface->is_directory = thunar_file_info_is_directory;
  iface->get_vfs_info = thunar_file_info_get_vfs_info;
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

  /* drop the entry from the cache */
  g_hash_table_remove (file_cache, file->info->path);

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

  /* drop a reference on the metadata if we own one */
  if ((file->flags & THUNAR_FILE_OWNS_METAFILE_REFERENCE) != 0)
    g_object_unref (G_OBJECT (metafile));

  /* release the file info */
  thunar_vfs_info_unref (file->info);

  (*G_OBJECT_CLASS (thunar_file_parent_class)->finalize) (object);
}



static void
thunar_file_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ThunarFile *file = THUNAR_FILE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gchar*
thunar_file_info_get_name (ThunarxFileInfo *file_info)
{
  return g_strdup (thunar_vfs_path_get_name (THUNAR_FILE (file_info)->info->path));
}



static gchar*
thunar_file_info_get_uri (ThunarxFileInfo *file_info)
{
  return thunar_vfs_path_dup_uri (THUNAR_FILE (file_info)->info->path);
}



static gchar*
thunar_file_info_get_parent_uri (ThunarxFileInfo *file_info)
{
  ThunarVfsPath *parent;

  parent = thunar_vfs_path_get_parent (THUNAR_FILE (file_info)->info->path);
  if (G_LIKELY (parent != NULL))
    return thunar_vfs_path_dup_uri (parent);

  return NULL;
}



static gchar*
thunar_file_info_get_uri_scheme (ThunarxFileInfo *file_info)
{
  return g_strdup ("file");
}



static gchar*
thunar_file_info_get_mime_type (ThunarxFileInfo *file_info)
{
  ThunarVfsMimeInfo *mime_info;
  gchar             *mime_type = NULL;

  /* determine the mime info for the file */
  mime_info = thunar_file_get_mime_info (THUNAR_FILE (file_info));
  if (G_LIKELY (mime_info != NULL))
    mime_type = g_strdup (thunar_vfs_mime_info_get_name (mime_info));

  return mime_type;
}



static gboolean
thunar_file_info_has_mime_type (ThunarxFileInfo *file_info,
                                const gchar     *mime_type)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  gboolean               valid = FALSE;
  GList                 *mime_infos;
  GList                 *lp;

  /* determine the mime info for the file */
  mime_info = thunar_file_get_mime_info (THUNAR_FILE (file_info));
  if (G_UNLIKELY (mime_info == NULL))
    return FALSE;

  /* check the related mime types for the file's mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_infos = thunar_vfs_mime_database_get_infos_for_info (mime_database, mime_info);
  for (lp = mime_infos; lp != NULL && !valid; lp = lp->next)
    valid = (strcmp (thunar_vfs_mime_info_get_name (lp->data), mime_type) == 0);
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_list_free (mime_infos);

  return valid;
}



static gboolean
thunar_file_info_is_directory (ThunarxFileInfo *file_info)
{
  return thunar_file_is_directory (THUNAR_FILE (file_info));
}



static ThunarVfsInfo*
thunar_file_info_get_vfs_info (ThunarxFileInfo *file_info)
{
  return thunar_vfs_info_ref (THUNAR_FILE (file_info)->info);
}



static void
thunar_file_info_changed (ThunarxFileInfo *file_info)
{
  /* reset the thumbnail state, so the next thunar_icon_factory_load_file_icon()
   * invokation will recheck the thumbnail.
   */
  thunar_file_set_thumb_state (THUNAR_FILE (file_info), THUNAR_FILE_THUMB_STATE_UNKNOWN);

  /* notify about changes of the display-name property */
  g_object_notify (G_OBJECT (file_info), "display-name");

  /* tell the file monitor that this file changed */
  thunar_file_monitor_file_changed (THUNAR_FILE (file_info));
}



static gboolean
thunar_file_denies_access_permission (const ThunarFile *file,
                                      ThunarVfsFileMode usr_permissions,
                                      ThunarVfsFileMode grp_permissions,
                                      ThunarVfsFileMode oth_permissions)
{
  ThunarVfsFileMode mode;
  ThunarVfsGroup   *group;
  ThunarVfsUser    *user;
  gboolean          result;
  GList            *groups;
  GList            *lp;

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

  if (thunar_vfs_user_is_me (user))
    {
      /* we're the owner, so the usr permissions must be granted */
      result = ((mode & usr_permissions) == 0);
    }
  else
    {
      group = thunar_file_get_group (file);
      if (G_LIKELY (group != NULL))
        {
          /* check the group permissions */
          groups = thunar_vfs_user_get_groups (user);
          for (lp = groups; lp != NULL; lp = lp->next)
            if (THUNAR_VFS_GROUP (lp->data) == group)
              {
                g_object_unref (G_OBJECT (user));
                g_object_unref (G_OBJECT (group));
                return ((mode & grp_permissions) == 0);
              }

          g_object_unref (G_OBJECT (group));
        }

      /* check other permissions */
      result = ((mode & oth_permissions) == 0);
    }

  g_object_unref (G_OBJECT (user));

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
thunar_file_monitor (ThunarVfsMonitor       *monitor,
                     ThunarVfsMonitorHandle *handle,
                     ThunarVfsMonitorEvent   event,
                     ThunarVfsPath          *handle_path,
                     ThunarVfsPath          *event_path,
                     gpointer                user_data)
{
  ThunarFile *file = THUNAR_FILE (user_data);

  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (thunar_vfs_path_equal (file->info->path, event_path));

  /* just to be sure... */
  if (G_UNLIKELY (!thunar_vfs_path_equal (handle_path, event_path)))
    return;

  switch (event)
    {
    case THUNAR_VFS_MONITOR_EVENT_CHANGED:
    case THUNAR_VFS_MONITOR_EVENT_CREATED:
      thunar_file_reload (file);
      break;

    case THUNAR_VFS_MONITOR_EVENT_DELETED:
      thunar_file_destroy (file);
      break;
    }
}



static void
thunar_file_watch_free (gpointer data)
{
  /* remove the watch from the VFS monitor */
  thunar_vfs_monitor_remove (monitor, data);

  /* release our reference on the VFS monitor */
  g_object_unref (G_OBJECT (monitor));
}



/**
 * thunar_file_get_for_info:
 * @info : a #ThunarVfsInfo.
 *
 * Looks up the #ThunarFile corresponding to @info. Use
 * this function if want to specify the #ThunarVfsInfo
 * to use for a #ThunarFile instead of thunar_file_get_for_path(),
 * which would determine the #ThunarVfsInfo once again.
 *
 * The caller is responsible to call g_object_unref()
 * when done with the returned object.
 *
 * Return value: the #ThunarFile for @info.
 **/
ThunarFile*
thunar_file_get_for_info (ThunarVfsInfo *info)
{
  ThunarFile *file;

  g_return_val_if_fail (info != NULL, NULL);

  /* check if we already have a cached version of that file */
  file = thunar_file_cache_lookup (info->path);
  if (G_UNLIKELY (file != NULL))
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (file));

      /* apply the new info */
      if (!thunar_vfs_info_matches (file->info, info))
        {
          thunar_vfs_info_unref (file->info);
          file->info = thunar_vfs_info_ref (info);
          thunar_file_changed (file);
        }
    }
  else
    {
      /* allocate a new object */
      file = g_object_new (THUNAR_TYPE_FILE, NULL);
      file->info = thunar_vfs_info_ref (info);

      /* insert the file into the cache */
      g_hash_table_insert (file_cache, thunar_vfs_path_ref (info->path), file);
    }

  return file;
}



/**
 * thunar_file_get_for_path:
 * @path  : a #ThunarVfsPath.
 * @error : error return location.
 *
 * Tries to query the file referred to by @path. Returns %NULL
 * if the file could not be queried and @error will point
 * to an error describing the problem, else the #ThunarFile
 * instance will be returned, which needs to freed using
 * g_object_unref() when no longer needed.
 *
 * Note that this function is not thread-safe and may only
 * be called from the main thread.
 *
 * Return value: the #ThunarFile instance or %NULL on error.
 **/
ThunarFile*
thunar_file_get_for_path (ThunarVfsPath *path,
                          GError       **error)
{
  ThunarVfsInfo *info;
  ThunarFile    *file;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* see if we have the corresponding file cached already */
  file = thunar_file_cache_lookup (path);
  if (file == NULL)
    {
      /* determine the file info */
      info = thunar_vfs_info_new_for_path (path, error);
      if (G_UNLIKELY (info == NULL))
        return NULL;

      /* allocate the new file object */
      file = thunar_file_get_for_info (info);

      /* release our reference to the info */
      thunar_vfs_info_unref (info);
    }
  else
    {
      /* take another reference on the cached file */
      g_object_ref (G_OBJECT (file));
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
  ThunarVfsPath *path;
  ThunarFile    *file;

  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* determine the path for the URI */
  path = thunar_vfs_path_new (uri, error);
  if (G_UNLIKELY (path == NULL))
    return NULL;

  /* determine the file for the path */
  file = thunar_file_get_for_path (path, error);

  /* release the path */
  thunar_vfs_path_unref (path);

  return file;
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!thunar_file_has_parent (file))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, _("The root folder has no parent"));
      return NULL;
    }

  return thunar_file_get_for_path (thunar_vfs_path_get_parent (file->info->path), error);
}



/**
 * thunar_file_execute:
 * @file      : a #ThunarFile instance.
 * @screen    : a #GdkScreen.
 * @path_list : the list of #ThunarVfsPath<!---->s to supply to @file on
 *              execution.
 * @error     : return location for errors or %NULL.
 *
 * Tries to execute @file on the specified @screen. If @file is executable
 * and could have been spawned successfully, %TRUE is returned, else %FALSE
 * will be returned and @error will be set to point to the error location.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_file_execute (ThunarFile *file,
                     GdkScreen  *screen,
                     GList      *path_list,
                     GError    **error)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  return thunar_vfs_info_execute (file->info, screen, path_list, NULL, error);
}



/**
 * thunar_file_launch:
 * @file   : a #ThunarFile instance.
 * @parent : a #GtkWidget or a #GdkScreen on which to launch the @file.
 *           May also be %NULL in which case the default #GdkScreen will
 *           be used.
 * @error  : return location for errors or %NULL.
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
thunar_file_launch (ThunarFile *file,
                    gpointer    parent,
                    GError    **error)
{
  ThunarVfsMimeApplication *handler;
  ThunarVfsMimeDatabase    *database;
  ThunarApplication        *application;
  GdkScreen                *screen;
  gboolean                  succeed;
  GList                     path_list;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent), FALSE);

  /* determine the screen for the parent */
  if (G_UNLIKELY (parent == NULL))
    screen = gdk_screen_get_default ();
  else if (GTK_IS_WIDGET (parent))
    screen = gtk_widget_get_screen (parent);
  else
    screen = GDK_SCREEN (parent);

  /* check if we should execute the file */
  if (thunar_file_is_executable (file))
    return thunar_file_execute (file, screen, NULL, error);

  /* check if we have a folder here */
  if (thunar_file_is_directory (file))
    {
      application = thunar_application_get ();
      thunar_application_open_window (application, file, screen);
      g_object_unref (G_OBJECT (application));
      return TRUE;
    }

  /* determine the default handler for the file */
  database = thunar_vfs_mime_database_get_default ();
  handler = thunar_vfs_mime_database_get_default_application (database, thunar_file_get_mime_info (file));
  g_object_unref (G_OBJECT (database));

  /* if we don't have any default handler, just popup the application chooser */
  if (G_UNLIKELY (handler == NULL))
    {
      thunar_show_chooser_dialog (parent, file, TRUE);
      return TRUE;
    }

  /* fake a path list */
  path_list.data = thunar_file_get_path (file);
  path_list.next = path_list.prev = NULL;

  /* otherwise try to execute the application */
  succeed = thunar_vfs_mime_handler_exec (THUNAR_VFS_MIME_HANDLER (handler), screen, &path_list, error);

  /* release the handler reference */
  g_object_unref (G_OBJECT (handler));

  return succeed;
}



/**
 * thunar_file_rename:
 * @file  : a #ThunarFile instance.
 * @name  : the new file name in UTF-8 encoding.
 * @error : return location for errors or %NULL.
 *
 * Tries to rename @file to the new @name. If @file cannot be renamed,
 * %FALSE will be returned and @error will be accordingly. Else, if
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
thunar_file_rename (ThunarFile  *file,
                    const gchar *name,
                    GError     **error)
{
  ThunarVfsPath *previous_path;
  gboolean       succeed;
  gint           watch_count;;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* remember the previous path */
  previous_path = thunar_vfs_path_ref (file->info->path);
  
  /* try to rename the file */
  succeed = thunar_vfs_info_rename (file->info, name, error);
  if (G_LIKELY (succeed))
    {
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

      /* drop the previous entry from the cache */
      g_hash_table_remove (file_cache, previous_path);

      /* insert the new entry */
      g_hash_table_insert (file_cache, thunar_vfs_path_ref (file->info->path), file);

      /* tell the associated folder that the file was renamed */
      thunarx_file_info_renamed (THUNARX_FILE_INFO (file));

      /* emit the file changed signal */
      thunar_file_changed (file);
    }

  /* drop the reference on the previous path */
  thunar_vfs_path_unref (previous_path);

  return succeed;
}



/**
 * thunar_file_accepts_drop:
 * @file                    : a #ThunarFile instance.
 * @path_list               : the list of #ThunarVfsPath<!---->s that will be droppped.
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
                          GList          *path_list,
                          GdkDragContext *context,
                          GdkDragAction  *suggested_action_return)
{
  ThunarVfsPath *parent_path;
  ThunarVfsPath *path;
  GdkDragAction  actions;
  GList         *lp;
  guint          n;

  g_return_val_if_fail (THUNAR_IS_FILE (file), 0);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  /* we can never drop an empty list */
  if (G_UNLIKELY (path_list == NULL))
    return 0;

  /* check if we have a writable directory here or an executable file */
  if (thunar_file_is_directory (file) && thunar_file_is_writable (file))
    actions = context->actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
  else if (thunar_file_is_executable (file))
    actions = context->actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_PRIVATE);
  else
    return 0;

  /* determine the path of the file */
  path = thunar_file_get_path (file);

  /* check up to 500 of the paths (just in case somebody tries to
   * drag around his music collection with 5000 files).
   */
  for (lp = path_list, n = 0; lp != NULL && n < 500; lp = lp->next, ++n)
    {
      /* we cannot drop a file on itself */
      if (G_UNLIKELY (thunar_vfs_path_equal (path, lp->data)))
        return 0;

      /* check whether source and destination are the same */
      parent_path = thunar_vfs_path_get_parent (lp->data);
      if (G_LIKELY (parent_path != NULL))
        {
          if (thunar_vfs_path_equal (path, parent_path))
            return 0;
        }
    }

  /* determine the preferred action based on the context */
  if (G_LIKELY (suggested_action_return != NULL))
    {
      /* determine a working action */
      if (G_LIKELY ((context->suggested_action & actions) != 0))
        *suggested_action_return = context->suggested_action;
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
 * thunar_file_get_display_name:
 * @file : a #ThunarFile instance.
 *
 * Returns the @file name in the UTF-8 encoding, which is
 * suitable for displaying the file name in the GUI.
 *
 * Return value: the @file name suitable for display.
 **/
const gchar*
thunar_file_get_display_name (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* root directory is always displayed as 'Filesystem' */
  if (thunar_file_is_root (file))
    return _("File System");

  return file->info->display_name;
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
ThunarVfsFileTime
thunar_file_get_date (const ThunarFile  *file,
                      ThunarFileDateType date_type)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  
  switch (date_type)
    {
    case THUNAR_FILE_DATE_ACCESSED: return file->info->atime;
    case THUNAR_FILE_DATE_CHANGED:  return file->info->ctime;
    case THUNAR_FILE_DATE_MODIFIED: return file->info->mtime;
    default:                        g_assert_not_reached ();
    }

  return (ThunarVfsFileTime) -1;
}



/**
 * thunar_file_get_date_string:
 * @file      : a #ThunarFile instance.
 * @date_type : the kind of date you are interested to know about @file.
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
                             ThunarFileDateType date_type)
{
  ThunarVfsFileTime time;
#ifdef HAVE_LOCALTIME_R
  struct tm         tmbuf;
#endif
  struct tm        *tm;
  gchar            *result;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* query the date on the given file */
  time = thunar_file_get_date (file, date_type);

  /* convert to local time */
#ifdef HAVE_LOCALTIME_R
  tm = localtime_r (&time, &tmbuf);
#else
  tm = localtime (&time);
#endif

  /* convert to string */
  result = g_new (gchar, 20);
  strftime (result, 20, "%Y-%m-%d %H:%M:%S", tm);

  return result;
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
  ThunarVfsFileType kind;
  ThunarVfsFileMode mode;
  gchar            *text;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  kind = thunar_file_get_kind (file);
  mode = thunar_file_get_mode (file);
  text = g_new (gchar, 11);

  /* file type */
  switch (kind)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:     text[0] = 's'; break;
    case THUNAR_VFS_FILE_TYPE_SYMLINK:    text[0] = 'l'; break;
    case THUNAR_VFS_FILE_TYPE_REGULAR:    text[0] = '-'; break;
    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:   text[0] = 'b'; break;
    case THUNAR_VFS_FILE_TYPE_DIRECTORY:  text[0] = 'd'; break;
    case THUNAR_VFS_FILE_TYPE_CHARDEV:    text[0] = 'c'; break;
    case THUNAR_VFS_FILE_TYPE_FIFO:       text[0] = 'f'; break;
    case THUNAR_VFS_FILE_TYPE_UNKNOWN:    text[0] = ' '; break;
    default:                              g_assert_not_reached ();
    }

  /* permission flags */
  text[1] = (mode & THUNAR_VFS_FILE_MODE_USR_READ)  ? 'r' : '-';
  text[2] = (mode & THUNAR_VFS_FILE_MODE_USR_WRITE) ? 'w' : '-';
  text[3] = (mode & THUNAR_VFS_FILE_MODE_USR_EXEC)  ? 'x' : '-';
  text[4] = (mode & THUNAR_VFS_FILE_MODE_GRP_READ)  ? 'r' : '-';
  text[5] = (mode & THUNAR_VFS_FILE_MODE_GRP_WRITE) ? 'w' : '-';
  text[6] = (mode & THUNAR_VFS_FILE_MODE_GRP_EXEC)  ? 'x' : '-';
  text[7] = (mode & THUNAR_VFS_FILE_MODE_OTH_READ)  ? 'r' : '-';
  text[8] = (mode & THUNAR_VFS_FILE_MODE_OTH_WRITE) ? 'w' : '-';
  text[9] = (mode & THUNAR_VFS_FILE_MODE_OTH_EXEC)  ? 'x' : '-';

  /* special flags */
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_SUID))
    text[3] = 's';
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_SGID))
    text[6] = 's';
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_STICKY))
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
gchar*
thunar_file_get_size_string (const ThunarFile *file)
{
  ThunarVfsFileSize size;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  size = thunar_file_get_size (file);

  return thunar_vfs_humanize_size (size, NULL, 0);
}



/**
 * thunar_file_get_volume:
 * @file           : a #ThunarFile instance.
 * @volume_manager : the #ThunarVfsVolumeManager to use for the volume lookup.
 *
 * Attempts to determine the #ThunarVfsVolume on which @file is located
 * using the given @volume_manager. If @file cannot determine it's volume,
 * then %NULL will be returned. Else a #ThunarVfsVolume instance is returned,
 * which is owned by @volume_manager and thereby the caller must not free
 * the returned object.
 *
 * Return value: the #ThunarVfsVolume for @file in @volume_manager or %NULL.
 **/
ThunarVfsVolume*
thunar_file_get_volume (const ThunarFile       *file,
                        ThunarVfsVolumeManager *volume_manager)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (volume_manager), NULL);
  return thunar_vfs_volume_manager_get_volume_by_info (volume_manager, file->info);
}



/**
 * thunar_file_get_group:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarVfsGroup for @file. If there's no
 * group associated with @file or if the system is unable to
 * determine the group, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarVfsGroup for @file or %NULL.
 **/
ThunarVfsGroup*
thunar_file_get_group (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return thunar_vfs_user_manager_get_group_by_id (user_manager, file->info->gid);
}



/**
 * thunar_file_get_user:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarVfsUser for @file. If there's no
 * user associated with @file or if the system is unable
 * to determine the user, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarVfsUser for @file or %NULL.
 **/
ThunarVfsUser*
thunar_file_get_user (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return thunar_vfs_user_manager_get_user_by_id (user_manager, file->info->uid);
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* we can only change the mode if we the euid is
   *   a) equal to the file owner id
   * or
   *   b) the super-user id.
   */
  return (effective_user_id == 0 || effective_user_id == file->info->uid);
}



/**
 * thunar_file_is_executable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to execute the @file (or enter the directory refered to by
 * @file).
 *
 * Return value: %TRUE if @file can be executed.
 **/
gboolean
thunar_file_is_executable (const ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return ((file->info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) != 0);
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return !thunar_file_denies_access_permission (file,
                                                THUNAR_VFS_FILE_MODE_USR_READ,
                                                THUNAR_VFS_FILE_MODE_GRP_READ,
                                                THUNAR_VFS_FILE_MODE_OTH_READ);
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
  gboolean renameable = FALSE;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* we cannot rename root nodes */
  if (G_LIKELY (!thunar_file_is_root (file)))
    {
      /* we just do a guess here, by checking whether the folder is writable */
      file = thunar_file_get_parent (file, NULL);
      if (G_LIKELY (file != NULL))
        {
          renameable = thunar_file_is_writable (file);
          g_object_unref (G_OBJECT (file));
        }
    }

  return renameable;
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return !thunar_file_denies_access_permission (file,
                                                THUNAR_VFS_FILE_MODE_USR_WRITE,
                                                THUNAR_VFS_FILE_MODE_GRP_WRITE,
                                                THUNAR_VFS_FILE_MODE_OTH_WRITE);


}



/**
 * thunar_file_get_actions:
 * @file   : a #ThunarFile instance.
 * @window : a #GdkWindow instance.
 *
 * Returns additional #GtkAction<!---->s for @file.
 *
 * The caller is responsible to free the returned list
 * using
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: additional #GtkAction<!---->s for @file.
 **/
GList*
thunar_file_get_actions (ThunarFile *file,
                         GtkWidget  *window)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  return NULL;
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
  const ThunarVfsInfo *info = file->info;
  const ThunarVfsPath *parent;
  const gchar         *emblem_string;
  gchar              **emblem_names;
  GList               *emblems = NULL;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

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

  if (G_UNLIKELY (strcmp (info->display_name, "Desktop") == 0))
    {
      parent = thunar_vfs_path_get_parent (info->path);
      if (G_LIKELY (parent != NULL) && thunar_vfs_path_is_home (parent))
        emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_DESKTOP);
    }

  if ((info->flags & THUNAR_VFS_FILE_FLAGS_SYMLINK) != 0)
    emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK);

  /* we add "cant-read" if either (a) the file is not readable or (b) a directory, that lacks the
   * x-bit, see http://bugzilla.xfce.org/show_bug.cgi?id=1408 for the details about this change.
   */
  if (!thunar_file_is_readable (file)
      || (thunar_file_is_directory (file)
        && thunar_file_denies_access_permission (file, THUNAR_VFS_FILE_MODE_USR_EXEC,
                                                       THUNAR_VFS_FILE_MODE_GRP_EXEC,
                                                       THUNAR_VFS_FILE_MODE_OTH_EXEC)))
    {
      emblems = g_list_prepend (emblems, THUNAR_FILE_EMBLEM_NAME_CANT_READ);
    }
  else if (G_UNLIKELY (file->info->uid == effective_user_id && !thunar_file_is_writable (file)))
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

  g_return_if_fail (THUNAR_IS_FILE (file));

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
 * thunar_file_get_icon_name:
 * @file       : a #ThunarFile instance.
 * @icon_state : the state of the @file<!---->s icon we are interested in.
 * @icon_theme : the #GtkIconTheme on which to lookup up the icon name.
 *
 * Returns the name of the icon that can be used to present @file, based
 * on the given @icon_state and @icon_theme.
 *
 * Return value: the icon name for @file in @icon_theme.
 **/
const gchar*
thunar_file_get_icon_name (const ThunarFile   *file,
                           ThunarFileIconState icon_state,
                           GtkIconTheme       *icon_theme)
{
  const gchar *icon_name;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* special icon for the root node */
  if (G_UNLIKELY (thunar_file_is_root (file))
      && gtk_icon_theme_has_icon (icon_theme, "gnome-dev-harddisk"))
    {
      return "gnome-dev-harddisk";
    }

  /* special icon for the home node */
  if (G_UNLIKELY (thunar_file_is_home (file))
      && gtk_icon_theme_has_icon (icon_theme, "gnome-fs-home"))
    {
      return "gnome-fs-home";
    }

  /* try to be smart when determining icons for executable files
   * in that we use the name of the file as icon name (which will
   * work for quite a lot of binaries, e.g. 'Terminal', 'mousepad',
   * 'Thunar', 'xfmedia', etc.).
   */
  if (G_UNLIKELY (thunar_file_is_executable (file)))
    {
      icon_name = thunar_vfs_path_get_name (file->info->path);
      if (G_UNLIKELY (gtk_icon_theme_has_icon (icon_theme, icon_name)))
        return icon_name;
    }

  /* default is the mime type icon */
  icon_name = thunar_vfs_mime_info_lookup_icon_name (file->info->mime_info, icon_theme);

  /* check if we have an accept icon for the icon we found */
  if ((icon_state == THUNAR_FILE_ICON_STATE_DROP || icon_state == THUNAR_FILE_ICON_STATE_OPEN)
      && strcmp (icon_name, "gnome-fs-directory") == 0
      && gtk_icon_theme_has_icon (icon_theme, "gnome-fs-directory-accept"))
    {
      return "gnome-fs-directory-accept";
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (key < THUNAR_METAFILE_N_KEYS, NULL);

  return thunar_metafile_fetch (thunar_file_get_metafile (file),
                                file->info->path, key,
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
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (key < THUNAR_METAFILE_N_KEYS);
  g_return_if_fail (default_value != NULL);
  g_return_if_fail (value != NULL);

  thunar_metafile_store (thunar_file_get_metafile (file),
                         file->info->path, key, value,
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
  ThunarVfsMonitorHandle *handle;
  gint                    watch_count;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_FILE_GET_WATCH_COUNT (file) >= 0);

  watch_count = THUNAR_FILE_GET_WATCH_COUNT (file);

  if (++watch_count == 1)
    {
      /* take a reference on the VFS monitor for this instance */
      if (G_LIKELY (monitor == NULL))
        {
          monitor = thunar_vfs_monitor_get_default ();
          g_object_add_weak_pointer (G_OBJECT (monitor), (gpointer) &monitor);
        }
      else
        {
          g_object_ref (G_OBJECT (monitor));
        }

      /* add us to the file monitor */
      handle = thunar_vfs_monitor_add_file (monitor, file->info->path, thunar_file_monitor, file);
      g_object_set_qdata_full (G_OBJECT (file), thunar_file_watch_handle_quark, handle, thunar_file_watch_free);
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

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_FILE_GET_WATCH_COUNT (file) > 0);

  watch_count = THUNAR_FILE_GET_WATCH_COUNT (file);

  if (--watch_count == 0)
    {
      /* just unset the watch handle */
      g_object_set_qdata (G_OBJECT (file), thunar_file_watch_handle_quark, NULL);
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
  ThunarVfsInfo *info;

  g_return_if_fail (THUNAR_IS_FILE (file));

  /* re-query the file info */
  info = thunar_vfs_info_new_for_path (file->info->path, NULL);
  if (G_UNLIKELY (info == NULL))
    {
      /* the file is no longer present */
      thunar_file_destroy (file);
    }
  else
    {
      /* apply the new info... */
      thunar_vfs_info_unref (file->info);
      file->info = info;

      /* ... and tell others */
      thunar_file_changed (file);
    }
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
  g_return_if_fail (THUNAR_IS_FILE (file));

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
  g_return_val_if_fail (THUNAR_IS_FILE (file_a), 0);
  g_return_val_if_fail (THUNAR_IS_FILE (file_b), 0);
#endif

  /* we compare only the display names (UTF-8!) */
  ap = file_a->info->display_name;
  bp = file_b->info->display_name;

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
      if (ap > file_a->info->display_name && bp > file_b->info->display_name
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



/**
 * thunar_file_cache_lookup:
 * @path : a #ThunarVfsPath.
 *
 * Looks up the #ThunarFile for @path in the internal file
 * cache and returns the file present for @path in the
 * cache or %NULL if no #ThunarFile is cached for @path.
 *
 * Note that no reference is taken for the caller.
 *
 * This method should not be used but in very rare cases.
 * Consider using thunar_file_get_for_path() instead.
 *
 * Return value: the #ThunarFile for @path in the internal
 *               cache, or %NULL.
 **/
ThunarFile*
thunar_file_cache_lookup (const ThunarVfsPath *path)
{
  /* allocate the ThunarFile cache on-demand */
  if (G_UNLIKELY (file_cache == NULL))
    file_cache = g_hash_table_new_full (thunar_vfs_path_hash, thunar_vfs_path_equal, (GDestroyNotify) thunar_vfs_path_unref, NULL);

  return g_hash_table_lookup (file_cache, path);
}



/**
 * thunar_file_list_get_applications:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Returns the #GList of #ThunarVfsMimeApplication<!---->s
 * that can be used to open all #ThunarFile<!---->s in the
 * given @file_list.
 *
 * The caller is responsible to free the returned list using
 * something like:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) thunar_vfs_mime_application_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the list of #ThunarVfsMimeApplication<!---->s that
 *               can be used to open all items in the @file_list.
 **/
GList*
thunar_file_list_get_applications (GList *file_list)
{
  ThunarVfsMimeDatabase *database;
  GList                 *applications = NULL;
  GList                 *list;
  GList                 *next;
  GList                 *ap;
  GList                 *lp;

  /* grab a reference on the mime database */
  database = thunar_vfs_mime_database_get_default ();

  /* determine the set of applications that can open all files */
  for (lp = file_list; lp != NULL; lp = lp->next)
    {
      /* no need to check anything if this file has the same mime type as the previous file */
      if (lp->prev != NULL && thunar_file_get_mime_info (lp->prev->data) == thunar_file_get_mime_info (lp->data))
        continue;

      /* determine the list of applications that can open this file */
      list = thunar_vfs_mime_database_get_applications (database, thunar_file_get_mime_info (lp->data));
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

  /* release the mime database */
  g_object_unref (G_OBJECT (database));

  return applications;
}



/**
 * thunar_file_list_to_path_list:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Transforms the @file_list to a #GList of #ThunarVfsPath<!---->s for
 * the #ThunarFile<!---->s contained within @file_list.
 *
 * The caller is responsible to free the returned list using
 * thunar_vfs_path_list_free() when no longer needed.
 *
 * Return value: the list of #ThunarVfsPath<!---->s for @file_list.
 **/
GList*
thunar_file_list_to_path_list (GList *file_list)
{
  GList *path_list = NULL;
  GList *lp;

  for (lp = g_list_last (file_list); lp != NULL; lp = lp->prev)
    path_list = g_list_prepend (path_list, thunar_vfs_path_ref (THUNAR_FILE (lp->data)->info->path));

  return path_list;
}



