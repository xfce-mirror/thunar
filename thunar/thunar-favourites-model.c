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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar/thunar-favourites-model.h>
#include <thunar/thunar-file.h>



typedef struct _ThunarFavourite ThunarFavourite;

typedef enum
{
  THUNAR_FAVOURITE_SEPARATOR,
  THUNAR_FAVOURITE_SYSTEM_DEFINED,
  THUNAR_FAVOURITE_REMOVABLE_MEDIA,
  THUNAR_FAVOURITE_USER_DEFINED,
} ThunarFavouriteType;



static void               thunar_favourites_model_class_init          (ThunarFavouritesModelClass *klass);
static void               thunar_favourites_model_init                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_tree_model_init     (GtkTreeModelIface          *iface);
static void               thunar_favourites_model_drag_source_init    (GtkTreeDragSourceIface     *iface);
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
static void               thunar_favourites_model_save                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_file_changed        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourites_model_file_destroy        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourites_model_volume_changed      (ThunarVfsVolume            *volume,
                                                                       ThunarFavouritesModel      *model);



struct _ThunarFavouritesModelClass
{
  GObjectClass __parent__;
};

struct _ThunarFavouritesModel
{
  GObject __parent__;

  guint                   stamp;
  gint                    n_favourites;
  GList                  *hidden_volumes;
  ThunarFavourite        *favourites;
  ThunarVfsVolumeManager *volume_manager;
};

struct _ThunarFavourite
{
  ThunarFavouriteType type;

  ThunarFile         *file;
  ThunarVfsVolume    *volume;

  ThunarFavourite    *next;
  ThunarFavourite    *prev;
};



G_DEFINE_TYPE_WITH_CODE (ThunarFavouritesModel,
                         thunar_favourites_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                thunar_favourites_model_tree_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                thunar_favourites_model_drag_source_init));


    
static void
thunar_favourites_model_class_init (ThunarFavouritesModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_favourites_model_finalize;
}



