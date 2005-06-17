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

#include <thunar/thunar-statusbar.h>



enum
{
  PROP_0,
  PROP_VIEW,
};



static void thunar_statusbar_class_init   (ThunarStatusbarClass *klass);
static void thunar_statusbar_init         (ThunarStatusbar      *statusbar);
static void thunar_statusbar_dispose      (GObject              *object);
static void thunar_statusbar_get_property (GObject              *object,
                                           guint                 prop_id,
                                           GValue               *value,
                                           GParamSpec           *pspec);
static void thunar_statusbar_set_property (GObject              *object,
                                           guint                 prop_id,
                                           const GValue         *value,
                                           GParamSpec           *pspec);
static void thunar_statusbar_update       (ThunarStatusbar      *statusbar);



struct _ThunarStatusbarClass
{
  GtkStatusbarClass __parent__;
};

struct _ThunarStatusbar
{
  GtkStatusbar __parent__;
  guint        context_id;
  ThunarView  *view;
};



G_DEFINE_TYPE (ThunarStatusbar, thunar_statusbar, GTK_TYPE_STATUSBAR);



static void
thunar_statusbar_class_init (ThunarStatusbarClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_statusbar_dispose;
  gobject_class->get_property = thunar_statusbar_get_property;
  gobject_class->set_property = thunar_statusbar_set_property;

  /**
   * ThunarStatusbar:view:
   *
   * The main view instance this statusbar is connected to.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_VIEW,
                                   g_param_spec_object ("view",
                                                        _("View"),
                                                        _("The main view connected to the statusbar"),
                                                        THUNAR_TYPE_VIEW,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_statusbar_init (ThunarStatusbar *statusbar)
{
  statusbar->context_id =
    gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Main text");
  gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (statusbar), TRUE);
}



static void
thunar_statusbar_dispose (GObject *object)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);
  thunar_statusbar_set_view (statusbar, NULL);
  G_OBJECT_CLASS (thunar_statusbar_parent_class)->dispose (object);
}



static void
thunar_statusbar_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, thunar_statusbar_get_view (statusbar));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_statusbar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      thunar_statusbar_set_view (statusbar, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_statusbar_update (ThunarStatusbar *statusbar)
{
  ThunarVfsFileSize size;
  ThunarListModel  *model;
  ThunarFile       *file;
  GList            *selected_files;
  GList            *lp;
  gchar            *size_string;
  gchar            *text;
  gint              n;
  
  /* drop the previous status text */
  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), statusbar->context_id);

  /* we cannot display anything useful without a view */
  if (G_UNLIKELY (statusbar->view == NULL))
    return;

  /* query the model associated with the view */
  model = thunar_view_get_model (statusbar->view);
  if (G_UNLIKELY (model == NULL))
    return;

  /* determine the new status text based on the selected files */
  selected_files = thunar_view_get_selected_files (statusbar->view);
  if (selected_files == NULL)
    {
      text = g_strdup_printf (_("%d items"), thunar_list_model_get_num_files (model));
    }
  else if (selected_files->next == NULL)
    {
      file = THUNAR_FILE (selected_files->data);
      size_string = thunar_file_get_size_string (file);
      text = g_strdup_printf (_("\"%s\" (%s) %s"), thunar_file_get_display_name (file), size_string,
                              exo_mime_info_get_comment (thunar_file_get_mime_info (file)));
      g_free (size_string);
    }
  else
    {
      /* sum up all sizes */
      for (lp = selected_files, n = 0, size = 0; lp != NULL; lp = lp->next, ++n)
        size += thunar_file_get_size (THUNAR_FILE (lp->data));

      size_string = thunar_vfs_humanize_size (size, NULL, 0);
      text = g_strdup_printf (_("%d items selected (%s)"), n, size_string);
      g_free (size_string);
    }

  /* free the list of selected files */
  g_list_foreach (selected_files, (GFunc)g_object_unref, NULL);
  g_list_free (selected_files);

  /* display the new status text */
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), statusbar->context_id, text);
  g_free (text);
}



/**
 * thunar_statusbar_new:
 *
 * Allocates a new #ThunarStatusbar instance with no
 * text set.
 *
 * Return value: the newly allocated #ThunarStatusbar instance.
 **/
GtkWidget*
thunar_statusbar_new (void)
{
  return g_object_new (THUNAR_TYPE_STATUSBAR, NULL);
}



/**
 * thunar_statusbar_get_view:
 * @statusbar : a #ThunarStatusbar instance.
 *
 * Returns the #ThunarView instance connected to the @statusbar
 * or %NULL if no view is connected to @statusbar.
 *
 * Return value: the view connected to @statusbar.
 **/
ThunarView*
thunar_statusbar_get_view (ThunarStatusbar *statusbar)
{
  g_return_val_if_fail (THUNAR_IS_STATUSBAR (statusbar), NULL);
  return statusbar->view;
}



/**
 * thunar_statusbar_set_view:
 * @statusbar : a #ThunarStatusbar instance.
 * @view      : a #ThunarView instance or %NULL.
 *
 * Connects the @statusbar to the given @view, so that @statusbar
 * will display the current status of @view or nothing if @view
 * is %NULL.
 **/
void
thunar_statusbar_set_view (ThunarStatusbar *statusbar,
                           ThunarView      *view)
{
  g_return_if_fail (THUNAR_IS_STATUSBAR (statusbar));
  g_return_if_fail (view == NULL || THUNAR_IS_VIEW (view));

  if (G_UNLIKELY (statusbar->view == view))
    return;

  /* disconnect from the previous view */
  if (statusbar->view != NULL)
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (statusbar->view),
                                            G_SIGNAL_MATCH_DATA, 0, 0,
                                            NULL, NULL, statusbar);
      g_object_unref (G_OBJECT (statusbar->view));
    }

  /* activate the new view */
  statusbar->view = view;

  /* connect to the new view */
  if (view != NULL)
    {
      g_object_ref (G_OBJECT (view));

      g_signal_connect_swapped (G_OBJECT (view), "file-selection-changed",
                                G_CALLBACK (thunar_statusbar_update), statusbar);
      g_signal_connect_swapped (G_OBJECT (view), "notify::model",
                                G_CALLBACK (thunar_statusbar_update), statusbar);

      thunar_statusbar_update (statusbar);
    }

  /* notify other modules */
  g_object_notify (G_OBJECT (statusbar), "view");
}



