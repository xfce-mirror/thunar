/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2012 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <exo/exo.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar-uca/thunar-uca-model.h>
#include <thunar-uca/thunar-uca-parser.h>
#include <thunar-uca/thunar-uca-private.h>



/* not all systems define _PATH_BSHELL */
#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif



typedef struct _ThunarUcaModelItem ThunarUcaModelItem;



static void               thunar_uca_model_tree_model_init  (GtkTreeModelIface *iface);
static void               thunar_uca_model_finalize         (GObject           *object);
static GtkTreeModelFlags  thunar_uca_model_get_flags        (GtkTreeModel      *tree_model);
static gint               thunar_uca_model_get_n_columns    (GtkTreeModel      *tree_model);
static GType              thunar_uca_model_get_column_type  (GtkTreeModel      *tree_model,
                                                             gint               column);
static gboolean           thunar_uca_model_get_iter         (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter,
                                                             GtkTreePath       *path);
static GtkTreePath       *thunar_uca_model_get_path         (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter);
static void               thunar_uca_model_get_value        (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter,
                                                             gint               column,
                                                             GValue            *value);
static gboolean           thunar_uca_model_iter_next        (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter);
static gboolean           thunar_uca_model_iter_children    (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter,
                                                             GtkTreeIter       *parent);
static gboolean           thunar_uca_model_iter_has_child   (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter);
static gint               thunar_uca_model_iter_n_children  (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter);
static gboolean           thunar_uca_model_iter_nth_child   (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter,
                                                             GtkTreeIter       *parent,
                                                             gint               n);
static gboolean           thunar_uca_model_iter_parent      (GtkTreeModel      *tree_model,
                                                             GtkTreeIter       *iter,
                                                             GtkTreeIter       *child);
static void               thunar_uca_model_resolve_paths    (GPtrArray         *normalized_uca_paths,
                                                             XfceResourceType   resource_type);
static gint               thunar_uca_model_sort_paths       (gconstpointer      path1,
                                                             gconstpointer      path2);



struct _ThunarUcaModelClass
{
  GObjectClass __parent__;
};

struct _ThunarUcaModel
{
  GObject __parent__;

  GList /* <ThunarUcaModelItem*> */ *items;
  gint                               stamp;
};

/**
 * THUNAR_UCA_FILE_PATTERNS:
 * These are paths within the user's folders
 * where UCA custom actions are searched for
 *
 * These patterns are specified in order of precedence
 * i.e. paths that are lower in the list take precedence over
 * ones that are higher up
 */
static const gchar* const THUNAR_UCA_FILE_PATTERNS[] =
{
  /* reserved for package managers distributing UCA's */
  "/usr/share/Thunar/uca.d/*.xml",
  /* deprecated, but kept for backwards compatibility */
  "/etc/xdg/Thunar/uca.xml",
  /* for admins */
  "/etc/xdg/Thunar/uca.d/*.xml",
  /* for users */
  "~/.config/Thunar/uca.xml",
  "~/.config/Thunar/uca.d/*.xml"
};

enum
{
  THUNAR_UCA_FILE_PATTERNS_SIZE = G_N_ELEMENTS (THUNAR_UCA_FILE_PATTERNS)
};



THUNARX_DEFINE_TYPE_WITH_CODE (ThunarUcaModel,
                               thunar_uca_model,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                            thunar_uca_model_tree_model_init));



static void
thunar_uca_model_class_init (ThunarUcaModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_uca_model_finalize;
}



static void
thunar_uca_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_uca_model_get_flags;
  iface->get_n_columns = thunar_uca_model_get_n_columns;
  iface->get_column_type = thunar_uca_model_get_column_type;
  iface->get_iter = thunar_uca_model_get_iter;
  iface->get_path = thunar_uca_model_get_path;
  iface->get_value = thunar_uca_model_get_value;
  iface->iter_next = thunar_uca_model_iter_next;
  iface->iter_children = thunar_uca_model_iter_children;
  iface->iter_has_child = thunar_uca_model_iter_has_child;
  iface->iter_n_children = thunar_uca_model_iter_n_children;
  iface->iter_nth_child = thunar_uca_model_iter_nth_child;
  iface->iter_parent = thunar_uca_model_iter_parent;
}