static void
thunar_favourites_model_init (ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  ThunarVfsVolume *volume;
  ThunarVfsURI    *uri;
  GtkTreePath     *path;
  ThunarFile      *file;
  GList           *volumes;
  GList           *lp;
  gchar           *bookmarks_path;
  gchar            line[1024];
  FILE            *fp;

  model->stamp = g_random_int ();
  model->n_favourites = 0;
  model->favourites = NULL;
  model->volume_manager = thunar_vfs_volume_manager_get_default ();

  /* will be used to append the favourites to the list */
  path = gtk_tree_path_new_from_indices (0, -1);

  /* append the 'Home' favourite */
  uri = thunar_vfs_uri_new_for_path (xfce_get_homedir ());
  file = thunar_file_get_for_uri (uri, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->type = THUNAR_FAVOURITE_SYSTEM_DEFINED;
      favourite->file = file;
      favourite->volume = NULL;

      /* append the favourite to the list */
      thunar_favourites_model_add_favourite (model, favourite, path);
      gtk_tree_path_next (path);
    }
  thunar_vfs_uri_unref (uri);

  /* append the 'Trash' favourite */
  uri = thunar_vfs_uri_new ("trash:", NULL);
  file = thunar_file_get_for_uri (uri, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->type = THUNAR_FAVOURITE_SYSTEM_DEFINED;
      favourite->file = file;
      favourite->volume = NULL;

      /* append the favourite to the list */
      thunar_favourites_model_add_favourite (model, favourite, path);
      gtk_tree_path_next (path);
    }
  thunar_vfs_uri_unref (uri);

  /* append the 'Filesystem' favourite */
  uri = thunar_vfs_uri_new_for_path ("/");
  file = thunar_file_get_for_uri (uri, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->type = THUNAR_FAVOURITE_SYSTEM_DEFINED;
      favourite->file = file;
      favourite->volume = NULL;

      /* append the favourite to the list */
      thunar_favourites_model_add_favourite (model, favourite, path);
      gtk_tree_path_next (path);
    }
  thunar_vfs_uri_unref (uri);

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
              uri = thunar_vfs_volume_get_mount_point (volume);
              file = thunar_file_get_for_uri (uri, NULL);
              if (G_LIKELY (file != NULL))
                {
                  /* generate the favourite */
                  favourite = g_new (ThunarFavourite, 1);
                  favourite->type = THUNAR_FAVOURITE_REMOVABLE_MEDIA;
                  favourite->file = file;
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

  /* append the GTK+ bookmarks (if any) */
  bookmarks_path = xfce_get_homefile (".gtk-bookmarks", NULL);
  fp = fopen (bookmarks_path, "r");
  if (G_LIKELY (fp != NULL))
    {
      while (fgets (line, sizeof (line), fp) != NULL)
        {
          /* strip leading/trailing whitespace */
          g_strstrip (line);

          uri = thunar_vfs_uri_new (line, NULL);
          if (G_UNLIKELY (uri == NULL))
            continue;

          /* try to open the file corresponding to the uri */
          file = thunar_file_get_for_uri (uri, NULL);
          thunar_vfs_uri_unref (uri);
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

          /* append the favourite to the list */
          thunar_favourites_model_add_favourite (model, favourite, path);
          gtk_tree_path_next (path);
        }

      fclose (fp);
    }
  g_free (bookmarks_path);

  /* cleanup */
  gtk_tree_path_free (path);
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
thunar_favourites_model_finalize (GObject *object)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (object);
  ThunarFavourite       *current;
  ThunarFavourite       *next;
  GList                 *lp;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* free all favourites */
  for (current = model->favourites; current != NULL; current = next)
    {
      next = current->next;

      if (G_LIKELY (current->file != NULL))
        {
          /* drop the file watch */
          thunar_file_unwatch (current->file);

          /* unregister from the file */
          g_signal_handlers_disconnect_matched (G_OBJECT (current->file),
                                                G_SIGNAL_MATCH_DATA, 0,
                                                0, NULL, NULL, model);
          g_object_unref (G_OBJECT (current->file));
        }

      if (G_LIKELY (current->volume != NULL))
        {
          g_signal_handlers_disconnect_matched (G_OBJECT (current->volume),
                                                G_SIGNAL_MATCH_DATA, 0,
                                                0, NULL, NULL, model);
          g_object_unref (G_OBJECT (current->volume));
        }

      g_free (current);
    }

  /* free all hidden volumes */
  for (lp = model->hidden_volumes; lp != NULL; lp = lp->next)
    g_object_unref (G_OBJECT (lp->data));
  g_list_free (model->hidden_volumes);

  /* unlink from the volume manager */
  g_object_unref (G_OBJECT (model->volume_manager));

  G_OBJECT_CLASS (thunar_favourites_model_parent_class)->finalize (object);
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
  ThunarFavourite       *favourite;
  gint                   index;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  index = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (index >= model->n_favourites))
    return FALSE;

  for (favourite = model->favourites; index-- > 0; favourite = favourite->next)
    g_assert (favourite != NULL);

  iter->stamp = model->stamp;
  iter->user_data = favourite;

  return TRUE;
}



static GtkTreePath*
thunar_favourites_model_get_path (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);
  ThunarFavourite       *favourite;
  GtkTreePath           *path;
  gint                   index = 0;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), NULL);
  g_return_val_if_fail (iter->stamp == model->stamp, NULL);

  for (favourite = model->favourites; favourite != NULL; favourite = favourite->next, ++index)
    if (favourite == iter->user_data)
      break;

  if (G_UNLIKELY (favourite == NULL))
    return NULL;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, index);
  return path;
}



static void
thunar_favourites_model_get_value (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   gint          column,
                                   GValue       *value)
{
  ThunarFavourite *favourite;
  GtkIconTheme    *icon_theme;
  GtkIconInfo     *icon_info;
  GdkPixbuf       *icon;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (tree_model));
  g_return_if_fail (iter->stamp == THUNAR_FAVOURITES_MODEL (tree_model)->stamp);

  favourite = iter->user_data;

  switch (column)
    {
    case THUNAR_FAVOURITES_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (favourite->volume != NULL))
        g_value_set_static_string (value, thunar_vfs_volume_get_name (favourite->volume));
      else if (favourite->file != NULL)
        g_value_set_static_string (value, thunar_file_get_special_name (favourite->file));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_ICON:
      g_value_init (value, GDK_TYPE_PIXBUF);
      if (G_UNLIKELY (favourite->volume != NULL))
        {
          icon_theme = gtk_icon_theme_get_default ();
          icon_info = thunar_vfs_volume_lookup_icon (favourite->volume, icon_theme, 32, 0);
          if (G_LIKELY (icon_info != NULL))
            {
              icon = gtk_icon_info_load_icon (icon_info, NULL);
              if (G_LIKELY (icon != NULL))
                g_value_take_object (value, icon);
              gtk_icon_info_free (icon_info);
            }
        }
      else if (G_LIKELY (favourite->file != NULL))
        {
          icon = thunar_file_load_icon (favourite->file, 32);
          if (G_LIKELY (icon != NULL))
            g_value_take_object (value, icon);
        }
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

  iter->user_data = ((ThunarFavourite *) iter->user_data)->next;
  return (iter->user_data != NULL);
}



