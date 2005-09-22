/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
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

#include <thunar/thunar-chooser-model.h>
#include <thunar/thunar-icon-factory.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_LOADING,
  PROP_MIME_INFO,
};



static void     thunar_chooser_model_class_init     (ThunarChooserModelClass  *klass);
static void     thunar_chooser_model_init           (ThunarChooserModel       *model);
static void     thunar_chooser_model_finalize       (GObject                  *object);
static void     thunar_chooser_model_get_property   (GObject                  *object,
                                                     guint                     prop_id,
                                                     GValue                   *value,
                                                     GParamSpec               *pspec);
static void     thunar_chooser_model_set_property   (GObject                  *object,
                                                     guint                     prop_id,
                                                     const GValue             *value,
                                                     GParamSpec               *pspec);
static void     thunar_chooser_model_append         (ThunarChooserModel       *model,
                                                     const gchar              *title,
                                                     const gchar              *icon_name,
                                                     GList                    *applications);
static void     thunar_chooser_model_import         (ThunarChooserModel       *model,
                                                     GList                    *applications);
static GList   *thunar_chooser_model_readdir        (ThunarChooserModel       *model,
                                                     const gchar              *absdir,
                                                     const gchar              *reldir);
static gpointer thunar_chooser_model_thread         (gpointer                  user_data);
static gboolean thunar_chooser_model_timer          (gpointer                  user_data);
static void     thunar_chooser_model_timer_destroy  (gpointer                  user_data);




struct _ThunarChooserModelClass
{
  GtkTreeStoreClass __parent__;
};

struct _ThunarChooserModel
{
  GtkTreeStore __parent__;

  ThunarVfsMimeInfo *mime_info;

  /* thread interaction */
  gint               timer_id;
  GThread           *thread;
  volatile gboolean  finished;
  volatile gboolean  cancelled;
};



G_DEFINE_TYPE (ThunarChooserModel, thunar_chooser_model, GTK_TYPE_TREE_STORE);



static void
thunar_chooser_model_class_init (ThunarChooserModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_chooser_model_finalize;
  gobject_class->get_property = thunar_chooser_model_get_property;
  gobject_class->set_property = thunar_chooser_model_set_property;

  /**
   * ThunarChooserModel::loading:
   *
   * Whether the contents of the #ThunarChooserModel are
   * currently being loaded from disk.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_boolean ("loading",
                                                         _("Loading"),
                                                         _("Whether the model is currently being loaded"),
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  /**
   * ThunarChooserModel::mime-info:
   *
   * The #ThunarVfsMimeInfo info for the model.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MIME_INFO,
                                   g_param_spec_boxed ("mime-info",
                                                       _("Mime info"),
                                                       _("The mime info for the model"),
                                                       THUNAR_VFS_TYPE_MIME_INFO,
                                                       G_PARAM_CONSTRUCT_ONLY | EXO_PARAM_READWRITE));
}



static void
thunar_chooser_model_init (ThunarChooserModel *model)
{
  /* allocate the types array for the columns */
  GType column_types[THUNAR_CHOOSER_MODEL_N_COLUMNS] =
  {
    G_TYPE_STRING,
    GDK_TYPE_PIXBUF,
    THUNAR_VFS_TYPE_MIME_APPLICATION,
    PANGO_TYPE_STYLE,
    G_TYPE_BOOLEAN,
  };

  /* register the column types */
  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), G_N_ELEMENTS (column_types), column_types);

  /* start to load the applications installed on the system */
  model->thread = g_thread_create (thunar_chooser_model_thread, model, TRUE, NULL);

  /* start the timer to monitor the thread */
  model->timer_id = g_timeout_add_full (G_PRIORITY_LOW, 200, thunar_chooser_model_timer,
                                        model, thunar_chooser_model_timer_destroy);
}



