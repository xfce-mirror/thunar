/* $Id$ */
/*-
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
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <thunar/thunar-computer-folder.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-local-file.h>
#include <thunar/thunar-trash-folder.h>


enum
{
  CHANGED,
  LAST_SIGNAL,
};



static void               thunar_file_class_init               (ThunarFileClass        *klass);
static void               thunar_file_finalize                 (GObject                *object);
static ThunarFile        *thunar_file_real_get_parent          (ThunarFile             *file,
                                                                GError                **error);
static ThunarFolder      *thunar_file_real_open_as_folder      (ThunarFile             *file,
                                                                GError                **error);
static const gchar       *thunar_file_real_get_special_name    (ThunarFile             *file);
static gboolean           thunar_file_real_can_execute         (ThunarFile             *file);
static gboolean           thunar_file_real_can_read            (ThunarFile             *file);
static gboolean           thunar_file_real_can_write           (ThunarFile             *file);
static void               thunar_file_real_changed             (ThunarFile             *file);
static ThunarFile        *thunar_file_new_internal             (ThunarVfsURI           *uri,
                                                                GError                **error);
static gboolean           thunar_file_denies_access_permission (ThunarFile             *file,
                                                                ThunarVfsFileMode       usr_permissions,
                                                                ThunarVfsFileMode       grp_permissions,
                                                                ThunarVfsFileMode       oth_permissions);
static void               thunar_file_destroyed                (gpointer                data,
                                                                GObject                *object);



static GObjectClass *thunar_file_parent_class;
static GHashTable   *file_cache;
static guint         file_signals[LAST_SIGNAL];



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

      type = g_type_register_static (GTK_TYPE_OBJECT,
                                     "ThunarFile", &info,
                                     G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



#ifndef G_DISABLE_CHECKS
static gboolean thunar_file_atexit_registered = FALSE;

static void
thunar_file_atexit_foreach (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  gchar *s;

  s = thunar_vfs_uri_to_string (THUNAR_VFS_URI (key), 0);
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

#ifndef G_DISABLE_CHECKS
  if (G_UNLIKELY (!thunar_file_atexit_registered))
    {
      g_atexit (thunar_file_atexit);
      thunar_file_atexit_registered = TRUE;
    }
#endif

  thunar_file_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_file_finalize;

  klass->has_parent = (gpointer) exo_noop_true;
  klass->get_parent = thunar_file_real_get_parent;
  klass->open_as_folder = thunar_file_real_open_as_folder;
  klass->get_mime_info = (gpointer) exo_noop_null;
  klass->get_special_name = thunar_file_real_get_special_name;
  klass->get_date = (gpointer) exo_noop_false;
  klass->get_size = (gpointer) exo_noop_false;
  klass->get_volume = (gpointer) exo_noop_null;
  klass->get_group = (gpointer) exo_noop_null;
  klass->get_user = (gpointer) exo_noop_null;
  klass->can_execute = thunar_file_real_can_execute;
  klass->can_read = thunar_file_real_can_read;
  klass->can_write = thunar_file_real_can_write;
  klass->get_emblem_names = (gpointer) exo_noop_null;
  klass->reload = (gpointer) exo_noop;
  klass->changed = thunar_file_real_changed;

  /**
   * ThunarFile::changed:
   * @file : the #ThunarFile instance.
   *
   * Emitted whenever the system notices a change to @file.
   **/
  file_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ThunarFileClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_file_finalize (GObject *object)
{
  ThunarFile *file = THUNAR_FILE (object);

#ifndef G_DISABLE_CHECKS
  if (G_UNLIKELY (file->watch_count != 0))
    {
      g_error ("Attempt to finalize a ThunarFile, which has an "
               "active watch count of %d", file->watch_count);
    }
#endif

  /* destroy the cached icon if any */
  if (G_LIKELY (file->cached_icon != NULL))
    g_object_unref (G_OBJECT (file->cached_icon));

  G_OBJECT_CLASS (thunar_file_parent_class)->finalize (object);
}



