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

#include <thunar/thunar-dnd.h>
#include <thunar/thunar-extension-manager.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-standard-view-ui.h>
#include <thunar/thunar-text-renderer.h>

#include <thunarx/thunarx.h>



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
static gboolean      thunar_standard_view_expose_event              (GtkWidget                *widget,
                                                                     GdkEventExpose           *event);
static ThunarFile   *thunar_standard_view_get_current_directory     (ThunarNavigator          *navigator);
static void          thunar_standard_view_set_current_directory     (ThunarNavigator          *navigator,
                                                                     ThunarFile               *current_directory);
static gboolean      thunar_standard_view_get_loading               (ThunarView               *view);
static const gchar  *thunar_standard_view_get_statusbar_text        (ThunarView               *view);
static GtkUIManager *thunar_standard_view_get_ui_manager            (ThunarView               *view);
static void          thunar_standard_view_set_ui_manager            (ThunarView               *view,
                                                                     GtkUIManager             *ui_manager);
static GdkDragAction thunar_standard_view_get_dest_actions          (ThunarStandardView       *standard_view,
                                                                     GdkDragContext           *context,
                                                                     gint                      x,
                                                                     gint                      y,
                                                                     guint                     time,
                                                                     ThunarFile              **file_return);
static GList        *thunar_standard_view_get_selected_files        (ThunarStandardView       *standard_view);
static GList        *thunar_standard_view_get_selected_uris         (ThunarStandardView       *standard_view);
static void          thunar_standard_view_merge_menu_extensions     (ThunarStandardView       *standard_view,
                                                                     GList                    *selected_items);
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
static void          thunar_standard_view_action_rename             (GtkAction                *action,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_action_show_hidden_files  (GtkToggleAction          *toggle_action,
                                                                     ThunarStandardView       *standard_view);
static gboolean      thunar_standard_view_button_release_event      (GtkWidget                *view,
                                                                     GdkEventButton           *event,
                                                                     ThunarStandardView       *standard_view);
static gboolean      thunar_standard_view_motion_notify_event       (GtkWidget                *view,
                                                                     GdkEventMotion           *event,
                                                                     ThunarStandardView       *standard_view);
static gboolean      thunar_standard_view_drag_drop                 (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     gint                      x,
                                                                     gint                      y,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_data_received        (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     gint                      x,
                                                                     gint                      y,
                                                                     GtkSelectionData         *selection_data,
                                                                     guint                     info,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_leave                (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static gboolean      thunar_standard_view_drag_motion               (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     gint                      x,
                                                                     gint                      y,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_begin                (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_data_get             (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     GtkSelectionData         *selection_data,
                                                                     guint                     info,
                                                                     guint                     time,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_data_delete          (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_drag_end                  (GtkWidget                *view,
                                                                     GdkDragContext           *context,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_renamed                   (ThunarTextRenderer       *text_renderer,
                                                                     const gchar              *path_string,
                                                                     const gchar              *text,
                                                                     ThunarStandardView       *standard_view);
static void          thunar_standard_view_loading_unbound           (gpointer                  user_data);
static gboolean      thunar_standard_view_drag_timer                (gpointer                  user_data);
static void          thunar_standard_view_drag_timer_destroy        (gpointer                  user_data);



struct _ThunarStandardViewPrivate
{
  ThunarLauncher         *launcher;
  GtkAction              *action_properties;
  GtkAction              *action_copy;
  GtkAction              *action_cut;
  GtkAction              *action_paste;
  GtkAction              *action_paste_into_folder;
  GtkAction              *action_select_all_files;
  GtkAction              *action_select_by_pattern;
  GtkAction              *action_rename;
  GtkAction              *action_show_hidden_files;

  /* menu extensions support */
  ThunarExtensionManager *extension_manager;
  GtkActionGroup         *extension_actions;
  gint                    extension_merge_id;

  /* right-click drag/popup support */
  GList                  *drag_uri_list;
  gint                    drag_timer_id;
  gint                    drag_x;
  gint                    drag_y;

  /* drop site support */
  guint                   drop_data_ready : 1; /* whether the drop data was received already */
  guint                   drop_highlight : 1;
  guint                   drop_occurred : 1;   /* whether the data was dropped */
  GList                  *drop_uri_list;       /* the list of URIs that are contained in the drop data */
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
  { "rename", NULL, N_ ("_Rename"), "F2", N_ ("Rename the selected item"), G_CALLBACK (thunar_standard_view_action_rename), },
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

/* Target types for dropping to the view */
static const GtkTargetEntry drop_targets[] =
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
  gtkwidget_class->expose_event = thunar_standard_view_expose_event;

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
  standard_view->priv->drag_timer_id = -1;

  /* grab a reference on the extension manager */
  standard_view->priv->extension_manager = thunar_extension_manager_get_default ();

  /* initialize the scrolled window */
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
  standard_view->priv->action_rename = gtk_action_group_get_action (standard_view->action_group, "rename");
  standard_view->priv->action_show_hidden_files = gtk_action_group_get_action (standard_view->action_group, "show-hidden-files");

  /* setup the list model */
  standard_view->model = thunar_list_model_new ();

  /* setup the icon renderer */
  standard_view->icon_renderer = thunar_icon_renderer_new ();
  g_object_ref (G_OBJECT (standard_view->icon_renderer));
  gtk_object_sink (GTK_OBJECT (standard_view->icon_renderer));

  /* setup the name renderer */
  standard_view->name_renderer = thunar_text_renderer_new ();
  g_signal_connect (G_OBJECT (standard_view->name_renderer), "edited", G_CALLBACK (thunar_standard_view_renamed), standard_view);
  g_object_ref (G_OBJECT (standard_view->name_renderer));
  gtk_object_sink (GTK_OBJECT (standard_view->name_renderer));

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files", G_CALLBACK (thunar_standard_view_selection_changed), standard_view);

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
  GtkWidget *view;
  GObject   *object;

  /* let the GObject constructor create the instance */
  object = G_OBJECT_CLASS (thunar_standard_view_parent_class)->constructor (type,
                                                                            n_construct_properties,
                                                                            construct_properties);

  /* determine the real view widget (treeview or iconview) */
  view = GTK_BIN (object)->child;

  /* apply our list model to the real view (the child of the scrolled window),
   * we therefore assume that all real views have the "model" property.
   */
  g_object_set (G_OBJECT (view), "model", THUNAR_STANDARD_VIEW (object)->model, NULL);

  /* setup the real view as drop site */
  gtk_drag_dest_set (view, 0, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (view), "drag-drop", G_CALLBACK (thunar_standard_view_drag_drop), object);
  g_signal_connect (G_OBJECT (view), "drag-data-received", G_CALLBACK (thunar_standard_view_drag_data_received), object);
  g_signal_connect (G_OBJECT (view), "drag-leave", G_CALLBACK (thunar_standard_view_drag_leave), object);
  g_signal_connect (G_OBJECT (view), "drag-motion", G_CALLBACK (thunar_standard_view_drag_motion), object);

  /* setup the real view as drag source */
  gtk_drag_source_set (view, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
  g_signal_connect (G_OBJECT (view), "drag-begin", G_CALLBACK (thunar_standard_view_drag_begin), object);
  g_signal_connect (G_OBJECT (view), "drag-data-get", G_CALLBACK (thunar_standard_view_drag_data_get), object);
  g_signal_connect (G_OBJECT (view), "drag-data-delete", G_CALLBACK (thunar_standard_view_drag_data_delete), object);
  g_signal_connect (G_OBJECT (view), "drag-end", G_CALLBACK (thunar_standard_view_drag_end), object);

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

  /* be sure to cancel any pending drag timer */
  if (G_UNLIKELY (standard_view->priv->drag_timer_id >= 0))
    g_source_remove (standard_view->priv->drag_timer_id);

  /* reset the UI manager property */
  thunar_view_set_ui_manager (THUNAR_VIEW (standard_view), NULL);

  /* disconnect the widget from the launcher support */
  thunar_launcher_set_widget (standard_view->priv->launcher, NULL);

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->dispose) (object);
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

  /* release our reference on the extension manager */
  g_object_unref (G_OBJECT (standard_view->priv->extension_manager));

  /* release the drag URI list (just in case the drag-end wasn't fired before) */
  thunar_vfs_uri_list_free (standard_view->priv->drag_uri_list);

  /* release the drop URI list (just in case the drag-leave wasn't fired before) */
  thunar_vfs_uri_list_free (standard_view->priv->drop_uri_list);

  /* release the reference on the name renderer */
  g_object_unref (G_OBJECT (standard_view->name_renderer));

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

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->finalize) (object);
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



static gboolean
thunar_standard_view_expose_event (GtkWidget      *widget,
                                   GdkEventExpose *event)
{
  gboolean result = FALSE;
#if GTK_CHECK_VERSION(2,7,2)
  cairo_t *cr;
#endif
  gint     x, y, width, height;

  /* let the scrolled window do it's work */
  result = (*GTK_WIDGET_CLASS (thunar_standard_view_parent_class)->expose_event) (widget, event);

  /* render the folder drop shadow */
  if (G_UNLIKELY (THUNAR_STANDARD_VIEW (widget)->priv->drop_highlight))
    {
      x = widget->allocation.x;
      y = widget->allocation.y;
      width = widget->allocation.width;
      height = widget->allocation.height;

      gtk_paint_shadow (widget->style, widget->window,
                        GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                        NULL, widget, "dnd",
                        x, y, width, height);

#if GTK_CHECK_VERSION(2,7,2)
      /* the cairo version looks better here, so we use it if possible */
      cr = gdk_cairo_create (widget->window);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_set_line_width (cr, 1.0);
      cairo_rectangle (cr, x + 0.5, y + 0.5, width - 1, height - 1);
      cairo_stroke (cr);
      cairo_destroy (cr);
#else
      gdk_draw_rectangle (widget->window, widget->style->black_gc,
                          FALSE, x, y, width - 1, height - 1);
#endif
    }

  return result;
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
      /* remove any registered menu extension actions */
      if (G_LIKELY (standard_view->priv->extension_merge_id != 0))
        {
          gtk_ui_manager_remove_ui (standard_view->ui_manager, standard_view->priv->extension_merge_id);
          standard_view->priv->extension_merge_id = 0;
        }

      /* drop any previous extensions action group */
      if (G_LIKELY (standard_view->priv->extension_actions != NULL))
        {
          gtk_ui_manager_remove_action_group (standard_view->ui_manager, standard_view->priv->extension_actions);
          g_object_unref (G_OBJECT (standard_view->priv->extension_actions));
          standard_view->priv->extension_actions = NULL;
        }

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



static GdkDragAction
thunar_standard_view_get_dest_actions (ThunarStandardView *standard_view,
                                       GdkDragContext     *context,
                                       gint                x,
                                       gint                y,
                                       guint               time,
                                       ThunarFile        **file_return)
{
  GdkDragAction actions = 0;
  GdkDragAction action = 0;
  GtkTreePath  *path;
  GtkTreeIter   iter;
  ThunarFile   *file;

  /* determine the path for the given coordinates */
  path = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_path_at_pos) (standard_view, x, y);
  if (G_LIKELY (path != NULL))
    {
      /* determine the file for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, path);
      file = thunar_list_model_get_file (standard_view->model, &iter);
    }
  else
    {
      /* determine the current directory */
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (file != NULL))
        g_object_ref (G_OBJECT (file));
    }

  /* check if we can drop there */
  if (G_LIKELY (file != NULL))
    {
      actions = thunar_file_accepts_uri_drop (file, standard_view->priv->drop_uri_list, context->actions);
      if (G_LIKELY (actions != 0))
        {
          /* determine a working action */
          if (G_LIKELY ((context->suggested_action & actions) != 0))
            action = context->suggested_action;
          else if ((actions & GDK_ACTION_ASK) != 0)
            action = GDK_ACTION_ASK;
          else if ((actions & GDK_ACTION_COPY) != 0)
            action = GDK_ACTION_COPY;
          else if ((actions & GDK_ACTION_LINK) != 0)
            action = GDK_ACTION_LINK;
          else if ((actions & GDK_ACTION_MOVE) != 0)
            action = GDK_ACTION_MOVE;
          else
            action = GDK_ACTION_PRIVATE;

          /* tell the caller about the file (if it's interested) */
          if (G_UNLIKELY (file_return != NULL))
            *file_return = g_object_ref (G_OBJECT (file));
        }

      /* release the file reference */
      g_object_unref (G_OBJECT (file));
    }

  /* reset path if we cannot drop */
  if (G_UNLIKELY (action == 0 && path != NULL))
    {
      gtk_tree_path_free (path);
      path = NULL;
    }

  /* setup the drop-file for the icon renderer, so the user
   * gets good visual feedback for the drop target.
   */
  g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", (action != 0) ? file : NULL, NULL);

  /* do the view highlighting */
  if (standard_view->priv->drop_highlight != (path == NULL && action != 0))
    {
      standard_view->priv->drop_highlight = (path == NULL && action != 0);
      gtk_widget_queue_draw (GTK_WIDGET (standard_view));
    }

  /* do the item highlighting */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, path);

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, time);

  /* clean up */
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return actions;
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
thunar_standard_view_merge_menu_extensions (ThunarStandardView *standard_view,
                                            GList              *selected_items)
{
  GtkTreeIter iter;
  ThunarFile *file = NULL;
  GtkWidget  *window;
  GList      *providers;
  GList      *actions = NULL;
  GList      *files = NULL;
  GList      *tmp;
  GList      *lp;

  /* we cannot add anything if we aren't connected to any UI manager */
  if (G_UNLIKELY (standard_view->ui_manager == NULL))
    return;

  /* load the menu providers from the extension manager */
  providers = thunar_extension_manager_list_providers (standard_view->priv->extension_manager, THUNARX_TYPE_MENU_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
      /* determine the toplevel window we belong to */
      window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

      /* determine the list of selected files or the current folder */
      if (G_LIKELY (selected_items != NULL))
        {
          for (lp = selected_items; lp != NULL; lp = lp->next)
            {
              gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);
              file = thunar_list_model_get_file (standard_view->model, &iter);
              files = g_list_append (files, file);
            }
        }
      else
        {
          /* grab a reference to the current directory of the view */
          file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
        }

      /* load the actions offered by the menu providers */
      for (lp = providers; lp != NULL; lp = lp->next)
        {
          if (G_LIKELY (files != NULL))
            tmp = thunarx_menu_provider_get_file_actions (lp->data, window, files);
          else if (G_LIKELY (file != NULL))
            tmp = thunarx_menu_provider_get_folder_actions (lp->data, window, THUNARX_FILE_INFO (file));
          else
            tmp = NULL;
          actions = g_list_concat (actions, tmp);
          g_object_unref (G_OBJECT (lp->data));
        }
      g_list_free (providers);

      /* cleanup the selected files list (if any) */
      if (G_LIKELY (files != NULL))
        thunar_file_list_free (files);
    }

  /* remove the previously determined menu actions from the UI manager */
  if (G_LIKELY (standard_view->priv->extension_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (standard_view->ui_manager, standard_view->priv->extension_merge_id);
      standard_view->priv->extension_merge_id = 0;
    }

  /* drop any previous extensions action group */
  if (G_LIKELY (standard_view->priv->extension_actions != NULL))
    {
      gtk_ui_manager_remove_action_group (standard_view->ui_manager, standard_view->priv->extension_actions);
      g_object_unref (G_OBJECT (standard_view->priv->extension_actions));
      standard_view->priv->extension_actions = NULL;
    }

  /* add the actions specified by the menu providers */
  if (G_LIKELY (actions != NULL))
    {
      /* allocate the action group and the merge id for the extensions */
      standard_view->priv->extension_actions = gtk_action_group_new ("thunar-standard-view-extensions");
      standard_view->priv->extension_merge_id = gtk_ui_manager_new_merge_id (standard_view->ui_manager);
      gtk_ui_manager_insert_action_group (standard_view->ui_manager, standard_view->priv->extension_actions, -1);

      /* add the actions */
      for (lp = actions; lp != NULL; lp = lp->next)
        {
          /* add the action to the action group */
          gtk_action_group_add_action (standard_view->priv->extension_actions, GTK_ACTION (lp->data));

          /* add the action to the UI manager */
          if (G_LIKELY (selected_items != NULL))
            {
              /* add to the file context menu */
              gtk_ui_manager_add_ui (standard_view->ui_manager,
                                     standard_view->priv->extension_merge_id,
                                     "/file-context-menu/placeholder-extensions",
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     GTK_UI_MANAGER_MENUITEM, FALSE);
            }
          else
            {
              /* add to the folder context menu */
              gtk_ui_manager_add_ui (standard_view->ui_manager,
                                     standard_view->priv->extension_merge_id,
                                     "/folder-context-menu/placeholder-extensions",
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     GTK_UI_MANAGER_MENUITEM, FALSE);
            }

          /* release the reference on the action */
          g_object_unref (G_OBJECT (lp->data));
        }

      /* be sure to update the UI manager to avoid flickering */
      gtk_ui_manager_ensure_update (standard_view->ui_manager);

      /* cleanup */
      g_list_free (actions);
    }
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
thunar_standard_view_action_rename (GtkAction          *action,
                                    ThunarStandardView *standard_view)
{
  GList *selected_items;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* start renaming if we have exactly one selected file */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  if (G_LIKELY (selected_items != NULL && selected_items->next == NULL))
    (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor) (standard_view, selected_items->data, TRUE);
  g_list_foreach (selected_items, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (selected_items);
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



static gboolean
thunar_standard_view_button_release_event (GtkWidget          *view,
                                           GdkEventButton     *event,
                                           ThunarStandardView *standard_view)
{
  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  g_return_val_if_fail (standard_view->priv->drag_timer_id >= 0, FALSE);

  /* cancel the pending drag timer */
  g_source_remove (standard_view->priv->drag_timer_id);

  /* fire up the context menu */
  thunar_standard_view_context_menu (standard_view, 0, event->time);

  return TRUE;
}



static gboolean
thunar_standard_view_motion_notify_event (GtkWidget          *view,
                                          GdkEventMotion     *event,
                                          ThunarStandardView *standard_view)
{
  GdkDragContext *context;
  GtkTargetList  *target_list;

  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  g_return_val_if_fail (standard_view->priv->drag_timer_id >= 0, FALSE);

  /* check if we passed the DnD threshold */
  if (gtk_drag_check_threshold (view, standard_view->priv->drag_x, standard_view->priv->drag_y, event->x, event->y))
    {
      /* cancel the drag timer, as we won't popup the menu anymore */
      g_source_remove (standard_view->priv->drag_timer_id);

      /* allocate the drag context (preferred action is to ask the user) */
      target_list = gtk_target_list_new (drag_targets, G_N_ELEMENTS (drag_targets));
      context = gtk_drag_begin (view, target_list, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK, 3, (GdkEvent *) event);
      context->suggested_action = GDK_ACTION_ASK;
      gtk_target_list_unref (target_list);

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_standard_view_drag_drop (GtkWidget          *view,
                                GdkDragContext     *context,
                                gint                x,
                                gint                y,
                                guint               time,
                                ThunarStandardView *standard_view)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (view, context, NULL);
  if (G_LIKELY (target != GDK_NONE))
    {
      /* set state so the drag-data-received knows that
       * this is really a drop this time.
       */
      standard_view->priv->drop_occurred = TRUE;

      /* request the drag data from the source */
      gtk_drag_get_data (view, context, target, time);
  
      /* we'll call gtk_drag_finish() later */
      return TRUE;
    }
  else
    {
      /* we cannot handle the drag data */
      return FALSE;
    }
}



static void
thunar_standard_view_drag_data_received (GtkWidget          *view,
                                         GdkDragContext     *context,
                                         gint                x,
                                         gint                y,
                                         GtkSelectionData   *selection_data,
                                         guint               info,
                                         guint               time,
                                         ThunarStandardView *standard_view)
{
  GdkDragAction actions;
  GdkDragAction action;
  ThunarFile   *file = NULL;
  gboolean      succeed = FALSE;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!standard_view->priv->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
        standard_view->priv->drop_uri_list = thunar_vfs_uri_list_from_string ((gchar *) selection_data->data, NULL);

      /* reset the state */
      standard_view->priv->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (standard_view->priv->drop_occurred))
    {
      /* reset the state */
      standard_view->priv->drop_occurred = FALSE;

      /* determine the drop position */
      actions = thunar_standard_view_get_dest_actions (standard_view, context, x, y, time, &file);
      if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
        {
          /* ask the user what to do with the drop data */
          action = (context->action == GDK_ACTION_ASK) ? thunar_dnd_ask (GTK_WIDGET (standard_view), time, actions) : context->action;

          /* perform the requested action */
          if (G_LIKELY (action != 0))
            succeed = thunar_dnd_perform (GTK_WIDGET (standard_view), file, standard_view->priv->drop_uri_list, action);
        }

      /* release the file reference */
      if (G_LIKELY (file != NULL))
        g_object_unref (G_OBJECT (file));

      /* disable the highlighting and release the drag data */
      thunar_standard_view_drag_leave (view, context, time, standard_view);

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, time);
    }
}



static void
thunar_standard_view_drag_leave (GtkWidget          *widget,
                                 GdkDragContext     *context,
                                 guint               time,
                                 ThunarStandardView *standard_view)
{
  /* reset the drop-file for the icon renderer */
  g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", NULL, NULL);

  /* disable the drop highlighting around the view */
  if (G_LIKELY (standard_view->priv->drop_highlight))
    {
      standard_view->priv->drop_highlight = FALSE;
      gtk_widget_queue_draw (GTK_WIDGET (standard_view));
    }

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (standard_view->priv->drop_data_ready))
    {
      thunar_vfs_uri_list_free (standard_view->priv->drop_uri_list);
      standard_view->priv->drop_uri_list = NULL;
      standard_view->priv->drop_data_ready = FALSE;
    }

  /* disable the highlighting of the items in the view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, NULL);
}



static gboolean
thunar_standard_view_drag_motion (GtkWidget          *view,
                                  GdkDragContext     *context,
                                  gint                x,
                                  gint                y,
                                  guint               time,
                                  ThunarStandardView *standard_view)
{
  GdkAtom target;

  /* request the drop data on-demand (if we don't have it already) */
  if (G_UNLIKELY (!standard_view->priv->drop_data_ready))
    {
      /* check if we can handle that drag data (yet?) */
      target = gtk_drag_dest_find_target (view, context, NULL);
      if (G_UNLIKELY (target == GDK_NONE))
        {
          /* we cannot handle the drag data */
          gdk_drag_status (context, 0, time);
        }
      else
        {
          /* request the drag data from the source */
          gtk_drag_get_data (view, context, target, time);

          /* and deny the drop so far */
          gdk_drag_status (context, 0, time);
        }
    }
  else
    {
      /* check whether we can drop at (x,y) */
      thunar_standard_view_get_dest_actions (standard_view, context, x, y, time, NULL);
    }

  return TRUE;
}



static void
thunar_standard_view_drag_begin (GtkWidget          *view,
                                 GdkDragContext     *context,
                                 ThunarStandardView *standard_view)
{
  ThunarFile *file;
  GdkPixbuf  *icon;
  gint        size;

  g_return_if_fail (standard_view->priv->drag_uri_list == NULL);

  /* query the list of selected URIs */
  standard_view->priv->drag_uri_list = thunar_standard_view_get_selected_uris (standard_view);
  if (G_LIKELY (standard_view->priv->drag_uri_list != NULL))
    {
      /* determine the first selected file */
      file = thunar_file_get_for_uri (standard_view->priv->drag_uri_list->data, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* generate an icon based on that file */
          g_object_get (G_OBJECT (standard_view->icon_renderer), "size", &size, NULL);
          icon = thunar_file_load_icon (file, THUNAR_FILE_ICON_STATE_DEFAULT, standard_view->icon_factory, size);
          gtk_drag_set_icon_pixbuf (context, icon, 0, 0);
          g_object_unref (G_OBJECT (icon));

          /* release the file */
          g_object_unref (G_OBJECT (file));
        }
    }
}



static void
thunar_standard_view_drag_data_get (GtkWidget          *view,
                                    GdkDragContext     *context,
                                    GtkSelectionData   *selection_data,
                                    guint               info,
                                    guint               time,
                                    ThunarStandardView *standard_view)
{
  gchar *uri_string;

  /* set the URI list for the drag selection */
  uri_string = thunar_vfs_uri_list_to_string (standard_view->priv->drag_uri_list, THUNAR_VFS_URI_STRING_ESCAPED);
  gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) uri_string, strlen (uri_string));
  g_free (uri_string);
}



static void
thunar_standard_view_drag_data_delete (GtkWidget          *view,
                                       GdkDragContext     *context,
                                       ThunarStandardView *standard_view)
{
  /* make sure the default handler of ExoIconView/GtkTreeView is never run */
  g_signal_stop_emission_by_name (G_OBJECT (view), "drag-data-delete");
}



static void
thunar_standard_view_drag_end (GtkWidget          *view,
                               GdkDragContext     *context,
                               ThunarStandardView *standard_view)
{
  /* release the list of dragged URIs */
  thunar_vfs_uri_list_free (standard_view->priv->drag_uri_list);
  standard_view->priv->drag_uri_list = NULL;
}



static void
thunar_standard_view_renamed (ThunarTextRenderer *text_renderer,
                              const gchar        *path_string,
                              const gchar        *text,
                              ThunarStandardView *standard_view)
{
  GtkTreeRowReference *row;
  const gchar         *old_name;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  ThunarFile          *file;
  GtkWidget           *message;
  GtkWidget           *window;
  GError              *error = NULL;

  g_return_if_fail (THUNAR_IS_TEXT_RENDERER (text_renderer));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (path_string != NULL);

  /* verify that the user supplied a valid file name */
  if (G_UNLIKELY (text == NULL || *text == '\0'))
    return;

  /* determine path and iterator for the edited item */
  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, path);

  /* determine the file from the iter */
  file = thunar_list_model_get_file (THUNAR_LIST_MODEL (standard_view->model), &iter);

  /* check if the name changed */
  old_name = thunar_file_get_display_name (file);
  if (G_LIKELY (!exo_str_is_equal (old_name, text)))
    {
      /* remember a row reference to the file as the
       * rename operation is likely cause a reordering
       * of the list model.
       */
      row = gtk_tree_row_reference_new (GTK_TREE_MODEL (standard_view->model), path);

      /* try to rename the file */
      if (!thunar_file_rename (file, text, &error))
        {
          /* determine the toplevel widget */
          window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

          /* display an error message */
          message = gtk_message_dialog_new (GTK_WINDOW (window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Failed to rename %s."),
                                            old_name);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
                                                    "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
        }
      else if (G_LIKELY (gtk_tree_row_reference_valid (row)))
        {
          /* place the cursor on the item again and scroll to the position */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, gtk_tree_row_reference_get_path (row));

          /* update the selection, so we get updated actions, statusbar,
           * etc. with the new file name and probably new mime type.
           */
          thunar_standard_view_selection_changed (standard_view);
        }

      /* release the row reference */
      gtk_tree_row_reference_free (row);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (file));
  gtk_tree_path_free (path);
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



static gboolean
thunar_standard_view_drag_timer (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);

  /* fire up the context menu */
  GDK_THREADS_ENTER ();
  thunar_standard_view_context_menu (standard_view, 3, gtk_get_current_event_time ());
  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_standard_view_drag_timer_destroy (gpointer user_data)
{
  /* unregister the motion notify and button release event handlers (thread-safe) */
  g_signal_handlers_disconnect_by_func (GTK_BIN (user_data)->child, thunar_standard_view_button_release_event, user_data);
  g_signal_handlers_disconnect_by_func (GTK_BIN (user_data)->child, thunar_standard_view_motion_notify_event, user_data);

  /* reset the drag timer source id */
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_timer_id = -1;
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

  /* merge the menu extensions for the selected items */
  thunar_standard_view_merge_menu_extensions (standard_view, selected_items);

  /* release the selected items */
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
 * thunar_standard_view_queue_popup:
 * @standard_view : a #ThunarStandardView.
 * @event         : the right click event.
 *
 * Schedules a context menu popup in response to
 * a right-click button event. Right-click events
 * need to be handled in a special way, as the
 * user may also start a drag using the right
 * mouse button and therefore this function
 * schedules a timer, which - once expired -
 * opens the context menu. If the user moves
 * the mouse prior to expiration, a right-click
 * drag (with #GDK_ACTION_ASK) will be started
 * instead.
 **/
void
thunar_standard_view_queue_popup (ThunarStandardView *standard_view,
                                  GdkEventButton     *event)
{
  GtkSettings *settings;
  GtkWidget   *view;
  gint         delay;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (event != NULL);

  /* check if we have already scheduled a drag timer */
  if (G_LIKELY (standard_view->priv->drag_timer_id < 0))
    {
      /* remember the new coordinates */
      standard_view->priv->drag_x = event->x;
      standard_view->priv->drag_y = event->y;

      /* figure out the real view */
      view = GTK_BIN (standard_view)->child;

      /* we use the menu popup delay here, which should give us good values */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (view));
      g_object_get (G_OBJECT (settings), "gtk-menu-popup-delay", &delay, NULL);

      /* schedule the timer */
      standard_view->priv->drag_timer_id = g_timeout_add_full (G_PRIORITY_LOW, delay, thunar_standard_view_drag_timer,
                                                               standard_view, thunar_standard_view_drag_timer_destroy);

      /* register the motion notify and the button release events on the real view */
      g_signal_connect (G_OBJECT (view), "button-release-event", G_CALLBACK (thunar_standard_view_button_release_event), standard_view);
      g_signal_connect (G_OBJECT (view), "motion-notify-event", G_CALLBACK (thunar_standard_view_motion_notify_event), standard_view);
    }
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
  writable = (current_directory != NULL && thunar_file_is_writable (current_directory));

  /* check whether the clipboard contains data that can be pasted here */
  pastable = (standard_view->clipboard != NULL && thunar_clipboard_manager_get_can_paste (standard_view->clipboard));

  /* determine the number of selected files */
  selected_files = thunar_standard_view_get_selected_files (standard_view);
  n_selected_files = g_list_length (selected_files);

  /* check whether the only selected file is
   * folder to which we can paste to */
  can_paste_into_folder = (n_selected_files == 1)
                       && thunar_file_is_directory (selected_files->data)
                       && thunar_file_is_writable (selected_files->data);

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

  /* update the "Rename" action */
  gtk_action_set_sensitive (standard_view->priv->action_rename, (n_selected_files == 1
                            && thunar_file_is_renameable (selected_files->data)));

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



