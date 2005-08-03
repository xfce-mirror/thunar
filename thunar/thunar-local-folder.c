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

#include <thunar/thunar-local-file.h>
#include <thunar/thunar-local-folder.h>



enum
{
  PROP_0,
  PROP_LOADING,
};



static void        thunar_local_folder_class_init                 (ThunarLocalFolderClass *klass);
static void        thunar_local_folder_folder_init                (ThunarFolderIface      *iface);
static void        thunar_local_folder_init                       (ThunarLocalFolder      *local_folder);
static void        thunar_local_folder_get_property               (GObject                *object,
                                                                   guint                   prop_id,
                                                                   GValue                 *value,
                                                                   GParamSpec             *pspec);
static void        thunar_local_folder_finalize                   (GObject                *object);
static ThunarFile *thunar_local_folder_get_corresponding_file     (ThunarFolder           *folder);
static GSList     *thunar_local_folder_get_files                  (ThunarFolder           *folder);
static gboolean    thunar_local_folder_get_loading                (ThunarFolder           *folder);
static void        thunar_local_folder_error                      (ThunarVfsJob           *job,
                                                                   GError                 *error,
                                                                   ThunarLocalFolder      *local_folder);
static void        thunar_local_folder_infos_ready                (ThunarVfsJob           *job,
                                                                   GSList                 *infos,
                                                                   ThunarLocalFolder      *local_folder);
static void        thunar_local_folder_finished                   (ThunarVfsJob           *job,
                                                                   ThunarLocalFolder      *local_folder);
static void        thunar_local_folder_corresponding_file_destroy (ThunarFile             *file,
                                                                   ThunarLocalFolder      *local_folder);
static void        thunar_local_folder_file_destroy               (ThunarFile             *file,
                                                                   ThunarLocalFolder      *local_folder);



struct _ThunarLocalFolderClass
{
  GtkObjectClass __parent__;
};

struct _ThunarLocalFolder
{
  GtkObject __parent__;

  ThunarVfsJob           *job;

  ThunarFile             *corresponding_file;
  GSList                 *files;

  GClosure               *file_destroy_closure;
  gint                    file_destroy_id;

  ThunarVfsMonitor       *monitor;
  ThunarVfsMonitorHandle *handle;
};



G_DEFINE_TYPE_WITH_CODE (ThunarLocalFolder,
                         thunar_local_folder,
                         GTK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_FOLDER,
                                                thunar_local_folder_folder_init));



static void
thunar_local_folder_class_init (ThunarLocalFolderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_local_folder_finalize;
  gobject_class->get_property = thunar_local_folder_get_property;

  g_object_class_override_property (gobject_class,
                                    PROP_LOADING,
                                    "loading");
}



static void
thunar_local_folder_folder_init (ThunarFolderIface *iface)
{
  iface->get_corresponding_file = thunar_local_folder_get_corresponding_file;
  iface->get_files = thunar_local_folder_get_files;
  iface->get_loading = thunar_local_folder_get_loading;
}



static void
thunar_local_folder_init (ThunarLocalFolder *local_folder)
{
  /* lookup the id for the "destroy" signal of ThunarFile's */
  local_folder->file_destroy_id = g_signal_lookup ("destroy", THUNAR_TYPE_FILE);

  /* generate the closure to connect to the "destroy" signal of all files */
  local_folder->file_destroy_closure = g_cclosure_new (G_CALLBACK (thunar_local_folder_file_destroy), local_folder, NULL);
  g_closure_ref (local_folder->file_destroy_closure);
  g_closure_sink (local_folder->file_destroy_closure);

  /* connect to the file alteration monitor */
  local_folder->monitor = thunar_vfs_monitor_get ();
}



