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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunarx/thunarx-gtk-extensions.h>

#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-standard-view-ui.h>



#define THUNAR_STANDARD_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_LOADING,
  PROP_STATUSBAR_TEXT,
  PROP_UI_MANAGER,
};

/* Identifiers for DnD target types */
enum
{
  TEXT_URI_LIST,
};



static void          thunar_standard_view_class_init                (ThunarStandardViewClass  *klass);
static void          thunar_standard_view_navigator_init            (ThunarNavigatorIface     *iface);
static void          thunar_standard_view_view_init                 (ThunarViewIface          *iface);
static void          thunar_standard_view_init                      (ThunarStandardView       *standard_view);
static GObject      *thunar_standard_view_constructor               (GType                     type,
                                                                     guint                     n_construct_properties,
                                                                     GObjectConstructParam    *construct_properties);
static void          thunar_standard_view_dispose                   (GObject                  *object);
static void          thunar_standard_view_finalize                  (GObject                  *object);
static void          thunar_standard_view_get_property              (GObject                  *object,
                                                                     guint                     prop_id,
                                                                     GValue                   *value,
                                                                     GParamSpec               *pspec);
static void          thunar_standard_view_set_property              (GObject                  *object,
                                                                     guint                     prop_id,
                                                                     const GValue             *value,
                                                                     GParamSpec               *pspec);
static void          thunar_standard_view_realize                   (GtkWidget                *widget);
static void          thunar_standard_view_unrealize                 (GtkWidget                *widget);
static void          thunar_standard_view_grab_focus                (GtkWidget                *widget);
static ThunarFile   *thunar_standard_view_get_current_directory     (ThunarNavigator          *navigator);
static void          thunar_standard_view_set_current_directory     (ThunarNavigator          *navigator,
                                                                     ThunarFile               *current_directory);
static gboolean      thunar_standard_view_get_loading               (ThunarView               *view);
static const gchar  *thunar_standard_view_get_statusbar_text        (ThunarView               *view);
static GtkUIManager *thunar_standard_view_get_ui_manager            (ThunarView               *view);
static void          thunar_standard_view_set_ui_manager            (ThunarView               *view,
                                                                     GtkUIManager             *ui_manager);
