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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#include <stdio.h>
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
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_access() if possible */
#if GLIB_CHECK_VERSION(2,8,0)
#include <glib/gstdio.h>
#else
#define g_access(path, mode) (access ((path), (mode)))
#endif

/* Use g_rename() if possible */
#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_rename(from, to) (rename ((from), (to)))
#endif



static ThunarVfsMimeDatabase *mime_database;
static ThunarVfsMimeInfo     *mime_application_x_shellscript;
static ThunarVfsMimeInfo     *mime_application_x_executable;
static ThunarVfsMimeInfo     *mime_application_x_desktop;
static ThunarVfsMimeInfo     *mime_inode_blockdevice;
static ThunarVfsMimeInfo     *mime_inode_chardevice;
static ThunarVfsMimeInfo     *mime_inode_directory;
static ThunarVfsMimeInfo     *mime_inode_symlink;
static ThunarVfsMimeInfo     *mime_inode_socket;
static ThunarVfsMimeInfo     *mime_inode_fifo;



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
  gchar absolute_path[PATH_MAX + 1];
  if (thunar_vfs_path_to_string (path, absolute_path, sizeof (absolute_path), error) < 0)
    return NULL;
  return _thunar_vfs_info_new_internal (path, absolute_path, error);
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

#ifdef G_ENABLE_DEBUG
      memset (info, 0xaa, sizeof (*info));
#endif

      g_free (info);
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

  dst = g_new (ThunarVfsInfo, 1);
  dst->type = info->type;
  dst->mode = info->mode;
  dst->flags = info->flags;
  dst->uid = info->uid;
  dst->gid = info->gid;
  dst->size = info->size;
  dst->atime = info->atime;
  dst->mtime = info->mtime;
  dst->ctime = info->ctime;
  dst->inode = info->inode;
  dst->device = info->device;
  dst->mime_info = thunar_vfs_mime_info_ref (info->mime_info);
  dst->path = thunar_vfs_path_ref (info->path);
  dst->custom_icon = g_strdup (info->custom_icon);
  dst->display_name = g_strdup (info->display_name);
  dst->ref_count = 1;

  return dst;
}



/**
 * thunar_vfs_info_execute:
 * @info      : a #ThunarVfsInfo.
 * @screen    : a #GdkScreen or %NULL to use the default #GdkScreen.
 * @path_list : the list of #ThunarVfsPath<!---->s to give as parameters
 *              to the file referred to by @info on execution.
 * @error     : return location for errors or %NULL.
 *
 * Executes the file referred to by @info, given @path_list as parameters,
 * on the specified @screen. @info may refer to either a regular,
 * executable file, or a <filename>.desktop</filename> file, whose
 * type is <literal>Application</literal>.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_info_execute (const ThunarVfsInfo *info,
                         GdkScreen           *screen,
                         GList               *path_list,
                         GError             **error)
{
  ThunarVfsPath *parent;
  const gchar   *icon;
  const gchar   *name;
  gboolean       startup_notify = FALSE;
  gboolean       terminal;
  gboolean       result = FALSE;
  XfceRc        *rc;
  gchar          absolute_path[PATH_MAX + 1];
  gchar         *working_directory;
  gchar         *path_escaped;
  gchar        **argv = NULL;
  gchar         *exec;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->ref_count > 0, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* fallback to the default screen if none given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* determine the absolute path */
  if (thunar_vfs_path_to_string (info->path, absolute_path, sizeof (absolute_path), error) < 0)
    return FALSE;

  /* check if we have a .desktop (and NOT a .directory) file here */
  if (G_UNLIKELY (info->mime_info == mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (info->path), ".directory") != 0))
    {
      rc = xfce_rc_simple_open (absolute_path, TRUE);
      if (G_LIKELY (rc != NULL))
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
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No Exec field specified"));
            }

          /* close the rc file */
          xfce_rc_close (rc);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unable to parse file"));
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
      if (G_LIKELY (path_list != NULL))
        {
          /* use the directory of the first list item */
          parent = thunar_vfs_path_get_parent (path_list->data);
          working_directory = (parent != NULL) ? thunar_vfs_path_dup_string (parent) : NULL;
        }
      else
        {
          /* use the directory of the executable file */
          working_directory = g_path_get_dirname (absolute_path);
        }

      /* execute the command */
      result = thunar_vfs_exec_on_screen (screen, working_directory, argv, NULL, G_SPAWN_SEARCH_PATH, startup_notify, error);

      /* release the working directory */
      g_free (working_directory);
    }

  /* clean up */
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
  const gchar * const *locale;
  ThunarVfsMimeInfo   *mime_info;
  GKeyFile            *key_file;
  gsize                data_length;
  gchar               *data;
  gchar               *key;
  gchar                src_path[PATH_MAX + 1];
  gchar               *dir_name;
  gchar               *dst_name;
  gchar               *dst_path;
