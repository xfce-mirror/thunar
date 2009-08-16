/* $Id$ */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>

#include <exo/exo.h>
#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-private.h>



GFile *
g_file_new_for_home (void)
{
  return g_file_new_for_path (xfce_get_homedir ());
}



GFile *
g_file_new_for_root (void)
{
  return g_file_new_for_uri ("file:///");
}



GFile *
g_file_new_for_trash (void)
{
  return g_file_new_for_uri ("trash:///");
}



GFile *
g_file_new_for_desktop (void)
{
  return g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
}



GFile *
g_file_new_for_user_special_dir (GUserDirectory dir)
{
  const gchar *path;

  _thunar_return_val_if_fail (dir >= 0 && dir < G_USER_N_DIRECTORIES, NULL);

  path = g_get_user_special_dir (dir);
  if (path == NULL)
    path = xfce_get_homedir ();

  return g_file_new_for_path (path);
}



gboolean
g_file_is_root (GFile *file)
{
  GFile   *parent;
  gboolean is_root = TRUE;

  parent = g_file_get_parent (file);
  if (G_UNLIKELY (parent != NULL))
    {
      is_root = FALSE;
      g_object_unref (parent);
    }

  return is_root;
}



gboolean 
g_file_is_trashed (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "trash");
}



gboolean
g_file_is_desktop (GFile *file)
{
  GFile   *desktop;
  gboolean is_desktop;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  desktop = g_file_new_for_desktop ();
  is_desktop = g_file_equal (desktop, file);
  g_object_unref (desktop);

  return is_desktop;
}



GKeyFile *
g_file_query_key_file (GFile              *file,
                       GCancellable       *cancellable,
                       GError            **error)
{
  GKeyFile *key_file;
  gchar    *contents = NULL;
  gsize     length;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to load the entire file into memory */
  if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, error))
    return NULL;

  /* allocate a new key file */
  key_file = g_key_file_new ();

  /* try to parse the key file from the contents of the file */
  if (!g_key_file_load_from_data (key_file, contents, length, 
                                  G_KEY_FILE_KEEP_COMMENTS 
                                  | G_KEY_FILE_KEEP_TRANSLATIONS,
                                  error))
    {
      g_free (contents);
      g_key_file_free (key_file);
      return NULL;
    }
  else
    {
      g_free (contents);
      return key_file;
    }
}



gboolean
g_file_write_key_file (GFile        *file,
                       GKeyFile     *key_file,
                       GCancellable *cancellable,
                       GError      **error)
{
  gchar *contents;
  gsize  length;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (key_file != NULL, FALSE);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* write the key file into the contents buffer */
  contents = g_key_file_to_data (key_file, &length, NULL);

  /* try to replace the file contents with the key file data */
  if (!g_file_replace_contents (file, contents, length, NULL, FALSE, 
#if GLIB_CHECK_VERSION(2,20,0)
                                G_FILE_CREATE_REPLACE_DESTINATION,
#else
                                G_FILE_CREATE_NONE,
#endif
                                NULL, cancellable, error))
    {
      g_free (contents);
      return FALSE;
    }
  else
    {
      g_free (contents);
      return TRUE;
    }
}



gchar *
g_file_get_location (GFile *file)
{
  gchar *location;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  
  location = g_file_get_path (file);
  if (location == NULL)
    location = g_file_get_uri (file);

  return location;
}



GType
g_file_list_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("GFileList"),
                                           (GBoxedCopyFunc) g_file_list_copy,
                                           (GBoxedFreeFunc) g_file_list_free);
    }

  return type;
}



/**
 * g_file_list_new_from_string:
 * @string : a string representation of an URI list.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace. The resulting
 * list will hold one #GFile for each URI.
 *
 * If @string contains no URIs, this function
 * will return %NULL.
 *
 * Return value: the list of #GFile<!---->s or %NULL.
 **/
GList *
g_file_list_new_from_string (const gchar *string)
{
  GList  *list = NULL;
  gchar **uris;
  gsize   n;

  uris = g_uri_list_extract_uris (string);

  for (n = 0; uris != NULL && uris[n] != NULL; ++n)
    list = g_list_append (list, g_file_new_for_uri (uris[n]));

  g_strfreev (uris);

  return list;
}



/**
 * g_file_list_to_string:
 * @list : a list of #GFile<!---->s.
 *
 * Free the returned value using g_free() when you
 * are done with it.
 *
 * Return value: the string representation of @list conforming to the
 *               text/uri-list mime type defined in RFC 2483.
 **/
gchar *
g_file_list_to_string (GList *list)
{
  GString *string;
  gchar   *uri;
  GList   *lp;

  /* allocate initial string */
  string = g_string_new (NULL);

  for (lp = list; lp != NULL; lp = lp->next)
    {
      uri = g_file_get_uri (lp->data);
      string = g_string_append (string, uri);
      g_free (uri);

      string = g_string_append (string, "\r\n");
    }

  return g_string_free (string, FALSE);
}



GList *
g_file_list_append (GList *list,
                    GFile *file)
{
  return g_list_append (list, g_object_ref (file));
}



GList *
g_file_list_prepend (GList *list,
                     GFile *file)
{
  return g_list_prepend (list, g_object_ref (file));
}



/**
 * g_file_list_copy:
 * @list : a list of #GFile<!---->s.
 *
 * Takes a deep copy of @list and returns the
 * result. The caller is responsible to free the
 * returned list using g_file_list_free().
 *
 * Return value: a deep copy of @list.
 **/
GList*
g_file_list_copy (GList *list)
{
  GList *copy = NULL;
  GList *lp;

  for (lp = g_list_last (list); lp != NULL; lp = lp->prev)
    copy = g_list_prepend (copy, g_object_ref (lp->data));

  return copy;
}



