/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
#include "config.h"
#endif

#include "thunar/thunar-folder.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-io-jobs.h"
#include "thunar/thunar-job.h"
#include "thunar/thunar-private.h"

#define DEBUG_FILE_CHANGES FALSE

/* The maximum throttle interval (in ms) in which files will be added, removed or notified to be changed */
#define THUNAR_FOLDER_UPDATE_TIMEOUT (25)

/* property identifiers */
enum
{
  PROP_0,
  PROP_CORRESPONDING_FILE,
  PROP_LOADING,
};

/* signal identifiers */
enum
{
  DESTROY,
  ERROR,
  FILES_ADDED,
  FILES_REMOVED,
  FILES_CHANGED,
  THUMBNAILS_UPDATED,
  LAST_SIGNAL,
};



static void
thunar_folder_dispose (GObject *object);
static void
thunar_folder_finalize (GObject *object);
static void
thunar_folder_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec);
static void
thunar_folder_set_property (GObject      *object,
                            guint         prop_uid,
                            const GValue *value,
                            GParamSpec   *pspec);
static void
thunar_folder_real_destroy (ThunarFolder *folder);
static void
thunar_folder_error (ExoJob       *job,
                     GError       *error,
                     ThunarFolder *folder);
static gboolean
thunar_folder_files_ready (ThunarJob    *job,
                           GList        *files,
                           ThunarFolder *folder);
static void
thunar_folder_finished (ExoJob       *job,
                        ThunarFolder *folder);
static void
thunar_folder_changed (ThunarFile   *file,
                       ThunarFolder *folder);
static void
thunar_folder_destroyed (ThunarFile   *file,
                         ThunarFolder *folder);
static void
thunar_folder_file_changed (ThunarFolder *folder,
                            ThunarFile   *file);
static void
thunar_folder_file_destroyed (ThunarFolder *folder,
                              ThunarFile   *file);
static void
thunar_folder_monitor (GFileMonitor     *monitor,
                       GFile            *file,
                       GFile            *other_file,
                       GFileMonitorEvent event_type,
                       gpointer          user_data);
static void
thunar_folder_load_content_types (ThunarFolder *folder,
                                  GHashTable   *files);
static void
thunar_folder_add_file (ThunarFolder *folder,
                        ThunarFile   *file);
static void
thunar_folder_remove_file (ThunarFolder *folder,
                           ThunarFile   *file);
static void
thunar_folder_thumbnail_updated (ThunarFolder       *folder,
                                 ThunarThumbnailSize size,
                                 ThunarFile         *file);



struct _ThunarFolderClass
{
  GObjectClass __parent__;

  /* signals */
  void (*destroy) (ThunarFolder *folder);
  void (*error) (ThunarFolder *folder,
                 const GError *error);
  void (*files_added) (ThunarFolder *folder,
                       GHashTable   *files);
  void (*files_removed) (ThunarFolder *folder,
                         GHashTable   *files);
  void (*files_changed) (ThunarFolder *folder,
                         GHashTable   *files);
  void (*thumbnails_updated) (ThunarFolder *folder,
                              GList        *files);
};

struct _ThunarFolder
{
  GObject __parent__;

  ThunarJob *job;
  ThunarJob *content_type_job;

  ThunarFile *corresponding_file;

  /* Files which were loaded a list directory jobs. The key is a ThunarFile; value is NULL (unimportant)*/
  GHashTable *loaded_files_map;

  /* Files which were changed recently. The key is a ThunarFile; value is NULL (unimportant)*/
  GHashTable *changed_files_map;

  /* Files which were changed recently. The key is a ThunarFile; value is NULL (unimportant)*/
  GHashTable *added_files_map;

  /* Files which were changed recently. The key is a ThunarFile; value is NULL (unimportant)*/
  GHashTable *removed_files_map;

  /* Files inside this folder. The key is a ThunarFile; value is NULL (unimportant)*/
  GHashTable *files_map;

  gboolean reload_info;

  guint in_destruction : 1;

  GFileMonitor *monitor;

  /* timeout source ID, used for collecting updates on files before sending the related signal */
  guint files_update_timeout_source_id;

