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
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-private.h>

#define SPINNER_CYCLE_DURATION 1000
#define SPINNER_NUM_STEPS      12



#define THUNAR_SHORTCUT(obj) ((ThunarShortcut *) (obj))



typedef struct _ThunarShortcut ThunarShortcut;



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
                                                                     ThunarShortcut            *shortcut);
static void               thunar_shortcuts_model_remove_shortcut    (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut);
static gboolean           thunar_shortcuts_model_load               (gpointer                   data);
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
static void               thunar_shortcuts_model_device_added       (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_device_removed     (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_device_changed     (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
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
  gint                 stamp;
#endif

  GList               *shortcuts;

  ThunarDeviceMonitor *device_monitor;
  guint                devices_monitor_idle_id;

  GFileMonitor        *monitor;
  guint                load_idle_id;

  guint busy_timeout_id;
};

struct _ThunarShortcut
{
  ThunarShortcutGroup  group;

  guint                is_header : 1;

  gchar               *name;
  GIcon               *gicon;
  gint                 sort_id;

  guint                busy : 1;
  guint                busy_pule;

  GFile               *location;
  ThunarFile          *file;
  ThunarDevice        *device;
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



static gboolean
thunar_shortcuts_model_devices_load (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  ThunarShortcut       *shortcut;
  GList                *devices;
  GList                *lp;

  GDK_THREADS_ENTER ();

  /* add the devices heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES;
  shortcut->name = g_strdup (_("DEVICES"));
  shortcut->is_header = TRUE;
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* the filesystem entry */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES;
  shortcut->name = g_strdup (_("File System"));
  shortcut->file = thunar_file_get_for_uri ("file:///", NULL);
  shortcut->gicon = g_themed_icon_new (GTK_STOCK_HARDDISK);
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* connect to the device monitor */
  model->device_monitor = thunar_device_monitor_get ();

  /* get a list of all devices available */
  devices = thunar_device_monitor_get_devices (model->device_monitor);
  for (lp = devices; lp != NULL; lp = lp->next)
    {
      thunar_shortcuts_model_device_added (model->device_monitor, lp->data, model);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (devices);

  GDK_THREADS_LEAVE ();

  /* monitor for changes */
  g_signal_connect (model->device_monitor, "device-added", G_CALLBACK (thunar_shortcuts_model_device_added), model);
  g_signal_connect (model->device_monitor, "device-removed", G_CALLBACK (thunar_shortcuts_model_device_removed), model);
  g_signal_connect (model->device_monitor, "device-changed", G_CALLBACK (thunar_shortcuts_model_device_changed), model);

  model->devices_monitor_idle_id = 0;

  return FALSE;
}



static void
thunar_shortcuts_model_shortcut_network (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;

  /* add the network heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK;
  shortcut->name = g_strdup (_("NETWORK"));
  shortcut->is_header = TRUE;
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* the browse network entry */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK;
  shortcut->name = g_strdup (_("Browse Network"));
  shortcut->location = g_file_new_for_uri ("network://");
  shortcut->gicon = g_themed_icon_new (GTK_STOCK_NETWORK);
  thunar_shortcuts_model_add_shortcut (model, shortcut);
}



static void
thunar_shortcuts_model_shortcut_places (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GFile          *home;
  GFile          *bookmarks;
  GFile          *desktop;
  GFile          *trash;
  ThunarFile     *file;

  /* add the places heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES;
  shortcut->name = g_strdup (_("PLACES"));
  shortcut->is_header = TRUE;
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* get home path */
  home = thunar_g_file_new_for_home ();

  /* add home entry */
  file = thunar_file_get (home, NULL);
  if (file != NULL)
    {
      shortcut = g_slice_new0 (ThunarShortcut);
      shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES;
      shortcut->file = file;
      shortcut->sort_id = 0;
      thunar_shortcuts_model_add_shortcut (model, shortcut);
    }

  /* add desktop entry */
  desktop = thunar_g_file_new_for_desktop ();
  if (!g_file_equal (desktop, home))
    {
      file = thunar_file_get (desktop, NULL);
      if (file != NULL)
        {
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES;
          shortcut->file = file;
          shortcut->sort_id =  1;
          thunar_shortcuts_model_add_shortcut (model, shortcut);
        }
    }
  g_object_unref (desktop);

  /* append the trash icon if the trash is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      trash = thunar_g_file_new_for_trash ();
      file = thunar_file_get (trash, NULL);
      g_object_unref (trash);

      if (file != NULL)
        {
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->group = THUNAR_SHORTCUT_GROUP_TRASH;
          shortcut->file = file;
          thunar_shortcuts_model_add_shortcut (model, shortcut);
        }
    }

  /* determine the URI to the Gtk+ bookmarks file */
  bookmarks = g_file_resolve_relative_path (home, ".gtk-bookmarks");

  /* register with the alteration monitor for the bookmarks file */
  model->monitor = g_file_monitor_file (bookmarks, G_FILE_MONITOR_NONE, NULL, NULL);
  if (G_LIKELY (model->monitor != NULL))
    g_signal_connect (model->monitor, "changed", G_CALLBACK (thunar_shortcuts_model_monitor), model);

  /* read the Gtk+ bookmarks file */
  model->load_idle_id = g_idle_add (thunar_shortcuts_model_load, model);

  g_object_unref (home);
  g_object_unref (bookmarks);
}




static void
thunar_shortcuts_model_init (ThunarShortcutsModel *model)
{
#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif

  /* load volumes */
  model->devices_monitor_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_shortcuts_model_devices_load, model, NULL);

  /* add network */
  thunar_shortcuts_model_shortcut_network (model);

  /* add bookmarks */
  thunar_shortcuts_model_shortcut_places (model);
}



static void
thunar_shortcuts_model_finalize (GObject *object)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (object);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* stop the busy timeout */
  if (model->busy_timeout_id != 0)
    g_source_remove (model->busy_timeout_id);

  /* stop bookmark load idle */
  if (model->load_idle_id != 0)
    g_source_remove (model->load_idle_id);

  /* stop device monitor loading */
  if (model->devices_monitor_idle_id != 0)
    g_source_remove (model->devices_monitor_idle_id);

  /* free all shortcuts */
  g_list_foreach (model->shortcuts, (GFunc) thunar_shortcut_free, model);
  g_list_free (model->shortcuts);

  /* detach from the file monitor */
  if (model->monitor != NULL)
    {
      g_file_monitor_cancel (model->monitor);
      g_object_unref (model->monitor);
    }

  /* unlink from the device monitor */
  g_signal_handlers_disconnect_matched (model->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
  g_object_unref (model->device_monitor);

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
    case THUNAR_SHORTCUTS_MODEL_COLUMN_HEADER:
    case THUNAR_SHORTCUTS_MODEL_COLUMN_NOT_HEADER:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION:
      return G_TYPE_FILE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GICON:
      return G_TYPE_ICON;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE:
      return THUNAR_TYPE_DEVICE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP:
      return G_TYPE_UINT;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE:
      return G_TYPE_UINT;
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
  gboolean        can_eject;

  _thunar_return_if_fail (iter->stamp == THUNAR_SHORTCUTS_MODEL (tree_model)->stamp);
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (tree_model));

  /* determine the shortcut for the list item */
  shortcut = THUNAR_SHORTCUT (((GList *) iter->user_data)->data);

  switch (column)
    {
    case THUNAR_SHORTCUTS_MODEL_COLUMN_HEADER:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->is_header);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_NOT_HEADER:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, !shortcut->is_header);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (shortcut->device != NULL))
        g_value_take_string (value, thunar_device_get_name (shortcut->device));
      else if (shortcut->name != NULL)
        g_value_set_static_string (value, shortcut->name);
      else if (shortcut->file != NULL)
        g_value_set_static_string (value, thunar_file_get_display_name (shortcut->file));
      else if (shortcut->location != NULL)
        g_value_take_string (value, thunar_g_file_get_display_name (shortcut->location));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, shortcut->file);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GICON:
      g_value_init (value, G_TYPE_ICON);
      g_value_set_object (value, shortcut->gicon);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE:
      g_value_init (value, THUNAR_TYPE_DEVICE);
      g_value_set_object (value, shortcut->device);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION:
      g_value_init (value, G_TYPE_FILE);
      if (shortcut->location != NULL)
        g_value_set_object (value, shortcut->location);
      else if (shortcut->file != NULL)
        g_value_set_object (value, thunar_file_get_file (shortcut->file));
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT:
      if (shortcut->device != NULL)
        can_eject = thunar_device_can_eject (shortcut->device);
      else
        can_eject = FALSE;

      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, can_eject);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, shortcut->group);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->busy);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, shortcut->busy_pule);
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
  return (shortcut != NULL && shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS);
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



