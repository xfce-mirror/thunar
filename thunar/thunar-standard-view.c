/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-standard-view-ui.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-templates-action.h>
#include <thunar/thunar-text-renderer.h>



#define THUNAR_STANDARD_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_LOADING,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_STATUSBAR_TEXT,
  PROP_UI_MANAGER,
  PROP_ZOOM_LEVEL,
};

/* Signal identifiers */
enum
{
  DELETE_SELECTED_FILES,
  START_OPEN_LOCATION,
  LAST_SIGNAL,
};

/* Identifiers for DnD target types */
enum
{
  TEXT_URI_LIST,
  XDND_DIRECT_SAVE0,
};



static void                 thunar_standard_view_class_init                 (ThunarStandardViewClass  *klass);
static void                 thunar_standard_view_component_init             (ThunarComponentIface     *iface);
static void                 thunar_standard_view_navigator_init             (ThunarNavigatorIface     *iface);
static void                 thunar_standard_view_view_init                  (ThunarViewIface          *iface);
static void                 thunar_standard_view_init                       (ThunarStandardView       *standard_view);
static GObject             *thunar_standard_view_constructor                (GType                     type,
                                                                             guint                     n_construct_properties,
                                                                             GObjectConstructParam    *construct_properties);
static void                 thunar_standard_view_dispose                    (GObject                  *object);
static void                 thunar_standard_view_finalize                   (GObject                  *object);
static void                 thunar_standard_view_get_property               (GObject                  *object,
                                                                             guint                     prop_id,
                                                                             GValue                   *value,
                                                                             GParamSpec               *pspec);
static void                 thunar_standard_view_set_property               (GObject                  *object,
                                                                             guint                     prop_id,
                                                                             const GValue             *value,
                                                                             GParamSpec               *pspec);
static void                 thunar_standard_view_realize                    (GtkWidget                *widget);
static void                 thunar_standard_view_unrealize                  (GtkWidget                *widget);
static void                 thunar_standard_view_grab_focus                 (GtkWidget                *widget);
static gboolean             thunar_standard_view_expose_event               (GtkWidget                *widget,
                                                                             GdkEventExpose           *event);
static GList               *thunar_standard_view_get_selected_files         (ThunarComponent          *component);
static void                 thunar_standard_view_set_selected_files         (ThunarComponent          *component,
                                                                             GList                    *selected_files);
static GtkUIManager        *thunar_standard_view_get_ui_manager             (ThunarComponent          *component);
static void                 thunar_standard_view_set_ui_manager             (ThunarComponent          *component,
                                                                             GtkUIManager             *ui_manager);
static ThunarFile          *thunar_standard_view_get_current_directory      (ThunarNavigator          *navigator);
static void                 thunar_standard_view_set_current_directory      (ThunarNavigator          *navigator,
                                                                             ThunarFile               *current_directory);
static gboolean             thunar_standard_view_get_loading                (ThunarView               *view);
static void                 thunar_standard_view_set_loading                (ThunarStandardView       *standard_view,
                                                                             gboolean                  loading);
static const gchar         *thunar_standard_view_get_statusbar_text         (ThunarView               *view);
static gboolean             thunar_standard_view_get_show_hidden            (ThunarView               *view);
static void                 thunar_standard_view_set_show_hidden            (ThunarView               *view,
                                                                             gboolean                  show_hidden);
static ThunarZoomLevel      thunar_standard_view_get_zoom_level             (ThunarView               *view);
static void                 thunar_standard_view_set_zoom_level             (ThunarView               *view,
                                                                             ThunarZoomLevel           zoom_level);
static void                 thunar_standard_view_reset_zoom_level           (ThunarView               *view);
static void                 thunar_standard_view_reload                     (ThunarView               *view);
static gboolean             thunar_standard_view_get_visible_range          (ThunarView               *view,
                                                                             ThunarFile              **start_file,
                                                                             ThunarFile              **end_file);
static void                 thunar_standard_view_scroll_to_file             (ThunarView               *view,
                                                                             ThunarFile               *file,
                                                                             gboolean                  select,
                                                                             gboolean                  use_align,
                                                                             gfloat                    row_align,
                                                                             gfloat                    col_align);
static gboolean             thunar_standard_view_delete_selected_files      (ThunarStandardView       *standard_view);
static GdkDragAction        thunar_standard_view_get_dest_actions           (ThunarStandardView       *standard_view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     time,
                                                                             ThunarFile              **file_return);
static ThunarFile          *thunar_standard_view_get_drop_file              (ThunarStandardView       *standard_view,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkTreePath             **path_return);
static void                 thunar_standard_view_merge_custom_actions       (ThunarStandardView       *standard_view,
                                                                             GList                    *selected_items);
