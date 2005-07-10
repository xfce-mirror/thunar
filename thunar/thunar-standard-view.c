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

#include <thunar/thunar-standard-view.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_LOADING,
  PROP_STATUSBAR_TEXT,
};



static void         thunar_standard_view_class_init             (ThunarStandardViewClass  *klass);
static void         thunar_standard_view_navigator_init         (ThunarNavigatorIface     *iface);
static void         thunar_standard_view_view_init              (ThunarViewIface          *iface);
static void         thunar_standard_view_init                   (ThunarStandardView       *standard_view);
static GObject     *thunar_standard_view_constructor            (GType                     type,
                                                                 guint                     n_construct_properties,
                                                                 GObjectConstructParam    *construct_properties);
static void         thunar_standard_view_dispose                (GObject                  *object);
static void         thunar_standard_view_finalize               (GObject                  *object);
static void         thunar_standard_view_get_property           (GObject                  *object,
                                                                 guint                     prop_id,
                                                                 GValue                   *value,
                                                                 GParamSpec               *pspec);
static void         thunar_standard_view_set_property           (GObject                  *object,
                                                                 guint                     prop_id,
                                                                 const GValue             *value,
                                                                 GParamSpec               *pspec);
static ThunarFile  *thunar_standard_view_get_current_directory  (ThunarNavigator          *navigator);
static void         thunar_standard_view_set_current_directory  (ThunarNavigator          *navigator,
                                                                 ThunarFile               *current_directory);
static gboolean     thunar_standard_view_get_loading            (ThunarView               *view);
static const gchar *thunar_standard_view_get_statusbar_text     (ThunarView               *view);
static gboolean     thunar_standard_view_loading_idle           (gpointer                  user_data);
static void         thunar_standard_view_loading_idle_destroy   (gpointer                  user_data);



static GObjectClass *thunar_standard_view_parent_class;



GType
thunar_standard_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarStandardViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_standard_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarStandardView),
        0,
        (GInstanceInitFunc) thunar_standard_view_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_standard_view_navigator_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo view_info =
      {
        (GInterfaceInitFunc) thunar_standard_view_view_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW,
                                     "ThunarStandardView",
                                     &info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_VIEW, &view_info);
    }

  return type;
}



static void
thunar_standard_view_class_init (ThunarStandardViewClass *klass)
{
  GObjectClass *gobject_class;

  thunar_standard_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor = thunar_standard_view_constructor;
  gobject_class->dispose = thunar_standard_view_dispose;
  gobject_class->finalize = thunar_standard_view_finalize;
  gobject_class->get_property = thunar_standard_view_get_property;
  gobject_class->set_property = thunar_standard_view_set_property;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");

  g_object_class_override_property (gobject_class,
                                    PROP_LOADING,
                                    "loading");
  g_object_class_override_property (gobject_class,
                                    PROP_STATUSBAR_TEXT,
                                    "statusbar-text");
}



static void
thunar_standard_view_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_standard_view_get_current_directory;
  iface->set_current_directory = thunar_standard_view_set_current_directory;
}



static void
thunar_standard_view_view_init (ThunarViewIface *iface)
{
  iface->get_loading = thunar_standard_view_get_loading;
  iface->get_statusbar_text = thunar_standard_view_get_statusbar_text;
}



static void
thunar_standard_view_init (ThunarStandardView *standard_view)
{
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (standard_view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);

  standard_view->model = thunar_list_model_new ();
  standard_view->loading_idle_id = -1;

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files",
                            G_CALLBACK (thunar_standard_view_selection_changed), standard_view);
}



static GObject*
thunar_standard_view_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  GObject *object;

  /* let the GObject constructor create the instance */
  object = G_OBJECT_CLASS (thunar_standard_view_parent_class)->constructor (type,
                                                                            n_construct_properties,
                                                                            construct_properties);

  /* apply our list model to the real view (the child of the scrolled window),
   * we therefore assume that all real views have the "model" property.
   */
  g_object_set (G_OBJECT (GTK_BIN (object)->child),
                "model", THUNAR_STANDARD_VIEW (object)->model,
                NULL);

  /* done, we have a working object */
  return object;
}



static void
thunar_standard_view_dispose (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* be sure to cancel any loading idle source */
  if (G_UNLIKELY (standard_view->loading_idle_id >= 0))
    g_source_remove (standard_view->loading_idle_id);

  /* reset the model's folder */
  thunar_list_model_set_folder (standard_view->model, NULL);

  G_OBJECT_CLASS (thunar_standard_view_parent_class)->dispose (object);
}



static void
thunar_standard_view_finalize (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* release the reference on the list model */
  g_object_unref (G_OBJECT (standard_view->model));
  
  /* free the statusbar text (if any) */
  g_free (standard_view->statusbar_text);

  G_OBJECT_CLASS (thunar_standard_view_parent_class)->finalize (object);
}



