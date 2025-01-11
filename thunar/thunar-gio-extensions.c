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

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#ifdef HAVE_GIO_UNIX

#ifdef __linux__

#define HAVE_DIRECT_IO 1
#include <fcntl.h>
#include <gio/gunixinputstream.h>

#if HAVE_STATX
#include <sys/stat.h>
#ifdef STATX_DIOALIGN
#define HAVE_STATX_DIOALIGN 1
#endif /* STATX_DIOALIGN */
#endif /* HAVE_STATX */

#endif /* __linux__ */

#include <gio/gdesktopappinfo.h>
#include <gio/gunixmounts.h>

#endif /* HAVE_GIO_UNIX */

#include "thunar/thunar-file.h"
#include "thunar/thunar-gio-extensions.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-util.h"

#include <libxfce4util/libxfce4util.h>

#define CMP_BUF_MIN_ALIGN (16)
#define CMP_BUF_SIZE (1024 * 512)

#ifndef O_BINARY
#define O_BINARY (0)
#endif



/* See : https://freedesktop.org/wiki/Specifications/icon-naming-spec/ */
static struct
{
  const gchar *icon_name;
  const gchar *type;
}

/* clang-format off */
device_icon_name [] =
{
  /* Implementation specific */
  { "multimedia-player-apple-ipod-touch" , N_("iPod touch") },
  { "computer-apple-ipad"                , N_("iPad") },
  { "phone-apple-iphone"                 , N_("iPhone") },
  { "drive-harddisk-solidstate"          , N_("Solid State Drive") },
  { "drive-harddisk-system"              , N_("System Drive") },
  { "drive-harddisk-usb"                 , N_("USB Drive") },
  { "drive-removable-media-usb"          , N_("USB Drive") },

  /* Freedesktop icon-naming-spec */
  { "camera-photo*"          , N_("Camera") },
  { "drive-harddisk*"        , N_("Harddisk") },
  { "drive-optical*"         , N_("Optical Drive") },
  { "drive-removable-media*" , N_("Removable Drive") },
  { "media-flash*"           , N_("Flash Drive") },
  { "media-floppy*"          , N_("Floppy") },
  { "media-optical*"         , N_("Optical Media") },
  { "media-tape*"            , N_("Tape") },
  { "multimedia-player*"     , N_("Multimedia Player") },
  { "pda*"                   , N_("PDA") },
  { "phone*"                 , N_("Phone") },
  { NULL                     , NULL }
};
/* clang-format on */



static const gchar *
guess_device_type_from_icon_name (const gchar *icon_name);
static GFileInfo *
thunar_g_file_get_content_type_querry_info (GFile  *gfile,
                                            GError *err);
static void
thunar_g_file_info_set_attribute (GFileInfo   *info,
                                  ThunarGType  type,
                                  const gchar *setting_name,
                                  const gchar *setting_value);

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
thunar_g_file_new_for_recent (void)
{
  return g_file_new_for_uri ("recent:///");
}



GFile *
thunar_g_file_new_for_trash (void)
{
  return g_file_new_for_uri ("trash:///");
}



GFile *
thunar_g_file_new_for_computer (void)
{
  return g_file_new_for_uri ("computer://");
}



GFile *
thunar_g_file_new_for_network (void)
{
  return g_file_new_for_uri ("network://");
}



GFile *
thunar_g_file_new_for_desktop (void)
{
  return g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
}



GFile *
thunar_g_file_new_for_bookmarks (void)
{
  gchar *filename;
  GFile *bookmarks;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  bookmarks = g_file_new_for_path (filename);
  g_free (filename);

  return bookmarks;
}



/**
 * thunar_g_file_resolve_symlink:
 * @file : a #GFile.
 *
 * Returns the resolved symlink target of @file as a new #GFile.
 * If @file is not a symlink, @file will just be returned
 *
 * Return value: (nullable) (transfer full): A #GFile on success and %NULL on failure.
 **/
GFile *
thunar_g_file_resolve_symlink (GFile *file)
{
  gchar *basename;
  GFile *parent = NULL;
  GFile *target = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  parent = g_file_get_parent (file);
  if (parent == NULL)
    return NULL;

  basename = g_file_get_basename (file);
  if (basename == NULL)
    {
      g_object_unref (parent);
      return NULL;
    }

  target = g_file_resolve_relative_path (parent, basename);
  g_object_unref (parent);
  g_free (basename);
  return target;
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
thunar_g_file_is_in_recent (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "recent");
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



gboolean
thunar_g_file_is_trash (GFile *file)
{
  char    *uri;
  gboolean is_trash = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_trash = g_strcmp0 (uri, "trash:///") == 0;
  g_free (uri);

  return is_trash;
}



gboolean
thunar_g_file_is_recent (GFile *file)
{
  char    *uri;
  gboolean is_recent = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_recent = g_strcmp0 (uri, "recent:///") == 0;
  g_free (uri);

  return is_recent;
}



gboolean
thunar_g_file_is_computer (GFile *file)
{
  char    *uri;
  gboolean is_computer = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_computer = g_strcmp0 (uri, "computer:///") == 0;
  g_free (uri);

  return is_computer;
}



gboolean
thunar_g_file_is_network (GFile *file)
{
  char    *uri;
  gboolean is_network = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_network = g_strcmp0 (uri, "network:///") == 0;
  g_free (uri);

  return is_network;
}



GKeyFile *
thunar_g_file_query_key_file (GFile        *file,
                              GCancellable *cancellable,
                              GError      **error)
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
  gchar   *contents;
  gsize    length;
  gboolean result = TRUE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (key_file != NULL, FALSE);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* write the key file into the contents buffer */
  contents = g_key_file_to_data (key_file, &length, NULL);

  /* try to replace the file contents with the key file data */
  if (contents != NULL)
    {
      result = g_file_replace_contents (file, contents, length, NULL, FALSE,
                                        G_FILE_CREATE_NONE,
                                        NULL, cancellable, error);

      /* cleanup */
      g_free (contents);
    }

  return result;
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



static const gchar *
guess_device_type_from_icon_name (const gchar *icon_name)
{
  for (int i = 0; device_icon_name[i].type != NULL; i++)
    {
      if (g_pattern_match_simple (device_icon_name[i].icon_name, icon_name))
        return g_dgettext (NULL, device_icon_name[i].type);
    }
  return NULL;
}



/**
 * thunar_g_file_guess_device_type:
 * @file : a #GFile instance.
 *
 * Returns : (transfer none) (nullable): the string of the device type.
 */
const gchar *
thunar_g_file_guess_device_type (GFile *file)
{
  GFileInfo   *fileinfo = NULL;
  GIcon       *icon = NULL;
  const gchar *icon_name = NULL;
  const gchar *device_type = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  fileinfo = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_ICON,
                                G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (fileinfo == NULL)
    return NULL;

  icon = G_ICON (g_file_info_get_attribute_object (fileinfo, G_FILE_ATTRIBUTE_STANDARD_ICON));
  if (!G_IS_THEMED_ICON (icon))
    {
      g_object_unref (fileinfo);
      return NULL;
    }

  icon_name = g_themed_icon_get_names (G_THEMED_ICON (icon))[0];
  device_type = guess_device_type_from_icon_name (icon_name);
  g_object_unref (fileinfo);

  return device_type;
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
        display_name = g_strdup (_("File System"));
      else if (thunar_g_file_is_trash (file))
        display_name = g_strdup (_("Trash"));
      else if (g_utf8_validate (base_name, -1, NULL))
        display_name = g_strdup (base_name);
      else
        display_name = g_uri_escape_string (base_name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);

      g_free (base_name);
    }
  else
    {
      display_name = g_strdup ("?");
    }

  return display_name;
}