static ThunarFile*
thunar_file_real_get_parent (ThunarFile *file,
                             GError    **error)
{
  ThunarVfsURI *parent_uri;
  ThunarFile   *parent_file;
  gchar        *p;

  /* lookup the parent's URI */
  parent_uri = thunar_vfs_uri_parent (thunar_file_get_uri (file));
  if (G_UNLIKELY (parent_uri == NULL))
    {
      p = thunar_vfs_uri_to_string (thunar_file_get_uri (file), 0);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   _("Failed to determine parent URI for '%s'"), p);
      g_free (p);
      return NULL;
    }

  /* lookup the file object for the parent_uri */
  parent_file = thunar_file_get_for_uri (parent_uri, error);

  /* release the reference on the parent_uri */
  thunar_vfs_uri_unref (parent_uri);

  return parent_file;
}



static ThunarFolder*
thunar_file_real_open_as_folder (ThunarFile *file,
                                 GError    **error)
{
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR, g_strerror (ENOTDIR));
  return NULL;
}



static const gchar*
thunar_file_real_get_special_name (ThunarFile *file)
{
  return THUNAR_FILE_GET_CLASS (file)->get_display_name (file);
}



static gboolean
thunar_file_real_can_execute (ThunarFile *file)
{
  return !thunar_file_denies_access_permission (file,
                                                THUNAR_VFS_FILE_MODE_USR_EXEC,
                                                THUNAR_VFS_FILE_MODE_GRP_EXEC,
                                                THUNAR_VFS_FILE_MODE_OTH_EXEC);
}



static gboolean
thunar_file_real_can_read (ThunarFile *file)
{
  return !thunar_file_denies_access_permission (file,
                                                THUNAR_VFS_FILE_MODE_USR_READ,
                                                THUNAR_VFS_FILE_MODE_GRP_READ,
                                                THUNAR_VFS_FILE_MODE_OTH_READ);
}



static gboolean
thunar_file_real_can_write (ThunarFile *file)
{
  return !thunar_file_denies_access_permission (file,
                                                THUNAR_VFS_FILE_MODE_USR_WRITE,
                                                THUNAR_VFS_FILE_MODE_GRP_WRITE,
                                                THUNAR_VFS_FILE_MODE_OTH_WRITE);
}



static void
thunar_file_real_changed (ThunarFile *file)
{
  /* destroy any cached icon, as we need to reload it */
  if (G_LIKELY (file->cached_icon != NULL))
    {
      g_object_unref (G_OBJECT (file->cached_icon));
      file->cached_icon = NULL;
    }
}



static ThunarFile*
thunar_file_new_internal (ThunarVfsURI *uri,
                          GError      **error)
{
  switch (thunar_vfs_uri_get_scheme (uri))
    {
    case THUNAR_VFS_URI_SCHEME_COMPUTER: return thunar_computer_folder_new (uri, error);
    case THUNAR_VFS_URI_SCHEME_FILE:     return thunar_local_file_new (uri, error);
    case THUNAR_VFS_URI_SCHEME_TRASH:    return thunar_trash_folder_new (uri, error);
    }

  g_assert_not_reached ();
  return NULL;
}