static GList        *thunar_standard_view_get_selected_files        (ThunarStandardView       *standard_view);
static GList        *thunar_standard_view_get_selected_uris         (ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_properties         (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_copy               (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_cut                (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_paste              (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_paste_into_folder  (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_select_all_files   (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_select_by_pattern  (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_show_hidden_files  (GtkToggleAction          *toggle_action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_begin                (GtkWidget                *widget,
                                                                     GdkDragContext           *context,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_data_get             (GtkWidget                *widget,
                                                                     GdkDragContext           *context,
                                                                     GtkSelectionData         *selection_data,
                                                                     guint                     info,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_loading_unbound           (gpointer                  user_data);



struct _ThunarStandardViewPrivate
{
  ThunarLauncher *launcher;
  GtkAction      *action_properties;
  GtkAction      *action_copy;
  GtkAction      *action_cut;
  GtkAction      *action_paste;
  GtkAction      *action_paste_into_folder;
  GtkAction      *action_select_all_files;
  GtkAction      *action_select_by_pattern;
  GtkAction      *action_show_hidden_files;
};



static const GtkActionEntry action_entries[] =
{
  { "file-context-menu", NULL, N_ ("File Context Menu"), NULL, NULL, NULL, },
  { "folder-context-menu", NULL, N_ ("Folder Context Menu"), NULL, NULL, NULL, },
  { "properties", GTK_STOCK_PROPERTIES, N_ ("_Properties"), "<alt>Return", N_ ("View the properties of the selected item"), G_CALLBACK (thunar_standard_view_action_properties), },
  { "copy", GTK_STOCK_COPY, N_ ("_Copy files"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_copy), },
  { "cut", GTK_STOCK_CUT, N_ ("Cu_t files"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_cut), },
  { "paste", GTK_STOCK_PASTE, N_ ("_Paste files"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_paste), },
  { "paste-into-folder", GTK_STOCK_PASTE, N_ ("Paste files into folder"), NULL, N_ ("Paste files into the selected folder"), G_CALLBACK (thunar_standard_view_action_paste_into_folder), },
  { "select-all-files", NULL, N_ ("Select _all files"), "<control>A", N_ ("Select all files in this window"), G_CALLBACK (thunar_standard_view_action_select_all_files), },
  { "select-by-pattern", NULL, N_ ("Select by _pattern"), "<control>S", N_ ("Select all files that match a certain pattern"), G_CALLBACK (thunar_standard_view_action_select_by_pattern), },
};

static const GtkToggleActionEntry toggle_action_entries[] =
{
  { "show-hidden-files", NULL, N_("Show _hidden files"), NULL, N_("Toggles the display of hidden files in the current window"), G_CALLBACK (thunar_standard_view_action_show_hidden_files), FALSE, },
};

/* Target types for dragging from the view */
static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, TEXT_URI_LIST, },
};

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
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  g_type_class_add_private (klass, sizeof (ThunarStandardViewPrivate));

  thunar_standard_view_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor = thunar_standard_view_constructor;
  gobject_class->dispose = thunar_standard_view_dispose;
  gobject_class->finalize = thunar_standard_view_finalize;
  gobject_class->get_property = thunar_standard_view_get_property;
  gobject_class->set_property = thunar_standard_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_standard_view_realize;
  gtkwidget_class->unrealize = thunar_standard_view_unrealize;
  gtkwidget_class->grab_focus = thunar_standard_view_grab_focus;

  klass->connect_ui_manager = (gpointer) exo_noop;
  klass->disconnect_ui_manager = (gpointer) exo_noop;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");

  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_override ("loading",
                                                          g_param_spec_boolean ("loading",
                                                                                _("Loading"),
                                                                                _("Whether the view is currently being loaded"),
                                                                                FALSE,
                                                                                EXO_PARAM_READWRITE)));

  g_object_class_override_property (gobject_class,
                                    PROP_STATUSBAR_TEXT,
                                    "statusbar-text");
  g_object_class_override_property (gobject_class,
                                    PROP_UI_MANAGER,
                                    "ui-manager");
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
  iface->get_ui_manager = thunar_standard_view_get_ui_manager;
  iface->set_ui_manager = thunar_standard_view_set_ui_manager;
}



static void
thunar_standard_view_init (ThunarStandardView *standard_view)
{
  standard_view->priv = THUNAR_STANDARD_VIEW_GET_PRIVATE (standard_view);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (standard_view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (standard_view), GTK_SHADOW_IN);

  /* setup the action group for this view */
  standard_view->action_group = gtk_action_group_new ("thunar-standard-view");
  gtk_action_group_set_translation_domain (standard_view->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (standard_view->action_group, action_entries,
                                G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (standard_view));
  gtk_action_group_add_toggle_actions (standard_view->action_group, toggle_action_entries,
                                       G_N_ELEMENTS (toggle_action_entries),
                                       GTK_WIDGET (standard_view));

  /* lookup all actions to speed up access later */
  standard_view->priv->action_properties = gtk_action_group_get_action (standard_view->action_group, "properties");
  standard_view->priv->action_copy = gtk_action_group_get_action (standard_view->action_group, "copy");
  standard_view->priv->action_cut = gtk_action_group_get_action (standard_view->action_group, "cut");
  standard_view->priv->action_paste = gtk_action_group_get_action (standard_view->action_group, "paste");
  standard_view->priv->action_paste_into_folder = gtk_action_group_get_action (standard_view->action_group, "paste-into-folder");
  standard_view->priv->action_select_all_files = gtk_action_group_get_action (standard_view->action_group, "select-all-files");
  standard_view->priv->action_select_by_pattern = gtk_action_group_get_action (standard_view->action_group, "select-by-pattern");
  standard_view->priv->action_show_hidden_files = gtk_action_group_get_action (standard_view->action_group, "show-hidden-files");

  /* setup the list model */
  standard_view->model = thunar_list_model_new ();

  /* setup the icon renderer */
  standard_view->icon_renderer = thunar_icon_renderer_new ();
  g_object_ref (G_OBJECT (standard_view->icon_renderer));
  gtk_object_sink (GTK_OBJECT (standard_view->icon_renderer));

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files",
                            G_CALLBACK (thunar_standard_view_selection_changed), standard_view);

  /* allocate the launcher support */
  standard_view->priv->launcher = thunar_launcher_new ();
  thunar_launcher_set_action_group (standard_view->priv->launcher, standard_view->action_group);
  thunar_launcher_set_widget (standard_view->priv->launcher, GTK_WIDGET (standard_view));
  g_signal_connect_swapped (G_OBJECT (standard_view->priv->launcher), "open-directory", G_CALLBACK (thunar_navigator_change_directory), standard_view);
}



static GObject*
thunar_standard_view_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  GtkWidget *widget;
  GObject   *object;

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

  /* determine the real view widget (treeview or iconview) */
  widget = GTK_BIN (object)->child;

  /* setup the real view as drag source */
  gtk_drag_source_set (widget, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                       drag_targets, G_N_ELEMENTS (drag_targets),
                       GDK_ACTION_COPY | GDK_ACTION_MOVE
                       | GDK_ACTION_LINK | GDK_ACTION_ASK);
  g_signal_connect (G_OBJECT (widget), "drag-begin", G_CALLBACK (thunar_standard_view_drag_begin), object);
  g_signal_connect (G_OBJECT (widget), "drag-data-get", G_CALLBACK (thunar_standard_view_drag_data_get), object);

  /* done, we have a working object */
  return object;
}



static void
thunar_standard_view_dispose (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* unregister the "loading" binding */
  if (G_UNLIKELY (standard_view->loading_binding != NULL))
    exo_binding_unbind (standard_view->loading_binding);

  /* reset the UI manager property */
  thunar_view_set_ui_manager (THUNAR_VIEW (standard_view), NULL);

  /* disconnect the widget from the launcher support */
  thunar_launcher_set_widget (standard_view->priv->launcher, NULL);

  G_OBJECT_CLASS (thunar_standard_view_parent_class)->dispose (object);
}



static void
thunar_standard_view_finalize (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* some safety checks */
  g_assert (standard_view->loading_binding == NULL);
  g_assert (standard_view->icon_factory == NULL);
  g_assert (standard_view->ui_manager == NULL);
  g_assert (standard_view->clipboard == NULL);

  /* release the reference on the icon renderer */
  g_object_unref (G_OBJECT (standard_view->icon_renderer));

  /* release the reference on the action group */
  g_object_unref (G_OBJECT (standard_view->action_group));

  /* release the reference on the launcher */
  g_object_unref (G_OBJECT (standard_view->priv->launcher));

  /* disconnect from the list model */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->model), thunar_standard_view_selection_changed, standard_view);
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

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_view_get_ui_manager (THUNAR_VIEW (object)));
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
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);
  gboolean            loading;

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_LOADING:
      loading = g_value_get_boolean (value);
      if (G_LIKELY (loading != standard_view->loading))
        {
          standard_view->loading = loading;
          g_object_notify (object, "loading");
          g_object_notify (object, "statusbar-text");
        }
      break;

    case PROP_UI_MANAGER:
      thunar_view_set_ui_manager (THUNAR_VIEW (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_standard_view_realize (GtkWidget *widget)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (widget);
  GtkIconTheme       *icon_theme;
  GdkDisplay         *display;

  /* let the GtkWidget do its work */
  GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->realize (widget);

  /* query the clipboard manager for the display */
  display = gtk_widget_get_display (widget);
  standard_view->clipboard = thunar_clipboard_manager_get_for_display (display);

  /* we need update the selection state based on the clipboard content */
  g_signal_connect_swapped (G_OBJECT (standard_view->clipboard), "changed",
                            G_CALLBACK (thunar_standard_view_selection_changed), standard_view);
  thunar_standard_view_selection_changed (standard_view);

  /* determine the icon factory for the screen on which we are realized */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  standard_view->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
}



static void
thunar_standard_view_unrealize (GtkWidget *widget)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (widget);

  /* disconnect the clipboard changed handler */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->clipboard), thunar_standard_view_selection_changed, standard_view);

  /* drop the reference on the icon factory */
  g_object_unref (G_OBJECT (standard_view->icon_factory));
  standard_view->icon_factory = NULL;

  /* drop the reference on the clipboard manager */
  g_object_unref (G_OBJECT (standard_view->clipboard));
  standard_view->clipboard = NULL;

  /* let the GtkWidget do its work */
  GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->unrealize (widget);
}



static void
thunar_standard_view_grab_focus (GtkWidget *widget)
{
  /* forward the focus grab to the real view */
  gtk_widget_grab_focus (GTK_BIN (widget)->child);
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

  /* disconnect any previous "loading" binding */
  if (G_LIKELY (standard_view->loading_binding != NULL))
    exo_binding_unbind (standard_view->loading_binding);

  /* check if we want to reset the directory */
  if (G_UNLIKELY (current_directory == NULL))
    {
      thunar_list_model_set_folder (standard_view->model, NULL);
      return;
    }

  /* We drop the model from the view as a simple optimization to speed up
   * the process of disconnecting the model data from the view.
   */
  g_object_set (G_OBJECT (GTK_BIN (standard_view)->child), "model", NULL, NULL);

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
      /* connect the "loading" binding */
      standard_view->loading_binding = exo_binding_new_full (G_OBJECT (folder), "loading",
                                                             G_OBJECT (standard_view), "loading",
                                                             NULL, thunar_standard_view_loading_unbound,
                                                             standard_view);

      /* apply the new folder */
      thunar_list_model_set_folder (standard_view->model, folder);
      g_object_unref (G_OBJECT (folder));
    }

  /* reconnect our model to the view */
  g_object_set (G_OBJECT (GTK_BIN (standard_view)->child), "model", standard_view->model, NULL);

  /* notify all listeners about the new/old current directory */
  g_object_notify (G_OBJECT (standard_view), "current-directory");
}