static void
thunar_local_folder_finalize (GObject *object)
{
  ThunarLocalFolder *local_folder = THUNAR_LOCAL_FOLDER (object);
  GSList            *lp;

  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));

  /* disconnect from the file alteration monitor */
  if (G_LIKELY (local_folder->handle != NULL))
    thunar_vfs_monitor_remove (local_folder->monitor, local_folder->handle);
  g_object_unref (G_OBJECT (local_folder->monitor));

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (local_folder->job != NULL))
    {
      g_signal_handlers_disconnect_matched (local_folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, local_folder);
      thunar_vfs_job_cancel (local_folder->job);
      thunar_vfs_job_unref (local_folder->job);
    }

  /* disconnect from the corresponding file */
  if (G_LIKELY (local_folder->corresponding_file != NULL))
    {
      /* drop the reference */
      g_signal_handlers_disconnect_matched (G_OBJECT (local_folder->corresponding_file),
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, local_folder);
      g_object_set_data (G_OBJECT (local_folder->corresponding_file), "thunar-local-folder", NULL);
      g_object_unref (G_OBJECT (local_folder->corresponding_file));
    }

  /* release references to the files */
  for (lp = local_folder->files; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (lp->data), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                            local_folder->file_destroy_id, 0, local_folder->file_destroy_closure,
                                            NULL, NULL);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_slist_free (local_folder->files);

  /* drop the "destroy" closure */
  g_closure_unref (local_folder->file_destroy_closure);

  G_OBJECT_CLASS (thunar_local_folder_parent_class)->finalize (object);
}



static void
thunar_local_folder_get_property (GObject    *object,
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
thunar_local_folder_get_corresponding_file (ThunarFolder *folder)
{
  return THUNAR_LOCAL_FOLDER (folder)->corresponding_file;
}



static GSList*
thunar_local_folder_get_files (ThunarFolder *folder)
{
  return THUNAR_LOCAL_FOLDER (folder)->files;
}



static gboolean
thunar_local_folder_get_loading (ThunarFolder *folder)
{
  return (THUNAR_LOCAL_FOLDER (folder)->job != NULL);
}



static void
thunar_local_folder_error (ThunarVfsJob      *job,
                           GError            *error,
                           ThunarLocalFolder *local_folder)
{
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->job == job);
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  // FIXME:
  g_warning ("ThunarLocalFolder error: %s", error->message);
}



static void
thunar_local_folder_infos_ready (ThunarVfsJob      *job,
                                 GSList            *infos,
                                 ThunarLocalFolder *local_folder)
{
  ThunarFile *file;
  GSList     *nfiles = NULL;
  GSList     *lp;
  GSList     *p;

  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->job == job);
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* add the new files */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      /* get the file corresponding to the info */
      file = thunar_local_file_get_for_info (lp->data);

      /* verify that we don't already have that file */
      for (p = local_folder->files; p != NULL; p = p->next)
        if (G_UNLIKELY (p->data == file))
          break;
      if (G_UNLIKELY (p != NULL))
        {
          g_object_unref (G_OBJECT (file));
          continue;
        }

      /* add the file */
      nfiles = g_slist_prepend (nfiles, file);
      local_folder->files = g_slist_prepend (local_folder->files, file);

      g_signal_connect_closure_by_id (G_OBJECT (file), local_folder->file_destroy_id,
                                      0, local_folder->file_destroy_closure, TRUE);
    }

  /* tell the consumers that we have new files */
  if (G_LIKELY (nfiles != NULL))
    {
      thunar_folder_files_added (THUNAR_FOLDER (local_folder), nfiles);
      g_slist_free (nfiles);
    }
}



static void
thunar_local_folder_finished (ThunarVfsJob      *job,
                              ThunarLocalFolder *local_folder)
{
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->job == job);
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* we did it, the folder is loaded */
  g_signal_handlers_disconnect_matched (local_folder->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, local_folder);
  thunar_vfs_job_unref (local_folder->job);
  local_folder->job = NULL;

  g_object_notify (G_OBJECT (local_folder), "loading");
}



static void
thunar_local_folder_corresponding_file_destroy (ThunarFile        *file,
                                                ThunarLocalFolder *local_folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->corresponding_file == file);

  /* the folder is useless now */
  gtk_object_destroy (GTK_OBJECT (local_folder));
}