static void                 thunar_standard_view_update_statusbar_text      (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_empty_file   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_folder       (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_template     (GtkAction                *action,
                                                                             const ThunarVfsInfo      *info,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_properties          (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_cut                 (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_copy                (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_paste               (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_delete              (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_paste_into_folder   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_select_all_files    (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_select_by_pattern   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_duplicate           (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_make_link           (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_rename              (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static GClosure            *thunar_standard_view_new_files_closure          (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_new_files                  (ThunarVfsJob             *job,
                                                                             GList                    *path_list,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_button_release_event       (GtkWidget                *view,
                                                                             GdkEventButton           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_motion_notify_event        (GtkWidget                *view,
                                                                             GdkEventMotion           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_key_press_event            (GtkWidget                *view,
                                                                             GdkEventKey              *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_scroll_event               (GtkWidget                *view,
                                                                             GdkEventScroll           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_drop                  (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     time,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_received         (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     time,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_leave                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             guint                     time,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_motion                (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     time,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_begin                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_get              (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     time,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_delete           (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_end                   (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_error                      (ThunarListModel          *model,
                                                                             const GError             *error,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_sort_column_changed        (GtkTreeSortable          *tree_sortable,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_loading_unbound            (gpointer                  user_data);
static gboolean             thunar_standard_view_drag_scroll_timer          (gpointer                  user_data);
static void                 thunar_standard_view_drag_scroll_timer_destroy  (gpointer                  user_data);
static gboolean             thunar_standard_view_drag_timer                 (gpointer                  user_data);
static void                 thunar_standard_view_drag_timer_destroy         (gpointer                  user_data);



struct _ThunarStandardViewPrivate
{
  GtkAction              *action_create_folder;
  GtkAction              *action_create_document;
  GtkAction              *action_properties;
  GtkAction              *action_cut;
  GtkAction              *action_copy;
  GtkAction              *action_paste;
  GtkAction              *action_delete;
  GtkAction              *action_paste_into_folder;
  GtkAction              *action_select_all_files;
  GtkAction              *action_select_by_pattern;
  GtkAction              *action_duplicate;
  GtkAction              *action_make_link;
  GtkAction              *action_rename;

  /* support for file manager extensions */
  ThunarxProviderFactory *provider_factory;

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* custom menu actions support */
  GtkActionGroup         *custom_actions;
  gint                    custom_merge_id;

  /* right-click drag/popup support */
  GList                  *drag_path_list;
  gint                    drag_scroll_timer_id;
  gint                    drag_timer_id;
  gint                    drag_x;
  gint                    drag_y;

  /* drop site support */
  guint                   drop_data_ready : 1; /* whether the drop data was received already */
  guint                   drop_highlight : 1;
  guint                   drop_occurred : 1;   /* whether the data was dropped */
  GList                  *drop_path_list;      /* the list of URIs that are contained in the drop data */

  /* the "new-files" closure, which is used to select files whenever 
   * new files are created by a ThunarVfsJob associated with this view
   */
  GClosure               *new_files_closure;

  /* scroll-to-file support */
  ThunarFile             *scroll_to_file;
  guint                   scroll_to_select : 1;
  guint                   scroll_to_use_align : 1;
  gfloat                  scroll_to_row_align;
  gfloat                  scroll_to_col_align;
};



static const GtkActionEntry action_entries[] =
{
  { "file-context-menu", NULL, N_ ("File Context Menu"), NULL, NULL, NULL, },
  { "folder-context-menu", NULL, N_ ("Folder Context Menu"), NULL, NULL, NULL, },
  { "create-folder", NULL, N_ ("Create _Folder..."), "<control><shift>N", N_ ("Create an empty folder within the current folder"), G_CALLBACK (thunar_standard_view_action_create_folder), },
  { "properties", GTK_STOCK_PROPERTIES, N_ ("_Properties..."), "<alt>Return", N_ ("View the properties of the selected file"), G_CALLBACK (thunar_standard_view_action_properties), },
  { "cut", GTK_STOCK_CUT, N_ ("Cu_t"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_cut), },
  { "copy", GTK_STOCK_COPY, N_ ("_Copy"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_copy), },
  { "paste", GTK_STOCK_PASTE, N_ ("_Paste"), NULL, N_ ("Move or copy files previously selected by a Cut or Copy command"), G_CALLBACK (thunar_standard_view_action_paste), },
  { "delete", GTK_STOCK_DELETE, N_ ("_Delete"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_delete), },
  { "paste-into-folder", GTK_STOCK_PASTE, N_ ("Paste Into Folder"), NULL, N_ ("Move or copy files previously selected by a Cut or Copy command into the selected folder"), G_CALLBACK (thunar_standard_view_action_paste_into_folder), },
  { "select-all-files", NULL, N_ ("Select _all Files"), "<control>A", N_ ("Select all files in this window"), G_CALLBACK (thunar_standard_view_action_select_all_files), },
  { "select-by-pattern", NULL, N_ ("Select _by Pattern..."), "<control>S", N_ ("Select all files that match a certain pattern"), G_CALLBACK (thunar_standard_view_action_select_by_pattern), },
  { "duplicate", NULL, N_ ("Du_plicate"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_duplicate), },
  { "make-link", NULL, N_ ("Ma_ke Link"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_make_link), },
  { "rename", NULL, N_ ("_Rename..."), "F2", N_ ("Rename the selected file"), G_CALLBACK (thunar_standard_view_action_rename), },
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
  { "XdndDirectSave0", 0, XDND_DIRECT_SAVE0, },
};



static guint         standard_view_signals[LAST_SIGNAL];
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

      static const GInterfaceInfo component_info =
      {
        (GInterfaceInitFunc) thunar_standard_view_component_init,
        NULL,
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

      type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW, I_("ThunarStandardView"), &info, G_TYPE_FLAG_ABSTRACT);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_COMPONENT, &component_info);
      g_type_add_interface_static (type, THUNAR_TYPE_VIEW, &view_info);
    }

  return type;
}



static void
thunar_standard_view_class_init (ThunarStandardViewClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkBindingSet  *binding_set;
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

  klass->delete_selected_files = thunar_standard_view_delete_selected_files;
  klass->connect_ui_manager = (gpointer) exo_noop;
  klass->disconnect_ui_manager = (gpointer) exo_noop;

  /**
   * ThunarStandardView:loading:
   *
   * Whether the folder associated with this view is
   * currently being loaded from the underlying media.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LOADING,
                                   g_param_spec_override ("loading",
                                                          g_param_spec_boolean ("loading",
                                                                                "loading",
                                                                                "loading",
                                                                                FALSE,
                                                                                EXO_PARAM_READWRITE)));

  /* override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_UI_MANAGER, "ui-manager");

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarView's properties */
  g_object_class_override_property (gobject_class, PROP_STATUSBAR_TEXT, "statusbar-text");
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
  g_object_class_override_property (gobject_class, PROP_ZOOM_LEVEL, "zoom-level");

  /**
   * ThunarStandardView::start-opn-location:
   * @standard_view : a #ThunarStandardView.
   * @initial_text  : the inital location text.
   *
   * Emitted by @standard_view, whenever the user requests to
   * select a custom location (using either the "Open Location"
   * dialog or the location entry widget) by specifying an
   * @initial_text (i.e. if the user types "/" into the
   * view).
   **/
  standard_view_signals[START_OPEN_LOCATION] =
    g_signal_new (I_("start-open-location"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarStandardViewClass, start_open_location),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * ThunarStandardView::delete-selected-files:
   * @standard_view : a #ThunarStandardView.
   *
   * Emitted whenever the user presses the Delete key. This
   * is an internal signal used to bind the action to keys.
   **/
  standard_view_signals[DELETE_SELECTED_FILES] =
    g_signal_new (I_("delete-selected-files"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (ThunarStandardViewClass, delete_selected_files),
                  g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /* setup the key bindings for the standard views */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0, "delete-selected-files", 0);
}



static void
thunar_standard_view_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_standard_view_get_selected_files;
  iface->set_selected_files = thunar_standard_view_set_selected_files;
  iface->get_ui_manager = thunar_standard_view_get_ui_manager;
  iface->set_ui_manager = thunar_standard_view_set_ui_manager;
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
  iface->get_show_hidden = thunar_standard_view_get_show_hidden;
  iface->set_show_hidden = thunar_standard_view_set_show_hidden;
  iface->get_zoom_level = thunar_standard_view_get_zoom_level;
  iface->set_zoom_level = thunar_standard_view_set_zoom_level;
  iface->reset_zoom_level = thunar_standard_view_reset_zoom_level;
  iface->reload = thunar_standard_view_reload;
  iface->get_visible_range = thunar_standard_view_get_visible_range;
  iface->scroll_to_file = thunar_standard_view_scroll_to_file;
}



static void
thunar_standard_view_init (ThunarStandardView *standard_view)
{
  standard_view->priv = THUNAR_STANDARD_VIEW_GET_PRIVATE (standard_view);
  standard_view->priv->drag_scroll_timer_id = -1;
  standard_view->priv->drag_timer_id = -1;

  /* grab a reference on the preferences */
  standard_view->preferences = thunar_preferences_get ();

  /* grab a reference on the provider factory */
  standard_view->priv->provider_factory = thunarx_provider_factory_get_default ();

  /* initialize the scrolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (standard_view),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (standard_view), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (standard_view), GTK_SHADOW_IN);

  /* setup the action group for this view */
  standard_view->action_group = gtk_action_group_new ("ThunarStandardView");
  gtk_action_group_set_translation_domain (standard_view->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (standard_view->action_group, action_entries,
                                G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (standard_view));

  /* lookup all actions to speed up access later */
  standard_view->priv->action_create_folder = gtk_action_group_get_action (standard_view->action_group, "create-folder");
  standard_view->priv->action_properties = gtk_action_group_get_action (standard_view->action_group, "properties");
  standard_view->priv->action_cut = gtk_action_group_get_action (standard_view->action_group, "cut");
  standard_view->priv->action_copy = gtk_action_group_get_action (standard_view->action_group, "copy");
  standard_view->priv->action_paste = gtk_action_group_get_action (standard_view->action_group, "paste");
  standard_view->priv->action_delete = gtk_action_group_get_action (standard_view->action_group, "delete");
  standard_view->priv->action_paste_into_folder = gtk_action_group_get_action (standard_view->action_group, "paste-into-folder");
  standard_view->priv->action_select_all_files = gtk_action_group_get_action (standard_view->action_group, "select-all-files");
  standard_view->priv->action_select_by_pattern = gtk_action_group_get_action (standard_view->action_group, "select-by-pattern");
  standard_view->priv->action_duplicate = gtk_action_group_get_action (standard_view->action_group, "duplicate");
  standard_view->priv->action_make_link = gtk_action_group_get_action (standard_view->action_group, "make-link");
  standard_view->priv->action_rename = gtk_action_group_get_action (standard_view->action_group, "rename");

  /* add the "Create Document" sub menu action */
  standard_view->priv->action_create_document = thunar_templates_action_new ("create-document", _("Create _Document"));
  g_signal_connect (G_OBJECT (standard_view->priv->action_create_document), "create-empty-file",
                    G_CALLBACK (thunar_standard_view_action_create_empty_file), standard_view);
  g_signal_connect (G_OBJECT (standard_view->priv->action_create_document), "create-template",
                    G_CALLBACK (thunar_standard_view_action_create_template), standard_view);
  gtk_action_group_add_action (standard_view->action_group, standard_view->priv->action_create_document);
  g_object_unref (G_OBJECT (standard_view->priv->action_create_document));

  /* setup the list model */
  standard_view->model = thunar_list_model_new ();
  g_signal_connect (G_OBJECT (standard_view->model), "error", G_CALLBACK (thunar_standard_view_error), standard_view);
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-folders-first", G_OBJECT (standard_view->model), "folders-first");

  /* setup the icon renderer */
  standard_view->icon_renderer = thunar_icon_renderer_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (standard_view->icon_renderer));
  exo_binding_new (G_OBJECT (standard_view), "zoom-level", G_OBJECT (standard_view->icon_renderer), "size");

  /* setup the name renderer */
  standard_view->name_renderer = thunar_text_renderer_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (standard_view->name_renderer));
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (standard_view->name_renderer), "follow-prelit");

  /* be sure to update the selection whenever the folder changes */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::folder", G_CALLBACK (thunar_standard_view_selection_changed), standard_view);

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files", G_CALLBACK (thunar_standard_view_update_statusbar_text), standard_view);
}



static GObject*
thunar_standard_view_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  ThunarStandardView *standard_view;
  ThunarZoomLevel     zoom_level;
  ThunarColumn        sort_column;
  GtkSortType         sort_order;
  GtkWidget          *view;
  GObject            *object;

  /* let the GObject constructor create the instance */
  object = G_OBJECT_CLASS (thunar_standard_view_parent_class)->constructor (type,
                                                                            n_construct_properties,
                                                                            construct_properties);

  /* cast to standard_view for convenience */
  standard_view = THUNAR_STANDARD_VIEW (object);

  /* setup the default zoom-level, determined from the "last-<view>-zoom-level" preference */
  g_object_get (G_OBJECT (standard_view->preferences), THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name, &zoom_level, NULL);
  thunar_view_set_zoom_level (THUNAR_VIEW (standard_view), zoom_level);

  /* save the "zoom-level" as "last-<view>-zoom-level" whenever the user changes the zoom level */
  exo_binding_new (object, "zoom-level", G_OBJECT (standard_view->preferences), THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name);

  /* determine the real view widget (treeview or iconview) */
  view = GTK_BIN (object)->child;

  /* apply our list model to the real view (the child of the scrolled window),
   * we therefore assume that all real views have the "model" property.
   */
  g_object_set (G_OBJECT (view), "model", standard_view->model, NULL);

  /* apply the single-click mode to the view */
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (view), "single-click");

  /* apply the default sort column and sort order */
  g_object_get (G_OBJECT (standard_view->preferences), "last-sort-column", &sort_column, "last-sort-order", &sort_order, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), sort_column, sort_order);

  /* stay informed about changes to the sort column/order */
  g_signal_connect (G_OBJECT (standard_view->model), "sort-column-changed", G_CALLBACK (thunar_standard_view_sort_column_changed), standard_view);

  /* setup support to navigate using a horizontal mouse wheel */
  g_signal_connect (G_OBJECT (view), "scroll-event", G_CALLBACK (thunar_standard_view_scroll_event), object);

  /* need to catch certain keys for the internal view widget */
  g_signal_connect (G_OBJECT (view), "key-press-event", G_CALLBACK (thunar_standard_view_key_press_event), object);

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

  /* be sure to cancel any pending drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id >= 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* be sure to cancel any pending drag timer */
  if (G_UNLIKELY (standard_view->priv->drag_timer_id >= 0))
    g_source_remove (standard_view->priv->drag_timer_id);

  /* reset the UI manager property */
  thunar_component_set_ui_manager (THUNAR_COMPONENT (standard_view), NULL);

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

  /* release the scroll_to_file reference (if any) */
  if (G_UNLIKELY (standard_view->priv->scroll_to_file != NULL))
    g_object_unref (G_OBJECT (standard_view->priv->scroll_to_file));

  /* release our reference on the provider factory */
  g_object_unref (G_OBJECT (standard_view->priv->provider_factory));

  /* release the drag path list (just in case the drag-end wasn't fired before) */
  thunar_vfs_path_list_free (standard_view->priv->drag_path_list);

  /* release the drop path list (just in case the drag-leave wasn't fired before) */
  thunar_vfs_path_list_free (standard_view->priv->drop_path_list);

  /* release the reference on the name renderer */
  g_object_unref (G_OBJECT (standard_view->name_renderer));

  /* release the reference on the icon renderer */
  g_object_unref (G_OBJECT (standard_view->icon_renderer));

  /* release the reference on the action group */
  g_object_unref (G_OBJECT (standard_view->action_group));

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
      standard_view->priv->new_files_closure = NULL;
    }

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (standard_view->preferences));

  /* disconnect from the list model */
  g_signal_handlers_disconnect_matched (G_OBJECT (standard_view->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
  g_object_unref (G_OBJECT (standard_view->model));
  
  /* release our list of currently selected files */
  thunar_file_list_free (standard_view->selected_files);

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

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_view_get_show_hidden (THUNAR_VIEW (object)));
      break;

    case PROP_STATUSBAR_TEXT:
      g_value_set_static_string (value, thunar_view_get_statusbar_text (THUNAR_VIEW (object)));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_component_get_ui_manager (THUNAR_COMPONENT (object)));
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_enum (value, thunar_view_get_zoom_level (THUNAR_VIEW (object)));
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

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_LOADING:
      thunar_standard_view_set_loading (standard_view, g_value_get_boolean (value));
      break;
      break;
      
    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_view_set_show_hidden (THUNAR_VIEW (object), g_value_get_boolean (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object), g_value_get_object (value));
      break;

    case PROP_ZOOM_LEVEL:
      thunar_view_set_zoom_level (THUNAR_VIEW (object), g_value_get_enum (value));
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

  /* we need to redraw whenever the "show-thumbnails" property is toggled */
  g_signal_connect_swapped (G_OBJECT (standard_view->icon_factory), "notify::show-thumbnails", G_CALLBACK (gtk_widget_queue_draw), standard_view);
}



static void
thunar_standard_view_unrealize (GtkWidget *widget)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (widget);

  /* disconnect the clipboard changed handler */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->clipboard), thunar_standard_view_selection_changed, standard_view);

  /* drop the reference on the icon factory */
  g_signal_handlers_disconnect_by_func (G_OBJECT (standard_view->icon_factory), gtk_widget_queue_draw, standard_view);
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



static GList*
thunar_standard_view_get_selected_files (ThunarComponent *component)
{
  return THUNAR_STANDARD_VIEW (component)->selected_files;
}



static void
thunar_standard_view_set_selected_files (ThunarComponent *component,
                                         GList           *selected_files)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (component);
  GtkTreePath        *first_path = NULL;
  GList              *paths;
  GList              *lp;

  /* verify that we have a valid model */
  if (G_UNLIKELY (standard_view->model == NULL))
    return;

  /* unselect all previously selected files */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);

  /* determine the tree paths for the given files */
  paths = thunar_list_model_get_paths_for_files (standard_view->model, selected_files);
  if (G_LIKELY (paths != NULL))
    {
      /* select the given tree paths */
      for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
        {
          /* check if this path is located before the current first_path */
          if (gtk_tree_path_compare (lp->data, first_path) < 0)
            first_path = lp->data;

          /* select the path */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, lp->data);
        }

      /* scroll to the first path (previously determined) */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, first_path, FALSE, 0.0f, 0.0f);

      /* release the tree paths */
      g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (paths);
    }
}



static GtkUIManager*
thunar_standard_view_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_STANDARD_VIEW (component)->ui_manager;
}



static void
thunar_standard_view_set_ui_manager (ThunarComponent *component,
                                     GtkUIManager    *ui_manager)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (component);
  GError             *error = NULL;

  /* disconnect from the previous UI manager */
  if (G_LIKELY (standard_view->ui_manager != NULL))
    {
      /* remove any registered custom menu actions */
      if (G_LIKELY (standard_view->priv->custom_merge_id != 0))
        {
          gtk_ui_manager_remove_ui (standard_view->ui_manager, standard_view->priv->custom_merge_id);
          standard_view->priv->custom_merge_id = 0;
        }

      /* drop the previous custom action group */
      if (G_LIKELY (standard_view->priv->custom_actions != NULL))
        {
          gtk_ui_manager_remove_action_group (standard_view->ui_manager, standard_view->priv->custom_actions);
          g_object_unref (G_OBJECT (standard_view->priv->custom_actions));
          standard_view->priv->custom_actions = NULL;
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
  g_object_notify (G_OBJECT (standard_view), "ui-manager");
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

  /* scroll to top-left when changing folder */
  gtk_adjustment_set_value (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);
  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);

  /* We drop the model from the view as a simple optimization to speed up
   * the process of disconnecting the model data from the view.
   */
  g_object_set (G_OBJECT (GTK_BIN (standard_view)->child), "model", NULL, NULL);

  /* open the new directory as folder */
  folder = thunar_folder_get_for_file (current_directory);

  /* connect the "loading" binding */
  standard_view->loading_binding = exo_binding_new_full (G_OBJECT (folder), "loading",
                                                         G_OBJECT (standard_view), "loading",
                                                         NULL, thunar_standard_view_loading_unbound,
                                                         standard_view);

  /* apply the new folder */
  thunar_list_model_set_folder (standard_view->model, folder);
  g_object_unref (G_OBJECT (folder));

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



static void
thunar_standard_view_set_loading (ThunarStandardView *standard_view,
                                  gboolean            loading)
{
  ThunarFile *file;

  loading = !!loading;

  /* check if we're already in that state */
  if (G_UNLIKELY (standard_view->loading == loading))
    return;

  /* apply the new state */
  standard_view->loading = loading;

  /* check if we're done loading and have a scheduled scroll_to_file */
  if (G_UNLIKELY (!loading && standard_view->priv->scroll_to_file != NULL))
    {
      /* remember and reset the scroll_to_file reference */
      file = standard_view->priv->scroll_to_file;
      standard_view->priv->scroll_to_file = NULL;

      /* and try again */
      thunar_view_scroll_to_file (THUNAR_VIEW (standard_view), file,
                                  standard_view->priv->scroll_to_select,
                                  standard_view->priv->scroll_to_use_align,
                                  standard_view->priv->scroll_to_row_align,
                                  standard_view->priv->scroll_to_col_align);

      /* cleanup */
      g_object_unref (G_OBJECT (file));
    }

  /* notify listeners */
  g_object_freeze_notify (G_OBJECT (standard_view));
  g_object_notify (G_OBJECT (standard_view), "loading");
  g_object_notify (G_OBJECT (standard_view), "statusbar-text");
  g_object_thaw_notify (G_OBJECT (standard_view));
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



static gboolean
thunar_standard_view_get_show_hidden (ThunarView *view)
{
  return thunar_list_model_get_show_hidden (THUNAR_STANDARD_VIEW (view)->model);
}



static void
thunar_standard_view_set_show_hidden (ThunarView *view,
                                      gboolean    show_hidden)
{
  thunar_list_model_set_show_hidden (THUNAR_STANDARD_VIEW (view)->model, show_hidden);
}



static ThunarZoomLevel
thunar_standard_view_get_zoom_level (ThunarView *view)
{
  return THUNAR_STANDARD_VIEW (view)->priv->zoom_level;
}



static void
thunar_standard_view_set_zoom_level (ThunarView     *view,
                                     ThunarZoomLevel zoom_level)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);

  /* check if we have a new zoom-level here */
  if (G_LIKELY (standard_view->priv->zoom_level != zoom_level))
    {
      standard_view->priv->zoom_level = zoom_level;
      g_object_notify (G_OBJECT (standard_view), "zoom-level");
    }
}



static void
thunar_standard_view_reset_zoom_level (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  const gchar        *property_name;
  GParamSpec         *pspec;
  GValue              value = { 0, };

  /* determine the default zoom level from the preferences */
  property_name = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name;
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (standard_view->preferences), property_name);
  g_value_init (&value, THUNAR_TYPE_ZOOM_LEVEL);
  g_param_value_set_default (pspec, &value);
  g_object_set_property (G_OBJECT (view), "zoom-level", &value);
  g_value_unset (&value);
}



static void
thunar_standard_view_reload (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  ThunarFolder       *folder;

  /* determine the folder for the view model */
  folder = thunar_list_model_get_folder (standard_view->model);
  if (G_LIKELY (folder != NULL))
    thunar_folder_reload (folder);
}



static gboolean
thunar_standard_view_get_visible_range (ThunarView  *view,
                                        ThunarFile **start_file,
                                        ThunarFile **end_file)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GtkTreePath        *start_path;
  GtkTreePath        *end_path;
  GtkTreeIter         iter;

  /* determine the visible range as tree paths */
  if ((*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_visible_range) (standard_view, &start_path, &end_path))
    {
      /* determine the file for the start path */
      if (G_LIKELY (start_file != NULL))
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, start_path);
          *start_file = thunar_list_model_get_file (standard_view->model, &iter);
        }

      /* determine the file for the end path */
      if (G_LIKELY (end_file != NULL))
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, end_path);
          *end_file = thunar_list_model_get_file (standard_view->model, &iter);
        }

      /* release the tree paths */
      gtk_tree_path_free (start_path);
      gtk_tree_path_free (end_path);

      return TRUE;
    }

  return FALSE;
}



static void
thunar_standard_view_scroll_to_file (ThunarView *view,
                                     ThunarFile *file,
                                     gboolean    select,
                                     gboolean    use_align,
                                     gfloat      row_align,
                                     gfloat      col_align)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GList               files;
  GList              *paths;

  /* release the previous scroll_to_file reference (if any) */
  if (G_UNLIKELY (standard_view->priv->scroll_to_file != NULL))
    {
      g_object_unref (G_OBJECT (standard_view->priv->scroll_to_file));
      standard_view->priv->scroll_to_file = NULL;
    }

  /* check if we're still loading */
  if (thunar_view_get_loading (view))
    {
      /* remember a reference for the new file and settings */
      standard_view->priv->scroll_to_file = g_object_ref (G_OBJECT (file));
      standard_view->priv->scroll_to_select = select;
      standard_view->priv->scroll_to_use_align = use_align;
      standard_view->priv->scroll_to_row_align = row_align;
      standard_view->priv->scroll_to_col_align = col_align;
    }
  else
    {
      /* fake a file list */
      files.data = file;
      files.next = NULL;
      files.prev = NULL;

      /* determine the path for the file */
      paths = thunar_list_model_get_paths_for_files (standard_view->model, &files);
      if (G_LIKELY (paths != NULL))
        {
          /* scroll to the path */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, paths->data, use_align, row_align, col_align);

          /* check if we should also alter the selection */
          if (G_UNLIKELY (select))
            {
              /* select only the file in question */
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, paths->data);
            }

          /* cleanup */
          g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
          g_list_free (paths);
        }
    }
}



static gboolean
thunar_standard_view_delete_selected_files (ThunarStandardView *standard_view)
{
  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* just emit the "activate" signal on the "delete" action */
  gtk_action_activate (standard_view->priv->action_delete);

  /* ...and we're done */
  return TRUE;
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
  ThunarFile   *file;

  /* determine the file and path for the given coordinates */
  file = thunar_standard_view_get_drop_file (standard_view, x, y, &path);

  /* check if we can drop there */
  if (G_LIKELY (file != NULL))
    {
      /* determine the possible drop actions for the file (and the suggested action if any) */
      actions = thunar_file_accepts_drop (file, standard_view->priv->drop_path_list, context, &action);
      if (G_LIKELY (actions != 0))
        {
          /* tell the caller about the file (if it's interested) */
          if (G_UNLIKELY (file_return != NULL))
            *file_return = g_object_ref (G_OBJECT (file));
        }
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
  if (G_LIKELY (file != NULL))
    g_object_unref (G_OBJECT (file));
  if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return actions;
}



static ThunarFile*
thunar_standard_view_get_drop_file (ThunarStandardView *standard_view,
                                    gint                x,
                                    gint                y,
                                    GtkTreePath       **path_return)
{
  GtkTreePath *path = NULL;
  GtkTreeIter  iter;
  ThunarFile  *file = NULL;

  /* determine the path for the given coordinates */
  path = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_path_at_pos) (standard_view, x, y);
  if (G_LIKELY (path != NULL))
    {
      /* determine the file for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, path);
      file = thunar_list_model_get_file (standard_view->model, &iter);

      /* we can only drop to directories and executable files */
      if (!thunar_file_is_directory (file) && !thunar_file_is_executable (file))
        {
          /* drop to the folder instead */
          g_object_unref (G_OBJECT (file));
          gtk_tree_path_free (path);
          path = NULL;
        }
    }

  /* if we don't have a path yet, we'll drop to the folder instead */
  if (G_UNLIKELY (path == NULL))
    {
      /* determine the current directory */
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (file != NULL))
        g_object_ref (G_OBJECT (file));
    }

  /* return the path (if any) */
  if (G_LIKELY (path_return != NULL))
    *path_return = path;
  else if (G_LIKELY (path != NULL))
    gtk_tree_path_free (path);

  return file;
}