static gboolean
thunar_standard_view_get_loading (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->loading;
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

      /* we display a loading text if no items are
       * selected and the view is loading
       */
      if (items == NULL && standard_view->loading)
        return _("Loading folder contents...");

      standard_view->statusbar_text = thunar_list_model_get_statusbar_text (standard_view->model, items);
      g_list_foreach (items, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (items);
    }

  return standard_view->statusbar_text;
}



static GtkUIManager*
thunar_standard_view_get_ui_manager (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->ui_manager;
}



static void
thunar_standard_view_set_ui_manager (ThunarView   *view,
                                     GtkUIManager *ui_manager)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GError             *error = NULL;

  /* disconnect from the previous UI manager */
  if (G_LIKELY (standard_view->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (standard_view->ui_manager, standard_view->action_group);

      /* unmerge the ui controls from derived classes */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->disconnect_ui_manager) (standard_view, standard_view->ui_manager);

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (standard_view->ui_manager, standard_view->ui_merge_id);

      /* drop the reference on the previous UI manager */
      g_object_unref (G_OBJECT (standard_view->ui_manager));
    }

  /* apply the new setting */
  standard_view->ui_manager = ui_manager;

  /* connect to the new manager (if any) */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, standard_view->action_group, -1);

      /* merge our UI control items with the new manager */
      standard_view->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_standard_view_ui,
                                                                      thunar_standard_view_ui_length, &error);
      if (G_UNLIKELY (standard_view->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarStandardView menus: %s", error->message);
          g_error_free (error);
        }

      /* merge the ui controls from derived classes */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->connect_ui_manager) (standard_view, ui_manager);
    }

  /* let others know that we have a new manager */
  g_object_notify (G_OBJECT (view), "ui-manager");
}