/**
 * g_file_list_free:
 * @list : a list of #GFile<!---->s.
 *
 * Frees the #GFile<!---->s in @list and
 * the @list itself.
 **/
void
g_file_list_free (GList *list)
{
  GList *lp;
  for (lp = list; lp != NULL; lp = lp->next)
    g_object_unref (lp->data);
  g_list_free (list);
}



gboolean
g_volume_is_removable (GVolume *volume)
{
  gboolean can_eject = FALSE;
  gboolean can_mount = FALSE;
  gboolean can_unmount = FALSE;
  gboolean is_removable = FALSE;
  GDrive  *drive;
  GMount  *mount;

  _thunar_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  /* check if the volume can be ejected */
  can_eject = g_volume_can_eject (volume);

  /* determine the drive for the volume */
  drive = g_volume_get_drive (volume);
  if (drive != NULL)
    {
      /*check if the drive media can be removed */
      is_removable = g_drive_is_media_removable (drive);

      /* release the drive */
      g_object_unref (drive);
    }

  /* determine the mount for the volume (if it is mounted at all) */
  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      /* check if the volume can be unmounted */
      can_unmount = g_mount_can_unmount (mount);

      /* release the mount */
      g_object_unref (mount);
    }

  /* determine whether the device can be mounted */
  can_mount = g_volume_can_mount (volume);

  return can_eject || can_unmount || is_removable || can_mount;
}



gboolean
g_volume_is_mounted (GVolume *volume)
{
  gboolean is_mounted = FALSE;
  GMount  *mount;

  _thunar_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  /* determine the mount for this volume (if it is mounted at all) */
  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      is_mounted = TRUE;
      g_object_unref (mount);
    }

  return is_mounted;
}



gboolean 
g_volume_is_present (GVolume *volume)
{
  gboolean has_media = FALSE;
  GDrive  *drive;

  _thunar_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  drive = g_volume_get_drive (volume);
  if (drive != NULL)
    {
      has_media = g_drive_has_media (drive);
      g_object_unref (drive);
    }

  return has_media;
}



gboolean
g_mount_is_same_drive (GMount *mount,
                       GMount *other)
{
  gboolean same_drive = FALSE;
  GDrive  *mount_drive;
  GDrive  *other_drive;

  _thunar_return_val_if_fail (mount == NULL || G_IS_MOUNT (mount), FALSE);
  _thunar_return_val_if_fail (other == NULL || G_IS_MOUNT (other), FALSE);

  if (mount == NULL || other == NULL)
    return FALSE;

  mount_drive = g_mount_get_drive (mount);
  other_drive = g_mount_get_drive (other);

  if (mount_drive != NULL && other_drive != NULL)
    {
      same_drive = (mount_drive == other_drive);
    }

  if (mount_drive != NULL)
    g_object_unref (mount_drive);
  
  if (other_drive != NULL)
    g_object_unref (other_drive);

  return same_drive;
}



#if !GLIB_CHECK_VERSION(2,18,0)
GFileType *
g_file_query_file_type (GFile              *file, 
                        GFileQueryInfoFlags flags
                        GCancellable       *cancellable)
{
  GFileInfo *info;
  GFileType  file_type = G_FILE_TYPE_UNKNOWN;

  _thunar_return_val_if_fail (G_IS_FILE (file), G_FILE_TYPE_UNKNOWN);

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, flags, 
                            cancellable, NULL);
  if (info != NULL)
    {
      file_type = g_file_info_get_file_type (info);
      g_object_unref (info);
    }

  return file_type;
}



GFileMonitor *
g_file_monitor (GFile            *file,
	              GFileMonitorFlags flags,
		            GCancellable     *cancellable,
		            GError          **error)
{
  GFileType file_type;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  file_type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, cancelllable);

  if (file_type == G_FILE_TYPE_DIRECTORY)
    return g_file_monitor_directory (file, flags, cancellable, error);
  else
    return g_file_onitor_file (file, flags, cancellable, error);
}



/**
 * Copied from http://git.gnome.org/cgit/glib/plain/gio/gfile.c
 * Copyright (c) 2006-2007 Red Hat, Inc.
 * Author: Alexander Larsson <alexl@redhat.com>
 */
gboolean
g_file_make_directory_with_parents (GFile        *file,
                                    GCancellable *cancellable,
                                    GError      **error)
{
  gboolean result;
  GFile *parent_file, *work_file;
  GList *list = NULL, *l;
  GError *my_error = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  result = g_file_make_directory (file, cancellable, &my_error);
  if (result || my_error->code != G_IO_ERROR_NOT_FOUND) 
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return result;
    }
  
  work_file = file;
  
  while (!result && my_error->code == G_IO_ERROR_NOT_FOUND) 
    {
      g_clear_error (&my_error);
    
      parent_file = g_file_get_parent (work_file);
      if (parent_file == NULL)
        break;
      result = g_file_make_directory (parent_file, cancellable, &my_error);
    
      if (!result && my_error->code == G_IO_ERROR_NOT_FOUND)
        list = g_list_prepend (list, parent_file);

      work_file = parent_file;
    }

  for (l = list; result && l; l = l->next)
    {
      result = g_file_make_directory ((GFile *) l->data, cancellable, &my_error);
    }
  
  /* Clean up */
  while (list != NULL) 
    {
      g_object_unref ((GFile *) list->data);
      list = g_list_remove (list, list->data);
    }

  if (!result) 
    {
      g_propagate_error (error, my_error);
      return result;
    }
  
  return g_file_make_directory (file, cancellable, error);
}
#endif /* !GLIB_CHECK_VERSION(2,18,0) */
