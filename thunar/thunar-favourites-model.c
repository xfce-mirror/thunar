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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <thunar/thunar-favourites-model.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-icon-factory.h>



#define THUNAR_FAVOURITE(obj) ((ThunarFavourite *) (obj))



typedef struct _ThunarFavourite ThunarFavourite;

typedef enum
{
  THUNAR_FAVOURITE_SEPARATOR,
  THUNAR_FAVOURITE_SYSTEM_DEFINED,
  THUNAR_FAVOURITE_REMOVABLE_MEDIA,
  THUNAR_FAVOURITE_USER_DEFINED,
} ThunarFavouriteType;



static void               thunar_favourites_model_class_init          (ThunarFavouritesModelClass *klass);
static void               thunar_favourites_model_tree_model_init     (GtkTreeModelIface          *iface);
static void               thunar_favourites_model_drag_source_init    (GtkTreeDragSourceIface     *iface);
static void               thunar_favourites_model_init                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_finalize            (GObject                    *object);
static GtkTreeModelFlags  thunar_favourites_model_get_flags           (GtkTreeModel               *tree_model);
static gint               thunar_favourites_model_get_n_columns       (GtkTreeModel               *tree_model);
static GType              thunar_favourites_model_get_column_type     (GtkTreeModel               *tree_model,
                                                                       gint                        index);
static gboolean           thunar_favourites_model_get_iter            (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter,
                                                                       GtkTreePath                *path);
static GtkTreePath       *thunar_favourites_model_get_path            (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter);
static void               thunar_favourites_model_get_value           (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter,
                                                                       gint                        column,
                                                                       GValue                     *value);
static gboolean           thunar_favourites_model_iter_next           (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter);
static gboolean           thunar_favourites_model_iter_children       (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter,
                                                                       GtkTreeIter                *parent);
static gboolean           thunar_favourites_model_iter_has_child      (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter);
static gint               thunar_favourites_model_iter_n_children     (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter);
static gboolean           thunar_favourites_model_iter_nth_child      (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter,
                                                                       GtkTreeIter                *parent,
                                                                       gint                        n);
static gboolean           thunar_favourites_model_iter_parent         (GtkTreeModel               *tree_model,
                                                                       GtkTreeIter                *iter,
                                                                       GtkTreeIter                *child);
static gboolean           thunar_favourites_model_row_draggable       (GtkTreeDragSource          *source,
                                                                       GtkTreePath                *path);
static gboolean           thunar_favourites_model_drag_data_get       (GtkTreeDragSource          *source,
                                                                       GtkTreePath                *path,
                                                                       GtkSelectionData           *selection_data);
static gboolean           thunar_favourites_model_drag_data_delete    (GtkTreeDragSource          *source,
                                                                       GtkTreePath                *path);
static void               thunar_favourites_model_add_favourite       (ThunarFavouritesModel      *model,
                                                                       ThunarFavourite            *favourite,
                                                                       GtkTreePath                *path);
static void               thunar_favourites_model_load                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_save                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_monitor             (ThunarVfsMonitor           *monitor,
                                                                       ThunarVfsMonitorHandle     *handle,
                                                                       ThunarVfsMonitorEvent       event,
                                                                       ThunarVfsPath              *handle_path,
                                                                       ThunarVfsPath              *event_path,
                                                                       gpointer                    user_data);
static void               thunar_favourites_model_file_changed        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourites_model_file_destroy        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourites_model_volume_changed      (ThunarVfsVolume            *volume,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourite_free                       (ThunarFavourite            *favourite,
                                                                       ThunarFavouritesModel      *model);



struct _ThunarFavouritesModelClass
{
  GObjectClass __parent__;
};

struct _ThunarFavouritesModel
{
  GObject __parent__;

  guint                   stamp;
  GList                  *favourites;
  GList                  *hidden_volumes;
  ThunarVfsVolumeManager *volume_manager;

  ThunarVfsMonitor       *monitor;
  ThunarVfsMonitorHandle *handle;
};

struct _ThunarFavourite
{
  ThunarFavouriteType type;

  gchar              *name;
  ThunarFile         *file;
  ThunarVfsVolume    *volume;
};



static GObjectClass *thunar_favourites_model_parent_class;



GType
thunar_favourites_model_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarFavouritesModelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_favourites_model_class_init,
        NULL,
        NULL,
        sizeof (ThunarFavouritesModel),
        0,
        (GInstanceInitFunc) thunar_favourites_model_init,
        NULL,
      };

      static const GInterfaceInfo tree_model_info =
      {
        (GInterfaceInitFunc) thunar_favourites_model_tree_model_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo drag_source_info =
      {
        (GInterfaceInitFunc) thunar_favourites_model_drag_source_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarFavouritesModel"), &info, 0);
      g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL, &tree_model_info);
      g_type_add_interface_static (type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_source_info);
    }

  return type;
}


    
static void
thunar_favourites_model_class_init (ThunarFavouritesModelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_favourites_model_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_favourites_model_finalize;
}