#if !GLIB_CHECK_VERSION(2,8,0)
  FILE                *fp;
#endif

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->ref_count > 0, FALSE);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* validate the name */
  if (*name == '\0' || strchr (name, '/') != NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid file name"));
      return FALSE;
    }

  /* determine the source path */
  if (thunar_vfs_path_to_string (info->path, src_path, sizeof (src_path), error) < 0)
    return FALSE;

  /* check if we have a .desktop (and NOT a .directory) file here */
  if (G_UNLIKELY (info->mime_info == mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (info->path), ".directory") != 0))
    {
      /* try to open the .desktop file */
      key_file = g_key_file_new ();
      if (!g_key_file_load_from_file (key_file, src_path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, error))
        {
          g_key_file_free (key_file);
          return FALSE;
        }

      /* check if the file is valid */
      if (G_UNLIKELY (!g_key_file_has_group (key_file, "Desktop Entry")))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
          g_key_file_free (key_file);
          return FALSE;
        }

      /* save the new name (localized if required) */
      for (locale = g_get_language_names (); *locale != NULL; ++locale)
        {
          key = g_strdup_printf ("Name[%s]", *locale);
          if (g_key_file_has_key (key_file, "Desktop Entry", key, NULL))
            {
              g_key_file_set_string (key_file, "Desktop Entry", key, name);
              g_free (key);
              break;
            }
          g_free (key);
        }

      /* fallback to unlocalized name */
      if (G_UNLIKELY (*locale == NULL))
        g_key_file_set_string (key_file, "Desktop Entry", "Name", name);

      /* serialize the key file to a buffer */
      data = g_key_file_to_data (key_file, &data_length, error);
      g_key_file_free (key_file);
      if (G_UNLIKELY (data == NULL))
        return FALSE;

      /* write the data back to the file */
#if GLIB_CHECK_VERSION(2,8,0)
      if (!g_file_set_contents (src_path, data, data_length, error))
        {
          g_free (data);
          return FALSE;
        }
#else
      fp = fopen (src_path, "w");
      if (G_UNLIKELY (fp == NULL))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          g_free (data);
          return FALSE;
        }
      fwrite (data, data_length, 1, fp);
      fclose (fp);
#endif

      /* release the previous display name */
      if (G_LIKELY (info->display_name != thunar_vfs_path_get_name (info->path)))
        g_free (info->display_name);

      /* apply the new display name */
      info->display_name = g_strdup (name);

      /* clean up */
      g_free (data);
    }
  else
    {
      /* convert the destination file to local encoding */
      dst_name = g_filename_from_utf8 (name, -1, NULL, NULL, error);
      if (G_UNLIKELY (dst_name == NULL))
        return FALSE;

      /* determine the destination path */
      dir_name = g_path_get_dirname (src_path);
      dst_path = g_build_filename (dir_name, dst_name, NULL);
      g_free (dst_name);
      g_free (dir_name);

      /* verify that the rename target does not already exist */
      if (G_UNLIKELY (g_file_test (dst_path, G_FILE_TEST_EXISTS)))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_EXIST, g_strerror (EEXIST));
          g_free (dst_path);
          return FALSE;
        }

      /* perform the rename */
      if (G_UNLIKELY (g_rename (src_path, dst_path) < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          g_free (dst_path);
          return FALSE;
        }

      /* update the info's display name */
      if (info->display_name != thunar_vfs_path_get_name (info->path))
        g_free (info->display_name);
      info->display_name = g_strdup (name);

      /* update the info's path */
      thunar_vfs_path_unref (info->path);
      info->path = thunar_vfs_path_new (dst_path, NULL);

      /* if we have a regular file here, then we'll need to determine
       * the mime type again, as it may be based on the file name
       */
      if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
        {
          mime_info = info->mime_info;
          info->mime_info = thunar_vfs_mime_database_get_info_for_file (mime_database, dst_path, info->display_name);
          thunar_vfs_mime_info_unref (mime_info);
        }

      /* clean up */
      g_free (dst_path);
    }

  return TRUE;
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
  g_return_val_if_fail (a->ref_count > 0, FALSE);
  g_return_val_if_fail (b->ref_count > 0, FALSE);

  return a->type == b->type
      && a->mode == b->mode
      && a->flags == b->flags
      && a->uid == b->uid
      && a->gid == b->gid
      && a->size == b->size
      && a->atime == b->atime
      && a->mtime == b->mtime
      && a->ctime == b->ctime
      && a->inode == b->inode
      && a->device == b->device
      && a->mime_info == b->mime_info
      && thunar_vfs_path_equal (a->path, b->path)
      && strcmp (a->display_name, b->display_name) == 0;
}