gchar *
thunar_g_file_get_display_name_remote (GFile *mount_point)
{
  gchar       *scheme;
  gchar       *parse_name;
  const gchar *p;
  const gchar *path;
  gchar       *unescaped;
  gchar       *hostname;
  gchar       *display_name = NULL;
  const gchar *skip;
  const gchar *firstdot;
  const gchar  skip_chars[] = ":@";
  guint        n;

  _thunar_return_val_if_fail (G_IS_FILE (mount_point), NULL);

  /* not intended for local mounts */
  if (!g_file_is_native (mount_point))
    {
      scheme = g_file_get_uri_scheme (mount_point);
      parse_name = g_file_get_parse_name (mount_point);

      if (scheme != NULL && g_str_has_prefix (parse_name, scheme))
        {
          /* extract the hostname */
          p = parse_name + strlen (scheme);
          while (*p == ':' || *p == '/')
            ++p;

          /* goto path part */
          path = strchr (p, '/');
          firstdot = strchr (p, '.');

          if (firstdot != NULL)
            {
              /* skip password or login names in the hostname */
              for (n = 0; n < G_N_ELEMENTS (skip_chars) - 1; n++)
                {
                  skip = strchr (p, skip_chars[n]);
                  if (skip != NULL
                      && (path == NULL || skip < path)
                      && (skip < firstdot))
                    p = skip + 1;
                }
            }

          /* extract the path and hostname from the string */
          if (G_LIKELY (path != NULL))
            {
              hostname = g_strndup (p, path - p);
            }
          else
            {
              hostname = g_strdup (p);
              path = "/";
            }

          /* unescape the path so that spaces and other characters are shown correctly */
          unescaped = g_uri_unescape_string (path, NULL);

          /* TRANSLATORS: this will result in "<path> on <hostname>" */
          display_name = g_strdup_printf (_("%s on %s"), unescaped, hostname);

          g_free (unescaped);
          g_free (hostname);
        }

      if (scheme != NULL)
        g_free (scheme);
      g_free (parse_name);
    }

  /* never return null */
  if (display_name == NULL)
    display_name = thunar_g_file_get_display_name (mount_point);

  return display_name;
}