static void
thunar_favourites_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_favourites_model_get_flags;
  iface->get_n_columns = thunar_favourites_model_get_n_columns;
  iface->get_column_type = thunar_favourites_model_get_column_type;
  iface->get_iter = thunar_favourites_model_get_iter;
  iface->get_path = thunar_favourites_model_get_path;
  iface->get_value = thunar_favourites_model_get_value;
  iface->iter_next = thunar_favourites_model_iter_next;
  iface->iter_children = thunar_favourites_model_iter_children;
  iface->iter_has_child = thunar_favourites_model_iter_has_child;
  iface->iter_n_children = thunar_favourites_model_iter_n_children;
  iface->iter_nth_child = thunar_favourites_model_iter_nth_child;
  iface->iter_parent = thunar_favourites_model_iter_parent;
}



static void
thunar_favourites_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = thunar_favourites_model_row_draggable;
  iface->drag_data_get = thunar_favourites_model_drag_data_get;
  iface->drag_data_delete = thunar_favourites_model_drag_data_delete;
}



static void
thunar_favourites_model_init (ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  ThunarVfsVolume *volume;
  ThunarVfsPath   *fhome;
  ThunarVfsPath   *fpath;
  GtkTreePath     *path;
  ThunarFile      *file;
  GList           *volumes;
  GList           *lp;

  model->stamp = g_random_int ();
  model->volume_manager = thunar_vfs_volume_manager_get_default ();

  /* will be used to append the favourites to the list */
  path = gtk_tree_path_new_from_indices (0, -1);

  /* append the 'Home' favourite */
  fpath = thunar_vfs_path_get_for_home ();
  file = thunar_file_get_for_path (fpath, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->type = THUNAR_FAVOURITE_SYSTEM_DEFINED;
      favourite->file = file;
      favourite->name = NULL;
      favourite->volume = NULL;

      /* append the favourite to the list */
      thunar_favourites_model_add_favourite (model, favourite, path);
      gtk_tree_path_next (path);
    }
  thunar_vfs_path_unref (fpath);

  /* append the 'Filesystem' favourite */
  fpath = thunar_vfs_path_get_for_root ();
  file = thunar_file_get_for_path (fpath, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->type = THUNAR_FAVOURITE_SYSTEM_DEFINED;
      favourite->file = file;
      favourite->name = NULL;
      favourite->volume = NULL;

      /* append the favourite to the list */
      thunar_favourites_model_add_favourite (model, favourite, path);
      gtk_tree_path_next (path);
    }
  thunar_vfs_path_unref (fpath);

  /* prepend the removable media volumes */
  volumes = thunar_vfs_volume_manager_get_volumes (model->volume_manager);
  for (lp = volumes; lp != NULL; lp = lp->next)
    {
      /* we list only removable devices here */
      volume = THUNAR_VFS_VOLUME (lp->data);
      if (thunar_vfs_volume_is_removable (volume))
        {
          /* monitor the volume for changes */
          g_object_ref (G_OBJECT (volume));
          g_signal_connect (G_OBJECT (volume), "changed",
                            G_CALLBACK (thunar_favourites_model_volume_changed), model);

          if (thunar_vfs_volume_is_present (volume))
            {
              fpath = thunar_vfs_volume_get_mount_point (volume);
              file = thunar_file_get_for_path (fpath, NULL);
              if (G_LIKELY (file != NULL))
                {
                  /* generate the favourite */
                  favourite = g_new (ThunarFavourite, 1);
                  favourite->type = THUNAR_FAVOURITE_REMOVABLE_MEDIA;
                  favourite->file = file;
                  favourite->name = NULL;
                  favourite->volume = volume;

                  /* link the favourite to the list */
                  thunar_favourites_model_add_favourite (model, favourite, path);
                  gtk_tree_path_next (path);
                  continue;
                }
            }

          /* schedule the volume for later checking, there's no medium present */
          model->hidden_volumes = g_list_prepend (model->hidden_volumes, volume);
        }
    }

  /* prepend the row separator (only supported with Gtk+ 2.6) */
#if GTK_CHECK_VERSION(2,6,0)
  favourite = g_new0 (ThunarFavourite, 1);
  favourite->type = THUNAR_FAVOURITE_SEPARATOR;
  thunar_favourites_model_add_favourite (model, favourite, path);
  gtk_tree_path_next (path);
#endif

  /* determine the URI to the Gtk+ bookmarks file */
  fhome = thunar_vfs_path_get_for_home ();
  fpath = thunar_vfs_path_relative (fhome, ".gtk-bookmarks");
  thunar_vfs_path_unref (fhome);

  /* register with the alteration monitor for the bookmarks file */
  model->monitor = thunar_vfs_monitor_get_default ();
  model->handle = thunar_vfs_monitor_add_file (model->monitor, fpath, thunar_favourites_model_monitor, G_OBJECT (model));

  /* read the Gtk+ bookmarks file */
  thunar_favourites_model_load (model);

  /* cleanup */
  thunar_vfs_path_unref (fpath);
  gtk_tree_path_free (path);
}