  /* List of ThunarFiles for which the thumbnail got updated recently */
  GList *thumbnail_updated_files;

  /* timeout source ID, used for collecting files for which to update the thumbnail before sending the 'thumbnail-updated' signal */
  guint thumbnail_updated_timeout_source_id;

  /* True if all files of the directory are available as ThunarFiles */
  gboolean loaded;
};



static guint  folder_signals[LAST_SIGNAL];
static GQuark thunar_folder_quark;



G_DEFINE_TYPE (ThunarFolder, thunar_folder, G_TYPE_OBJECT)



static void
thunar_folder_constructed (GObject *object)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);
  GError       *error = NULL;

  folder->monitor = g_file_monitor_directory (thunar_file_get_file (folder->corresponding_file),
                                              G_FILE_MONITOR_WATCH_MOVES, NULL, &error);

  if (G_LIKELY (folder->monitor != NULL))
    g_signal_connect (folder->monitor, "changed", G_CALLBACK (thunar_folder_monitor), folder);
  else
    {
      g_debug ("Could not create folder monitor: %s", error->message);
      g_error_free (error);
    }

  G_OBJECT_CLASS (thunar_folder_parent_class)->constructed (object);
}



static void
thunar_folder_class_init (ThunarFolderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_folder_dispose;
  gobject_class->finalize = thunar_folder_finalize;
  gobject_class->get_property = thunar_folder_get_property;
  gobject_class->set_property = thunar_folder_set_property;
  gobject_class->constructed = thunar_folder_constructed;

  klass->destroy = thunar_folder_real_destroy;

  /**
   * ThunarFolder::corresponding-file:
   *
   * The #ThunarFile referring to the #ThunarFolder.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CORRESPONDING_FILE,
                                   g_param_spec_object ("corresponding-file",
                                                        "corresponding-file",
                                                        "corresponding-file",
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READABLE
                                                        | G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY));

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
   * ThunarFolder::destroy:
   * @folder : a #ThunarFolder.
   *
   * Emitted when the #ThunarFolder is destroyed.
   **/
  folder_signals[DESTROY] =
  g_signal_new (I_ ("destroy"),
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                G_STRUCT_OFFSET (ThunarFolderClass, destroy),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * ThunarFolder::error:
   * @folder : a #ThunarFolder.
   * @error  : the #GError describing the problem.
   *
   * Emitted when the #ThunarFolder fails to completly
   * load the directory content because of an error.
   **/
  folder_signals[ERROR] =
  g_signal_new (I_ ("error"),
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
  g_signal_new (I_ ("files-added"),
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
  g_signal_new (I_ ("files-removed"),
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (ThunarFolderClass, files_removed),
                NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarFolder::file-changed:
   *
   * Emitted by the #ThunarFolder whenever at least one file in the folder has changed
   **/
  folder_signals[FILES_CHANGED] =
  g_signal_new (I_ ("files-changed"),
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (ThunarFolderClass, files_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarFolder::thumbnails-updated:
   *
   * This signal is emitted on #ThunarFolder whenever for the currently
   * existing #ThunarFile thumbnail status changes.
   **/
  folder_signals[THUMBNAILS_UPDATED] =
  g_signal_new (I_ ("thumbnails-updated"),
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (ThunarFolderClass, thumbnails_updated),
                NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
thunar_folder_init (ThunarFolder *folder)
{
  /* If hashtable is initialized without a key_equal_func (we'd use g_direct_equal here);
   * then equality is checked similar to g_direct_equal but without the overhead of a function call */
  folder->files_map = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  folder->loaded_files_map = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  folder->added_files_map = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  folder->removed_files_map = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  folder->changed_files_map = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);

  folder->loaded = FALSE;
  folder->reload_info = FALSE;
  folder->files_update_timeout_source_id = 0;
  folder->thumbnail_updated_files = NULL;
  folder->thumbnail_updated_timeout_source_id = 0;
}



static void
thunar_folder_dispose (GObject *object)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);

  if (!folder->in_destruction)
    {
      folder->in_destruction = TRUE;
      g_signal_emit (G_OBJECT (folder), folder_signals[DESTROY], 0);
      folder->in_destruction = FALSE;
    }

  (*G_OBJECT_CLASS (thunar_folder_parent_class)->dispose) (object);
}



static void
thunar_folder_finalize (GObject *object)
{
  ThunarFolder  *folder = THUNAR_FOLDER (object);
  GHashTableIter iter;
  gpointer       key, file;

  /* stop content type loading */
  if (G_UNLIKELY (folder->content_type_job != NULL))
    {
      exo_job_cancel (EXO_JOB (folder->content_type_job));
      g_object_unref (folder->content_type_job);
      folder->content_type_job = NULL;
    }

  /* stop any running tumbnailing timeout source */
  if (folder->thumbnail_updated_timeout_source_id != 0)
    g_source_remove (folder->thumbnail_updated_timeout_source_id);

  /* stop any running changed_files timeout source */
  if (folder->files_update_timeout_source_id != 0)
    g_source_remove (folder->files_update_timeout_source_id);

  if (folder->monitor != NULL)
    g_signal_handlers_disconnect_by_data (folder->monitor, folder);

  if (folder->corresponding_file)
    {
      thunar_file_unwatch (folder->corresponding_file);

      /* disconnect from the folder */
      g_signal_handlers_disconnect_by_data (folder->corresponding_file, folder);
    }

  /* disconnect ThunarFile signals from files inside the folder */
  g_hash_table_iter_init (&iter, folder->files_map);
  while (g_hash_table_iter_next (&iter, &key, &file))
    g_signal_handlers_disconnect_by_data (file, folder);

  /* release files to thumbnail if any */
  thunar_g_list_free_full (folder->thumbnail_updated_files);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (folder->job != NULL))
    {
      g_signal_handlers_disconnect_by_data (folder->job, folder);
      g_object_unref (folder->job);
      folder->job = NULL;
    }

  /* disconnect from the corresponding file */
  if (G_LIKELY (folder->corresponding_file != NULL))
    {
      /* drop the reference */
      g_object_set_qdata (G_OBJECT (folder->corresponding_file), thunar_folder_quark, NULL);
      g_object_unref (G_OBJECT (folder->corresponding_file));
    }

  /* release references to the current files lists */
  g_hash_table_destroy (folder->files_map);
  g_hash_table_destroy (folder->loaded_files_map);
  g_hash_table_destroy (folder->changed_files_map);
  g_hash_table_destroy (folder->added_files_map);
  g_hash_table_destroy (folder->removed_files_map);

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
    case PROP_CORRESPONDING_FILE:
      g_value_set_object (value, folder->corresponding_file);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, thunar_folder_get_loading (folder));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_folder_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ThunarFolder *folder = THUNAR_FOLDER (object);

  switch (prop_id)
    {
    case PROP_CORRESPONDING_FILE:
      folder->corresponding_file = g_value_dup_object (value);
      if (folder->corresponding_file)
        {
          thunar_file_watch (folder->corresponding_file);
          g_signal_connect (G_OBJECT (folder->corresponding_file), "changed", G_CALLBACK (thunar_folder_changed), folder);
          g_signal_connect (G_OBJECT (folder->corresponding_file), "destroy", G_CALLBACK (thunar_folder_destroyed), folder);
        }

      break;

    case PROP_LOADING:
      _thunar_assert_not_reached ();
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static gboolean
_thunar_folder_remove_file (ThunarFolder *folder,
                            ThunarFile   *file)
{
  /* skip, if we dont have that file */
  if (!g_hash_table_contains (folder->files_map, file))
    return FALSE;

  /* disconnect all signals for the file */
  g_signal_handlers_disconnect_by_data (G_OBJECT (file), folder);

  /* remove the ThunarFile from our map (the destroy method of the hashmap will to the 'g_object_unref')*/
  g_hash_table_remove (folder->files_map, file);

  return TRUE;
}



static gboolean
_thunar_folder_add_file (ThunarFolder *folder,
                         ThunarFile   *file)
{
  /* skip, if we already have that file */
  if (g_hash_table_contains (folder->files_map, file))
    return FALSE;

  /* add the ThunarFile) to the hashmap and keep a reference to it */
  g_hash_table_add (folder->files_map, g_object_ref (file));

  /* connect relevant signals */
  g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_folder_file_changed), folder);
  g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (thunar_folder_file_destroyed), folder);
  g_signal_connect_swapped (G_OBJECT (file), "thumbnail-updated", G_CALLBACK (thunar_folder_thumbnail_updated), folder);

  return TRUE;
}


static gboolean
_thunar_folder_files_update_timeout (gpointer data)
{
  ThunarFolder  *folder = THUNAR_FOLDER (data);
  ThunarFile    *file;
  GHashTable    *files = g_hash_table_new_full (g_direct_hash, NULL, g_object_unref, NULL);
  GHashTableIter iter;
  gpointer       key;

  /* send a 'files-removed' signal for all files which were removed */
  g_hash_table_iter_init (&iter, folder->removed_files_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      /* prevent destruction by adding a temporary reference to the file */
      file = g_object_ref (THUNAR_FILE (key));
      if (_thunar_folder_remove_file (folder, file))
        g_hash_table_add (files, g_object_ref (file));
      g_object_unref (file);
    }

  g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, files);

  g_hash_table_remove_all (files);
  g_hash_table_remove_all (folder->removed_files_map);

  /* send a 'files-added' signal for all files which were added */
  g_hash_table_iter_init (&iter, folder->added_files_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      file = THUNAR_FILE (key);
      if (_thunar_folder_add_file (folder, file))
        g_hash_table_add (files, g_object_ref (file));
    }

  /* start loading the content types of all added files */
  thunar_folder_load_content_types (folder, files);

  g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, files);

  g_hash_table_remove_all (files);
  g_hash_table_remove_all (folder->added_files_map);

  /* send a 'changed' signal for all files which changed */
  g_hash_table_iter_init (&iter, folder->changed_files_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      if (!g_hash_table_contains (folder->files_map, key))
        continue;

      g_hash_table_add (files, g_object_ref (key));
    }

  g_signal_emit (G_OBJECT (folder), folder_signals[FILES_CHANGED], 0, files);
  g_hash_table_destroy (files);
  g_hash_table_remove_all (folder->changed_files_map);

  /* Loading is done for this folder */
  if (folder->loaded == FALSE && folder->job == NULL)
    {
      folder->loaded = TRUE;
      g_object_notify (G_OBJECT (folder), "loading");
    }

  folder->files_update_timeout_source_id = 0;

  return G_SOURCE_REMOVE;
}