static gboolean
thunar_favourites_model_iter_children (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);

  if (G_UNLIKELY (parent != NULL))
    return FALSE;

  if (G_LIKELY (model->favourites != NULL))
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

  return (iter == NULL) ? model->n_favourites : 0;
}



static gboolean
thunar_favourites_model_iter_nth_child (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *parent,
                                        gint          n)
{
  ThunarFavouritesModel *model = THUNAR_FAVOURITES_MODEL (tree_model);
  ThunarFavourite       *favourite;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);

  if (G_UNLIKELY (parent != NULL))
    return FALSE;

  for (favourite = model->favourites; favourite != NULL && n-- > 0; favourite = favourite->next)
    ;

  if (G_LIKELY (favourite != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = favourite;
      return TRUE;
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
  gint                   n;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* verify that the index of the path is valid */
  n = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (n < 0 || n >= model->n_favourites))
    return FALSE;

  /* lookup the ThunarFavourite for the path */
  for (favourite = model->favourites; --n >= 0; favourite = favourite->next)
    ;

  /* special favourites cannot be reordered */
  return (favourite->type == THUNAR_FAVOURITE_USER_DEFINED);
}



static gboolean
thunar_favourites_model_drag_data_get (GtkTreeDragSource *source,
                                       GtkTreePath       *path,
                                       GtkSelectionData  *selection_data)
{
  // FIXME
  return FALSE;
}



static gboolean
thunar_favourites_model_drag_data_delete (GtkTreeDragSource *source,
                                          GtkTreePath       *path)
{
  // we simply return FALSE here, as this function can only be
  // called if the user is re-arranging favourites within the
  // model, which will be handle by the exchange method
  return FALSE;
}



static void
thunar_favourites_model_add_favourite (ThunarFavouritesModel *model,
                                       ThunarFavourite       *favourite,
                                       GtkTreePath           *path)
{
  ThunarFavourite *current;
  GtkTreeIter      iter;
  gint             index;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (favourite->file == NULL || THUNAR_IS_FILE (favourite->file));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (path)[0] <= model->n_favourites);

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

  /* check if this is the first favourite to insert (shouldn't happen normally) */
  if (G_UNLIKELY (model->favourites == NULL))
    {
      model->favourites = favourite;
      favourite->next = NULL;
      favourite->prev = NULL;
    }
  else
    {
      index = gtk_tree_path_get_indices (path)[0];
      if (index == 0)
        {
          /* the new favourite should be prepended to the existing list */
          favourite->next = model->favourites;
          favourite->prev = NULL;
          model->favourites->prev = favourite;
          model->favourites = favourite;
        }
      else if (index == model->n_favourites)
        {
          /* the new favourite should be appended to the existing list */
          for (current = model->favourites; current->next != NULL; current = current->next)
            ;

          favourite->next = NULL;
          favourite->prev = current;
          current->next = favourite;
        }
      else
        {
          /* inserting somewhere into the existing list */
          for (current = model->favourites; index-- > 0; current = current->next)
            ;

          favourite->next = current;
          favourite->prev = current->prev;
          current->prev->next = favourite;
          current->prev = favourite;
        }
    }

  /* we've just inserted a new item */
  ++model->n_favourites;

  /* tell everybody that we have a new favourite */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
}