static void
thunar_standard_view_merge_custom_actions (ThunarStandardView *standard_view,
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

  /* determine the toplevel window we belong to */
  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

  /* load the menu providers from the provider factory */
  providers = thunarx_provider_factory_list_providers (standard_view->priv->provider_factory, THUNARX_TYPE_MENU_PROVIDER);
  if (G_LIKELY (providers != NULL))
    {
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

  /* determine the currently selected file/folder for which to load custom actions */
  if (G_LIKELY (selected_items != NULL && selected_items->next == NULL))
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, selected_items->data);
      file = thunar_list_model_get_file (standard_view->model, &iter);
    }
  else if (selected_items == NULL)
    {
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (file != NULL))
        g_object_ref (G_OBJECT (file));
    }
  else
    {
      file = NULL;
    }

  /* load the custom actions for the currently selected file/folder */
  if (G_LIKELY (file != NULL))
    {
      tmp = thunar_file_get_actions (file, window);
      actions = g_list_concat (actions, tmp);
      g_object_unref (G_OBJECT (file));
    }

  /* remove the previously determined menu actions from the UI manager */
  if (G_LIKELY (standard_view->priv->custom_merge_id != 0))
    {
      gtk_ui_manager_remove_ui (standard_view->ui_manager, standard_view->priv->custom_merge_id);
      gtk_ui_manager_ensure_update (standard_view->ui_manager);
      standard_view->priv->custom_merge_id = 0;
    }

  /* drop any previous custom action group */
  if (G_LIKELY (standard_view->priv->custom_actions != NULL))
    {
      gtk_ui_manager_remove_action_group (standard_view->ui_manager, standard_view->priv->custom_actions);
      g_object_unref (G_OBJECT (standard_view->priv->custom_actions));
      standard_view->priv->custom_actions = NULL;
    }

  /* add the actions specified by the menu providers */
  if (G_LIKELY (actions != NULL))
    {
      /* allocate the action group and the merge id for the custom actions */
      standard_view->priv->custom_actions = gtk_action_group_new ("thunar-standard-view-custom-actions");
      standard_view->priv->custom_merge_id = gtk_ui_manager_new_merge_id (standard_view->ui_manager);
      gtk_ui_manager_insert_action_group (standard_view->ui_manager, standard_view->priv->custom_actions, -1);

      /* add the actions to the UI manager */
      for (lp = actions; lp != NULL; lp = lp->next)
        {
          /* add the action to the action group */
          gtk_action_group_add_action (standard_view->priv->custom_actions, GTK_ACTION (lp->data));

          /* add the action to the UI manager */
          if (G_LIKELY (selected_items != NULL))
            {
              /* add to the file context menu */
              gtk_ui_manager_add_ui (standard_view->ui_manager,
                                     standard_view->priv->custom_merge_id,
                                     "/file-context-menu/placeholder-custom-actions",
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     gtk_action_get_name (GTK_ACTION (lp->data)),
                                     GTK_UI_MANAGER_MENUITEM, FALSE);
            }
          else
            {
              /* add to the folder context menu */
              gtk_ui_manager_add_ui (standard_view->ui_manager,
                                     standard_view->priv->custom_merge_id,
                                     "/folder-context-menu/placeholder-custom-actions",
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
thunar_standard_view_update_statusbar_text (ThunarStandardView *standard_view)
{
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* clear the current status text (will be recalculated on-demand) */
  g_free (standard_view->statusbar_text);
  standard_view->statusbar_text = NULL;

  /* tell everybody that the statusbar text may have changed */
  g_object_notify (G_OBJECT (standard_view), "statusbar-text");
}



static void
thunar_standard_view_action_create_empty_file (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  ThunarApplication     *application;
  ThunarFile            *current_directory;
  GList                  path_list;
  gchar                 *name;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* lookup "text/plain" mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_info = thunar_vfs_mime_database_get_info (mime_database, "text/plain");

  /* ask the user to enter a name for the new empty file */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view), mime_info, _("New Empty File"), _("New Empty File..."));
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the path list */
          path_list.data = thunar_vfs_path_relative (thunar_file_get_path (current_directory), name);
          path_list.next = path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_creat (application, GTK_WIDGET (standard_view), &path_list,
                                    thunar_standard_view_new_files_closure (standard_view));
          g_object_unref (G_OBJECT (application));

          /* release the path */
          thunar_vfs_path_unref (path_list.data);
        }

      /* release the file name in the local encoding */
      g_free (name);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_unref (mime_info);
}



static void
thunar_standard_view_action_create_folder (GtkAction          *action,
                                           ThunarStandardView *standard_view)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  ThunarApplication     *application;
  ThunarFile            *current_directory;
  GList                  path_list;
  gchar                 *name;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* lookup "inode/directory" mime info */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view), mime_info, _("New Folder"), _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the path list */
          path_list.data = thunar_vfs_path_relative (thunar_file_get_path (current_directory), name);
          path_list.next = path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_mkdir (application, GTK_WIDGET (standard_view), &path_list,
                                    thunar_standard_view_new_files_closure (standard_view));
          g_object_unref (G_OBJECT (application));

          /* release the path */
          thunar_vfs_path_unref (path_list.data);
        }

      /* release the file name */
      g_free (name);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_unref (mime_info);
}