static void
thunar_folder_file_changed (ThunarFolder *folder,
                            ThunarFile   *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  g_hash_table_add (folder->changed_files_map, g_object_ref (file));

  if (folder->files_update_timeout_source_id == 0)
    folder->files_update_timeout_source_id = g_timeout_add (THUNAR_FOLDER_UPDATE_TIMEOUT, (GSourceFunc) _thunar_folder_files_update_timeout, folder);
}



static void
thunar_folder_add_file (ThunarFolder *folder,
                        ThunarFile   *file)
{
  /* If it possibly was removed shortly before, just undo the "remove" */
  if (g_hash_table_remove (folder->removed_files_map, file))
    return;

  g_hash_table_add (folder->added_files_map, g_object_ref (file));

  if (folder->files_update_timeout_source_id == 0)
    folder->files_update_timeout_source_id = g_timeout_add (THUNAR_FOLDER_UPDATE_TIMEOUT, (GSourceFunc) _thunar_folder_files_update_timeout, folder);
}



static void
thunar_folder_remove_file (ThunarFolder *folder,
                           ThunarFile   *file)
{
  /* If it possibly was added shortly before, just undo the "add" */
  if (g_hash_table_remove (folder->added_files_map, file))
    return;

  g_hash_table_add (folder->removed_files_map, g_object_ref (file));

  if (folder->files_update_timeout_source_id == 0)
    folder->files_update_timeout_source_id = g_timeout_add (THUNAR_FOLDER_UPDATE_TIMEOUT, (GSourceFunc) _thunar_folder_files_update_timeout, folder);
}