static void
thunar_chooser_model_finalize (GObject *object)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);
  GList              *applications;

  /* drop any pending timer */
  if (G_UNLIKELY (model->timer_id >= 0))
    g_source_remove (model->timer_id);

  /* cancel the thread (if running) */
  if (G_UNLIKELY (model->thread != NULL))
    {
      /* set the cancellation flag */
      model->cancelled = TRUE;

      /* join the thread */
      applications = g_thread_join (model->thread);

      /* ditch the returned application list */
      g_list_foreach (applications, (GFunc) thunar_vfs_mime_application_unref, NULL);
      g_list_free (applications);
    }

  /* drop the mime info (if any) */
  if (G_LIKELY (model->mime_info != NULL))
    thunar_vfs_mime_info_unref (model->mime_info);

  (*G_OBJECT_CLASS (thunar_chooser_model_parent_class)->finalize) (object);
}



static void
thunar_chooser_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  switch (prop_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, thunar_chooser_model_get_loading (model));
      break;

    case PROP_MIME_INFO:
      g_value_set_boxed (value, thunar_chooser_model_get_mime_info (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  switch (prop_id)
    {
    case PROP_MIME_INFO:
      if (G_LIKELY (model->mime_info == NULL))
        model->mime_info = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_model_append (ThunarChooserModel *model,
                             const gchar        *title,
                             const gchar        *icon_name,
                             GList              *applications)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GtkTreeIter        parent_iter;
  GtkTreeIter        child_iter;
  GdkPixbuf         *icon;
  GList             *lp;

  g_return_if_fail (THUNAR_IS_CHOOSER_MODEL (model));
  g_return_if_fail (title != NULL);
  g_return_if_fail (icon_name != NULL);

  /* query the default icon theme/factory */
  icon_theme = gtk_icon_theme_get_default ();
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* insert the section title */
  icon = gtk_icon_theme_load_icon (icon_theme, icon_name, 24, 0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent_iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter,
                      THUNAR_CHOOSER_MODEL_COLUMN_NAME, title,
                      THUNAR_CHOOSER_MODEL_COLUMN_ICON, icon,
                      -1);
  if (G_LIKELY (icon != NULL))
    g_object_unref (G_OBJECT (icon));

  /* check if we have any program items */
  if (G_LIKELY (applications != NULL))
    {
      /* insert the program items */
      for (lp = applications; lp != NULL; lp = lp->next)
        {
          /* determine the icon name for the application */
          icon_name = thunar_vfs_mime_application_lookup_icon_name (lp->data, icon_theme);

          /* try to load the themed icon for the program */
          icon = thunar_icon_factory_load_icon (icon_factory, icon_name, 24, NULL, FALSE);

          /* append the tree row with the program data */
          gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &parent_iter);
          gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                              THUNAR_CHOOSER_MODEL_COLUMN_NAME, thunar_vfs_mime_application_get_name (lp->data),
                              THUNAR_CHOOSER_MODEL_COLUMN_ICON, icon,
                              THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, lp->data,
                              -1);

          /* release the icon (if any) */
          if (G_LIKELY (icon != NULL))
            g_object_unref (G_OBJECT (icon));
        }
    }
  else
    {
      /* tell the user that we don't have any applications for this category */
      gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &parent_iter);
      gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                          THUNAR_CHOOSER_MODEL_COLUMN_NAME, _("None available"),
                          THUNAR_CHOOSER_MODEL_COLUMN_STYLE, PANGO_STYLE_ITALIC,
                          THUNAR_CHOOSER_MODEL_COLUMN_STYLE_SET, TRUE,
                          -1);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));
}



