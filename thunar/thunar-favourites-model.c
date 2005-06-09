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
static void               thunar_favourites_model_save                (ThunarFavouritesModel      *model);
static void               thunar_favourites_model_file_changed        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);
static void               thunar_favourites_model_file_destroy        (ThunarFile                 *file,
                                                                       ThunarFavouritesModel      *model);



struct _ThunarFavouritesModelClass
{
  GObjectClass __parent__;
};

struct _ThunarFavouritesModel
{
  GObject __parent__;

  guint            stamp;
  gint             n_favourites;
  ThunarFavourite *favourites;
};

struct _ThunarFavourite
{
  ThunarFile      *file;
  const gchar     *name;
  ThunarFavourite *next;
  ThunarFavourite *prev;
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
  ThunarFavourite *current;
  ThunarVfsURI    *uri;
  ThunarFile      *file;
  gchar           *path;
  gchar            line[1024];
  FILE            *fp;

  model->stamp = g_random_int ();
  model->n_favourites = 0;
  model->favourites = NULL;

  /* append the GTK+ bookmarks (if any) */
  path = xfce_get_homefile (".gtk-bookmarks", NULL);
  fp = fopen (path, "r");
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
          g_object_unref (G_OBJECT (uri));
          if (G_UNLIKELY (file == NULL))
            continue;

          /* create the favourite entry */
          favourite = g_new0 (ThunarFavourite, 1);
          favourite->file = file;

          /* append the favourite to the list */
          if (model->favourites == NULL)
            {
              model->favourites = favourite;
            }
          else
            {
              for (current = model->favourites;; current = current->next)
                if (current->next == NULL)
                  break;
              favourite->prev = current;
              current->next = favourite;
            }
          ++model->n_favourites;

          g_signal_connect (G_OBJECT (file), "changed",
                            G_CALLBACK (thunar_favourites_model_file_changed), model);
          g_signal_connect (G_OBJECT (file), "destroy",
                            G_CALLBACK (thunar_favourites_model_file_destroy), model);
        }

      fclose (fp);
    }
  g_free (path);

  /* prepend the row separator (only supported with Gtk+ 2.6) */
#if GTK_CHECK_VERSION(2,6,0)
  favourite = g_new0 (ThunarFavourite, 1);
  if (G_LIKELY (model->favourites != NULL))
    model->favourites->prev = favourite;
  favourite->next = model->favourites;
  model->favourites = favourite;
  ++model->n_favourites;
#endif

  /* prepend the 'Filesystem' favourite */
  uri = thunar_vfs_uri_new_for_path ("/");
  file = thunar_file_get_for_uri (uri, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->file = file;
      favourite->name = _("Filesystem");

      /* link the favourites to the list */
      if (G_LIKELY (model->favourites != NULL))
        model->favourites->prev = favourite;
      favourite->next = model->favourites;
      model->favourites = favourite;
      ++model->n_favourites;

      g_signal_connect (G_OBJECT (file), "changed",
                        G_CALLBACK (thunar_favourites_model_file_changed), model);
      g_signal_connect (G_OBJECT (file), "destroy",
                        G_CALLBACK (thunar_favourites_model_file_destroy), model);
    }
  g_object_unref (G_OBJECT (uri));

  /* prepend the 'Home' favourite */
  uri = thunar_vfs_uri_new_for_path (xfce_get_homedir ());
  file = thunar_file_get_for_uri (uri, NULL);
  if (G_LIKELY (file != NULL))
    {
      favourite = g_new (ThunarFavourite, 1);
      favourite->file = file;
      favourite->name = _("Home");

      /* link the favourites to the list */
      if (G_LIKELY (model->favourites != NULL))
        model->favourites->prev = favourite;
      favourite->next = model->favourites;
      model->favourites = favourite;
      ++model->n_favourites;

      g_signal_connect (G_OBJECT (file), "changed",
                        G_CALLBACK (thunar_favourites_model_file_changed), model);
      g_signal_connect (G_OBJECT (file), "destroy",
                        G_CALLBACK (thunar_favourites_model_file_destroy), model);
    }
  g_object_unref (G_OBJECT (uri));
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

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  /* free all favourites */
  for (current = model->favourites; current != NULL; current = next)
    {
      next = current->next;

      if (G_LIKELY (current->file != NULL))
        {
          g_signal_handlers_disconnect_matched (G_OBJECT (current->file),
                                                G_SIGNAL_MATCH_DATA, 0,
                                                0, NULL, NULL, model);
          g_object_unref (G_OBJECT (current->file));
        }

      g_free (current);
    }

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
  const gchar     *name;
  GdkPixbuf       *icon;

  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (tree_model));
  g_return_if_fail (iter->stamp == THUNAR_FAVOURITES_MODEL (tree_model)->stamp);

  favourite = iter->user_data;

  switch (column)
    {
    case THUNAR_FAVOURITES_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (favourite->name != NULL)
        name = favourite->name;
      else if (favourite->file != NULL)
        name = thunar_file_get_display_name (favourite->file);
      else
        name = NULL;
      g_value_set_static_string (value, name);
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_ICON:
      g_value_init (value, GDK_TYPE_PIXBUF);
      if (G_LIKELY (favourite->file != NULL))
        {
          icon = thunar_file_load_icon (favourite->file, 32);
          if (G_LIKELY (icon != NULL))
            g_value_take_object (value, icon);
        }
      break;

    case THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, favourite->file == NULL);
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
  return (favourite->name == NULL);
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
    if (favourite->name == NULL && favourite->file != NULL)
      {
        uri = thunar_vfs_uri_to_string (thunar_file_get_uri (favourite->file));
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
  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FAVOURITES_MODEL (model));

  // TODO: Implement this function.
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
  return (favourite->name == NULL
       && favourite->file != NULL);
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
  const gchar     *name;
  ThunarFile      *file;
  gint            *order;
  gint             index_src;
  gint             index_dst;
  gint             index;

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
      file = favourite->file;
      name = favourite->name;

      for (; index < index_dst; favourite = favourite->next, ++index)
        {
          favourite->file = favourite->next->file;
          favourite->name = favourite->next->name;
          order[index] = index + 1;
        }

      favourite->file = file;
      favourite->name = name;
      order[index++] = index_src;
    }
  else
    {
      for (; index < index_src; favourite = favourite->next, ++index)
        ;

      g_assert (index == index_src);

      file = favourite->file;
      name = favourite->name;

      for (; index > index_dst; favourite = favourite->prev, --index)
        {
          favourite->file = favourite->prev->file;
          favourite->name = favourite->prev->name;
          order[index] = index - 1;
        }

      g_assert (index == index_dst);

      favourite->file = file;
      favourite->name = name;
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