static void
thunar_standard_view_action_create_template (GtkAction           *action,
                                             const ThunarVfsInfo *info,
                                             ThunarStandardView  *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GList              source_path_list;
  GList              target_path_list;
  gchar             *title;
  gchar             *name;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (info != NULL);

  /* generate a title for the create dialog */
  title = g_strdup_printf (_("Create Document from template \"%s\""), info->display_name);

  /* ask the user to enter a name for the new document */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view), info->mime_info, info->display_name, title);
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the source path list */
          source_path_list.data = info->path;
          source_path_list.next = NULL;
          source_path_list.prev = NULL;

          /* fake the target path list */
          target_path_list.data = thunar_vfs_path_relative (thunar_file_get_path (current_directory), name);
          target_path_list.next = NULL;
          target_path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_copy_to (application, GTK_WIDGET (standard_view), &source_path_list, &target_path_list,
                                      thunar_standard_view_new_files_closure (standard_view));
          g_object_unref (G_OBJECT (application));

          /* release the target path */
          thunar_vfs_path_unref (target_path_list.data);
        }

      /* release the file name */
      g_free (name);
    }

  /* cleanup */
  g_free (title);
}



static void
thunar_standard_view_action_properties (GtkAction          *action,
                                        ThunarStandardView *standard_view)
{
  ThunarFile *file = NULL;
  GtkWidget  *toplevel;
  GtkWidget  *dialog;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check if no files are currently selected */
  if (standard_view->selected_files == NULL)
    {
      /* if we don't have any files selected, we just popup
       * the properties dialog for the current folder.
       */
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
    }
  else if (g_list_length (standard_view->selected_files) == 1)
    {
      /* popup the properties dialog for the one and only selected file */
      file = THUNAR_FILE (standard_view->selected_files->data);
    }

  /* popup the properties dialog if we have exactly one file */
  if (G_LIKELY (file != NULL))
    {
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
      if (G_LIKELY (toplevel != NULL))
        {
          dialog = g_object_new (THUNAR_TYPE_PROPERTIES_DIALOG,
                                 "destroy-with-parent", TRUE,
                                 "file", file,
                                 NULL);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
          gtk_widget_show (dialog);
        }
    }
}



