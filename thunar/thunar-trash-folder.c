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

#include <thunar/thunar-application.h>
#include <thunar/thunar-folder.h>
#include <thunar/thunar-trash-file.h>
#include <thunar/thunar-trash-folder.h>



enum
{
  PROP_0,
  PROP_LOADING,
};



static void               thunar_trash_folder_class_init              (ThunarTrashFolderClass *klass);
static void               thunar_trash_folder_folder_init             (ThunarFolderIface      *iface);
static void               thunar_trash_folder_init                    (ThunarTrashFolder      *trash_folder);
static void               thunar_trash_folder_finalize                (GObject                *object);
static void               thunar_trash_folder_get_property            (GObject                *object,
                                                                       guint                   prop_id,
                                                                       GValue                 *value,
                                                                       GParamSpec             *pspec);
static ThunarFile        *thunar_trash_folder_get_parent              (ThunarFile             *file,
                                                                       GError                **error);
static ThunarFolder      *thunar_trash_folder_open_as_folder          (ThunarFile             *file,
                                                                       GError                **error);
static ThunarVfsURI      *thunar_trash_folder_get_uri                 (ThunarFile             *file);
static ThunarVfsMimeInfo *thunar_trash_folder_get_mime_info           (ThunarFile             *file);
static const gchar       *thunar_trash_folder_get_display_name        (ThunarFile             *file);
static ThunarVfsFileType  thunar_trash_folder_get_kind                (ThunarFile             *file);
static ThunarVfsFileMode  thunar_trash_folder_get_mode                (ThunarFile             *file);
static GList             *thunar_trash_folder_get_actions             (ThunarFile             *file,
                                                                       GtkWidget              *window);
static const gchar       *thunar_trash_folder_get_icon_name           (ThunarFile             *file,
                                                                       ThunarFileIconState     icon_state,
                                                                       GtkIconTheme           *icon_theme);
static ThunarFile        *thunar_trash_folder_get_corresponding_file  (ThunarFolder           *folder);
static GList             *thunar_trash_folder_get_files               (ThunarFolder           *folder);
static gboolean           thunar_trash_folder_get_loading             (ThunarFolder           *folder);
static void               thunar_trash_folder_action_empty            (GtkAction              *action,
                                                                       GtkWindow              *window);
static void               thunar_trash_folder_file_destroy            (ThunarFile             *file,
                                                                       ThunarTrashFolder      *trash_folder);



struct _ThunarTrashFolderClass
{
  ThunarFileClass __parent__;
};

struct _ThunarTrashFolder
{
  ThunarFile __parent__;

  ThunarVfsTrashManager *manager;
  ThunarVfsURI          *uri;
  GList                 *files;
};



static GQuark thunar_trash_folder_action_quark;

G_DEFINE_TYPE_WITH_CODE (ThunarTrashFolder,
                         thunar_trash_folder,
                         THUNAR_TYPE_FILE,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_FOLDER,
                                                thunar_trash_folder_folder_init));



static void
thunar_trash_folder_class_init (ThunarTrashFolderClass *klass)
{
  ThunarFileClass *thunarfile_class;
  GObjectClass    *gobject_class;

  /* setup the thunar-trash-folder-action quark, which is
   * used to connect the custom actions to the folder instance.
   */
  thunar_trash_folder_action_quark = g_quark_from_static_string ("thunar-trash-folder-action");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_trash_folder_finalize;
  gobject_class->get_property = thunar_trash_folder_get_property;

  thunarfile_class = THUNAR_FILE_CLASS (klass);
  thunarfile_class->get_parent = thunar_trash_folder_get_parent;
  thunarfile_class->open_as_folder = thunar_trash_folder_open_as_folder;
  thunarfile_class->get_uri = thunar_trash_folder_get_uri;
  thunarfile_class->get_mime_info = thunar_trash_folder_get_mime_info;
  thunarfile_class->get_display_name = thunar_trash_folder_get_display_name;
  thunarfile_class->get_kind = thunar_trash_folder_get_kind;
  thunarfile_class->get_mode = thunar_trash_folder_get_mode;
  thunarfile_class->get_actions = thunar_trash_folder_get_actions;
  thunarfile_class->get_icon_name = thunar_trash_folder_get_icon_name;

  g_object_class_override_property (gobject_class,
                                    PROP_LOADING,
                                    "loading");
}



static void
thunar_trash_folder_folder_init (ThunarFolderIface *iface)
{
  iface->get_corresponding_file = thunar_trash_folder_get_corresponding_file;
  iface->get_files = thunar_trash_folder_get_files;
  iface->get_loading = thunar_trash_folder_get_loading;
}



static void
thunar_trash_folder_init (ThunarTrashFolder *trash_folder)
{
  /* register with the trash manager */
  trash_folder->manager = thunar_vfs_trash_manager_get_default ();
  g_signal_connect_swapped (G_OBJECT (trash_folder->manager), "notify::empty",
                            G_CALLBACK (thunar_file_changed), trash_folder);
}