static void
thunar_local_folder_file_destroy (ThunarFile        *file,
                                  ThunarLocalFolder *local_folder)
{
  GSList *files;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (g_slist_find (local_folder->files, file) != NULL);

  /* disconnect from the file */
  g_signal_handlers_disconnect_matched (G_OBJECT (file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                        local_folder->file_destroy_id, 0, local_folder->file_destroy_closure,
                                        NULL, NULL);
  local_folder->files = g_slist_remove (local_folder->files, file);

  /* tell everybody that the file is gone */
  files = g_slist_prepend (NULL, file);
  thunar_folder_files_removed (THUNAR_FOLDER (local_folder), files);
  g_slist_free_1 (files);

  /* drop our reference to the file */
  g_object_unref (G_OBJECT (file));
}



static void
thunar_local_folder_monitor (ThunarVfsMonitor       *monitor,
                             ThunarVfsMonitorHandle *handle,
                             ThunarVfsMonitorEvent   event,
                             ThunarVfsURI           *handle_uri,
                             ThunarVfsURI           *event_uri,
                             gpointer                user_data)
{
  ThunarLocalFolder *local_folder = THUNAR_LOCAL_FOLDER (user_data);
  ThunarFile        *file;
  GSList            *lp;

  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (THUNAR_VFS_IS_URI (handle_uri));
  g_return_if_fail (THUNAR_VFS_IS_URI (event_uri));
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->monitor == monitor);
  g_return_if_fail (local_folder->handle == handle);

  /* determine the file for the uri */
  file = thunar_file_get_for_uri (event_uri, NULL);
  if (G_UNLIKELY (file == NULL))
    return;

  /* check the event */
  switch (event)
    {
    case THUNAR_VFS_MONITOR_EVENT_CHANGED:
    case THUNAR_VFS_MONITOR_EVENT_CREATED:
      if (G_LIKELY (file != local_folder->corresponding_file))
        {
          /* check if we already ship the file */
          for (lp = local_folder->files; lp != NULL; lp = lp->next)
            if (G_UNLIKELY (lp->data == file))
              break;

          /* if we don't have it, add it */
          if (G_UNLIKELY (lp == NULL))
            {
              /* prepend it to our internal list */
              g_signal_connect_closure_by_id (G_OBJECT (file), local_folder->file_destroy_id,
                                              0, local_folder->file_destroy_closure, TRUE);
              local_folder->files = g_slist_prepend (local_folder->files, file);
              g_object_ref (G_OBJECT (file));

              /* tell others about the new file */
              lp = g_slist_prepend (NULL, file);
              thunar_folder_files_added (THUNAR_FOLDER (local_folder), lp);
              g_slist_free (lp);
            }
          else
            {
              /* force an update on the file */
              thunar_file_reload (file);
            }
        }
      else
        {
          /* force an update on the corresponding file */
          thunar_file_reload (file);
        }
      break;

    case THUNAR_VFS_MONITOR_EVENT_DELETED:
      /* drop the file */
      gtk_object_destroy (GTK_OBJECT (file));
      break;
    }

  /* cleanup */
  g_object_unref (G_OBJECT (file));
}



/**
 * thunar_local_folder_get_for_file:
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
thunar_local_folder_get_for_file (ThunarLocalFile *local_file,
                                  GError         **error)
{
  ThunarLocalFolder *local_folder;

  g_return_val_if_fail (THUNAR_IS_LOCAL_FILE (local_file), NULL);

  local_folder = g_object_get_data (G_OBJECT (local_file), "thunar-local-folder");
  if (local_folder != NULL)
    {
      g_object_ref (G_OBJECT (local_folder));
    }
  else
    {
      /* allocate the new instance */
      local_folder = g_object_new (THUNAR_TYPE_LOCAL_FOLDER, NULL);
      local_folder->corresponding_file = THUNAR_FILE (local_file);
      g_object_ref (G_OBJECT (local_file));

      /* drop the floating reference */
      g_assert (GTK_OBJECT_FLOATING (local_folder));
      g_object_ref (G_OBJECT (local_folder));
      gtk_object_sink (GTK_OBJECT (local_folder));

      g_signal_connect (G_OBJECT (local_file), "destroy", G_CALLBACK (thunar_local_folder_corresponding_file_destroy), local_folder);

      g_object_set_data (G_OBJECT (local_file), "thunar-local-folder", local_folder);

      /* schedule the loading of the folder */
      local_folder->job = thunar_vfs_listdir (thunar_file_get_uri (THUNAR_FILE (local_file)), NULL);
      g_signal_connect (local_folder->job, "error", G_CALLBACK (thunar_local_folder_error), local_folder);
      g_signal_connect (local_folder->job, "finished", G_CALLBACK (thunar_local_folder_finished), local_folder);
      g_signal_connect (local_folder->job, "infos-ready", G_CALLBACK (thunar_local_folder_infos_ready), local_folder);

      /* add us to the file alteration monitor */
      local_folder->handle = thunar_vfs_monitor_add_directory (local_folder->monitor, thunar_file_get_uri (THUNAR_FILE (local_file)),
                                                               thunar_local_folder_monitor, local_folder);
    }

  return THUNAR_FOLDER (local_folder);
}


