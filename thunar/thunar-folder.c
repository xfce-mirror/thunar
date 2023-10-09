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

#include "thunar/thunar-file-monitor.h"
#include "thunar/thunar-folder.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-io-jobs.h"
#include "thunar/thunar-job.h"
#include "thunar/thunar-private.h"

#define DEBUG_FILE_CHANGES FALSE



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
  LAST_SIGNAL,
};



static void           thunar_folder_dispose               (GObject               *object);
static void           thunar_folder_finalize              (GObject               *object);
static void           thunar_folder_get_property          (GObject               *object,
                                                           guint                  prop_id,
                                                           GValue                *value,
                                                           GParamSpec            *pspec);
static void           thunar_folder_set_property          (GObject               *object,
                                                           guint                  prop_uid,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void           thunar_folder_real_destroy          (ThunarFolder          *folder);
static void           thunar_folder_error                 (ExoJob                *job,
                                                           GError                *error,
                                                           ThunarFolder          *folder);
static gboolean       thunar_folder_files_ready           (ThunarJob             *job,
                                                           GList                 *files,
                                                           ThunarFolder          *folder);
static void           thunar_folder_finished              (ExoJob                *job,
                                                           ThunarFolder          *folder);
static void           thunar_folder_file_changed          (ThunarFileMonitor     *file_monitor,
                                                           ThunarFile            *file,
                                                           ThunarFolder          *folder);
static void           thunar_folder_file_destroyed        (ThunarFileMonitor     *file_monitor,
                                                           ThunarFile            *file,
                                                           ThunarFolder          *folder);
static void           thunar_folder_monitor               (GFileMonitor          *monitor,
                                                           GFile                 *file,
                                                           GFile                 *other_file,
                                                           GFileMonitorEvent      event_type,
                                                           gpointer               user_data);
static void           thunar_folder_load_content_types    (ThunarFolder          *folder,
                                                           GList                 *files);



struct _ThunarFolderClass
{
  GObjectClass __parent__;

  /* signals */
  void (*destroy)       (ThunarFolder *folder);
  void (*error)         (ThunarFolder *folder,
                         const GError *error);
  void (*files_added)   (ThunarFolder *folder,
                         GList        *files);
  void (*files_removed) (ThunarFolder *folder,
                         GList        *files);
};

struct _ThunarFolder
{
  GObject            __parent__;

  ThunarJob         *job;
  ThunarJob         *content_type_job;

  ThunarFile        *corresponding_file;

  /* map of GFile --> ThunarFile */
  GHashTable        *new_files_map;

  /* map of GFile --> ThunarFile */
  GHashTable        *files_map;

  gboolean           reload_info;

  guint              in_destruction : 1;

  ThunarFileMonitor *file_monitor;

  GFileMonitor      *monitor;
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
}



static void
thunar_folder_init (ThunarFolder *folder)
{
  folder->files_map = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, NULL, g_object_unref);
  folder->new_files_map = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, NULL, g_object_unref);

  /* connect to the ThunarFileMonitor instance */
  folder->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (folder->file_monitor), "file-changed", G_CALLBACK (thunar_folder_file_changed), folder);
  g_signal_connect (G_OBJECT (folder->file_monitor), "file-destroyed", G_CALLBACK (thunar_folder_file_destroyed), folder);

  folder->monitor = NULL;
  folder->reload_info = FALSE;
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
  ThunarFolder *folder = THUNAR_FOLDER (object);

  if (folder->corresponding_file)
    thunar_file_unwatch (folder->corresponding_file);

  /* disconnect from the ThunarFileMonitor instance */
  g_signal_handlers_disconnect_matched (folder->file_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
  g_object_unref (folder->file_monitor);

  /* disconnect from the file alteration monitor */
  if (G_LIKELY (folder->monitor != NULL))
    {
      g_signal_handlers_disconnect_matched (folder->monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      g_file_monitor_cancel (folder->monitor);
      g_object_unref (folder->monitor);
    }

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (folder->job != NULL))
    {
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
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
  g_hash_table_destroy (folder->new_files_map);

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
          thunar_file_watch (folder->corresponding_file);
        break;

      case PROP_LOADING:
        _thunar_assert_not_reached ();
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
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
    g_hash_table_insert (folder->new_files_map, thunar_file_get_file (lp->data), g_object_ref (lp->data));

  thunar_g_list_free_full (files);

  /* indicate that we took over ownership of the file list */
  return TRUE;
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
                                  GList        *files)
{
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->content_type_job != NULL))
    {
      exo_job_cancel (EXO_JOB (folder->content_type_job));
      g_object_unref (folder->content_type_job);
      folder->content_type_job = NULL;
    }

  /* start a new content_type_job */
  folder->content_type_job = thunar_io_jobs_load_content_types (files);
  exo_job_launch (EXO_JOB (folder->content_type_job));
}