static void
thunar_folder_real_destroy (ThunarFolder *folder)
{
  g_signal_handlers_destroy (G_OBJECT (folder));
}



static void
thunar_folder_error (ExoJob       *job,
                     GError       *error,
                     ThunarFolder *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  /* tell the consumer about the problem */
  g_signal_emit (G_OBJECT (folder), folder_signals[ERROR], 0, error);
}



static gboolean
thunar_folder_files_ready (ThunarJob    *job,
                           GList        *files,
                           ThunarFolder *folder)
{
  GList *lp;

  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);

  /* merge the list with the existing list of new files */
  for (lp = files; lp != NULL; lp = lp->next)
    g_hash_table_add (folder->loaded_files_map, g_object_ref (lp->data));

  thunar_g_list_free_full (files);

  /* indicate that we took over ownership of the file list */
  return TRUE;
}



static void
_thunar_folder_load_content_types_finished (ThunarFolder *folder, ThunarJob *job)
{
  if (folder->content_type_job != NULL)
    {
      g_object_unref (folder->content_type_job);
      folder->content_type_job = NULL;
    }
}



/**
 * thunar_folder_load_content_types:
 * @folder : a #ThunarFolder instance.
 * @files : a #GList of #ThunarFile's for which the content type needs to be loaded.
 *
 * Starts a job to load the content type of all files inside the folder
 **/
