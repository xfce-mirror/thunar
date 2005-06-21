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

#include <thunar/thunar-folder.h>



enum
{
  FILES_ADDED,
  LAST_SIGNAL,
};



static void thunar_folder_base_init  (gpointer klass);
static void thunar_folder_class_init (gpointer klass);



static guint folder_signals[LAST_SIGNAL];



GType
thunar_folder_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarFolderIface),
        (GBaseInitFunc) thunar_folder_base_init,
        NULL,
        (GClassInitFunc) thunar_folder_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "ThunarFolder",
                                     &info, 0);

      g_type_interface_add_prerequisite (type, GTK_TYPE_OBJECT);
    }

  return type;
}



static void
thunar_folder_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      /**
       * ThunarFolder::files-added:
       *
       * Emitted by the #ThunarFolder implementations whenever new files have
       * been added to a particular folder.
       **/
      folder_signals[FILES_ADDED] =
        g_signal_new ("files-added",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarFolderIface, files_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

      initialized = TRUE;
    }
}



static void
thunar_folder_class_init (gpointer klass)
{
  /**
   * ThunarFolder:corresponding-file:
   *
   * The #ThunarFile corresponding to this #ThunarFolder instance.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("corresponding-file",
                                                            _("Corresponding file"),
                                                            _("The file corresponding to a folder"),
                                                            THUNAR_TYPE_FILE,
                                                            EXO_PARAM_READABLE));

  /**
   * ThunarFolder:files:
   *
   * The list of files currently known for the #ThunarFolder.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_pointer ("files",
                                                             _("Files"),
                                                             _("The files in the folder"),
                                                             EXO_PARAM_READABLE));
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
  return THUNAR_FOLDER_GET_IFACE (folder)->get_corresponding_file (folder);
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
  return THUNAR_FOLDER_GET_IFACE (folder)->get_files (folder);
}



/**
 * thunar_folder_files_added:
 * @folder : a #ThunarFolder instance.
 * @files  : the list of added #ThunarFile<!---->s.
 *
 * Emits the ::files-added signal on @folder using the given
 * @files. This should only be called by #ThunarFolder
 * implementations.
 **/
void
thunar_folder_files_added (ThunarFolder *folder,
                           GSList       *files)
{
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (files != NULL);

  g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, files);
}


