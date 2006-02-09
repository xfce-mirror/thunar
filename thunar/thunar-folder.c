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

#include <thunar/thunar-folder.h>
#include <thunar/thunar-gobject-extensions.h>



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



static void thunar_folder_class_init                  (ThunarFolderClass      *klass);
static void thunar_folder_init                        (ThunarFolder           *folder);
static void thunar_folder_finalize                    (GObject                *object);
static void thunar_folder_get_property                (GObject                *object,
                                                       guint                   prop_id,
                                                       GValue                 *value,
                                                       GParamSpec             *pspec);
static void thunar_folder_error                       (ThunarVfsJob           *job,
                                                       GError                 *error,
                                                       ThunarFolder           *folder);
static void thunar_folder_infos_ready                 (ThunarVfsJob           *job,
                                                       GList                  *infos,
                                                       ThunarFolder           *folder);
static void thunar_folder_finished                    (ThunarVfsJob           *job,
                                                       ThunarFolder           *folder);
static void thunar_folder_corresponding_file_destroy  (ThunarFile             *file,
                                                       ThunarFolder           *folder);
static void thunar_folder_corresponding_file_renamed  (ThunarFile             *file,
                                                       ThunarFolder           *folder);
static void thunar_folder_file_destroy                (ThunarFile             *file,
                                                       ThunarFolder           *folder);
static void thunar_folder_monitor                     (ThunarVfsMonitor       *monitor,
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
  GList                  *previous_files;
  GList                  *files;

  GClosure               *file_destroy_closure;
  gint                    file_destroy_id;

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
                                   g_param_spec_boolean ("loading", "loading", "loading",
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
  /* lookup the id for the "destroy" signal of ThunarFile's */
  folder->file_destroy_id = g_signal_lookup ("destroy", THUNAR_TYPE_FILE);

  /* generate the closure to connect to the "destroy" signal of all files */
  folder->file_destroy_closure = g_cclosure_new (G_CALLBACK (thunar_folder_file_destroy), folder, NULL);
  g_closure_ref (folder->file_destroy_closure);
  g_closure_sink (folder->file_destroy_closure);

  /* connect to the file alteration monitor */
  folder->monitor = thunar_vfs_monitor_get_default ();
}



static void
thunar_folder_finalize (GObject *object)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);
  GList        *lp;

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
      g_signal_handlers_disconnect_matched (G_OBJECT (folder->corresponding_file), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      g_object_set_qdata (G_OBJECT (folder->corresponding_file), thunar_folder_quark, NULL);
      g_object_unref (G_OBJECT (folder->corresponding_file));
    }

  /* drop all previous files (if any) */
  if (G_UNLIKELY (folder->previous_files != NULL))
    {
      /* actually drop the list of previous files */
      g_list_foreach (folder->previous_files, (GFunc) g_object_unref, NULL);
      g_list_free (folder->previous_files);
    }

  /* release references to the current files */
  for (lp = folder->files; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (lp->data), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                            folder->file_destroy_id, 0, folder->file_destroy_closure, NULL, NULL);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (folder->files);

  /* drop the "destroy" closure */
  g_closure_unref (folder->file_destroy_closure);

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
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (folder->job == job);

  /* tell the consumer about the problem */
  g_signal_emit (G_OBJECT (folder), folder_signals[ERROR], 0, error);
}



static void
thunar_folder_infos_ready (ThunarVfsJob *job,
                           GList        *infos,
                           ThunarFolder *folder)
{
  ThunarFile *file;
  GList      *nfiles = NULL;
  GList      *fp;
  GList      *lp;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (folder->handle == NULL);
  g_return_if_fail (folder->job == job);

  /* check if we have any previous files */
  if (G_UNLIKELY (folder->previous_files != NULL))
    {
      /* for every new info, check if we already have it on the previous list */
      for (lp = infos; lp != NULL; lp = lp->next)
        {
          /* get the file corresponding to the info */
          file = thunar_file_get_for_info (lp->data);

          /* check if we have that file on the previous list */
          fp = g_list_find (folder->previous_files, file);
          if (G_LIKELY (fp != NULL))
            {
              /* move it back to the current list */
              folder->previous_files = g_list_delete_link (folder->previous_files, fp);
              folder->files = g_list_prepend (folder->files, file);
              g_object_unref (G_OBJECT (file));
            }
          else
            {
              /* yep, that's a new file then */
              nfiles = g_list_prepend (nfiles, file);
            }

          /* connect the "destroy" signal */
          g_signal_connect_closure_by_id (G_OBJECT (file), folder->file_destroy_id,
                                          0, folder->file_destroy_closure, TRUE);
        }
    }
  else
    {
      /* add the new files to our temporary list of new files */
      for (lp = infos; lp != NULL; lp = lp->next)
        {
          /* get the file corresponding to the info */
          file = thunar_file_get_for_info (lp->data);

          /* add the file to the temporary list of new files */
          nfiles = g_list_prepend (nfiles, file);

          /* connect the "destroy" signal */
          g_signal_connect_closure_by_id (G_OBJECT (file), folder->file_destroy_id,
                                          0, folder->file_destroy_closure, TRUE);
        }
    }

  /* check if we have any new files */
  if (G_LIKELY (nfiles != NULL))
    {
      /* add the new files to our existing list of files */
      folder->files = g_list_concat (folder->files, nfiles);

      /* tell the consumers that we have new files */
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, nfiles);
    }
}