static void
thunar_standard_view_action_cut (GtkAction          *action,
                                 ThunarStandardView *standard_view)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  thunar_clipboard_manager_cut_files (standard_view->clipboard, standard_view->selected_files);
}



static void
thunar_standard_view_action_copy (GtkAction          *action,
                                  ThunarStandardView *standard_view)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  thunar_clipboard_manager_copy_files (standard_view->clipboard, standard_view->selected_files);
}



static void
thunar_standard_view_action_paste (GtkAction          *action,
                                   ThunarStandardView *standard_view)
{
  ThunarFile *current_directory;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      thunar_clipboard_manager_paste_files (standard_view->clipboard, thunar_file_get_path (current_directory),
                                            GTK_WIDGET (standard_view), thunar_standard_view_new_files_closure (standard_view));
    }
}



static void
thunar_standard_view_action_delete (GtkAction          *action,
                                    ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  GtkWidget         *dialog;
  GtkWidget         *window;
  GList             *selected_paths;
  guint              n_selected_files;
  gchar             *message;
  gint               response;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the number of currently selected files */
  n_selected_files = g_list_length (standard_view->selected_files);
  if (G_UNLIKELY (n_selected_files <= 0))
    return;

  /* generate the question to confirm the delete operation */
  if (G_LIKELY (n_selected_files == 1))
    {
      message = g_strdup_printf (_("Are you sure that you want to\npermanently delete \"%s\"?"),
                                 thunar_file_get_display_name (THUNAR_FILE (standard_view->selected_files->data)));
    }
  else
    {
      message = g_strdup_printf (ngettext ("Are you sure that you want to permanently\ndelete the selected file?",
                                           "Are you sure that you want to permanently\ndelete the %u selected files?",
                                           n_selected_files),
                                 n_selected_files);
    }

  /* determine the selected paths, just in case the selection changes in the meantime */
  selected_paths = thunar_file_list_to_path_list (standard_view->selected_files);

  /* ask the user to confirm the delete operation */
  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL
                                   | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   message);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_DELETE, GTK_RESPONSE_YES,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("If you delete a file, it is permanently lost."));
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  g_free (message);

  /* perform the delete operation */
  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      application = thunar_application_get ();
      thunar_application_unlink (application, window, selected_paths);
      g_object_unref (G_OBJECT (application));
    }

  /* release the path list */
  thunar_vfs_path_list_free (selected_paths);
}



static void
thunar_standard_view_action_paste_into_folder (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  ThunarFile *file;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the first selected file and verify that it's a folder */
  file = g_list_nth_data (standard_view->selected_files, 0);
  if (G_LIKELY (file != NULL && thunar_file_is_directory (file)))
    thunar_clipboard_manager_paste_files (standard_view->clipboard, thunar_file_get_path (file), GTK_WIDGET (standard_view), NULL);
}