static void
thunar_chooser_model_import (ThunarChooserModel *model,
                             GList              *applications)
{
  ThunarVfsMimeDatabase *mime_database;
  GList                 *recommended;
  GList                 *other = NULL;
  GList                 *lp;
  GList                 *p;

  g_return_if_fail (THUNAR_IS_CHOOSER_MODEL (model));
  g_return_if_fail (model->mime_info != NULL);

  /* determine the recommended applications for the mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  recommended = thunar_vfs_mime_database_get_applications (mime_database, model->mime_info);
  g_object_unref (G_OBJECT (mime_database));

  /* add all applications to other that are not already on recommended */
  for (lp = applications; lp != NULL; lp = lp->next)
    {
      /* check if this application is already on the recommended list */
      for (p = recommended; p != NULL; p = p->next)
        if (thunar_vfs_mime_application_equal (p->data, lp->data))
          break;

      /* add to the list of other applications if it's not on recommended */
      if (G_LIKELY (p == NULL))
        other = g_list_append (other, lp->data);
    }

  /* append the "Recommended Applications:" category */
  thunar_chooser_model_append (model, _("Recommended Applications:"), "gnome-settings-default-applications", recommended);

  /* append the "Other Applications:" category */
  thunar_chooser_model_append (model, _("Other Applications:"), "gnome-applications", other);

  /* cleanup */
  g_list_foreach (recommended, (GFunc) thunar_vfs_mime_application_unref, NULL);
  g_list_free (recommended);
  g_list_free (other);
}



static GList*
thunar_chooser_model_readdir (ThunarChooserModel *model,
                              const gchar        *absdir,
                              const gchar        *reldir)
{
  ThunarVfsMimeApplication *application;
  const gchar              *name;
  GList                    *applications = NULL;
  gchar                    *abspath;
  gchar                    *relpath;
  gchar                    *p;
  GDir                     *dp;

  g_return_val_if_fail (THUNAR_IS_CHOOSER_MODEL (model), NULL);
  g_return_val_if_fail (reldir == NULL || !g_path_is_absolute (reldir), NULL);
  g_return_val_if_fail (g_path_is_absolute (absdir), NULL);

  /* try to open the directory */
  dp = g_dir_open (absdir, 0, NULL);
  if (G_LIKELY (dp != NULL))
    {
      /* process the files within the directory */
      while (!model->cancelled)
        {
          /* read the next file entry */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;

          /* generate the absolute path to the file entry */
          abspath = g_build_filename (absdir, name, NULL);

          /* generate the relative path to the file entry */
          relpath = (reldir != NULL) ? g_build_filename (reldir, name, NULL) : g_strdup (name);

          /* check if we have a directory or a regular file here */
          if (g_file_test (abspath, G_FILE_TEST_IS_DIR))
            {
              /* recurse for directories */
              applications = g_list_concat (applications, thunar_chooser_model_readdir (model, abspath, relpath));
            }
          else if (g_file_test (abspath, G_FILE_TEST_IS_REGULAR) && g_str_has_suffix (name, ".desktop"))
            {
              /* generate the desktop-id from the relative path */
              for (p = relpath; *p != '\0'; ++p)
                if (*p == G_DIR_SEPARATOR)
                  *p = '-';

              /* try to load the application for the given desktop-id */
              application = thunar_vfs_mime_application_new_from_file (abspath, relpath);

              /* check if we have successfully loaded the application */
              if (G_LIKELY (application != NULL))
                {
                  /* check if the application supports atleast one mime-type */
                  if (thunar_vfs_mime_application_get_mime_types (application) != NULL)
                    applications = g_list_append (applications, application);
                  else
                    thunar_vfs_mime_application_unref (application);
                }
            }

          /* cleanup */
          g_free (abspath);
          g_free (relpath);
        }

      /* close the directory handle */
      g_dir_close (dp);
    }

  return applications;
}



static gint
compare_application_by_name (gconstpointer a,
                             gconstpointer b)
{
  return strcmp (thunar_vfs_mime_application_get_name (a),
                 thunar_vfs_mime_application_get_name (b));
}