static gint
thunar_shortcuts_model_sort_func (gconstpointer shortcut_a,
                                  gconstpointer shortcut_b)
{
  const ThunarShortcut *a = shortcut_a;
  const ThunarShortcut *b = shortcut_b;

  /* sort groups */
  if (a->group != b->group)
    return a->group - b->group;

  /* sort header at the top of a group */
  if (a->is_header != b->is_header)
    return b->is_header ? 1 : -1;

  /* use sort order */
  if (a->sort_id != b->sort_id)
    return a->sort_id > b->sort_id ? 1 : -1;

  /* properly sort devices by timestamp */
  if (a->device != NULL && b->device != NULL)
    return -g_strcmp0 (thunar_device_get_sort_key (a->device),
                       thunar_device_get_sort_key (b->device));

  return g_strcmp0 (a->name, b->name);
}



static void
thunar_shortcuts_model_add_shortcut_with_path (ThunarShortcutsModel *model,
                                               ThunarShortcut       *shortcut,
                                               GtkTreePath          *path)
{
  GtkTreeIter  iter;
  GtkTreePath *sorted_path = NULL;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (shortcut->file == NULL || THUNAR_IS_FILE (shortcut->file));

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

  if (path == NULL)
    {
      /* insert the new shortcut to the shortcuts list */
      model->shortcuts = g_list_insert_sorted (model->shortcuts, shortcut, thunar_shortcuts_model_sort_func);
      sorted_path = gtk_tree_path_new_from_indices (g_list_index (model->shortcuts, shortcut), -1);
      path = sorted_path;
    }
  else
    {
      model->shortcuts = g_list_insert (model->shortcuts, shortcut, gtk_tree_path_get_indices (path)[0]);
    }

  /* tell everybody that we have a new shortcut */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);

  if (sorted_path)
    gtk_tree_path_free (sorted_path);
}