gboolean
thunar_g_vfs_is_uri_scheme_supported (const gchar *scheme)
{
  const gchar *const *supported_schemes;
  gboolean            supported = FALSE;
  guint               n;
  GVfs               *gvfs;

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



/**
 * thunar_g_file_get_free_space:
 * @file           : a #GFile instance.
 * @fs_free_return : return location for the amount of
 *                   free space or %NULL.
 * @fs_size_return : return location for the total volume size.
 *
 * Determines the amount of free space of the volume on
 * which @file resides. Returns %TRUE if the amount of
 * free space was determined successfully and placed into
 * @free_space_return, else %FALSE will be returned.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
thunar_g_file_get_free_space (GFile   *file,
                              guint64 *fs_free_return,
                              guint64 *fs_size_return)
{
  GFileInfo *filesystem_info;
  gboolean   success = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  filesystem_info = g_file_query_filesystem_info (file,
                                                  THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                                  NULL, NULL);

  if (filesystem_info != NULL)
    {
      if (fs_free_return != NULL)
        {
          *fs_free_return = g_file_info_get_attribute_uint64 (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
          success = g_file_info_has_attribute (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        }

      if (fs_size_return != NULL)
        {
          *fs_size_return = g_file_info_get_attribute_uint64 (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
          success = g_file_info_has_attribute (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        }

      g_object_unref (filesystem_info);
    }

  return success;
}



gchar *
thunar_g_file_get_free_space_string (GFile *file, gboolean file_size_binary)
{
  gchar  *fs_size_free_str;
  gchar  *fs_size_used_str;
  guint64 fs_size_free;
  guint64 fs_size_total;
  gchar  *free_space_string = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  if (thunar_g_file_get_free_space (file, &fs_size_free, &fs_size_total) && fs_size_total > 0)
    {
      fs_size_free_str = g_format_size_full (fs_size_free, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
      fs_size_used_str = g_format_size_full (fs_size_total - fs_size_free, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);

      free_space_string = g_strdup_printf (_("%s used (%.0f%%)  |  %s free (%.0f%%)"),
                                           fs_size_used_str, ((fs_size_total - fs_size_free) * 100.0 / fs_size_total),
                                           fs_size_free_str, (fs_size_free * 100.0 / fs_size_total));

      g_free (fs_size_free_str);
      g_free (fs_size_used_str);
    }

  return free_space_string;
}



GType
thunar_g_file_list_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_ ("ThunarGFileList"),
                                           (GBoxedCopyFunc) thunar_g_list_copy_deep,
                                           (GBoxedFreeFunc) thunar_g_list_free_full);
    }

  return type;
}



static GHashTable *
thunar_g_hastable_copy_deep (GHashTable *table)
{
  GHashTable    *copy = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  GHashTableIter iter;
  gpointer       key;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_hash_table_add (copy, g_object_ref (key));
  return copy;
}



GType
thunar_g_file_hastable_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_ ("ThunarGHashTableList"),
                                           (GBoxedCopyFunc) thunar_g_hastable_copy_deep,
                                           (GBoxedFreeFunc) g_hash_table_destroy);
    }

  return type;
}



/**
 * thunar_g_file_copy:
 * @source                 : input #GFile
 * @destination            : destination #GFile
 * @flags                  : set of #GFileCopyFlags
 * @use_partial            : option to use *.partial~
 * @cancellable            : (nullable): optional #GCancellable object
 * @progress_callback      : (nullable) (scope call): function to callback with progress information
 * @progress_callback_data : (clousure): user data to pass to @progress_callback
 * @error                  : (nullable): #GError to set on error
 *
 * Calls g_file_copy() if @use_partial is not enabled.
 * If enabled, copies files to *.partial~ first and then
 * renames *.partial~ into its original name.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 **/
gboolean
thunar_g_file_copy (GFile                *source,
                    GFile                *destination,
                    GFileCopyFlags        flags,
                    gboolean              use_partial,
                    GCancellable         *cancellable,
                    GFileProgressCallback progress_callback,
                    gpointer              progress_callback_data,
                    GError              **error)
{
  gboolean            success;
  GFileQueryInfoFlags query_flags;
  GFileInfo          *info = NULL;
  GFile              *parent;
  GFile              *partial;
  gchar              *partial_name;
  gchar              *base_name;

  _thunar_return_val_if_fail (g_file_has_parent (destination, NULL), FALSE);

  if (use_partial)
    {
      query_flags = (flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) ? G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS : G_FILE_QUERY_INFO_NONE;
      info = g_file_query_info (source,
                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                query_flags,
                                cancellable,
                                NULL);
    }

  /* directory does not need .partial */
  if (info == NULL)
    {
      use_partial = FALSE;
    }
  else
    {
      use_partial = g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR;
      g_clear_object (&info);
    }

  if (!use_partial)
    {
      success = g_file_copy (source, destination, flags, cancellable, progress_callback, progress_callback_data, error);
      return success;
    }

  /* check destination */
  if (g_file_query_exists (destination, NULL))
    {
      if (flags & G_FILE_COPY_OVERWRITE)
        {
          /* We want to overwrite. Just delete the old file */
          if (error != NULL)
            g_clear_error (error);
          error = NULL;
          g_file_delete (destination, NULL, error);
          if (error != NULL)
            return FALSE;
        }
      else
        {
          /* Try to mimic g_file_copy() error */
          if (error != NULL)
            *error = g_error_new (G_IO_ERROR, G_IO_ERROR_EXISTS,
                                  "Error opening file \"%s\": File exists", g_file_peek_path (destination));
          return FALSE;
        }
    }

  /* generate partial file name */
  base_name = g_file_get_basename (destination);
  if (base_name == NULL)
    {
      base_name = g_strdup ("UNNAMED");
    }

  /* limit filename length */
  partial_name = g_strdup_printf ("%.100s.partial~", base_name);
  parent = g_file_get_parent (destination);

  /* parent can't be NULL since destination must be a file */
  partial = g_file_get_child (parent, partial_name);
  g_clear_object (&parent);
  g_free (partial_name);

  /* check if partial file exists */
  if (g_file_query_exists (partial, NULL))
    g_file_delete (partial, NULL, error);

  /* copy file to .partial */
  success = g_file_copy (source, partial, flags, cancellable, progress_callback, progress_callback_data, error);

  if (success)
    {
      /* rename .partial if done without problem */
      GFile *renamed_file = g_file_set_display_name (partial, base_name, NULL, error);
      success = (renamed_file != NULL);
      if (success)
        g_object_unref (renamed_file);
    }

  if (!success)
    {
      /* try to remove incomplete file. */
      /* failure is expected so error is ignored */
      /* it must be triggered if cancelled */
      /* thus cancellable is also ignored */
      g_file_delete (partial, NULL, NULL);
    }

  g_clear_object (&partial);
  g_free (base_name);
  return success;
}



#ifdef HAVE_DIRECT_IO
static GInputStream *
open_file_and_buffer_for_direct_io (GFile        *file,
                                    GCancellable *cancellable,
                                    void        **buf_out,
                                    unsigned int *buf_align_out,
                                    size_t       *buf_size_out)
{
  int    fd = -1;
  size_t buf_align = 0;
  size_t offset_align = 0;
  size_t buf_size;
  void  *buf = NULL;
#ifdef HAVE_STATX_DIOALIGN
  struct statx statx_buf;

  // First we use statx () to ask the filesystem the required buffer alignment
  // for direct I/O (stx_dio_mem_align), as well as what the file offset
  // alignment (stx_dio_offset_align), which tells us what the buffer size is
  // required to be a multiple of.
  DBG ("Trying statx ()");
  if (statx (-1, g_file_peek_path (file), AT_STATX_SYNC_AS_STAT, STATX_DIOALIGN, &statx_buf) == 0
      && statx_buf.stx_dio_mem_align > 0
      && statx_buf.stx_dio_offset_align > 0)
    {
      DBG ("statx () succeeded, direct I/O align=%u, offset_align=%u",
           statx_buf.stx_dio_mem_align,
           statx_buf.stx_dio_offset_align);
      // Ensure alignment is at least 16, which is required for most SIMD
      // operations.  If somehow the required filesystem alignment/offset
      // alignment is lower than 16, it's always safe to go higher.
      buf_align = MAX (CMP_BUF_MIN_ALIGN, statx_buf.stx_dio_mem_align);
      offset_align = MAX (CMP_BUF_MIN_ALIGN, statx_buf.stx_dio_offset_align);
    }
#endif

  if (buf_align == 0 || offset_align == 0)
    {
      DBG ("statx () not supported or failed; using alignment/offset alignment fallback of 512");
      // Generally this fallback value should work on linux 2.6 & above.
      buf_align = 512;
      offset_align = 512;
    }

  // Since we're using blocking stdio operations and not cancellable GIO
  // operations, we should check the cancellable periodically.
  if (cancellable != NULL && g_cancellable_is_cancelled (cancellable))
    {
      goto out_err;
    }

  // Ensure the buffer size is a multiple of offset_align.
  buf_size = (CMP_BUF_SIZE / offset_align) * offset_align;
  DBG ("buf_size will be %" G_GSIZE_FORMAT " based on offset_align", buf_size);
  if (buf_size == 0)
    {
      // This shouldn't happen, but if somehow we get a truly bizarre
      // offst_align value that is larger than CMP_BUF_SIZE, we'll just bail.
      goto out_err;
    }

  // Open using the same flags g_file_read () uses, plus O_DIRECT.
  fd = open (g_file_peek_path (file), O_RDONLY | O_BINARY | O_CLOEXEC | O_DIRECT, 0);
  if (fd == -1 || (cancellable != NULL && g_cancellable_is_cancelled (cancellable)))
    {
      goto out_err;
    }
  DBG ("Successfully opened file with O_DIRECT");

  // Create a buffer to read data into, with the required alignment and size.
  if (posix_memalign (&buf, buf_align, buf_size) != 0)
    {
      goto out_err;
    }
  DBG ("Successfully allocated memory for direct I/O buffers");

  // Even if we've successfully opened the file for direct I/O and have
  // allocated our buffer, it's still possible that there's something wrong
  // with our buffer size or alignment.  So do a test read to see if it
  // succeeds, and if so, seek back to the beginning of the file.
  if (read (fd, buf, buf_size) == -1
      || lseek (fd, 0, SEEK_SET) == (off_t) -1
      || (cancellable != NULL && g_cancellable_is_cancelled (cancellable)))
    {
      goto out_err;
    }
  DBG ("Direct I/O is available and seems to be working");

  *buf_out = buf;
  *buf_align_out = buf_align;
  *buf_size_out = buf_size;

  return g_unix_input_stream_new (fd, TRUE);

out_err:

  if (fd >= 0)
    {
      close (fd);
    }
  free (buf);

  return NULL;
}
#endif



static GInputStream *
open_file_and_buffer_fallback (GFile        *file,
                               GCancellable *cancellable,
                               void        **buf_out,
                               unsigned int *buf_align_out,
                               size_t       *buf_size_out,
                               GError      **error)
{
  GFileInputStream *finp;
  void             *buf = NULL;

  finp = g_file_read (file, cancellable, error);

  if (finp != NULL)
    {
      /* Use 16-byte aligned allocations (rather than allocating on the stack), as
         that's usually a requirement for any SIMD optimizations the compiler might
         be able to do. */
      if (posix_memalign (&buf, CMP_BUF_MIN_ALIGN, CMP_BUF_SIZE) != 0)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "Failed to allocate aligned memory: %s", strerror (errno));
        }
    }

  if (finp != NULL && buf != NULL)
    {
      *buf_out = buf;
      *buf_align_out = CMP_BUF_MIN_ALIGN;
      *buf_size_out = CMP_BUF_SIZE;

      return G_INPUT_STREAM (finp);
    }
  else
    {
      if (finp != NULL)
        {
          g_object_unref (finp);
        }
      free (buf);

      return FALSE;
    }
}



