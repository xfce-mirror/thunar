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

#include <thunar/thunar-icon-view.h>



enum
{
  PROP_0,
  PROP_LIST_MODEL,
  PROP_MODEL,
  PROP_STATUSBAR_TEXT,
};



static void             thunar_icon_view_class_init         (ThunarIconViewClass *klass);
static void             thunar_icon_view_view_init          (ThunarViewIface     *iface);
static void             thunar_icon_view_init               (ThunarIconView      *icon_view);
static void             thunar_icon_view_finalize           (GObject             *object);
static void             thunar_icon_view_get_property       (GObject             *object,
                                                             guint                prop_id,
                                                             GValue              *value,
                                                             GParamSpec          *pspec);
static void             thunar_icon_view_set_property       (GObject             *object,
                                                             guint                prop_id,
                                                             const GValue        *value,
                                                             GParamSpec          *pspec);
static void             thunar_icon_view_item_activated     (ExoIconView         *view,
                                                             GtkTreePath         *path);
static ThunarListModel *thunar_icon_view_get_list_model     (ThunarView          *view);
static void             thunar_icon_view_set_list_model     (ThunarView          *view,
                                                             ThunarListModel     *model);
static const gchar     *thunar_icon_view_get_statusbar_text (ThunarView          *view);
static void             thunar_icon_view_selection_changed  (ExoIconView         *view);
static void             thunar_icon_view_update             (ThunarIconView      *icon_view);
static gboolean         thunar_icon_view_update_idle        (gpointer             user_data);
static void             thunar_icon_view_update_idle_destroy(gpointer             user_data);



struct _ThunarIconViewClass
{
  ExoIconViewClass __parent__;
};

struct _ThunarIconView
{
  ExoIconView __parent__;

  ThunarListModel *list_model;
  gchar           *statusbar_text;
  gint             update_idle_id;
};



G_DEFINE_TYPE_WITH_CODE (ThunarIconView,
                         thunar_icon_view,
                         EXO_TYPE_ICON_VIEW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW,
                                                thunar_icon_view_view_init));



static void
thunar_icon_view_class_init (ThunarIconViewClass *klass)
{
  ExoIconViewClass *exoicon_view_class;
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_icon_view_finalize;
  gobject_class->get_property = thunar_icon_view_get_property;
  gobject_class->set_property = thunar_icon_view_set_property;

  exoicon_view_class = EXO_ICON_VIEW_CLASS (klass);
  exoicon_view_class->item_activated = thunar_icon_view_item_activated;
  exoicon_view_class->selection_changed = thunar_icon_view_selection_changed;

  g_object_class_override_property (gobject_class,
                                    PROP_LIST_MODEL,
                                    "list-model");
  g_object_class_override_property (gobject_class,
                                    PROP_MODEL,
                                    "model");
  g_object_class_override_property (gobject_class,
                                    PROP_STATUSBAR_TEXT,
                                    "statusbar-text");
}



static void
thunar_icon_view_view_init (ThunarViewIface *iface)
{
  iface->get_list_model = thunar_icon_view_get_list_model;
  iface->set_list_model = thunar_icon_view_set_list_model;
  iface->get_statusbar_text = thunar_icon_view_get_statusbar_text;
}



static void
thunar_icon_view_init (ThunarIconView *icon_view)
{
  icon_view->update_idle_id = -1;

  /* initialize the icon view properties */
  exo_icon_view_set_text_column (EXO_ICON_VIEW (icon_view), THUNAR_LIST_MODEL_COLUMN_NAME);
  exo_icon_view_set_pixbuf_column (EXO_ICON_VIEW (icon_view), THUNAR_LIST_MODEL_COLUMN_ICON_NORMAL);
  exo_icon_view_set_selection_mode (EXO_ICON_VIEW (icon_view), GTK_SELECTION_MULTIPLE);
}



static void
thunar_icon_view_finalize (GObject *object)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (object);

  /* drop any pending idle invokation */
  if (G_UNLIKELY (icon_view->update_idle_id >= 0))
    g_source_remove (icon_view->update_idle_id);

  /* free the statusbar-text */
  if (G_LIKELY (icon_view->statusbar_text != NULL))
    g_free (icon_view->statusbar_text);

  /* free the model association */
  thunar_view_set_list_model (THUNAR_VIEW (icon_view), NULL);

  G_OBJECT_CLASS (thunar_icon_view_parent_class)->finalize (object);
}