static void
thunar_shortcuts_model_add_shortcut (ThunarShortcutsModel *model,
                                     ThunarShortcut       *shortcut)
{
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (shortcut->file == NULL || THUNAR_IS_FILE (shortcut->file));

  thunar_shortcuts_model_add_shortcut_with_path (model, shortcut, NULL);
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



static gboolean
thunar_shortcuts_model_load (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  ThunarShortcut       *shortcut;
  ThunarFile           *file;
  GFile                *file_path;
  GFile                *home;
  gchar                *bookmarks_path;
  gchar                 line[2048];
  gchar                *name;
  FILE                 *fp;
  gint                  sort_id;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  home = thunar_g_file_new_for_home ();

  /* determine the path to the GTK+ bookmarks file */
  bookmarks_path = xfce_get_homefile (".gtk-bookmarks", NULL);

  /* append the GTK+ bookmarks (if any) */
  fp = fopen (bookmarks_path, "r");
  if (G_LIKELY (fp != NULL))
    {
      sort_id = 0;

      GDK_THREADS_ENTER ();

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

          /* handle local and remove files differently */
          if (g_file_has_uri_scheme (file_path, "file"))
            {
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
                  shortcut->group = THUNAR_SHORTCUT_GROUP_BOOKMARKS;
                  shortcut->file = file;
                  shortcut->sort_id = ++sort_id;
                  shortcut->name = (*name != '\0') ? g_strdup (name) : NULL;

                  /* append the shortcut to the list */
                  thunar_shortcuts_model_add_shortcut (model, shortcut);
                }
              else
                {
                  g_object_unref (file);
                }
            }
          else
            {
              /* create the shortcut entry */
              shortcut = g_slice_new0 (ThunarShortcut);
              shortcut->group = THUNAR_SHORTCUT_GROUP_BOOKMARKS;
              shortcut->gicon = g_themed_icon_new ("folder-remote");
              shortcut->location = file_path;
              shortcut->sort_id = ++sort_id;
              shortcut->name = (*name != '\0') ? g_strdup (name) : NULL;

              /* append the shortcut to the list */
              thunar_shortcuts_model_add_shortcut (model, shortcut);
            }
        }

      GDK_THREADS_LEAVE ();

      /* clean up */
      fclose (fp);
    }

  /* clean up */
  g_object_unref (home);
  g_free (bookmarks_path);

  model->load_idle_id = 0;

  return FALSE;
}