/**
 * thunar_g_file_compare_contents:
 * @file_a      : a #GFile
 * @file_b      : a #GFile
 * @cancellable : (nullalble): optional #GCancellable object
 * @error       : (nullalble): optional #GError
 *
 * Compares the contents of @file_a and @file_b.
 *
 * Return value: %TRUE if the files match, %FALSE if not.
 **/
gboolean
thunar_g_file_compare_contents (GFile        *file_a,
                                GFile        *file_b,
                                GCancellable *cancellable,
                                GError      **error)
{
  GFileInputStream *inp_a = NULL;
  GInputStream     *inp_b = NULL;
  void             *buf_a = NULL;
  void             *buf_b = NULL;
  unsigned int      buf_align = 0;
  size_t            buf_size = 0;
  gboolean          is_equal = FALSE;

  g_return_val_if_fail (G_IS_FILE (file_a), FALSE);
  g_return_val_if_fail (G_IS_FILE (file_b), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  // We assume that file_b is the destination file, and that's the one that
  // really needs to be opened using O_DIRECT, as it's just been written, and
  // some (or even all) of it will still be in the kernel's buffer cache.
  // file_a can be opened normally, and will hopefully/likely be read from
  // buffer cache, which is both fine and faster.

  inp_a = g_file_read (file_a, cancellable, error);

  if (inp_a != NULL)
    {
#ifdef HAVE_DIRECT_IO
      if (g_file_has_uri_scheme (file_b, "file"))
        {
          DBG ("Attempting direct I/O for destination file");
          inp_b = open_file_and_buffer_for_direct_io (file_b, cancellable, &buf_b, &buf_align, &buf_size);
        }
#endif

      if (inp_b == NULL)
        {
          inp_b = open_file_and_buffer_fallback (file_b, cancellable, &buf_b, &buf_align, &buf_size, error);
        }
    }

  if (inp_b != NULL)
    {
      // Now that we know the alignment and size needed for file_b, we can also
      // allocate the buffer for file_a.
      if (posix_memalign (&buf_a, buf_align, buf_size) != 0)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "Failed to allocate aligned memory: %s", strerror (errno));
        }
    }

  if (inp_a != NULL && inp_b != NULL && buf_a != NULL)
    {
      for (;;)
        {
          gsize bytes_read_a;
          gsize bytes_read_b;

          if (!g_input_stream_read_all (G_INPUT_STREAM (inp_a), buf_a, buf_size, &bytes_read_a, cancellable, error))
            {
              break;
            }
          if (!g_input_stream_read_all (G_INPUT_STREAM (inp_b), buf_b, buf_size, &bytes_read_b, cancellable, error))
            {
              break;
            }

          if (bytes_read_a == 0 && bytes_read_b == 0)
            {
              is_equal = TRUE;
              break;
            }
          else if (bytes_read_a != bytes_read_b)
            {
              break;
            }
          else if (memcmp (buf_a, buf_b, bytes_read_a) != 0)
            {
              break;
            }
        }
    }

  if (inp_a != NULL)
    {
      g_object_unref (inp_a);
    }
  if (inp_b != NULL)
    {
      g_object_unref (inp_b);
    }

  free (buf_a);
  free (buf_b);

  return is_equal;
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
    {
      /* Prefer native paths for interoperability. */
      gchar *path = g_file_get_path (G_FILE (lp->data));
      if (path == NULL)
        {
          uris[n++] = g_file_get_uri (G_FILE (lp->data));
        }
      else
        {
          uris[n++] = g_filename_to_uri (path, NULL, NULL);
          g_free (path);
        }
    }

  return uris;
}



