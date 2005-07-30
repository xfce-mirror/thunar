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
static void        thunar_local_folder_corresponding_file_changed (ThunarFile             *file,
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

  ThunarVfsJob *job;

  ThunarFile *corresponding_file;
  GSList     *files;

  GClosure   *file_destroy_closure;
  gint        file_destroy_id;
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
thunar_local_folder_init (ThunarLocalFolder *folder)
{
  /* lookup the id for the "destroy" signal of ThunarFile's */
  folder->file_destroy_id = g_signal_lookup ("destroy", THUNAR_TYPE_FILE);

  /* generate the closure to connect to the "destroy" signal of all files */
  folder->file_destroy_closure = g_cclosure_new (G_CALLBACK (thunar_local_folder_file_destroy), folder, NULL);
  g_closure_ref (folder->file_destroy_closure);
  g_closure_sink (folder->file_destroy_closure);
}



static void
thunar_local_folder_finalize (GObject *object)
{
  ThunarLocalFolder *local_folder = THUNAR_LOCAL_FOLDER (object);
  GSList            *lp;

  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (local_folder->job != NULL))
    {
      thunar_vfs_job_cancel (local_folder->job);
      thunar_vfs_job_unref (local_folder->job);
    }

  /* disconnect from the corresponding file */
  if (G_LIKELY (local_folder->corresponding_file != NULL))
    {
      /* unwatch the corresponding file */
      thunar_file_unwatch (local_folder->corresponding_file);

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

  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->job == job);
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));

  /* add the new files */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      file = thunar_local_file_get_for_info (lp->data);
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
  thunar_vfs_job_unref (local_folder->job);
  local_folder->job = NULL;

  g_object_notify (G_OBJECT (local_folder), "loading");
}



static void
thunar_local_folder_corresponding_file_changed (ThunarFile        *file,
                                                ThunarLocalFolder *local_folder)
{
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (local_folder->corresponding_file == file);

  /* rescan the directory (FIXME) */
#if 0
  if (!thunar_local_folder_rescan (local_folder, NULL))
    gtk_object_destroy (GTK_OBJECT (local_folder));
#endif
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
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_LOCAL_FOLDER (local_folder));
  g_return_if_fail (g_slist_find (local_folder->files, file) != NULL);

  g_signal_handlers_disconnect_matched (G_OBJECT (file), G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_CLOSURE,
                                        local_folder->file_destroy_id, 0, local_folder->file_destroy_closure,
                                        NULL, NULL);
  local_folder->files = g_slist_remove (local_folder->files, file);
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

      /* watch the corresponding file for changes */
      thunar_file_watch (local_folder->corresponding_file);

      /* drop the floating reference */
      g_assert (GTK_OBJECT_FLOATING (local_folder));
      g_object_ref (G_OBJECT (local_folder));
      gtk_object_sink (GTK_OBJECT (local_folder));

      g_signal_connect (G_OBJECT (local_file), "changed", G_CALLBACK (thunar_local_folder_corresponding_file_changed), local_folder);
      g_signal_connect (G_OBJECT (local_file), "destroy", G_CALLBACK (thunar_local_folder_corresponding_file_destroy), local_folder);

      g_object_set_data (G_OBJECT (local_file), "thunar-local-folder", local_folder);

      /* schedule the loading of the folder */
      local_folder->job = thunar_vfs_listdir (thunar_file_get_uri (THUNAR_FILE (local_file)), NULL);
      g_signal_connect (local_folder->job, "error", G_CALLBACK (thunar_local_folder_error), local_folder);
      g_signal_connect (local_folder->job, "finished", G_CALLBACK (thunar_local_folder_finished), local_folder);
      g_signal_connect (local_folder->job, "infos-ready", G_CALLBACK (thunar_local_folder_infos_ready), local_folder);
    }

  return THUNAR_FOLDER (local_folder);
}