static void
thunar_favourites_model_finalize (GObject *object)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (object);

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* free all favourites */
  g_list_foreach (model->favourites, (GFunc) thunar_favourite_free, model);
  g_list_free (model->favourites);

  /* free all hidden volumes */
  g_list_foreach (model->hidden_volumes, (GFunc) g_object_unref, NULL);
  g_list_free (model->hidden_volumes);

  /* detach from the VFS monitor */
  thunar_vfs_monitor_remove (model->monitor, model->handle);
  g_object_unref (G_OBJECT (model->monitor));

  /* unlink from the volume manager */
  g_object_unref (G_OBJECT (model->volume_manager));

  (*G_OBJECT_CLASS (thunar_favourites_model_parent_class)->finalize) (object);
}



static GtkTreeModelFlags
thunar_favourites_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_favourites_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_FAVOURITES_MODEL_N_COLUMNS;
}



static GType
thunar_favourites_model_get_column_type (GtkTreeModel *tree_model,
                                         gint          index)
{
  switch (index)
    {
    case THUNAR_FAVOURITES_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_FAVOURITES_MODEL_COLUMN_ICON:
      return GDK_TYPE_PIXBUF;

    case THUNAR_FAVOURITES_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR:
      return G_TYPE_BOOLEAN;
    }

  g_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_favourites_model_get_iter (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreePath  *path)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);
  GList                 *lp;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  lp = g_list_nth (model->favourites, gtk_tree_path_get_indices (path)[0]);
  if (G_LIKELY (lp != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = lp;
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_favourites_model_get_path (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);
  gint                   index;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* lookup the list item in the favourites list */
  index = g_list_position (model->favourites, iter->user_data);
  if (G_LIKELY (index >= 0))
    return gtk_tree_path_new_from_indices (index, -1);

  return NULL;
}



static void
thunar_favourites_model_get_value (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   gint          column,
                                   GValue       *value)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);
  ThunarIconFactory     *icon_factory;
  ThunarFavourite       *favourite;
  GtkIconTheme          *icon_theme;
  const gchar           *icon_name;
  GdkPixbuf             *icon = NULL;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (iter->stamp == model->stamp);

  /* determine the favourite for the list item */
  favourite = THUNAR_FAVOURITE (((GList *) iter->user_data)->data);

  switch (column)
    {
    case THUNAR_FAVOURITES_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (favourite->volume != NULL))
        g_value_set_static_string (value, thunar_vfs_volume_get_name (favourite->volume));
      else if (favourite->name != NULL)
        g_value_set_static_string (value, favourite->name);
      else if (favourite->file != NULL)
        g_value_set_static_string (value, thunar_file_get_special_name (favourite->file));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_ICON:
      g_value_init (value, GDK_TYPE_PIXBUF);
      icon_factory = thunar_icon_factory_get_default ();
      icon_theme = thunar_icon_factory_get_icon_theme (icon_factory);
      if (G_UNLIKELY (favourite->volume != NULL))
        {
          icon_name = thunar_vfs_volume_lookup_icon_name (favourite->volume, icon_theme);
          icon = thunar_icon_factory_load_icon (icon_factory, icon_name, 32, NULL, TRUE);
        }
      else if (G_LIKELY (favourite->file != NULL))
        {
          icon = thunar_icon_factory_load_file_icon (icon_factory, favourite->file, THUNAR_FILE_ICON_STATE_DEFAULT, 32);
        }
      if (G_LIKELY (icon != NULL))
        g_value_take_object (value, icon);
      g_object_unref (G_OBJECT (icon_factory));
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_MUTABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, favourite->type == THUNAR_FAVOURITE_SEPARATOR);
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
thunar_favourites_model_iter_next (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter)
{
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter->stamp == THUNAR_FAVOURITES_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_list_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
thunar_favourites_model_iter_children (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL && model->favourites != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = model->favourites;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_favourites_model_iter_has_child (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_favourites_model_iter_n_children (GtkTreeModel *tree_model,
                                         GtkTreeIter  *iter)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);

  return (iter == NULL) ? g_list_length (model->favourites) : 0;
}



static gboolean
thunar_favourites_model_iter_nth_child (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *parent,
                                        gint          n)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);

  if (G_LIKELY (parent != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = g_list_nth (model->favourites, n);
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
thunar_favourites_model_iter_parent (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
thunar_favourites_model_row_draggable (GtkTreeDragSource *source,
                                       GtkTreePath       *path)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (source);
  ThunarFavourite       *favourite;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* lookup the ThunarFavourite for the path */
  favourite = g_list_nth_data (model->favourites, gtk_tree_path_get_indices (path)[0]);

  /* special favourites cannot be reordered */
  return (favourite != NULL && favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
}



static gboolean
thunar_favourites_model_drag_data_get (GtkTreeDragSource *source,
                                       GtkTreePath       *path,
                                       GtkSelectionData  *selection_data)
{
  /* we simply return FALSE here, as the drag handling is done in
   * the ThunarFavouritesView class.
   */
  return FALSE;
}



static gboolean
thunar_favourites_model_drag_data_delete (GtkTreeDragSource *source,
                                          GtkTreePath       *path)
{
  /* we simply return FALSE here, as this function can only be
   * called if the user is re-arranging favourites within the
   * model, which will be handle by the exchange method.
   */
  return FALSE;
}



static void
thunar_favourites_model_add_favourite (ThunarFavouritesModel *model,
                                       ThunarFavourite       *favourite,
                                       GtkTreePath           *path)
{
  GtkTreeIter iter;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (favourite->file == NULL || THUNAR_IS_FILE (favourite->file));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] <= g_list_length (model->favourites));

  /* we want to stay informed about changes to the file */
  if (G_LIKELY (favourite->file != NULL))
    {
      /* watch the file for changes */
      thunar_file_watch (favourite->file);

      /* connect appropriate signals */
      g_signal_connect (G_OBJECT (favourite->file), "changed",
                        G_CALLBACK (thunar_favourites_model_file_changed), model);
      g_signal_connect (G_OBJECT (favourite->file), "destroy",
                        G_CALLBACK (thunar_favourites_model_file_destroy), model);
    }

  /* insert the new favourite to the favourites list */
  model->favourites = g_list_insert (model->favourites, favourite, gtk_tree_path_get_indices (path)[0]);

  /* tell everybody that we have a new favourite */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
}



static void
thunar_favourites_model_load (ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  ThunarVfsPath   *file_path;
  GtkTreePath     *path;
  ThunarFile      *file;
  gchar           *bookmarks_path;
  gchar            line[2048];
  gchar           *name;
  FILE            *fp;

  /* determine the path to the GTK+ bookmarks file */
  bookmarks_path = xfce_get_homefile (".gtk-bookmarks", NULL);

  /* append the GTK+ bookmarks (if any) */
  fp = fopen (bookmarks_path, "r");
  if (G_LIKELY (fp != NULL))
    {
      /* allocate a tree path for appending to the model */
      path = gtk_tree_path_new_from_indices (g_list_length (model->favourites), -1);

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
          file_path = thunar_vfs_path_new (line, NULL);
          if (G_UNLIKELY (file_path == NULL))
            continue;

          /* try to open the file corresponding to the uri */
          file = thunar_file_get_for_path (file_path, NULL);
          thunar_vfs_path_unref (file_path);
          if (G_UNLIKELY (file == NULL))
            continue;

          /* make sure the file refers to a directory */
          if (G_UNLIKELY (!thunar_file_is_directory (file)))
            {
              g_object_unref (G_OBJECT (file));
              continue;
            }

          /* create the favourite entry */
          favourite = g_new (ThunarFavourite, 1);
          favourite->type = THUNAR_FAVOURITE_USER_DEFINED;
          favourite->file = file;
          favourite->volume = NULL;
          favourite->name = (*name != '\0') ? g_strdup (name) : NULL;

          /* append the favourite to the list */
          thunar_favourites_model_add_favourite (model, favourite, path);
          gtk_tree_path_next (path);
        }

      /* clean up */
      gtk_tree_path_free (path);
      fclose (fp);
    }

  /* clean up */
  g_free (bookmarks_path);
}