static void
thunar_folder_finished (ThunarVfsJob *job,
                        ThunarFolder *folder)
{
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (THUNAR_IS_FILE (folder->corresponding_file));
  g_return_if_fail (folder->handle == NULL);
  g_return_if_fail (folder->job == job);

  /* check if we have any previous files */
  if (G_UNLIKELY (folder->previous_files != NULL))
    {
      /* emit a "files-removed" signal for the previous files */
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, folder->previous_files);

      /* actually drop the list of previous files */
      g_list_foreach (folder->previous_files, (GFunc) g_object_unref, NULL);
      g_list_free (folder->previous_files);
      folder->previous_files = NULL;
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
thunar_folder_corresponding_file_destroy (ThunarFile   *file,
                                          ThunarFolder *folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (folder->corresponding_file == file);

  /* the folder is useless now */
  gtk_object_destroy (GTK_OBJECT (folder));
}



static void
thunar_folder_corresponding_file_renamed (ThunarFile   *file,
                                          ThunarFolder *folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (folder->corresponding_file == file);

  /* re-register the VFS monitor handle if already connected */
  if (G_LIKELY (folder->handle != NULL))
    {
      thunar_vfs_monitor_remove (folder->monitor, folder->handle);
      folder->handle = thunar_vfs_monitor_add_directory (folder->monitor, thunar_file_get_path (file),
                                                         thunar_folder_monitor, folder);
    }
}



static void
thunar_folder_file_destroy (ThunarFile   *file,
                            ThunarFolder *folder)
{
  GList files;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (g_list_find (folder->files, file) != NULL);

  /* disconnect from the file */
  g_signal_handlers_disconnect_matched (G_OBJECT (file), G_SIGNAL_MATCH_CLOSURE, 0, 0, folder->file_destroy_closure, NULL, NULL);
  folder->files = g_list_remove (folder->files, file);

  /* tell everybody that the file is gone */
  files.data = file; files.next = files.prev = NULL;
  g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, &files);

  /* drop our reference to the file */
  g_object_unref (G_OBJECT (file));
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

  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (THUNAR_IS_FOLDER (folder));
  g_return_if_fail (folder->monitor == monitor);
  g_return_if_fail (folder->handle == handle);
  g_return_if_fail (folder->job == NULL);

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
          g_signal_connect_closure_by_id (G_OBJECT (file), folder->file_destroy_id, 0, folder->file_destroy_closure, TRUE);
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

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (thunar_file_is_directory (file), NULL);

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

      g_signal_connect (G_OBJECT (file), "destroy", G_CALLBACK (thunar_folder_corresponding_file_destroy), folder);
      g_signal_connect (G_OBJECT (file), "renamed", G_CALLBACK (thunar_folder_corresponding_file_renamed), folder);

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
GList*
thunar_folder_get_files (const ThunarFolder *folder)
{
  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
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
  g_return_val_if_fail (THUNAR_IS_FOLDER (folder), FALSE);
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
  GList *lp;

  g_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->job != NULL))
    {
      /* disconnect from the job */
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      g_object_unref (G_OBJECT (folder->job));
    }

  /* disconnect from the file alteration monitor */
  if (G_UNLIKELY (folder->handle != NULL))
    {
      thunar_vfs_monitor_remove (folder->monitor, folder->handle);
      folder->handle = NULL;
    }

  /* drop all previous files (if any) */
  if (G_UNLIKELY (folder->previous_files != NULL))
    {
      /* actually drop the list of previous files */
      g_list_foreach (folder->previous_files, (GFunc) g_object_unref, NULL);
      g_list_free (folder->previous_files);
    }

  /* disconnect signals from the current files */
  for (lp = folder->files; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (lp->data), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                            folder->file_destroy_id, 0, folder->file_destroy_closure, NULL, NULL);
    }

  /* remember the current files as previous files */
  folder->previous_files = folder->files;
  folder->files = NULL;

  /* start a new job */
  folder->job = thunar_vfs_listdir (thunar_file_get_path (folder->corresponding_file), NULL);
  g_signal_connect (folder->job, "error", G_CALLBACK (thunar_folder_error), folder);
  g_signal_connect (folder->job, "finished", G_CALLBACK (thunar_folder_finished), folder);
  g_signal_connect (folder->job, "infos-ready", G_CALLBACK (thunar_folder_infos_ready), folder);

  /* tell all consumers that we're loading */
  g_object_notify (G_OBJECT (folder), "loading");
}