static GList*
thunar_standard_view_get_selected_files (ThunarStandardView *standard_view)
{
  GtkTreeIter iter;
  ThunarFile *file;
  GList      *selected_items;
  GList      *selected_files = NULL;
  GList      *lp;

  selected_items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);
  for (lp = selected_items; lp != NULL; lp = lp->next)
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);
      file = thunar_list_model_get_file (standard_view->model, &iter);
      selected_files = g_list_append (selected_files, file);
      gtk_tree_path_free (lp->data);
    }
  g_list_free (selected_items);

  return selected_files;
}



static GList*
thunar_standard_view_get_selected_uris (ThunarStandardView *standard_view)
{
  GtkTreeIter iter;
  ThunarFile *file;
  GList      *selected_items;
  GList      *selected_uris = NULL;
  GList      *lp;

  selected_items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);
  for (lp = selected_items; lp != NULL; lp = lp->next)
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);
      file = thunar_list_model_get_file (standard_view->model, &iter);
      selected_uris = thunar_vfs_uri_list_append (selected_uris, thunar_file_get_uri (file));
      g_object_unref (G_OBJECT (file));
      gtk_tree_path_free (lp->data);
    }
  g_list_free (selected_items);

  return selected_uris;
}



static void
thunar_standard_view_action_properties (GtkAction          *action,
                                        ThunarStandardView *standard_view)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;
  GList     *files;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  files = thunar_standard_view_get_selected_files (standard_view);
  if (G_LIKELY (g_list_length (files) == 1))
    {
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
      if (G_LIKELY (toplevel != NULL))
        {
          dialog = g_object_new (THUNAR_TYPE_PROPERTIES_DIALOG,
                                 "destroy-with-parent", TRUE,
                                 "file", files->data,
                                 NULL);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
          gtk_widget_show (dialog);
        }
    }

  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);
}