static void
thunar_favourites_model_save (ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  gchar           *dst_path;
  gchar           *tmp_path;
  gchar           *uri;
  FILE            *fp = NULL;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* open a temporary file for writing */
  tmp_path = g_strdup_printf ("%s%c.gtk-bookmarks.tmp-%d",
                              xfce_get_homedir (),
                              G_DIR_SEPARATOR,
                              (gint) getpid ());
  fp = fopen (tmp_path, "w");
  if (G_UNLIKELY (fp == NULL))
    {
      g_warning ("Failed to open `%s' for writing: %s",
                 tmp_path, g_strerror (errno));
      g_free (tmp_path);
      return;
    }

  /* write the uris of user customizable favourites */
  for (favourite = model->favourites; favourite != NULL; favourite = favourite->next)
    if (favourite->type == THUNAR_FAVOURITE_USER_DEFINED)
      {
        uri = thunar_vfs_uri_to_string (thunar_file_get_uri (favourite->file),
                                        THUNAR_VFS_URI_HIDE_HOST);
        fprintf (fp, "%s\n", uri);
        g_free (uri);
      }

  /* we're done writing the temporary file */
  fclose (fp);

  /* move the temporary file to it's final location (atomic writing) */
  dst_path = xfce_get_homefile (".gtk-bookmarks", NULL);
  if (rename (tmp_path, dst_path) < 0)
    {
      g_warning ("Failed to write `%s': %s",
                 dst_path, g_strerror (errno));
      unlink (tmp_path);
    }

  /* cleanup */
  g_free (dst_path);
  g_free (tmp_path);
}



static void
thunar_favourites_model_file_changed (ThunarFile            *file,
                                      ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  gint             n;

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

  for (favourite = model->favourites, n = 0; favourite != NULL; favourite = favourite->next, ++n)
    if (favourite->file == file)
      {
        iter.stamp = model->stamp;
        iter.user_data = favourite;

        path = gtk_tree_path_new_from_indices (n, -1);
        gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
        gtk_tree_path_free (path);
      }
}



static void
thunar_favourites_model_file_destroy (ThunarFile            *file,
                                      ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  GtkTreePath     *path;
  gint             n;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* lookup the favourite matching the file */
  for (favourite = model->favourites, n = 0; favourite != NULL; favourite = favourite->next, ++n)
    if (favourite->file == file)
      break;

  g_assert (favourite != NULL);
  g_assert (n < model->n_favourites);
  g_assert (THUNAR_IS_FILE (favourite->file));

  /* unlink the favourite from the model */
  --model->n_favourites;
  if (favourite == model->favourites)
    {
      favourite->next->prev = NULL;
      model->favourites = favourite->next;
    }
  else
    {
      if (favourite->next != NULL)
        favourite->next->prev = favourite->prev;
      favourite->prev->next = favourite->next;
    }

  /* tell everybody that we have lost a favourite */
  path = gtk_tree_path_new_from_indices (n, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);

  /* drop the watch from the file */
  thunar_file_unwatch (favourite->file);

  /* disconnect us from the favourite's file */
  g_signal_handlers_disconnect_matched (G_OBJECT (favourite->file),
                                        G_SIGNAL_MATCH_DATA, 0,
                                        0, NULL, NULL, model);
  g_object_unref (G_OBJECT (favourite->file));

  /* disconnect us from the favourite's volume (if any) */
  if (favourite->volume != NULL) 
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (favourite->volume),
                                            G_SIGNAL_MATCH_DATA, 0,
                                            0, NULL, NULL, model);
      g_object_unref (G_OBJECT (favourite->volume));
    }

  /* the favourites list was changed, so write the gtk bookmarks file */
  thunar_favourites_model_save (model);

  /* free the favourite data */
  g_free (favourite);
}