static void
thunar_uca_model_init (ThunarUcaModel *uca_model)
{
  GError                   *error = NULL;
  gchar                    *filename;
  GPtrArray /* <gchar*> */ *normalized_uca_paths;
  guint                     n;

  /* generate a unique stamp */
  uca_model->stamp = g_random_int_range (G_MININT32, G_MAXINT32);
  normalized_uca_paths = g_ptr_array_new_with_free_func (g_free);

  /* determine the path to the (uca|*).xml config files */
  thunar_uca_model_resolve_paths (normalized_uca_paths, XFCE_RESOURCE_DATA);
  thunar_uca_model_resolve_paths (normalized_uca_paths, XFCE_RESOURCE_CONFIG);
  g_ptr_array_sort (normalized_uca_paths, thunar_uca_model_sort_paths);

  for (n = 0; n < normalized_uca_paths->len; n++)
    {
      filename = g_ptr_array_index (normalized_uca_paths, n);
      if (G_LIKELY (filename != NULL))
        {
#ifdef DEBUG
          g_debug ("Found uca file: %s", ( gchar* ) filename);
#endif
          if (!thunar_uca_parser_read_uca_from_file (uca_model, filename, &error))
            {
              /* TODO #179: This error does no good for the user.
               * can we hoist this to the gui */
              g_warning ("Failed to read `%s': %s", filename, error->message);
              g_error_free (error);
              error = NULL;
            }
        }
    }
  g_ptr_array_free (normalized_uca_paths, TRUE);
}



static void
thunar_uca_model_finalize (GObject *object)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (object);

  /* release all items */
  g_list_free_full (uca_model->items, (GDestroyNotify) thunar_uca_model_item_free);

  (*G_OBJECT_CLASS (thunar_uca_model_parent_class)->finalize) (object);
}



static GtkTreeModelFlags
thunar_uca_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_uca_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_UCA_MODEL_N_COLUMNS;
}



static GType
thunar_uca_model_get_column_type (GtkTreeModel *tree_model,
                                  gint          column)
{
  switch (column)
    {
    case THUNAR_UCA_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_SUB_MENU:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_DESCRIPTION:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_GICON:
      return G_TYPE_ICON;

    case THUNAR_UCA_MODEL_COLUMN_ICON_NAME:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_COMMAND:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_STARTUP_NOTIFY:
      return G_TYPE_BOOLEAN;

    case THUNAR_UCA_MODEL_COLUMN_PATTERNS:
      return G_TYPE_STRING;

    case THUNAR_UCA_MODEL_COLUMN_TYPES:
      return G_TYPE_UINT;

    case THUNAR_UCA_MODEL_COLUMN_STOCK_LABEL:
      return G_TYPE_STRING;

    default:
        g_assert_not_reached ();
    }
}



static gboolean
thunar_uca_model_get_iter (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           GtkTreePath  *path)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  iter->stamp = uca_model->stamp;
  iter->user_data = g_list_nth (uca_model->items, gtk_tree_path_get_indices (path)[0]);

  return (iter->user_data != NULL);
}