static void
thunar_standard_view_action_select_all_files (GtkAction          *action,
                                              ThunarStandardView *standard_view)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_all) (standard_view);
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
  dialog = gtk_dialog_new_with_buttons (_("Select by Pattern"),
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
thunar_standard_view_action_duplicate (GtkAction          *action,
                                       ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GClosure          *new_files_closure;
  GList             *selected_paths;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the file for the current directory */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      /* determine the selected paths for the view */
      selected_paths = thunar_file_list_to_path_list (standard_view->selected_files);
      if (G_LIKELY (selected_paths != NULL))
        {
          /* copy the selected files into the current directory, which effectively
           * creates duplicates of the files.
           */
          application = thunar_application_get ();
          new_files_closure = thunar_standard_view_new_files_closure (standard_view);
          thunar_application_copy_into (application, GTK_WIDGET (standard_view), selected_paths,
                                        thunar_file_get_path (current_directory), new_files_closure);
          g_object_unref (G_OBJECT (application));

          /* clean up */
          thunar_vfs_path_list_free (selected_paths);
        }
    }
}



static void
thunar_standard_view_action_make_link (GtkAction          *action,
                                       ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GClosure          *new_files_closure;
  GList             *selected_paths;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the file for the current directory */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      /* determine the selected paths for the view */
      selected_paths = thunar_file_list_to_path_list (standard_view->selected_files);
      if (G_LIKELY (selected_paths != NULL))
        {
          /* link the selected files into the current directory, which effectively
           * creates new unique links for the files.
           */
          application = thunar_application_get ();
          new_files_closure = thunar_standard_view_new_files_closure (standard_view);
          thunar_application_link_into (application, GTK_WIDGET (standard_view), selected_paths,
                                        thunar_file_get_path (current_directory), new_files_closure);
          g_object_unref (G_OBJECT (application));

          /* clean up */
          thunar_vfs_path_list_free (selected_paths);
        }
    }
}



static void
thunar_standard_view_action_rename (GtkAction          *action,
                                    ThunarStandardView *standard_view)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  const gchar       *filename;
  const gchar       *text;
  GtkTreeIter        iter;
  ThunarFile        *file;
  GtkWidget         *window;
  GtkWidget         *dialog;
  GtkWidget         *entry;
  GtkWidget         *label;
  GtkWidget         *image;
  GtkWidget         *table;
  GdkPixbuf         *icon;
  GError            *error = NULL;
  GList             *selected_items;
  GList             *path_list;
  GList              file_list;
  glong              offset;
  gchar             *title;
  gint               response;

  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* start renaming if we have exactly one selected file */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  if (G_LIKELY (selected_items != NULL && selected_items->next == NULL))
    {
      /* determine the file in question */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, selected_items->data);
      file = thunar_list_model_get_file (standard_view->model, &iter);
      filename = thunar_file_get_display_name (file);

      /* create a new dialog window */
      title = g_strdup_printf (_("Rename \"%s\""), filename);
      window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
      dialog = gtk_dialog_new_with_buttons (title,
                                            GTK_WINDOW (window),
                                            GTK_DIALOG_MODAL
                                            | GTK_DIALOG_NO_SEPARATOR
                                            | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                            THUNAR_STOCK_RENAME, GTK_RESPONSE_OK,
                                            NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);
      g_free (title);

      table = g_object_new (GTK_TYPE_TABLE, "border-width", 6, "column-spacing", 6, "row-spacing", 3, NULL);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
      gtk_widget_show (table);

      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (dialog));
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
      icon = thunar_icon_factory_load_file_icon (icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
      g_object_unref (G_OBJECT (icon_factory));

      image = gtk_image_new_from_pixbuf (icon);
      gtk_misc_set_padding (GTK_MISC (image), 6, 6);
      gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 2, GTK_FILL, GTK_FILL, 0, 0);
      g_object_unref (G_OBJECT (icon));
      gtk_widget_show (image);

      label = gtk_label_new (_("Enter the new name:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
      gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (label);

      entry = gtk_entry_new ();
      gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
      gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (entry);

      /* setup the old filename */
      gtk_entry_set_text (GTK_ENTRY (entry), filename);

      /* check if the filename contains a dot */
      text = g_utf8_strrchr (filename, -1, '.');
      if (G_LIKELY (text != NULL))
        {
          /* grab focus to the entry first, else the selection will be altered later */
          gtk_widget_grab_focus (entry);

          /* determine the UTF-8 char offset */
          offset = g_utf8_pointer_to_offset (filename, text);

          /* select the text prior to the dot */
          if (G_LIKELY (offset > 0))
            gtk_entry_select_region (GTK_ENTRY (entry), 0, offset);
        }

      /* run the dialog */
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* determine the new filename */
          text = gtk_entry_get_text (GTK_ENTRY (entry));

          /* check if we have a new name here */
          if (G_LIKELY (!exo_str_is_equal (filename, text)))
            {
              /* try to rename the file */
              if (!thunar_file_rename (file, text, &error))
                {
                  /* display an error message */
                  thunar_dialogs_show_error (GTK_WIDGET (standard_view), error, _("Failed to rename \"%s\""), filename);

                  /* release the error */
                  g_error_free (error);
                }
              else
                {
                  /* fake a file list with only the file in it */
                  file_list.data = file;
                  file_list.next = NULL;
                  file_list.prev = NULL;

                  /* determine the path for the file */
                  path_list = thunar_list_model_get_paths_for_files (standard_view->model, &file_list);
                  if (G_LIKELY (path_list != NULL))
                    {
                      /* place the cursor on the file and scroll the new position */
                      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, path_list->data, FALSE, 0.0f, 0.0f);

                      /* update the selection, so we get updated actions, statusbar,
                       * etc. with the new file name and probably new mime type.
                       */
                      thunar_standard_view_selection_changed (standard_view);

                      /* release the path list */
                      g_list_foreach (path_list, (GFunc) gtk_tree_path_free, NULL);
                      g_list_free (path_list);
                    }
                }
            }
        }

      /* cleanup */
      g_object_unref (G_OBJECT (file));
      gtk_widget_destroy (dialog);
    }

  /* release the selected items */
  g_list_foreach (selected_items, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (selected_items);
}



static GClosure*
thunar_standard_view_new_files_closure (ThunarStandardView *standard_view)
{
  /* drop any previous "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
    }

  /* allocate a new "new-files" closure */
  standard_view->priv->new_files_closure = g_cclosure_new (G_CALLBACK (thunar_standard_view_new_files), standard_view, NULL);
  g_closure_ref (standard_view->priv->new_files_closure);
  g_closure_sink (standard_view->priv->new_files_closure);

  /* and return our new closure */
  return standard_view->priv->new_files_closure;
}



static void
thunar_standard_view_new_files (ThunarVfsJob       *job,
                                GList              *path_list,
                                ThunarStandardView *standard_view)
{
  ThunarFile*file;
  GList     *file_list = NULL;
  GList     *lp;

  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* verify that we have a non-empty path_list */
  if (G_UNLIKELY (path_list == NULL))
    return;

  /* determine the files for the paths */
  for (lp = path_list; lp != NULL; lp = lp->next)
    {
      file = thunar_file_cache_lookup (lp->data);
      if (G_LIKELY (file != NULL))
        file_list = g_list_prepend (file_list, file);
    }

  /* check if we have any new files here */
  if (G_LIKELY (file_list != NULL))
    {
      /* select the files */
      thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), file_list);

      /* release the file list */
      g_list_free (file_list);

      /* grab the focus to the view widget */
      gtk_widget_grab_focus (GTK_BIN (standard_view)->child);
    }
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
thunar_standard_view_scroll_event (GtkWidget          *view,
                                   GdkEventScroll     *event,
                                   ThunarStandardView *standard_view)
{
  GtkAction *action = NULL;
  gboolean   misc_horizontal_wheel_navigates;

  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* check if we should use the horizontal mouse wheel for navigation */
  g_object_get (G_OBJECT (standard_view->preferences), "misc-horizontal-wheel-navigates", &misc_horizontal_wheel_navigates, NULL);
  if (G_UNLIKELY (misc_horizontal_wheel_navigates))
    {
      /* determine the appropriate action ("back" for scroll left, "forward" for scroll right) */
      if (G_UNLIKELY (event->type == GDK_SCROLL && event->direction == GDK_SCROLL_LEFT))
        action = gtk_ui_manager_get_action (standard_view->ui_manager, "/main-menu/go-menu/back");
      else if (G_UNLIKELY (event->type == GDK_SCROLL && event->direction == GDK_SCROLL_RIGHT))
        action = gtk_ui_manager_get_action (standard_view->ui_manager, "/main-menu/go-menu/forward");

      /* perform the action (if any) */
      if (G_UNLIKELY (action != NULL))
        {
          gtk_action_activate (action);
          return TRUE;
        }
    }

  /* zoom-in/zoom-out on control+mouse wheel */
  if ((event->state & GDK_CONTROL_MASK) != 0 && (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN))
    {
      thunar_view_set_zoom_level (THUNAR_VIEW (standard_view),
          (event->direction == GDK_SCROLL_DOWN)
          ? MIN (standard_view->priv->zoom_level + 1, THUNAR_ZOOM_N_LEVELS - 1)
          : MAX (standard_view->priv->zoom_level, 1) - 1);
      return TRUE;
    }

  /* next please... */
  return FALSE;
}



