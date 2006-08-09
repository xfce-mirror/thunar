/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-folder.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>



/* property identifiers */
enum
{
  PROP_0,
  PROP_LOADING,
};

/* signal identifiers */
enum
{
  ERROR,
  FILES_ADDED,
  FILES_REMOVED,
  LAST_SIGNAL,
};



static void     thunar_folder_class_init                  (ThunarFolderClass      *klass);
static void     thunar_folder_init                        (ThunarFolder           *folder);
static void     thunar_folder_finalize                    (GObject                *object);
static void     thunar_folder_get_property                (GObject                *object,
                                                           guint                   prop_id,
                                                           GValue                 *value,
                                                           GParamSpec             *pspec);
static void     thunar_folder_error                       (ThunarVfsJob           *job,
                                                           GError                 *error,
                                                           ThunarFolder           *folder);
static gboolean thunar_folder_infos_ready                 (ThunarVfsJob           *job,
                                                           GList                  *infos,
                                                           ThunarFolder           *folder);
static void     thunar_folder_finished                    (ThunarVfsJob           *job,
                                                           ThunarFolder           *folder);
static void     thunar_folder_file_changed                (ThunarFileMonitor      *file_monitor,
                                                           ThunarFile             *file,
                                                           ThunarFolder           *folder);
static void     thunar_folder_file_destroyed              (ThunarFileMonitor      *file_monitor,
                                                           ThunarFile             *file,
                                                           ThunarFolder           *folder);
static void     thunar_folder_monitor                     (ThunarVfsMonitor       *monitor,
                                                           ThunarVfsMonitorHandle *handle,
                                                           ThunarVfsMonitorEvent   event,
                                                           ThunarVfsPath          *handle_path,
                                                           ThunarVfsPath          *event_path,
                                                           gpointer                user_data);



struct _ThunarFolderClass
{
  GtkObjectClass __parent__;

  /* signals */
  void (*error)         (ThunarFolder *folder,
                         const GError *error);
  void (*files_added)   (ThunarFolder *folder,
                         GList        *files);
  void (*files_removed) (ThunarFolder *folder,
                         GList        *files);
};

struct _ThunarFolder
{
  GtkObject __parent__;

  ThunarVfsJob           *job;

  ThunarFile             *corresponding_file;
  GList                  *new_files;
  GList                  *files;

  ThunarFileMonitor      *file_monitor;

  ThunarVfsMonitor       *monitor;
  ThunarVfsMonitorHandle *handle;
};



static GObjectClass *thunar_folder_parent_class;
static guint         folder_signals[LAST_SIGNAL];
static GQuark        thunar_folder_quark;



GType
thunar_folder_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarFolderClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_folder_class_init,
        NULL,
        NULL,
        sizeof (ThunarFolder),
        0,
        (GInstanceInitFunc) thunar_folder_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_OBJECT, I_("ThunarFolder"), &info, 0);
    }

  return type;
}