static GtkTreePath*
thunar_uca_model_get_path (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (tree_model);
  gint            idx;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), NULL);
  g_return_val_if_fail (iter->stamp == uca_model->stamp, NULL);

  /* determine the index of the iter */
  idx = g_list_position (uca_model->items, iter->user_data);
  if (G_UNLIKELY (idx < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (idx, -1);
}



static void
thunar_uca_model_get_value (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            gint          column,
                            GValue       *value)
{
  ThunarUcaModelItem *item;
  ThunarUcaModel     *uca_model = THUNAR_UCA_MODEL (tree_model);
  gchar              *str;

  g_return_if_fail (THUNAR_UCA_IS_MODEL (tree_model));
  g_return_if_fail (iter->user_data != NULL);
  g_return_if_fail (iter->stamp == uca_model->stamp);

  item = ((GList *) iter->user_data)->data;

  /* initialize the value with the proper type */
  g_value_init (value, gtk_tree_model_get_column_type (tree_model, column));

  switch (column)
    {
    case THUNAR_UCA_MODEL_COLUMN_NAME:
      g_value_set_static_string (value, item->name ? item->name : "");
      break;

    case THUNAR_UCA_MODEL_COLUMN_SUB_MENU:
      g_value_set_static_string (value, item->submenu ? item->submenu : "");
      break;

    case THUNAR_UCA_MODEL_COLUMN_DESCRIPTION:
      g_value_set_static_string (value, item->description);
      break;

    case THUNAR_UCA_MODEL_COLUMN_GICON:
      if (item->gicon == NULL && item->icon_name != NULL)
        {
          /* cache gicon from the name */
          item->gicon = g_icon_new_for_string (item->icon_name, NULL);
        }
      g_value_set_object (value, item->gicon);
      break;

    case THUNAR_UCA_MODEL_COLUMN_ICON_NAME:
      g_value_set_static_string (value, item->icon_name);
      break;

    case THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID:
      g_value_set_static_string (value, item->unique_id);
      break;

    case THUNAR_UCA_MODEL_COLUMN_COMMAND:
      g_value_set_static_string (value, item->command);
      break;

    case THUNAR_UCA_MODEL_COLUMN_STARTUP_NOTIFY:
      g_value_set_boolean (value, item->startup_notify);
      break;

    case THUNAR_UCA_MODEL_COLUMN_PATTERNS:
      str = g_strjoinv (";", item->patterns);
      g_value_take_string (value, str);
      break;

    case THUNAR_UCA_MODEL_COLUMN_TYPES:
      g_value_set_uint (value, item->types);
      break;

    case THUNAR_UCA_MODEL_COLUMN_STOCK_LABEL:
      str = g_markup_printf_escaped ("<b>%s</b>\n%s",
                                     (item->name != NULL) ? item->name : "",
                                     (item->description != NULL) ? item->description : "");
      g_value_take_string (value, str);
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
thunar_uca_model_iter_next (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter->stamp == THUNAR_UCA_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_list_next (iter->user_data);

  return (iter->user_data != NULL);
}



static gboolean
thunar_uca_model_iter_children (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                GtkTreeIter  *parent)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);

  if (G_LIKELY (parent == NULL && uca_model->items != NULL))
    {
      iter->stamp = uca_model->stamp;
      iter->user_data = uca_model->items;

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_uca_model_iter_has_child (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_uca_model_iter_n_children (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), 0);
  g_return_val_if_fail (iter == NULL, 0);

  return g_list_length (uca_model->items);
}



static gboolean
thunar_uca_model_iter_nth_child (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent,
                                 gint          n)
{
  ThunarUcaModel *uca_model = THUNAR_UCA_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      iter->stamp = uca_model->stamp;
      iter->user_data = g_list_nth (uca_model->items, n);

      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
thunar_uca_model_iter_parent (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *child)
{
  return FALSE;
}



static void
thunar_uca_model_resolve_paths (GPtrArray       *normalized_uca_paths,
                                XfceResourceType resource_type)
{
  gchar      *pattern;
  gchar      *resource;
  gchararray *resource_folders;
  gchararray *resolve_paths;
  guint       n, s, o;

  resource_folders = xfce_resource_dirs (resource_type);

  for (s = 0; s < THUNAR_UCA_FILE_PATTERNS_SIZE; s++)
    {
      if (g_str_has_prefix (THUNAR_UCA_FILE_PATTERNS[s], "~"))
          pattern = exo_str_replace (THUNAR_UCA_FILE_PATTERNS[s], "~", xfce_get_homedir ());
      else
          pattern = g_strdup (THUNAR_UCA_FILE_PATTERNS[s]);

      for (n = 0; resource_folders[n] != NULL; n++)
        {
          if (g_str_has_prefix (pattern, resource_folders[n]))
            {
              resolve_paths = xfce_resource_match (resource_type, &pattern[strlen (resource_folders[n]) + 1], TRUE);
              for (o = 0; resolve_paths[o] != NULL; o++)
                {
                  resource = g_strjoin (G_DIR_SEPARATOR_S, resource_folders[n], resolve_paths[o], NULL);
                  if (g_file_test (resource, G_FILE_TEST_IS_REGULAR))
                    {
#ifdef DEBUG
                      g_debug ("Found resource file `%s'", resource);
#endif
                      g_ptr_array_add (normalized_uca_paths, resource);
                    }
                  else
                    {
#ifdef DEBUG
                      g_debug ("Resource file `%s' does not exist: Skipped");
#endif
                      g_free (resource);
                    }
                }
              g_strfreev (resolve_paths);
              break;
            }
        }
      g_free (pattern);
    }
  g_strfreev (resource_folders);
}



/**
 * This function is used to sort the uca files found in the file system
 * by the order found in the THUNAR_UCA_FILE_PATTERNS array.
 * @param path1 A pointer to an element in the GPtrArray
 * @param path2 A pointer to an element in the GPtrArray
 * @return 0 if both have the same rank
 * -num or +num if path1 goes before or after path2
 */
static gint
thunar_uca_model_sort_paths (gconstpointer path1,
                             gconstpointer path2)
{
  guint  n;
  gint   path1_pos = 0, path2_pos = 0;
  gchar *norm_pattern;

  for (n = 0; n < THUNAR_UCA_FILE_PATTERNS_SIZE; n++)
    {
      norm_pattern = exo_str_replace (THUNAR_UCA_FILE_PATTERNS[n], "~", xfce_get_homedir ());
      if (path1_pos == 0 && g_pattern_match_simple (norm_pattern, *( gpointer* ) path1))
        {
          path1_pos = ( gint ) n + 1;
        }
      if (path2_pos == 0 && g_pattern_match_simple (norm_pattern, *( gpointer* ) path2))
        {
          path2_pos = ( gint ) n + 1;
        }
      g_clear_pointer (&norm_pattern, g_free);
      if (path1_pos != 0 && path2_pos != 0)
        {
          break;
        }
    }

  return path1_pos - path2_pos;
}



/**
 * thunar_uca_model_get_default:
 *
 * Returns a reference to the default shared
 * #ThunarUcaModel instance. The caller is
 * responsible to free the returned instance
 * using g_object_unref() when no longer
 * needed.
 *
 * Return value: a reference to the default
 *               #ThunarUcaModel instance.
 **/
ThunarUcaModel*
thunar_uca_model_get_default (void)
{
  static ThunarUcaModel *model = NULL;

  if (model == NULL)
    {
      model = g_object_new (THUNAR_UCA_TYPE_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer*) &model);
    }
  else
    {
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



static inline ThunarUcaTypes
types_from_mime_type (const gchar *mime_type)
{
  if (mime_type == NULL)
    return 0;
  if (strcmp (mime_type, "inode/directory") == 0)
    return THUNAR_UCA_TYPE_DIRECTORIES;
  else if (strncmp (mime_type, "audio/", 6) == 0)
    return THUNAR_UCA_TYPE_AUDIO_FILES;
  else if (strncmp (mime_type, "image/", 6) == 0)
    return THUNAR_UCA_TYPE_IMAGE_FILES;
  else if (strncmp (mime_type, "text/", 5) == 0)
    return THUNAR_UCA_TYPE_TEXT_FILES;
  else if (strncmp (mime_type, "video/", 6) == 0)
    return THUNAR_UCA_TYPE_VIDEO_FILES;
  else if (strncmp (mime_type, "application/", 12) == 0)
    {
      /* quite cumbersome, certain mime types do not
       * belong here, but despite that fact, they are...
       */
      mime_type += 12;
      if (strcmp (mime_type, "javascript") == 0
          || strcmp (mime_type, "x-awk") == 0
          || strcmp (mime_type, "x-csh") == 0
          || strcmp (mime_type, "xhtml+xml") == 0
          || strcmp (mime_type, "xml") == 0)
        return THUNAR_UCA_TYPE_TEXT_FILES;
      else if (strcmp (mime_type, "ogg") == 0)
        return THUNAR_UCA_TYPE_AUDIO_FILES;
    }

  return 0;
}



/**
 * thunar_uca_model_match:
 * @uca_model  : a #ThunarUcaModel.
 * @file_infos : a #GList of #ThunarxFileInfo<!---->s.
 *
 * Returns the #GtkTreePath<!---->s for the items in @uca_model,
 * that are defined for the #ThunarxFileInfo<!---->s in the
 * @file_infos.
 *
 * The caller is responsible to free the returned list using
 * <informalexample><programlisting>
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s to items
 *               that match @file_infos.
 **/
GList*
thunar_uca_model_match (ThunarUcaModel *uca_model,
                        GList          *file_infos)
{
  typedef struct
  {
    gchar          *name;
    ThunarUcaTypes  types;
  } ThunarUcaFile;

  ThunarUcaModelItem *item;
  ThunarUcaFile      *files;
  GFile              *location;
  gchar              *mime_type;
  gboolean            matches;
  GList              *paths = NULL;
  GList              *lp;
  guint               n_files;
  guint               i, m, n;
  gchar              *path_test;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), NULL);
  g_return_val_if_fail (file_infos != NULL, NULL);

  /* special case to avoid overhead */
  if (G_UNLIKELY (uca_model->items == NULL))
    return NULL;

  /* determine the ThunarUcaFile's for the given file_infos */
  n_files = g_list_length (file_infos);
  files = g_new (ThunarUcaFile, n_files);
  for (lp = file_infos, n = 0; lp != NULL; lp = lp->next, ++n)
    {
      location = thunarx_file_info_get_location (lp->data);

      path_test = g_file_get_path (location);
      if (path_test == NULL)
        {
          /* cannot handle non-local files */
          g_object_unref (location);
          g_free (files);

          return NULL;
        }
      g_free (path_test);

      g_object_unref (location);

      mime_type = thunarx_file_info_get_mime_type (lp->data);

      files[n].name = thunarx_file_info_get_name (lp->data);
      files[n].types = types_from_mime_type (mime_type);

      if (G_UNLIKELY (files[n].types == 0))
        files[n].types = THUNAR_UCA_TYPE_OTHER_FILES;

      g_free (mime_type);
    }

  /* lookup the matching items */
  for (i = 0, lp = uca_model->items; lp != NULL; ++i, lp = lp->next)
    {
      /* check if we can just ignore this item */
      item = (ThunarUcaModelItem *) lp->data;
      if (!item->multiple_selection && n_files > 1)
        continue;

      /* match the specified files */
      for (n = 0; n < n_files; ++n)
        {
          /* verify that we support this type of file */
          if ((files[n].types & item->types) == 0)
            break;

          /* at least on pattern must match the file name */
          for (m = 0, matches = FALSE; item->patterns[m] != NULL && !matches; ++m)
            matches = g_pattern_match_simple (item->patterns[m], files[n].name);

          /* no need to continue if none of the patterns match */
          if (!matches)
            break;
        }

      /* add the path if all files match one of the patterns */
      if (G_UNLIKELY (n == n_files))
        paths = g_list_append (paths, gtk_tree_path_new_from_indices (i, -1));
    }

  /* cleanup */
  for (n = 0; n < n_files; ++n)
    g_free (files[n].name);
  g_free (files);

  return paths;
}



/**
 * thunar_uca_model_append:
 * @uca_model : a #ThunarUcaModel.
 * @iter      : return location for the new #GtkTreeIter.
 *
 * Appends a new item to the @uca_model and returns the
 * @iter of the new item.
 **/
void
thunar_uca_model_append (ThunarUcaModel *uca_model,
                         GtkTreeIter    *iter)
{
  ThunarUcaModelItem *item;
  GtkTreePath        *path;

  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter != NULL);

  /* append the new item */
  item = thunar_uca_model_item_new ();
  uca_model->items = g_list_append (uca_model->items, item);

  /* determine the tree iter of the new item */
  iter->stamp = uca_model->stamp;
  iter->user_data = g_list_last (uca_model->items);

  /* notify listeners about the new item */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (uca_model), iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (uca_model), path, iter);
  gtk_tree_path_free (path);
}