static void
thunar_folder_finished (ExoJob       *job,
                        ThunarFolder *folder)
{
  ThunarFile     *file;
  GHashTableIter iter;
  gpointer       key, value;
  GList          *files = NULL;
  GList          *lp;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_FILE (folder->corresponding_file));

  /* determine all added files (files on new_files, but not on files) */
  g_hash_table_iter_init (&iter, folder->new_files_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value != NULL && !g_hash_table_contains (folder->files_map, thunar_file_get_file (THUNAR_FILE (value))))
        {
          file = THUNAR_FILE (value);

          /* put the file on the added list */
          files = g_list_prepend (files, file);

          g_hash_table_insert (folder->files_map, thunar_file_get_file (file), g_object_ref (file));
        }
    }

  /* check if any files were added */
  if (G_UNLIKELY (files != NULL))
    {
      /* emit a "files-added" signal for the added files */
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, files);

      /* start loading the content types of all files in the folder */
      thunar_folder_load_content_types (folder, files);

      /* release the added files list */
      g_list_free (files);
      files = NULL;
    }

  /* this is to handle removed files after a folder reload */
  /* determine all removed files (files on files, but not on new_files) */
  g_hash_table_iter_init (&iter, folder->files_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value != NULL)
        {
          /* determine the file */
          file = THUNAR_FILE (value);

          /* check if the file is not on new_files */
          if (g_hash_table_contains (folder->new_files_map, thunar_file_get_file (file)))
            continue;

          /* put the file on the removed list (owns the reference now) */
          files = g_list_prepend (files, g_object_ref (file));
        }
    }

  for (lp = files; lp != NULL; lp = lp->next)
    {
      file = THUNAR_FILE (lp->data);

      /* remove from the hashmap too */
      g_hash_table_remove (folder->files_map, thunar_file_get_file (file));
    }

  /* check if any files were removed */
  if (G_UNLIKELY (files != NULL))
    {
      /* emit a "files-removed" signal for the removed files */
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, files);

      /* release the removed files list */
      thunar_g_list_free_full (files);
    }

  /* drop all mappings for new_files list too */
  g_hash_table_remove_all (folder->new_files_map);

  /* schedule a reload of the file information of all files if requested */
  if (folder->reload_info)
    {
      folder->reload_info = FALSE;
      g_hash_table_iter_init (&iter, folder->files_map);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (value != NULL)
            thunar_file_reload (THUNAR_FILE (value));
        }

      /* block 'file-changed' signals of the folder itself until reload is done, in order to prevent recursion */
      g_signal_handlers_block_by_func (G_OBJECT (folder->file_monitor), G_CALLBACK (thunar_folder_file_changed), folder);

      /* reload folder itself */
      thunar_file_reload (folder->corresponding_file);

      /* listen to 'file-changed' signals of the folder itself again when reload is done */
      g_signal_handlers_unblock_by_func (G_OBJECT (folder->file_monitor), G_CALLBACK (thunar_folder_file_changed), folder);
    }

  /* we did it, the folder is loaded */
  if (G_LIKELY (folder->job != NULL))
    {
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      g_object_unref (folder->job);
      folder->job = NULL;
    }

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
      thunar_folder_reload (folder, FALSE);
    }
}