static void
thunar_folder_class_init (ThunarFolderClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_folder_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_folder_finalize;
  gobject_class->get_property = thunar_folder_get_property;

  /**
   * ThunarFolder::loading:
   *
   * Tells whether the contents of the #ThunarFolder are
   * currently being loaded.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         "loading",
                                                         "loading",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  /**
   * ThunarFolder::error:
   * @folder : a #ThunarFolder.
   * @error  : the #GError describing the problem.
   *
   * Emitted when the #ThunarFolder fails to completly
   * load the directory content because of an error.
   **/
  folder_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFolderClass, error),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarFolder::files-added:
   *
   * Emitted by the #ThunarFolder whenever new files have
   * been added to a particular folder.
   **/
  folder_signals[FILES_ADDED] =
    g_signal_new (I_("files-added"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFolderClass, files_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarFolder::files-removed:
   *
   * Emitted by the #ThunarFolder whenever a bunch of files
   * is removed from the folder, which means they are not
   * necessarily deleted from disk. This can be used to implement
   * the reload of folders, which take longer to load.
   **/
  folder_signals[FILES_REMOVED] =
    g_signal_new (I_("files-removed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFolderClass, files_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
thunar_folder_init (ThunarFolder *folder)
{
  /* connect to the ThunarFileMonitor instance */
  folder->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (folder->file_monitor), "file-changed", G_CALLBACK (thunar_folder_file_changed), folder);
  g_signal_connect (G_OBJECT (folder->file_monitor), "file-destroyed", G_CALLBACK (thunar_folder_file_destroyed), folder);

  /* connect to the file alteration monitor */
  folder->monitor = thunar_vfs_monitor_get_default ();
}



static void
thunar_folder_finalize (GObject *object)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);

  /* disconnect from the ThunarFileMonitor instance */
  g_signal_handlers_disconnect_matched (G_OBJECT (folder->file_monitor), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
  g_object_unref (G_OBJECT (folder->file_monitor));

  /* disconnect from the file alteration monitor */
  if (G_LIKELY (folder->handle != NULL))
    thunar_vfs_monitor_remove (folder->monitor, folder->handle);
  g_object_unref (G_OBJECT (folder->monitor));

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (folder->job != NULL))
    {
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      thunar_vfs_job_cancel (folder->job);
      g_object_unref (G_OBJECT (folder->job));
    }

  /* disconnect from the corresponding file */
  if (G_LIKELY (folder->corresponding_file != NULL))
    {
      /* drop the reference */
      g_object_set_qdata (G_OBJECT (folder->corresponding_file), thunar_folder_quark, NULL);
      g_object_unref (G_OBJECT (folder->corresponding_file));
    }

  /* release references to the new files */
  thunar_file_list_free (folder->new_files);

  /* release references to the current files */
  thunar_file_list_free (folder->files);

  (*G_OBJECT_CLASS (thunar_folder_parent_class)->finalize) (object);
}



static void
thunar_folder_get_property (GObject    *object,
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



static void
thunar_folder_error (ThunarVfsJob *job,
                     GError       *error,
                     ThunarFolder *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_VFS_IS_JOB (job));
  _thunar_return_if_fail (folder->job == job);

  /* tell the consumer about the problem */
  g_signal_emit (G_OBJECT (folder), folder_signals[ERROR], 0, error);
}



static gboolean
thunar_folder_infos_ready (ThunarVfsJob *job,
                           GList        *infos,
                           ThunarFolder *folder)
{
  ThunarFile *file;
  GList      *lp;

  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), FALSE);
  _thunar_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (folder->handle == NULL, FALSE);
  _thunar_return_val_if_fail (folder->job == job, FALSE);

  /* turn the info list into a file list */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      /* get the file corresponding to the info... */
      file = thunar_file_get_for_info (lp->data);

      /* ...release the info at the list position... */
      thunar_vfs_info_unref (lp->data);

      /* ...and replace it with the file */
      lp->data = file;
    }

  /* merge the list with the existing list of new files */
  folder->new_files = g_list_concat (folder->new_files, infos);

  /* TRUE to indicate that we took over ownership of the infos list */
  return TRUE;
}



