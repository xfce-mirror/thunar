/* $Id$ */
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
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <thunar/thunar-file.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-private.h>



#define THUNAR_SHORTCUT(obj) ((ThunarShortcut *) (obj))


/* I don't particularly like this here, but it's shared across a few
 * files. */
const gchar *_thunar_user_directory_names[9] = {
  "Desktop", "Documents", "Download", "Music", "Pictures", "Public",
  "Templates", "Videos", NULL,
};


typedef struct _ThunarShortcut ThunarShortcut;

typedef enum
{
  THUNAR_SHORTCUT_SEPARATOR,
  THUNAR_SHORTCUT_SYSTEM_DEFINED,
  THUNAR_SHORTCUT_REMOVABLE_MEDIA,
  THUNAR_SHORTCUT_USER_DEFINED,
} ThunarShortcutType;



static void               thunar_shortcuts_model_tree_model_init    (GtkTreeModelIface         *iface);
static void               thunar_shortcuts_model_drag_source_init   (GtkTreeDragSourceIface    *iface);
static void               thunar_shortcuts_model_finalize           (GObject                   *object);
static GtkTreeModelFlags  thunar_shortcuts_model_get_flags          (GtkTreeModel              *tree_model);
static gint               thunar_shortcuts_model_get_n_columns      (GtkTreeModel              *tree_model);
static GType              thunar_shortcuts_model_get_column_type    (GtkTreeModel              *tree_model,
                                                                     gint                       idx);
