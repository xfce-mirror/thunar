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

#include <thunar/thunar-local-file.h>



static void               thunar_local_file_class_init        (ThunarLocalFileClass *klass);
static void               thunar_local_file_init              (ThunarLocalFile      *local_file);
static void               thunar_local_file_finalize          (GObject              *object);
static ThunarVfsURI      *thunar_local_file_get_uri           (ThunarFile           *file);
static ExoMimeInfo       *thunar_local_file_get_mime_info     (ThunarFile           *file);
static const gchar       *thunar_local_file_get_display_name  (ThunarFile           *file);
static const gchar       *thunar_local_file_get_special_name  (ThunarFile           *file);
static ThunarVfsFileType  thunar_local_file_get_kind          (ThunarFile           *file);
static ThunarVfsFileMode  thunar_local_file_get_mode          (ThunarFile           *file);
static ThunarVfsFileSize  thunar_local_file_get_size          (ThunarFile           *file);
static const gchar       *thunar_local_file_get_icon_name     (ThunarFile           *file,
                                                               GtkIconTheme         *icon_theme);



struct _ThunarLocalFileClass
{
  ThunarFileClass __parent__;
};

struct _ThunarLocalFile
{
  ThunarFile __parent__;

  gchar         *display_name;
  ExoMimeInfo   *mime_info;
  ThunarVfsInfo  info;
};



G_DEFINE_TYPE (ThunarLocalFile, thunar_local_file, THUNAR_TYPE_FILE);



static void
thunar_local_file_class_init (ThunarLocalFileClass *klass)
{
  ThunarFileClass *thunarfile_class;
  GObjectClass    *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_local_file_finalize;

  thunarfile_class = THUNAR_FILE_CLASS (klass);
  thunarfile_class->get_uri = thunar_local_file_get_uri;
  thunarfile_class->get_mime_info = thunar_local_file_get_mime_info;
  thunarfile_class->get_display_name = thunar_local_file_get_display_name;
  thunarfile_class->get_special_name = thunar_local_file_get_special_name;
  thunarfile_class->get_kind = thunar_local_file_get_kind;
  thunarfile_class->get_mode = thunar_local_file_get_mode;
  thunarfile_class->get_size = thunar_local_file_get_size;
  thunarfile_class->get_icon_name = thunar_local_file_get_icon_name;
}



static void
thunar_local_file_init (ThunarLocalFile *local_file)
{
  thunar_vfs_info_init (&local_file->info);
}



static void
thunar_local_file_finalize (GObject *object)
{
  ThunarLocalFile *local_file = THUNAR_LOCAL_FILE (object);

  /* free the mime info */
  if (G_LIKELY (local_file->mime_info != NULL))
    g_object_unref (G_OBJECT (local_file->mime_info));

  /* reset the vfs info (freeing the specific data) */
  thunar_vfs_info_reset (&local_file->info);

  /* free the display name */
  g_free (local_file->display_name);

  G_OBJECT_CLASS (thunar_local_file_parent_class)->finalize (object);
}



static ThunarVfsURI*
thunar_local_file_get_uri (ThunarFile *file)
{
  return THUNAR_LOCAL_FILE (file)->info.uri;
}



static ExoMimeInfo*
thunar_local_file_get_mime_info (ThunarFile *file)
{
  ThunarLocalFile *local_file = THUNAR_LOCAL_FILE (file);

  /* determine the mime type on-demand */
  if (G_UNLIKELY (local_file->mime_info == NULL))
    local_file->mime_info = thunar_vfs_info_get_mime_info (&local_file->info);

  return local_file->mime_info;
}



static const gchar*
thunar_local_file_get_display_name (ThunarFile *file)
{
  ThunarLocalFile *local_file = THUNAR_LOCAL_FILE (file);

  /* determine the display name on-demand */
  if (G_UNLIKELY (local_file->display_name == NULL))
    local_file->display_name = thunar_vfs_uri_get_display_name (local_file->info.uri);

  return local_file->display_name;
}



static const gchar*
thunar_local_file_get_special_name (ThunarFile *file)
{
  ThunarLocalFile *local_file = THUNAR_LOCAL_FILE (file);

  /* root and home dir have special names */
  if (thunar_vfs_uri_is_root (local_file->info.uri))
    return _("Filesystem");
  else if (thunar_vfs_uri_is_home (local_file->info.uri))
    return _("Home");

  return thunar_file_get_display_name (file);
}



static ThunarVfsFileMode
thunar_local_file_get_mode (ThunarFile *file)
{
  return THUNAR_LOCAL_FILE (file)->info.mode;
}



static ThunarVfsFileSize
thunar_local_file_get_size (ThunarFile *file)
{
  return THUNAR_LOCAL_FILE (file)->info.size;
}



static ThunarVfsFileType
thunar_local_file_get_kind (ThunarFile *file)
{
  return THUNAR_LOCAL_FILE (file)->info.type;
}



static const gchar*
thunar_local_file_get_icon_name (ThunarFile   *file,
                                 GtkIconTheme *icon_theme)
{
  ThunarLocalFile *local_file = THUNAR_LOCAL_FILE (file);
  ExoMimeInfo     *mime_info;

  /* special icon for the root node */
  if (G_UNLIKELY (thunar_vfs_uri_is_root (local_file->info.uri))
      && gtk_icon_theme_has_icon (icon_theme, "gnome-dev-harddisk"))
    {
      return "gnome-dev-harddisk";
    }

  /* special icon for the home node */
  if (G_UNLIKELY (thunar_vfs_uri_is_home (local_file->info.uri))
      && gtk_icon_theme_has_icon (icon_theme, "gnome-fs-home"))
    {
      return "gnome-fs-home";
    }

  /* default is the mime type icon */
  mime_info = thunar_file_get_mime_info (file);
  return exo_mime_info_lookup_icon_name (mime_info, icon_theme);
}



/**
 * thunar_local_file_new:
 * @uri   : the #ThunarVfsURI referrring to the local file.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarLocalFile object for the given @uri.
 * Returns %NULL on error, and sets @error to point to an
 * #GError object describing the cause, if the operation fails.
 *
 * You should not ever call this function directly. Instead
 * use the #thunar_file_get_for_uri method, which performs
 * some caching of #ThunarFile objects.
 *
 * Return value: the newly allocated #ThunarLocalFile instance
 *               or %NULL on error.
 **/
ThunarFile*
thunar_local_file_new (ThunarVfsURI *uri,
                       GError      **error)
{
  ThunarLocalFile *local_file;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_FILE, NULL);

  /* allocate the new object */
  local_file = g_object_new (THUNAR_TYPE_LOCAL_FILE, NULL);

  /* query the file info */
  if (!thunar_vfs_info_query (&local_file->info, uri, error))
    {
      g_object_unref (G_OBJECT (local_file));
      return NULL;
    }

  return THUNAR_FILE (local_file);
}