static void
thunar_folder_finished (ThunarVfsJob *job,
                        ThunarFolder *folder)
{
  ThunarFile *file;
  GList      *files;
  GList      *lp;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_VFS_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_FILE (folder->corresponding_file));
  _thunar_return_if_fail (folder->handle == NULL);
  _thunar_return_if_fail (folder->job == job);

  /* check if we need to merge new files with existing files */
  if (G_UNLIKELY (folder->files != NULL))
    {
      /* determine all added files (files on new_files, but not on files) */
      for (files = NULL, lp = folder->new_files; lp != NULL; lp = lp->next)
        if (g_list_find (folder->files, lp->data) == NULL)
          {
            /* put the file on the added list */
            files = g_list_prepend (files, lp->data);

            /* add to the internal files list */
            folder->files = g_list_prepend (folder->files, lp->data);
            g_object_ref (G_OBJECT (lp->data));
          }

      /* check if any files were added */
      if (G_UNLIKELY (files != NULL))
        {
          /* emit a "files-added" signal for the added files */
          g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, files);

          /* release the added files list */
          g_list_free (files);
        }

      /* determine all removed files (files on files, but not on new_files) */
      for (files = NULL, lp = folder->files; lp != NULL; )
        {
          /* determine the file */
          file = THUNAR_FILE (lp->data);

          /* determine the next list item */
          lp = lp->next;

          /* check if the file is not on new_files */
          if (g_list_find (folder->new_files, file) == NULL)
            {
              /* put the file on the removed list (owns the reference now) */
              files = g_list_prepend (files, file);

              /* remove from the internal files list */
              folder->files = g_list_remove (folder->files, file);
            }
        }

      /* check if any files were removed */
      if (G_UNLIKELY (files != NULL))
        {
          /* emit a "files-removed" signal for the removed files */
          g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, files);

          /* release the removed files list */
          thunar_file_list_free (files);
        }

      /* drop the temporary new_files list */
      thunar_file_list_free (folder->new_files);
      folder->new_files = NULL;
    }
  else
    {
      /* just use the new files for the files list */
      folder->files = folder->new_files;
      folder->new_files = NULL;

      /* emit a "files-added" signal for the new files */
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, folder->files);
    }

  /* we did it, the folder is loaded */
  g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
  g_object_unref (G_OBJECT (folder->job));
  folder->job = NULL;

  /* add us to the file alteration monitor */
  folder->handle = thunar_vfs_monitor_add_directory (folder->monitor, thunar_file_get_path (folder->corresponding_file),
                                                     thunar_folder_monitor, folder);

  /* tell the consumers that we have loaded the directory */
  g_object_notify (G_OBJECT (folder), "loading");
}



static void
thunar_folder_file_changed (ThunarFileMonitor *file_monitor,
                            ThunarFile        *file,
                            ThunarFolder      *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));

  /* check if the corresponding file changed... */
  if (G_UNLIKELY (folder->corresponding_file == file))
    {
      /* ...and if so, reload the folder */
      thunar_folder_reload (folder);
    }
}



static void
thunar_folder_file_destroyed (ThunarFileMonitor *file_monitor,
                              ThunarFile        *file,
                              ThunarFolder      *folder)
{
  GList  files;
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));

  /* check if the corresponding file was destroyed */
  if (G_UNLIKELY (folder->corresponding_file == file))
    {
      /* the folder is useless now */
      gtk_object_destroy (GTK_OBJECT (folder));
    }
  else
    {
      /* check if we have that file */
      lp = g_list_find (folder->files, file);
      if (G_LIKELY (lp != NULL))
        {
          /* remove the file from our list */
          folder->files = g_list_delete_link (folder->files, lp);

          /* tell everybody that the file is gone */
          files.data = file; files.next = files.prev = NULL;
          g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, &files);

          /* drop our reference to the file */
          g_object_unref (G_OBJECT (file));
        }
    }
}



static void
thunar_folder_monitor (ThunarVfsMonitor       *monitor,
                       ThunarVfsMonitorHandle *handle,
                       ThunarVfsMonitorEvent   event,
                       ThunarVfsPath          *handle_path,
                       ThunarVfsPath          *event_path,
                       gpointer                user_data)
{
  ThunarFolder *folder = THUNAR_FOLDER (user_data);
  ThunarFile   *file;
  GList        *lp;
  GList         list;

  _thunar_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (folder->monitor == monitor);
  _thunar_return_if_fail (folder->handle == handle);
  _thunar_return_if_fail (folder->job == NULL);

  /* check on which file the event occurred */
  if (!thunar_vfs_path_equal (event_path, thunar_file_get_path (folder->corresponding_file)))
    {
      /* check if we already ship the file */
      for (lp = folder->files; lp != NULL; lp = lp->next)
        if (thunar_vfs_path_equal (event_path, thunar_file_get_path (lp->data)))
          break;

      /* if we don't have it, add it if the event is not an "deleted" event */
      if (G_UNLIKELY (lp == NULL && event != THUNAR_VFS_MONITOR_EVENT_DELETED))
        {
          /* allocate a file for the path */
          file = thunar_file_get_for_path (event_path, NULL);
          if (G_UNLIKELY (file == NULL))
            return;

          /* prepend it to our internal list */
          folder->files = g_list_prepend (folder->files, file);

          /* tell others about the new file */
          list.data = file; list.next = list.prev = NULL;
          g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, &list);
        }
      else if (lp != NULL)
        {
          /* update/destroy the file */
          if (event == THUNAR_VFS_MONITOR_EVENT_DELETED)
            thunar_file_destroy (lp->data);
          else
            thunar_file_reload (lp->data);
        }
    }
  else
    {
      /* update/destroy the corresponding file */
      if (event == THUNAR_VFS_MONITOR_EVENT_DELETED)
        thunar_file_destroy (folder->corresponding_file);
      else
        thunar_file_reload (folder->corresponding_file);
    }
}