static gboolean
thunar_standard_view_key_press_event (GtkWidget          *view,
                                      GdkEventKey        *event,
                                      ThunarStandardView *standard_view)
{
  g_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* need to catch "/" and "~" first, as the views would otherwise start interactive search */
  if ((event->keyval == GDK_slash || event->keyval == GDK_asciitilde) && !(event->state & (~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask ())))
    {
      /* popup the location selector (in whatever way) */
      g_signal_emit (G_OBJECT (standard_view), standard_view_signals[START_OPEN_LOCATION], 0, event->string);
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
  ThunarVfsPath *path;
  ThunarFile    *file = NULL;
  GdkAtom        target;
  guchar        *prop_text;
  gint           prop_len;
  gchar         *uri = NULL;

  target = gtk_drag_dest_find_target (view, context, NULL);
  if (G_UNLIKELY (target == GDK_NONE))
    {
      /* we cannot handle the drag data */
      return FALSE;
    }
  else if (G_UNLIKELY (target == gdk_atom_intern ("XdndDirectSave0", FALSE)))
    {
      /* determine the file for the drop position */
      file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* determine the file name from the DnD source window */
          if (gdk_property_get (context->source_window, gdk_atom_intern ("XdndDirectSave0", FALSE),
                                gdk_atom_intern ("text/plain", FALSE), 0, 1024, FALSE, NULL, NULL,
                                &prop_len, &prop_text) && prop_text != NULL)
            {
              /* zero-terminate the string */
              prop_text = g_realloc (prop_text, prop_len + 1);
              prop_text[prop_len] = '\0';

              /* allocate the relative path for the target */
              path = thunar_vfs_path_relative (thunar_file_get_path (file), (const gchar *) prop_text);

              /* determine the new URI */
              uri = thunar_vfs_path_dup_uri (path);

              /* setup the property */
              gdk_property_change (GDK_DRAWABLE (context->source_window),
                                   gdk_atom_intern ("XdndDirectSave0", FALSE),
                                   gdk_atom_intern ("text/plain", FALSE), 8,
                                   GDK_PROP_MODE_REPLACE, (const guchar *) uri,
                                   strlen (uri));

              /* cleanup */
              thunar_vfs_path_unref (path);
              g_free (prop_text);
              g_free (uri);
            }

          /* release the file reference */
          g_object_unref (G_OBJECT (file));
        }

      /* if uri == NULL, we didn't set the property */
      if (G_UNLIKELY (uri == NULL))
        return FALSE;
    }

  /* set state so the drag-data-received knows that
   * this is really a drop this time.
   */
  standard_view->priv->drop_occurred = TRUE;

  /* request the drag data from the source (initiates
   * saving in case of XdndDirectSave).
   */
  gtk_drag_get_data (view, context, target, time);

  /* we'll call gtk_drag_finish() later */
  return TRUE;
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
  ThunarFolder *folder;
  ThunarFile   *file = NULL;
  gboolean      succeed = FALSE;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!standard_view->priv->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
        standard_view->priv->drop_path_list = thunar_vfs_path_list_from_string ((gchar *) selection_data->data, NULL);

      /* reset the state */
      standard_view->priv->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (standard_view->priv->drop_occurred))
    {
      /* reset the state */
      standard_view->priv->drop_occurred = FALSE;

      /* check if we're doing XdndDirectSave */
      if (G_UNLIKELY (info == XDND_DIRECT_SAVE0))
        {
          /* we don't handle XdndDirectSave stage (3), result "F" yet */
          if (G_UNLIKELY (selection_data->format == 8 && selection_data->length == 1 && selection_data->data[0] == 'F'))
            {
              /* indicate that we don't provide "F" fallback */
              gdk_property_change (GDK_DRAWABLE (context->source_window),
                                   gdk_atom_intern ("XdndDirectSave0", FALSE),
                                   gdk_atom_intern ("text/plain", FALSE), 8,
                                   GDK_PROP_MODE_REPLACE, (const guchar *) "", 0);
            }
          else if (G_LIKELY (selection_data->format == 8 && selection_data->length == 1 && selection_data->data[0] == 'S'))
            {
              /* XDS was successfull, so determine the file for the drop position */
              file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
              if (G_LIKELY (file != NULL))
                {
                  /* verify that we have a directory here */
                  if (thunar_file_is_directory (file))
                    {
                      /* reload the folder corresponding to the file */
                      folder = thunar_folder_get_for_file (file);
                      thunar_folder_reload (folder);
                      g_object_unref (G_OBJECT (folder));
                    }

                  /* cleanup */
                  g_object_unref (G_OBJECT (file));
                }
            }

          /* in either case, we succeed! */
          succeed = TRUE;
        }
      else if (G_LIKELY (info == TEXT_URI_LIST))
        {
          /* determine the drop position */
          actions = thunar_standard_view_get_dest_actions (standard_view, context, x, y, time, &file);
          if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
            {
              /* ask the user what to do with the drop data */
              action = (context->action == GDK_ACTION_ASK) ? thunar_dnd_ask (GTK_WIDGET (standard_view), time, actions) : context->action;

              /* perform the requested action */
              if (G_LIKELY (action != 0))
                {
                  succeed = thunar_dnd_perform (GTK_WIDGET (standard_view), file, standard_view->priv->drop_path_list,
                                                action, thunar_standard_view_new_files_closure (standard_view));
                }
            }

          /* release the file reference */
          if (G_LIKELY (file != NULL))
            g_object_unref (G_OBJECT (file));
        }

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, time);

      /* disable the highlighting and release the drag data */
      thunar_standard_view_drag_leave (view, context, time, standard_view);
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

  /* stop any running drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id >= 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* disable the drop highlighting around the view */
  if (G_LIKELY (standard_view->priv->drop_highlight))
    {
      standard_view->priv->drop_highlight = FALSE;
      gtk_widget_queue_draw (GTK_WIDGET (standard_view));
    }

  /* reset the "drop data ready" status and free the URI list */
  if (G_LIKELY (standard_view->priv->drop_data_ready))
    {
      thunar_vfs_path_list_free (standard_view->priv->drop_path_list);
      standard_view->priv->drop_path_list = NULL;
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
  GdkDragAction action = 0;
  GtkTreePath  *path;
  ThunarFile   *file = NULL;
  GdkAtom       target;

  /* request the drop data on-demand (if we don't have it already) */
  if (G_UNLIKELY (!standard_view->priv->drop_data_ready))
    {
      /* check if we can handle that drag data (yet?) */
      target = gtk_drag_dest_find_target (view, context, NULL);
      if (G_UNLIKELY (target == gdk_atom_intern ("XdndDirectSave0", FALSE)))
        {
          /* determine the file for the given coordinates */
          file = thunar_standard_view_get_drop_file (standard_view, x, y, &path);

          /* check if we can save here */
          if (G_LIKELY (file != NULL && thunar_file_is_directory (file) && thunar_file_is_writable (file)))
            action = context->suggested_action;

          /* reset path if we cannot drop */
          if (G_UNLIKELY (action == 0 && path != NULL))
            {
              gtk_tree_path_free (path);
              path = NULL;
            }

          /* do the view highlighting */
          if (standard_view->priv->drop_highlight != (path == NULL && action != 0))
            {
              standard_view->priv->drop_highlight = (path == NULL && action != 0);
              gtk_widget_queue_draw (GTK_WIDGET (standard_view));
            }

          /* setup drop-file for the icon renderer to highlight the target */
          g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", (action != 0) ? file : NULL, NULL);

          /* do the item highlighting */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->highlight_path) (standard_view, path);

          /* cleanup */
          if (G_LIKELY (file != NULL))
            g_object_unref (G_OBJECT (file));
          if (G_LIKELY (path != NULL))
            gtk_tree_path_free (path);
        }
      else
        {
          /* request the drag data from the source */
          gtk_drag_get_data (view, context, target, time);
        }

      /* tell Gdk whether we can drop here */
      gdk_drag_status (context, action, time);
    }
  else
    {
      /* check whether we can drop at (x,y) */
      thunar_standard_view_get_dest_actions (standard_view, context, x, y, time, NULL);
    }

  /* start the drag autoscroll timer if not already running */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id < 0))
    {
      /* schedule the drag autoscroll timer */
      standard_view->priv->drag_scroll_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 50, thunar_standard_view_drag_scroll_timer,
                                                                      standard_view, thunar_standard_view_drag_scroll_timer_destroy);
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

  /* release the drag path list (just in case the drag-end wasn't fired before) */
  thunar_vfs_path_list_free (standard_view->priv->drag_path_list);

  /* query the list of selected URIs */
  standard_view->priv->drag_path_list = thunar_file_list_to_path_list (standard_view->selected_files);
  if (G_LIKELY (standard_view->priv->drag_path_list != NULL))
    {
      /* determine the first selected file */
      file = thunar_file_get_for_path (standard_view->priv->drag_path_list->data, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* generate an icon based on that file */
          g_object_get (G_OBJECT (standard_view->icon_renderer), "size", &size, NULL);
          icon = thunar_icon_factory_load_file_icon (standard_view->icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, size);
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
  uri_string = thunar_vfs_path_list_to_string (standard_view->priv->drag_path_list);
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
  /* stop any running drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id >= 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* release the list of dragged URIs */
  thunar_vfs_path_list_free (standard_view->priv->drag_path_list);
  standard_view->priv->drag_path_list = NULL;
}



static void
thunar_standard_view_error (ThunarListModel    *model,
                            const GError       *error,
                            ThunarStandardView *standard_view)
{
  ThunarFile *file;

  g_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  g_return_if_fail (standard_view->model == model);

  /* determine the ThunarFile for the current directory */
  file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_UNLIKELY (file == NULL))
    return;

  /* inform the user about the problem */
  thunar_dialogs_show_error (GTK_WIDGET (standard_view), error,
                             _("Failed to open directory \"%s\""),
                             thunar_file_get_display_name (file));
}