static gboolean           thunar_shortcuts_model_get_iter           (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreePath               *path);
static GtkTreePath       *thunar_shortcuts_model_get_path           (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static void               thunar_shortcuts_model_get_value          (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     gint                       column,
                                                                     GValue                    *value);
static gboolean           thunar_shortcuts_model_iter_next          (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gboolean           thunar_shortcuts_model_iter_children      (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *parent);
static gboolean           thunar_shortcuts_model_iter_has_child     (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gint               thunar_shortcuts_model_iter_n_children    (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gboolean           thunar_shortcuts_model_iter_nth_child     (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *parent,
                                                                     gint                       n);
static gboolean           thunar_shortcuts_model_iter_parent        (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *child);
static gboolean           thunar_shortcuts_model_row_draggable      (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path);
static gboolean           thunar_shortcuts_model_drag_data_get      (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path,
                                                                     GtkSelectionData          *selection_data);
static gboolean           thunar_shortcuts_model_drag_data_delete   (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path);
static void               thunar_shortcuts_model_add_shortcut       (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut,
                                                                     GtkTreePath               *path);
static void               thunar_shortcuts_model_remove_shortcut    (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut);
static void               thunar_shortcuts_model_load               (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_save               (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_monitor            (GFileMonitor              *monitor,
                                                                     GFile                     *file,
                                                                     GFile                     *other_file,
                                                                     GFileMonitorEvent          event_type,
                                                                     gpointer                   user_data);
static void               thunar_shortcuts_model_file_changed       (ThunarFile                *file,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_file_destroy       (ThunarFile                *file,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_volume_added       (GVolumeMonitor            *volume_monitor,
                                                                     GVolume                   *volume,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_volume_removed     (GVolumeMonitor            *volume_monitor,
                                                                     GVolume                   *volume,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_volume_changed     (GVolumeMonitor            *monitor,
                                                                     GVolume                   *volume,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcut_free                      (ThunarShortcut            *shortcut,
                                                                     ThunarShortcutsModel      *model);



struct _ThunarShortcutsModelClass
{
  GObjectClass __parent__;
};

struct _ThunarShortcutsModel
{
  GObject         __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint            stamp;
#endif

  GList          *shortcuts;
  GList          *hidden_volumes;
  GVolumeMonitor *volume_monitor;

  GFileMonitor   *monitor;
};

struct _ThunarShortcut
{
  ThunarShortcutType type;

  gchar              *name;
  ThunarFile         *file;
  GVolume            *volume;
};



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsModel, thunar_shortcuts_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_shortcuts_model_tree_model_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, thunar_shortcuts_model_drag_source_init))


    
static void
thunar_shortcuts_model_class_init (ThunarShortcutsModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_model_finalize;
}



static void
thunar_shortcuts_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_shortcuts_model_get_flags;
  iface->get_n_columns = thunar_shortcuts_model_get_n_columns;
  iface->get_column_type = thunar_shortcuts_model_get_column_type;
  iface->get_iter = thunar_shortcuts_model_get_iter;
  iface->get_path = thunar_shortcuts_model_get_path;
  iface->get_value = thunar_shortcuts_model_get_value;
  iface->iter_next = thunar_shortcuts_model_iter_next;
  iface->iter_children = thunar_shortcuts_model_iter_children;
  iface->iter_has_child = thunar_shortcuts_model_iter_has_child;
  iface->iter_n_children = thunar_shortcuts_model_iter_n_children;
  iface->iter_nth_child = thunar_shortcuts_model_iter_nth_child;
  iface->iter_parent = thunar_shortcuts_model_iter_parent;
}



static void
thunar_shortcuts_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = thunar_shortcuts_model_row_draggable;
  iface->drag_data_get = thunar_shortcuts_model_drag_data_get;
  iface->drag_data_delete = thunar_shortcuts_model_drag_data_delete;
}



static void
thunar_shortcuts_model_init (ThunarShortcutsModel *model)
{
  ThunarShortcut  *shortcut;
  GtkTreePath     *path;
  ThunarFile      *file;
  GVolume         *volume;
  GFile           *bookmarks;
  GFile           *desktop;
  GFile           *home;
  GList           *system_paths = NULL;
  GList           *volumes;
  GList           *lp;

#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif

  /* connect to the volume monitor */
  model->volume_monitor = g_volume_monitor_get ();
  g_signal_connect (model->volume_monitor, "volume-added", G_CALLBACK (thunar_shortcuts_model_volume_added), model);
  g_signal_connect (model->volume_monitor, "volume-removed", G_CALLBACK (thunar_shortcuts_model_volume_removed), model);
  g_signal_connect (model->volume_monitor, "volume-changed", G_CALLBACK (thunar_shortcuts_model_volume_changed), model);

  /* add the home folder to the system paths */
  home = thunar_g_file_new_for_home ();
  system_paths = g_list_append (system_paths, g_object_ref (home));

  /* append the user's desktop folder */
  desktop = thunar_g_file_new_for_desktop ();
  if (!g_file_equal (desktop, home))
    system_paths = g_list_append (system_paths, desktop);
  else
    g_object_unref (desktop);

  /* append the trash icon if the trash is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    system_paths = g_list_append (system_paths, thunar_g_file_new_for_trash ());

  /* append the root file system */
  system_paths = g_list_append (system_paths, thunar_g_file_new_for_root ());

  /* append the network icon if browsing the network is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("network"))
    system_paths = g_list_append (system_paths, g_file_new_for_uri ("network://"));

  /* will be used to append the shortcuts to the list */
  path = gtk_tree_path_new_from_indices (0, -1);

  /* append the system defined items ('Home', 'Trash', 'File System') */
  for (lp = system_paths; lp != NULL; lp = lp->next)
    {
      /* determine the file for the path */
      file = thunar_file_get (lp->data, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* create the shortcut */
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->type = THUNAR_SHORTCUT_SYSTEM_DEFINED;
          shortcut->file = file;

          /* append the shortcut to the list */
          thunar_shortcuts_model_add_shortcut (model, shortcut, path);
          gtk_tree_path_next (path);
        }

      /* release the system defined path */
      g_object_unref (lp->data);
    }

  g_list_free (system_paths);

  /* prepend the removable media volumes */
  volumes = g_volume_monitor_get_volumes (model->volume_monitor);
  for (lp = volumes; lp != NULL; lp = lp->next)
    {
      /* monitor the volume for changes */
      volume = G_VOLUME (lp->data);

      /* we list only present, removable devices here */
      if (thunar_g_volume_is_removable (volume) && thunar_g_volume_is_present (volume))
        {
          /* generate the shortcut (w/o a file, else we might
           * prevent the volume from being unmounted)
           */
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->type = THUNAR_SHORTCUT_REMOVABLE_MEDIA;
          shortcut->volume = volume;

          /* link the shortcut to the list */
          thunar_shortcuts_model_add_shortcut (model, shortcut, path);
          gtk_tree_path_next (path);
        }
      else
        {
          /* schedule the volume for later checking, not removable or 
           * there's no medium present */
          model->hidden_volumes = g_list_prepend (model->hidden_volumes, volume);
        }
    }
  g_list_free (volumes);

  /* prepend the row separator */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->type = THUNAR_SHORTCUT_SEPARATOR;
  thunar_shortcuts_model_add_shortcut (model, shortcut, path);
  gtk_tree_path_next (path);

  /* determine the URI to the Gtk+ bookmarks file */
  bookmarks = g_file_resolve_relative_path (home, ".gtk-bookmarks");

  /* register with the alteration monitor for the bookmarks file */
  model->monitor = g_file_monitor_file (bookmarks, G_FILE_MONITOR_NONE, NULL, NULL);
  if (G_LIKELY (model->monitor != NULL))
    g_signal_connect (model->monitor, "changed", G_CALLBACK (thunar_shortcuts_model_monitor), model);

  /* read the Gtk+ bookmarks file */
  thunar_shortcuts_model_load (model);

  /* cleanup */
  g_object_unref (bookmarks);
  g_object_unref (home);
  gtk_tree_path_free (path);
}



static void
thunar_shortcuts_model_finalize (GObject *object)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (object);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* free all shortcuts */
  g_list_foreach (model->shortcuts, (GFunc) thunar_shortcut_free, model);
  g_list_free (model->shortcuts);

  /* free all hidden volumes */
  g_list_foreach (model->hidden_volumes, (GFunc) g_object_unref, NULL);
  g_list_free (model->hidden_volumes);

  /* detach from the file monitor */
  if (model->monitor != NULL)
    {
      g_file_monitor_cancel (model->monitor);
      g_object_unref (model->monitor);
    }

  /* unlink from the volume monitor */
  g_signal_handlers_disconnect_matched (model->volume_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
  g_object_unref (model->volume_monitor);

  (*G_OBJECT_CLASS (thunar_shortcuts_model_parent_class)->finalize) (object);
}



static GtkTreeModelFlags
thunar_shortcuts_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_shortcuts_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_SHORTCUTS_MODEL_N_COLUMNS;
}



static GType
thunar_shortcuts_model_get_column_type (GtkTreeModel *tree_model,
                                        gint          idx)
{
  switch (idx)
    {
    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME:
      return G_TYPE_VOLUME;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT:
      return G_TYPE_STRING;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_SEPARATOR:
      return G_TYPE_BOOLEAN;
    }

  _thunar_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_shortcuts_model_get_iter (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 GtkTreePath  *path)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);
  GList                 *lp;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  lp = g_list_nth (model->shortcuts, gtk_tree_path_get_indices (path)[0]);
  if (G_LIKELY (lp != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_shortcuts_model_get_path (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);
  gint                  idx;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* lookup the list item in the shortcuts list */
  idx = g_list_position (model->shortcuts, iter->user_data);
  if (G_LIKELY (idx >= 0))
    return gtk_tree_path_new_from_indices (idx, -1);

  return NULL;
}



static void
thunar_shortcuts_model_get_value (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value)
{
  ThunarShortcut *shortcut;
  ThunarFile     *file;
  GMount         *mount;
  GFile          *mount_point;

  _thunar_return_if_fail (iter->stamp == THUNAR_SHORTCUTS_MODEL (tree_model)->stamp);
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (tree_model));

  /* determine the shortcut for the list item */
  shortcut = THUNAR_SHORTCUT (((GList *) iter->user_data)->data);

  switch (column)
    {
    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (shortcut->volume != NULL))
        g_value_take_string (value, g_volume_get_name (shortcut->volume));
      else if (shortcut->name != NULL)
        g_value_set_static_string (value, shortcut->name);
      else if (shortcut->file != NULL)
        g_value_set_static_string (value, thunar_file_get_display_name (shortcut->file));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      if (shortcut->volume != NULL && shortcut->file == NULL)
        {
          /* determine the mount of the volume */
          mount = g_volume_get_mount (shortcut->volume);
          
          if (G_LIKELY (mount != NULL))
            {
              /* the volume is mounted, get the mount point */
              mount_point = g_mount_get_root (mount);

              /* try to allocate/reference a file pointing to the mount point */
              file = thunar_file_get (mount_point, NULL);
              g_value_take_object (value, file);

              /* release resources */
              g_object_unref (mount_point);
              g_object_unref (mount);
            }
        }
      else
        {
          g_value_set_object (value, shortcut->file);
        }
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME:
      g_value_init (value, G_TYPE_VOLUME);
      g_value_set_object (value, shortcut->volume);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->type == THUNAR_SHORTCUT_USER_DEFINED);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT:
      g_value_init (value, G_TYPE_STRING);
      if (shortcut->volume != NULL)
        {
          if (thunar_g_volume_is_removable (shortcut->volume) 
              && thunar_g_volume_is_present (shortcut->volume))
            {
              g_value_set_static_string (value, "media-eject");
            }
          else
            {
              g_value_set_static_string (value, "");
            }
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_SEPARATOR:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->type == THUNAR_SHORTCUT_SEPARATOR);
      break;

    default:
      _thunar_assert_not_reached ();
    }
}



static gboolean
thunar_shortcuts_model_iter_next (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (tree_model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_SHORTCUTS_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_list_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
thunar_shortcuts_model_iter_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL && model->shortcuts != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, model->shortcuts);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_shortcuts_model_iter_has_child (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_shortcuts_model_iter_n_children (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), 0);

  return (iter == NULL) ? g_list_length (model->shortcuts) : 0;
}



static gboolean
thunar_shortcuts_model_iter_nth_child (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent,
                                       gint          n)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, g_list_nth (model->shortcuts, n));
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
thunar_shortcuts_model_iter_parent (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
thunar_shortcuts_model_row_draggable (GtkTreeDragSource *source,
                                      GtkTreePath       *path)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (source);
  ThunarShortcut       *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* lookup the ThunarShortcut for the path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* special shortcuts cannot be reordered */
  return (shortcut != NULL && shortcut->type == THUNAR_SHORTCUT_USER_DEFINED);
}



static gboolean
thunar_shortcuts_model_drag_data_get (GtkTreeDragSource *source,
                                      GtkTreePath       *path,
                                      GtkSelectionData  *selection_data)
{
  /* we simply return FALSE here, as the drag handling is done in
   * the ThunarShortcutsView class.
   */
  return FALSE;
}



static gboolean
thunar_shortcuts_model_drag_data_delete (GtkTreeDragSource *source,
                                         GtkTreePath       *path)
{
  /* we simply return FALSE here, as this function can only be
   * called if the user is re-arranging shortcuts within the
   * model, which will be handle by the exchange method.
   */
  return FALSE;
}



static void
thunar_shortcuts_model_add_shortcut (ThunarShortcutsModel *model,
                                     ThunarShortcut       *shortcut,
                                     GtkTreePath          *path)
{
  GtkTreeIter iter;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (shortcut->file == NULL || THUNAR_IS_FILE (shortcut->file));
  _thunar_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] <= (gint) g_list_length (model->shortcuts));

  /* we want to stay informed about changes to the file */
  if (G_LIKELY (shortcut->file != NULL))
    {
      /* watch the file for changes */
      thunar_file_watch (shortcut->file);

      /* connect appropriate signals */
      g_signal_connect (G_OBJECT (shortcut->file), "changed",
                        G_CALLBACK (thunar_shortcuts_model_file_changed), model);
      g_signal_connect (G_OBJECT (shortcut->file), "destroy",
                        G_CALLBACK (thunar_shortcuts_model_file_destroy), model);
    }

  /* insert the new shortcut to the shortcuts list */
  model->shortcuts = g_list_insert (model->shortcuts, shortcut, gtk_tree_path_get_indices (path)[0]);

  /* tell everybody that we have a new shortcut */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
}



static void
thunar_shortcuts_model_remove_shortcut (ThunarShortcutsModel *model,
                                        ThunarShortcut       *shortcut)
{
  GtkTreePath *path;
  gint         idx;

  /* determine the index of the shortcut */
  idx = g_list_index (model->shortcuts, shortcut);
  if (G_LIKELY (idx >= 0))
    {
      /* unlink the shortcut from the model */
      model->shortcuts = g_list_remove (model->shortcuts, shortcut);

      /* tell everybody that we have lost a shortcut */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
      gtk_tree_path_free (path);

      /* actually free the shortcut */
      thunar_shortcut_free (shortcut, model);

      /* the shortcuts list was changed, so write the gtk bookmarks file */
      thunar_shortcuts_model_save (model);
    }
}

/* Reads the current xdg user dirs locale from ~/.config/xdg-user-dirs.locale
 * Notice that the result shall be freed by using g_free (). */
gchar *
_thunar_get_xdg_user_dirs_locale (void)
{
  gchar *file    = NULL;
  gchar *content = NULL;
  gchar *locale  = NULL;

  /* get the file pathname */
  file = g_build_filename (g_get_user_config_dir (), LOCALE_FILE_NAME, NULL);

  /* grab the contents and get ride of the surrounding spaces */
  if (g_file_get_contents (file, &content, NULL, NULL))
    locale = g_strdup (g_strstrip (content));

  g_free (content);
  g_free (file);

  /* if we got nothing, let's set the default locale as C */
  if (exo_str_is_equal (locale, ""))
    {
      g_free (locale);
      locale = g_strdup ("C");
    }

  return locale;
}

static void
thunar_shortcuts_model_load (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GtkTreePath     *path;
  const gchar     *user_special_dir = NULL;
  ThunarFile      *file;
  GFile           *file_path;
  GFile           *home;
  gchar           *bookmarks_path;
  gchar            line[2048];
  gchar           *name;
  FILE            *fp;
  gint             i;

  home = thunar_g_file_new_for_home ();

  /* determine the path to the GTK+ bookmarks file */
  bookmarks_path = xfce_get_homefile (".gtk-bookmarks", NULL);

  /* append the GTK+ bookmarks (if any) */
  fp = fopen (bookmarks_path, "r");
  if (G_LIKELY (fp != NULL))
    {
      /* allocate a tree path for appending to the model */
      path = gtk_tree_path_new_from_indices (g_list_length (model->shortcuts), -1);

      while (fgets (line, sizeof (line), fp) != NULL)
        {
          /* strip leading/trailing whitespace */
          g_strstrip (line);

          /* skip over the URI */
          for (name = line; *name != '\0' && !g_ascii_isspace (*name); ++name)
            ;

          /* zero-terminate the URI */
          *name++ = '\0';

          /* check if we have a name */
          for (; g_ascii_isspace (*name); ++name)
            ;

          /* parse the URI */
          file_path = g_file_new_for_uri (line);

          /* try to open the file corresponding to the uri */
          file = thunar_file_get (file_path, NULL);
          g_object_unref (file_path);

          if (G_UNLIKELY (file == NULL))
            continue;

          /* make sure the file refers to a directory */
          if (G_UNLIKELY (thunar_file_is_directory (file)))
            {
              /* create the shortcut entry */
              shortcut = g_slice_new0 (ThunarShortcut);
              shortcut->type = THUNAR_SHORTCUT_USER_DEFINED;
              shortcut->file = file;
              shortcut->name = (*name != '\0') ? g_strdup (name) : NULL;

              /* append the shortcut to the list */
              thunar_shortcuts_model_add_shortcut (model, shortcut, path);
              gtk_tree_path_next (path);
            }
          else
            {
              g_object_unref (file);
            }
        }

      /* clean up */
      gtk_tree_path_free (path);
      fclose (fp);
    }
  else
    {
      /* ~/.gtk-bookmarks wasn't there or it was unreadable.
       * here we recreate it with some useful xdg user special dirs */
      const char *old_locale = NULL;
      gchar *locale          = NULL;

      bindtextdomain (XDG_USER_DIRS_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (XDG_USER_DIRS_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */

      /* save the old locale */
      old_locale = setlocale (LC_MESSAGES, NULL);

      /* set the new locale */
      locale = _thunar_get_xdg_user_dirs_locale ();
      setlocale (LC_MESSAGES, locale);
      g_free (locale);

      path = gtk_tree_path_new_from_indices (g_list_length (model->shortcuts), -1);
      for (i = G_USER_DIRECTORY_DESKTOP;
           i < G_USER_N_DIRECTORIES && _thunar_user_directory_names[i] != NULL;
           ++i)
        {
          /* let's ignore some directories we don't want in the side pane */
          if (i == G_USER_DIRECTORY_DESKTOP
              || i == G_USER_DIRECTORY_PUBLIC_SHARE
              || i == G_USER_DIRECTORY_TEMPLATES)
            {
              continue;
            }

          user_special_dir = g_get_user_special_dir (i);

          if (G_UNLIKELY (user_special_dir == NULL))
            continue;

          file_path = g_file_new_for_path (user_special_dir);
          if (G_UNLIKELY (g_file_equal (file_path, home)))
            {
              g_object_unref (file_path);
              continue;
            }

          /* try to open the file corresponding to the uri */
          file = thunar_file_get (file_path, NULL);
          g_object_unref (file_path);

          if (G_UNLIKELY (file == NULL))
            continue;

          /* make sure the file refers to a directory */
          if (G_UNLIKELY (!thunar_file_is_directory (file)))
            {
              g_object_unref (file);
              continue;
            }

          /* create the shortcut entry */
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->type = THUNAR_SHORTCUT_USER_DEFINED;
          shortcut->file = file;
          shortcut->name = g_strdup (dgettext (XDG_USER_DIRS_PACKAGE, 
                                               (gchar *) _thunar_user_directory_names[i]));

          /* append the shortcut to the list */
          thunar_shortcuts_model_add_shortcut (model, shortcut, path);
          gtk_tree_path_next (path);
        }

      /* restore the old locale */
      setlocale (LC_MESSAGES, old_locale);

      gtk_tree_path_free (path);

      /* we try to save the obtained new model */
      thunar_shortcuts_model_save (model);
    }

  /* clean up */
  g_object_unref (home);
  g_free (bookmarks_path);
}



static void
thunar_shortcuts_model_monitor (GFileMonitor     *monitor,
                                GFile            *file,
                                GFile            *other_file,
                                GFileMonitorEvent event_type,
                                gpointer          user_data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (user_data);
  ThunarShortcut       *shortcut;
  GtkTreePath          *path;
  GList                *lp;
  gint                  idx;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (model->monitor == monitor);

  /* drop all existing user-defined shortcuts from the model */
  for (idx = 0, lp = model->shortcuts; lp != NULL; )
    {
      /* grab the shortcut */
      shortcut = THUNAR_SHORTCUT (lp->data);

      /* advance to the next list item */
      lp = g_list_next (lp);

      /* drop the shortcut if it is user-defined */
      if (shortcut->type == THUNAR_SHORTCUT_USER_DEFINED)
        {
          /* unlink the shortcut from the model */
          model->shortcuts = g_list_remove (model->shortcuts, shortcut);
          
          /* tell everybody that we have lost a shortcut */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
          gtk_tree_path_free (path);

          /* actually free the shortcut */
          thunar_shortcut_free (shortcut, model);
        }
      else
        {
          ++idx;
        }
    }

  /* reload the shortcuts model */
  thunar_shortcuts_model_load (model);
}



static void
thunar_shortcuts_model_save (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  gchar          *bookmarks_path;
  gchar          *tmp_path;
  gchar          *uri;
  GList          *lp;
  FILE           *fp;
  gint            fd;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* open a temporary file for writing */
  tmp_path = xfce_get_homefile (".gtk-bookmarks.XXXXXX", NULL);
  fd = g_mkstemp (tmp_path);
  if (G_UNLIKELY (fd < 0))
    {
      g_warning ("Failed to open `%s' for writing: %s",
                 tmp_path, g_strerror (errno));
      g_free (tmp_path);
      return;
    }

  /* write the uris of user customizable shortcuts */
  fp = fdopen (fd, "w");
  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);
      if (shortcut->type == THUNAR_SHORTCUT_USER_DEFINED)
        {
          uri = g_file_get_uri (thunar_file_get_file (shortcut->file));
          if (G_LIKELY (shortcut->name != NULL))
            fprintf (fp, "%s %s\n", uri, shortcut->name);
          else
            fprintf (fp, "%s\n", uri);
          g_free (uri);
        }
    }

  /* we're done writing the temporary file */
  fclose (fp);

  /* move the temporary file to it's final location (atomic writing) */
  bookmarks_path = xfce_get_homefile (".gtk-bookmarks", NULL);
  if (rename (tmp_path, bookmarks_path) < 0)
    {
      g_warning ("Failed to write `%s': %s",
                 bookmarks_path, g_strerror (errno));
      g_unlink (tmp_path);
    }

  /* cleanup */
  g_free (bookmarks_path);
  g_free (tmp_path);
}



static void
thunar_shortcuts_model_file_changed (ThunarFile           *file,
                                     ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GtkTreePath    *path;
  GtkTreeIter     iter;
  GList          *lp;
  gint            idx;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
 
  /* check if the file still refers to a directory or a not mounted URI, 
   * otherwise we cannot keep it on the shortcuts list, and so we'll treat 
   * it like the file was destroyed (and thereby removed) */

  if (G_UNLIKELY (!thunar_file_is_directory (file)))
    {
      thunar_shortcuts_model_file_destroy (file, model);
      return;
    }

  for (idx = 0, lp = model->shortcuts; lp != NULL; ++idx, lp = lp->next)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);
      if (shortcut->file == file)
        {
          GTK_TREE_ITER_INIT (iter, model->stamp, lp);

          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
          break;
        }
    }
}



