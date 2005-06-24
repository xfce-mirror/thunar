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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-trash-file.h>



static void              thunar_trash_file_class_init       (ThunarTrashFileClass *klass);
static void              thunar_trash_file_init             (ThunarTrashFile      *trash_file);
static void              thunar_trash_file_finalize         (GObject              *object);
static ThunarFolder     *thunar_trash_file_open_as_folder   (ThunarFile           *file,
                                                             GError              **error);
static ThunarVfsURI     *thunar_trash_file_get_uri          (ThunarFile           *file);
static ExoMimeInfo      *thunar_trash_file_get_mime_info    (ThunarFile           *file);
static const gchar      *thunar_trash_file_get_display_name (ThunarFile           *file);
static gboolean          thunar_trash_file_get_date         (ThunarFile           *file,
                                                             ThunarFileDateType    date_type,
                                                             ThunarVfsFileTime    *date_return);
static ThunarVfsFileType thunar_trash_file_get_kind         (ThunarFile           *file);
static ThunarVfsFileMode thunar_trash_file_get_mode         (ThunarFile           *file);
static ThunarVfsFileSize thunar_trash_file_get_size         (ThunarFile           *file);
static const gchar      *thunar_trash_file_get_icon_name    (ThunarFile           *file,
                                                             GtkIconTheme         *icon_theme);



struct _ThunarTrashFileClass
{
  ThunarFileClass __parent__;
};

struct _ThunarTrashFile
{
  ThunarFile __parent__;

  ThunarVfsTrashManager *manager;
  ThunarVfsTrash        *trash;
  ThunarVfsURI          *uri;
  ThunarFile            *real_file;
  gchar                 *display_name;
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
  thunarfile_class->open_as_folder = thunar_trash_file_open_as_folder;
  thunarfile_class->get_uri = thunar_trash_file_get_uri;
  thunarfile_class->get_mime_info = thunar_trash_file_get_mime_info;
  thunarfile_class->get_display_name = thunar_trash_file_get_display_name;
  thunarfile_class->get_date = thunar_trash_file_get_date;
  thunarfile_class->get_kind = thunar_trash_file_get_kind;
  thunarfile_class->get_mode = thunar_trash_file_get_mode;
  thunarfile_class->get_size = thunar_trash_file_get_size;
  thunarfile_class->get_icon_name = thunar_trash_file_get_icon_name;
}



static void
thunar_trash_file_init (ThunarTrashFile *trash_file)
{
}



static void
thunar_trash_file_finalize (GObject *object)
{
  ThunarTrashFile *trash_file = THUNAR_TRASH_FILE (object);

  /* disconnect from the real file */
  g_signal_handlers_disconnect_matched (G_OBJECT (trash_file->real_file),
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL,
                                        NULL, trash_file);

  /* cleanup */
  g_object_unref (G_OBJECT (trash_file->real_file));
  g_object_unref (G_OBJECT (trash_file->manager));
  g_object_unref (G_OBJECT (trash_file->trash));
  g_object_unref (G_OBJECT (trash_file->uri));
  g_free (trash_file->display_name);

  G_OBJECT_CLASS (thunar_trash_file_parent_class)->finalize (object);
}



static ThunarFolder*
thunar_trash_file_open_as_folder (ThunarFile *file,
                                  GError    **error)
{
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
               "Thunar does not yet support opening trashed folders");
  return NULL;
}



static ThunarVfsURI*
thunar_trash_file_get_uri (ThunarFile *file)
{
  return THUNAR_TRASH_FILE (file)->uri;
}



static ExoMimeInfo*
thunar_trash_file_get_mime_info (ThunarFile *file)
{
  return thunar_file_get_mime_info (THUNAR_TRASH_FILE (file)->real_file);
}



static const gchar*
thunar_trash_file_get_display_name (ThunarFile *file)
{
  return THUNAR_TRASH_FILE (file)->display_name;
}



static gboolean
thunar_trash_file_get_date (ThunarFile        *file,
                            ThunarFileDateType date_type,
                            ThunarVfsFileTime *date_return)
{
  ThunarTrashFile *trash_file = THUNAR_TRASH_FILE (file);
  return THUNAR_FILE_GET_CLASS (trash_file->real_file)->get_date (trash_file->real_file, date_type, date_return);
}