void
thunar_folder_load_content_types (ThunarFolder *folder,
                                  GHashTable   *files)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->content_type_job != NULL))
    exo_job_cancel (EXO_JOB (folder->content_type_job));

  /* start a new content_type_job */
  folder->content_type_job = thunar_io_jobs_load_content_types (files);
  g_signal_connect_object (folder->content_type_job, "finished", G_CALLBACK (_thunar_folder_load_content_types_finished), folder, G_CONNECT_SWAPPED);

  exo_job_launch (EXO_JOB (folder->content_type_job));
}



static void
thunar_folder_finished (ExoJob       *job,
                        ThunarFolder *folder)
{
  GHashTableIter iter;
  gpointer       key;
  gboolean       file_list_changed = FALSE;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_FILE (folder->corresponding_file));

  /* determine all added files (files on new_files, but not on files) */
  g_hash_table_iter_init (&iter, folder->loaded_files_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      if (g_hash_table_contains (folder->files_map, key))
        continue;

      thunar_folder_add_file (folder, key);
      file_list_changed = TRUE;
    }

  /* this is to handle removed files after a folder reload */
  /* determine all removed files (files on files, but not on new_files) */
  g_hash_table_iter_init (&iter, folder->files_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      if (key == NULL || g_hash_table_contains (folder->loaded_files_map, key))
        continue;

      /* will mark them to be removed on next timeout */
      thunar_folder_remove_file (folder, THUNAR_FILE (key));
      file_list_changed = TRUE;
    }

  /* drop all mappings for new_files list too */
  g_hash_table_remove_all (folder->loaded_files_map);

  /* schedule a reload of the file information of all files if requested */
  if (folder->reload_info)
    {
      folder->reload_info = FALSE;
      g_hash_table_iter_init (&iter, folder->files_map);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        {
          if (key == NULL)
            continue;
          thunar_file_reload (THUNAR_FILE (key));
        }

      /* block 'file-changed' signals of the folder itself until reload is done, in order to prevent recursion */
      g_signal_handlers_block_by_func (G_OBJECT (folder->corresponding_file), G_CALLBACK (thunar_folder_changed), folder);

      /* reload folder itself */
      thunar_file_reload (folder->corresponding_file);

      /* listen to 'file-changed' signals of the folder itself again when reload is done */
      g_signal_handlers_unblock_by_func (G_OBJECT (folder->corresponding_file), G_CALLBACK (thunar_folder_changed), folder);
    }

  if (G_LIKELY (folder->job != NULL))
    {
      g_signal_handlers_disconnect_by_data (folder->job, folder);
      g_object_unref (folder->job);
      folder->job = NULL;
    }

  /* notify finished loading already here, if the filelist is already correct */
  if (file_list_changed == FALSE)
    {
      folder->loaded = TRUE;
      g_object_notify (G_OBJECT (folder), "loading");
    }
}