static void
thunar_shortcuts_model_file_destroy (ThunarFile           *file,
                                     ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut = NULL;
  GList          *lp;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* lookup the shortcut matching the file */
  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);
      if (shortcut->file == file)
        break;
    }

  /* verify that we actually found a shortcut */
  _thunar_assert (lp != NULL);
  _thunar_assert (THUNAR_IS_FILE (shortcut->file));

  /* drop the shortcut from the model */
  thunar_shortcuts_model_remove_shortcut (model, shortcut);
}



static void
thunar_shortcuts_model_volume_changed (GVolumeMonitor       *volume_monitor,
                                       GVolume              *volume,
                                       ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut = NULL;
  GtkTreePath    *path;
  GtkTreeIter     iter;
  GList          *lp;
  gint            idx;

  _thunar_return_if_fail (G_IS_VOLUME_MONITOR (volume_monitor));
  _thunar_return_if_fail (model->volume_monitor == volume_monitor);
  _thunar_return_if_fail (G_IS_VOLUME (volume));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* check if the volume is on the hidden list */
  lp = g_list_find (model->hidden_volumes, volume);
  if (lp != NULL)
    {
      /* check if we need to display the volume now */
      if (thunar_g_volume_is_removable (volume) && thunar_g_volume_is_present (volume))
        {
          /* remove the volume from the list of hidden volumes */
          model->hidden_volumes = g_list_delete_link (model->hidden_volumes, lp);

          /* find the insert position */
          for (idx = 0, lp = model->shortcuts; lp != NULL; ++idx, lp = lp->next)
            {
              shortcut = THUNAR_SHORTCUT (lp->data);
              if (shortcut->type == THUNAR_SHORTCUT_SEPARATOR
                  || shortcut->type == THUNAR_SHORTCUT_USER_DEFINED)
                break;
            }

          /* allocate a new shortcut */
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->type = THUNAR_SHORTCUT_REMOVABLE_MEDIA;
          shortcut->volume = volume;

          /* the volume is present now, so we have to display it */
          path = gtk_tree_path_new_from_indices (idx, -1);
          thunar_shortcuts_model_add_shortcut (model, shortcut, path);
          gtk_tree_path_free (path);
        }
    }
  else
    {
      /* lookup the shortcut that contains the given volume */
      for (idx = 0, lp = model->shortcuts; 
           shortcut == NULL && lp != NULL; 
           ++idx, lp = lp->next)
        {
          if (THUNAR_SHORTCUT (lp->data)->volume == volume)
            shortcut = lp->data;
        }

      /* verify that we actually found the shortcut */
      _thunar_assert (shortcut != NULL);
      _thunar_assert (shortcut->volume == volume);

      /* check if we need to hide the volume now */
      if (!thunar_g_volume_is_removable (volume) || !thunar_g_volume_is_present (volume))
        {
          /* move the volume to the hidden list */
          model->hidden_volumes = g_list_prepend (model->hidden_volumes, 
                                                  g_object_ref (volume));

          /* remove the shortcut from the user interface */
          thunar_shortcuts_model_remove_shortcut (model, shortcut);
        }
      else
        {
          /* generate an iterator for the path */
          GTK_TREE_ITER_INIT (iter, model->stamp, lp);

          /* tell the view that the volume has changed in some way */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}



static void
thunar_shortcuts_model_volume_added (GVolumeMonitor       *volume_monitor,
                                     GVolume              *volume,
                                     ThunarShortcutsModel *model)
{
  _thunar_return_if_fail (G_IS_VOLUME_MONITOR (volume_monitor));
  _thunar_return_if_fail (model->volume_monitor == volume_monitor);
  _thunar_return_if_fail (G_IS_VOLUME (volume));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* place the volume on the hidden list */
  model->hidden_volumes = g_list_prepend (model->hidden_volumes, g_object_ref (volume));

  /* let the "changed" handler place the volume where appropriate */
  thunar_shortcuts_model_volume_changed (volume_monitor, volume, model);
}



static void
thunar_shortcuts_model_volume_removed (GVolumeMonitor       *volume_monitor,
                                       GVolume              *volume,
                                       ThunarShortcutsModel *model)
{
  GList *lp;

  _thunar_return_if_fail (G_IS_VOLUME_MONITOR (volume_monitor));
  _thunar_return_if_fail (model->volume_monitor == volume_monitor);
  _thunar_return_if_fail (G_IS_VOLUME (volume));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  
  lp = g_list_find (model->hidden_volumes, volume);
  if (G_LIKELY (lp != NULL))
    {
      /* remove the volume from the hidden list and drop our reference */
      model->hidden_volumes = g_list_delete_link (model->hidden_volumes, lp);
      g_object_unref (volume);
    }
  else
    {
      /* must be an active shortcut then... */
      for (lp = model->shortcuts; lp != NULL; lp = lp->next)
        if (THUNAR_SHORTCUT (lp->data)->volume == volume)
          break;

      /* something is broken if we don't have a shortcut here */
      _thunar_assert (lp != NULL);
      _thunar_assert (THUNAR_SHORTCUT (lp->data)->volume == volume);

      /* drop the shortcut from the model */
      thunar_shortcuts_model_remove_shortcut (model, lp->data);
    }
}



static void
thunar_shortcut_free (ThunarShortcut       *shortcut,
                      ThunarShortcutsModel *model)
{
  if (G_LIKELY (shortcut->file != NULL))
    {
      /* drop the file watch */
      thunar_file_unwatch (shortcut->file);

      /* unregister from the file */
      g_signal_handlers_disconnect_matched (shortcut->file,
                                            G_SIGNAL_MATCH_DATA, 0,
                                            0, NULL, NULL, model);
      g_object_unref (shortcut->file);
    }

  if (G_LIKELY (shortcut->volume != NULL))
    {
      g_signal_handlers_disconnect_matched (shortcut->volume,
                                            G_SIGNAL_MATCH_DATA, 0,
                                            0, NULL, NULL, model);
      g_object_unref (shortcut->volume);
    }

  /* release the shortcut name */
  g_free (shortcut->name);

  /* release the shortcut itself */
  g_slice_free (ThunarShortcut, shortcut);
}



/**
 * thunar_shortcuts_model_get_default:
 *
 * Returns the default #ThunarShortcutsModel instance shared by
 * all #ThunarShortcutsView instances.
 *
 * Call #g_object_unref() on the returned object when you
 * don't need it any longer.
 *
 * Return value: the default #ThunarShortcutsModel instance.
 **/
ThunarShortcutsModel*
thunar_shortcuts_model_get_default (void)
{
  static ThunarShortcutsModel *model = NULL;

  if (G_UNLIKELY (model == NULL))
    {
      model = g_object_new (THUNAR_TYPE_SHORTCUTS_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);
    }
  else
    {
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



/**
 * thunar_shortcuts_model_iter_for_file:
 * @model : a #ThunarShortcutsModel instance.
 * @file  : a #ThunarFile instance.
 * @iter  : pointer to a #GtkTreeIter.
 *
 * Tries to lookup the #GtkTreeIter, that belongs to a shortcut, which
 * refers to @file and stores it to @iter. If no such #GtkTreeIter was
 * found, %FALSE will be returned and @iter won't be changed. Else
 * %TRUE will be returned and @iter will be set appropriately.
 *
 * Return value: %TRUE if @file was found, else %FALSE.
 **/
gboolean
thunar_shortcuts_model_iter_for_file (ThunarShortcutsModel *model,
                                      ThunarFile           *file,
                                      GtkTreeIter          *iter)
{
  GMount *mount;
  GFile  *mount_point;
  GList  *lp;
  
  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (iter != NULL, FALSE);

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      /* check if we have a file that matches */
      if (THUNAR_SHORTCUT (lp->data)->file == file)
        {
          GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
          return TRUE;
        }

      /* but maybe we have a mounted(!) volume with a matching mount point */
      if (THUNAR_SHORTCUT (lp->data)->volume != NULL)
        {
          mount = g_volume_get_mount (THUNAR_SHORTCUT (lp->data)->volume);

          if (G_LIKELY (mount != NULL))
            {
              mount_point = g_mount_get_root (mount);

              if (G_LIKELY (g_file_equal (mount_point, thunar_file_get_file (file))))
                {
                  GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
                  g_object_unref (mount_point);
                  g_object_unref (mount);
                  return TRUE;
                }

              g_object_unref (mount_point);
              g_object_unref (mount);
            }
        }
    }

  return FALSE;
}



/**
 * thunar_shortcuts_model_drop_possible:
 * @model : a #ThunarShortcutstModel.
 * @path  : a #GtkTreePath.
 *
 * Determines whether a drop is possible before the given @path, at the same depth
 * as @path. I.e., can we drop data at that location. @path does not have to exist;
 * the return value will almost certainly be FALSE if the parent of @path doesn't
 * exist, though.
 *
 * Return value: %TRUE if it's possible to drop data before @path, else %FALSE.
 **/
gboolean
thunar_shortcuts_model_drop_possible (ThunarShortcutsModel *model,
                                      GtkTreePath          *path)
{
  ThunarShortcut *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* append to the list is always possible */
  if (G_LIKELY (shortcut == NULL))
    return TRUE;

  /* cannot drop before special shortcuts! */
  return (shortcut->type == THUNAR_SHORTCUT_USER_DEFINED);
}



/**
 * thunar_shortcuts_model_add:
 * @model    : a #ThunarShortcutsModel.
 * @dst_path : the destination path.
 * @file     : the #ThunarFile that should be added to the shortcuts list.
 *
 * Adds the shortcut @file to the @model at @dst_path, unless @file is
 * already present in @model in which case no action is performed.
 **/
void
thunar_shortcuts_model_add (ThunarShortcutsModel *model,
                            GtkTreePath          *dst_path,
                            ThunarFile           *file)
{
  ThunarShortcut *shortcut;
  GList           *lp;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] <= (gint) g_list_length (model->shortcuts));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* verify that the file is not already in use as shortcut */
  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    if (THUNAR_SHORTCUT (lp->data)->file == file)
      return;

  /* create the new shortcut that will be inserted */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->type = THUNAR_SHORTCUT_USER_DEFINED;
  shortcut->file = g_object_ref (G_OBJECT (file));

  /* add the shortcut to the list at the given position */
  thunar_shortcuts_model_add_shortcut (model, shortcut, dst_path);

  /* the shortcuts list was changed, so write the gtk bookmarks file */
  thunar_shortcuts_model_save (model);
}



/**
 * thunar_shortcuts_model_move:
 * @model    : a #ThunarShortcutsModel.
 * @src_path : the source path.
 * @dst_path : the destination path.
 *
 * Moves the shortcut at @src_path to @dst_path, adjusting other
 * shortcut's positions as required.
 **/
void
thunar_shortcuts_model_move (ThunarShortcutsModel *model,
                             GtkTreePath          *src_path,
                             GtkTreePath          *dst_path)
{
  ThunarShortcut *shortcut;
  GtkTreePath     *path;
  GList           *lp;
  gint            *order;
  gint             index_src;
  gint             index_dst;
  gint             idx;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (src_path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (src_path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (src_path)[0] < (gint) g_list_length (model->shortcuts));
  _thunar_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] > 0);

  index_src = gtk_tree_path_get_indices (src_path)[0];
  index_dst = gtk_tree_path_get_indices (dst_path)[0];

  if (G_UNLIKELY (index_src == index_dst))
    return;

  /* generate the order for the rows prior the dst/src rows */
  order = g_newa (gint, g_list_length (model->shortcuts));
  for (idx = 0, lp = model->shortcuts; idx < index_src && idx < index_dst; ++idx, lp = lp->next)
    order[idx] = idx;

  if (idx == index_src)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);

      for (; idx < index_dst; ++idx, lp = lp->next)
        {
          lp->data = lp->next->data;
          order[idx] = idx + 1;
        }

      lp->data = shortcut;
      order[idx++] = index_src;
    }
  else
    {
      for (; idx < index_src; ++idx, lp = lp->next)
        ;

      _thunar_assert (idx == index_src);

      shortcut = THUNAR_SHORTCUT (lp->data);

      for (; idx > index_dst; --idx, lp = lp->prev)
        {
          lp->data = lp->prev->data;
          order[idx] = idx - 1;
        }

      _thunar_assert (idx == index_dst);

      lp->data = shortcut;
      order[idx] = index_src;
      idx = index_src + 1;
    }

  /* generate the remaining order */
  for (; idx < (gint) g_list_length (model->shortcuts); ++idx)
    order[idx] = idx;

  /* tell all listeners about the reordering just performed */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, order);
  gtk_tree_path_free (path);

  /* the shortcuts list was changed, so write the gtk bookmarks file */
  thunar_shortcuts_model_save (model);
}