static gboolean
thunar_file_denies_access_permission (ThunarFile       *file,
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

  // FIXME: We should add a superuser check here!

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



static void
thunar_file_destroyed (gpointer data,
                       GObject *object)
{
  ThunarVfsURI *uri = THUNAR_VFS_URI (data);

  g_return_if_fail (THUNAR_VFS_IS_URI (uri));

  /* drop the entry from the cache */
  g_hash_table_remove (file_cache, uri);

  /* drop the reference on the uri */
  thunar_vfs_uri_unref (uri);
}



/**
 * _thunar_file_cache_lookup:
 * @uri : a #ThunarVfsURI.
 *
 * Checks if the #ThunarFile which handles @uri is
 * already present in the #ThunarFile cache. Returns
 * a reference to the cached #ThunarFile or %NULL
 * if no matching file is found.
 *
 * No reference on the returned object is taken for
 * the caller.
 *
 * This function is solely intended to be used by
 * implementors of the #ThunarFile class.
 *
 * Return value: the #ThunarFile cached for
 *               @uri or %NULL.
 **/
ThunarFile*
_thunar_file_cache_lookup (ThunarVfsURI *uri)
{
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* allocate the ThunarFile cache on-demand */
  if (G_UNLIKELY (file_cache == NULL))
    {
      file_cache = g_hash_table_new (thunar_vfs_uri_hash,
                                     thunar_vfs_uri_equal);
    }

  return g_hash_table_lookup (file_cache, uri);
}



/**
 * _thunar_file_cache_insert:
 * @file : a #ThunarFile.
 *
 * Inserts the @file into the #ThunarFile cache and
 * removes the floating reference from the @file.
 **/
void
_thunar_file_cache_insert (ThunarFile *file)
{
  ThunarVfsURI *uri;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (file_cache != NULL);

  /* drop the floating reference */
  g_assert (GTK_OBJECT_FLOATING (file));
  g_object_ref (G_OBJECT (file));
  gtk_object_sink (GTK_OBJECT (file));

  /* insert the file into the cache */
  uri = thunar_file_get_uri (file);
  g_object_weak_ref (G_OBJECT (file), thunar_file_destroyed, uri);
  g_hash_table_insert (file_cache, thunar_vfs_uri_ref (uri), file);
}



/**
 * thunar_file_get_for_uri:
 * @uri   : an #ThunarVfsURI instance.
 * @error : error return location.
 *
 * Tries to query the file referred to by @uri. Returns %NULL
 * if the file could not be queried and @error will point
 * to an error describing the problem, else the #ThunarFile
 * instance will be returned, which needs to freed using
 * #g_object_unref() when no longer needed.
 *
 * Note that this function is not thread-safe and may only
 * be called from the main thread.
 *
 * Return value: the #ThunarFile instance or %NULL on error.
 **/
ThunarFile*
thunar_file_get_for_uri (ThunarVfsURI *uri,
                         GError      **error)
{
  ThunarFile *file;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* see if we have the corresponding file cached already */
  file = _thunar_file_cache_lookup (uri);
  if (file == NULL)
    {
      /* allocate the new file object */
      file = thunar_file_new_internal (uri, error);
      if (G_LIKELY (file != NULL))
        _thunar_file_cache_insert (file);
    }
  else
    {
      /* take another reference on the cached file */
      g_object_ref (G_OBJECT (file));
    }

  return file;
}



/**
 * thunar_file_has_parent:
 * @file : a #ThunarFile instance.
 *
 * Checks whether it is possible to determine the parent #ThunarFile
 * for @file.
 *
 * Return value: whether @file has a parent.
 **/
gboolean
thunar_file_has_parent (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return THUNAR_FILE_GET_CLASS (file)->has_parent (file);
}



/**
 * thunar_file_get_parent:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors.
 *
 * Determines the parent #ThunarFile (the directory or virtual folder,
 * which includes @file) for @file. If @file has no parent or the
 * user is not allowed to open the parent folder of @file, %NULL will
 * be returned and @error will be set to point to a #GError that
 * describes the cause. Else, the #ThunarFile will be returned, and
 * the caller must call #g_object_unref() on it.
 *
 * You may want to call #thunar_file_has_parent() first to
 * determine whether @file has a parent.
 *
 * Return value: the parent #ThunarFile or %NULL.
 **/
ThunarFile*
thunar_file_get_parent (ThunarFile *file,
                        GError    **error)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_parent (file, error);
}



/**
 * thunar_file_open_as_folder:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors or %NULL.
 *
 * Tries to open the #ThunarFolder instance corresponding to @file. If @file
 * does not refer to a folder in any way or you don't have permission to open
 * the @file as a folder, %NULL will be returned and @error will be appropriately.
 *
 * You'll need to call #g_object_unref() on the returned instance when you're
 * done with it.
 *
 * Return value: the #ThunarFolder corresponding to @file or %NULL.
 **/
ThunarFolder*
thunar_file_open_as_folder (ThunarFile *file,
                            GError    **error)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return THUNAR_FILE_GET_CLASS (file)->open_as_folder (file, error);
}



/**
 * thunar_file_get_uri:
 * @file  : a #ThunarFile instance.
 *
 * Returns the #ThunarVfsURI, that refers to the location of the @file.
 *
 * This method cannot return %NULL, unless there's a bug in the
 * application. So you should never check the returned value for
 * a possible %NULL pointer, except in the context of a #g_return_if_fail()
 * or #g_assert() macro, which will not be compiled into the final
 * binary.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ThunarVfsURI, so if you need the object for a longer
 * period, you'll need to take a reference yourself using the
 * #thunar_vfs_uri_ref() function.
 *
 * Return value: the URI to the @file.
 **/
ThunarVfsURI*
thunar_file_get_uri (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_uri (file);
}