static void
thunar_standard_view_action_copy (GtkAction          *action,
                                  ThunarStandardView *standard_view)
{
  GList *uri_list;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  uri_list = thunar_standard_view_get_selected_uris (standard_view);
  if (G_LIKELY (uri_list != NULL))
    {
      thunar_clipboard_manager_copy_uri_list (standard_view->clipboard, uri_list);
      thunar_vfs_uri_list_free (uri_list);
    }
}



static void
thunar_standard_view_action_cut (GtkAction          *action,
                                 ThunarStandardView *standard_view)
{
  GList *uri_list;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  uri_list = thunar_standard_view_get_selected_uris (standard_view);
  if (G_LIKELY (uri_list != NULL))
    {
      thunar_clipboard_manager_cut_uri_list (standard_view->clipboard, uri_list);
      thunar_vfs_uri_list_free (uri_list);
    }
}



static void
thunar_standard_view_action_paste (GtkAction          *action,
                                   ThunarStandardView *standard_view)
{
  ThunarFile *current_directory;
  GtkWidget  *window;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
      thunar_clipboard_manager_paste_uri_list (standard_view->clipboard, thunar_file_get_uri (current_directory), GTK_WINDOW (window));
    }
}



static void
thunar_standard_view_action_paste_into_folder (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  ThunarFile *file;
  GtkWidget  *window;
  GList      *files;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the first selected directory and paste into it */
  files = thunar_standard_view_get_selected_files (standard_view);
  file = (files != NULL) ? THUNAR_FILE (files->data) : NULL;
  if (G_LIKELY (file != NULL && thunar_file_is_directory (file)))
    {
      window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
      thunar_clipboard_manager_paste_uri_list (standard_view->clipboard, thunar_file_get_uri (file), GTK_WINDOW (window));
    }
  g_list_foreach (files, (GFunc) g_object_unref, NULL);
  g_list_free (files);
}