static void
thunar_standard_view_sort_column_changed (GtkTreeSortable    *tree_sortable,
                                          ThunarStandardView *standard_view)
{
  GtkSortType sort_order;
  gint        sort_column;

  g_return_if_fail (GTK_IS_TREE_SORTABLE (tree_sortable));
  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the new sort column and sort order */
  if (gtk_tree_sortable_get_sort_column_id (tree_sortable, &sort_column, &sort_order))
    {
      /* remember the new values as default */
      g_object_set (G_OBJECT (standard_view->preferences),
                    "last-sort-column", sort_column,
                    "last-sort-order", sort_order,
                    NULL);
    }
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
      g_object_freeze_notify (G_OBJECT (standard_view));
      g_object_notify (G_OBJECT (standard_view), "loading");
      g_object_notify (G_OBJECT (standard_view), "statusbar-text");
      g_object_thaw_notify (G_OBJECT (standard_view));
    }
}



static gboolean
thunar_standard_view_drag_scroll_timer (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);
  GtkAdjustment      *vadjustment;
  gfloat              value;
  gint                offset;
  gint                y, h;

  GDK_THREADS_ENTER ();

  /* verify that we are realized */
  if (G_LIKELY (GTK_WIDGET_REALIZED (standard_view)))
    {
      /* determine pointer location and window geometry */
      gdk_window_get_pointer (GTK_BIN (standard_view)->child->window, NULL, &y, NULL);
      gdk_window_get_geometry (GTK_BIN (standard_view)->child->window, NULL, NULL, NULL, &h, NULL);

      /* check if we are near the edge */
      offset = y - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (y - (h - 2 * 20), 0);

      /* change the vertical adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));

          /* determine the new value */
          value = CLAMP (vadjustment->value + 2 * offset, vadjustment->lower, vadjustment->upper - vadjustment->page_size);

          /* apply the new value */
          gtk_adjustment_set_value (vadjustment, value);
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_standard_view_drag_scroll_timer_destroy (gpointer user_data)
{
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_scroll_timer_id = -1;
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
  GMainLoop  *loop;
  GtkWidget  *menu;
  GList      *selected_items;
  guint       id;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the selected items */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);

  /* merge the custom menu actions for the selected items */
  thunar_standard_view_merge_custom_actions (standard_view, selected_items);

  /* check if we need to popup the file or the folder context menu */
  menu = gtk_ui_manager_get_widget (standard_view->ui_manager, (selected_items != NULL) ? "/file-context-menu" : "/folder-context-menu");

  /* release the selected items */
  g_list_foreach (selected_items, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (selected_items);

  /* take a reference on the context menu */
  exo_gtk_object_ref_sink (GTK_OBJECT (menu));

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
  GtkTreeIter iter;
  ThunarFile *current_directory;
  gboolean    can_paste_into_folder;
  gboolean    pastable;
  gboolean    writable;
  GList      *lp, *selected_files;
  gint        n_selected_files = 0;

  g_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
      standard_view->priv->new_files_closure = NULL;
    }

  /* release the previously selected files */
  thunar_file_list_free (standard_view->selected_files);

  /* determine the new list of selected files (replacing GtkTreePath's with ThunarFile's) */
  selected_files = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  for (lp = selected_files; lp != NULL; lp = lp->next, ++n_selected_files)
    {
      /* determine the iterator for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);

      /* release the tree path... */
      gtk_tree_path_free (lp->data);

      /* ...and replace it with the file */
      lp->data = thunar_list_model_get_file (standard_view->model, &iter);
    }

  /* and setup the new selected files list */
  standard_view->selected_files = selected_files;

  /* check whether the folder displayed by the view is writable */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  writable = (current_directory != NULL && thunar_file_is_writable (current_directory));

  /* check whether the clipboard contains data that can be pasted here */
  pastable = (standard_view->clipboard != NULL && thunar_clipboard_manager_get_can_paste (standard_view->clipboard));

  /* check whether the only selected file is
   * folder to which we can paste to */
  can_paste_into_folder = (n_selected_files == 1)
                       && thunar_file_is_directory (selected_files->data)
                       && thunar_file_is_writable (selected_files->data);

  /* update the "Create Folder"/"Create Document" actions */
  gtk_action_set_sensitive (standard_view->priv->action_create_folder, writable);
  gtk_action_set_sensitive (standard_view->priv->action_create_document, writable);

  /* update the "Properties" action */
  gtk_action_set_sensitive (standard_view->priv->action_properties, (n_selected_files == 1 || (n_selected_files == 0 && current_directory != NULL)));

  /* update the "Cut" action */
  g_object_set (G_OBJECT (standard_view->priv->action_cut),
                "sensitive", (n_selected_files > 0) && writable,
                "tooltip", ngettext ("Prepare the selected file to be moved with a Paste command",
                                     "Prepare the selected files to be moved with a Paste command",
                                     n_selected_files),
                NULL);

  /* update the "Copy" action */
  g_object_set (G_OBJECT (standard_view->priv->action_copy),
                "sensitive", (n_selected_files > 0),
                "tooltip", ngettext ("Prepare the selected file to be copied with a Paste command",
                                     "Prepare the selected files to be copied with a Paste command",
                                     n_selected_files),
                NULL);

  /* update the "Paste" action */
  gtk_action_set_sensitive (standard_view->priv->action_paste, writable && pastable);

  /* update the "Delete" action */
  g_object_set (G_OBJECT (standard_view->priv->action_delete),
                "sensitive", (n_selected_files > 0) && writable,
                "tooltip", ngettext ("Delete the selected file permanently",
                                     "Delete the selected files permanently",
                                     n_selected_files),
                NULL);

  /* update the "Paste Into Folder" action */
  g_object_set (G_OBJECT (standard_view->priv->action_paste_into_folder),
                "sensitive", pastable,
                "visible", can_paste_into_folder,
                NULL);

  /* update the "Duplicate File(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_duplicate),
                "sensitive", (n_selected_files > 0) && writable,
                "tooltip", ngettext ("Duplicate the selected file",
                                     "Duplicate each selected file",
                                     n_selected_files),
                NULL);

  /* update the "Make Link(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_make_link),
                "label", ngettext ("Ma_ke Link", "Ma_ke Links", n_selected_files),
                "sensitive", (n_selected_files > 0) && writable,
                "tooltip", ngettext ("Create a symbolic link for the selected file",
                                     "Create a symbolic link for each selected file",
                                     n_selected_files),
                NULL);

  /* update the "Rename" action */
  gtk_action_set_sensitive (standard_view->priv->action_rename, (n_selected_files == 1
                            && thunar_file_is_renameable (selected_files->data)));

  /* update the statusbar text */
  thunar_standard_view_update_statusbar_text (standard_view);

  /* emit notification for "selected-files" */
  g_object_notify (G_OBJECT (standard_view), "selected-files");
}