/* The file representing the folder has changed */
static void
thunar_folder_changed (ThunarFile   *file,
                       ThunarFolder *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* reload the folder */
  thunar_folder_reload (folder, FALSE);
}


/* The file representing the folder is in destruction */
static void
thunar_folder_destroyed (ThunarFile   *file,
                         ThunarFolder *folder)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* the folder is useless now */
  if (!folder->in_destruction)
    g_object_run_dispose (G_OBJECT (folder));
}



/* One of the files inside the folder is in destruction */
static void
thunar_folder_file_destroyed (ThunarFolder *folder,
                              ThunarFile   *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* check if the corresponding file was destroyed */
  if (G_UNLIKELY (folder->corresponding_file == file))
    {
      /* the folder is useless now */
      if (!folder->in_destruction)
        g_object_run_dispose (G_OBJECT (folder));
    }
  else
    {
      /* remove the file from our list */
      thunar_folder_remove_file (folder, file);
    }
}



#if DEBUG_FILE_CHANGES
static void
thunar_file_infos_equal (ThunarFile *file,
                         GFile      *event_file)
{
  gchar    **attrs;
  GFileInfo *info1 = G_FILE_INFO (file->info);
  GFileInfo *info2;
  guint      i;
  gchar     *attr1, *attr2;
  gboolean   printed = FALSE;
  gchar     *bname;

  attrs = g_file_info_list_attributes (info1, NULL);
  info2 = g_file_query_info (event_file, THUNARX_FILE_INFO_NAMESPACE,
                             G_FILE_QUERY_INFO_NONE, NULL, NULL);

  if (info1 != NULL && info2 != NULL)
    {
      for (i = 0; attrs[i] != NULL; i++)
        {
          if (g_file_info_has_attribute (info2, attrs[i]))
            {
              attr1 = g_file_info_get_attribute_as_string (info1, attrs[i]);
              attr2 = g_file_info_get_attribute_as_string (info2, attrs[i]);

              if (g_strcmp0 (attr1, attr2) != 0)
                {
                  if (!printed)
                    {
                      bname = g_file_get_basename (event_file);
                      g_print ("%s\n", bname);
                      g_free (bname);

                      printed = TRUE;
                    }

                  g_print ("  %s: %s -> %s\n", attrs[i], attr1, attr2);
                }

              g_free (attr1);
              g_free (attr2);
            }
        }

      g_object_unref (info2);
    }

  if (printed)
    g_print ("\n");

  g_free (attrs);
}
#endif