static void
thunar_favourites_model_monitor (ThunarVfsMonitor       *monitor,
                                 ThunarVfsMonitorHandle *handle,
                                 ThunarVfsMonitorEvent   event,
                                 ThunarVfsPath          *handle_path,
                                 ThunarVfsPath          *event_path,
                                 gpointer                user_data)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (user_data);
  ThunarFavourite       *favourite;
  GtkTreePath           *path;
  GList                 *lp;
  gint                   index;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (model->monitor == monitor);
  g_return_if_fail (model->handle == handle);

  /* drop all existing user-defined favourites from the model */
  for (index = 0, lp = model->favourites; lp != NULL; )
    {
      /* grab the favourite */
      favourite = THUNAR_FAVOURITE (lp->data);

      /* advance to the next list item */
      lp = g_list_next (lp);

      /* drop the favourite if it is user-defined */
      if (favourite->type == THUNAR_FAVOURITE_USER_DEFINED)
        {
          /* unlink the favourite from the model */
          model->favourites = g_list_remove (model->favourites, favourite);
          
          /* tell everybody that we have lost a favourite */
          path = gtk_tree_path_new_from_indices (index, -1);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
          gtk_tree_path_free (path);

          /* actually free the favourite */
          thunar_favourite_free (favourite, model);
        }
      else
        {
          ++index;
        }
    }

  /* reload the favourites model */
  thunar_favourites_model_load (model);
}



