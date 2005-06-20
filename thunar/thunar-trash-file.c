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

#include <thunar/thunar-trash-file.h>



static void               thunar_trash_file_class_init        (ThunarTrashFileClass   *klass);
static void               thunar_trash_file_init              (ThunarTrashFile        *trash_file);
static void               thunar_trash_file_finalize          (GObject                *object);
static ThunarVfsURI      *thunar_trash_file_get_uri           (ThunarFile             *file);
static ExoMimeInfo       *thunar_trash_file_get_mime_info     (ThunarFile             *file);
static const gchar       *thunar_trash_file_get_display_name  (ThunarFile             *file);
static ThunarVfsFileType  thunar_trash_file_get_kind          (ThunarFile             *file);
static ThunarVfsFileMode  thunar_trash_file_get_mode          (ThunarFile             *file);
static ThunarVfsFileSize  thunar_trash_file_get_size          (ThunarFile             *file);
static const gchar       *thunar_trash_file_get_icon_name     (ThunarFile             *file,
                                                               GtkIconTheme           *icon_theme);



struct _ThunarTrashFileClass
{
  ThunarFileClass __parent__;
};

struct _ThunarTrashFile
{
  ThunarFile __parent__;

  ThunarVfsTrashManager *manager;
  ThunarVfsURI          *uri;
};



G_DEFINE_TYPE (ThunarTrashFile, thunar_trash_file, THUNAR_TYPE_FILE);



static void
thunar_trash_file_class_init (ThunarTrashFileClass *klass)
{
  ThunarFileClass *thunarfile_class;
  GObjectClass    *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_trash_file_finalize;

  thunarfile_class = THUNAR_FILE_CLASS (klass);
  thunarfile_class->get_uri = thunar_trash_file_get_uri;
  thunarfile_class->get_mime_info = thunar_trash_file_get_mime_info;
  thunarfile_class->get_display_name = thunar_trash_file_get_display_name;
  thunarfile_class->get_kind = thunar_trash_file_get_kind;
  thunarfile_class->get_mode = thunar_trash_file_get_mode;
  thunarfile_class->get_size = thunar_trash_file_get_size;
  thunarfile_class->get_icon_name = thunar_trash_file_get_icon_name;
}



static void
thunar_trash_file_init (ThunarTrashFile *trash_file)
{
  /* register with the trash manager */
  trash_file->manager = thunar_vfs_trash_manager_get_default ();
  g_signal_connect_swapped (G_OBJECT (trash_file->manager), "notify::empty",
                            G_CALLBACK (thunar_file_changed), trash_file);
}



static void
thunar_trash_file_finalize (GObject *object)
{
  ThunarTrashFile *trash_file = THUNAR_TRASH_FILE (object);

  /* unregister from the trash manager */
  g_signal_handlers_disconnect_by_func (G_OBJECT (trash_file->manager), thunar_file_changed, trash_file);
  g_object_unref (G_OBJECT (trash_file->manager));

  G_OBJECT_CLASS (thunar_trash_file_parent_class)->finalize (object);
}



static ThunarVfsURI*
thunar_trash_file_get_uri (ThunarFile *file)
{
  return THUNAR_TRASH_FILE (file)->uri;
}



static ExoMimeInfo*
thunar_trash_file_get_mime_info (ThunarFile *file)
{
  return NULL;
}



static const gchar*
thunar_trash_file_get_display_name (ThunarFile *file)
{
  return _("Trash");
}



static ThunarVfsFileType
thunar_trash_file_get_kind (ThunarFile *file)
{
  return THUNAR_VFS_FILE_TYPE_DIRECTORY;
}



static ThunarVfsFileMode
thunar_trash_file_get_mode (ThunarFile *file)
{
  return THUNAR_VFS_FILE_MODE_USR_ALL;
}



static ThunarVfsFileSize
thunar_trash_file_get_size (ThunarFile *file)
{
  return 0;
}



static const gchar*
thunar_trash_file_get_icon_name (ThunarFile   *file,
                                 GtkIconTheme *icon_theme)
{
  ThunarTrashFile *trash_file = THUNAR_TRASH_FILE (file);
  const gchar     *icon_name;

  /* determine the proper icon for the trash state */
  icon_name = thunar_vfs_trash_manager_is_empty (trash_file->manager)
            ? "gnome-fs-trash-empty" : "gnome-fs-trash-full";

  /* check if the icon is present in the icon theme */
  if (gtk_icon_theme_has_icon (icon_theme, icon_name))
    return icon_name;

  return NULL;
}



/**
 * thunar_trash_file_new:
 * @uri   : the #ThunarVfsURI referrring to the trash file.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarTrashFile object for the given @uri.
 * Returns %NULL on error, and sets @error to point to an
 * #GError object describing the cause, if the operation fails.
 *
 * You should not ever call this function directly. Instead
 * use the #thunar_file_get_for_uri method, which performs
 * some caching of #ThunarFile objects.
 *
 * Return value: the newly allocated #ThunarTrashFile instance
 *               or %NULL on error.
 **/
ThunarFile*
thunar_trash_file_new (ThunarVfsURI *uri,
                       GError      **error)
{
  ThunarTrashFile *trash_file;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_TRASH, NULL);

  /* take an additional reference on the uri */
  g_object_ref (G_OBJECT (uri));

  /* allocate the new object */
  trash_file = g_object_new (THUNAR_TYPE_TRASH_FILE, NULL);
  trash_file->uri = uri;

  return THUNAR_FILE (trash_file);
}




