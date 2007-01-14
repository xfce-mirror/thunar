/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* implement thunar-vfs-info's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __THUNAR_VFS_INFO_C__
#include <thunar-vfs/thunar-vfs-info.h>

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-io-local.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-mime-database-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



GType
thunar_vfs_info_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarVfsInfo"),
                                           (GBoxedCopyFunc) thunar_vfs_info_ref,
                                           (GBoxedFreeFunc) thunar_vfs_info_unref);
    }

  return type;
}



/**
 * thunar_vfs_info_new_for_path:
 * @path  : the #ThunarVfsPath of the file whose info should be queried.
 * @error : return location for errors or %NULL.
 *
 * Queries the #ThunarVfsInfo for the given @path. Returns the
 * #ThunarVfsInfo if the operation is successfull, else %NULL.
 * In the latter case, @error will be set to point to a #GError
 * describing the cause of the failure.
 *
 * Return value: the #ThunarVfsInfo for @path or %NULL.
 **/
ThunarVfsInfo*
thunar_vfs_info_new_for_path (ThunarVfsPath *path,
                              GError       **error)
{
  ThunarVfsInfo *info;
  gchar         *absolute_path;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  switch (thunar_vfs_path_get_scheme (path))
    {
    case THUNAR_VFS_PATH_SCHEME_FILE:
      absolute_path = thunar_vfs_path_dup_string (path);
      info = _thunar_vfs_io_local_get_info (path, absolute_path, error);
      g_free (absolute_path);
      break;

    case THUNAR_VFS_PATH_SCHEME_TRASH:
      info = _thunar_vfs_io_trash_get_info (path, error);
      break;

    default:
      g_assert_not_reached ();
      info = NULL;
    }

  return info;
}



/**
 * thunar_vfs_info_unref:
 * @info : a #ThunarVfsInfo.
 *
 * Decrements the reference count on @info by 1 and if
 * the reference count drops to zero as a result of this
 * operation, the @info will be freed completely.
 **/
void
thunar_vfs_info_unref (ThunarVfsInfo *info)
{
  if (exo_atomic_dec (&info->ref_count))
    {
      /* free the display name if dynamically allocated */
      if (info->display_name != thunar_vfs_path_get_name (info->path))
        g_free (info->display_name);

      /* free the custom icon name */
      g_free (info->custom_icon);

      /* drop the public info part */
      thunar_vfs_mime_info_unref (info->mime_info);
      thunar_vfs_path_unref (info->path);

      /* release the memory */
      _thunar_vfs_slice_free (ThunarVfsInfo, info);
    }
}



/**
 * thunar_vfs_info_copy:
 * @info : a #ThunarVfsInfo.
 *
 * Takes a deep copy of the @info and returns
 * it.
 *
 * Return value: a deep copy of @info.
 **/
ThunarVfsInfo*
thunar_vfs_info_copy (const ThunarVfsInfo *info)
{
  ThunarVfsInfo *dst;

  g_return_val_if_fail (info != NULL, NULL);

  dst = _thunar_vfs_slice_new (ThunarVfsInfo);
  dst->type = info->type;
  dst->mode = info->mode;
  dst->flags = info->flags;
  dst->uid = info->uid;
  dst->gid = info->gid;
  dst->size = info->size;
  dst->atime = info->atime;
  dst->mtime = info->mtime;
  dst->ctime = info->ctime;
  dst->device = info->device;
  dst->mime_info = thunar_vfs_mime_info_ref (info->mime_info);
  dst->path = thunar_vfs_path_ref (info->path);
  dst->custom_icon = g_strdup (info->custom_icon);
  dst->display_name = g_strdup (info->display_name);
  dst->ref_count = 1;

  return dst;
}



/**
 * thunar_vfs_info_get_free_space:
 * @info              : a #ThunarVfsInfo.
 * @free_space_return : return location for the amount of free space or %NULL.
 *
 * Determines the amount of free space available on the volume on which the
 * file to which @info refers resides. If the system is able to determine the
 * amount of free space, it will be placed into the location to which
 * @free_space_return points and %TRUE will be returned, else the function
 * will return %FALSE indicating that the system is unable to determine the
 * amount of free space.
 *
 * Return value: %TRUE if the amount of free space could be determined, else
 *               %FALSE:
 **/
gboolean
thunar_vfs_info_get_free_space (const ThunarVfsInfo *info,
                                ThunarVfsFileSize   *free_space_return)
{
  ThunarVfsPath *path;
  gboolean       succeed = FALSE;

  g_return_val_if_fail (info != NULL, FALSE);

  /* translate the info's path to a file:-URI path */
  path = _thunar_vfs_path_translate (info->path, THUNAR_VFS_PATH_SCHEME_FILE, NULL);
  if (G_UNLIKELY (path != NULL))
    {
      /* determine the amount of free space for the path */
      succeed = _thunar_vfs_io_local_get_free_space (path, free_space_return);
      thunar_vfs_path_unref (path);
    }

  return succeed;
}