/**
 * thunar_uca_model_exchange:
 * @uca_model : a #ThunarUcaModel.
 * @iter_a    : a #GtkTreeIter.
 * @iter_b    : a #GtkTreeIter.
 *
 * Exchanges the items at @iter_a and @iter_b in
 * @uca_model.
 **/
void
thunar_uca_model_exchange (ThunarUcaModel *uca_model,
                           GtkTreeIter    *iter_a,
                           GtkTreeIter    *iter_b)
{
  ThunarUcaModelItem *item;
  GtkTreePath        *path;
  GList              *list_a = iter_a->user_data;
  GList              *list_b = iter_b->user_data;
  gint               *new_order;
  guint               n_items;
  guint               n;

  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter_a->stamp == uca_model->stamp);
  g_return_if_fail (iter_b->stamp == uca_model->stamp);

  /* allocate and initialize the new order array */
  n_items = g_list_length (uca_model->items);
  new_order = g_newa (gint, n_items);
  for (n = 0; n < n_items; ++n)
    new_order[n] = n;

  /* change new_order appropriately */
  new_order[g_list_position (uca_model->items, list_a)] = g_list_position (uca_model->items, list_b);
  new_order[g_list_position (uca_model->items, list_b)] = g_list_position (uca_model->items, list_a);

  /* perform the exchange */
  item = list_a->data;
  list_a->data = list_b->data;
  list_b->data = item;

  /* notify listeners about the new order */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (uca_model), path, NULL, new_order);
  gtk_tree_path_free (path);
}