/**
 * thunar_g_file_list_get_parents:
 * @list : a list of #GFile<!---->s.
 *
 * Collects all parent folders of the passed files
 * If multiple files share the same parent, the parent will only be added once to the returned list.
 * Each list element of the returned list needs to be freed with g_object_unref() after use.
 *
 * Return value: A list of #GFile<!---->s of all parent folders. Free the returned list with calling g_object_unref() on each element
 **/
GList *
thunar_g_file_list_get_parents (GList *file_list)
{
  GList   *lp_file_list;
  GList   *lp_parent_folder_list;
  GFile   *parent_folder;
  GList   *parent_folder_list = NULL;
  gboolean folder_already_added;

  for (lp_file_list = file_list; lp_file_list != NULL; lp_file_list = lp_file_list->next)
    {
      if (!G_IS_FILE (lp_file_list->data))
        continue;
      parent_folder = g_file_get_parent (lp_file_list->data);
      if (parent_folder == NULL)
        continue;
      folder_already_added = FALSE;
      /* Check if the folder already is in our list */
      for (lp_parent_folder_list = parent_folder_list; lp_parent_folder_list != NULL; lp_parent_folder_list = lp_parent_folder_list->next)
        {
          if (g_file_equal (lp_parent_folder_list->data, parent_folder))
            {
              folder_already_added = TRUE;
              break;
            }
        }
      /* Keep the reference for each folder added to parent_folder_list */
      if (folder_already_added)
        g_object_unref (parent_folder);
      else
        parent_folder_list = g_list_append (parent_folder_list, parent_folder);
    }
  return parent_folder_list;
}



/**
 * thunar_g_file_is_descendant:
 * @descendant : a #GFile that is a potential descendant of @ancestor
 * @ancestor   : a #GFile
 *
 * Check if @descendant is a subdirectory of @ancestor.
 * @descendant == @ancestor also counts.
 *
 * Returns: %TRUE if @descendant is a subdirectory of @ancestor.
 **/
gboolean
thunar_g_file_is_descendant (GFile *descendant,
                             GFile *ancestor)
{
  _thunar_return_val_if_fail (descendant != NULL && G_IS_FILE (descendant), FALSE);
  _thunar_return_val_if_fail (ancestor != NULL && G_IS_FILE (ancestor), FALSE);

  for (GFile *parent = g_object_ref (descendant), *temp;
       parent != NULL;
       temp = g_file_get_parent (parent), g_object_unref (parent), parent = temp)
    {
      if (g_file_equal (parent, ancestor))
        {
          g_object_unref (parent);
          return TRUE;
        }
    }

  return FALSE;
}