/**
 * thunar_vfs_info_set_custom_icon:
 * @info        : a #ThunarVfsInfo.
 * @custom_icon : the new custom icon for the @info.
 * @error       : return location for errors or %NULL.
 *
 * Sets the custom icon for the file referred to by @info to the specified
 * @custom_icon, which can be either an absolute path to an image file or
 * the name of a themed icon.
 *
 * The @info must refer to a valid .desktop file, that is, its mime type
 * must be application/x-desktop.
 *
 * Return value: %TRUE if the custom icon of @info was updated successfully,
 *               %FALSE otherwise.
 **/
gboolean
thunar_vfs_info_set_custom_icon (ThunarVfsInfo *info,
                                 const gchar   *custom_icon,
                                 GError       **error)
{
  gboolean succeed = FALSE;
  gchar   *absolute_path;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (custom_icon != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  /* determine the absolute path in the file:-URI scheme */
  absolute_path = _thunar_vfs_path_translate_dup_string (info->path, THUNAR_VFS_PATH_SCHEME_FILE, error);
  if (G_LIKELY (absolute_path != NULL))
    {
      /* update the icon to the new custom_icon */
      succeed = _thunar_vfs_desktop_file_set_value (absolute_path, "Icon", custom_icon, error);
      if (G_LIKELY (succeed))
        {
          /* release the previous custom_icon */
          g_free (info->custom_icon);

          /* use the new custom_icon for the info */
          info->custom_icon = g_strdup (custom_icon);
        }

      /* cleanup */
      g_free (absolute_path);
    }

  return succeed;
}



/**
 * thunar_vfs_info_get_metadata:
 * @info     : a #ThunarVfsInfo.
 * @metadata : the #ThunarVfsInfoMetadata you are interested in.
 * @error    : return location for errors or %NULL.
 *
 * Queries the @metadata for @info and returns a string with the
 * data, or %NULL if either @metadata is invalid for @info or an
 * error occurred while querying the @metadata.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the @metadata for @info or %NULL in case of an
 *               error.
 **/
gchar*
thunar_vfs_info_get_metadata (const ThunarVfsInfo  *info,
                              ThunarVfsInfoMetadata metadata,
                              GError              **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  switch (thunar_vfs_path_get_scheme (info->path))
    {
    case THUNAR_VFS_PATH_SCHEME_FILE:
      return _thunar_vfs_io_local_get_metadata (info->path, metadata, error);

    case THUNAR_VFS_PATH_SCHEME_TRASH:
      return _thunar_vfs_io_trash_get_metadata (info->path, metadata, error);

    default:
      g_assert_not_reached ();
      return NULL;
    }
}



/**
 * thunar_vfs_info_execute:
 * @info              : a #ThunarVfsInfo.
 * @screen            : a #GdkScreen or %NULL to use the default #GdkScreen.
 * @path_list         : the list of #ThunarVfsPath<!---->s to give as parameters
 *                      to the file referred to by @info on execution.
 * @working_directory : the working directory in which to execute @info or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Executes the file referred to by @info, given @path_list as parameters,
 * on the specified @screen. @info may refer to either a regular,
 * executable file, or a <filename>.desktop</filename> file, whose
 * type is <literal>Application</literal>.
 *
 * If @working_directory is %NULL, the directory of the first path in @path_list
 * will be used as working directory. If @path_list is also %NULL, the directory
 * of @info will be used.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_info_execute (const ThunarVfsInfo *info,
                         GdkScreen           *screen,
                         GList               *path_list,
                         const gchar         *working_directory,
                         GError             **error)
{
  ThunarVfsPath *parent;
  const gchar   *icon = NULL;
  const gchar   *name;
  const gchar   *type;
  const gchar   *url;
  gboolean       startup_notify = FALSE;
  gboolean       terminal;
  gboolean       result = FALSE;
  XfceRc        *rc;
  gchar         *absolute_path;
  gchar         *path_escaped;
  gchar         *directory;
  gchar        **argv = NULL;
  gchar         *exec;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (working_directory == NULL || g_path_is_absolute (working_directory), FALSE);

  /* fallback to the default screen if none given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* determine the absolute path in the file:-URI scheme */
  absolute_path = _thunar_vfs_path_translate_dup_string (info->path, THUNAR_VFS_PATH_SCHEME_FILE, error);
  if (G_UNLIKELY (absolute_path == NULL))
    return FALSE;

  /* check if we have a .desktop (and NOT a .directory) file here */
  if (G_UNLIKELY (info->mime_info == _thunar_vfs_mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (info->path), ".directory") != 0))
    {
      rc = xfce_rc_simple_open (absolute_path, TRUE);
      if (G_LIKELY (rc != NULL))
        {
          /* we're only interested in [Desktop Entry] */
          xfce_rc_set_group (rc, "Desktop Entry");

          /* check if we have an application or a link here */
          type = xfce_rc_read_entry_untranslated (rc, "Type", "Application");
          if (G_LIKELY (exo_str_is_equal (type, "Application")))
            {
              /* check if we have a valid Exec field */
              exec = (gchar *) xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
              if (G_LIKELY (exec != NULL))
                {
                  /* parse the Exec field */
                  name = xfce_rc_read_entry (rc, "Name", NULL);
                  icon = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
                  terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);
                  startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);
                  result = thunar_vfs_exec_parse (exec, path_list, icon, name, absolute_path, terminal, NULL, &argv, error);
                }
              else
                {
                  /* TRANSLATORS: `Exec' is a field name in a .desktop file. You should leave it as-is. */
                  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No Exec field specified"));
                }
            }
          else if (exo_str_is_equal (type, "Link"))
            {
              /* check if we have a valid URL field */
              url = xfce_rc_read_entry_untranslated (rc, "URL", NULL);
              if (G_LIKELY (url != NULL))
                {
                  /* pass the URL to exo-open, which will fire up the appropriate viewer */
                  argv = g_new (gchar *, 3);
                  argv[0] = g_strdup ("exo-open");
                  argv[1] = g_strdup (url);
                  argv[2] = NULL;
                  result = TRUE;
                }
              else
                {
                  /* TRANSLATORS: `URL' is a field name in a .desktop file. You should leave it as-is. */
                  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No URL field specified"));
                }
            }
          else
            {
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
            }

          /* close the rc file */
          xfce_rc_close (rc);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Failed to parse file"));
        }
    }
  else
    {
      /* fake the Exec line */
      path_escaped = g_shell_quote (absolute_path);
      exec = g_strconcat (path_escaped, " %F", NULL);
      result = thunar_vfs_exec_parse (exec, path_list, NULL, NULL, NULL, FALSE, NULL, &argv, error);
      g_free (path_escaped);
      g_free (exec);
    }

  if (G_LIKELY (result))
    {
      /* determine the working directory */
      if (working_directory != NULL)
        {
          /* use the supplied working directory */
          directory = g_strdup (working_directory);
        }
      else if (G_LIKELY (path_list != NULL))
        {
          /* use the directory (in the file:-URI scheme) of the first list item */
          parent = thunar_vfs_path_get_parent (path_list->data);
          directory = (parent != NULL) ? _thunar_vfs_path_translate_dup_string (parent, THUNAR_VFS_PATH_SCHEME_FILE, NULL) : NULL;
        }
      else
        {
          /* use the directory of the executable file */
          directory = g_path_get_dirname (absolute_path);
        }

      /* execute the command */
      result = thunar_vfs_exec_on_screen (screen, directory, argv, NULL, G_SPAWN_SEARCH_PATH, startup_notify, icon, error);

      /* release the working directory */
      g_free (directory);
    }

  /* clean up */
  g_free (absolute_path);
  g_strfreev (argv);

  return result;
}



