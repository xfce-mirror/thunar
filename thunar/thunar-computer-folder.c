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

#include <thunar/thunar-computer-folder.h>
#include <thunar/thunar-folder.h>



enum
{
  PROP_0,
  PROP_LOADING,
};



static void               thunar_computer_folder_class_init             (ThunarComputerFolderClass *klass);
static void               thunar_computer_folder_folder_init            (ThunarFolderIface         *iface);
static void               thunar_computer_folder_init                   (ThunarComputerFolder      *computer_folder);
static void               thunar_computer_folder_finalize               (GObject                   *object);
static void               thunar_computer_folder_get_property           (GObject                   *object,
                                                                         guint                      prop_id,
                                                                         GValue                    *value,
                                                                         GParamSpec                *pspec);
static gboolean           thunar_computer_folder_has_parent             (ThunarFile                *file);
static ThunarFolder      *thunar_computer_folder_open_as_folder         (ThunarFile                *file,
                                                                         GError                   **error);
static ThunarVfsURI      *thunar_computer_folder_get_uri                (ThunarFile                *file);
static const gchar       *thunar_computer_folder_get_display_name       (ThunarFile                *file);
static ThunarVfsFileType  thunar_computer_folder_get_kind               (ThunarFile                *file);
static ThunarVfsFileMode  thunar_computer_folder_get_mode               (ThunarFile                *file);
static const gchar       *thunar_computer_folder_get_icon_name          (ThunarFile                *file,
                                                                         GtkIconTheme              *icon_theme);
static ThunarFile        *thunar_computer_folder_get_corresponding_file (ThunarFolder              *folder);
static GSList            *thunar_computer_folder_get_files              (ThunarFolder              *folder);
static gboolean           thunar_computer_folder_get_loading            (ThunarFolder              *folder);



struct _ThunarComputerFolderClass
{
  ThunarFileClass __parent__;
};

struct _ThunarComputerFolder
{
  ThunarFile __parent__;

  ThunarVfsURI *uri;
  GSList       *files;
};



G_DEFINE_TYPE_WITH_CODE (ThunarComputerFolder,
                         thunar_computer_folder,
                         THUNAR_TYPE_FILE,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_FOLDER,
                                                thunar_computer_folder_folder_init));



static void
thunar_computer_folder_class_init (ThunarComputerFolderClass *klass)
{
  ThunarFileClass *thunarfile_class;
  GObjectClass    *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_computer_folder_finalize;
  gobject_class->get_property = thunar_computer_folder_get_property;

  thunarfile_class = THUNAR_FILE_CLASS (klass);
  thunarfile_class->has_parent = thunar_computer_folder_has_parent;
  thunarfile_class->open_as_folder = thunar_computer_folder_open_as_folder;
  thunarfile_class->get_uri = thunar_computer_folder_get_uri;
  thunarfile_class->get_display_name = thunar_computer_folder_get_display_name;
  thunarfile_class->get_kind = thunar_computer_folder_get_kind;
  thunarfile_class->get_mode = thunar_computer_folder_get_mode;
  thunarfile_class->get_icon_name = thunar_computer_folder_get_icon_name;

  g_object_class_override_property (gobject_class,
                                    PROP_LOADING,
                                    "loading");
}



static void
thunar_computer_folder_folder_init (ThunarFolderIface *iface)
{
  iface->get_corresponding_file = thunar_computer_folder_get_corresponding_file;
  iface->get_files = thunar_computer_folder_get_files;
  iface->get_loading = thunar_computer_folder_get_loading;
}



static void
thunar_computer_folder_init (ThunarComputerFolder *computer_folder)
{
}