static void
thunar_standard_view_action_select_all_files (GtkAction          *action,
                                              ThunarStandardView *standard_view)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_all (standard_view);
}



static void
thunar_standard_view_action_select_by_pattern (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  GtkWidget *window;
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GList     *paths;
  GList     *lp;
  gint       response;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  dialog = gtk_dialog_new_with_buttons (_("Select by pattern"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

  hbox = g_object_new (GTK_TYPE_HBOX, "border-width", 6, "spacing", 10, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Pattern:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = g_object_new (GTK_TYPE_ENTRY, "activates-default", TRUE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_OK)
    {
      /* select all files that match the entered pattern */
      paths = thunar_list_model_get_paths_for_pattern (standard_view->model, gtk_entry_get_text (GTK_ENTRY (entry)));
      THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all (standard_view);
      for (lp = paths; lp != NULL; lp = lp->next)
        {
          THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path (standard_view, lp->data);
          gtk_tree_path_free (lp->data);
        }
      g_list_free (paths);
    }

  gtk_widget_destroy (dialog);
}



static void
thunar_standard_view_action_show_hidden_files (GtkToggleAction    *toggle_action,
                                               ThunarStandardView *standard_view)
{
  gboolean active;

  g_return_if_fail (GTK_IS_TOGGLE_ACTION (toggle_action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  active = gtk_toggle_action_get_active (toggle_action);
  thunar_list_model_set_show_hidden (standard_view->model, active);
}



static void
thunar_standard_view_drag_begin (GtkWidget          *widget,
                                 GdkDragContext     *context,
                                 ThunarStandardView *standard_view)
{
  GtkTreeIter iter;
  ThunarFile *file;
  GdkPixbuf  *icon;
  GList      *selected_items;
  gint        size;

  /* query the list of selected items */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  if (G_LIKELY (selected_items != NULL))
    {
      /* grab the tree iterator for the first selected item */
      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, selected_items->data))
        {
          /* determine the first selected file */
          file = thunar_list_model_get_file (THUNAR_LIST_MODEL (standard_view->model), &iter);
          if (G_LIKELY (file != NULL))
            {
              /* generate an icon based on that file */
              g_object_get (G_OBJECT (standard_view->icon_renderer), "size", &size, NULL);
              icon = thunar_file_load_icon (file, standard_view->icon_factory, size);
              gtk_drag_set_icon_pixbuf (context, icon, 0, 0);
              g_object_unref (G_OBJECT (icon));

              /* release the file */
              g_object_unref (G_OBJECT (file));
            }
        }

      /* release the selected items */
      g_list_foreach (selected_items, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (selected_items);
    }
}



static void
thunar_standard_view_drag_data_get (GtkWidget          *widget,
                                    GdkDragContext     *context,
                                    GtkSelectionData   *selection_data,
                                    guint               info,
                                    guint               time,
                                    ThunarStandardView *standard_view)
{
  gchar *uri_string;
  GList *uri_list;

  /* transform the list of selected URIs to a string */
  uri_list = thunar_standard_view_get_selected_uris (standard_view);
  uri_string = thunar_vfs_uri_list_to_string (uri_list, 0);
  thunar_vfs_uri_list_free (uri_list);

  /* set the URI list for the drag selection */
  gtk_selection_data_set (selection_data, selection_data->target, 8,
                          (guchar *) uri_string, strlen (uri_string));

  /* clean up */
  g_free (uri_string);
}



static void
thunar_standard_view_loading_unbound (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);

  /* we don't have any binding now */
  standard_view->loading_binding = NULL;

  /* reset the "loading" property */
  if (G_UNLIKELY (standard_view->loading))
    {
      standard_view->loading = FALSE;
      g_object_notify (G_OBJECT (standard_view), "loading");
      g_object_notify (G_OBJECT (standard_view), "statusbar-text");
    }
}



/**
 * thunar_standard_view_context_menu:
 * @standard_view : a #ThunarStandardView instance.
 * @button        : the mouse button which triggered the context menu or %0 if
 *                  the event wasn't triggered by a pointer device.
 * @time          : the event time.
 *
 * Invoked by derived classes (and only by derived classes!) whenever the user
 * requests to open a context menu, e.g. by right-clicking on a file/folder or by
 * using one of the context menu shortcuts.
 **/
void
thunar_standard_view_context_menu (ThunarStandardView *standard_view,
                                   guint               button,
                                   guint32             time)
{
  GMainLoop *loop;
  GtkWidget *menu;
  GList     *selected_items;
  guint      id;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check if we need to popup the file or the folder context menu */
  selected_items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);
  if (G_LIKELY (selected_items != NULL))
    menu = gtk_ui_manager_get_widget (standard_view->ui_manager, "/file-context-menu");
  else
    menu = gtk_ui_manager_get_widget (standard_view->ui_manager, "/folder-context-menu");
  g_list_foreach (selected_items, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (selected_items);

  /* take a reference on the context menu */
  g_object_ref (G_OBJECT (menu));
  gtk_object_sink (GTK_OBJECT (menu));

  /* grab an additional reference on the view */
  g_object_ref (G_OBJECT (standard_view));

  loop = g_main_loop_new (NULL, FALSE);

  /* connect the deactivate handler */
  id = g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);

  /* make sure the menu is on the proper screen */
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (standard_view)));

  /* run our custom main loop */
  gtk_grab_add (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  gtk_grab_remove (menu);

  /* unlink the deactivate callback */
  g_signal_handler_disconnect (G_OBJECT (menu), id);

  /* release the additional reference on the view */
  g_object_unref (G_OBJECT (standard_view));

  /* decrease the reference count on the menu */
  g_object_unref (G_OBJECT (menu));
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
  ThunarFile *current_directory;
  gboolean    can_paste_into_folder;
  gboolean    pastable;
  gboolean    writable;
  GList      *selected_files;
  gint        n_selected_files;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check whether the folder displayed by the view is writable */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  writable = (current_directory != NULL && thunar_file_can_write (current_directory));

  /* check whether the clipboard contains data that can be pasted here */
  pastable = (standard_view->clipboard != NULL && thunar_clipboard_manager_get_can_paste (standard_view->clipboard));

  /* determine the number of selected files */
  selected_files = thunar_standard_view_get_selected_files (standard_view);
  n_selected_files = g_list_length (selected_files);

  /* check whether the only selected file is
   * folder to which we can paste to */
  can_paste_into_folder = (n_selected_files == 1)
                       && thunar_file_is_directory (selected_files->data)
                       && thunar_file_can_write (selected_files->data);

  /* update the "Properties" action */
  gtk_action_set_sensitive (standard_view->priv->action_properties, (n_selected_files == 1));

  /* update the "Copy file(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_copy),
                "label", (n_selected_files > 1) ? _("_Copy files") : _("_Copy file"),
                "sensitive", (n_selected_files > 0),
                NULL);

  /* update the "Cut file(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_cut),
                "label", (n_selected_files > 1) ? _("Cu_t files") : _("Cu_t file"),
                "sensitive", (n_selected_files > 0) && writable,
                NULL);

  /* update the "Paste file(s)" action */
  gtk_action_set_sensitive (standard_view->priv->action_paste, writable && pastable);

  /* update the "Paste file(s) Into Folder" action */
  g_object_set (G_OBJECT (standard_view->priv->action_paste_into_folder),
                "sensitive", pastable,
                "visible", can_paste_into_folder,
                NULL);

  /* clear the current status text (will be recalculated on-demand) */
  g_free (standard_view->statusbar_text);
  standard_view->statusbar_text = NULL;

  /* tell everybody that the statusbar text may have changed */
  g_object_notify (G_OBJECT (standard_view), "statusbar-text");

  /* setup the new file selection for the launcher support */
  thunar_launcher_set_selected_files (standard_view->priv->launcher, selected_files);

  /* cleanup */
  thunar_file_list_free (selected_files);
}