/**
 * thunar_folder_get_for_file:
 * @file : a #ThunarFile.
 *
 * Opens the specified @file as #ThunarFolder and
 * returns a reference to the folder.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer
 * needed.
 *
 * Return value: the #ThunarFolder which corresponds
 *               to @file.
 **/
ThunarFolder*
thunar_folder_get_for_file (ThunarFile *file)
{
  ThunarFolder *folder;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (thunar_file_is_directory (file), NULL);

  /* determine the "thunar-folder" quark on-demand */
  if (G_UNLIKELY (thunar_folder_quark == 0))
    thunar_folder_quark = g_quark_from_static_string ("thunar-folder");

  /* check if we already know that folder */
  folder = g_object_get_qdata (G_OBJECT (file), thunar_folder_quark);
  if (G_UNLIKELY (folder != NULL))
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
      exo_gtk_object_ref_sink (GTK_OBJECT (folder));

      /* connect the folder to the file */
      g_object_set_qdata (G_OBJECT (file), thunar_folder_quark, folder);

      /* schedule the loading of the folder */
      thunar_folder_reload (folder);
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
 * the file, you'll have to call g_object_ref() yourself.
 *
 * Return value: the #ThunarFile corresponding to @folder.
 **/
ThunarFile*
thunar_folder_get_corresponding_file (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
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
GList*
thunar_folder_get_files (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return folder->files;
}



/**
 * thunar_folder_get_loading:
 * @folder : a #ThunarFolder instance.
 *
 * Tells whether the contents of the @folder are currently
 * being loaded.
 *
 * Return value: %TRUE if @folder is loading, else %FALSE.
 **/
gboolean
thunar_folder_get_loading (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), FALSE);
  return (folder->job != NULL);
}



/**
 * thunar_folder_reload:
 * @folder : a #ThunarFolder instance.
 *
 * Tells the @folder object to reread the directory
 * contents from the underlying media.
 **/
void
thunar_folder_reload (ThunarFolder *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->job != NULL))
    {
      /* disconnect from the job */
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      thunar_vfs_job_cancel (THUNAR_VFS_JOB (folder->job));
      g_object_unref (G_OBJECT (folder->job));
    }

  /* disconnect from the file alteration monitor */
  if (G_UNLIKELY (folder->handle != NULL))
    {
      thunar_vfs_monitor_remove (folder->monitor, folder->handle);
      folder->handle = NULL;
    }

  /* reset the new_files list */
  thunar_file_list_free (folder->new_files);
  folder->new_files = NULL;

  /* start a new job */
  folder->job = thunar_vfs_listdir (thunar_file_get_path (folder->corresponding_file), NULL);
  g_signal_connect (folder->job, "error", G_CALLBACK (thunar_folder_error), folder);
  g_signal_connect (folder->job, "finished", G_CALLBACK (thunar_folder_finished), folder);
  g_signal_connect (folder->job, "infos-ready", G_CALLBACK (thunar_folder_infos_ready), folder);

  /* tell all consumers that we're loading */
  g_object_notify (G_OBJECT (folder), "loading");
}




