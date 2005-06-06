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

#include <thunar/thunar-folder.h>



enum
{
  PROP_0,
  PROP_CORRESPONDING_FILE,
  PROP_FILES,
};

enum
{
  FILES_ADDED,
  LAST_SIGNAL,
};



static void     thunar_folder_class_init                  (ThunarFolderClass  *klass);
static void     thunar_folder_init                        (ThunarFolder       *folder);
static void     thunar_folder_finalize                    (GObject            *object);
static void     thunar_folder_get_property                (GObject            *object,
                                                           guint               prop_id,
                                                           GValue             *value,
                                                           GParamSpec         *pspec);
static gboolean thunar_folder_rescan                      (ThunarFolder       *folder,
                                                           GError            **error);
static void     thunar_folder_corresponding_file_changed  (ThunarFile         *file,
                                                           ThunarFolder       *folder);
static void     thunar_folder_corresponding_file_destroy  (ThunarFile         *file,
                                                           ThunarFolder       *folder);
static void     thunar_folder_file_destroy                (ThunarFile         *file,
                                                           ThunarFolder       *folder);



struct _ThunarFolderClass
{
  GtkObjectClass __parent__;

  void (*files_added) (ThunarFolder *folder, GSList *files);
};

struct _ThunarFolder
{
  GtkObject __parent__;

  ThunarFile *corresponding_file;
  GSList     *files;
};



static GQuark folder_quark = 0;
static guint  folder_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarFolder, thunar_folder, GTK_TYPE_OBJECT);



static void
thunar_folder_class_init (ThunarFolderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_folder_finalize;
  gobject_class->get_property = thunar_folder_get_property;

  /**
   * ThunarFolder:corresponding-file:
   *
   * The #ThunarFile corresponding to this #ThunarFolder instance.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CORRESPONDING_FILE,
                                   g_param_spec_object ("corresponding-file",
                                                        _("Corresponding file"),
                                                        _("The file corresponding to this folder"),
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READABLE));

  /**
   * ThunarFolder:files:
   *
   * The list of files currently known for the #ThunarFolder.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILES,
                                   g_param_spec_pointer ("files",
                                                         _("Files in the folder"),
                                                         _("The list of files currently known for the folder"),
                                                         G_PARAM_READABLE));

  /**
   * ThunarFolder::files-added:
   *
   * Invoked whenever new files have been added to the folder (also
   * during loading state).
   **/
  folder_signals[FILES_ADDED] =
    g_signal_new ("files-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFolderClass, files_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
thunar_folder_init (ThunarFolder *folder)
{
}



static void
thunar_folder_finalize (GObject *object)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);
  GSList       *lp;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* disconnect from the corresponding file */
  if (G_LIKELY (folder->corresponding_file != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (folder->corresponding_file),
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);

      g_object_set_qdata (G_OBJECT (folder->corresponding_file),
                          folder_quark, NULL);
      g_object_unref (G_OBJECT (folder->corresponding_file));
    }

  /* release references to the files */
  for (lp = folder->files; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (lp->data),
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, folder);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_slist_free (folder->files);

  G_OBJECT_CLASS (thunar_folder_parent_class)->finalize (object);
}



static void
thunar_folder_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);

  switch (prop_id)
    {
    case PROP_CORRESPONDING_FILE:
      g_value_set_object (value, folder->corresponding_file);
      break;

    case PROP_FILES:
      g_value_set_pointer (value, folder->files);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_folder_rescan (ThunarFolder *folder,
                      GError      **error)
{
  ThunarVfsURI  *folder_uri;
  ThunarVfsURI  *file_uri;
  const gchar   *name;
  ThunarFile    *file;
  GSList        *nfiles = NULL;
  GSList        *ofiles;
  GSList        *lp;
  GDir          *dp;

  /* remember the previously known items */
  ofiles = folder->files;
  folder->files = NULL;

  folder_uri = thunar_file_get_uri (folder->corresponding_file);
  dp = g_dir_open (thunar_vfs_uri_get_path (folder_uri), 0, error);
  if (G_UNLIKELY (dp == NULL))
    return FALSE;

  for (;;)
    {
      name = g_dir_read_name (dp);
      if (G_UNLIKELY (name == NULL))
        break;

      /* ignore ".." and "." entries */
      if (G_UNLIKELY (name[0] == '.' && (name[1] == '\0' ||
              (name[1] == '.' && name[2] == '\0'))))
        continue;

      /* check if the item was already present */
      for (lp = ofiles; lp != NULL; lp = lp->next)
        if (exo_str_is_equal (thunar_file_get_name (lp->data), name))
          break;

      if (lp != NULL)
        {
          /* the file was already present, relink to active items list */
          ofiles = g_slist_remove_link (ofiles, lp);
          lp->next = folder->files;
          folder->files = lp;
        }
      else
        {
          /* we discovered a new file */
          file_uri = thunar_vfs_uri_relative (folder_uri, name);
          file = thunar_file_get_for_uri (file_uri, NULL);
          g_object_unref (G_OBJECT (file_uri));

          if (G_UNLIKELY (file == NULL))
            continue;

          nfiles = g_slist_prepend (nfiles, file);

          g_signal_connect (G_OBJECT (file), "destroy",
                            G_CALLBACK (thunar_folder_file_destroy), folder);
        }
    }

  g_dir_close (dp);

  /* notify listeners about removed files */
  for (lp = ofiles; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);
      
      g_signal_handlers_disconnect_matched
        (G_OBJECT (file), G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
         0, 0, NULL, thunar_folder_file_destroy, folder);
      gtk_object_destroy (GTK_OBJECT (file));
      g_object_unref (G_OBJECT (file));
    }
  g_slist_free (ofiles);

  /* notify listening parties about added files and append the
   * new files to our internal file list.
   */
  if (G_LIKELY (nfiles != NULL))
    {
      folder->files = g_slist_concat (folder->files, nfiles);
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, nfiles);
    }

  g_object_notify (G_OBJECT (folder), "files");

  return TRUE;
}