static gboolean
thunar_shortcuts_model_reload (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  ThunarShortcut       *shortcut;
  GtkTreePath          *path;
  GList                *lp;
  gint                  idx;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  GDK_THREADS_ENTER ();

  /* drop all existing user-defined shortcuts from the model */
  for (idx = 0, lp = model->shortcuts; lp != NULL; )
    {
      /* grab the shortcut */
      shortcut = THUNAR_SHORTCUT (lp->data);

      /* advance to the next list item */
      lp = g_list_next (lp);

      /* drop the shortcut if it is user-defined */
      if (shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS)
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

  GDK_THREADS_LEAVE ();

  /* load new bookmarks */
  return thunar_shortcuts_model_load (data);
}



static void
thunar_shortcuts_model_monitor (GFileMonitor     *monitor,
                                GFile            *file,
                                GFile            *other_file,
                                GFileMonitorEvent event_type,
                                gpointer          user_data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (user_data);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (model->monitor == monitor);

  /* reload the shortcuts model */
  if (model->load_idle_id == 0)
    model->load_idle_id = g_idle_add (thunar_shortcuts_model_reload, model);
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
      if (shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS)
        {
          if (shortcut->file != NULL)
            uri = thunar_file_dup_uri (shortcut->file);
          else if (shortcut->location != NULL)
            uri = g_file_get_uri (shortcut->location);
          else
            continue;

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
thunar_shortcuts_model_device_added (ThunarDeviceMonitor  *device_monitor,
                                     ThunarDevice         *device,
                                     ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;

  _thunar_return_if_fail (THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* allocate a new shortcut */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->device = g_object_ref (device);

  switch (thunar_device_get_kind (device))
    {
    case THUNAR_DEVICE_KIND_VOLUME:
      shortcut->group = THUNAR_SHORTCUT_GROUP_VOLUMES;
      break;

    case THUNAR_DEVICE_KIND_MOUNT_LOCAL:
      shortcut->group = THUNAR_SHORTCUT_GROUP_MOUNTS;
      break;

    case THUNAR_DEVICE_KIND_MOUNT_REMOTE:
      shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK_MOUNTS;
      break;
    }

  /* insert in the model */
  thunar_shortcuts_model_add_shortcut (model, shortcut);
}



static void
thunar_shortcuts_model_device_removed (ThunarDeviceMonitor  *device_monitor,
                                       ThunarDevice         *device,
                                       ThunarShortcutsModel *model)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* find the shortcut */
  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  /* something is broken if we don't have a shortcut here */
  _thunar_assert (lp != NULL);
  _thunar_assert (THUNAR_SHORTCUT (lp->data)->device == device);

  /* drop the shortcut from the model */
  if (G_LIKELY (lp != NULL))
    thunar_shortcuts_model_remove_shortcut (model, lp->data);
}



static void
thunar_shortcuts_model_device_changed (ThunarDeviceMonitor  *device_monitor,
                                       ThunarDevice         *device,
                                       ThunarShortcutsModel *model)
{
  GtkTreeIter  iter;
  GList       *lp;
  gint         idx;
  GtkTreePath *path;

  _thunar_return_if_fail (THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

    /* find the shortcut */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  /* something is broken if we don't have a shortcut here */
  _thunar_assert (lp != NULL);
  _thunar_assert (THUNAR_SHORTCUT (lp->data)->device == device);

  if (G_LIKELY (lp != NULL))
    {
      /* generate an iterator for the path */
      GTK_TREE_ITER_INIT (iter, model->stamp, lp);

      /* tell the view that the volume has changed in some way */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);
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

  if (G_LIKELY (shortcut->device != NULL))
    g_object_unref (shortcut->device);

  if (G_LIKELY (shortcut->gicon != NULL))
    g_object_unref (shortcut->gicon);

  if (G_LIKELY (shortcut->location != NULL))
    g_object_unref (shortcut->location);

  /* release the shortcut name */
  g_free (shortcut->name);

  /* release the shortcut itself */
  g_slice_free (ThunarShortcut, shortcut);
}



static gboolean
thunar_shortcuts_model_busy_timeout (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  gboolean              keep_running = FALSE;
  ThunarShortcut       *shortcut;
  GtkTreePath          *path;
  guint                 idx;
  GtkTreeIter           iter;
  GList                *lp;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (data), FALSE);

  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    {
      shortcut = lp->data;
      if (!shortcut->busy)
        continue;

      /* loop the pulse of the shortcut */
      shortcut->busy_pule++;
      if (shortcut->busy_pule >= SPINNER_NUM_STEPS)
        shortcut->busy_pule = 0;

      /* generate an iterator for the path */
      GTK_TREE_ITER_INIT (iter, model->stamp, lp);

      /* notify the views about the change */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);

      /* keep the timeout running */
      keep_running = TRUE;
    }

  return keep_running;
}



static void
thunar_shortcuts_model_busy_timeout_destroyed (gpointer data)
{
  THUNAR_SHORTCUTS_MODEL (data)->busy_timeout_id = 0;
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
 * @file  : a #ThuanrFile instance.
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
  GFile          *mount_point;
  GList          *lp;
  ThunarShortcut *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (iter != NULL, FALSE);

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = lp->data;

      /* check if we have a location that matches */
      if (shortcut->file == file)
        {
          GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
          return TRUE;
        }

      /* but maybe we have a mounted(!) volume with a matching mount point */
      if (shortcut->device != NULL)
        {
          mount_point = thunar_device_get_root (shortcut->device);
          if (mount_point != NULL)
            {
              if (g_file_equal (mount_point, thunar_file_get_file (file)))
                {
                  GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
                  g_object_unref (mount_point);
                  return TRUE;
                }

              g_object_unref (mount_point);
            }
        }
    }

  return FALSE;
}



/**
 * thunar_shortcuts_model_set_file:
 * @location : a #GFile.
 * @file  : a #ThunarFile.
 *
 * Set the ThunarFile for the activated GFile in the database.
 **/
void
thunar_shortcuts_model_set_file (ThunarShortcutsModel *model,
                                 GFile                *location,
                                 ThunarFile           *file)
{
  GList          *lp;
  ThunarShortcut *shortcut;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (G_IS_FILE (location));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = lp->data;

      /* check if we have a location that matches */
      if (shortcut->location != NULL
          && shortcut->file == NULL
          && g_file_equal (shortcut->location, location))
        {
          shortcut->file = g_object_ref (file);

          /* watch the file for changes */
          thunar_file_watch (shortcut->file);

          /* connect appropriate signals */
          g_signal_connect (G_OBJECT (shortcut->file), "changed",
                            G_CALLBACK (thunar_shortcuts_model_file_changed), model);
          g_signal_connect (G_OBJECT (shortcut->file), "destroy",
                            G_CALLBACK (thunar_shortcuts_model_file_destroy), model);
        }
    }
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

  /* append to the list is not possible */
  if (G_LIKELY (shortcut == NULL))
    return FALSE;

  /* cannot drop before special shortcuts! */
  if (shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS)
    return TRUE;

  /* we can drop at the end of the bookmarks (before network header) */
  if (shortcut->group == THUNAR_SHORTCUT_GROUP_NETWORK && shortcut->is_header)
    return TRUE;

  return FALSE;
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
  shortcut->group = THUNAR_SHORTCUT_GROUP_BOOKMARKS;
  shortcut->file = g_object_ref (G_OBJECT (file));

  /* add the shortcut to the list at the given position */
  thunar_shortcuts_model_add_shortcut_with_path (model, shortcut, dst_path);

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
          if (lp->next == NULL)
            break;
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
  _thunar_assert (shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS);

  /* remove the shortcut (using the file destroy handler) */
  thunar_shortcuts_model_remove_shortcut (model, shortcut);
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
  _thunar_assert (shortcut->group == THUNAR_SHORTCUT_GROUP_BOOKMARKS);
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



void
thunar_shortcuts_model_set_busy (ThunarShortcutsModel *model,
                                 ThunarDevice         *device,
                                 gboolean              busy)
{
  ThunarShortcut *shortcut;
  GList          *lp;
  guint           idx;
  GtkTreeIter     iter;
  GtkTreePath    *path;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));

  /* get the device */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  if (lp == NULL)
    return;

  shortcut = lp->data;
  _thunar_assert (shortcut->device == device);

  if (G_LIKELY (shortcut->busy != busy))
    {
      shortcut->busy = busy;

      if (busy && model->busy_timeout_id == 0)
        {
          /* start the global cycle timeout */
          model->busy_timeout_id =
            gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                          SPINNER_CYCLE_DURATION / SPINNER_NUM_STEPS,
                                          thunar_shortcuts_model_busy_timeout, model,
                                          thunar_shortcuts_model_busy_timeout_destroyed);
        }
      else if (!busy)
        {
          /* generate an iterator for the path */
          GTK_TREE_ITER_INIT (iter, model->stamp, lp);

          /* notify the views about the change */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}