/**
 * thunar_file_get_mime_info:
 * @file : a #ThunarFile instance.
 *
 * Returns the MIME type information for the given @file object. This
 * function may return %NULL if it is unable to determine a MIME type.
 * Therefore your component must be able to handle this case!
 *
 * This method automatically takes a reference on the returned
 * object for the caller, so you'll need to call #thunar_vfs_mime_info()
 * when you are done with it.
 *
 * Return value: the MIME type or %NULL.
 **/
ThunarVfsMimeInfo*
thunar_file_get_mime_info (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_mime_info (file);
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
thunar_file_get_display_name (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_display_name (file);
}



/**
 * thunar_file_get_special_name:
 * @file : a #ThunarFile instance.
 *
 * Returns the special name for the @file, e.g. "Filesystem" for the #ThunarFile
 * referring to "/" and so on. If there's no special name for this @file, the
 * value of the "display-name" property will be returned instead.
 *
 * Return value: the special name of @file.
 **/
const gchar*
thunar_file_get_special_name (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_special_name (file);
}



/**
 * thunar_file_get_kind:
 * @file : a #ThunarFile instance.
 *
 * Returns the kind of @file.
 *
 * Return value: the kind of @file.
 **/
ThunarVfsFileType
thunar_file_get_kind (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), THUNAR_VFS_FILE_TYPE_UNKNOWN);
  return THUNAR_FILE_GET_CLASS (file)->get_kind (file);
}



/**
 * thunar_file_get_date:
 * @file        : a #ThunarFile instance.
 * @date_type   : the kind of date you are interested in.
 * @date_return : the return location for the date.
 *
 * Queries the given @date_type from @file and stores the result in @date_return.
 * Not all #ThunarFile implementations may support all kinds of @date_type<!---->s,
 * some may not even support dates at all. If the @date_type could not be determined
 * on @file, %FALSE will be returned and @date_type will not be set to the proper
 * value. Else if the operation was successful, %TRUE will be returned.
 *
 * Modules using this method must be prepared to handle the case when %FALSE is
 * returned!
 *
 * Return value: %TRUE if the operation was successful, else %FALSE.
 **/
gboolean
thunar_file_get_date (ThunarFile        *file,
                      ThunarFileDateType date_type,
                      ThunarVfsFileTime *date_return)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (date_type == THUNAR_FILE_DATE_ACCESSED
                     || date_type == THUNAR_FILE_DATE_CHANGED
                     || date_type == THUNAR_FILE_DATE_MODIFIED, FALSE);
  g_return_val_if_fail (date_return != NULL, FALSE);

  return THUNAR_FILE_GET_CLASS (file)->get_date (file, date_type, date_return);
}



/**
 * thunar_file_get_mode:
 * @file : a #ThunarFile instance.
 *
 * Returns the permission bits of @file.
 *
 * Return value: the permission bits of @file.
 **/
ThunarVfsFileMode
thunar_file_get_mode (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), 0);
  return THUNAR_FILE_GET_CLASS (file)->get_mode (file);
}



/**
 * thunar_file_get_size:
 * @file        : a #ThunarFile instance.
 * @size_return : location to store the file size to.
 *
 * Tries to determine the size of @file in bytes. If it
 * was possible to determine the size, %TRUE will be
 * returned and @size_return will be set to the size
 * in bytes. Else %FALSE will be returned, which means
 * that it is not possible to determine the size of
 * @file.
 *
 * Return value: %TRUE if the operation succeeds, else %FALSE.
 **/
gboolean
thunar_file_get_size (ThunarFile        *file,
                      ThunarVfsFileSize *size_return)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (size_return != NULL, FALSE);
  return THUNAR_FILE_GET_CLASS (file)->get_size (file, size_return);
}



/**
 * thunar_file_get_date_string:
 * @file      : a #ThunarFile instance.
 * @date_type : the kind of date you are interested to know about @file.
 *
 * Tries to determine the @date_type of @file, and if @file supports the
 * given @date_type, it'll be formatted as string and returned. The
 * caller is responsible for freeing the string using the #g_free()
 * function.
 *
 * Else if @date_type is not supported for @file, %NULL will be returned
 * and the caller must be able to handle that case.
 *
 * Return value: the @date_type of @file formatted as string or %NULL.
 **/