static void
thunar_trash_folder_finalize (GObject *object)
{
  ThunarTrashFolder *trash_folder = THUNAR_TRASH_FOLDER (object);
  GList             *lp;

  /* drop the files list */
  for (lp = trash_folder->files; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), thunar_trash_folder_file_destroy, trash_folder);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (trash_folder->files);

  /* unregister from the trash manager */
  g_signal_handlers_disconnect_by_func (G_OBJECT (trash_folder->manager), thunar_file_changed, trash_folder);
  g_object_unref (G_OBJECT (trash_folder->manager));

  /* release the trash URI */
  thunar_vfs_uri_unref (trash_folder->uri);

  (*G_OBJECT_CLASS (thunar_trash_folder_parent_class)->finalize) (object);
}



static void
thunar_trash_folder_get_property (GObject    *object,
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



static ThunarFile*
thunar_trash_folder_get_parent (ThunarFile *file,
                                GError    **error)
{
  ThunarVfsURI *computer_uri;
  ThunarFile   *computer_file = NULL;

  /* the 'computer:' location is the parent for the trash vfolder */
  computer_uri = thunar_vfs_uri_new ("computer:", error);
  if (G_LIKELY (computer_uri != NULL))
    {
      computer_file = thunar_file_get_for_uri (computer_uri, error);
      thunar_vfs_uri_unref (computer_uri);
    }
  else
    {
      computer_file = NULL;
    }

  return computer_file;
}



static ThunarFolder*
thunar_trash_folder_open_as_folder (ThunarFile *file,
                                    GError    **error)
{
  return THUNAR_FOLDER (g_object_ref (G_OBJECT (file)));
}



static ThunarVfsURI*
thunar_trash_folder_get_uri (ThunarFile *file)
{
  return THUNAR_TRASH_FOLDER (file)->uri;
}



static ThunarVfsMimeInfo*
thunar_trash_folder_get_mime_info (ThunarFile *file)
{
  ThunarVfsMimeDatabase *database;
  ThunarVfsMimeInfo     *info;

  database = thunar_vfs_mime_database_get_default ();
  info = thunar_vfs_mime_database_get_info (database, "inode/directory");
  g_object_unref (G_OBJECT (database));

  return info;
}



static const gchar*
thunar_trash_folder_get_display_name (ThunarFile *file)
{
  return _("Trash");
}



static ThunarVfsFileType
thunar_trash_folder_get_kind (ThunarFile *file)
{
  return THUNAR_VFS_FILE_TYPE_DIRECTORY;
}



static ThunarVfsFileMode
thunar_trash_folder_get_mode (ThunarFile *file)
{
  return THUNAR_VFS_FILE_MODE_USR_ALL;
}



static GList*
thunar_trash_folder_get_actions (ThunarFile *file,
                                 GtkWidget  *window)
{
  ThunarTrashFolder *trash_folder = THUNAR_TRASH_FOLDER (file);
  GtkAction         *action;

  /* prepare the "Empty Trash Bin" action */
  action = gtk_action_new ("ThunarTrashFolder::empty-trash-bin", _("_Empty Trash Bin"), _("Delete all items in the Trash"), NULL);
  g_object_set_qdata_full (G_OBJECT (action), thunar_trash_folder_action_quark, g_object_ref (G_OBJECT (trash_folder)), g_object_unref);
  exo_binding_new_with_negation (G_OBJECT (trash_folder->manager), "empty", G_OBJECT (action), "sensitive");
  g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (thunar_trash_folder_action_empty), window);

  return g_list_prepend (NULL, action);
}



static const gchar*
thunar_trash_folder_get_icon_name (ThunarFile         *file,
                                   ThunarFileIconState icon_state,
                                   GtkIconTheme       *icon_theme)
{
  ThunarTrashFolder *trash_folder = THUNAR_TRASH_FOLDER (file);
  const gchar       *icon_name = NULL;

  if (G_UNLIKELY (icon_state == THUNAR_FILE_ICON_STATE_DROP))
    {
      icon_name = thunar_vfs_trash_manager_is_empty (trash_folder->manager)
                ? "gnome-fs-trash-empty-accept" : "gnome-fs-trash-full-accept";
      if (G_UNLIKELY (!gtk_icon_theme_has_icon (icon_theme, icon_name)))
        icon_name = NULL;
    }

  if (G_UNLIKELY (icon_name == NULL))
    {
      icon_name = thunar_vfs_trash_manager_is_empty (trash_folder->manager)
                ? "gnome-fs-trash-empty" : "gnome-fs-trash-full";
    }

  return icon_name;
}



static ThunarFile*
thunar_trash_folder_get_corresponding_file (ThunarFolder *folder)
{
  return THUNAR_FILE (folder);
}