static void
thunar_favourites_model_save (ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  gchar           *bookmarks_path;
  gchar           *tmp_path;
  gchar           *uri;
  GList           *lp;
  FILE            *fp;
  gint             fd;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

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

  /* write the uris of user customizable favourites */
  fp = fdopen (fd, "w");
  for (lp = model->favourites; lp != NULL; lp = lp->next)
    {
      favourite = THUNAR_FAVOURITE (lp->data);
      if (favourite->type == THUNAR_FAVOURITE_USER_DEFINED)
        {
          uri = thunar_vfs_path_dup_uri (thunar_file_get_path (favourite->file));
          if (G_LIKELY (favourite->name != NULL))
            fprintf (fp, "%s %s\n", uri, favourite->name);
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
      unlink (tmp_path);
    }

  /* cleanup */
  g_free (bookmarks_path);
  g_free (tmp_path);
}



static void
thunar_favourites_model_file_changed (ThunarFile            *file,
                                      ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  GList           *lp;
  gint             index;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
 
  /* check if the file still refers to a directory, else we cannot keep
   * it on the favourites list, and so we'll treat it like the file
   * was destroyed (and thereby removed)
   */
  if (G_UNLIKELY (!thunar_file_is_directory (file)))
    {
      thunar_favourites_model_file_destroy (file, model);
      return;
    }

  for (index = 0, lp = model->favourites; lp != NULL; ++index, lp = lp->next)
    {
      favourite = THUNAR_FAVOURITE (lp->data);
      if (favourite->file == file)
        {
          iter.stamp = model->stamp;
          iter.user_data = lp;

          path = gtk_tree_path_new_from_indices (index, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
          break;
        }
    }
}



static void
thunar_favourites_model_file_destroy (ThunarFile            *file,
                                      ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite = NULL;
  GtkTreePath     *path;
  GList           *lp;
  gint             index;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* lookup the favourite matching the file */
  for (index = 0, lp = model->favourites; lp != NULL; ++index, lp = lp->next)
    {
      favourite = THUNAR_FAVOURITE (lp->data);
      if (favourite->file == file)
        break;
    }

  /* verify that we actually found a favourite */
  g_assert (lp != NULL);
  g_assert (THUNAR_IS_FILE (favourite->file));

  /* unlink the favourite from the model */
  model->favourites = g_list_delete_link (model->favourites, lp);

  /* tell everybody that we have lost a favourite */
  path = gtk_tree_path_new_from_indices (index, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);

  /* actually free the favourite */
  thunar_favourite_free (favourite, model);

  /* the favourites list was changed, so write the gtk bookmarks file */
  thunar_favourites_model_save (model);
}



static void
thunar_favourites_model_volume_changed (ThunarVfsVolume       *volume,
                                        ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite = NULL;
  ThunarVfsPath   *fpath;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  ThunarFile      *file;
  GList           *lp;
  gint             index;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* check if the volume is on the hidden list */
  lp = g_list_find (model->hidden_volumes, volume);
  if (lp != NULL)
    {
      /* check if we need to display the volume now */
      if (thunar_vfs_volume_is_present (volume))
        {
          fpath = thunar_vfs_volume_get_mount_point (volume);
          file = thunar_file_get_for_path (fpath, NULL);
          if (G_LIKELY (file != NULL))
            {
              /* remove the volume from the list of hidden volumes */
              model->hidden_volumes = g_list_delete_link (model->hidden_volumes, lp);

              /* find the insert position */
              for (index = 0, lp = model->favourites; lp != NULL; ++index, lp = lp->next)
                {
                  favourite = THUNAR_FAVOURITE (lp->data);
                  if (favourite->type == THUNAR_FAVOURITE_SEPARATOR
                      || favourite->type == THUNAR_FAVOURITE_USER_DEFINED)
                    break;
                }

              /* allocate a new favourite */
              favourite = g_new (ThunarFavourite, 1);
              favourite->type = THUNAR_FAVOURITE_REMOVABLE_MEDIA;
              favourite->file = file;
              favourite->name = NULL;
              favourite->volume = volume;

              /* the volume is present now, so we have to display it */
              path = gtk_tree_path_new_from_indices (index, -1);
              thunar_favourites_model_add_favourite (model, favourite, path);
              gtk_tree_path_free (path);
            }
        }
    }
  else
    {
      /* lookup the favourite that contains the given volume */
      for (index = 0, lp = model->favourites; lp != NULL; ++index, lp = lp->next)
        {
          favourite = THUNAR_FAVOURITE (lp->data);
          if (favourite->volume == volume)
            break;
        }

      /* verify that we actually found the favourite */
      g_assert (favourite != NULL);
      g_assert (favourite->volume == volume);

      /* check if we need to hide the volume now */
      if (!thunar_vfs_volume_is_present (volume))
        {
          /* need to ref here, because the file_destroy() handler will drop the refcount. */
          g_object_ref (G_OBJECT (volume));

          /* move the volume to the hidden list */
          model->hidden_volumes = g_list_prepend (model->hidden_volumes, volume);

          /* we misuse the file_destroy handler to get rid of the volume entry */
          thunar_favourites_model_file_destroy (favourite->file, model);

          /* need to reconnect to the volume, as the file_destroy removes the handler */
          g_signal_connect (G_OBJECT (volume), "changed",
                            G_CALLBACK (thunar_favourites_model_volume_changed), model);
        }
      else
        {
          /* tell the view that the volume has changed in some way */
          iter.stamp = model->stamp;
          iter.user_data = lp;

          path = gtk_tree_path_new_from_indices (index, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}



static void
thunar_favourite_free (ThunarFavourite       *favourite,
                       ThunarFavouritesModel *model)
{
  if (G_LIKELY (favourite->file != NULL))
    {
      /* drop the file watch */
      thunar_file_unwatch (favourite->file);

      /* unregister from the file */
      g_signal_handlers_disconnect_matched (G_OBJECT (favourite->file),
                                            G_SIGNAL_MATCH_DATA, 0,
                                            0, NULL, NULL, model);
      g_object_unref (G_OBJECT (favourite->file));
    }

  if (G_LIKELY (favourite->volume != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (favourite->volume),
                                            G_SIGNAL_MATCH_DATA, 0,
                                            0, NULL, NULL, model);
      g_object_unref (G_OBJECT (favourite->volume));
    }

  g_free (favourite->name);
  g_free (favourite);
}



/**
 * thunar_favourites_model_get_default:
 *
 * Returns the default #ThunarFavouritesModel instance shared by
 * all #ThunarFavouritesView instances.
 *
 * Call #g_object_unref() on the returned object when you
 * don't need it any longer.
 *
 * Return value: the default #ThunarFavouritesModel instance.
 **/
ThunarFavouritesModel*
thunar_favourites_model_get_default (void)
{
  static ThunarFavouritesModel *model = NULL;

  if (G_UNLIKELY (model == NULL))
    {
      model = g_object_new (THUNAR_TYPE_FAVOURITES_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);
    }
  else
    {
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



/**
 * thunar_favourites_model_iter_for_file:
 * @model : a #ThunarFavouritesModel instance.
 * @file  : a #ThunarFile instance.
 * @iter  : pointer to a #GtkTreeIter.
 *
 * Tries to lookup the #GtkTreeIter, that belongs to a favourite, which
 * refers to @file and stores it to @iter. If no such #GtkTreeIter was
 * found, %FALSE will be returned and @iter won't be changed. Else
 * %TRUE will be returned and @iter will be set appropriately.
 *
 * Return value: %TRUE if @file was found, else %FALSE.
 **/
gboolean
thunar_favourites_model_iter_for_file (ThunarFavouritesModel *model,
                                       ThunarFile            *file,
                                       GtkTreeIter           *iter)
{
  GList *lp;
  
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  for (lp = model->favourites; lp != NULL; lp = lp->next)
    if (THUNAR_FAVOURITE (lp->data)->file == file)
      {
        iter->stamp = model->stamp;
        iter->user_data = lp;
        return TRUE;
      }

  return FALSE;
}



/**
 * thunar_favourites_model_file_for_iter:
 * @model : a #ThunarFavouritesModel instance.
 * @iter  : pointer to a valid #GtkTreeIter.
 *
 * Return value: the #ThunarFile matching the given @iter.
 **/
ThunarFile*
thunar_favourites_model_file_for_iter (ThunarFavouritesModel *model,
                                       GtkTreeIter           *iter)
{
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), NULL);
  g_return_val_if_fail (iter != NULL && iter->stamp == model->stamp, NULL);
  return THUNAR_FAVOURITE (((GList *) iter->user_data)->data)->file;
}



/**
 * thunar_favourites_model_drop_possible:
 * @model : a #ThunarFavouritestModel.
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
thunar_favourites_model_drop_possible (ThunarFavouritesModel *model,
                                       GtkTreePath           *path)
{
  ThunarFavourite *favourite;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  favourite = g_list_nth_data (model->favourites, gtk_tree_path_get_indices (path)[0]);

  /* append to the list is always possible */
  if (G_LIKELY (favourite == NULL))
    return TRUE;

  /* cannot drop before special favourites! */
  return (favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
}



/**
 * thunar_favourites_model_add:
 * @model    : a #ThunarFavouritesModel.
 * @dst_path : the destination path.
 * @file     : the #ThunarFile that should be added to the favourites list.
 *
 * Adds the favourite @file to the @model at @dst_path, unless @file is
 * already present in @model in which case no action is performed.
 **/
void
thunar_favourites_model_add (ThunarFavouritesModel *model,
                             GtkTreePath           *dst_path,
                             ThunarFile            *file)
{
  ThunarFavourite *favourite;
  GList           *lp;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] <= g_list_length (model->favourites));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* verify that the file is not already in use as favourite */
  for (lp = model->favourites; lp != NULL; lp = lp->next)
    if (THUNAR_FAVOURITE (lp->data)->file == file)
      return;

  /* create the new favourite that will be inserted */
  favourite = g_new0 (ThunarFavourite, 1);
  favourite->type = THUNAR_FAVOURITE_USER_DEFINED;
  favourite->file = g_object_ref (G_OBJECT (file));

  /* add the favourite to the list at the given position */
  thunar_favourites_model_add_favourite (model, favourite, dst_path);

  /* the favourites list was changed, so write the gtk bookmarks file */
  thunar_favourites_model_save (model);
}



/**
 * thunar_favourites_model_move:
 * @model    : a #ThunarFavouritesModel.
 * @src_path : the source path.
 * @dst_path : the destination path.
 *
 * Moves the favourite at @src_path to @dst_path, adjusting other
 * favourite's positions as required.
 **/
void
thunar_favourites_model_move (ThunarFavouritesModel *model,
                              GtkTreePath           *src_path,
                              GtkTreePath           *dst_path)
{
  ThunarFavourite *favourite;
  GtkTreePath     *path;
  GList           *lp;
  gint            *order;
  gint             index_src;
  gint             index_dst;
  gint             index;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (src_path) > 0);
  g_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (src_path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (src_path)[0] < g_list_length (model->favourites));
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] > 0);

  index_src = gtk_tree_path_get_indices (src_path)[0];
  index_dst = gtk_tree_path_get_indices (dst_path)[0];

  if (G_UNLIKELY (index_src == index_dst))
    return;

  /* generate the order for the rows prior the dst/src rows */
  order = g_newa (gint, g_list_length (model->favourites));
  for (index = 0, lp = model->favourites; index < index_src && index < index_dst; ++index, lp = lp->next)
    order[index] = index;

  if (index == index_src)
    {
      favourite = THUNAR_FAVOURITE (lp->data);

      for (; index < index_dst; ++index, lp = lp->next)
        {
          lp->data = lp->next->data;
          order[index] = index + 1;
        }

      lp->data = favourite;
      order[index++] = index_src;
    }
  else
    {
      for (; index < index_src; ++index, lp = lp->next)
        ;

      g_assert (index == index_src);

      favourite = THUNAR_FAVOURITE (lp->data);

      for (; index > index_dst; --index, lp = lp->prev)
        {
          lp->data = lp->prev->data;
          order[index] = index - 1;
        }

      g_assert (index == index_dst);

      lp->data = favourite;
      order[index] = index_src;
      index = index_src + 1;
    }

  /* generate the remaining order */
  for (; index < g_list_length (model->favourites); ++index)
    order[index] = index;

  /* tell all listeners about the reordering just performed */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, order);
  gtk_tree_path_free (path);

  /* the favourites list was changed, so write the gtk bookmarks file */
  thunar_favourites_model_save (model);
}