gboolean
thunar_g_app_info_launch (GAppInfo          *info,
                          GFile             *working_directory,
                          GList             *path_list,
                          GAppLaunchContext *context,
                          GError           **error)
{
  ThunarFile  *file;
  GAppInfo    *default_app_info;
  GList       *recommended_app_infos;
  GList       *lp;
  const gchar *content_type;
  gboolean     result = FALSE;
  gchar       *new_path = NULL;
  gchar       *old_path = NULL;
  gboolean     skip_app_info_update;

  _thunar_return_val_if_fail (G_IS_APP_INFO (info), FALSE);
  _thunar_return_val_if_fail (working_directory == NULL || G_IS_FILE (working_directory), FALSE);
  _thunar_return_val_if_fail (path_list != NULL, FALSE);
  _thunar_return_val_if_fail (G_IS_APP_LAUNCH_CONTEXT (context), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  skip_app_info_update = (g_object_get_data (G_OBJECT (info), "skip-app-info-update") != NULL);

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

  /* if successful, remember the application as last used for the file types */
  if (result == TRUE)
    {
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          gboolean update_app_info = !skip_app_info_update;

          file = thunar_file_get (lp->data, NULL);
          if (file == NULL)
            continue;

          content_type = thunar_file_get_content_type (file);

          /* determine default application */
          default_app_info = thunar_file_get_default_handler (file);
          if (default_app_info != NULL)
            {
              /* check if the application is the default one */
              if (g_app_info_equal (info, default_app_info))
                update_app_info = FALSE;
              g_object_unref (default_app_info);
            }

          if (update_app_info)
            {
              /* obtain list of last used applications */
              recommended_app_infos = g_app_info_get_recommended_for_type (content_type);
              if (recommended_app_infos != NULL)
                {
                  /* check if the application is already the last used one
                   * by comparing it with the first entry in the list */
                  if (g_app_info_equal (info, recommended_app_infos->data))
                    update_app_info = FALSE;

                  g_list_free_full (recommended_app_infos, g_object_unref);
                }
            }

          /* emit "changed" on the file if we successfully changed the last used application */
          if (update_app_info && g_app_info_set_as_last_used_for_type (info, content_type, NULL))
            thunar_file_changed (file);

          g_object_unref (file);
        }
    }

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



gboolean
thunar_g_app_info_should_show (GAppInfo *info)
{
#ifdef HAVE_GIO_UNIX
  _thunar_return_val_if_fail (G_IS_APP_INFO (info), FALSE);

  if (G_IS_DESKTOP_APP_INFO (info))
    {
      /* NoDisplay=true files should be visible in the interface,
       * because this key is intent to hide mime-helpers from the
       * application menu. Hidden=true is never returned by GIO. */
      return g_desktop_app_info_get_show_in (G_DESKTOP_APP_INFO (info), NULL);
    }

  return TRUE;
#else
  /* we cannot exclude custom actions, so show everything */
  return TRUE;
#endif
}



gboolean
thunar_g_vfs_metadata_is_supported (void)
{
  GFile                  *root;
  GFileAttributeInfoList *attr_info_list;
  gint                    n;
  gboolean                metadata_is_supported = FALSE;

  /* get a GFile for the root directory, and obtain the list of writable namespaces for it */
  root = thunar_g_file_new_for_root ();
  attr_info_list = g_file_query_writable_namespaces (root, NULL, NULL);
  g_object_unref (root);

  /* loop through the returned namespace names and check if "metadata" is included */
  for (n = 0; n < attr_info_list->n_infos; n++)
    {
      if (g_strcmp0 (attr_info_list->infos[n].name, "metadata") == 0)
        {
          metadata_is_supported = TRUE;
          break;
        }
    }

  /* release the attribute info list */
  g_file_attribute_info_list_unref (attr_info_list);

  return metadata_is_supported;
}



/**
 * thunar_g_file_is_on_local_device:
 * @file : the source or target #GFile to test.
 *
 * Tries to find if the @file is on a local device or not.
 * Local device if (all conditions should match):
 * - the file has a 'file' uri scheme.
 * - the file is located on devices not handled by the #GVolumeMonitor (GVFS).
 * - the device is handled by #GVolumeMonitor (GVFS) and cannot be unmounted
 *   (USB key/disk, fuse mounts, Samba shares, PTP devices).
 *
 * The target @file may not exist yet when this function is used, so recurse
 * the parent directory, possibly reaching the root mountpoint.
 *
 * This should be enough to determine if a @file is on a local device or not.
 *
 * Return value: %TRUE if #GFile @file is on a so-called local device.
 **/
gboolean
thunar_g_file_is_on_local_device (GFile *file)
{
  gboolean is_local = FALSE;
  GFile   *target_file;
  GFile   *target_parent;
  GMount  *file_mount;

  _thunar_return_val_if_fail (file != NULL, TRUE);
  _thunar_return_val_if_fail (G_IS_FILE (file), TRUE);

  if (g_file_has_uri_scheme (file, "file") == FALSE)
    return FALSE;
  for (target_file = g_object_ref (file);
       target_file != NULL;
       target_file = target_parent)
    {
      if (g_file_query_exists (target_file, NULL))
        break;

      /* file or parent directory does not exist (yet)
       * query the parent directory */
      target_parent = g_file_get_parent (target_file);
      g_object_unref (target_file);
    }

  if (target_file == NULL)
    return FALSE;

  /* file_mount will be NULL for local files on local partitions/devices */
  file_mount = g_file_find_enclosing_mount (target_file, NULL, NULL);
  g_object_unref (target_file);
  if (file_mount == NULL)
    is_local = TRUE;
  else
    {
      /* mountpoints which cannot be unmounted are local devices.
       * attached devices like USB key/disk, fuse mounts, Samba shares,
       * PTP devices can always be unmounted and are considered remote/slow. */
      is_local = !g_mount_can_unmount (file_mount);
      g_object_unref (file_mount);
    }

  return is_local;
}



/**
 * thunar_g_file_set_executable_flags:
 * @file : the #GFile for which execute flags should be set
 *
 * Tries to set +x flag of the file for user, group and others
 *
 * Return value: %TRUE on sucess, %FALSE on error
 **/
gboolean
thunar_g_file_set_executable_flags (GFile   *file,
                                    GError **error)
{
  ThunarFileMode old_mode;
  ThunarFileMode new_mode;
  GFileInfo     *info;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to query information about the file */
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_UNIX_MODE,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL, error);

  if (G_LIKELY (info != NULL))
    {
      if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE))
        {
          /* determine the current mode */
          old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

          /* generate the new mode */
          new_mode = old_mode | THUNAR_FILE_MODE_USR_EXEC | THUNAR_FILE_MODE_GRP_EXEC | THUNAR_FILE_MODE_OTH_EXEC;

          if (old_mode != new_mode)
            {
              g_file_set_attribute_uint32 (file,
                                           G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL, error);
            }
        }
      else
        {
          g_warning ("No %s attribute found", G_FILE_ATTRIBUTE_UNIX_MODE);
        }

      g_object_unref (info);
    }

  return (error == NULL);
}



