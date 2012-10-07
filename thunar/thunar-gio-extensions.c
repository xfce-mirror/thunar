/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gunixmounts.h>
#endif

#include <exo/exo.h>
#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-file.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



GFile *
thunar_g_file_new_for_home (void)
{
  return g_file_new_for_path (xfce_get_homedir ());
}



GFile *
thunar_g_file_new_for_root (void)
{
  return g_file_new_for_uri ("file:///");
}



GFile *
thunar_g_file_new_for_trash (void)
{
  return g_file_new_for_uri ("trash:///");
}



GFile *
thunar_g_file_new_for_desktop (void)
{
  return g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
}



GFile *
thunar_g_file_new_for_user_special_dir (GUserDirectory dir)
{
  const gchar *path;

  _thunar_return_val_if_fail (dir < G_USER_N_DIRECTORIES, NULL);

  path = g_get_user_special_dir (dir);
  if (path == NULL)
    path = xfce_get_homedir ();

  return g_file_new_for_path (path);
}



gboolean
thunar_g_file_is_root (GFile *file)
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
thunar_g_file_is_trashed (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "trash");
}



gboolean
thunar_g_file_is_home (GFile *file)
{
  GFile   *home;
  gboolean is_home = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  home = thunar_g_file_new_for_home ();
  is_home = g_file_equal (home, file);
  g_object_unref (home);

  return is_home;
}



GKeyFile *
thunar_g_file_query_key_file (GFile              *file,
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
  if (G_LIKELY (length == 0
      || g_key_file_load_from_data (key_file, contents, length,
                                    G_KEY_FILE_KEEP_COMMENTS
                                    | G_KEY_FILE_KEEP_TRANSLATIONS,
                                    error)))
    {
      g_free (contents);
      return key_file;
    }
  else
    {
      g_free (contents);
      g_key_file_free (key_file);
      return NULL;
    }
}



gboolean
thunar_g_file_write_key_file (GFile        *file,
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
                                G_FILE_CREATE_REPLACE_DESTINATION,
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
thunar_g_file_get_location (GFile *file)
{
  gchar *location;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  
  location = g_file_get_path (file);
  if (location == NULL)
    location = g_file_get_uri (file);

  return location;
}



gchar *
thunar_g_file_get_display_name (GFile *file)
{
  gchar *base_name;
  gchar *display_name;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  base_name = g_file_get_basename (file);
  if (G_LIKELY (base_name != NULL))
    {
      if (strcmp (base_name, "/") == 0)
        {
          display_name = g_strdup (_("File System"));
          g_free (base_name);
        }
      else if (g_utf8_validate (base_name, -1, NULL))
       {
         display_name = base_name;
       }
     else
       {
         display_name = g_uri_escape_string (base_name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
         g_free (base_name);
       }
   }
 else
   {
     display_name = g_strdup ("?");
   }

  return display_name;
}



gboolean
thunar_g_vfs_is_uri_scheme_supported (const gchar *scheme)
{
  const gchar * const *supported_schemes;
  gboolean             supported = FALSE;
  guint                n;
  GVfs                *gvfs;

  _thunar_return_val_if_fail (scheme != NULL && *scheme != '\0', FALSE);

  gvfs = g_vfs_get_default ();
  supported_schemes = g_vfs_get_supported_uri_schemes (gvfs);

  if (supported_schemes == NULL)
    return FALSE;

  for (n = 0; !supported && supported_schemes[n] != NULL; ++n) 
    if (g_strcmp0 (supported_schemes[n], scheme) == 0)
      supported = TRUE;

  return supported;
}



GType
thunar_g_file_list_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarGFileList"),
                                           (GBoxedCopyFunc) thunar_g_file_list_copy,
                                           (GBoxedFreeFunc) thunar_g_file_list_free);
    }

  return type;
}



/**
 * thunar_g_file_list_new_from_string:
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
thunar_g_file_list_new_from_string (const gchar *string)
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
 * thunar_g_file_list_to_stringv:
 * @list : a list of #GFile<!---->s.
 *
 * Free the returned value using g_strfreev() when you
 * are done with it. Useful for gtk_selection_data_set_uris.
 *
 * Return value: and array of uris.
 **/
gchar **
thunar_g_file_list_to_stringv (GList *list)
{
  gchar **uris;
  guint   n;
  GList  *lp;

  /* allocate initial string */
  uris = g_new0 (gchar *, g_list_length (list) + 1);

  for (lp = list, n = 0; lp != NULL; lp = lp->next)
    uris[n++] = g_file_get_uri (G_FILE (lp->data));

  return uris;
}



GList *
thunar_g_file_list_append (GList *list,
                           GFile *file)
{
  return g_list_append (list, g_object_ref (file));
}



GList *
thunar_g_file_list_prepend (GList *list,
                            GFile *file)
{
  return g_list_prepend (list, g_object_ref (file));
}



/**
 * thunar_g_file_list_copy:
 * @list : a list of #GFile<!---->s.
 *
 * Takes a deep copy of @list and returns the
 * result. The caller is responsible to free the
 * returned list using thunar_g_file_list_free().
 *
 * Return value: a deep copy of @list.
 **/
GList*
thunar_g_file_list_copy (GList *list)
{
  GList *copy = NULL;
  GList *lp;

  for (lp = g_list_last (list); lp != NULL; lp = lp->prev)
    copy = g_list_prepend (copy, g_object_ref (lp->data));

  return copy;
}



/**
 * thunar_g_file_list_free:
 * @list : a list of #GFile<!---->s.
 *
 * Frees the #GFile<!---->s in @list and
 * the @list itself.
 **/
void
thunar_g_file_list_free (GList *list)
{
  GList *lp;
  for (lp = list; lp != NULL; lp = lp->next)
    g_object_unref (lp->data);
  g_list_free (list);
}



gboolean
thunar_g_app_info_launch (GAppInfo          *info,
                          GFile             *working_directory,
                          GList             *path_list,
                          GAppLaunchContext *context,
                          GError           **error)
{
  gboolean result = FALSE;
  gchar   *new_path = NULL;
  gchar   *old_path = NULL;

  _thunar_return_val_if_fail (G_IS_APP_INFO (info), FALSE);
  _thunar_return_val_if_fail (working_directory == NULL || G_IS_FILE (working_directory), FALSE);
  _thunar_return_val_if_fail (path_list != NULL, FALSE);
  _thunar_return_val_if_fail (G_IS_APP_LAUNCH_CONTEXT (context), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if we want to set the working directory of the spawned app */
  if (working_directory != NULL)
    {
      /* determine the working directory path */
      new_path = g_file_get_path (working_directory);
      if (new_path != NULL)
        {
          /* switch to the desired working directory, remember that of Thunar itself */
          old_path = thunar_util_change_working_directory (new_path);

          /* forget about the new working directory path */
          g_free (new_path);
        }
    }

  /* launch the paths with the specified app info */
  result = g_app_info_launch (info, path_list, context, error);

  /* check if we need to reset the working directory to the one Thunar was
   * opened from */
  if (old_path != NULL)
    {
      /* switch to Thunar's original working directory */
      new_path = thunar_util_change_working_directory (old_path);

      /* clean up */
      g_free (new_path);
      g_free (old_path);
    }

  return result;
}