static void
thunar_computer_folder_finalize (GObject *object)
{
  ThunarComputerFolder *computer_folder = THUNAR_COMPUTER_FOLDER (object);
  GSList               *lp;

  /* free the virtual folder nodes */
  for (lp = computer_folder->files; lp != NULL; lp = lp->next)
    g_object_unref (G_OBJECT (lp->data));
  g_slist_free (computer_folder->files);

  /* release the folder's URI */
  thunar_vfs_uri_unref (computer_folder->uri);

  G_OBJECT_CLASS (thunar_computer_folder_parent_class)->finalize (object);
}



static void
thunar_computer_folder_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);

  switch (prop_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, thunar_folder_get_loading (folder));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_computer_folder_has_parent (ThunarFile *file)
{
  return FALSE;
}



static ThunarFolder*
thunar_computer_folder_open_as_folder (ThunarFile *file,
                                       GError    **error)
{
  return THUNAR_FOLDER (g_object_ref (G_OBJECT (file)));
}



static ThunarVfsURI*
thunar_computer_folder_get_uri (ThunarFile *file)
{
  return THUNAR_COMPUTER_FOLDER (file)->uri;
}



static const gchar*
thunar_computer_folder_get_display_name (ThunarFile *file)
{
  return _("Computer");
}



static ThunarVfsFileType
thunar_computer_folder_get_kind (ThunarFile *file)
{
  return THUNAR_VFS_FILE_TYPE_DIRECTORY;
}



static ThunarVfsFileMode
thunar_computer_folder_get_mode (ThunarFile *file)
{
  return THUNAR_VFS_FILE_MODE_USR_ALL;
}



static const gchar*
thunar_computer_folder_get_icon_name (ThunarFile   *file,
                                      GtkIconTheme *icon_theme)
{
  if (gtk_icon_theme_has_icon (icon_theme, "gnome-fs-client"))
    return "gnome-fs-client";

  return NULL;
}



static ThunarFile*
thunar_computer_folder_get_corresponding_file (ThunarFolder *folder)
{
  return THUNAR_FILE (folder);
}



static GSList*
thunar_computer_folder_get_files (ThunarFolder *folder)
{
  static const gchar * const identifiers[] = { "file:/", "trash:" };

  ThunarComputerFolder *computer_folder = THUNAR_COMPUTER_FOLDER (folder);
  ThunarVfsURI         *uri;
  ThunarFile           *file;
  guint                 n;

  /* load the list of files in the computer folder on-demand */
  if (G_UNLIKELY (computer_folder->files == NULL))
    {
      for (n = 0; n < G_N_ELEMENTS (identifiers); ++n)
        {
          uri = thunar_vfs_uri_new (identifiers[n], NULL);
          if (G_LIKELY (uri != NULL))
            {
              file = thunar_file_get_for_uri (uri, NULL);
              if (G_LIKELY (file != NULL))
                computer_folder->files = g_slist_append (computer_folder->files, file);
              thunar_vfs_uri_unref (uri);
            }
        }
    }

  return computer_folder->files;
}



static gboolean
thunar_computer_folder_get_loading (ThunarFolder *folder)
{
  return FALSE;
}



/**
 * thunar_computer_folder_new:
 * @uri   : the #ThunarVfsURI referring to the computer vfolder.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarComputerFolder object for the given @uri.
 * Currently only the root path is support for the 'computer://',
 * which contains links to the objects that life within the
 * virtual folder.
 *
 * Return value: the newly allocated #ThunarComputerFolder instance
 *               or %NULL on error.
 **/
ThunarFile*
thunar_computer_folder_new (ThunarVfsURI *uri,
                            GError      **error)
{
  ThunarComputerFolder *computer_folder;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_COMPUTER, NULL);

  /* verify that the uri refers to the root element in 'computer:' */
  if (!thunar_vfs_uri_is_root (uri))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, g_strerror (ENOENT));
      return NULL;
    }

  /* allocate the new object */
  computer_folder = g_object_new (THUNAR_TYPE_COMPUTER_FOLDER, NULL);
  computer_folder->uri = thunar_vfs_uri_ref (uri);

  return THUNAR_FILE (computer_folder);
}