gboolean
thunar_uca_model_action_exists (ThunarUcaModel *uca_model,
                                GtkTreeIter    *iter,
                                const gchar    *action_name)
{
  GtkTreeModel *tree_model;
  gboolean      iter_valid;
  gchar        *existing_name;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);
  tree_model = GTK_TREE_MODEL (uca_model);

  if (iter == NULL)
    {
      GtkTreeIter foo;
      iter = &foo;
    }

  /* remove the existing item (if any)
   * TODO #179: This is kinda slow O(N). Can we make it faster?
   * Using GTree instead of GList will offer O(log N) lookups.
   *
   * GHashTable would have been best, but it will require more work to
   * make iteration idempotent
   */
  if ((iter_valid = gtk_tree_model_iter_nth_child (tree_model, iter, NULL, 0)))
    {
      for (gtk_tree_model_get (tree_model, iter,
                               THUNAR_UCA_MODEL_COLUMN_NAME, &existing_name, -1);
           g_strcmp0(action_name, existing_name) != 0;
           gtk_tree_model_get (tree_model, iter,
                               THUNAR_UCA_MODEL_COLUMN_NAME, &action_name, -1))
        {
          g_free (existing_name);
          if (!gtk_tree_model_iter_next (tree_model, iter))
            return FALSE;
        }
      g_free (existing_name);
    }
  return iter_valid;
}



