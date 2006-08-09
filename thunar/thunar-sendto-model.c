/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-private.h>
#include <thunar/thunar-sendto-model.h>



static void thunar_sendto_model_class_init  (ThunarSendtoModelClass *klass);
static void thunar_sendto_model_finalize    (GObject                *object);
static void thunar_sendto_model_load        (ThunarSendtoModel      *sendto_model);
static void thunar_sendto_model_event       (ThunarVfsMonitor       *monitor,
                                             ThunarVfsMonitorHandle *handle,
                                             ThunarVfsMonitorEvent   event,
                                             ThunarVfsPath          *handle_path,
                                             ThunarVfsPath          *event_path,
                                             gpointer                user_data);



struct _ThunarSendtoModelClass
{
  GObjectClass __parent__;
};

struct _ThunarSendtoModel
{
  GObject           __parent__;
  ThunarVfsMonitor *monitor;
  GList            *handles;
  GList            *handlers;
  guint             loaded : 1;
};



static GObjectClass *thunar_sendto_model_parent_class;



GType
thunar_sendto_model_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarSendtoModelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_sendto_model_class_init,
        NULL,
        NULL,
        sizeof (ThunarSendtoModel),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarSendtoModel"), &info, 0);
    }

  return type;
}



static void
thunar_sendto_model_class_init (ThunarSendtoModelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_sendto_model_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_sendto_model_finalize;
}



static void
thunar_sendto_model_finalize (GObject *object)
{
  ThunarSendtoModel *sendto_model = THUNAR_SENDTO_MODEL (object);
  GList             *lp;

  /* release the handlers */
  g_list_foreach (sendto_model->handlers, (GFunc) g_object_unref, NULL);
  g_list_free (sendto_model->handlers);

  /* disconnect from the monitor (if connected) */
  if (G_LIKELY (sendto_model->monitor != NULL))
    {
      /* disconnect all handles and release monitor reference */
      for (lp = sendto_model->handles; lp != NULL; lp = lp->next)
        thunar_vfs_monitor_remove (sendto_model->monitor, lp->data);
      g_object_unref (G_OBJECT (sendto_model->monitor));
      g_list_free (sendto_model->handles);
    }

  (*G_OBJECT_CLASS (thunar_sendto_model_parent_class)->finalize) (object);
}



static gint
tvma_compare (gconstpointer a,
              gconstpointer b)
{
  return strcmp (thunar_vfs_mime_application_get_desktop_id (b),
                 thunar_vfs_mime_application_get_desktop_id (a));
}



static void
thunar_sendto_model_load (ThunarSendtoModel *sendto_model)
{
  ThunarVfsMimeApplication *handler;
  const gchar              *id;
  gchar                   **specs;
  gchar                    *path;
  guint                     n;

  /* lookup all sendto .desktop files */
  specs = xfce_resource_match (XFCE_RESOURCE_DATA, "Thunar/sendto/*.desktop", TRUE);
  for (n = 0; specs[n] != NULL; ++n)
    {
      /* lookup the absolute path to the .desktop file */
      path = xfce_resource_lookup (XFCE_RESOURCE_DATA, specs[n]);
      if (G_LIKELY (path != NULL))
        {
          /* we use the filename as desktop-id */
          id = specs[n] + (sizeof ("Thunar/sendto/") - 1);

          /* try to load the .desktop file */
          handler = thunar_vfs_mime_application_new_from_file (path, id);
          if (G_LIKELY (handler != NULL))
            {
              /* add to our handler list, sorted by their desktop-ids (reverse order) */
              sendto_model->handlers = g_list_insert_sorted (sendto_model->handlers, handler, tvma_compare);
            }
        }

      /* cleanup */
      g_free (specs[n]);
      g_free (path);
    }
  g_free (specs);
}



static void
thunar_sendto_model_event (ThunarVfsMonitor       *monitor,
                           ThunarVfsMonitorHandle *handle,
                           ThunarVfsMonitorEvent   event,
                           ThunarVfsPath          *handle_path,
                           ThunarVfsPath          *event_path,
                           gpointer                user_data)
{
  ThunarSendtoModel *sendto_model = THUNAR_SENDTO_MODEL (user_data);

  /* release the previously loaded handlers */
  if (G_LIKELY (sendto_model->handlers != NULL))
    {
      g_list_foreach (sendto_model->handlers, (GFunc) g_object_unref, NULL);
      g_list_free (sendto_model->handlers);
      sendto_model->handlers = NULL;
    }

  /* reload the handlers for the model */
  thunar_sendto_model_load (sendto_model);
}