static GList *
thunar_trash_folder_get_files (ThunarFolder *folder)
{
  ThunarTrashFolder *trash_folder = THUNAR_TRASH_FOLDER (folder);
  ThunarVfsTrash    *trash;
  ThunarVfsURI      *uri;
  ThunarFile        *file;
  GList             *trashes;
  GList             *files;
  GList             *tp;
  GList             *fp;

  if (trash_folder->files == NULL)
    {
      trashes = thunar_vfs_trash_manager_get_trashes (trash_folder->manager);
      for (tp = trashes; tp != NULL; tp = tp->next)
        {
          trash = tp->data;
          files = thunar_vfs_trash_get_files (trash);
          for (fp = files; fp != NULL; fp = fp->next)
            {
              uri = thunar_vfs_trash_get_uri (trash, fp->data);
              file = thunar_file_get_for_uri (uri, NULL);
              if (file != NULL)
                {
                  /* watch the lifecycle of the file */
                  g_signal_connect (G_OBJECT (file), "destroy", G_CALLBACK (thunar_trash_folder_file_destroy), trash_folder);

                  /* add the file to our internal list */
                  trash_folder->files = g_list_prepend (trash_folder->files, file);
                }
              thunar_vfs_uri_unref (uri);
            }
          g_object_unref (G_OBJECT (trash));
        }
      g_list_free (trashes);
    }

  return trash_folder->files;
}



static gboolean
thunar_trash_folder_get_loading (ThunarFolder *folder)
{
  return FALSE;
}



static void
thunar_trash_folder_action_empty (GtkAction *action,
                                  GtkWindow *window)
{
  ThunarApplication *application;
  ThunarTrashFolder *trash_folder;
  GtkWidget         *message;
  GList             *uri_list;
  GList             *files;
  GList             *lp;
  gint               response;
  gint               n_files;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_WINDOW (window));

  /* grab the pointer on the trash folder for the action */
  trash_folder = g_object_get_qdata (G_OBJECT (action), thunar_trash_folder_action_quark);

  /* determine the files currently stored in the trash can */
  files = thunar_folder_get_files (THUNAR_FOLDER (trash_folder));
  n_files = g_list_length (files);
  if (G_UNLIKELY (n_files == 0))
    return;

  /* ask the user whether to empty the trash can */
  message = gtk_message_dialog_new (GTK_WINDOW (window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT
                                    | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("Are you sure that you want to delete all trashed files?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
                                            ngettext ("This will delete %d file currently located in the trash bin.",
                                                      "This will delete %d files currently located in the trash bin.",
                                                      n_files),
                                            n_files);
  response = gtk_dialog_run (GTK_DIALOG (message));
  gtk_widget_destroy (message);
  
  /* check if we should delete all trashed files */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      /* determine the URIs for the trashed files */
      for (lp = files, uri_list = NULL; lp != NULL; lp = lp->next)
        uri_list = g_list_prepend (uri_list, thunar_file_get_uri (lp->data));

      /* perform the unlink operation */
      application = thunar_application_get ();
      thunar_application_delete_uris (application, window, uri_list);
      g_object_unref (G_OBJECT (application));

      /* cleanup */
      g_list_free (uri_list);
    }
}



static void
thunar_trash_folder_file_destroy (ThunarFile        *file,
                                  ThunarTrashFolder *trash_folder)
{
  GList files;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_TRASH_FOLDER (trash_folder));
  g_return_if_fail (g_list_find (trash_folder->files, file) != NULL);

  /* disconnect from the file */
  g_signal_handlers_disconnect_by_func (G_OBJECT (file), thunar_trash_folder_file_destroy, trash_folder);
  trash_folder->files = g_list_remove (trash_folder->files, file);

  /* tell everybody that the file is gone */
  files.data = file; files.next = files.prev = NULL;
  thunar_folder_files_removed (THUNAR_FOLDER (trash_folder), &files);

  /* drop our reference on the fole */
  g_object_unref (G_OBJECT (file));
}



/**
 * thunar_trash_folder_new:
 * @uri   : the #ThunarVfsURI referrring to the trash file.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarTrashFolder object for the given @uri.
 * Returns %NULL on error, and sets @error to point to an
 * #GError object describing the cause, if the operation fails.
 *
 * You should not ever call this function directly. Instead
 * use the #thunar_file_get_for_uri method, which performs
 * some caching of #ThunarFile objects.
 *
 * Return value: the newly allocated #ThunarTrashFolder instance
 *               or %NULL on error.
 **/
ThunarFile*
thunar_trash_folder_new (ThunarVfsURI *uri,
                         GError      **error)
{
  ThunarTrashFolder *trash_folder;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (thunar_vfs_uri_get_scheme (uri) == THUNAR_VFS_URI_SCHEME_TRASH, NULL);

  /* the ThunarTrashFile class is responsible for items within the trash */
  if (!thunar_vfs_uri_is_root (uri))
    return thunar_trash_file_new (uri, error);

  /* allocate the new object */
  trash_folder = g_object_new (THUNAR_TYPE_TRASH_FOLDER, NULL);
  trash_folder->uri = thunar_vfs_uri_ref (uri);

  return THUNAR_FILE (trash_folder);
}