gboolean
thunar_uca_model_action_exists_no_iter (ThunarUcaModel *uca_model,
                                        const gchar    *action_name)
{
  static GtkTreeIter iter;
  return thunar_uca_model_action_exists (uca_model, &iter, action_name);
}



/**
 * thunar_uca_model_remove:
 * @uca_model : a #ThunarUcaModel.
 * @iter      : a #GtkTreeIter.
 *
 * Removes the item at the given @iter from the
 * @uca_model.
 **/
void
thunar_uca_model_remove (ThunarUcaModel *uca_model,
                         GtkTreeIter    *iter)
{
  ThunarUcaModelItem *item;
  GtkTreePath        *path;
  gchar              *unique_id;
  gchar              *accel_path;
  GtkAccelKey         key;

  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter->stamp == uca_model->stamp);

  /* clear any accelerator associated to the item */
  gtk_tree_model_get (GTK_TREE_MODEL (uca_model), iter,
                      THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID, &unique_id,
                      -1);
  accel_path = g_strdup_printf ("<Actions>/ThunarActions/uca-action-%s", unique_id);

  if (gtk_accel_map_lookup_entry (accel_path, &key) && key.accel_key != 0)
    gtk_accel_map_change_entry (accel_path, 0, 0, TRUE);
  g_free (accel_path);

  /* determine the path for the item to remove */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (uca_model), iter);

  /* remove the node from the list */
  item = ((GList *) iter->user_data)->data;
  uca_model->items = g_list_delete_link (uca_model->items, iter->user_data);
  thunar_uca_model_item_free (item);

  /* notify listeners */
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (uca_model), path);

  /* cleanup */
  gtk_tree_path_free (path);
}