static void
thunar_standard_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, thunar_view_get_loading (THUNAR_VIEW (object)));
      break;

    case PROP_STATUSBAR_TEXT:
      g_value_set_static_string (value, thunar_view_get_statusbar_text (THUNAR_VIEW (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_standard_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (navigator, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_standard_view_get_current_directory (ThunarNavigator *navigator)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  ThunarFolder       *folder;

  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  /* try to determine the current folder from the model */
  folder = thunar_list_model_get_folder (standard_view->model);
  if (G_LIKELY (folder != NULL))
    return thunar_folder_get_corresponding_file (folder);

  return NULL;
}



static void
thunar_standard_view_set_current_directory (ThunarNavigator *navigator,
                                            ThunarFile      *current_directory)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  ThunarFolder       *folder;
  GtkWidget          *toplevel;
  GtkWidget          *dialog;
  GError             *error = NULL;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* check if we want to reset the directory */
  if (G_UNLIKELY (current_directory == NULL))
    {
      thunar_list_model_set_folder (standard_view->model, NULL);
      return;
    }

  /* setup the folder for the view, we use a simple but very effective
   * trick here to speed up the folder change: we completely disconnect
   * the model from the view, load the folder into the model and afterwards
   * reconnect the model with the view. This way we avoid having to fire
   * and process thousands of row_removed() and row_inserted() signals.
   * Instead the view can process the complete file list in the model
   * ONCE.
   */
  g_object_set (G_OBJECT (GTK_BIN (standard_view)->child),
                "model", NULL,
                NULL);

  /* give the real view some time to apply the new (not existing!) model */
  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* enter loading state (if not already in loading state) */
  if (G_LIKELY (standard_view->loading_idle_id < 0))
    {
      /* launch the idle function, which will reset the loading state */
      standard_view->loading_idle_id = g_idle_add_full (G_PRIORITY_LOW + 100, thunar_standard_view_loading_idle,
                                                        standard_view, thunar_standard_view_loading_idle_destroy);

      /* tell everybody that we are loading */
      g_object_notify (G_OBJECT (standard_view), "loading");
    }

  /* try to open the new directory */
  folder = thunar_file_open_as_folder (current_directory, &error);
  if (G_UNLIKELY (folder == NULL))
    {
      /* set an empty folder */
      thunar_list_model_set_folder (standard_view->model, NULL);

      /* query the toplevel window */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

      /* make sure the toplevel window is shown */
      gtk_widget_show_now (GTK_WIDGET (toplevel));

      /* display an error dialog */
      dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "Failed to open directory `%s': %s",
                                       thunar_file_get_display_name (current_directory),
                                       error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      /* free the error details */
      g_error_free (error);
    }
  else
    {
      /* apply the new folder */
      thunar_list_model_set_folder (standard_view->model, folder);
      g_object_unref (G_OBJECT (folder));
    }

  /* reset the model on the real view */
  g_object_set (G_OBJECT (GTK_BIN (standard_view)->child),
                "model", standard_view->model,
                NULL);

  /* notify all listeners about the new/old current directory */
  g_object_notify (G_OBJECT (standard_view), "current-directory");
}



static gboolean
thunar_standard_view_get_loading (ThunarView *view)
{
  return (THUNAR_STANDARD_VIEW (view)->loading_idle_id >= 0);
}



static const gchar*
thunar_standard_view_get_statusbar_text (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GList              *items;

  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  /* generate the statusbar text on-demand */
  if (standard_view->statusbar_text == NULL)
    {
      /* query the selected items (actually a list of GtkTreePath's) */
      items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);
      standard_view->statusbar_text = thunar_list_model_get_statusbar_text (standard_view->model, items);
      g_list_foreach (items, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (items);
    }

  return standard_view->statusbar_text;
}



static gboolean
thunar_standard_view_loading_idle (gpointer user_data)
{
  /* we just return FALSE here, and let the thunar_standard_view_loading_idle_destroy()
   * method do the real work for us.
   */
  return FALSE;
}



static void
thunar_standard_view_loading_idle_destroy (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);

  GDK_THREADS_ENTER ();

  /* reset the loading state... */
  standard_view->loading_idle_id = -1;

  /* ...and tell everybody */
  g_object_notify (G_OBJECT (standard_view), "loading");

  GDK_THREADS_LEAVE ();
}



/**
 * thunar_standard_view_selection_changed:
 * @standard_view : a #ThunarStandardView instance.
 *
 * Called by derived classes (and only by derived classes!) whenever the file
 * selection changes.
 *
 * Note, that this is also called internally whenever the number of
 * files in the @standard_view<!---->s model changes.
 **/
void
thunar_standard_view_selection_changed (ThunarStandardView *standard_view)
{
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* clear the current status text (will be recalculated on-demand) */
  g_free (standard_view->statusbar_text);
  standard_view->statusbar_text = NULL;

  /* tell everybody that the statusbar text may have changed */
  g_object_notify (G_OBJECT (standard_view), "statusbar-text");
}