static void
thunar_icon_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ThunarView *view = THUNAR_VIEW (object);

  switch (prop_id)
    {
    case PROP_LIST_MODEL:
    case PROP_MODEL:
      g_value_set_object (value, thunar_view_get_list_model (view));
      break;

    case PROP_STATUSBAR_TEXT:
      g_value_set_static_string (value, thunar_view_get_statusbar_text (view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarView *view = THUNAR_VIEW (object);

  switch (prop_id)
    {
    case PROP_LIST_MODEL:
      thunar_view_set_list_model (view, g_value_get_object (value));
      break;

    case PROP_MODEL:
      g_error ("The \"model\" property may not be set directly on ThunarIconView's");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_view_item_activated (ExoIconView *view,
                                 GtkTreePath *path)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (view);
  GtkTreeIter     iter;
  ThunarFile     *file;

  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));
  g_return_if_fail (THUNAR_IS_LIST_MODEL (icon_view->list_model));

  /* if the user activated a directory, change to that directory */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (icon_view->list_model), &iter, path);
  file = thunar_list_model_get_file (icon_view->list_model, &iter);
  if (thunar_file_is_directory (file))
    thunar_view_change_directory (THUNAR_VIEW (icon_view), file);

  /* invoke the item_activated method on the parent class */
  if (EXO_ICON_VIEW_CLASS (thunar_icon_view_parent_class)->item_activated != NULL)
    EXO_ICON_VIEW_CLASS (thunar_icon_view_parent_class)->item_activated (view, path);
}



static ThunarListModel*
thunar_icon_view_get_list_model (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (view), NULL);
  return THUNAR_ICON_VIEW (view)->list_model;
}



static void
thunar_icon_view_set_list_model (ThunarView      *view,
                                 ThunarListModel *model)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (view);

  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));
  g_return_if_fail (model == NULL || THUNAR_IS_LIST_MODEL (model));

  /* disconnect from the previously set model */
  if (icon_view->list_model != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (icon_view->list_model),
                                            thunar_icon_view_update, icon_view);
      g_object_unref (G_OBJECT (icon_view->list_model));
    }

  icon_view->list_model = model;

  /* connect to the new model */
  if (model != NULL)
    {
      g_object_ref (G_OBJECT (model));

      /* we have to update the "statusbar-text" everytime the number of rows changes */
      g_signal_connect_swapped (G_OBJECT (model), "row-deleted",
                                G_CALLBACK (thunar_icon_view_update), icon_view);
      g_signal_connect_swapped (G_OBJECT (model), "row-inserted",
                                G_CALLBACK (thunar_icon_view_update), icon_view);
    }

  /* forward the model-change request to the ExoIconView */
  exo_icon_view_set_model (EXO_ICON_VIEW (view), (GtkTreeModel *) model);

  /* notify listeners */
  g_object_notify (G_OBJECT (view), "list-model");

  /* update the statusbar-text */
  thunar_icon_view_update (icon_view);
}



static const gchar*
thunar_icon_view_get_statusbar_text (ThunarView *view)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (view);
  GList          *items;

  /* generate the statusbar text on demand */
  if (icon_view->statusbar_text == NULL)
    {
      /* query the selected items (actually a list of GtkTreePath's) */
      if (G_UNLIKELY (icon_view->list_model == NULL))
        icon_view->statusbar_text = g_strdup ("");
      else
        {
          items = exo_icon_view_get_selected_items (EXO_ICON_VIEW (icon_view));
          icon_view->statusbar_text =
            thunar_list_model_get_statusbar_text (icon_view->list_model, items);
          g_list_foreach (items, (GFunc) gtk_tree_path_free, NULL);
          g_list_free (items);
        }
    }

  return icon_view->statusbar_text;
}



static void
thunar_icon_view_selection_changed (ExoIconView *view)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (view));

  /* update the status text */
  thunar_icon_view_update (THUNAR_ICON_VIEW (view));

  if (EXO_ICON_VIEW_CLASS (thunar_icon_view_parent_class)->selection_changed != NULL)
    EXO_ICON_VIEW_CLASS (thunar_icon_view_parent_class)->selection_changed (view);
}



static void
thunar_icon_view_update (ThunarIconView *icon_view)
{
  g_return_if_fail (THUNAR_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->update_idle_id < 0))
    {
      icon_view->update_idle_id =
        g_idle_add_full (G_PRIORITY_LOW, thunar_icon_view_update_idle,
                         icon_view, thunar_icon_view_update_idle_destroy);
    }
}



static gboolean
thunar_icon_view_update_idle (gpointer user_data)
{
  ThunarIconView *icon_view = THUNAR_ICON_VIEW (user_data);

  g_return_val_if_fail (THUNAR_IS_ICON_VIEW (icon_view), FALSE);

  GDK_THREADS_ENTER ();

  /* free the previous text (will be automatically regenerated) */
  g_free (icon_view->statusbar_text);
  icon_view->statusbar_text = NULL;

  /* notify interested modules (esp. the status bar) */
  g_object_notify (G_OBJECT (icon_view), "statusbar-text");

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_icon_view_update_idle_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_ICON_VIEW (user_data)->update_idle_id = -1;
  GDK_THREADS_LEAVE ();
}



/**
 * thunar_icon_view_new:
 *
 * Allocates a new #ThunarIconView instance, which is not
 * associated with any #ThunarListModel yet.
 *
 * Return value: the newly allocated #ThunarIconView instance.
 **/
GtkWidget*
thunar_icon_view_new (void)
{
  return g_object_new (THUNAR_TYPE_ICON_VIEW, NULL);
}