/**
 * thunar_vfs_info_rename:
 * @info  : a #ThunarVfsInfo.
 * @name  : the new file name in UTF-8 encoding.
 * @error : return location for errors or %NULL.
 *
 * Tries to rename the file referred to by @info to the
 * new @name.
 *
 * The rename operation is smart in that it checks the
 * type of @info first, and if @info refers to a
 * <filename>.desktop</filename> file, the file name
 * won't be touched, but instead the <literal>Name</literal>
 * field of the <filename>.desktop</filename> will be
 * changed to @name. Else, if @info refers to a regular
 * file or directory, the file will be given a new
 * name.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_info_rename (ThunarVfsInfo *info,
                        const gchar   *name,
                        GError       **error)
{
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* validate the name */
  if (*name == '\0' || strchr (name, '/') != NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid file name"));
      return FALSE;
    }

  /* validate the info */
  if (!_thunar_vfs_path_is_local (info->path))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Only local files may be renamed"));
      return FALSE;
    }

  return _thunar_vfs_io_local_rename (info, name, error);
}



/**
 * thunar_vfs_info_matches:
 * @a : a #ThunarVfsInfo.
 * @b : a #ThunarVfsInfo.
 *
 * Checks whether @a and @b refer to the same file
 * and share the same properties.
 *
 * Return value: %TRUE if @a and @b match.
 **/
gboolean
thunar_vfs_info_matches (const ThunarVfsInfo *a,
                         const ThunarVfsInfo *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  return a->type == b->type
      && a->mode == b->mode
      && a->flags == b->flags
      && a->uid == b->uid
      && a->gid == b->gid
      && a->size == b->size
      && a->atime == b->atime
      && a->mtime == b->mtime
      && a->ctime == b->ctime
      && a->device == b->device
      && a->mime_info == b->mime_info
      && thunar_vfs_path_equal (a->path, b->path)
      && strcmp (a->display_name, b->display_name) == 0;
}



/**
 * thunar_vfs_info_list_free:
 * @info_list : a list of #ThunarVfsInfo<!---->s.
 *
 * Unrefs all #ThunarVfsInfo<!---->s in @info_list and
 * frees the list itself.
 **/
void
thunar_vfs_info_list_free (GList *info_list)
{
  g_list_foreach (info_list, (GFunc) thunar_vfs_info_unref, NULL);
  g_list_free (info_list);
}



#define __THUNAR_VFS_INFO_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