/**
 * thunar_favourites_model_remove:
 * @model : a #ThunarFavouritesModel.
 * @path  : the #GtkTreePath of the favourite to remove.
 *
 * Removes the favourite at @path from the @model and syncs to
 * on-disk storage. @path must refer to a valid, user-defined
 * favourite, as you cannot remove system-defined entities (they
 * are managed internally).
 **/
void
thunar_favourites_model_remove (ThunarFavouritesModel *model,
                                GtkTreePath           *path)
{
  ThunarFavourite *favourite;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] < g_list_length (model->favourites));

  /* lookup the favourite for the given path */
  favourite = g_list_nth_data (model->favourites, gtk_tree_path_get_indices (path)[0]);

  /* verify that the favourite is removable */
  g_assert (favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
  g_assert (THUNAR_IS_FILE (favourite->file));

  /* remove the favourite (using the file destroy handler) */
  thunar_favourites_model_file_destroy (favourite->file, model);
}



/**
 * thunar_favourites_model_rename:
 * @model : a #ThunarFavouritesModel.
 * @path  : the #GtkTreePath which refers to the favourite that
 *          should be renamed to @name.
 * @name  : the new name for the favourite at @path or %NULL to
 *          return to the default name.
 *
 * Renames the favourite at @path to the new @name in @model.
 *
 * @name may be %NULL or an empty to reset the favourite to
 * its default name.
 **/
void
thunar_favourites_model_rename (ThunarFavouritesModel *model,
                                GtkTreePath           *path,
                                const gchar           *name)
{
  ThunarFavourite *favourite;
  GtkTreeIter      iter;
  GList           *lp;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] < g_list_length (model->favourites));
  g_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));

  /* lookup the favourite for the given path */
  lp = g_list_nth (model->favourites, gtk_tree_path_get_indices (path)[0]);
  favourite = THUNAR_FAVOURITE (lp->data);

  /* verify the favourite */
  g_assert (favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
  g_assert (THUNAR_IS_FILE (favourite->file));

  /* perform the rename */
  if (G_UNLIKELY (favourite->name != NULL))
    g_free (favourite->name);
  if (G_UNLIKELY (name == NULL || *name == '\0'))
    favourite->name = NULL;
  else
    favourite->name = g_strdup (name);

  /* notify the views about the change */
  iter.stamp = model->stamp;
  iter.user_data = lp;
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);

  /* save the changes to the model */
  thunar_favourites_model_save (model);
}