/**
 * thunar_sendto_model_get_default:
 *
 * Returns a reference to the default #ThunarSendtoModel
 * instance. The caller is responsible to free the returned
 * reference using g_object_unref() when no longer needed.
 *
 * Return value: reference to the default sendto model.
 **/
ThunarSendtoModel*
thunar_sendto_model_get_default (void)
{
  static ThunarSendtoModel *sendto_model = NULL;

  if (G_UNLIKELY (sendto_model == NULL))
    {
      sendto_model = g_object_new (THUNAR_TYPE_SENDTO_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (sendto_model), (gpointer) &sendto_model);
    }
  else
    {
      g_object_ref (G_OBJECT (sendto_model));
    }

  return sendto_model;
}



/**
 * thunar_sendto_model_get_matching:
 * @sendto_model : a #ThunarSendtoModel.
 * @files        : a #GList of #ThunarFile<!---->s.
 * 
 * Returns the list of #ThunarVfsMimeHandler<!---->s for the "Send To"
 * targets that support the specified @files.
 *
 * The returned list is owned by the caller and must be freed when no
 * longer needed, using:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: a #GList of supported #ThunarVfsMimeHandler<!---->s as
 *               "Send To" targets for the specified @files.
 **/
GList*
thunar_sendto_model_get_matching (ThunarSendtoModel *sendto_model,
                                  GList             *files)
{
  ThunarVfsMimeHandlerFlags flags;
  ThunarVfsMonitorHandle   *handle;
  const gchar * const      *mime_types;
  ThunarVfsPath            *path;
  gchar                   **datadirs;
  gchar                    *dir;
  GList                    *handlers = NULL;
  GList                    *hp;
  GList                    *fp;
  guint                     n;

  _thunar_return_val_if_fail (THUNAR_IS_SENDTO_MODEL (sendto_model), NULL);

  /* no files, no sendto actions */
  if (G_UNLIKELY (files == NULL))
    return NULL;

  /* connect to the monitor on-demand */
  if (G_UNLIKELY (sendto_model->monitor == NULL))
    {
      /* connect to the monitor */
      sendto_model->monitor = thunar_vfs_monitor_get_default ();

      /* watch all possible sendto directories */
      datadirs = xfce_resource_dirs (XFCE_RESOURCE_DATA);
      for (n = 0; datadirs[n] != NULL; ++n)
        {
          /* determine the path to the sendto directory */
          dir = g_build_filename (datadirs[n], "Thunar", "sendto", NULL);
          path = thunar_vfs_path_new (dir, NULL);
          if (G_LIKELY (path != NULL))
            {
              /* watch the directory for changes */
              handle = thunar_vfs_monitor_add_file (sendto_model->monitor, path, thunar_sendto_model_event, sendto_model);
              sendto_model->handles = g_list_prepend (sendto_model->handles, handle);
              thunar_vfs_path_unref (path);
            }
          g_free (dir);
        }
      g_strfreev (datadirs);

      /* load the model */
      thunar_sendto_model_load (sendto_model);
    }

  /* test all handlers */
  for (hp = sendto_model->handlers; hp != NULL; hp = hp->next)
    {
      /* ignore the handler if it doesn't support multiple files, but we have more than one file */
      flags = thunar_vfs_mime_handler_get_flags (THUNAR_VFS_MIME_HANDLER (hp->data));
      if ((flags & THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI) == 0 && files->next != NULL)
        continue;

      /* ignore the handler if it doesn't support URIs, but we don't have a local file */
      if ((flags & THUNAR_VFS_MIME_HANDLER_SUPPORTS_URIS) == 0)
        {
          /* check if we have any non-local files */
          for (fp = files; fp != NULL; fp = fp->next)
            if (!thunar_file_is_local (fp->data))
              break;

          /* check if the test failed */
          if (G_UNLIKELY (fp != NULL))
            continue;
        }

      /* check if we need to test mime types for this handler */
      mime_types = thunar_vfs_mime_application_get_mime_types (hp->data);
      if (G_LIKELY (mime_types != NULL && *mime_types != NULL))
        {
          /* each file must match atleast one of the specified mime types */
          for (fp = files; fp != NULL; fp = fp->next)
            {
              /* each file must be supported by one of the mime types */
              for (n = 0; mime_types[n] != NULL; ++n)
                if (thunarx_file_info_has_mime_type (fp->data, mime_types[n]))
                  break;

              /* check if all mime types failed */
              if (mime_types[n] == NULL)
                break;
            }

          /* check if atleast one file failed */
          if (G_UNLIKELY (fp != NULL))
            {
              /* skip this handler */
              continue;
            }
        }

      /* the handler is supported */
      handlers = g_list_prepend (handlers, g_object_ref (G_OBJECT (hp->data)));
    }

  return handlers;
}