static void
thunar_folder_monitor (GFileMonitor     *monitor,
                       GFile            *event_file,
                       GFile            *other_file,
                       GFileMonitorEvent event_type,
                       gpointer          user_data)
{
  ThunarFolder *folder = THUNAR_FOLDER (user_data);
  ThunarFile   *renamed_file = NULL;
  ThunarFile   *event_file_thunar = NULL;
  ThunarFile   *other_file_thunar = NULL;

  _thunar_return_if_fail (G_IS_FILE_MONITOR (monitor));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (folder->monitor == monitor);
  _thunar_return_if_fail (THUNAR_IS_FILE (folder->corresponding_file));
  _thunar_return_if_fail (G_IS_FILE (event_file));

  if (g_file_equal (event_file, thunar_file_get_file (folder->corresponding_file)))
    {
      /* update/destroy the corresponding file */
      if (event_type == G_FILE_MONITOR_EVENT_DELETED)
        thunar_file_destroy (folder->corresponding_file);
      else
        thunar_file_reload (folder->corresponding_file);
      return;
    }

  /* For rename/delete it is important to only do lookup here, no ThunarFile creation */
  event_file_thunar = thunar_file_cache_lookup (event_file);
  other_file_thunar = (other_file == NULL) ? NULL : thunar_file_cache_lookup (other_file);

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_CREATED:
      if (event_file_thunar == NULL)
        {
          event_file_thunar = thunar_file_get (event_file, NULL);
          if (event_file_thunar == NULL)
            {
              g_info ("Failed to create ThunarFile for gfile");
              break;
            }
        }

      /* Add the file to our map via the timeout source */
      thunar_folder_add_file (folder, event_file_thunar);

      /* if we already ship the ThunarFile, reload it */
      if (g_hash_table_lookup (folder->files_map, event_file_thunar) != NULL)
        thunar_file_reload (event_file_thunar);

      /* notify the thumbnail cache that we can now also move the thumbnail */
      if (event_type == G_FILE_MONITOR_EVENT_MOVED_IN && other_file != NULL)
        thunar_file_move_thumbnail_cache_file (other_file, event_file);
      break;

    case G_FILE_MONITOR_EVENT_MOVED_OUT:
    case G_FILE_MONITOR_EVENT_DELETED:
      if (event_type == G_FILE_MONITOR_EVENT_MOVED_OUT && other_file != NULL)
        thunar_file_move_thumbnail_cache_file (event_file, other_file);

      /* If the ThunarFile is not known to us, than we cannot remove it */
      if (event_file_thunar == NULL)
        break;

      /* Drop it from the map, if we still have it */
      thunar_folder_remove_file (folder, event_file_thunar);

      /* destroy the old file */
      thunar_file_destroy (event_file_thunar);

      break;

    case G_FILE_MONITOR_EVENT_RENAMED:

      /* in case source and dest are already the same ThunarFile, just assume we have no source ThunarFile */
      if (event_file_thunar != NULL && event_file_thunar == other_file_thunar)
        {
          g_object_unref (event_file_thunar);
          event_file_thunar = NULL;
        }

      /* if we dont have any of the two files in the cache yet, try to create the renamed file */
      if (event_file_thunar == NULL && other_file_thunar == NULL)
        {
          other_file_thunar = thunar_file_get (other_file, NULL);
          if (other_file_thunar == NULL)
            {
              g_info ("Failed to create ThunarFile for gfile");
              break;
            }
        }

      /* if we already ship the new file as a ThunarFile, make use of it */
      if (other_file_thunar != NULL)
        renamed_file = other_file_thunar;
      else
        renamed_file = event_file_thunar;

      /* remove both files from the internal maps */
      /* trigger the timeout manually, so that connected models will get updated (required for move+replace use-case) */
      if (event_file_thunar != NULL)
        {
          thunar_folder_remove_file (folder, event_file_thunar);
          _thunar_folder_files_update_timeout (folder);
        }
      if (other_file_thunar != NULL)
        {
          thunar_folder_remove_file (folder, other_file_thunar);
          _thunar_folder_files_update_timeout (folder);
        }

      /* replace the GFile in the ThunarFile for the renamed file with the new gfile */
      thunar_file_replace_file (renamed_file, other_file);

      /* re-add the renamed file into our internal map (this time we can use the timeout source) */
      thunar_folder_add_file (folder, renamed_file);

      /* destroy the old ThunarFile, if we have two of them */
      if (event_file_thunar != NULL && other_file_thunar != NULL)
        thunar_file_destroy (event_file_thunar);

      /* reload the renamed file */
      if (thunar_file_reload (renamed_file))
        {
          /* notify the thumbnail cache that we can now also move the thumbnail */
          thunar_file_move_thumbnail_cache_file (event_file, other_file);
        }

      break;

    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      if (event_file_thunar != NULL)
        {
          /* If we have that file in our internal map, reload it and send a file-changed signal */
          if (g_hash_table_lookup (folder->files_map, event_file_thunar) != NULL)
            {
              thunar_folder_file_changed (folder, event_file_thunar);

#if DEBUG_FILE_CHANGES
              thunar_file_infos_equal (event_file_thunar, event_file);
#endif
              thunar_file_reload (event_file_thunar);
            }
        }

    default:
      /*
        G_FILE_MONITOR_EVENT_PRE_UNMOUNT
        G_FILE_MONITOR_EVENT_UNMOUNTED
        G_FILE_MONITOR_EVENT_MOVED (deprecated/unused)
      */
      break;

    } /* end switch */

  if (event_file_thunar != NULL)
    g_object_unref (event_file_thunar);
  if (other_file_thunar != NULL)
    g_object_unref (other_file_thunar);
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
ThunarFolder *
thunar_folder_get_for_file (ThunarFile *file)
{
  ThunarFolder *folder;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* make sure the file is loaded */
  if (!thunar_file_check_loaded (file))
    return NULL;

  _thunar_return_val_if_fail (thunar_file_is_directory (file), NULL);

  /* load if the file is not a folder */
  if (!thunar_file_is_directory (file))
    return NULL;

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
      folder = g_object_new (THUNAR_TYPE_FOLDER, "corresponding-file", file, NULL);

      /* connect the folder to the file */
      g_object_set_qdata (G_OBJECT (file), thunar_folder_quark, folder);

      /* schedule the loading of the folder */
      thunar_folder_reload (folder, FALSE);
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
ThunarFile *
thunar_folder_get_corresponding_file (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return folder->corresponding_file;
}



/**
 * thunar_folder_get_files:
 * @folder : a #ThunarFolder instance.
 *
 * Returns the hashtable of files currently known for @folder.
 * The data is owned by the #ThunarFile instance and should not be modified or freed.
 *
 * Returns: (transfer none): A GHashTable containing all the #ThunarFiles inside the folder.
 **/
GHashTable *
thunar_folder_get_files (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return folder->files_map;
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
  return (!folder->loaded);
}



/**
 * thunar_folder_has_folder_monitor:
 * @folder : a #ThunarFolder instance.
 *
 * Tells whether the @folder has a folder monitor running
 *
 * Return value: %TRUE if @folder has a folder monitor, else %FALSE.
 **/
gboolean
thunar_folder_has_folder_monitor (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), FALSE);
  return (folder->monitor != NULL);
}