static gpointer
thunar_chooser_model_thread (gpointer user_data)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (user_data);
  GList              *applications = NULL;
  GList              *list;
  GList              *lp;
  GList              *p;
  gchar             **paths;
  guint               n;

  /* determine the available applications/ directories */
  paths = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, "applications/");
  for (n = 0; !model->cancelled && paths[n] != NULL; ++n)
    {
      /* lookup the applications in this directory */
      list = thunar_chooser_model_readdir (model, paths[n], NULL);

      /* merge the applications with what we already have */
      for (lp = list; lp != NULL; lp = lp->next)
        {
          /* ignore hidden applications to be compatible with the Nautilus mess */
          if ((thunar_vfs_mime_application_get_flags (lp->data) & THUNAR_VFS_MIME_APPLICATION_HIDDEN) != 0)
            {
              thunar_vfs_mime_application_unref (lp->data);
              continue;
            }

          /* check if we have that application already */
          for (p = applications; p != NULL; p = p->next)
            {
              /* compare the desktop-ids */
              if (thunar_vfs_mime_application_equal (p->data, lp->data))
                break;
            }

          /* no need to add if we have it already */
          if (G_UNLIKELY (p != NULL))
            {
              thunar_vfs_mime_application_unref (lp->data);
              continue;
            }

          /* insert the application into the list */
          applications = g_list_insert_sorted (applications, lp->data, compare_application_by_name);
        }
      
      /* free the temporary list */
      g_list_free (list);
    }
  g_strfreev (paths);

  /* tell the model that we're done */
  model->finished = TRUE;

  return applications;
}



static gboolean
thunar_chooser_model_timer (gpointer user_data)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (user_data);
  gboolean            finished;
  GList              *applications;

  /* check if the applications are loaded */
  GDK_THREADS_ENTER ();
  finished = model->finished;
  if (G_LIKELY (finished))
    {
      /* grab the application list from the thread */
      applications = g_thread_join (model->thread);
      model->thread = NULL;

      /* process the applications list */
      thunar_chooser_model_import (model, applications);

      /* tell everybody that the model is loaded */
      g_object_notify (G_OBJECT (model), "loading");

      /* free the application list */
      g_list_foreach (applications, (GFunc) thunar_vfs_mime_application_unref, NULL);
      g_list_free (applications);
    }
  GDK_THREADS_LEAVE ();

  return !finished;
}



static void
thunar_chooser_model_timer_destroy (gpointer user_data)
{
  THUNAR_CHOOSER_MODEL (user_data)->timer_id = -1;
}



/**
 * thunar_chooser_model_new:
 * @mime_info : a #ThunarVfsMimeInfo.
 *
 * Allocates a new #ThunarChooserModel for @mime_info.
 *
 * Return value: the newly allocated #ThunarChooserModel.
 **/
ThunarChooserModel*
thunar_chooser_model_new (ThunarVfsMimeInfo *mime_info)
{
  return g_object_new (THUNAR_TYPE_CHOOSER_MODEL,
                       "mime-info", mime_info,
                       NULL);
}



/**
 * thunar_chooser_model_get_loading:
 * @model : a #ThunarChooserModel.
 *
 * Returns %TRUE if @model is currently being loaded.
 *
 * Return value: %TRUE if @model is currently being loaded.
 **/
gboolean
thunar_chooser_model_get_loading (ThunarChooserModel *model)
{
  g_return_val_if_fail (THUNAR_IS_CHOOSER_MODEL (model), FALSE);
  return (model->thread != NULL);
}



/**
 * thunar_chooser_model_get_mime_info:
 * @model : a #ThunarChooserModel.
 *
 * Returns the #ThunarVfsMimeInfo for @model.
 *
 * Return value: the #ThunarVfsMimeInfo for @model.
 **/
ThunarVfsMimeInfo*
thunar_chooser_model_get_mime_info (ThunarChooserModel *model)
{
  g_return_val_if_fail (THUNAR_IS_CHOOSER_MODEL (model), NULL);
  return model->mime_info;
}