static void
thunar_folder_file_destroyed (ThunarFileMonitor *file_monitor,
                              ThunarFile        *file,
                              ThunarFolder      *folder)
{
  GList    files;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));

  /* check if the corresponding file was destroyed */
  if (G_UNLIKELY (folder->corresponding_file == file))
    {
      /* the folder is useless now */
      if (!folder->in_destruction)
        g_object_run_dispose (G_OBJECT (folder));
    }
  else if (g_hash_table_contains (folder->files_map, thunar_file_get_file (file)))
    {
      /* tell everybody that the file is gone */
      files.data = file;
      files.next = files.prev = NULL;
      g_signal_emit (G_OBJECT (folder), folder_signals[FILES_REMOVED], 0, &files);

      /* remove the file from our list */
      g_hash_table_remove (folder->files_map, thunar_file_get_file (file));
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
  ThunarFile   *file = NULL;
  ThunarFile   *file_in_map;
  GList         list;

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

  file_in_map = g_hash_table_lookup (folder->files_map, event_file);

  /* if we don't have it, add it if the event does not "delete" the "event_file" */
  if (file_in_map == NULL && event_type != G_FILE_MONITOR_EVENT_DELETED && event_type != G_FILE_MONITOR_EVENT_MOVED_OUT)
    {
      if (event_type == G_FILE_MONITOR_EVENT_RENAMED)
        {
          if (G_LIKELY (other_file != NULL))
            {
              ThunarFile *other_file_in_map;
              other_file_in_map = g_hash_table_lookup (folder->files_map, other_file);

              /* create a renamed file only if it doesn't exist */
              if (!other_file_in_map)
                file = thunar_file_get (other_file, NULL);
            }
        }
      else
        {
          file = thunar_file_get (event_file, NULL);
        }

      /* the file should not exist in file cache, so it's (re)loaded now */
      if (file != NULL)
        {
          /* add a mapping of (gfile -> ThunarFile) to the hashmap */
          g_hash_table_insert (folder->files_map, thunar_file_get_file (file), g_object_ref (file));

          /* tell others about the new file */
          list.data = file;
          list.next = list.prev = NULL;
          g_signal_emit (G_OBJECT (folder), folder_signals[FILES_ADDED], 0, &list);

          if (other_file != NULL)
            {
              /* notify the thumbnail cache that we can now also move the thumbnail */
              if (event_type == G_FILE_MONITOR_EVENT_MOVED_IN)
                thunar_file_move_thumbnail_cache_file (other_file, event_file);
              else
                thunar_file_move_thumbnail_cache_file (event_file, other_file);
            }
        }
    }
  else if (file_in_map != NULL)
    {
      if (event_type == G_FILE_MONITOR_EVENT_DELETED)
        {
          /* destroy the file */
          thunar_file_destroy (file_in_map);
        }
      else if (event_type == G_FILE_MONITOR_EVENT_RENAMED || event_type == G_FILE_MONITOR_EVENT_MOVED_IN || event_type == G_FILE_MONITOR_EVENT_MOVED_OUT)
        {
          if (event_type == G_FILE_MONITOR_EVENT_MOVED_IN)
            {
              /* reload existing file, the case when file doesn't exist is handled above */
              thunar_file_reload (file_in_map);
            }
          else if (event_type == G_FILE_MONITOR_EVENT_MOVED_OUT)
            {
              /* destroy the old file */
              thunar_file_destroy (file_in_map);
            }
          else if (event_type == G_FILE_MONITOR_EVENT_RENAMED && G_LIKELY (other_file != NULL))
            {
              ThunarFile *dest_file_in_map;

              /* check if we already ship the destination file */
              dest_file_in_map = g_hash_table_lookup (folder->files_map, other_file);

              if (dest_file_in_map)
                {
                  /* destroy source file if the destination file already exists
                      to prevent duplicated file */
                  if (file_in_map == dest_file_in_map)
                    {
                      g_warning ("Same g_file for source and destination file during rename");
                    }
                  else
                    {
                      thunar_file_destroy (file_in_map);
                      file = dest_file_in_map;
                    }
                }
              else
                {
                  file = file_in_map;

                  /* remove the old reference from the hash table before it becomes invalid;
                   * during thunar_file_replace_file call */
                  g_hash_table_remove (folder->files_map, event_file);

                  /* replace GFile in ThunarFile for the renamed file */
                  thunar_file_replace_file (file, other_file);

                  /* insert new mapping of (gfile, ThunarFile) for the newly renamed file */
                  g_hash_table_insert (folder->files_map, other_file, g_object_ref (file));


                }

              /* reload the renamed file */
              if (thunar_file_reload (file))
                {
                  /* notify the thumbnail cache that we can now also move the thumbnail */
                  thunar_file_move_thumbnail_cache_file (event_file, other_file);
                }
            }
        }
      /* This should handle G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &
       * G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED */
      else
        {
#if DEBUG_FILE_CHANGES
          thunar_file_infos_equal (file_in_map, event_file);
#endif
          thunar_file_reload (file_in_map);
        }
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
 * Returns the list of files currently known for @folder.
 * The content of the list is owned by the hash table and should not be modified or freed. Use g_list_free() when done using the list.
 * The caller of the function takes ownership of the data container, but not the data inside it.
 *
 * Returns: (transfer container): A GList containing all the #ThunarFiles inside the hash table. Use g_list_free() when done using the list.
 **/
GList *
thunar_folder_get_files (const ThunarFolder *folder)
{
  _thunar_return_val_if_fail (THUNAR_IS_FOLDER (folder), NULL);
  return g_hash_table_get_values (folder->files_map);
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
    {
      exo_job_cancel (EXO_JOB (folder->content_type_job));
      g_object_unref (folder->content_type_job);
      folder->content_type_job = NULL;
    }

  /* check if we are currently connect to a job */
  if (G_UNLIKELY (folder->job != NULL))
    {
      /* disconnect from the job */
      g_signal_handlers_disconnect_matched (folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, folder);
      g_object_unref (folder->job);
      folder->job = NULL;
    }

  /* reset the new_files list & new_files_map hash table */
  g_hash_table_remove_all (folder->new_files_map);

  /* start a new job */
  folder->job = thunar_io_jobs_list_directory (thunar_file_get_file (folder->corresponding_file));
  exo_job_launch (EXO_JOB (folder->job));
  g_signal_connect (folder->job, "error", G_CALLBACK (thunar_folder_error), folder);
  g_signal_connect (folder->job, "finished", G_CALLBACK (thunar_folder_finished), folder);
  g_signal_connect (folder->job, "files-ready", G_CALLBACK (thunar_folder_files_ready), folder);

  /* tell all consumers that we're loading */
  g_object_notify (G_OBJECT (folder), "loading");
}