static void
thunar_favourites_model_volume_changed (ThunarVfsVolume       *volume,
                                        ThunarFavouritesModel *model)
{
  ThunarFavourite *favourite;
  ThunarVfsURI    *uri;
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
          uri = thunar_vfs_volume_get_mount_point (volume);
          file = thunar_file_get_for_uri (uri, NULL);
          if (G_LIKELY (file != NULL))
            {
              /* find the insert position */
              for (favourite = model->favourites, index = 0; favourite != NULL; favourite = favourite->next, ++index)
                if (favourite->type == THUNAR_FAVOURITE_SEPARATOR || favourite->type == THUNAR_FAVOURITE_USER_DEFINED)
                  break;

              /* remove the volume from the list of hidden volumes */
              model->hidden_volumes = g_list_delete_link (model->hidden_volumes, lp);

              /* allocate a new favourite */
              favourite = g_new (ThunarFavourite, 1);
              favourite->type = THUNAR_FAVOURITE_REMOVABLE_MEDIA;
              favourite->file = file;
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
      for (favourite = model->favourites, index = 0; favourite != NULL; favourite = favourite->next, ++index)
        if (favourite->volume == volume)
          break;

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
          iter.user_data = favourite;

          path = gtk_tree_path_new_from_indices (index, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
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
  ThunarFavourite *favourite;
  
  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  for (favourite = model->favourites; favourite != NULL; favourite = favourite->next)
    if (favourite->file == file)
      {
        iter->stamp = model->stamp;
        iter->user_data = favourite;
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
  return ((ThunarFavourite *) iter->user_data)->file;
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
  gint             n;

  g_return_val_if_fail (THUNAR_IS_FAVOURITES_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* append to the list is always possible */
  n = gtk_tree_path_get_indices (path)[0];
  if (n >= model->n_favourites)
    return TRUE;

  /* lookup the ThunarFavourite for the path (remember, we are
   * checking whether we can insert BEFORE path)
   */
  for (favourite = model->favourites; --n >= 0; favourite = favourite->next)
    ;

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

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] <= model->n_favourites);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* verify that the file is not already in use as favourite */
  for (favourite = model->favourites; favourite != NULL; favourite = favourite->next)
    if (G_UNLIKELY (favourite->file == file))
      return;

  /* create the new favourite that will be inserted */
  favourite = g_new0 (ThunarFavourite, 1);
  favourite->type = THUNAR_FAVOURITE_USER_DEFINED;
  favourite->file = file;
  g_object_ref (G_OBJECT (file));

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
  ThunarFavouriteType type;
  ThunarFavourite    *favourite;
  ThunarVfsVolume    *volume;
  GtkTreePath        *path;
  ThunarFile         *file;
  gint               *order;
  gint                index_src;
  gint                index_dst;
  gint                index;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));
  g_return_if_fail (gtk_tree_path_get_depth (src_path) > 0);
  g_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  g_return_if_fail (gtk_tree_path_get_indices (src_path)[0] >= 0);
  g_return_if_fail (gtk_tree_path_get_indices (src_path)[0] < model->n_favourites);
  g_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] > 0);

  index_src = gtk_tree_path_get_indices (src_path)[0];
  index_dst = gtk_tree_path_get_indices (dst_path)[0];

  if (G_UNLIKELY (index_src == index_dst))
    return;

  /* generate the order for the rows prior the dst/src rows */
  order = g_new (gint, model->n_favourites);
  for (favourite = model->favourites, index = 0;
       index < index_src && index < index_dst;
       favourite = favourite->next, ++index)
    {
      order[index] = index;
    }

  if (index == index_src)
    {
      type = favourite->type;
      file = favourite->file;
      volume = favourite->volume;

      for (; index < index_dst; favourite = favourite->next, ++index)
        {
          favourite->type = favourite->next->type;
          favourite->file = favourite->next->file;
          favourite->volume = favourite->next->volume;
          order[index] = index + 1;
        }

      favourite->type = type;
      favourite->file = file;
      favourite->volume = volume;
      order[index++] = index_src;
    }
  else
    {
      for (; index < index_src; favourite = favourite->next, ++index)
        ;

      g_assert (index == index_src);

      type = favourite->type;
      file = favourite->file;
      volume = favourite->volume;

      for (; index > index_dst; favourite = favourite->prev, --index)
        {
          favourite->type = favourite->prev->type;
          favourite->file = favourite->prev->file;
          favourite->volume = favourite->prev->volume;
          order[index] = index - 1;
        }

      g_assert (index == index_dst);

      favourite->type = type;
      favourite->file = file;
      favourite->volume = volume;
      order[index] = index_src;
      index = index_src + 1;
    }

  /* generate the remaining order */
  for (; index < model->n_favourites; ++index)
    order[index] = index;

  /* tell all listeners about the reordering just performed */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, order);
  gtk_tree_path_free (path);

  /* the favourites list was changed, so write the gtk bookmarks file */
  thunar_favourites_model_save (model);

  /* cleanup */
  g_free (order);
}