/**
 * thunar_g_file_is_in_xdg_data_dir:
 * @file      : a #GFile.
 *
 * Returns %TRUE if @file is located below one of the directories given in XDG_DATA_DIRS
 *
 * Return value: %TRUE if @file is located inside a XDG_DATA_DIR
 **/
gboolean
thunar_g_file_is_in_xdg_data_dir (GFile *file)
{
  const gchar *const *data_dirs;
  guint               i;
  gchar              *path;
  gboolean            found = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_file_is_native (file))
    {
      data_dirs = g_get_system_data_dirs ();
      if (G_LIKELY (data_dirs != NULL))
        {
          path = g_file_get_path (file);
          for (i = 0; data_dirs[i] != NULL; i++)
            {
              if (g_str_has_prefix (path, data_dirs[i]))
                {
                  found = TRUE;
                  break;
                }
            }
          g_free (path);
        }
    }
  return found;
}



/**
 * thunar_g_file_is_desktop_file:
 * @file      : a #GFile.
 *
 * Returns %TRUE if @file is a .desktop file.
 *
 * Return value: %TRUE if @file is a .desktop file.
 **/
gboolean
thunar_g_file_is_desktop_file (GFile *file)
{
  gchar     *basename;
  gboolean   is_desktop_file = FALSE;
  GFileInfo *info;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  basename = g_file_get_basename (file);

  /* only allow regular files with a .desktop extension */
  if (g_str_has_suffix (basename, ".desktop"))
    {
      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
      if (G_LIKELY (info != NULL && g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR))
        is_desktop_file = TRUE;
      g_object_unref (info);
    }

  g_free (basename);
  return is_desktop_file;
}



/**
 * thunar_g_file_get_link_path_for_symlink:
 * @file_to_link : the #GFile to which @symlink will have to point
 * @symlink      : a #GFile representing the symlink
 *
 * Method to build the link target path in order to link from @symlink to @file_to_link.
 * The caller is responsible for freeing the string using g_free() when done.
 *
 * Return value: The link path, or NULL on failure
 **/
char *
thunar_g_file_get_link_path_for_symlink (GFile *file_to_link,
                                         GFile *symlink)
{
  GFile *root;
  GFile *parent;
  char  *relative_path;
  char  *link_path;

  _thunar_return_val_if_fail (G_IS_FILE (file_to_link), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (symlink), NULL);

  /* */
  if (g_file_is_native (file_to_link) || g_file_is_native (symlink))
    {
      return g_file_get_path (file_to_link);
    }

  /* Search for the filesystem root */
  root = g_object_ref (file_to_link);
  while ((parent = g_file_get_parent (root)) != NULL)
    {
      g_object_unref (root);
      root = parent;
    }

  /* Build a absolute path, using the relative path up to the filesystem root */
  relative_path = g_file_get_relative_path (root, file_to_link);
  g_object_unref (root);
  link_path = g_strconcat ("/", relative_path, NULL);
  g_free (relative_path);
  return link_path;
}



static GFileInfo *
thunar_g_file_get_content_type_querry_info (GFile  *gfile,
                                            GError *err)
{
  GFileInfo *info = NULL;

  info = g_file_query_info (gfile,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL, &err);
  return info;
}



/**
 * thunar_g_file_get_content_type:
 * @file : #GFile for which the content type will be returned
 *
 * Gets the content type of the passed #GFile. Will always return a valid string.
 *
 * Return value: (transfer full): The content type as #gchar
 * The returned string should be freed with g_free() when no longer needed.
 **/
char *
thunar_g_file_get_content_type (GFile *gfile)
{
  gboolean   is_symlink = FALSE;
  GFileInfo *info = NULL;
  GError    *err = NULL;
  gchar     *content_type = NULL;

  info = thunar_g_file_get_content_type_querry_info (gfile, err);

  /* follow symlink, if found */
  if (G_LIKELY (info != NULL))
    {
      is_symlink = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
      if (is_symlink)
        {
          GFile *link_target = thunar_g_file_resolve_symlink (gfile);
          g_object_unref (info);
          info = NULL;
          if (link_target != NULL)
            info = thunar_g_file_get_content_type_querry_info (link_target, err);
          g_object_unref (link_target);
        }
    }

  if (G_LIKELY (info != NULL))
    {
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          /* this we known for sure */
          content_type = g_strdup ("inode/directory");
        }
      else
        {
          content_type = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));
          if (G_UNLIKELY (content_type == NULL))
            content_type = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE));
        }

      g_object_unref (info);
    }
  else
    {
      /* If gfile retrieved above is NULL, then g_file_query_info won't be called, thus keeping info NULL.
       * In this case, err will also be NULL. So it will fallback to "unknown" mime-type */
      if (G_LIKELY (err != NULL))
        {
          /* The mime-type 'inode/symlink' is  only used for broken links.
           * When the link is functional, the mime-type of the link target will be used */
          if (G_LIKELY (is_symlink && err->code == G_IO_ERROR_NOT_FOUND))
            content_type = g_strdup ("inode/symlink");
          else
            g_warning ("Content type loading failed for %s: %s",
                       g_file_get_uri (gfile),
                       err->message);
          g_error_free (err);
        }
    }

  /* fallback */
  if (content_type == NULL)
    content_type = g_strdup (DEFAULT_CONTENT_TYPE);

  return content_type;
}



static void
thunar_g_file_set_metadata_setting_finish (GObject      *source_object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GFile      *file = NULL;
  GError     *error = NULL;
  ThunarFile *thunar_file = NULL;

  file = G_FILE (source_object);

  if (file == NULL)
    return;

  if (!g_file_set_attributes_finish (file, result, NULL, &error))
    {
      g_warning ("Failed to set metadata: %s", error->message);
      g_error_free (error);
    }

  thunar_file = thunar_file_get (file, NULL);

  if (thunar_file == NULL)
    return;

  thunar_file_changed (thunar_file);
  g_object_unref (thunar_file);
}