/**
 * _thunar_vfs_info_init:
 *
 * Initializes the info component of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_info_init (void)
{
  /* grab a reference on the mime database */
  mime_database = thunar_vfs_mime_database_get_default ();

  /* pre-determine the most important mime types */
  mime_inode_fifo = thunar_vfs_mime_database_get_info (mime_database, "inode/fifo");
  mime_inode_socket = thunar_vfs_mime_database_get_info (mime_database, "inode/socket");
  mime_inode_symlink = thunar_vfs_mime_database_get_info (mime_database, "inode/symlink");
  mime_inode_directory = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");
  mime_inode_chardevice = thunar_vfs_mime_database_get_info (mime_database, "inode/chardevice");
  mime_inode_blockdevice = thunar_vfs_mime_database_get_info (mime_database, "inode/blockdevice");
  mime_application_x_desktop = thunar_vfs_mime_database_get_info (mime_database, "application/x-desktop");
  mime_application_x_executable = thunar_vfs_mime_database_get_info (mime_database, "application/x-executable");
  mime_application_x_shellscript = thunar_vfs_mime_database_get_info (mime_database, "application/x-shellscript");
}



/**
 * _thunar_vfs_info_shutdown:
 *
 * Shuts down the info component of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_info_shutdown (void)
{
  /* release the mime type references */
  thunar_vfs_mime_info_unref (mime_application_x_shellscript);
  thunar_vfs_mime_info_unref (mime_application_x_executable);
  thunar_vfs_mime_info_unref (mime_application_x_desktop);
  thunar_vfs_mime_info_unref (mime_inode_blockdevice);
  thunar_vfs_mime_info_unref (mime_inode_chardevice);
  thunar_vfs_mime_info_unref (mime_inode_directory);
  thunar_vfs_mime_info_unref (mime_inode_symlink);
  thunar_vfs_mime_info_unref (mime_inode_socket);
  thunar_vfs_mime_info_unref (mime_inode_fifo);

  /* release the reference on the mime database */
  g_object_unref (G_OBJECT (mime_database));
  mime_database = NULL;
}



/**
 * _thunar_vfs_info_new_internal:
 * @path
 * @absolute_path
 * @error         : return location for errors or %NULL.
 *
 * Return value:
 **/