static void
thunar_folder_corresponding_file_changed (ThunarFile   *file,
                                          ThunarFolder *folder)
{
  // TODO: Rescan
}



static void
thunar_folder_corresponding_file_destroy (ThunarFile   *file,
                                          ThunarFolder *folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (folder->corresponding_file == file);

  // TODO: Error or Destroy?
}



static void
thunar_folder_file_destroy (ThunarFile   *file,
                            ThunarFolder *folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (g_slist_find (folder->files, file) != NULL);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (file), G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
     0, 0, NULL, thunar_folder_file_destroy, folder);
  folder->files = g_slist_remove (folder->files, file);
  g_object_unref (G_OBJECT (file));
}



/**
 * thunar_folder_get_for_file:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors.
 *
 * This function automatically takes a reference on the returned
 * #ThunarFile for the caller, which means, you'll have to call
 * #g_object_unref() on the returned object when you are done
 * with it.
 *
 * Return value: the folder corresponding to @file or %NULL in case
 *               of an error.
 **/
ThunarFolder*
thunar_folder_get_for_file (ThunarFile *file,
                            GError    **error)
{
  ThunarFolder *folder;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (G_UNLIKELY (folder_quark == 0))
    folder_quark = g_quark_from_static_string ("thunar-folder");

  folder = g_object_get_qdata (G_OBJECT (file), folder_quark);
  if (folder != NULL)
    {
      g_object_ref (G_OBJECT (folder));
    }
  else
    {
      /* allocate the new instance */
      folder = g_object_new (THUNAR_TYPE_FOLDER, NULL);
      folder->corresponding_file = file;
      g_object_ref (G_OBJECT (file));

      /* drop the floating reference */
      g_object_ref (G_OBJECT (folder));
      gtk_object_sink (GTK_OBJECT (folder));

      g_signal_connect (G_OBJECT (file), "changed", G_CALLBACK (thunar_folder_corresponding_file_changed), folder);
      g_signal_connect (G_OBJECT (file), "destroy", G_CALLBACK (thunar_folder_corresponding_file_destroy), folder);

      g_object_set_qdata (G_OBJECT (file), folder_quark, folder);

      /* try to scan the new folder */
      if (G_UNLIKELY (!thunar_folder_rescan (folder, error)))
        {
          g_object_unref (G_OBJECT (folder));
          return NULL;
        }
    }

  return folder;
}



/**
 * thunar_folder_get_for_uri:
 * @uri   : a #ThunarVfsURI.
 * @error : return location for errors or %NULL.
 *
 * Wrapper function to #thunar_folder_get_for_file().
 *
 * Return value: the #ThunarFolder instance or %NULL in case of an error.
 **/
ThunarFolder*
thunar_folder_get_for_uri (ThunarVfsURI *uri,
                           GError      **error)
{
  ThunarFolder *folder = NULL;
  ThunarFile   *file;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  file = thunar_file_get_for_uri (uri, error);
  if (G_LIKELY (file != NULL))
    {
      folder = thunar_folder_get_for_file (file, error);
      g_object_unref (G_OBJECT (file));
    }
  return folder;
}



/**
 * thunar_folder_get_corresponding_file:
 * @folder : a #ThunarFolder instance.
 *
 * Returns the #ThunarFile corresponding to this @folder.
 *
 * No reference is taken on the returned #ThunarFile for
 * the caller, so if you need a persistent reference to
 * the file, you'll have to call #g_object_ref() yourself.
 *
 * Return value: the #ThunarFile corresponding to @folder.
 **/
ThunarFile*
thunar_folder_get_corresponding_file (ThunarFolder *folder)
{
  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return folder->corresponding_file;
}



/**
 * thunar_folder_get_files:
 * @folder : a #ThunarFolder instance.
 *
 * Returns the list of files currently known for @folder.
 * The returned list is owned by @folder and may not be freed!
 *
 * Return value: the list of #ThunarFiles for @folder.
 **/
GSList*
thunar_folder_get_files (ThunarFolder *folder)
{
  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return folder->files;
}