/**
 * thunar_shortcuts_model_remove:
 * @model : a #ThunarShortcutsModel.
 * @path  : the #GtkTreePath of the shortcut to remove.
 *
 * Removes the shortcut at @path from the @model and syncs to
 * on-disk storage. @path must refer to a valid, user-defined
 * shortcut, as you cannot remove system-defined entities (they
 * are managed internally).
 **/
void
thunar_shortcuts_model_remove (ThunarShortcutsModel *model,
                               GtkTreePath          *path)
{
  ThunarShortcut *shortcut;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] < (gint) g_list_length (model->shortcuts));

  /* lookup the shortcut for the given path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* verify that the shortcut is removable */
  _thunar_assert (shortcut->type == THUNAR_SHORTCUT_USER_DEFINED);
  _thunar_assert (THUNAR_IS_FILE (shortcut->file));

  /* remove the shortcut (using the file destroy handler) */
  thunar_shortcuts_model_file_destroy (shortcut->file, model);
}



/**
 * thunar_shortcuts_model_rename:
 * @model : a #ThunarShortcutsModel.
 * @iter  : the #GtkTreeIter which refers to the shortcut that
 *          should be renamed to @name.
 * @name  : the new name for the shortcut at @path or %NULL to
 *          return to the default name.
 *
 * Renames the shortcut at @iter to the new @name in @model.
 *
 * @name may be %NULL or an empty to reset the shortcut to
 * its default name.
 **/
void
thunar_shortcuts_model_rename (ThunarShortcutsModel *model,
                               GtkTreeIter          *iter,
                               const gchar          *name)
{
  ThunarShortcut *shortcut;
  GtkTreePath    *path;

  _thunar_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (iter->stamp == model->stamp);
  _thunar_return_if_fail (iter->user_data != NULL);

  /* lookup the shortcut for the given path */
  shortcut = THUNAR_SHORTCUT (((GList *) iter->user_data)->data);

  /* verify the shortcut */
  _thunar_assert (shortcut->type == THUNAR_SHORTCUT_USER_DEFINED);
  _thunar_assert (THUNAR_IS_FILE (shortcut->file));

  /* perform the rename */
  if (G_UNLIKELY (shortcut->name != NULL))
    g_free (shortcut->name);
  if (G_UNLIKELY (name == NULL || *name == '\0'))
    shortcut->name = NULL;
  else
    shortcut->name = g_strdup (name);

  /* notify the views about the change */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
  gtk_tree_path_free (path);

  /* save the changes to the model */
  thunar_shortcuts_model_save (model);
}