/**
 * thunar_folder_reload:
 * @folder : a #ThunarFolder instance.
 * @reload_info : reload all information for the files too
 *
 * Tells the @folder object to reread the directory
 * contents from the underlying media.
 **/
void
thunar_folder_reload (ThunarFolder *folder,
                      gboolean      reload_info)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* reload file info too? */
  folder->reload_info = reload_info;

  /* stop content type loading */
  if (G_UNLIKELY (folder->content_type_job != NULL))
    exo_job_cancel (EXO_JOB (folder->content_type_job));

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->job != NULL))
    {
      /* disconnect from the job */
      g_signal_handlers_disconnect_by_data (folder->job, folder);
      g_object_unref (folder->job);
      folder->job = NULL;
    }

  /* reset the loaded_files_map hash table */
  g_hash_table_remove_all (folder->loaded_files_map);

  /* start a new job */
  folder->loaded = FALSE;
  g_object_notify (G_OBJECT (folder), "loading");

  folder->job = thunar_io_jobs_list_directory (thunar_file_get_file (folder->corresponding_file));
  g_signal_connect (folder->job, "error", G_CALLBACK (thunar_folder_error), folder);
  g_signal_connect (folder->job, "finished", G_CALLBACK (thunar_folder_finished), folder);
  g_signal_connect (folder->job, "files-ready", G_CALLBACK (thunar_folder_files_ready), folder);
  exo_job_launch (EXO_JOB (folder->job));
}



static gboolean
_thunar_folder_thumbnail_updated_timeout (gpointer data)
{
  ThunarFolder *folder = THUNAR_FOLDER (data);

  g_signal_emit (G_OBJECT (folder), folder_signals[THUMBNAILS_UPDATED], 0, folder->thumbnail_updated_files);

  thunar_g_list_free_full (folder->thumbnail_updated_files);
  folder->thumbnail_updated_files = NULL;
  folder->thumbnail_updated_timeout_source_id = 0;

  return G_SOURCE_REMOVE;
}



static void
thunar_folder_thumbnail_updated (ThunarFolder       *folder,
                                 ThunarThumbnailSize size,
                                 ThunarFile         *file)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  folder->thumbnail_updated_files =
  g_list_prepend (folder->thumbnail_updated_files, g_object_ref (file));

  if (folder->thumbnail_updated_timeout_source_id == 0)
    folder->thumbnail_updated_timeout_source_id = g_timeout_add (THUNAR_FOLDER_UPDATE_TIMEOUT, (GSourceFunc) _thunar_folder_thumbnail_updated_timeout, folder);
}