/**
 * thunar_uca_model_update:
 * @uca_model          : a #ThunarUcaModel.
 * @iter               : the #GtkTreeIter of the item to update.
 * @filename           : (Optional) name of the where the action was read
 * @name               : the name of the item.
 * @submenu           : the submenu structure in which the item is placed.
 * @unique_id          : a unique ID for the item before any edits.
 * @description        : the description of the item.
 * @icon               : the icon for the item.
 * @command            : the command of the item.
 * @patterns           : the patterns to match files for this item.
 * @types              : where to apply this item.
 *
 * Updates the @uca_model item at @iter with the given values.
 **/
void
thunar_uca_model_update (ThunarUcaModel *uca_model,
                         GtkTreeIter    *iter,
                         const gchar    *filename,
                         const gchar    *name,
                         const gchar    *submenu,
                         const gchar    *unique_id,
                         const gchar    *description,
                         const gchar    *icon,
                         const gchar    *command,
                         gboolean        startup_notify,
                         const gchar    *patterns,
                         ThunarUcaTypes  types,
                         guint           accel_key,
                         GdkModifierType accel_mods)
{
  ThunarUcaModelItem *item;
  GtkTreePath        *path;
  gchar              *accel_path;

  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter->stamp == uca_model->stamp);

  /* reset the previous item values */
  item = ((GList *) iter->user_data)->data;
  thunar_uca_model_item_reset (item);

  /* setup the new item values */
  thunar_uca_model_item_update (item,
                                name,
                                submenu,
                                unique_id,
                                description,
                                icon,
                                command,
                                startup_notify,
                                patterns,
                                filename,
                                types);

#ifdef DEBUG
  if (!xfce_str_is_empty (filename))
    g_debug ("Updated item: `%s', found in file: %s", name, filename);
#endif

  /* notify listeners about the changed item */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (uca_model), iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (uca_model), path, iter);
  gtk_tree_path_free (path);

  /* update accelerator */
  if (accel_key > 0)
    {
      accel_path = g_strdup_printf ("<Actions>/ThunarActions/uca-action-%s", item->unique_id);
      gtk_accel_map_change_entry (accel_path, accel_key, accel_mods, TRUE);
      g_free (accel_path);
    }
}



/**
 * thunar_uca_model_save:
 * @uca_model : a #ThunarUcaModel.
 * @error     : return location for errors or %NULL.
 *
 * Instructs @uca_model to sync its current state to
 * persistent storage.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
thunar_uca_model_save (ThunarUcaModel *uca_model,
                       GError        **error)
{
  ThunarUcaModelItem                       *item;
  gboolean                                  result = FALSE;
  gchar                                    *tmp_path = NULL;
  const gchar                              *path;
  gchar                                    *default_path = NULL;
  GList /* <ThunarUcaModelItem*> */        *lp;
  FILE                                     *fp;
  gint                                      fd;
  GHashTable /* <gchar*, GPtrArray*> */    *item_files;
  GHashTableIter /* <gchar*, GPtrArray*> */ item_files_iter;
  GPtrArray /* <ThunarUcaModelItem*> */    *item_files_list = NULL;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* determine the save location */
  default_path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/uca.xml", TRUE);
  if (G_UNLIKELY (default_path == NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to determine save location for uca.xml"));
      g_free (default_path);

      return FALSE;
    }
  item_files = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) g_ptr_array_unref);
  g_hash_table_replace (item_files, (gpointer) default_path, g_ptr_array_new ());

  for (lp = uca_model->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;
      path = thunar_uca_model_item_get_filename (item);
      if (path == NULL)
        {
          g_hash_table_lookup_extended (item_files, default_path, NULL, (gpointer *) &item_files_list);
        }
      else if (!g_hash_table_lookup_extended (item_files, path, NULL, (gpointer *) &item_files_list))
        {
          /* can we write to the file? */
          if (g_access (path, W_OK) < 0)
            {
              /* should we write the item to a file? */
              if (!thunar_uca_model_item_is_modified (item))
                continue;
#ifdef DEBUG
              g_debug ("Action called: `%s', cannot be written to: %s", item->name, path);
#endif
              /* I guess not so just use the default path ... */
              item_files_list = g_hash_table_lookup (item_files, default_path);
            }
          else
            {
#ifdef DEBUG
              g_debug ("Action called: `%s', will be written to: %s", item->name, path);
#endif
              item_files_list = g_ptr_array_new ();
              g_hash_table_replace (item_files, (gpointer) path, item_files_list);
            }
        }
      g_ptr_array_add (item_files_list, item);
      item_files_list = NULL;
    }

  /* start writing out the actions */
  for (g_hash_table_iter_init (&item_files_iter, item_files);
       g_hash_table_iter_next (&item_files_iter, (gpointer *) &path, (gpointer *) &item_files_list);)
    {
      /* try to open a temporary file */
      tmp_path = g_strconcat (path, ".XXXXXX", NULL);
      fd = g_mkstemp (tmp_path);
      if (G_UNLIKELY (fd < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s", g_strerror (errno));
          goto cleanup;
        }
#ifdef DEBUG
      g_debug ("Created temp path: `%s'", tmp_path);
#endif

      /* wrap the file descriptor into a file pointer */
      fp = fdopen (fd, "w");

      /* write the header */
      fputs ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<actions>\n", fp);

      /* write the model items */
      g_ptr_array_foreach (item_files_list, (GFunc) thunar_uca_model_item_write_file, fp);

      /* write the footer and close the tmp file */
      fputs ("</actions>\n", fp);
      fclose (fp);

      /* rename the file (atomic) */
      if (g_rename (tmp_path, path) < 0)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       "%s", g_strerror (errno));
          g_unlink (tmp_path);
          goto cleanup;
        }