static void
thunar_g_file_info_set_attribute (GFileInfo   *info,
                                  ThunarGType  type,
                                  const gchar *setting_name,
                                  const gchar *setting_value)
{
  switch (type)
    {
    case THUNAR_GTYPE_STRING:
      g_file_info_set_attribute_string (info, setting_name, setting_value);
      break;

    case THUNAR_GTYPE_STRINGV:
      {
        gchar **setting_values;
        setting_values = g_strsplit (setting_value, THUNAR_METADATA_STRING_DELIMETER, 100);
        g_file_info_set_attribute_stringv (info, setting_name, setting_values);
        g_strfreev (setting_values);
        break;
      }

    default:
      g_warning ("ThunarGType not supported, skipping");
      break;
    }
}



static gchar *
thunar_g_file_info_get_attribute (GFileInfo   *info,
                                  ThunarGType  type,
                                  const gchar *setting_name)
{
  switch (type)
    {
    case THUNAR_GTYPE_STRING:
      return g_strdup (g_file_info_get_attribute_string (info, setting_name));

    case THUNAR_GTYPE_STRINGV:
      {
        gchar **stringv = g_file_info_get_attribute_stringv (info, setting_name);
        GList  *string_list = NULL;
        gchar  *joined_string = NULL;

        if (stringv == NULL)
          return NULL;

        for (; *stringv != NULL; ++stringv)
          string_list = g_list_append (string_list, *stringv);

        joined_string = thunar_util_strjoin_list (string_list, THUNAR_METADATA_STRING_DELIMETER);
        g_list_free (string_list);
        return joined_string;
      }

    default:
      g_warning ("ThunarGType not supported, skipping");
      return NULL;
    }
}



/**
 * thunar_g_file_set_metadata_setting:
 * @file          : a #GFile instance.
 * @info          : Additional #GFileInfo instance to update
 * @type          : #ThunarGType of the metadata
 * @setting_name  : the name of the setting to set
 * @setting_value : the value to set
 * @async         : whether g_file_set_attributes_async or g_file_set_attributes_from_info should be used
 *
 * Sets the setting @setting_name of @file to @setting_value and stores it in
 * the @file<!---->s metadata.
 **/
void
thunar_g_file_set_metadata_setting (GFile       *file,
                                    GFileInfo   *info,
                                    ThunarGType  type,
                                    const gchar *setting_name,
                                    const gchar *setting_value,
                                    gboolean     async)
{
  GFileInfo *info_new;
  gchar     *attr_name;

  _thunar_return_if_fail (G_IS_FILE (file));
  _thunar_return_if_fail (G_IS_FILE_INFO (info));

  /* convert the setting name to an attribute name */
  attr_name = g_strdup_printf ("metadata::%s", setting_name);

  /* set the value in the current info. this call is needed to update the in-memory
   * GFileInfo structure to ensure that the new attribute value is available immediately */
  if (setting_value)
    thunar_g_file_info_set_attribute (info, type, attr_name, setting_value);

  /* send meta data to the daemon. this call is needed to store the new value of
   * the attribute in the file system */
  info_new = g_file_info_new ();
  thunar_g_file_info_set_attribute (info_new, type, attr_name, setting_value);
  if (async)
    {
      g_file_set_attributes_async (file, info_new,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   thunar_g_file_set_metadata_setting_finish,
                                   NULL);
    }
  else
    {
      g_file_set_attributes_from_info (file, info_new,
                                       G_FILE_QUERY_INFO_NONE,
                                       NULL,
                                       NULL);
    }
  g_free (attr_name);
  g_object_unref (G_OBJECT (info_new));
}



/**
 * thunar_file_clear_metadata_setting:
 * @file          : a #GFile instance.
 * @setting_name  : the name of the setting to clear
 *
 * Clear the metadata setting @setting_name of @file
 **/
void
thunar_g_file_clear_metadata_setting (GFile       *file,
                                      GFileInfo   *info,
                                      const gchar *setting_name)
{
  gchar *attr_name;

  _thunar_return_if_fail (G_IS_FILE (file));

  if (info == NULL)
    return;

  /* convert the setting name to an attribute name */
  attr_name = g_strdup_printf ("metadata::%s", setting_name);

  if (!g_file_info_has_attribute (info, attr_name))
    {
      g_free (attr_name);
      return;
    }

  g_file_info_remove_attribute (info, attr_name);
  g_file_set_attribute (file, attr_name, G_FILE_ATTRIBUTE_TYPE_INVALID,
                        NULL, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  g_free (attr_name);
}



/**
 * thunar_file_get_metadata_setting:
 * @file         : a #GFile instance.
 * @info         : #GFileInfo instance to extract the setting
 * @type         : #ThunarGType of the metadata
 * @setting_name : the name of the setting to get
 *
 * Gets the stored value of the metadata setting @setting_name for @file. Returns %NULL
 * if there is no stored setting. After usage the returned string needs to be released with 'g_free'
 *
 * Return value: (transfer full): the stored value of the setting for @file, or %NULL
 **/
gchar *
thunar_g_file_get_metadata_setting (GFile       *file,
                                    GFileInfo   *info,
                                    ThunarGType  type,
                                    const gchar *setting_name)
{
  gchar *attr_name;
  gchar *attr_value;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  if (info == NULL)
    return NULL;

  /* convert the setting name to an attribute name */
  attr_name = g_strdup_printf ("metadata::%s", setting_name);

  if (!g_file_info_has_attribute (info, attr_name))
    {
      g_free (attr_name);
      return NULL;
    }

  attr_value = thunar_g_file_info_get_attribute (info, type, attr_name);
  g_free (attr_name);

  return attr_value;
}