gchar*
thunar_file_get_date_string (ThunarFile        *file,
                             ThunarFileDateType date_type)
{
  ThunarVfsFileTime time;
#ifdef HAVE_LOCALTIME_R
  struct tm         tmbuf;
#endif
  struct tm        *tm;
  gchar            *result;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (date_type == THUNAR_FILE_DATE_ACCESSED
                     || date_type == THUNAR_FILE_DATE_CHANGED
                     || date_type == THUNAR_FILE_DATE_MODIFIED, NULL);

  /* query the date on the given file */
  if (!thunar_file_get_date (file, date_type, &time))
    return NULL;

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
 * the result using #g_free() when you're done with it.
 *
 * Return value: the mode of @file as string.
 **/
gchar*
thunar_file_get_mode_string (ThunarFile *file)
{
  ThunarVfsFileType kind;
  ThunarVfsFileMode mode;
  gchar            *text;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  kind = THUNAR_FILE_GET_CLASS (file)->get_kind (file);
  mode = THUNAR_FILE_GET_CLASS (file)->get_mode (file);
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
 * format. You'll need to free the result using #g_free()
 * if you're done with it.
 *
 * If it is not possible to determine the size of @file,
 * %NULL will be returned, so the caller must be able to
 * handle this case.
 *
 * Return value: the size of @file in a human readable
 *               format or %NULL.
 **/
gchar*
thunar_file_get_size_string (ThunarFile *file)
{
  ThunarVfsFileSize size;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (!thunar_file_get_size (file, &size))
    return NULL;

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
thunar_file_get_volume (ThunarFile             *file,
                        ThunarVfsVolumeManager *volume_manager)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (volume_manager), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_volume (file, volume_manager);
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
 * using #g_object_unref().
 *
 * Return value: the #ThunarVfsGroup for @file or %NULL.
 **/
ThunarVfsGroup*
thunar_file_get_group (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_group (file);
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
 * using #g_object_unref().
 *
 * Return value: the #ThunarVfsUser for @file or %NULL.
 **/
ThunarVfsUser*
thunar_file_get_user (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_user (file);
}



/**
 * thunar_file_can_execute:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to execute the @file (or enter the directory refered to by
 * @file).
 *
 * If the specific #ThunarFile implementation does not provide
 * a custom #thunar_file_can_execute() method, the fallback
 * method provided by #ThunarFile is used, which determines
 * whether the @file can be executed based on the data provided
 * by #thunar_file_get_mode(), #thunar_file_get_user() and
 * #thunar_file_get_group().
 *
 * Return value: %TRUE if @file can be executed.
 **/
gboolean
thunar_file_can_execute (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return THUNAR_FILE_GET_CLASS (file)->can_execute (file);
}



/**
 * thunar_file_can_read:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to read the @file.
 *
 * If the specific #ThunarFile implementation does not provide
 * a custom #thunar_file_can_read() method, the fallback
 * method provided by #ThunarFile is used, which determines
 * whether the @file can be read based on the data provided
 * by #thunar_file_get_mode(), #thunar_file_get_user() and
 * #thunar_file_get_group().
 *
 * Return value: %TRUE if @file can be read.
 **/
gboolean
thunar_file_can_read (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return THUNAR_FILE_GET_CLASS (file)->can_read (file);
}



/**
 * thunar_file_can_write:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to write the @file.
 *
 * If the specific #ThunarFile implementation does not provide
 * a custom #thunar_file_can_write() method, the fallback
 * method provided by #ThunarFile is used, which determines
 * whether the @file can be written based on the data provided
 * by #thunar_file_get_mode(), #thunar_file_get_user() and
 * #thunar_file_get_group().
 *
 * Return value: %TRUE if @file can be read.
 **/
gboolean
thunar_file_can_write (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  return THUNAR_FILE_GET_CLASS (file)->can_write (file);
}



/**
 * thunar_file_get_emblem_names:
 * @file : a #ThunarFile instance.
 *
 * Determines the names of the emblems that should be displayed for
 * @file. The returned list is owned by the caller, but the list
 * items - the name strings - are owned by @file. So the caller
 * must call #g_list_free(), but don't #g_free() the list items.
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
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return THUNAR_FILE_GET_CLASS (file)->get_emblem_names (file);
}



/**
 * thunar_file_load_icon:
 * @file         : a #ThunarFile instance.
 * @icon_factory : the #ThunarIconFactory, which should be used to load the icon.
 * @size         : the icon size in pixels.
 *
 * Loads an icon from @icon_factory for @file at @size.
 *
 * You need to call #g_object_unref() on the returned icon
 * when you don't need it any longer.
 *
 * Return value: the icon for @file at @size.
 **/
GdkPixbuf*
thunar_file_load_icon (ThunarFile        *file,
                       ThunarIconFactory *icon_factory,
                       gint               size)
{
  GtkIconTheme *icon_theme;
  const gchar  *icon_name;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* see if we have a cached icon that matches the request */
  if (file->cached_icon == NULL || file->cached_size != size)
    {
      /* drop any previous icon */
      if (file->cached_icon != NULL)
        g_object_unref (G_OBJECT (file->cached_icon));

      /* load the icon */
      icon_theme = thunar_icon_factory_get_icon_theme (icon_factory);
      icon_name = THUNAR_FILE_GET_CLASS (file)->get_icon_name (file, icon_theme);
      file->cached_icon = thunar_icon_factory_load_icon (icon_factory, icon_name, size, NULL, TRUE);
    }

  /* take a reference on the cached icon for the caller */
  g_object_ref (G_OBJECT (file->cached_icon));
  return file->cached_icon;
}



/**
 * thunar_file_watch:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to watch itself for changes. Not all #ThunarFile
 * implementations must support this, but if a #ThunarFile
 * implementation implements the #thunar_file_watch() method,
 * it must also implement the #thunar_file_unwatch() method.
 *
 * The #ThunarFile base class implements automatic "ref
 * counting" for watches, that says, you can call #thunar_file_watch()
 * multiple times, but the virtual method will be invoked only
 * once. This also means that you MUST call #thunar_file_unwatch()
 * for every #thunar_file_watch() invokation, else the application
 * will abort.
 **/
void
thunar_file_watch (ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (file->watch_count >= 0);

  if (++file->watch_count == 1 && THUNAR_FILE_GET_CLASS (file)->watch != NULL)
    {
      g_return_if_fail (THUNAR_FILE_GET_CLASS (file)->unwatch != NULL);
      THUNAR_FILE_GET_CLASS (file)->watch (file);
    }
}



/**
 * thunar_file_unwatch:
 * @file : a #ThunarFile instance.
 *
 * See #thunar_file_watch() for a description of how watching
 * #ThunarFile<!---->s works.
 **/
void
thunar_file_unwatch (ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (file->watch_count > 0);

  if (--file->watch_count == 0 && THUNAR_FILE_GET_CLASS (file)->unwatch != NULL)
    {
      g_return_if_fail (THUNAR_FILE_GET_CLASS (file)->watch != NULL);
      THUNAR_FILE_GET_CLASS (file)->unwatch (file);
    }
}



/**
 * thunar_file_reload:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to reload its internal state, e.g. by reacquiring
 * the file info from the underlying media. Not all #ThunarFile
 * implementations may actually implement this method, so don't
 * count on it to do anything useful. Some implementations may
 * also decide to reload its state asynchronously.
 *
 * You must also be able to handle the case that @file is
 * destroyed during the reload call.
 **/
void
thunar_file_reload (ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  THUNAR_FILE_GET_CLASS (file)->reload (file);
}


 
/**
 * thunar_file_changed:
 * @file : a #ThunarFile instance.
 *
 * Emits the ::changed signal on @file. This function is meant to be called
 * by derived classes whenever they notice changes to the @file.
 **/
void
thunar_file_changed (ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_signal_emit (G_OBJECT (file), file_signals[CHANGED], 0);
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
thunar_file_is_hidden (ThunarFile *file)
{
  const gchar *p;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  p = THUNAR_FILE_GET_CLASS (file)->get_display_name (file);
  if (*p != '.' && *p != '\0')
    {
      for (; p[1] != '\0'; ++p)
        ;
      return (*p == '~');
    }

  return TRUE;
}




/**
 * thunar_file_list_dup:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Returns a deep-copy of @file_list, which must be
 * freed using #thunar_file_list_free().
 *
 * Return value: a deep copy of @file_list.
 **/
GList*
thunar_file_list_dup (const GList *file_list)
{
  GList *list = g_list_copy ((GList *) file_list);
  g_list_foreach (list, (GFunc) g_object_ref, NULL);
  return list;
}



/**
 * thunar_file_list_free:
 * @file_list : a list of #ThunarFile<!---->s.
 *
 * Unrefs the #ThunarFile<!---->s contained in @file_list
 * and frees the list itself.
 **/
void
thunar_file_list_free (GList *file_list)
{
  g_list_foreach (file_list, (GFunc) g_object_unref, NULL);
  g_list_free (file_list);
}