ThunarVfsInfo*
_thunar_vfs_info_new_internal (ThunarVfsPath *path,
                               const gchar   *absolute_path,
                               GError       **error)
{
  ThunarVfsInfo *info;
  const guchar  *s;
  const gchar   *name;
  const gchar   *str;
  struct stat    lsb;
  struct stat    sb;
  XfceRc        *rc;
  GList         *mime_infos;
  GList         *lp;
  gchar         *p;

  g_return_val_if_fail (g_path_is_absolute (absolute_path), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (G_UNLIKELY (lstat (absolute_path, &lsb) < 0))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Failed to stat file `%s': %s"),
                   absolute_path, g_strerror (errno));
      return NULL;
    }

  info = g_new (ThunarVfsInfo, 1);
  info->path = thunar_vfs_path_ref (path);
  info->ref_count = 1;
  info->custom_icon = NULL;

  /* determine the display name of the file */
  name = thunar_vfs_path_get_name (path);
  for (s = (const guchar *) name; *s >= 32 && *s <= 127; ++s)
    ;
  if (G_LIKELY (*s == '\0'))
    {
      /* we don't need to perform any transformation if
       * the file contains only valid ASCII characters.
       */
      info->display_name = (gchar *) name;
    }
  else
    {
      /* determine the displayname using various tricks */
      info->display_name = g_filename_display_name (name);
    }

  /* determine the POSIX file attributes */
  if (G_LIKELY (!S_ISLNK (lsb.st_mode)))
    {
      info->type = (lsb.st_mode & S_IFMT) >> 12;
      info->mode = lsb.st_mode & 07777;
      info->flags = THUNAR_VFS_FILE_FLAGS_NONE;
      info->uid = lsb.st_uid;
      info->gid = lsb.st_gid;
      info->size = lsb.st_size;
      info->atime = lsb.st_atime;
      info->ctime = lsb.st_ctime;
      info->mtime = lsb.st_mtime;
      info->inode = lsb.st_ino;
      info->device = lsb.st_dev;
    }
  else
    {
      if (stat (absolute_path, &sb) == 0)
        {
          info->type = (sb.st_mode & S_IFMT) >> 12;
          info->mode = sb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = sb.st_uid;
          info->gid = sb.st_gid;
          info->size = sb.st_size;
          info->atime = sb.st_atime;
          info->ctime = sb.st_ctime;
          info->mtime = sb.st_mtime;
          info->inode = sb.st_ino;
          info->device = sb.st_dev;
        }
      else
        {
          info->type = THUNAR_VFS_FILE_TYPE_SYMLINK;
          info->mode = lsb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = lsb.st_uid;
          info->gid = lsb.st_gid;
          info->size = lsb.st_size;
          info->atime = lsb.st_atime;
          info->ctime = lsb.st_ctime;
          info->mtime = lsb.st_mtime;
          info->inode = lsb.st_ino;
          info->device = lsb.st_dev;
        }
    }

  /* determine the file's mime type */
  switch (info->type)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_socket);
      break;

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_symlink);
      break;

    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_blockdevice);
      break;

    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_directory);
      break;

    case THUNAR_VFS_FILE_TYPE_CHARDEV:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_chardevice);
      break;

    case THUNAR_VFS_FILE_TYPE_FIFO:
      info->mime_info = thunar_vfs_mime_info_ref (mime_inode_fifo);
      break;

    case THUNAR_VFS_FILE_TYPE_REGULAR:
      /* determine the MIME-type for the regular file */
      info->mime_info = thunar_vfs_mime_database_get_info_for_file (mime_database, absolute_path, info->display_name);

      /* check if the file is executable (for security reasons
       * we only allow execution of well known file types).
       */
      if ((info->mode & 0444) != 0 && g_access (absolute_path, X_OK) == 0)
        {
          mime_infos = thunar_vfs_mime_database_get_infos_for_info (mime_database, info->mime_info);
          for (lp = mime_infos; lp != NULL; lp = lp->next)
            {
              if (lp->data == mime_application_x_executable || lp->data == mime_application_x_shellscript)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                  break;
                }
            }
          thunar_vfs_mime_info_list_free (mime_infos);
        }

      /* check if we have a .desktop (and NOT a .directory) file here */
      if (G_UNLIKELY (info->mime_info == mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (info->path), ".directory") != 0))
        {
          /* try to query the hints from the .desktop file */
          rc = xfce_rc_simple_open (absolute_path, TRUE);
          if (G_LIKELY (rc != NULL))
            {
              /* we're only interested in the desktop data */
              xfce_rc_set_group (rc, "Desktop Entry");

              /* check if we have a valid icon info */
              str = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
              if (G_LIKELY (str != NULL && *str != '\0'))
                {
                  /* setup the custom icon */
                  info->custom_icon = g_strdup (str);

                  /* drop any suffix (e.g. '.png') from themed icons */
                  if (!g_path_is_absolute (info->custom_icon))
                    {
                      p = strrchr (info->custom_icon, '.');
                      if (G_UNLIKELY (p != NULL))
                        *p = '\0';
                    }
                }

              /* check if we have a valid name info */
              str = xfce_rc_read_entry (rc, "Name", NULL);
              if (G_LIKELY (str != NULL && *str != '\0'))
                {
                  /* release the previous display name */
                  if (G_UNLIKELY (info->display_name != thunar_vfs_path_get_name (info->path)))
                    g_free (info->path);

                  /* use the name specified by the .desktop file as display name */
                  info->display_name = g_strdup (str);
                }

              /* check if the desktop file refers to an application
               * and has a non-NULL Exec field set.
               */
              str = xfce_rc_read_entry_untranslated (rc, "Type", "Application");
              if (G_LIKELY (exo_str_is_equal (str, "Application"))
                  && xfce_rc_read_entry (rc, "Exec", NULL) != NULL)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                }

              /* close the file */
              xfce_rc_close (rc);
            }
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return info;
}



#define __THUNAR_VFS_INFO_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