static ThunarVfsFileType
thunar_trash_file_get_kind (ThunarFile *file)
{
  return thunar_file_get_kind (THUNAR_TRASH_FILE (file)->real_file);
}



static ThunarVfsFileMode
thunar_trash_file_get_mode (ThunarFile *file)
{
  return thunar_file_get_mode (THUNAR_TRASH_FILE (file)->real_file);
}



static ThunarVfsFileSize
thunar_trash_file_get_size (ThunarFile *file)
{
  return thunar_file_get_size (THUNAR_TRASH_FILE (file)->real_file);
}



static const gchar*
thunar_trash_file_get_icon_name (ThunarFile   *file,
                                 GtkIconTheme *icon_theme)
{
  ThunarTrashFile *trash_file = THUNAR_TRASH_FILE (file);
  return THUNAR_FILE_GET_CLASS (trash_file->real_file)->get_icon_name (trash_file->real_file, icon_theme);
}



/**
 * thunar_trash_file_new:
 * @uri   : a #ThunarVfsURI referring to a trashed file.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarTrashFile for the given @uri.
 *
 * Return value: the newly allocated #ThunarTrashFile or %NULL.
 **/
ThunarFile*
thunar_trash_file_new (ThunarVfsURI *uri,
                       GError      **error)
{
  ThunarVfsTrashManager *trash_manager;
  ThunarVfsTrashInfo    *info;
  ThunarTrashFile       *trash_file;
  ThunarVfsTrash        *trash;
  ThunarVfsURI          *real_uri;
  ThunarFile            *real_file;
  gchar                 *real_path;
  gchar                 *path;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* grab a reference to the global trash manager */
  trash_manager = thunar_vfs_trash_manager_get_default ();

  /* try to resolve the given uri */
  trash = thunar_vfs_trash_manager_resolve_uri (trash_manager, uri, &path, error);
  if (G_UNLIKELY (trash == NULL))
    {
      g_object_unref (G_OBJECT (trash_manager));
      return NULL;
    }

  /* check if the path is ok */
  if (G_UNLIKELY (strchr (path, '/') != NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   "Thunar does not yet support opening trashed folders");
      g_object_unref (G_OBJECT (trash_manager));
      g_object_unref (G_OBJECT (trash));
      g_free (path);
      return NULL;
    }

  /* query the trash information for the determined file */
  info = thunar_vfs_trash_get_info (trash, path);
  if (G_UNLIKELY (info == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                   "The given file is not present in the trash");
      g_object_unref (G_OBJECT (trash_manager));
      g_object_unref (G_OBJECT (trash));
      g_free (path);
      return NULL;
    }

  /* query the 'file://' uri corresponding to the trashed file */
  real_path = thunar_vfs_trash_get_path (trash, path);
  real_uri = thunar_vfs_uri_new_for_path (real_path);
  g_free (real_path);

  /* try to open the real file */
  real_file = thunar_file_get_for_uri (real_uri, error);
  if (G_UNLIKELY (real_file == NULL))
    {
      g_object_unref (G_OBJECT (trash_manager));
      g_object_unref (G_OBJECT (real_uri));
      g_object_unref (G_OBJECT (trash));
      thunar_vfs_trash_info_free (info);
      g_free (path);
      return NULL;
    }

  /* allocate the new file object */
  trash_file = g_object_new (THUNAR_TYPE_TRASH_FILE, NULL);
  trash_file->display_name = g_path_get_basename (thunar_vfs_trash_info_get_path (info));
  trash_file->manager = trash_manager;
  trash_file->real_file = real_file;
  trash_file->uri = g_object_ref (G_OBJECT (uri));
  trash_file->trash = trash;

  /* watch the real file */
  g_signal_connect_swapped (G_OBJECT (real_file), "changed", G_CALLBACK (thunar_file_changed), trash_file);
  g_signal_connect_swapped (G_OBJECT (real_file), "destroy", G_CALLBACK (gtk_object_destroy), trash_file);

  /* cleanup */
  g_object_unref (G_OBJECT (real_uri));
  thunar_vfs_trash_info_free (info);
  g_free (path);

  return THUNAR_FILE (trash_file);
}