#ifdef DEBUG
      g_debug ("Finished writing to temp path: `%s'", tmp_path);
#endif
      g_clear_pointer (&tmp_path, g_free);
    }

  /* yeppa, successful */
  result = TRUE;

cleanup:
  /* cleanup */
  g_free (tmp_path);
  g_hash_table_remove_all (item_files);
  g_hash_table_unref (item_files);
  g_free (default_path);

  return result;
}



/**
 * thunar_uca_model_parse_argv:
 * @uca_model   : a #ThunarUcaModel.
 * @iter        : the #GtkTreeIter of the item.
 * @file_infos  : the #GList of #ThunarxFileInfo<!---->s to pass to the item.
 * @argcp       : return location for number of args.
 * @argvp       : return location for array of args.
 * @error       : return location for errors or %NULL.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
thunar_uca_model_parse_argv (ThunarUcaModel *uca_model,
                             GtkTreeIter    *iter,
                             GList          *file_infos,
                             gint           *argcp,
                             gchar        ***argvp,
                             GError        **error)
{
  ThunarUcaModelItem *item;
  GString            *command_line = g_string_new (NULL);
  GList              *lp;
  GSList             *uri_list = NULL;
  gchar              *expanded;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (uca_model), FALSE);
  g_return_val_if_fail (iter->stamp == uca_model->stamp, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* verify that a command is set for the item */
  item = (ThunarUcaModelItem *) ((GList *) iter->user_data)->data;
  if (item->command == NULL || *item->command == '\0')
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Command not configured"));
      goto error;
    }

  command_line->str = thunar_uca_expand_deprecated_desktop_entry_field_codes (item->command, file_infos);

  /* expand the non deprecated field codes */
  for (lp = file_infos; lp != NULL; lp = lp->next)
    uri_list = g_slist_prepend (uri_list, thunarx_file_info_get_uri (lp->data));
  uri_list = g_slist_reverse (uri_list);

  expanded = xfce_expand_desktop_entry_field_codes (command_line->str, uri_list,
                                                    NULL, NULL, NULL, FALSE);
  g_string_free (command_line, TRUE);
  g_slist_free_full (uri_list, g_free);

  /* we run the command using the bourne shell (or the systems
   * replacement for the bourne shell), so environment variables
   * and backticks can be used for the action commands.
   */
  (*argcp) = 3;
  (*argvp) = g_new (gchar *, 4);
  (*argvp)[0] = g_strdup (_PATH_BSHELL);
  (*argvp)[1] = g_strdup ("-c");
  (*argvp)[2] = expanded;
  (*argvp)[3] = NULL;

  return TRUE;

error:
  g_string_free (command_line, TRUE);

  return FALSE;
}
